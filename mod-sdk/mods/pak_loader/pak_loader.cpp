#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/ModMenu/ModUI.h"
extern "C" {
#include "global.h"
const char* CVarGetString(const char*, const char*);
void CVarSetString(const char*, const char*);
float CVarGetFloat(const char*, float);
void CVarSetFloat(const char*, float);
void CVarSave(void);
int32_t CVarGetInteger(const char*, int32_t);
void CVarSetInteger(const char*, int32_t);
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
Gfx* ResourceMgr_LoadGfxByName(const char* path);
int ResourceMgr_OTRSigCheck(char* imgData);
}
namespace pak {
using Api = S2HModApi;
using UserFile = S2HUserFile;
} // namespace pak
#include "appearance.h"
#include <span>

enum Appearance { APPEARANCE_NONE = -1, APPEARANCE_HUMAN, APPEARANCE_ADULT, APPEARANCE_DEITY, APPEARANCE_COUNT };

static const S2HModApi* sApi;
static pak::Library sLibrary;
static pak::Mixer sMixer;
static pak::Composer sComposer;
static pak::EmotePlayer sEmotePlayer;
static pak::ModelPicker sModelPicker;
static bool sOtherPlayer = false;
static bool sEmoteRequested = false;
static pak::ChoiceList sVoiceChoices("Default voice");
static pak::ChoiceList sEmoteChoices("No emote");
static std::vector<pak::EquipSlot> sEquipSlots;
static pak::VoicePack* sVoices[APPEARANCE_COUNT] = {};
static pak::Emote* sEmote = nullptr;
static const char* const sCVars[] = { "gMods.PakLoader.Human", "gMods.PakLoader.Adult", "gMods.PakLoader.Deity" };
static const char* const sScaleCVars[] = { "gMods.PakLoader.HumanRoot", "gMods.PakLoader.AdultRoot",
                                           "gMods.PakLoader.DeityRoot" };
static const pak::AppearanceRow sRows[] = {
    { "Link Model", sCVars[0], "gMods.PakLoader.HumanType", sScaleCVars[0], pak::BODY_MM_CHILD },
    { "Adult Link Model", sCVars[1], "gMods.PakLoader.AdultType", sScaleCVars[1], pak::BODY_OOT_ADULT },
    { "Fierce Deity Model", sCVars[2], "gMods.PakLoader.DeityType", sScaleCVars[2], pak::BODY_MM_DEITY },
};
static const char* const sVoiceMenuCVars[] = { "gMods.PakLoader.VoiceMenu.Human", "gMods.PakLoader.VoiceMenu.Adult",
                                               "gMods.PakLoader.VoiceMenu.Deity" };
static const char* const sVoiceCVars[] = { "gMods.PakLoader.Voice.Human", "gMods.PakLoader.Voice.Adult",
                                           "gMods.PakLoader.Voice.Deity" };
static const char* const sSlotNames[] = { "Link", "Adult Link", "Fierce Deity" };
static const char* const sDestinationSkeletons[] = { "objects/object_link_child/gLinkHumanSkel",
                                                     "objects/oot_object_link_boy/gLinkAdultSkel",
                                                     "objects/object_link_boy/gLinkFierceDeitySkel" };

namespace pak {
const char* BaseSkeletonPath(bool childSized) {
    return childSized ? sDestinationSkeletons[APPEARANCE_HUMAN] : sDestinationSkeletons[APPEARANCE_DEITY];
}

const char* ResolveKeepTexture(const char* name) {
    static std::set<std::string> paths;
    std::string path = std::string("objects/oot_gameplay_keep/") + name;
    if (!sApi->HasResource(path.c_str())) {
        return nullptr;
    }
    return paths.insert("__OTR__" + path).first->c_str();
}

void Log(const Api* api, const char* message) {
    if (S2H_MOD_API_HAS(api, Log)) {
        api->Log(message);
    }
}

bool CanReadModsFolder(const Api* api) {
    return S2H_MOD_API_HAS(api, ReadModsFolderFiles);
}
} // namespace pak

