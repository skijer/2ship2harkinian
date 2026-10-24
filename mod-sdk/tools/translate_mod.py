"""Translate a Shipwright (OoT) mod into a 2Ship (MM) mod folder.

Names with the same contract in both games resolve through include/oot_compat (z64oot_names.h). This script
handles what macros cannot: hook names inside strings and registrations, fields whose meaning is swapped between
the games, and the manifest. Everything it cannot translate safely is listed in TRANSLATION.md.
"""
import argparse
import json
import re
import shutil
import sys
from pathlib import Path

SDK_DIRECTORY = Path(__file__).resolve().parent.parent
REPOSITORY_ROOT = SDK_DIRECTORY.parent
DEFAULT_OUTPUT_ROOT = REPOSITORY_ROOT.parent / "2ship-unbound-mod-nei" / "mod-sdk" / "mods"
SOURCE_SUFFIXES = {".c", ".h", ".cpp", ".hpp"}
COMPAT_INCLUDE = '#include "z64oot_names.h"'

HOOK_RENAMES = {
    "OnActorDraw": "OnActorDrawBegin",
}

# Order matters: Actor's speedXZ becomes speed before Player's linearVelocity takes the speedXZ name.
FIELD_REWRITES = [
    (re.compile(r"(\.|->)speedXZ\b"), r"\1speed"),
    (re.compile(r"(\.|->)linearVelocity\b"), r"\1speedXZ"),
    (re.compile(r"(actorLists\[(?:[^\[\]]|\[[^\]]*\])*\])\.head\b"), r"\1.first"),
    (re.compile(r"(?<![Rr]oom)(\.|->)segment\b"), r"\1segmentVoid"),
    (re.compile(r"(\.|->)ageRequirement(\s*=\s*)([^;,}\n]+)"), r"\1formMask\2OOT_COMPAT_AGE_FORM_MASK(\3)"),
    (re.compile(r"(\.|->)nextEntranceIndex\b"), r"\1nextEntrance"),
    (re.compile(r"(\.|->)entranceIndex\b"), r"\1entrance"),
    # EnvironmentContext weather, same offsets in both games.
    (re.compile(r"(\.|->)indoors\b"), r"\1lightMode"),
    (re.compile(r"(\.|->)unk_EE\b"), r"\1precipitation"),
    (re.compile(r"(\.|->)gloomySkyMode\b"), r"\1stormRequest"),
    (re.compile(r"(\.|->)unk_DE\b"), r"\1stormState"),
    (re.compile(r"(\.|->)lightningMode\b"), r"\1lightningState"),
    (re.compile(r"(\.|->)camDataIdx\b"), r"\1bgCamIndex"),
    (re.compile(r"\b(msgCtx\s*(?:\.|->)\s*)textId\b"), r"\1currentTextId"),
    (re.compile(r"(\.|->)debugState\b"), r"\1debugEditor"),
    (re.compile(r"(\.|->)stickRelX\b"), r"\1stickAdjX"),
    (re.compile(r"(\.|->)stickRelY\b"), r"\1stickAdjY"),
    (re.compile(r"(\.|->)arrowPointedActor\b"), r"\1arrowHoverActor"),
    (re.compile(r"\bgMtxClear\b"), "gIdentityMtx"),
    (re.compile(r"(\.|->)meleeWeaponEffectIndex\b(?!\s*\[)"), r"\1meleeWeaponEffectIndex[0]"),
    # EffectBlure colours are Color_RGBA8 in Ship and u8[4] in MM.
    (re.compile(r"\b(p1StartColor|p2StartColor|p1EndColor|p2EndColor)\.r\b"), r"\1[0]"),
    (re.compile(r"\b(p1StartColor|p2StartColor|p1EndColor|p2EndColor)\.g\b"), r"\1[1]"),
    (re.compile(r"\b(p1StartColor|p2StartColor|p1EndColor|p2EndColor)\.b\b"), r"\1[2]"),
    (re.compile(r"\b(p1StartColor|p2StartColor|p1EndColor|p2EndColor)\.a\b"), r"\1[3]"),
    (re.compile(r"(\.|->)activeCamera\b"), r"\1activeCamId"),
    (re.compile(r"(\.|->)targetCtx\b"), r"\1attention"),
    (re.compile(r"(\.|->)targetMode\b"), r"\1attentionRangeType"),
    (re.compile(r"(\.|->)lookAt\b"), r"\1at"),
    (re.compile(r"(\.|->)leftHandPos\b"), r"\1leftHandWorld.pos"),
    # MM's Player has no tunic; reads go to the worn tunic (writes stay and are reported by the build).
    (re.compile(r"\b[\w\[\]]+\s*(?:\.|->)\s*currentTunic\b(?!\s*=[^=])"), "OotCompat_CurrentTunic()"),
    (re.compile(r"(\.|->)colType\b"), r"\1colMaterial"),
    (re.compile(r"(\.|->)behaviorType1\b"), r"\1type"),
    (re.compile(r"(\.|->)behaviorType2\b"), r"\1environmentType"),
    (re.compile(r"(\.|->|ICHAIN_\w+\(\s*)uncullZoneForward\b"), r"\1cullingVolumeDistance"),
    (re.compile(r"(\.|->|ICHAIN_\w+\(\s*)uncullZoneScale\b"), r"\1cullingVolumeScale"),
    (re.compile(r"(\.|->|ICHAIN_\w+\(\s*)uncullZoneDownward\b"), r"\1cullingVolumeDownward"),    (re.compile(r"(\.|->)xMin\b"), r"\1minPos.x"),
    (re.compile(r"(\.|->)ySurface\b"), r"\1minPos.y"),
    (re.compile(r"(\.|->)zMin\b"), r"\1minPos.z"),
    (re.compile(r"(\.|->)bufp\b"), r"\1start"),
    (re.compile(r"(\.|->)adjAmbientColor\b"), r"\1adjLightSettings.ambientColor"),
    (re.compile(r"(\.|->)adjLight1Color\b"), r"\1adjLightSettings.light1Color"),
    (re.compile(r"(\.|->)adjFogColor\b"), r"\1adjLightSettings.fogColor"),
    (re.compile(r"(\.|->)adjFogNear\b"), r"\1adjLightSettings.fogNear"),
    (re.compile(r"(\.|->)adjFogFar\b"), r"\1adjLightSettings.zFar"),
    # Curve skeleton members (the types are aliased in z64oot_names.h).
    (re.compile(r"(\.|->)firstChildIdx\b"), r"\1child"),
    (re.compile(r"(\.|->)nextLimbIdx\b"), r"\1sibling"),
    (re.compile(r"(\.|->)transUpdIdx\b"), r"\1animation"),
    (re.compile(r"(\.|->)animFinalFrame\b"), r"\1endFrame"),
    (re.compile(r"(\.|->)animCurFrame\b"), r"\1curFrame"),
    (re.compile(r"(\.|->)sceneNum\b"), r"\1sceneId"),
    # Player members MM moved or renamed (same meaning and type).
    (re.compile(r"(\.|->)unk_858\b"), r"\1unk_B08"),
    # OoT 0x864-0x880 (walk phase, blend weights, yaw delta) sit at MM 0xB34-0xB50 in the same order.
    (re.compile(r"(\.|->)unk_864\b"), r"\1unk_B34"),
    (re.compile(r"(\.|->)unk_868\b"), r"\1unk_B38"),
    (re.compile(r"(\.|->)unk_86C\b"), r"\1unk_B3C"),
    (re.compile(r"(\.|->)unk_870\b"), r"\1unk_B40"),
    (re.compile(r"(\.|->)unk_874\b"), r"\1unk_B44"),
    (re.compile(r"(\.|->)unk_878\b"), r"\1unk_B48"),
    (re.compile(r"(\.|->)unk_87C\b"), r"\1unk_B4C"),
    (re.compile(r"(\.|->)unk_880\b"), r"\1unk_B50"),
    (re.compile(r"(\.|->)unk_844\b"), r"\1unk_ADC"),
    (re.compile(r"(\.|->)unk_845\b"), r"\1unk_ADD"),
    (re.compile(r"(\.|->)unk_6AD\b"), r"\1unk_AA5"),
    (re.compile(r"(\.|->)unk_6AE_rotFlags\b"), r"\1unk_AA6_rotFlags"),
    (re.compile(r"\bUNK6AE_ROT_"), "UNKAA6_ROT_"),
    (re.compile(r"(\.|->)knockbackDamage\b"), r"\1unk_B74"),
    (re.compile(r"(\.|->)knockbackType\b"), r"\1unk_B75"),
    (re.compile(r"(\.|->)knockbackRot\b"), r"\1unk_B76"),
    (re.compile(r"(\.|->)knockbackSpeed\b"), r"\1unk_B78"),
    (re.compile(r"(\.|->)knockbackYVelocity\b"), r"\1unk_B7C"),
    (re.compile(r"(\.|->)naviActor\b"), r"\1tatlActor"),
    (re.compile(r"(\.|->)boomerangActor\b"), r"\1zoraBoomerangActor"),
    (re.compile(r"(\.|->)mf_9E0\b"), r"\1leftHandMf"),
    (re.compile(r"\bPLAYER_LIMB_LOWER\b"), "PLAYER_LIMB_LOWER_ROOT"),
    (re.compile(r"\bPLAYER_LIMB_UPPER\b"), "PLAYER_LIMB_UPPER_ROOT"),
    (re.compile(r"\bNA_SE_IT_SHIELD_POSTURE\b"), "NA_SE_IT_SHIELD_SWING"),
    (re.compile(r"\bENHORSE_ACT_MOUNTED_IDLE_WHINNEYING\b"), "ENHORSE_ACTION_MOUNTED_IDLE_WHINNYING"),
    (re.compile(r"\bENHORSE_ACT_"), "ENHORSE_ACTION_"),
    (re.compile(r"(\.|->)sfx\b"), r"\1sfxId"),
    # Collider names the MM decomp renamed; the structs keep OoT's layout.
    (re.compile(r"\bCOLTYPE_"), "COL_MATERIAL_"),
    (re.compile(r"\bELEMTYPE_"), "ELEM_MATERIAL_"),
    (re.compile(r"\bTOUCH_"), "ATELEM_"),
    (re.compile(r"\bBUMP_"), "ACELEM_"),
    (re.compile(r"(\.|->)toucherFlags\b"), r"\1atElemFlags"),
    (re.compile(r"(\.|->)bumperFlags\b"), r"\1acElemFlags"),
    (re.compile(r"(\.|->)toucher\b"), r"\1atDmgInfo"),
    (re.compile(r"(\.|->)bumper\b"), r"\1acDmgInfo"),
    (re.compile(r"(\.|->)elemType\b"), r"\1elemMaterial"),
    (re.compile(r"(\.|->)atHitInfo\b"), r"\1atHitElem"),
    (re.compile(r"(\.|->)acHitInfo\b"), r"\1acHitElem"),
]

