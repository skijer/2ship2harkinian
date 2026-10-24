"""Import an already extracted personal OoT archive into the loader's isolated resource names."""
import argparse
import re
import struct
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    xml_root = root / "mod-sdk/mods/oot_assets/assets/oot_zapd/assets/xml/N64_NTSC_10"
    prefixes = {}
    for path in xml_root.rglob("*.xml"):
        category = path.relative_to(xml_root).parts[0]
        for file in ET.parse(path).getroot().iter("File"):
            prefixes[category + "/" + file.attrib["Name"] + "/"] = category + "/" + file.attrib["OutName"] + "/"

    table_source = (root / "libultraship/src/ship/utils/StrHash64.cpp").read_text()
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
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as out:
            for name, data in payload.items():
                out.writestr(name, data)
        print(f"Imported {len(payload)} isolated OoT resources into {args.output}")


if __name__ == "__main__":
    main()