static bool Has(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

static Appearance ActiveAppearance(Player* player) {
    if (sOtherPlayer || gPlayState == nullptr || player == nullptr) {
        return APPEARANCE_NONE;
    }
    if (player->transformation == PLAYER_FORM_FIERCE_DEITY) {
        return APPEARANCE_DEITY;
    }
    if (player->transformation != PLAYER_FORM_HUMAN) {
        return APPEARANCE_NONE;
    }
    const char* form = sApi->GetActiveForm();
    if (form != nullptr && !strcmp(form, S2H_FORM_KEY_ADULT)) {
        return APPEARANCE_ADULT;
    }
    if (form != nullptr && *form && strcmp(form, S2H_FORM_KEY_HUMAN)) {
        return APPEARANCE_NONE;
    }
    return APPEARANCE_HUMAN;
}

static pak::Model* Selected(Appearance slot) {
    if (slot == APPEARANCE_NONE || !CVarGetInteger("gMods.PakLoader.Enabled", 1)) {
        return nullptr;
    }
    std::string key = CVarGetString(sCVars[slot], "");
    if (key.empty()) {
        return nullptr;
    }
    pak::Model* model = sLibrary.Load(key);
    if (model == nullptr && sLibrary.Find(key) != nullptr) {
        CVarSetString(sCVars[slot], "");
    }
    return model;
}

static Gfx* LoadVanilla(const std::string& path) {
    return sApi->HasResource(path.c_str()) ? ResourceMgr_LoadGfxByName(path.c_str()) : nullptr;
}

static bool IsHoldingItem(const std::string& original) {
    static const char* const kHeld[] = { "Hold",     "Bgs",       "GiantsKnife", "Bottle",    "Hammer", "Bow",
                                         "Hookshot", "Slingshot", "Ocarina",     "Boomerang", "Sword",  "Shield" };
    for (const char* held : kHeld) {
        if (Has(original, held)) {
            return true;
        }
    }
    return false;
}

struct AliasRule {
    const char* part;
    uint32_t alias;
};

static Gfx* HandEquipment(pak::Model& model, const std::string& original, bool left) {
    static constexpr AliasRule kLeft[] = { { "Bottle", 0x50A8 },
                                           { "Bgs", 0x5458 },
                                           { "GiantsKnife", 0x54F0 },
                                           { "HoldingKokiriSword", 0x5440 },
                                           { "HoldingMasterSword", 0x5448 },
                                           { "Hammer", 0x5460 },
                                           { "Closed", 0x50A0 } };
    static constexpr AliasRule kRight[] = { { "Mirror", 0x5478 },      { "DekuShield", 0x5468 }, { "Hylian", 0x5470 },
                                            { "HerosShield", 0x5470 }, { "Bow", 0x5480 },        { "Hookshot", 0x5488 },
                                            { "Ocarina", 0x5490 },     { "Closed", 0x50B8 } };
    uint32_t alias = 0;
    for (const AliasRule& rule : left ? std::span<const AliasRule>(kLeft) : std::span<const AliasRule>(kRight)) {
        if (Has(original, rule.part)) {
            alias = rule.alias;
            break;
        }
    }
    if (alias == 0 && !IsHoldingItem(original)) {
        alias = left ? 0x5098 : 0x50B0;
    }
    return pak::FindAlias(model, alias);
}

static Gfx* HandBase(pak::Model* model, pak::PieceKind kind, int limb, Appearance slot) {
    uint32_t alias = pak::HandAlias(kind);
    if (model != nullptr && !model->native) {
        Gfx* own = pak::FindAlias(*model, alias);
        return own != nullptr ? own : model->limbs[limb - 1].dLists[0];
    }
    bool adult = slot == APPEARANCE_ADULT;
    std::string path = adult ? "objects/oot_object_link_boy/gLinkAdult" : "objects/object_link_child/gLinkHuman";
    if (alias == pak::kLeftFist) {
        path += adult ? "LeftHandClosedNearDL" : "LeftHandClosedDL";
    } else if (alias == pak::kRightFist) {
        path += adult ? "RightHandClosedNearDL" : "RightHandClosedDL";
    } else {
        path += adult ? "RightHandNearDL" : "RightHandOpenDL";
    }
    if (model != nullptr) {
        std::string native = model->NativeResourceFor(path);
        if (Gfx* list = native.empty() ? nullptr : LoadVanilla(native)) {
            return list;
        }
    }
    return LoadVanilla(path);
}

static Gfx* ComposeHand(pak::Model* model, int limb, const std::string& original, Appearance slot) {
    pak::HeldItem item;
    bool left = limb == PLAYER_LIMB_LEFT_HAND;
    if (!pak::ParseHeldItem(pak::Leaf(original), item) || pak::IsLeftHandItem(item.kind) != left) {
        return nullptr;
    }
    pak::Piece* piece = pak::FindEquipped(sEquipSlots, item);
    Gfx* hand = piece != nullptr ? HandBase(model, item.kind, limb, slot) : nullptr;
    if (hand == nullptr) {
        return nullptr;
    }
    if (item.kind == pak::PieceKind::Sword) {
        return sComposer.Compose({ { hand }, { piece->hilt }, { piece->blade } });
    }
    return sComposer.Compose({ { hand }, { piece->main } });
}

static float DestinationLegLength(Appearance slot) {
    static float lengths[APPEARANCE_COUNT];
    if (lengths[slot] == 0.f) {
        try {
            lengths[slot] = pak::SkeletonLegLength(pak::LoadLinkSkeleton(sDestinationSkeletons[slot]));
        } catch (const std::exception&) { lengths[slot] = -1.f; }
    }
    return lengths[slot];
}

static void PlaceLimb(const pak::Model& model, Appearance slot, s32 limb, Vec3f* pos) {
    if (pos == nullptr) {
        return;
    }
    if (limb != PLAYER_LIMB_ROOT) {
        Vec3s joint = model.limbs[limb - 1].jointPos;
        *pos = { float(joint.x), float(joint.y), float(joint.z) };
        return;
    }
    float destination = DestinationLegLength(slot);
    float ratio = model.legLength > 0.f && destination > 0.f ? model.legLength / destination : 1.f;
    float factor = ratio * CVarGetFloat(sScaleCVars[slot], 1.f);
    pos->x *= factor;
    pos->y *= factor;
    pos->z *= factor;
}

static void ResolveBody(Player* player, s32 limb, Gfx** dList, Gfx*, Vec3f* pos) {
    if (player != GET_PLAYER(gPlayState) || dList == nullptr || limb <= 0 || limb > s32(pak::kLimbCount)) {
        return;
    }
    Appearance slot = ActiveAppearance(player);
    pak::Model* model = Selected(slot);
    bool equipment = (slot == APPEARANCE_HUMAN || slot == APPEARANCE_ADULT) && pak::HasEquipped(sEquipSlots);
    if (model == nullptr && !equipment) {
        return;
    }
    if (model != nullptr) {
        PlaceLimb(*model, slot, limb, pos);
    }
    if (*dList == nullptr) {
        return;
    }
    std::string original;
    if (ResourceMgr_OTRSigCheck(reinterpret_cast<char*>(*dList))) {
        original = reinterpret_cast<const char*>(*dList);
    }
    bool hand = limb == PLAYER_LIMB_LEFT_HAND || limb == PLAYER_LIMB_RIGHT_HAND;
    if (equipment && hand && !original.empty()) {
        if (Gfx* composed = ComposeHand(model, limb, original, slot)) {
            *dList = composed;
            return;
        }
    }
    if (model == nullptr) {
        return;
    }
    if (model->native && !original.empty()) {
        std::string path = model->NativeResourceFor(original);
        if (!path.empty() && sApi->HasResource(path.c_str())) {
            *dList = ResourceMgr_LoadGfxByName(path.c_str());
            return;
        }
    }
    if (limb == PLAYER_LIMB_SHEATH) {
        return;
    }
    if (hand) {
        if (Gfx* equipped = HandEquipment(*model, original, limb == PLAYER_LIMB_LEFT_HAND)) {
            *dList = equipped;
            return;
        }
        if (IsHoldingItem(original)) {
            return;
        }
    }
    *dList = model->limbs[limb - 1].dLists[0];
}

static void ResolveFaceTexture(const pak::Model& model, const char** texture, bool eyes) {
    static std::set<std::string> paths;
    int index = pak::FaceIndex(*texture, eyes);
    if (!model.native) {
        index = index < 0 ? 0 : index;
        const uint8_t* data = eyes ? model.EyeTexture(index) : model.MouthTexture(index);
        *texture = reinterpret_cast<const char*>(data);
        return;
    }
    if (index < 0 && !model.deity) {
        return;
    }
    std::string path = model.NativeFaceName(index, eyes);
    if (sApi->HasResource(path.c_str())) {
        *texture = paths.insert("__OTR__" + path).first->c_str();
    }
}

static void ResolveFace(const char** eyes, const char** mouth) {
    if (gPlayState == nullptr || eyes == nullptr || mouth == nullptr) {
        return;
    }
    pak::Model* model = Selected(ActiveAppearance(GET_PLAYER(gPlayState)));
    if (model == nullptr) {
        return;
    }
    ResolveFaceTexture(*model, eyes, true);
    ResolveFaceTexture(*model, mouth, false);
}

static void ReplaceVoice(Actor* actor, s32, u16* sfxId, bool* handled) {
    if (*handled || gPlayState == nullptr || actor != &GET_PLAYER(gPlayState)->actor || sOtherPlayer) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    Appearance slot = ActiveAppearance(player);
    pak::VoicePack* voice = slot == APPEARANCE_NONE ? nullptr : sVoices[slot];
    if (voice == nullptr) {
        return;
    }
    uint16_t id = *sfxId;
    bool isVoice = id >= 0x6800 && id < 0x6900;
    uint16_t offset = isVoice && player->ageProperties != nullptr ? player->ageProperties->voiceSfxIdOffset : 0;
    uint32_t roll = pak::NextRoll();
    const pak::Pcm* clip = voice->Pick(uint16_t(id - offset), roll);
    if (clip == nullptr) {
        clip = voice->Pick(id, roll);
    }
    if (clip != nullptr) {
        sMixer.Play(clip, 1.f);
        *handled = true;
    }
}

static void MixVoices(int16_t* stereo, uint32_t frames) {
    sMixer.Mix(stereo, frames);
}

static bool WantsToAct(const Input* input) {
    constexpr uint16_t kActions = BTN_A | BTN_B | BTN_Z | BTN_R | BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT;
    return (input->press.button & kActions) != 0 || std::abs(input->rel.stick_x) > 20 ||
           std::abs(input->rel.stick_y) > 20;
}

static void UpdateEmote(Player* player) {
    const Input* input = &gPlayState->state.input[0];
    bool shortcut = CHECK_BTN_ALL(input->cur.button, BTN_L) && CHECK_BTN_ALL(input->press.button, BTN_DDOWN);
    if (sEmotePlayer.IsPlaying()) {
        if (WantsToAct(input) || player->actor.speed > 0.5f) {
            sEmotePlayer.Stop();
            return;
        }
        sEmotePlayer.Advance();
        return;
    }
    if (sEmote != nullptr && (shortcut || sEmoteRequested)) {
        sEmotePlayer.Start(sEmote, sMixer, 1.f);
    }
    sEmoteRequested = false;
}

static void DrawBegin(Actor* actor, PlayState* play) {
    if (actor->id != ACTOR_PLAYER) {
        return;
    }
    sOtherPlayer = actor != &GET_PLAYER(play)->actor;
    if (!sOtherPlayer && sEmotePlayer.IsPlaying()) {
        sEmotePlayer.Pose(reinterpret_cast<Player*>(actor)->skelAnime.jointTable);
    }
}

static void DrawEnd(Actor* actor, PlayState*) {
    if (actor->id == ACTOR_PLAYER) {
        sEmotePlayer.Unpose();
        sOtherPlayer = false;
    }
}

static void Tint(bool* should, va_list) {
    if (gPlayState != nullptr && Selected(ActiveAppearance(GET_PLAYER(gPlayState))) != nullptr) {
        *should = false;
    }
}

static void DrawModelRow(Appearance slot) {
    if (!sModelPicker.Draw(sRows[slot], sLibrary)) {
        return;
    }
    pak::Model* model = Selected(slot);
    sLibrary.status = std::string(sSlotNames[slot]) + ": " +
                      (model != nullptr ? pak::Library::Describe(model->label, model->author) : "default appearance.");
}

static void HumanRow() {
    DrawModelRow(APPEARANCE_HUMAN);
}

static void AdultRow() {
    DrawModelRow(APPEARANCE_ADULT);
}

static void DeityRow() {
    DrawModelRow(APPEARANCE_DEITY);
}

static void ResolveVoices() {
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        sVoices[slot] = sLibrary.FindVoice(CVarGetString(sVoiceCVars[slot], ""));
        if (sVoices[slot] != nullptr) {
            sVoices[slot]->Decode();
        }
    }
}

