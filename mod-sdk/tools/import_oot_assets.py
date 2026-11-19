"""Import an already extracted personal OoT archive into the loader's isolated resource names."""
import argparse
import re
import struct
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

# OoT's sOcarinaSongFanfares for the songs MM lacks; oot_items plays them under the song's replay.
SONG_FANFARES = ("037_Prelude_of_Light", "051_Bolero_of_Fire", "052_Minuet_of_Forest", "053_Serenade_of_Water",
                 "054_Requiem_of_Spirit", "055_Nocturne_of_Shadow", "070_Zeldas_Lullaby")
RESOURCE_HEADER_SIZE = 0x40
SET_INSTRUMENT = 0xC1
# OoT's quest-screen tints; oot_items shows each song as the game's own note in an item icon's 32px square.
SONG_NOTE_TINTS = {"Minuet": (150, 255, 100), "Bolero": (255, 80, 40), "Serenade": (100, 150, 255),
                   "Requiem": (255, 160, 0), "Nocturne": (255, 100, 255), "Prelude": (255, 240, 100),
                   "Lullaby": (255, 20, 20), "Custom": (255, 255, 255)}
SONG_NOTE_SOURCE = "textures/icon_item_static/gSongNoteTex"
TEXTURE_IA8 = 8
TEXTURE_RGBA32 = 1
ICON_SIZE = 32


class Reader:
    def __init__(self, data):
        self.data = data
        self.offset = RESOURCE_HEADER_SIZE

    def read(self, layout):
        values = struct.unpack_from("<" + layout, self.data, self.offset)
        self.offset += struct.calcsize("<" + layout)
        return values if len(values) > 1 else values[0]

    def read_sound(self):
        has_sample, length = self.read("bI")
        name = self.data[self.offset:self.offset + length].decode("latin1")
        self.offset += length
        return has_sample, name, self.read("f")

    def read_optional_sound(self):
        return self.read_sound() if self.read("b") else None


def read_font(data):
    reader = Reader(data)
    font = {"header": reader.read("ibbHHH")}
    drum_count, instrument_count, effect_count = reader.read("III")
    font["drums"] = []
    for _ in range(drum_count):
        release, pan, loaded, envelope_count = reader.read("BBBI")
        envelope = [reader.read("hh") for _ in range(envelope_count)]
        font["drums"].append((release, pan, loaded, envelope, reader.read_sound()))
    font["instruments"] = []
    for _ in range(instrument_count):
        fields, envelope_count = reader.read("BBBBB"), reader.read("i")
        envelope = [reader.read("hh") for _ in range(envelope_count)]
        sounds = [reader.read_optional_sound() for _ in range(3)]
        font["instruments"].append((fields, envelope, sounds))
    font["effects"] = [reader.read_optional_sound() for _ in range(effect_count)]
    return font


def write_font(header, font):
    out = bytearray(header)

    def write(layout, *values):
        out.extend(struct.pack("<" + layout, *values))

    def write_sound(sound):
        name = sound[1].encode("latin1")
        write("bI", sound[0], len(name))
        out.extend(name)
        write("f", sound[2])

    def write_optional_sound(sound):
        write("b", sound is not None)
        if sound is not None:
            write_sound(sound)

    write("ibbHHH", *font["header"])
    write("III", len(font["drums"]), len(font["instruments"]), len(font["effects"]))
    for release, pan, loaded, envelope, sound in font["drums"]:
        write("BBBI", release, pan, loaded, len(envelope))
        for point in envelope:
            write("hh", *point)
        write_sound(sound)
    for fields, envelope, sounds in font["instruments"]:
        write("BBBBB", *fields)
        write("i", len(envelope))
        for point in envelope:
            write("hh", *point)
        for sound in sounds:
            write_optional_sound(sound)
    for sound in font["effects"]:
        write_optional_sound(sound)
    return bytes(out)


def sequence_parts(data):
    size = struct.unpack_from("<I", data, RESOURCE_HEADER_SIZE)[0]
    body = data[RESOURCE_HEADER_SIZE + 4:RESOURCE_HEADER_SIZE + 4 + size]
    font_count = struct.unpack_from("<I", data, RESOURCE_HEADER_SIZE + 7 + size)[0]
    fonts = data[RESOURCE_HEADER_SIZE + 11 + size:RESOURCE_HEADER_SIZE + 11 + size + font_count]
    return body, list(fonts)