# OoT actor members MM renamed, rewritten only on expressions of that actor type.
ACTOR_MEMBERS = {
    "EnArrow": {"timer": "unk_260", "hitFlags": "unk_261", "touchedPoly": "unk_262", "isCsNut": "unk_263",
                "hitActor": "unk_264", "weaponInfo": "unk_244", "effectIndex": "unk_240", "unk_210": "unk_228",
                "unk_21C": "unk_234", "unk_250": "unk_268"},
    "EnBom": {"bombCollider": "collider1", "explosionCollider": "collider2",
              "explosionColliderItems": "collider2Elements", "flashIntensity": "unk_1F4", "bumpOn": "unk_1F8"},
    "EnMThunder": {"spinAttackTimer": "lightColorFrac", "spinAttackAlpha": "alphaFrac",
                   "spinTrailTexScroll": "scroll", "spinChargePercent": "unk1B0",
                   "dimmingIntensity": "adjustLightsArg1", "followPlayerTimer": "timer",
                   "attackStrength": "subtype", "swordType": "type", "chargeAlpha": "chargingAlpha",
                   "targetScale": "scaleTarget", "isUsingMagic": "isCharging"},
    "EnHorse": {"cyl1": "colliderCylinder1", "cyl2": "colliderCylinder2", "animationIdx": "animIndex"},
    "EnBoom": {"grabbed": "unk_1C8", "returnTimer": "unk_1CC", "activeTimer": "unk_1CD",
               "boomerangInfo": "weaponInfo"},
    "ObjSyokudai": {"colliderStand": "standCollider", "colliderFlame": "flameCollider", "litTimer": "snuffTimer"},
    "ArmsHook": {"hookInfo": "weaponInfo", "unk_1E8": "unk1E0", "unk_1F4": "unk1EC", "grabbed": "attachedActor",
                 "grabbedDistDiff": "attachPointOffset"},
}