static void VoicesChanged() {
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        sVoiceChoices.Apply(sVoiceMenuCVars[slot], sVoiceCVars[slot]);
    }
    ResolveVoices();
}

static void EquipmentChanged() {
    for (auto& slot : sEquipSlots) {
        slot.selected = sLibrary.FindPiece(slot.choices.Apply(slot.indexCVar.c_str(), slot.keyCVar.c_str()));
    }
}

static void EmoteChanged() {
    sEmote = sLibrary.FindEmote(sEmoteChoices.Apply("gMods.PakLoader.EmoteMenu", "gMods.PakLoader.Emote"));
    sEmotePlayer.Stop();
}

static void ResetAll() {
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        CVarSetString(sCVars[slot], "");
        CVarSetInteger(sRows[slot].typeCVar, sRows[slot].defaultType);
        CVarSetString(sVoiceCVars[slot], "");
        CVarSetFloat(sScaleCVars[slot], 1.f);
        sVoiceChoices.Sync(sVoiceMenuCVars[slot], sVoiceCVars[slot]);
        sVoices[slot] = nullptr;
    }
    for (auto& slot : sEquipSlots) {
        CVarSetString(slot.keyCVar.c_str(), "");
        slot.choices.Sync(slot.indexCVar.c_str(), slot.keyCVar.c_str());
        slot.selected = nullptr;
    }
    CVarSave();
    sLibrary.status = "Default models, voices, equipment and root adjustments restored.";
}