def renamed_sound(sound):
    if sound is None or not sound[1]:
        return sound
    return sound[0], sound[1].replace("audio/samples/", "audio/oot_samples/", 1), sound[2]


def isolate_font(font, instruments):
    """Points the font at renamed samples; a sound-effect bank keeps only the fanfares' instruments."""
    if font["effects"]:
        font["drums"] = []
        font["effects"] = []
        font["instruments"] = [entry if index in instruments else ((0,) + entry[0][1:], entry[1], [None] * 3)
                               for index, entry in enumerate(font["instruments"])]
    font["drums"] = [drum[:4] + (renamed_sound(drum[4]),) for drum in font["drums"]]
    font["instruments"] = [(fields, envelope, [renamed_sound(sound) for sound in sounds])
                           for fields, envelope, sounds in font["instruments"]]
    return font


def font_samples(font):
    sounds = [drum[4] for drum in font["drums"]] + font["effects"]
    sounds += [sound for entry in font["instruments"] if entry[0][0] for sound in entry[2]]
    return {sound[1] for sound in sounds if sound is not None and sound[1]}


def song_fanfares(source):
    """OoT's song fanfares under audio/oot_*, so MM's own sequences, fonts and samples keep their names."""
    font_files = {int(name.split("/")[-1][:2]): name for name in source.namelist() if name.startswith("audio/fonts/")}
    instruments_by_font = {}
    payload = {}
    for name in SONG_FANFARES:
        data = source.read("audio/sequences/" + name)
        body, fonts = sequence_parts(data)
        used = {body[index + 1] for index in range(len(body) - 1) if body[index] == SET_INSTRUMENT}
        for font in fonts:
            instruments_by_font.setdefault(font, set()).update(used)
        payload["audio/oot_sequences/" + name] = data
    for index, instruments in instruments_by_font.items():
        data = source.read(font_files[index])
        font = isolate_font(read_font(data), instruments)
        payload[font_files[index].replace("audio/fonts/", "audio/oot_fonts/", 1)] = write_font(
            data[:RESOURCE_HEADER_SIZE], font)
        for sample in font_samples(font):
            payload[sample] = source.read(sample.replace("audio/oot_samples/", "audio/samples/", 1))
    return payload