def rewrite_actor_members(text):
    for type_name, members in ACTOR_MEMBERS.items():
        variables = set(re.findall(rf"\b{type_name}\s*\*\s*(\w+)", text))
        targets = [rf"\b{re.escape(name)}" for name in variables] + [rf"\(\s*\(\s*{type_name}\s*\*\s*\)[^()]*\)"]
        access = re.compile(r"((?:" + "|".join(targets) + r")\s*->\s*)(" + "|".join(members) + r")\b")
        text = access.sub(lambda match: match.group(1) + members[match.group(2)], text)
    return text


# Ship-only data: EffectBlureInit's trail colour type, and rando pool sources, which are OoT randomizer ids.
DROPPED_CODE = [
    re.compile(r",\s*TRAIL_TYPE_\w+(?=\s*,?\s*\})"),
    re.compile(r"[^;{}\n]*(?:\.|->)(?:trailType|poolSources|poolSourceCount)\s*=[^;]*;"),
    re.compile(r"\.(?:poolSources|poolSourceCount)\s*=\s*[^,}]*,?"),
]


def drop_ship_only_code(text):
    for pattern in DROPPED_CODE:
        text = pattern.sub("", text)
    return text


REVIEW_PATTERNS = [
    (re.compile(r"\bfunc_80[0-9A-Fa-f]{6}\b"), "OoT routine by address: find the MM counterpart by behavior"),
    (re.compile(r"\blinkAge\b|\bLINK_IS_(ADULT|CHILD)\b|\bLINK_AGE_\w+"), "age: MM has forms, linkAge is always 0"),
    (re.compile(r"\bGetItemEntry\b|\bItemTable_RetrieveEntry\b"),
     "get-item table: MM draws by GID and gives by GetItemId"),
    (re.compile(r"\brandomizerTypes\.h\b|\bRandomizerCheck\b|\bRandomizerGet\b|\bRC_[A-Z_]+\b|\bRG_[A-Z_]+\b"),
     "randomizer: MM randomizer types differ"),
    (re.compile(r"\bResourceMgr_(LoadAudioSample|TexIsRaw)\b"), "resource helper 2Ship does not export"),
    (re.compile(r"\bSOH(CustomEnemy|CustomItemAppearance|RegisteredActor)\b|\bSOH_(ACTOR_ENEMY\w*|BOSS_SLOT\w*|"
                r"MOD_MENU_COMBOBOX\w*|PLAYER_MOTION_WATER_SPEED)\b"), "ModApi name 2Ship lacks"),
]