static void ResetAction() {
    if (ModUI_Button("Restore Defaults", -1, 0)) {
        ResetAll();
    }
}

static void PlayEmoteAction() {
    if (ModUI_Button("Play Emote", -1, 0)) {
        sEmoteRequested = true;
    }
}

static void ImportAction() {
    if (ModUI_Button("Import PAK / ZOBJ", -1, 0)) {
        sLibrary.PickAndImport();
    }
}

static void ArchiveAction() {
    if (ModUI_Button("Install O2R / OTR", -1, 0)) {
        sLibrary.InstallArchive();
    }
}

static void StatusRow() {
    ModUI_TextWrapped(sLibrary.status.c_str());
    if (sLibrary.restartNeeded && S2H_MOD_API_HAS(sApi, RequestRestart) &&
        ModUI_Button("Restart to Update Lists", -1, 0)) {
        sApi->RequestRestart("Load the newly installed packs.");
    }
}

static S2HModMenuWidget Widget(S2HModMenuWidgetType type, const char* label, uint8_t column = 0) {
    S2HModMenuWidget widget{};
    widget.structSize = sizeof(widget);
    widget.section = "Mods";
    widget.sidebar = "PAK Loader";
    widget.type = type;
    widget.label = label;
    widget.column = column;
    return widget;
}