def tinted_note_icon(note, tint):
    """The 16x24 IA8 note centred in a 32x32 RGBA32 icon, coloured as the quest screen colours it."""
    texture_type, width, height, size = struct.unpack_from("<4I", note, RESOURCE_HEADER_SIZE)
    if texture_type != TEXTURE_IA8 or width > ICON_SIZE or height > ICON_SIZE:
        raise ValueError("The song note is not the IA8 texture this importer knows")
    texels = note[RESOURCE_HEADER_SIZE + 16:RESOURCE_HEADER_SIZE + 16 + size]
    left = (ICON_SIZE - width) // 2
    top = (ICON_SIZE - height) // 2
    pixels = bytearray(ICON_SIZE * ICON_SIZE * 4)
    for index, texel in enumerate(texels):
        intensity = (texel >> 4) * 17
        offset = ((top + index // width) * ICON_SIZE + left + index % width) * 4
        pixels[offset:offset + 4] = bytes(channel * intensity // 255 for channel in tint) + bytes([(texel & 15) * 17])
    layout = struct.pack("<4I", TEXTURE_RGBA32, ICON_SIZE, ICON_SIZE, len(pixels))
    return note[:RESOURCE_HEADER_SIZE] + layout + bytes(pixels)


def song_note_icons(source):
    note = source.read(SONG_NOTE_SOURCE)
    return {f"textures/oot_icon_item_static/gItemIconOotSong{song}Tex": tinted_note_icon(note, tint)
            for song, tint in SONG_NOTE_TINTS.items()}


def append_song_resources(source_path, output):
    with zipfile.ZipFile(source_path) as source:
        payload = song_fanfares(source)
        payload.update(song_note_icons(source))
    with zipfile.ZipFile(output) as existing:
        present = set(existing.namelist())
    with zipfile.ZipFile(output, "a", compression=zipfile.ZIP_DEFLATED) as out:
        for name, data in payload.items():
            if name not in present:
                out.writestr(name, data)
    print(f"Added OoT song fanfares and note icons ({len(payload)} resources) to {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--libultraship", type=Path, help="host checkout's libultraship, when not beside mod-sdk")
    parser.add_argument("--songs-only", action="store_true",
                        help="add the song fanfares and note icons to an existing output")
    args = parser.parse_args()
    if args.songs_only:
        append_song_resources(args.source, args.output)
        return
    root = Path(__file__).resolve().parents[2]
    xml_root = root / "mod-sdk/mods/oot_assets/assets/oot_zapd/assets/xml/N64_NTSC_10"
    prefixes = {}
    for path in xml_root.rglob("*.xml"):
        category = path.relative_to(xml_root).parts[0]
        for file in ET.parse(path).getroot().iter("File"):
            prefixes[category + "/" + file.attrib["Name"] + "/"] = category + "/" + file.attrib["OutName"] + "/"

    libultraship = args.libultraship or root / "libultraship"
    table_source = (libultraship / "src/ship/utils/StrHash64.cpp").read_text()
    table = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)",
             table_source.split("CRC64_Table[256] = {")[1].split("};")[0])]

    def crc(name):
        value = 0xffffffffffffffff
        for byte in name.encode():
            value = (table[(value >> 56) ^ byte] ^ (value << 8)) & 0xffffffffffffffff
        return value

    def hashes(value):
        return (struct.pack("<Q", value), struct.pack(">Q", value),
                struct.pack("<II", value >> 32, value & 0xffffffff),
                struct.pack(">II", value & 0xffffffff, value >> 32))

    def renamed(name):
        for old, new in prefixes.items():
            if name.startswith(old):
                return new + name[len(old):]
        return name

    with zipfile.ZipFile(args.source) as source:
        names = {name: renamed(name) for name in source.namelist() if renamed(name) != name}
        replacements = {}
        for old, new in names.items():
            replacements.update(zip(hashes(crc(old)), hashes(crc(new))))
        payload = {}
        string_pattern = re.compile(rb"(?:__OTR__)?(?:objects|textures|overlays|misc)/[^\x00\s]+\x00")
        for old, new in names.items():
            data = bytearray(source.read(old))
            for offset in range(64, len(data) - 7, 8):
                replacement = replacements.get(bytes(data[offset:offset + 8]))
                if replacement is not None:
                    data[offset:offset + 8] = replacement
            strings = []
            for offset in range(64, len(data) - 8):
                length = struct.unpack_from("<I", data, offset)[0]
                if length < 8 or length > 1024 or offset + 4 + length > len(data):
                    continue
                raw = bytes(data[offset + 4:offset + 4 + length])
                if not raw.startswith((b"__OTR__", b"objects/", b"textures/", b"overlays/", b"misc/")):
                    continue
                terminated = raw.endswith(b"\0")
                try:
                    value = raw.rstrip(b"\0").decode("ascii")
                except UnicodeDecodeError:
                    continue
                marker = "__OTR__" if value.startswith("__OTR__") else ""
                updated = marker + renamed(value[len(marker):])
                if updated != value:
                    strings.append((offset, length, updated.encode() + (b"\0" if terminated else b"")))
            for offset, length, updated in reversed(strings):
                data[offset:offset + 4 + length] = struct.pack("<I", len(updated)) + updated
            payload[new] = data
        if not payload:
            raise ValueError("No OoT item resources in this archive")
        required = "textures/oot_icon_item_static/gItemIconSlingshotTex"
        if required not in payload:
            raise ValueError("Source archive has no OoT item icons")
        payload.update(song_fanfares(source))
        payload.update(song_note_icons(source))
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as out:
            for name, data in payload.items():
                out.writestr(name, data)
        print(f"Imported {len(payload)} isolated OoT resources into {args.output}")


if __name__ == "__main__":
    main()