GAME_CONSTANT = re.compile(r"\b(?:SCENE|ENTR|ACTOR|OBJECT|ITEM|ITEM00|SLOT|GI|GID|ITEMGETINF|EVENTCHKINF|INFTABLE|"
                           r"COLLECTFLAG|RANDOMIZER_INF|NA_SE|NA_BGM|PLAYER_STATE[123]|PLAYER_IA|PLAYER_MWA|"
                           r"PLAYER_ANIMGROUP|PLAYER_MODELGROUP|PLAYER_BOOTS|PLAYER_TUNIC|PLAYER_SHIELD|PLAYER_SWORD|"
                           r"EQUIP_VALUE|EQUIP_TYPE|DMG|EFFECT_SS)_[A-Z0-9_]+\b")
IDENTIFIER = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")


def load_mm_identifiers():
    roots = [REPOSITORY_ROOT / "mm" / "include", REPOSITORY_ROOT / "mm" / "2s2h" / "ModApi", SDK_DIRECTORY / "include",
             REPOSITORY_ROOT / "mm" / "src" / "overlays"]
    identifiers = set()
    for root in roots:
        for header in root.rglob("*.h"):
            identifiers.update(IDENTIFIER.findall(header.read_text(encoding="utf-8", errors="ignore")))
    return identifiers


INCLUDE = re.compile(r'^\s*#include\s+"([^"]+)"', re.M)


def include_roots():
    mm = REPOSITORY_ROOT / "mm"
    roots = [mm, mm / "2s2h", mm / "include", mm / "src", mm / "assets", REPOSITORY_ROOT / "libultraship" / "include"]
    return roots + [helper for helper in (SDK_DIRECTORY / "include").iterdir() if helper.is_dir()]


def collect_missing_includes(path, text, mod_directory, roots):
    items = []
    for match in INCLUDE.finditer(text):
        header = match.group(1)
        searched = [mod_directory, (mod_directory / path).parent] + roots
        if not any((root / header).is_file() for root in searched):
            number = text.count("\n", 0, match.start()) + 1
            items.append((path, number, header, "header MM does not have: port what it provides or drop it"))
    return items


def collect_unknown_constants(path, text, mm_identifiers, local_identifiers):
    items = []
    for number, line in enumerate(text.splitlines(), start=1):
        for match in GAME_CONSTANT.finditer(line):
            token = match.group(0)
            if token not in mm_identifiers and token not in local_identifiers:
                items.append((path, number, token, "OoT constant with no MM counterpart of that name"))
    return items


# MM's actor headers expect gameplay_keep's limb enums (ARROW_LIMB_MAX) before them; OoT's need none.
MM_PRELUDE_INCLUDE = '#include "objects/gameplay_keep/gameplay_keep.h"'


def insert_compat_include(text):
    lines = text.splitlines(keepends=True)
    last_include = -1
    first_actor_include = -1
    for index, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("#include"):
            last_include = index
            if first_actor_include < 0 and "overlays/actors/" in stripped:
                first_actor_include = index
        elif stripped and not stripped.startswith(("//", "/*", "*", "#pragma", "#ifndef", "#define", "#if", "#endif")):
            break
    if last_include < 0:
        return COMPAT_INCLUDE + "\n" + text
    lines.insert(last_include + 1, COMPAT_INCLUDE + "\n")
    if first_actor_include >= 0:
        lines.insert(first_actor_include, MM_PRELUDE_INCLUDE + "\n")
    return "".join(lines)