static void RegisterWidget(S2HModMenuWidget widget) {
    sApi->RegisterMenuWidget(&widget);
}

static void RegisterCustom(const char* label, S2HModMenuCallback draw, uint8_t column = 0) {
    S2HModMenuWidget widget = Widget(S2H_MOD_MENU_CUSTOM, label, column);
    widget.onChange = draw;
    RegisterWidget(widget);
}

static void RegisterCombo(const char* label, const char* cvar, const pak::ChoiceList& choices, const char* tooltip,
                          S2HModMenuCallback changed, uint8_t column) {
    S2HModMenuWidget widget = Widget(S2H_MOD_MENU_COMBOBOX, label, column);
    widget.cvar = cvar;
    widget.tooltip = tooltip;
    widget.values = choices.values.data();
    widget.valueCount = uint32_t(choices.values.size());
    widget.onChange = changed;
    RegisterWidget(widget);
}

static void RegisterAppearanceColumn() {
    RegisterWidget(Widget(S2H_MOD_MENU_TEXT, "Custom Player Models"));
    RegisterWidget(Widget(S2H_MOD_MENU_SEPARATOR, "Player Models Separator"));
    S2HModMenuWidget enable = Widget(S2H_MOD_MENU_CHECKBOX, "Enable Custom Player Model");
    enable.cvar = "gMods.PakLoader.Enabled";
    enable.defaultInt = 1;
    enable.tooltip = "Replace Link's appearance. Turn off to restore every original model without losing your "
                     "selections.";
    RegisterWidget(enable);
    static const S2HModMenuCallback kRows[] = { HumanRow, AdultRow, DeityRow };
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        RegisterCustom(sRows[slot].label, kRows[slot]);
    }
    RegisterWidget(Widget(S2H_MOD_MENU_SEPARATOR, "Voices Separator"));
    static const char* const kVoiceLabels[] = { "Link Voice", "Adult Link Voice", "Fierce Deity Voice" };
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        sVoiceChoices.Sync(sVoiceMenuCVars[slot], sVoiceCVars[slot]);
        RegisterCombo(kVoiceLabels[slot], sVoiceMenuCVars[slot], sVoiceChoices,
                      "Replace Link's voice clips with a ModLoader64 voice pack.", VoicesChanged, 0);
    }
    S2HModMenuWidget volume = Widget(S2H_MOD_MENU_FLOAT_SLIDER, "Voice Pack Volume");
    volume.cvar = "gMods.PakLoader.VoiceVolume";
    volume.defaultFloat = 1.f;
    volume.minFloat = 0.f;
    volume.maxFloat = 1.f;
    volume.tooltip = "Volume of voice pack and emote sounds, on top of the sound effects volume.";
    RegisterWidget(volume);
    RegisterCustom("Restore Defaults", ResetAction);
}

