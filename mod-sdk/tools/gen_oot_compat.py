"""Generate include/oot_compat from Ship's and 2Ship's ModApi headers.

soh_api_names.h maps every SOH* name 2Ship has as S2H*, and soh/ModApi/<path>.h forwards each Ship ModApi header
to 2Ship's at the same path. Run again whenever either ModApi changes. Names and headers printed as missing need
a hand-written mapping in z64oot_names.h or have no Majora's Mask counterpart.
"""
import argparse
import json
import re
import sys
from pathlib import Path

SDK_DIRECTORY = Path(__file__).resolve().parent.parent
REPOSITORY_ROOT = SDK_DIRECTORY.parent
OUTPUT = SDK_DIRECTORY / "include" / "oot_compat" / "soh_api_names.h"
SHIP_NAME = re.compile(r"\b(SOH|Soh)([A-Za-z0-9_]+)\b")
HEADER_GUARD = re.compile(r"_H$")
COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)


def strip_comments(text):
    return COMMENT.sub("", text)


def collect_identifiers(roots):
    identifiers = set()
    for root in roots:
        for header in root.rglob("*.h"):
            if OUTPUT.parent in header.parents:
                continue
            code = strip_comments(header.read_text(encoding="utf-8", errors="ignore"))
            identifiers.update(re.findall(r"\b[A-Za-z_][A-Za-z0-9_]*\b", code))
    return identifiers


def collect_ship_names(roots):
    names = set()
    for root in roots:
        for header in root.rglob("*.h"):
            for prefix, rest in SHIP_NAME.findall(strip_comments(header.read_text(encoding="utf-8", errors="ignore"))):
                names.add((prefix, rest))
    return names


def to_mm_name(prefix, rest):
    return ("S2H" if prefix == "SOH" else "S2h") + rest


SAVE_FIELDS_OUTPUT = SDK_DIRECTORY / "tools" / "oot_save_fields.json"
STRUCT_FIELD = re.compile(r"^\s*(?:/\*[^*]*\*/)?\s*[A-Za-z_][\w ]*?[\s\*]+([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*;", re.M)
# OoT SaveContext fields whose MM counterpart has another name.
RENAMED_SAVE_FIELDS = {
    "isDoubleDefenseAcquired": "doubleDefense",
    "naviTimer": "tatlTimer",
    "entranceIndex": "entrance",
    "savedSceneNum": "savedSceneId",
    "dayTime": "time",
    "nightFlag": "isNight",
    "totalDays": "day",
    "hudVisibilityMode": "hudVisibility",
    "nextHudVisibilityMode": "nextHudVisibility",
    "prevHudVisibilityMode": "prevHudVisibility",
    "hudVisibilityModeTimer": "hudVisibilityTimer",
    "minigameState": "minigameStatus",
    "childEquips": "equips",
    "adultEquips": "equips",
}
# Under SaveOptions in MM.
OPTION_SAVE_FIELDS = {"language", "audioSetting", "zTargetSetting"}


def struct_body(text, struct_name):
    end = re.search(r"\}\s*" + struct_name + r"\s*;", text)
    if end is None:
        return ""
    depth = 0
    for index in range(end.start(), -1, -1):
        if text[index] == "}":
            depth += 1
        elif text[index] == "{":
            depth -= 1
            if depth == 0:
                return text[index + 1:end.start()]
    return ""


# Top-level fields only: members of nested struct/union bodies are dropped with their braces.
def read_struct_fields(header, struct_name):
    body = struct_body(strip_comments(header.read_text(encoding="utf-8", errors="ignore")), struct_name)
    while "{" in body:
        body = re.sub(r"\{[^{}]*\}", "", body)
    return STRUCT_FIELD.findall(body)