def rename_hooks(text):
    for ship, mm in HOOK_RENAMES.items():
        text = re.sub(rf'(SOH_REGISTER_HOOK\w*\(\s*[^,]+,\s*){ship}\b', rf"\g<1>{mm}", text)
        text = re.sub(rf'"{ship}"', f'"{mm}"', text)
    return adapt_hook_callbacks(text)


# Hooks whose arguments differ between the games: the mod's callback keeps Ship's signature and a generated
# adapter with MM's is registered in its place.
HOOK_ADAPTERS = {
    "OnPlayerPostLimbDraw": "static void {name}_Mm(Player* player, s32 limbIndex) {{\n"
                            "    {name}(gPlayState, player, limbIndex);\n}}\n",
    "OnGetItemDraw": "static void {name}_Mm(PlayState* play, s16* drawId, bool* handled) {{\n"
                     "    GetItemEntry entry = OotCompat_GetItemEntryForDrawId(*drawId);\n"
                     "    {name}(play, &entry, handled);\n}}\n",
    "OnGetItemDrawPost": "static void {name}_Mm(PlayState* play, s16 drawId) {{\n"
                         "    GetItemEntry entry = OotCompat_GetItemEntryForDrawId(drawId);\n"
                         "    {name}(play, &entry);\n}}\n",
}
ADAPTED_REGISTRATION = re.compile(r"(SOH_REGISTER_HOOK\w*\(\s*[^,]+,\s*(" + "|".join(HOOK_ADAPTERS) +
                                  r")\s*,\s*(?:[^,()]+,\s*)?)(\w+)(\s*\))")