static void RegisterEquipmentColumn() {
    static const std::pair<pak::HeldItem, const char*> kSlots[] = {
        { { pak::PieceKind::Sword, 0 }, "Kokiri Sword" },   { { pak::PieceKind::Sword, 1 }, "Razor Sword" },
        { { pak::PieceKind::Sword, 2 }, "Gilded Sword" },   { { pak::PieceKind::Shield, 1 }, "Hero's Shield" },
        { { pak::PieceKind::Shield, 2 }, "Mirror Shield" }, { { pak::PieceKind::Bow, 0 }, "Hero's Bow" },
        { { pak::PieceKind::Hookshot, 0 }, "Hookshot" },    { { pak::PieceKind::Ocarina, 0 }, "Ocarina of Time" },
    };
    sEquipSlots.clear();
    sEquipSlots.reserve(std::size(kSlots));
    for (const auto& [item, label] : kSlots) {
        sEquipSlots.push_back({ item.kind, item.variant, label });
    }
    RegisterWidget(Widget(S2H_MOD_MENU_TEXT, "Equipment and Items", 1));
    RegisterWidget(Widget(S2H_MOD_MENU_SEPARATOR, "Equipment Separator", 1));
    for (auto& slot : sEquipSlots) {
        pak::BuildEquipSlot(slot, sLibrary);
        RegisterCombo(slot.label, slot.indexCVar.c_str(), slot.choices,
                      "Draw this item in Link's hand from any imported pack. Pieces of the same kind fit every slot.",
                      EquipmentChanged, 1);
    }
    RegisterWidget(Widget(S2H_MOD_MENU_TEXT, "Emotes", 1));
    RegisterWidget(Widget(S2H_MOD_MENU_SEPARATOR, "Emote Separator", 1));
    sEmoteChoices.Sync("gMods.PakLoader.EmoteMenu", "gMods.PakLoader.Emote");
    RegisterCombo("Emote", "gMods.PakLoader.EmoteMenu", sEmoteChoices, "Animation played with the emote shortcut.",
                  EmoteChanged, 1);
    RegisterCustom("Play Emote", PlayEmoteAction, 1);
    RegisterWidget(
        Widget(S2H_MOD_MENU_TEXT, "Hold L and press D-Pad Down to play the emote. Moving or acting stops it.", 1));
}