# SaveContext is flat in OoT and nested in MM: each OoT field maps to the MM path where a field of that name lives.
def write_save_field_map(ship_root):
    ship_fields = read_struct_fields(ship_root / "soh" / "include" / "z64save.h", "SaveContext")
    mm_header = REPOSITORY_ROOT / "mm" / "include" / "z64save.h"
    mm_paths = [("", "SaveContext"), ("save.", "Save"), ("save.saveInfo.", "SaveInfo"),
                ("save.saveInfo.playerData.", "SavePlayerData")]
    mm_fields = [(prefix, set(read_struct_fields(mm_header, struct))) for prefix, struct in mm_paths]
    mapping = {}
    unmapped = []
    for field in ship_fields:
        if field.startswith("unk_"):
            continue
        mm_name = RENAMED_SAVE_FIELDS.get(field, field)
        path = next((prefix + mm_name for prefix, fields in mm_fields if mm_name in fields), None)
        if field in OPTION_SAVE_FIELDS:
            path = "options." + field
        if path is None:
            unmapped.append(field)
        elif path != field:
            mapping[field] = path
    SAVE_FIELDS_OUTPUT.write_text(json.dumps(mapping, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return mapping, unmapped


IDS_OUTPUT = OUTPUT.parent / "oot_ids.h"
# Far above every MM id and every Unbound runtime id, so an OoT-only id never matches an actor, object or scene.
SAVE_FLAG = re.compile(r"^#define ((?:EVENTCHKINF|ITEMGETINF|INFTABLE)_\w+)[ \t]+(.+?)\s*$", re.M)
ID_CATEGORIES = [("tables/actor_table.h", "ACTOR", 0x7E00), ("tables/object_table.h", "OBJECT", 0x7E00),
                 ("tables/scene_table.h", "SCENE", 0x7E00), ("z64item.h", "GI", 0x7E00),
                 ("../soh/Enhancements/game-interactor/vanilla-behavior/GIVanillaBehavior.h", "VB", 0x7E00),
                 # Player.itemAction is s8: OoT-only actions sit past MM's PLAYER_IA_MAX and below 0x80.
                 ("z64player.h", "PLAYER_IA", 0x58)]
HAND_WRITTEN = OUTPUT.parent / "z64oot_names.h"


# OoT ids MM lacks get unique ids nothing in MM uses, so `actor->id == ACTOR_EN_X` compiles and stays false.
def write_oot_ids(ship_root):
    mm_identifiers = collect_identifiers([REPOSITORY_ROOT / "mm" / "include", REPOSITORY_ROOT / "mm" / "2s2h"])
    mm_identifiers |= set(re.findall(r"#define\s+(\w+)", HAND_WRITTEN.read_text(encoding="utf-8")))
    lines = ["// Generated by mod-sdk/tools/gen_oot_compat.py from Ship's tables. Do not edit.",
             "#ifndef OOT_COMPAT_OOT_IDS_H", "#define OOT_COMPAT_OOT_IDS_H", ""]
    counts = {}
    include = ship_root / "soh" / "include"
    for table, prefix, base in ID_CATEGORIES:
        text = strip_comments((include / table).read_text(encoding="utf-8"))
        names = sorted(set(re.findall(rf"\b{prefix}_[A-Z0-9_]+\b", text)) - mm_identifiers - {f"{prefix}_ID_MAX"})
        if base < 0x80 and base + len(names) > 0x80:
            sys.exit(f"{len(names)} OoT-only {prefix} values do not fit in s8 from 0x{base:X}")
        lines += [f"#define {name} 0x{base + index:04X}" for index, name in enumerate(names)]
        lines.append("")
        counts[prefix] = len(names)
    # MM has no OoT sound bank: an OoT-only sound plays nothing.
    sounds = set(re.findall(r"#define (NA_SE_\w+)\s", strip_comments((include / "sfx.h").read_text(encoding="utf-8"))))
    sounds = sorted(sounds - mm_identifiers)
    lines += [f"#define {name} NA_SE_NONE" for name in sounds] + [""]
    counts["NA_SE"] = len(sounds)
    # A GID indexes MM's draw table, so an OoT-only model draws nothing instead of reading past it.
    item_text = strip_comments((include / "z64item.h").read_text(encoding="utf-8"))
    draw_ids = sorted(set(re.findall(r"\bGID_[A-Z0-9_]+\b", item_text)) - mm_identifiers - {"GID_MAXIMUM"})
    lines += [f"#define {name} GID_NONE" for name in draw_ids] + [""]
    counts["GID"] = len(draw_ids)
    # OoT save flag numbers keep their values; the compat's flag getters read them as never set.
    save_text = strip_comments((include / "z64save.h").read_text(encoding="utf-8"))
    flags = [(name, value) for name, value in SAVE_FLAG.findall(save_text) if name not in mm_identifiers]
    lines += [f"#define {name} {value}" for name, value in flags] + [""]
    counts["flags"] = len(flags)
    lines += ["#endif", ""]
    IDS_OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    return counts


RANDO_OUTPUT = OUTPUT.parent / "oot_rando.h"
RANDO_HEADERS = ["randomizerTypes.h", "randomizerEnums.h"]
RANDO_TOKEN = re.compile(r"RANDO_ENUM_BEGIN\((\w+)\)|RANDO_ENUM_ITEM\((\w+)\s*(?:,\s*([^)]*))?\)")


# Values as Ship's compiler numbers them: an explicit value it can evaluate, else the previous one plus one.
def read_rando_enums(enum_directory):
    types = []
    values = {}
    for header in sorted(enum_directory.glob("*.h")):
        counter = 0
        for type_name, name, explicit in RANDO_TOKEN.findall(strip_comments(header.read_text(encoding="utf-8"))):
            if type_name:
                types.append(type_name)
                counter = 0
                continue
            if explicit:
                try:
                    counter = int(eval(explicit, {"__builtins__": {}}, dict(values)))
                except Exception:
                    pass
            values.setdefault(name, counter)
            counter += 1
    return types, values


# OoT randomizer enums as plain ints, without Ship's headers, whose RC_ names collide with MM's randomizer.
def write_rando_shim(ship_root):
    enums = ship_root / "soh" / "soh" / "Enhancements" / "randomizer" / "randomizerEnums"
    mm_identifiers = collect_identifiers([REPOSITORY_ROOT / "mm" / "include", REPOSITORY_ROOT / "mm" / "2s2h"])
    types, values = read_rando_enums(enums)
    lines = ["// Generated by mod-sdk/tools/gen_oot_compat.py from Ship's randomizer enums. Do not edit.",
             "#ifndef OOT_COMPAT_OOT_RANDO_H", "#define OOT_COMPAT_OOT_RANDO_H", "", "#include <stdint.h>", ""]
    lines += [f"typedef int32_t {name};" for name in dict.fromkeys(types) if name not in mm_identifiers]
    lines.append("")
    fresh = [name for name in values if name not in mm_identifiers]
    lines += [f"#define {name} {values[name]}" for name in fresh]
    lines += ["", "#endif", ""]
    RANDO_OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    shim_directory = OUTPUT.parent / "soh" / "Enhancements" / "randomizer"
    shim_directory.mkdir(parents=True, exist_ok=True)
    for name in RANDO_HEADERS:
        (shim_directory / name).write_text('#pragma once\n#include "oot_rando.h"\n', encoding="utf-8")
    return len(fresh)


def write_header_shims(ship_modapi, mm_modapi):
    missing = []
    for ship_header in sorted(ship_modapi.rglob("*.h")):
        relative = ship_header.relative_to(ship_modapi)
        if not (mm_modapi / relative).is_file():
            missing.append(relative.as_posix())
            continue
        guard = "OOT_COMPAT_SOH_MODAPI_" + re.sub(r"[^A-Za-z0-9]", "_", relative.as_posix()).upper()
        shim = OUTPUT.parent / "soh" / "ModApi" / relative
        shim.parent.mkdir(parents=True, exist_ok=True)
        shim.write_text(f"#ifndef {guard}\n#define {guard}\n\n#include \"2s2h/ModApi/{relative.as_posix()}\"\n"
                        f"#include \"soh_api_names.h\"\n\n#endif\n", encoding="utf-8")
    return missing


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--ship", type=Path, default=REPOSITORY_ROOT.parent.parent / "Shipwright-unbound",
                        help="Shipwright Unbound checkout")
    args = parser.parse_args()

    ship_roots = [args.ship / "soh" / "soh" / "ModApi", args.ship / "mod-sdk" / "include"]
    mm_roots = [REPOSITORY_ROOT / "mm" / "2s2h" / "ModApi", SDK_DIRECTORY / "include"]
    if not all(root.is_dir() for root in ship_roots):
        sys.exit(f"Ship ModApi not found under {args.ship}")

    mm_identifiers = collect_identifiers(mm_roots)
    mapped = []
    missing = []
    for prefix, rest in sorted(collect_ship_names(ship_roots)):
        ship_name = prefix + rest
        if HEADER_GUARD.search(ship_name) or ship_name in mm_identifiers:
            continue
        mm_name = to_mm_name(prefix, rest)
        (mapped if mm_name in mm_identifiers else missing).append((ship_name, mm_name))

    lines = ["// Generated by mod-sdk/tools/gen_oot_compat.py from Ship's and 2Ship's ModApi headers. Do not edit.",
             "#ifndef OOT_COMPAT_SOH_API_NAMES_H", "#define OOT_COMPAT_SOH_API_NAMES_H", ""]
    lines += [f"#define {ship_name} {mm_name}" for ship_name, mm_name in mapped]
    lines += ["", "#endif", ""]
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")

    missing_headers = write_header_shims(ship_roots[0], mm_roots[0])
    print(f"OoT-only ids written to {IDS_OUTPUT}: {write_oot_ids(args.ship)}")
    print(f"{write_rando_shim(args.ship)} OoT randomizer names written to {RANDO_OUTPUT}")
    save_mapping, unmapped_save_fields = write_save_field_map(args.ship)
    print(f"{len(save_mapping)} SaveContext fields relocated into {SAVE_FIELDS_OUTPUT}")
    if unmapped_save_fields:
        print(f"SaveContext fields MM lacks: {', '.join(unmapped_save_fields)}")

    print(f"{len(mapped)} names mapped into {OUTPUT}")
    if missing:
        print(f"{len(missing)} Ship names with no S2H counterpart:")
        for ship_name, _ in missing:
            print(f"  {ship_name}")
    if missing_headers:
        print(f"{len(missing_headers)} Ship ModApi headers with no 2Ship counterpart:")
        for header in missing_headers:
            print(f"  {header}")


if __name__ == "__main__":
    main()