def function_end(text, name):
    definition = re.search(rf"^[ \t]*(?:static\s+)?(?:inline\s+)?[\w \t\*]+?\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{",
                           text, re.M)
    if definition is None:
        return -1
    depth = 0
    for index in range(definition.end() - 1, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index + 1
    return -1


def adapt_hook_callbacks(text):
    adapters = {}
    def register_adapter(match):
        hook, name = match.group(2), match.group(3)
        if function_end(text, name) < 0:
            return match.group(0)
        adapters[name] = HOOK_ADAPTERS[hook].format(name=name)
        return f"{match.group(1)}{name}_Mm{match.group(4)}"
    text = ADAPTED_REGISTRATION.sub(register_adapter, text)
    for name, adapter in adapters.items():
        end = function_end(text, name)
        text = text[:end] + "\n\n" + adapter + text[end:]
    return text


SAVE_FIELDS = json.loads((Path(__file__).resolve().parent / "oot_save_fields.json").read_text(encoding="utf-8"))
SAVE_FIELD = re.compile(r"\bgSaveContext\.(\w+)")
SAVE_POINTER = re.compile(r"\bSaveContext\s*\*\s*(\w+)")
BUTTON_ITEM = re.compile(r"\bgSaveContext\.save\.saveInfo\.equips\.buttonItems\[([^\]]+)\]")
BUTTON_SLOT = re.compile(r"\bgSaveContext\.save\.saveInfo\.equips\.cButtonSlots\[([^\]]+)\]")


# OoT's SaveContext is flat; MM nests the same fields (oot_save_fields.json, from gen_oot_compat.py).
def relocate_save_fields(text):
    text = SAVE_FIELD.sub(lambda match: "gSaveContext." + SAVE_FIELDS.get(match.group(1), match.group(1)), text)
    for pointer in set(SAVE_POINTER.findall(text)):
        access = re.compile(rf"\b{re.escape(pointer)}->(\w+)")
        text = access.sub(lambda match: f"{pointer}->" + SAVE_FIELDS.get(match.group(1), match.group(1)), text)
    text = BUTTON_ITEM.sub(r"(*OotCompat_ButtonItem(\1))", text)
    text = BUTTON_SLOT.sub(r"(*OotCompat_ButtonSlot(\1))", text)
    for pattern, replacement in MAGIC_TARGET_REWRITES:
        text = pattern.sub(replacement, text)
    return text


MAGIC = r"gSaveContext\.save\.saveInfo\.playerData\.magic"
MAGIC_TARGET_REWRITES = [
    (re.compile(MAGIC + r"\s*=\s*gSaveContext\.magicTarget\s*;"), "OotCompat_Magic_ApplyTarget();"),
    (re.compile(r"gSaveContext\.magicTarget\s*=\s*" + MAGIC + r"\s*;"), "OotCompat_Magic_ClearTarget();"),
    (re.compile(r"gSaveContext\.magicTarget\b(?!\s*=[^=])"), "OotCompat_Magic_GetTarget()"),
]


COLLIDER_INFO = re.compile(r"([A-Za-z_]\w*)(\s*\[[^\]]*\])?(\.|->)info\b(?=(\.\w+)?)")
COLLIDER_NAME = re.compile(r"collider|cyl|quad|tris|jntsph|hurtbox|hitbox|^hit$", re.I)
ELEMENT_MEMBER = re.compile(r"\.(atDmgInfo|acDmgInfo|atElemFlags|acElemFlags|ocElemFlags|elemMaterial|atHit|acHit|"
                            r"atHitElem|acHitElem|bumpedPos)$")


COLLIDER_DECLARATION = re.compile(r"\b(Collider(?:Cylinder|Quad|JntSph|Tris)\w*)\s*\*?\s*(\w+)")


# OoT names a collider's element `info`; MM calls it `elem` on cylinders and quads and `base` on JntSph/Tris
# elements. Other structs keep `info` (LightNode): the declared type decides, else the name or the member read.
def rename_collider_info(match, declared):
    name, index, accessor, member = match.group(1), match.group(2) or "", match.group(3), match.group(4) or ""
    declared_type = declared.get(name, "")
    if "Element" in declared_type:
        return f"{name}{index}{accessor}base"
    if declared_type.startswith(("ColliderCylinder", "ColliderQuad")):
        return f"{name}{index}{accessor}elem"
    if "lement" in name or name.endswith("Items") or name in ("elem", "element"):
        return f"{name}{index}{accessor}base"
    if COLLIDER_NAME.search(name) or ELEMENT_MEMBER.match(member):
        return f"{name}{index}{accessor}elem"
    if index:
        return f"{name}{index}{accessor}base"
    return match.group(0)


SIMPLE_MEMBER_PATTERN = re.compile(r"^\(\\\.\|->\)(\w+)\\b$")
MEMBER_RENAMES = [(re.compile(rf"\b{match.group(1)}\b(?=\s*(?:\[[^\]]*\]\s*)*[;,:])"), replacement[2:])
                  for pattern, replacement in FIELD_REWRITES
                  for match in [SIMPLE_MEMBER_PATTERN.match(pattern.pattern)]
                  if match and re.fullmatch(r"\\1\w+", replacement)]
STRUCT_BODY = re.compile(r"\b(?:struct|union)\s*\w*\s*\{[^{}]*\}")


# A struct the mod declares itself keeps its members in step with the renamed accesses.
def rename_struct_members(text):
    def rename(match):
        body = match.group(0)
        for pattern, replacement in MEMBER_RENAMES:
            body = pattern.sub(replacement, body)
        return body
    return STRUCT_BODY.sub(rename, text)


# The save goes first: its OoT names (entranceIndex) are what the relocation table knows.
def rewrite_fields(text):
    text = rename_struct_members(rewrite_actor_members(drop_ship_only_code(relocate_save_fields(text))))
    for pattern, replacement in FIELD_REWRITES:
        text = pattern.sub(replacement, text)
    declared = {name: collider_type for collider_type, name in COLLIDER_DECLARATION.findall(text)}
    return COLLIDER_INFO.sub(lambda match: rename_collider_info(match, declared), text)


ASSET_KINDS = ("objects", "textures", "overlays", "misc", "scenes")
ASSET_PATH = re.compile(r'"(__OTR__)?(' + "|".join(ASSET_KINDS) + r')/(\w+)/([^"]*)')


ASSET_DEFINITION = re.compile(r"^([ \t]*static const ALIGN_ASSET\(\d+\) char )(\w+)(\[\]\s*=)", re.M)


ASSET_XML_VERSION = "N64_NTSC_10"
XML_FILE = re.compile(r'<File\s+Name="(\w+)"(.*?)</File>', re.S)
XML_SYMBOL = re.compile(r'\sName="(\w+)"')


class OotAssets:
    def __init__(self, ship_root):
        assets = ship_root / "soh" / "assets"
        self.folders = {kind: {child.name for child in (assets / kind).iterdir() if child.is_dir()}
                        for kind in ASSET_KINDS if (assets / kind).is_dir()}
        self.symbols = self.load_symbols(ship_root / "build" / "soh" / "assets" / "xml" / ASSET_XML_VERSION)
        self.mm_symbols = set()
        for header in (REPOSITORY_ROOT / "mm" / "assets").rglob("*.h"):
            text = header.read_text(encoding="utf-8", errors="ignore")
            self.mm_symbols.update(match.group(2) for match in ASSET_DEFINITION.finditer(text))
        keep = REPOSITORY_ROOT / "mm" / "assets" / "objects" / "gameplay_keep" / "gameplay_keep.h"
        self.mm_keep_symbols = {match.group(2) for match in
                                ASSET_DEFINITION.finditer(keep.read_text(encoding="utf-8", errors="ignore"))}

    # (kind, file) -> resource names, from the ZAPD descriptions Ship extracts OoT with.
    @staticmethod
    def load_symbols(xml_root):
        symbols = {}
        for xml in xml_root.rglob("*.xml") if xml_root.is_dir() else []:
            kind = xml.relative_to(xml_root).parts[0]
            for file_name, body in XML_FILE.findall(xml.read_text(encoding="utf-8", errors="ignore")):
                symbols.setdefault((kind, file_name), set()).update(XML_SYMBOL.findall(body))
        return symbols

    # Only resources OoT has move to oot_<file>, where oot_assets mounts them; a mod's own resource in a folder
    # of the same name (misc/link_animetion/<its clip>) stays where its archive puts it.
    def is_oot_resource(self, kind, file_name, rest):
        if file_name not in self.folders.get(kind, ()):
            return False
        resource = rest.split("/")[0]
        if not resource or resource.endswith(".h") or (kind, file_name) not in self.symbols:
            return True
        return resource in self.symbols[(kind, file_name)]

    def namespace(self, text):
        def rename(match):
            prefix, kind, file_name, rest = match.group(1) or "", match.group(2), match.group(3), match.group(4)
            if not self.is_oot_resource(kind, file_name, rest):
                return match.group(0)
            return f'"{prefix}{kind}/oot_{file_name}/{rest}'
        return ASSET_PATH.sub(rename, text)

    # OoT asset symbols MM's headers also define (gEffFlash1DL) become NAME_oot in every file of the mod, so MM's
    # headers, which MM's actor headers need, can be included next to OoT's.
    def separate_symbols(self, files):
        colliding = set()
        for text in files.values():
            colliding.update(match.group(2) for match in ASSET_DEFINITION.finditer(text))
        # gameplay_keep is shared engine data (effects, Link's animations made for MM's skeleton): MM's own
        # definition, which z64oot_names.h includes, replaces OoT's.
        preferred = colliding & self.mm_keep_symbols
        files = {path: self.drop_definitions(text, preferred) for path, text in files.items()}
        colliding = (colliding & self.mm_symbols) - preferred
        if not colliding:
            return files
        # String literals are matched first and kept, so resource paths keep the asset's real name.
        symbol = re.compile(r'"(?:\\.|[^"\\\n])*"|\b(' + "|".join(sorted(colliding)) + r")\b")
        rename = lambda match: match.group(0) if match.group(1) is None else match.group(1) + "_oot"
        return {path: symbol.sub(rename, text) for path, text in files.items()}

    @staticmethod
    def drop_definitions(text, names):
        if not names:
            return text
        alternatives = "|".join(sorted(names))
        text = re.sub(rf'^[ \t]*#define d({alternatives})\s+"[^"]*"[ \t]*\n', "", text, flags=re.M)
        return re.sub(rf"^[ \t]*static const ALIGN_ASSET\(\d+\) char ({alternatives})\[\]\s*=[^;]*;[ \t]*\n", "",
                      text, flags=re.M)


def translate_source(text, oot_assets):
    return insert_compat_include(oot_assets.namespace(rewrite_fields(rename_hooks(text))))


VENDOR_DIRECTORY = "oot_include"
# OoT and MM headers of one name share a guard (OBJECTS_GAMEPLAY_KEEP_H); a copied one must not hide MM's.
INCLUDE_GUARD = re.compile(r"^#ifndef (\w+)[ \t]*\n#define \1\b", re.M)
OOT_ASSET_FOLDER = re.compile(r"^(" + "|".join(ASSET_KINDS) + r")/oot_")


def ship_include_roots(ship_root):
    return [ship_root / "soh" / sub for sub in ("include", "src", "assets", "")]


def is_resolvable(header, including_path, roots):
    return (including_path.parent / header).is_file() or any((root / header).is_file() for root in roots)


# OoT headers MM lacks (actor structs, asset symbols) are copied from Ship into <mod>/oot_include, translated too.
def vendor_missing_headers(destination, sources, roots, ship_root, oot_assets):
    vendor = destination / VENDOR_DIRECTORY
    ship_roots = ship_include_roots(ship_root)
    pending = [(path, None, text) for path, text in sources.items()]
    vendored = {}
    while pending:
        path, ship_path, text = pending.pop()
        for header in INCLUDE.findall(text):
            if is_resolvable(header, path, roots + [vendor]):
                continue
            beside = ship_path.parent / header if ship_path else None
            if beside is not None and beside.is_file():
                found, target = beside.resolve(), (path.parent / header).resolve()
            else:
                ship_header = OOT_ASSET_FOLDER.sub(r"\1/", header)
                found = next((root / ship_header for root in ship_roots if (root / ship_header).is_file()), None)
                target = vendor / header
            if found is None or target in vendored:
                continue
            translated = INCLUDE_GUARD.sub(r"#ifndef OOT_\1\n#define OOT_\1",
                                           translate_source(read_text(found)[0], oot_assets), count=1)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(translated, encoding="utf-8")
            vendored[target] = translated
            pending.append((target, found, translated))
    return vendored


def load_compat_names():
    names = set()
    for header in (SDK_DIRECTORY / "include" / "oot_compat").rglob("*.h"):
        names.update(re.findall(r"#define\s+([A-Za-z_]\w*)", header.read_text(encoding="utf-8")))
    return names


def collect_review_items(path, text, compat_names):
    items = []
    for number, line in enumerate(text.splitlines(), start=1):
        for pattern, reason in REVIEW_PATTERNS:
            for match in pattern.finditer(line):
                if match.group(0) not in compat_names:
                    items.append((path, number, match.group(0), reason))
    return items


def translate_manifest(path):
    manifest = json.loads(path.read_text(encoding="utf-8"))
    manifest["game"] = "mm"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def write_report(destination, source, items):
    lines = [f"# Translation of {source.name}", "",
             f"Translated from `{source}` by mod-sdk/tools/translate_mod.py. Every line below needs a person.",
             "Fields of OoT actor structs are not checked here: the first build lists those.", ""]
    if not items:
        lines.append("Nothing flagged. Build it and test it in game.")
    for path, number, token, reason in items:
        lines.append(f"- `{path}:{number}` `{token}`: {reason}")
    (destination / "TRANSLATION.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def read_text(path):
    raw = path.read_bytes()
    try:
        return raw.decode("utf-8"), "utf-8"
    except UnicodeDecodeError:
        return raw.decode("cp1252"), "cp1252"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("source", type=Path, help="Ship mod folder (the one with manifest.json)")
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--name", help="Target folder name; defaults to the source folder name")
    parser.add_argument("--force", action="store_true", help="Replace an existing target folder")
    parser.add_argument("--ship", type=Path, default=REPOSITORY_ROOT.parent.parent / "Shipwright-unbound",
                        help="Shipwright checkout the OoT headers MM lacks are copied from")
    args = parser.parse_args()
    if not (args.ship / "soh" / "include").is_dir():
        sys.exit(f"Ship checkout not found at {args.ship}")
    oot_assets = OotAssets(args.ship)

    source = args.source.resolve()
    if not (source / "manifest.json").is_file():
        sys.exit(f"{source} has no manifest.json")
    destination = (args.output_root / (args.name or source.name)).resolve()
    if destination.exists():
        if not args.force:
            sys.exit(f"{destination} exists; pass --force to replace it")
        shutil.rmtree(destination)
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns("build", "__pycache__"))

    sources = [path for path in sorted(destination.rglob("*"))
               if path.suffix in SOURCE_SUFFIXES and "assets" not in path.relative_to(destination).parts]
    translated_sources = {}
    encodings = {}
    for path in sources:
        text, encodings[path] = read_text(path)
        translated_sources[path] = translate_source(text, oot_assets)

    roots = include_roots()
    vendored = vendor_missing_headers(destination, translated_sources, roots, args.ship, oot_assets)
    roots.append(destination / VENDOR_DIRECTORY)
    separated = oot_assets.separate_symbols({**translated_sources, **vendored})
    translated_sources = {path: separated[path] for path in translated_sources}
    vendored = {path: separated[path] for path in vendored}
    for path, text in separated.items():
        path.write_bytes(text.encode(encodings.get(path, "utf-8")))
    mm_identifiers = load_mm_identifiers()
    compat_names = load_compat_names()
    local_identifiers = set()
    for text in translated_sources.values():
        local_identifiers.update(re.findall(r"#define\s+([A-Za-z_]\w*)", text))
    for text in vendored.values():
        local_identifiers.update(IDENTIFIER.findall(text))
    review_items = []
    for path, text in translated_sources.items():
        relative = path.relative_to(destination).as_posix()
        review_items += collect_missing_includes(relative, text, destination, roots)
        review_items += collect_review_items(relative, text, compat_names)
        review_items += collect_unknown_constants(relative, text, mm_identifiers, local_identifiers)
    review_items.sort(key=lambda item: (item[0], item[1]))
    translate_manifest(destination / "manifest.json")
    write_report(destination, source, review_items)
    print(f"Translated into {destination}: {len(review_items)} line(s) to review in TRANSLATION.md")


if __name__ == "__main__":
    main()
