"""Add ZAPD object roots from the authoritative Ship checkout's asset metadata."""
import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path

VERSIONS = {"N64_NTSC_10": "ntsc_1-0", "N64_NTSC_11": "ntsc_1-1", "N64_NTSC_12": "ntsc_1-2"}
OBJECTS = ("object_niw", "object_gi_niwatori", "object_ma1", "object_ma2", "object_gi_bracelet")


def entries(path):
    current = None
    for line in path.read_text(encoding="utf-8").splitlines():
        name = re.fullmatch(r"([A-Za-z_][A-Za-z_0-9]*):", line)
        if name:
            if current:
                yield current
            current = {"symbol": name[1]}
        elif current:
            field = re.fullmatch(r"  ([a-z_]+):\s*(.*?)\s*", line)
            if field:
                current[field[1]] = field[2]
    if current:
        yield current


def make_xml(source, name):
    root = ET.Element("Root")
    ET.SubElement(root, "ExternalFile", XmlPath="objects/gameplay_keep.xml", OutPath="objects/oot_gameplay_keep/")
    file = ET.SubElement(root, "File", Name=name, Segment="6", OutName="oot_" + name, Game="OOT")
    for entry in entries(source):
        kind = entry.get("type")
        attributes = {"Name": entry["symbol"], "Offset": entry.get("offset", "0x0")}
        if kind == "GFX":
            ET.SubElement(file, "DList", attributes)
        elif kind == "OOT:ANIMATION":
            ET.SubElement(file, "Animation", attributes)
        elif kind == "OOT:SKELETON":
            attributes.update(Type=entry["skel_type"], LimbType=entry["limb_type"])
            ET.SubElement(file, "Skeleton", attributes)
        elif kind == "TEXTURE":
            attributes.update(Format=entry["format"].lower(), Width=entry["width"], Height=entry["height"])
            if "tlut" in entry:
                attributes["TlutOffset"] = entry["tlut"]
            ET.SubElement(file, "Texture", attributes)
    ET.indent(root, space="    ")
    return ET.tostring(root, encoding="unicode") + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ship", type=Path, required=True)
    parser.add_argument("--oot-assets", type=Path, required=True)
    parser.add_argument("--object", action="append", dest="objects", help="Object metadata name; repeatable")
    args = parser.parse_args()
    outputs = []
    for version, metadata in VERSIONS.items():
        for name in args.objects or OBJECTS:
            source = args.ship / "soh/assets/yml" / metadata / "objects" / (name + ".yml")
            target = args.oot_assets / "assets/oot_zapd/assets/xml" / version / "objects" / (name + ".xml")
            if target.exists():
                raise SystemExit(f"Refusing to replace existing XML: {target}")
            outputs.append((target, make_xml(source, name)))
    for target, content in outputs:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding="utf-8")
        print(target)


if __name__ == "__main__":
    main()