static void RegisterLibraryColumn() {
    RegisterWidget(Widget(S2H_MOD_MENU_TEXT, "Pack Library", 1));
    RegisterWidget(Widget(S2H_MOD_MENU_SEPARATOR, "Pack Library Separator", 1));
    RegisterCustom("Import Model File", ImportAction, 1);
    RegisterCustom("Install Model Archive", ArchiveAction, 1);
    RegisterWidget(Widget(S2H_MOD_MENU_TEXT, "New files appear in the lists after restarting.", 1));
    RegisterCustom("Pack Library Status", StatusRow, 1);
}

static void BuildChoices() {
    sModelPicker.Build(sLibrary);
    for (const auto& voice : sLibrary.voices) {
        sVoiceChoices.Add(voice->key, pak::Library::Describe(voice->label, voice->author));
    }
    for (const auto& emote : sLibrary.emotes) {
        sEmoteChoices.Add(emote->key, pak::Library::Describe(emote->label, emote->author));
    }
    sVoiceChoices.Finish();
    sEmoteChoices.Finish();
}

static void RegisterMenu() {
    sLibrary.Scan();
    BuildChoices();
    ResolveVoices();
    sEmote = sLibrary.FindEmote(CVarGetString("gMods.PakLoader.Emote", ""));
    sApi->RegisterMenuSidebar("Mods", "PAK Loader", "gMods.Menu.Page", 2);
    RegisterAppearanceColumn();
    RegisterEquipmentColumn();
    RegisterLibraryColumn();
}

static void Tick(Player* player) {
    if (!sLibrary.scanned) {
        sLibrary.Scan();
    }
    if (gPlayState != nullptr && player == GET_PLAYER(gPlayState)) {
        UpdateEmote(player);
    }
}

static const char* const sHooks[] = { "OnPlayerResolveLimbDraw", "OnPlayerResolveFaceTextures",
                                      "OnActorDrawBegin",        "OnActorDrawEnd",
                                      "OnPlayerUpdate",          "OnActorPlaySfx" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sHooks, ARRAY_COUNT(sHooks) };

extern "C" S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
    sLibrary.Bind(api);
}

extern "C" S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements() {
    return &sRequirements;
}

extern "C" S2H_MOD_EXPORT void ModInit() {
    if (!S2H_MOD_API_HAS(sApi, ListArchiveFiles) || !S2H_MOD_API_HAS(sApi, GetActiveForm) ||
        !S2H_MOD_API_HAS(sApi, HasResource) || !S2H_MOD_API_HAS(sApi, RegisterMenuSidebar)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveBody);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveFaceTextures, ResolveFace);
    S2H_REGISTER_HOOK(sApi, OnActorDrawBegin, DrawBegin);
    S2H_REGISTER_HOOK(sApi, OnActorDrawEnd, DrawEnd);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, Tick);
    S2H_REGISTER_HOOK(sApi, OnActorPlaySfx, ReplaceVoice);
    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR, Tint);
    if (S2H_MOD_API_HAS(sApi, RegisterAudioMixInGroup)) {
        sApi->RegisterAudioMixInGroup(MixVoices, S2H_AUDIO_GROUP_SFX, "gMods.PakLoader.VoiceVolume");
    }
    RegisterMenu();
}
