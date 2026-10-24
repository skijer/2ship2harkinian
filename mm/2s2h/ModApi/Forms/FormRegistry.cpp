#include "FormRegistry.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/debug/Console.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/ConsoleArguments.h"
#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/Forms/FormModel.h"
#include "2s2h/ModApi/Forms/FormTransform.h"
#include "2s2h/ModApi/Player/PlayerHookTypes.h"
#include "2s2h/ModApi/PlayerInput/PlayerInput.h"
#include "2s2h/SaveManager/SaveSections.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayerAnimationHeader* D_8085BE84[PLAYER_ANIMGROUP_MAX][PLAYER_ANIMTYPE_MAX];
extern PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX];
}

namespace {

constexpr const char* SaveSection = "forms";
constexpr int32_t EquipTypeCount = EQUIP_TYPE_TUNIC;
constexpr uint8_t ButtonCount = S2H_ITEM_BUTTON_COUNT;

struct RegisteredForm {
    std::string key;
    std::string label;
    std::string item;
    std::string modelPath;
    S2HFormDefinition definition = {};
    std::vector<S2HFormActionOverride> actions;
    std::vector<S2HFormAnimOverride> anims;
    PlayerAgeProperties body = {};
};

struct SavedAnim {
    s32 group;
    s32 animType;
    PlayerAnimationHeader* anim;
};

struct StashedLoadout {
    std::array<bool, EquipTypeCount> hasEquip{};
    std::array<uint16_t, EquipTypeCount> equip{};
    uint8_t buttonB = ITEM_NONE;
    std::array<bool, ButtonCount> hasButton{};
    std::array<uint8_t, ButtonCount> buttonSlot{};
    std::array<std::string, ButtonCount> buttonKey{};
};

std::deque<RegisteredForm> sForms;
std::unordered_map<std::string, RegisteredForm*> sFormsByKey;
// The form the save says Link wears, versus the one whose effects are live: only a Human Link shows a form.
RegisteredForm* sActiveForm = nullptr;
RegisteredForm* sAppliedForm = nullptr;
std::string sPendingForm;
StashedLoadout sStash;
PlayerAgeProperties sHumanBody = {};
bool sBodyPatched = false;
std::vector<SavedAnim> sSavedAnims;

Player* GetPlayer() {
    return gPlayState == nullptr ? nullptr : GET_PLAYER(gPlayState);
}

bool IsHuman(const Player* player) {
    return player != nullptr && player->transformation == PLAYER_FORM_HUMAN;
}

bool IsAppliedKind(S2HFormKind kind) {
    return sAppliedForm != nullptr && sAppliedForm->definition.kind == kind;
}

RegisteredForm* FindForm(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    auto form = sFormsByKey.find(key);
    return form == sFormsByKey.end() ? nullptr : form->second;
}

bool IsFormItemOwned(const RegisteredForm& form) {
    return form.item.empty() || CustomItemRegistry_IsOwned(form.item.c_str());
}

const S2HFormDefinition* ActiveDefinition() {
    return sActiveForm == nullptr ? nullptr : &sActiveForm->definition;
}

uint8_t SwordItemFor(uint16_t swordValue) {
    switch (swordValue) {
        case EQUIP_VALUE_SWORD_KOKIRI:
            return ITEM_SWORD_KOKIRI;
        case EQUIP_VALUE_SWORD_RAZOR:
            return ITEM_SWORD_RAZOR;
        case EQUIP_VALUE_SWORD_GILDED:
            return ITEM_SWORD_GILDED;
        case EQUIP_VALUE_SWORD_DIETY:
            return ITEM_SWORD_DEITY;
        default:
            return ITEM_NONE;
    }
}

void ChangeSword(uint16_t swordValue) {
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, swordValue);
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = SwordItemFor(swordValue);
}

bool IsDpadButton(uint8_t button) {
    return button >= S2H_ITEM_BUTTON_DPAD_FIRST && button <= S2H_ITEM_BUTTON_DPAD_LAST;
}

bool IsStashableButton(uint8_t button) {
    return (button >= S2H_ITEM_BUTTON_C_FIRST && button <= S2H_ITEM_BUTTON_C_LAST) || IsDpadButton(button);
}

// C and D-pad buttons are shared by every form, so the Human row is the one they live in.
uint8_t GetButtonItem(uint8_t button) {
    return IsDpadButton(button) ? DPAD_GET_CUR_FORM_BTN_ITEM(HELD_ITEM_TO_DPAD(button))
                                : BUTTON_ITEM_EQUIP(0, button);
}

uint8_t GetButtonSlot(uint8_t button) {
    return IsDpadButton(button) ? DPAD_GET_CUR_FORM_BTN_SLOT(HELD_ITEM_TO_DPAD(button)) : C_SLOT_EQUIP(0, button);
}

void SetButton(uint8_t button, uint8_t item, uint8_t slot) {
    if (IsDpadButton(button)) {
        DPAD_SET_CUR_FORM_BTN_ITEM(HELD_ITEM_TO_DPAD(button), item);
        DPAD_SET_CUR_FORM_BTN_SLOT(HELD_ITEM_TO_DPAD(button), slot);
    } else {
        BUTTON_ITEM_EQUIP(0, button) = item;
        C_SLOT_EQUIP(0, button) = slot;
    }
}

void ReloadButtonIcon(uint8_t button) {
    if (gPlayState == nullptr) {
        return;
    }
    if (IsDpadButton(button)) {
        Interface_Dpad_LoadItemIconImpl(gPlayState, HELD_ITEM_TO_DPAD(button));
    } else {
        Interface_LoadItemIconImpl(gPlayState, button);
    }
}

bool EnforceEquipment(const S2HFormDefinition& form) {
    if (form.resolveEquipment == nullptr) {
        return false;
    }
    bool changed = false;

    for (int32_t type = 0; type < EquipTypeCount; type++) {
        const uint16_t current = GET_CUR_EQUIP_VALUE(type);
        const uint16_t wanted = form.resolveEquipment(type, current);
        if (wanted == current) {
            continue;
        }
        sStash.hasEquip[type] = true;
        sStash.equip[type] = current;
        if (type == EQUIP_TYPE_SWORD) {
            sStash.buttonB = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);
            ChangeSword(wanted);
        } else {
            SET_EQUIP_VALUE(type, wanted);
        }
        changed = true;
    }
    return changed;
}

bool EnforceButtons(const RegisteredForm& form) {
    if (form.definition.allowsButtonItem == nullptr) {
        return false;
    }
    bool changed = false;

    for (uint8_t button = 0; button < ButtonCount; button++) {
        if (!IsStashableButton(button)) {
            continue;
        }
        const uint8_t item = GetButtonItem(button);
        const char* key = CustomItemRegistry_GetEquippedKey(button);
        const bool isOwnMask = key != nullptr && form.item == key;
        if (item == ITEM_NONE || isOwnMask || form.definition.allowsButtonItem(item, key)) {
            continue;
        }
        sStash.hasButton[button] = true;
        sStash.buttonSlot[button] = GetButtonSlot(button);
        sStash.buttonKey[button] = key != nullptr ? key : "";
        if (key != nullptr) {
            CustomItemRegistry_Unequip(button);
        }
        SetButton(button, ITEM_NONE, SLOT_NONE);
        ReloadButtonIcon(button);
        changed = true;
    }
    return changed;
}

void RefreshEquipment(Player* player) {
    if (player != nullptr && gPlayState != nullptr) {
        Player_SetEquipmentData(gPlayState, player);
    }
}

void EnforceLoadout(Player* player) {
    if (sAppliedForm == nullptr) {
        return;
    }
    const bool equipmentChanged = EnforceEquipment(sAppliedForm->definition);
    const bool buttonsChanged = EnforceButtons(*sAppliedForm);
    if (!equipmentChanged && !buttonsChanged) {
        return;
    }
    SPDLOG_INFO("[FormRegistry] '{}' took off what it does not allow", sAppliedForm->key);
    RefreshEquipment(player);
}

void RestoreButton(uint8_t button) {
    if (!sStash.buttonKey[button].empty()) {
        CustomItemRegistry_Equip(button, sStash.buttonKey[button].c_str());
        return;
    }
    const uint8_t slot = sStash.buttonSlot[button];
    const uint8_t item = slot == SLOT_NONE ? ITEM_NONE : gSaveContext.save.saveInfo.inventory.items[slot];
    if (item == ITEM_NONE) {
        return;
    }
    SetButton(button, item, slot);
    ReloadButtonIcon(button);
}

void RestoreLoadout(Player* player) {
    bool restored = false;

    for (int32_t type = 0; type < EquipTypeCount; type++) {
        if (!sStash.hasEquip[type]) {
            continue;
        }
        SET_EQUIP_VALUE(type, sStash.equip[type]);
        if (type == EQUIP_TYPE_SWORD) {
            BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = sStash.buttonB;
        }
        restored = true;
    }
    for (uint8_t button = 0; button < ButtonCount; button++) {
        if (sStash.hasButton[button]) {
            RestoreButton(button);
            restored = true;
        }
    }
    sStash = {};
    if (!restored) {
        return;
    }

    SPDLOG_INFO("[FormRegistry] Gave Link back what the form took off");
    RefreshEquipment(player);
    if (gPlayState != nullptr) {
        Interface_LoadItemIconImpl(gPlayState, EQUIP_SLOT_B);
    }
}

void PatchBody(const RegisteredForm& form) {
    if (form.definition.body == nullptr) {
        return;
    }
    sHumanBody = sPlayerAgeProperties[PLAYER_FORM_HUMAN];
    sPlayerAgeProperties[PLAYER_FORM_HUMAN] = form.body;
    sBodyPatched = true;
}

void UnpatchBody() {
    if (sBodyPatched) {
        sPlayerAgeProperties[PLAYER_FORM_HUMAN] = sHumanBody;
        sBodyPatched = false;
    }
}

// MM reads its animation tables directly at dozens of sites, so the form rewrites the rows for as long as it is worn.
void PatchAnims(const RegisteredForm& form) {
    for (const S2HFormAnimOverride& override : form.anims) {
        for (s32 animType = 0; animType < PLAYER_ANIMTYPE_MAX; animType++) {
            if (override.animType != S2H_FORM_ANIM_TYPE_ALL && override.animType != animType) {
                continue;
            }
            sSavedAnims.push_back({ override.group, animType, D_8085BE84[override.group][animType] });
            D_8085BE84[override.group][animType] = override.anim;
        }
    }
}

void UnpatchAnims() {
    for (auto saved = sSavedAnims.rbegin(); saved != sSavedAnims.rend(); ++saved) {
        D_8085BE84[saved->group][saved->animType] = saved->anim;
    }
    sSavedAnims.clear();
}

void ApplyEffects(RegisteredForm* form, Player* player) {
    sAppliedForm = form;
    FormModel_SetActive(&form->definition);
    PatchBody(*form);
    PatchAnims(*form);
    if (form->definition.blockedButtons != 0) {
        PlayerInput_Block(form->key.c_str(), form->definition.blockedButtons, false);
    }
    EnforceLoadout(player);
    if (form->definition.onEnter != nullptr) {
        form->definition.onEnter(gPlayState, player);
    }
}

void RevokeEffects(Player* player) {
    RegisteredForm* leaving = sAppliedForm;
    if (leaving == nullptr) {
        return;
    }
    sAppliedForm = nullptr;
    FormModel_SetActive(nullptr);
    PlayerInput_Release(leaving->key.c_str());
    if (player != nullptr && leaving->definition.onExit != nullptr) {
        leaving->definition.onExit(gPlayState, player);
    }
    UnpatchAnims();
    UnpatchBody();
    RestoreLoadout(player);
}

void SyncEffects(Player* player) {
    RegisteredForm* wanted = IsHuman(player) ? sActiveForm : nullptr;
    if (wanted == sAppliedForm) {
        return;
    }
    RevokeEffects(player);
    if (wanted != nullptr) {
        ApplyEffects(wanted, player);
    }
}

void SelectForm(RegisteredForm* form) {
    Player* player = GetPlayer();
    if (form != nullptr && player == nullptr) {
        sPendingForm = form->key;
        return;
    }
    sPendingForm.clear();
    sActiveForm = form;
    SyncEffects(player);
}

void ExitActiveForm() {
    SelectForm(nullptr);
}

// The world a form was applied to is gone when a save loads, so nothing may call back into it.
void DropActiveForm() {
    FormModel_SetActive(nullptr);
    if (sAppliedForm != nullptr) {
        PlayerInput_Release(sAppliedForm->key.c_str());
    }
    UnpatchAnims();
    UnpatchBody();
    sAppliedForm = nullptr;
    sActiveForm = nullptr;
    sStash = {};
    sPendingForm.clear();
}

bool IsValidDefinition(const S2HFormDefinition* definition) {
    if (definition == nullptr || definition->structSize < S2H_FORM_DEFINITION_MIN_SIZE || definition->key == nullptr ||
        definition->key[0] == '\0' || definition->label == nullptr || definition->kind > S2H_FORM_KIND_TAKEOVER) {
        SPDLOG_ERROR("[FormRegistry] Invalid form definition");
        return false;
    }
    if (sFormsByKey.contains(definition->key)) {
        SPDLOG_ERROR("[FormRegistry] Form '{}' is already registered", definition->key);
        return false;
    }
    if ((definition->actionCount != 0 && definition->actions == nullptr) ||
        (definition->animCount != 0 && definition->anims == nullptr)) {
        SPDLOG_ERROR("[FormRegistry] Form '{}' declares overrides without a table", definition->key);
        return false;
    }
    for (uint32_t i = 0; i < definition->actionCount; i++) {
        const S2HFormActionOverride& action = definition->actions[i];
        if (action.action < 0 || action.action >= SOH_PLAYER_ACTION_MAX || action.run == nullptr) {
            SPDLOG_ERROR("[FormRegistry] Form '{}' has an invalid action override {}", definition->key, i);
            return false;
        }
    }
    for (uint32_t i = 0; i < definition->animCount; i++) {
        const S2HFormAnimOverride& anim = definition->anims[i];
        if (anim.group < 0 || anim.group >= PLAYER_ANIMGROUP_MAX || anim.animType < S2H_FORM_ANIM_TYPE_ALL ||
            anim.animType >= PLAYER_ANIMTYPE_MAX || anim.anim == nullptr) {
            SPDLOG_ERROR("[FormRegistry] Form '{}' has an invalid animation override {}", definition->key, i);
            return false;
        }
    }
    return true;
}

void ResolvePendingForm() {
    if (sPendingForm.empty()) {
        return;
    }
    RegisteredForm* pending = FindForm(sPendingForm.c_str());
    if (pending == nullptr || !IsFormItemOwned(*pending)) {
        SPDLOG_WARN("[FormRegistry] Saved form '{}' is not available; staying as Link", sPendingForm);
        sPendingForm.clear();
        return;
    }
    SelectForm(pending);
}

// PLAYER_STATE3_4 gates Player_UpdateCommon's action function for the frame, which is how a takeover form
// replaces it; its update then runs from OnPlayerUpdate, after the vanilla frame.
void PrepareFormFrame(Input*) {
    Player* player = GetPlayer();
    if (player == nullptr) {
        return;
    }
    ResolvePendingForm();
    SyncEffects(player);
    if (IsAppliedKind(S2H_FORM_KIND_TAKEOVER)) {
        player->stateFlags3 |= PLAYER_STATE3_4;
    }
}

void UpdateForms(Player* player) {
    if (player != GetPlayer()) {
        return;
    }
    if (sAppliedForm != nullptr && !IsFormItemOwned(*sAppliedForm)) {
        ExitActiveForm();
        return;
    }
    if (sAppliedForm == nullptr || FormTransform_IsActive()) {
        return;
    }
    EnforceLoadout(player);
    if (sAppliedForm->definition.update != nullptr) {
        sAppliedForm->definition.update(gPlayState, player);
    }
}

void DrawTakeoverForm(Actor* actor, bool* should) {
    if (IsAppliedKind(S2H_FORM_KIND_TAKEOVER) && sAppliedForm->definition.draw != nullptr) {
        *should = false;
        sAppliedForm->definition.draw(gPlayState, reinterpret_cast<Player*>(actor));
    }
}

void DrawLinkFormOverlay(Actor* actor) {
    if (IsAppliedKind(S2H_FORM_KIND_LINK) && sAppliedForm->definition.draw != nullptr) {
        sAppliedForm->definition.draw(gPlayState, reinterpret_cast<Player*>(actor));
    }
}

nlohmann::json WriteSection() {
    return { { "active", sActiveForm != nullptr ? sActiveForm->key : sPendingForm },
             { "stash",
               { { "buttonB", sStash.buttonB },
                 { "hasEquip", sStash.hasEquip },
                 { "equip", sStash.equip },
                 { "hasButton", sStash.hasButton },
                 { "buttonSlot", sStash.buttonSlot },
                 { "buttonKey", sStash.buttonKey } } } };
}

void ReadStash(const nlohmann::json& stash) {
    sStash.buttonB = stash.value("buttonB", static_cast<uint8_t>(ITEM_NONE));
    sStash.hasEquip = stash.value("hasEquip", sStash.hasEquip);
    sStash.equip = stash.value("equip", sStash.equip);
    sStash.hasButton = stash.value("hasButton", sStash.hasButton);
    sStash.buttonSlot = stash.value("buttonSlot", sStash.buttonSlot);
    sStash.buttonKey = stash.value("buttonKey", sStash.buttonKey);
}

void ReadSection(const nlohmann::json& section) {
    DropActiveForm();
    if (!section.is_object()) {
        return;
    }
    try {
        sPendingForm = section.value("active", "");
        ReadStash(section.value("stash", nlohmann::json::object()));
    } catch (const nlohmann::json::exception& error) {
        SPDLOG_WARN("[FormRegistry] Ignoring a malformed forms section: {}", error.what());
        DropActiveForm();
    }
}

int32_t FormCommand(std::shared_ptr<Ship::Console>, const std::vector<std::string>& args, std::string* output) {
    if (args.size() != 2) {
        if (output != nullptr) {
            *output = "Usage: form \"<key>\" | form link";
        }
        return 1;
    }
    const std::string key = ModApi_UnquoteConsoleArgument(args[1]);
    const bool toLink = key == "link";
    const bool changed = FormRegistry_SetActive(toLink ? nullptr : key.c_str());
    if (output != nullptr) {
        *output = changed ? "Form: " + key : "Unknown form " + key;
    }
    return changed ? 0 : 1;
}

} // namespace

void FormRegistry_Init() {
    // Registered ahead of FormModel's skeleton swap, which must not run for a draw this hook vetoes.
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorDraw>(ACTOR_PLAYER, DrawTakeoverForm);
    FormTransform_Init();
    FormModel_Init();
    SaveSections_Register(SaveSection, WriteSection, ReadSection);

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPassPlayerInputs>(PrepareFormFrame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(UpdateForms);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorDraw>(ACTOR_PLAYER, DrawLinkFormOverlay);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerActionHandler>(FormRegistry_RunActionOverride);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveMotionScale>(
        FormRegistry_ResolveMotionScale);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveHeight>(
        [](Player* player, f32* height) { *height = FormRegistry_ResolveHeight(player, *height); });

    Ship::Context::GetRawInstance()->GetConsole()->AddCommand(
        "form", { FormCommand, "Switches the player to a registered form", { { "key", Ship::ArgumentType::TEXT } } });
}

void FormRegistry_RunActionOverride(PlayState* play, Player* player, int32_t action, bool* consumed,
                                    bool* startedAction) {
    if (*consumed || sAppliedForm == nullptr) {
        return;
    }
    for (const S2HFormActionOverride& override : sAppliedForm->actions) {
        if (override.action != action) {
            continue;
        }
        const int32_t result = override.run(play, player);
        if (result == S2H_FORM_ACTION_VANILLA) {
            return;
        }
        *consumed = true;
        *startedAction = result == S2H_FORM_ACTION_STARTED;
        return;
    }
}

void FormRegistry_ResolveMotionScale(Player*, int32_t kind, float* scale) {
    if (kind == SOH_PLAYER_MOTION_STICK_SPEED && sAppliedForm != nullptr && sAppliedForm->definition.motionScale > 0.0f) {
        *scale *= sAppliedForm->definition.motionScale;
    }
}

float FormRegistry_ResolveHeight(Player* player, float height) {
    const bool hasOwnHeight = sAppliedForm != nullptr && player == GetPlayer() && sAppliedForm->definition.height > 0.0f;
    return hasOwnHeight ? sAppliedForm->definition.height : height;
}

bool FormRegistry_Register(const S2HFormDefinition* definition) {
    if (!IsValidDefinition(definition)) {
        return false;
    }
    RegisteredForm& form = sForms.emplace_back();
    std::memcpy(&form.definition, definition, std::min<size_t>(definition->structSize, sizeof(form.definition)));
    form.definition.structSize = sizeof(form.definition);
    form.key = definition->key;
    form.label = definition->label;
    form.item = form.definition.item == nullptr ? "" : form.definition.item;
    form.modelPath = form.definition.modelPath == nullptr ? "" : form.definition.modelPath;
    form.definition.key = form.key.c_str();
    form.definition.label = form.label.c_str();
    form.definition.item = form.item.empty() ? nullptr : form.item.c_str();
    form.definition.modelPath = form.modelPath.empty() ? nullptr : form.modelPath.c_str();
    form.actions.assign(definition->actions, definition->actions + definition->actionCount);
    form.anims.assign(definition->anims, definition->anims + definition->animCount);
    form.definition.actions = form.actions.data();
    form.definition.anims = form.anims.data();
    if (definition->body != nullptr) {
        form.body = *definition->body;
        form.definition.body = &form.body;
    }
    sFormsByKey[form.key] = &form;
    SPDLOG_INFO("[FormRegistry] Registered form '{}'", form.key);
    return true;
}

const S2HFormDefinition* FormRegistry_Find(const char* key) {
    RegisteredForm* form = FindForm(key);
    return form == nullptr ? nullptr : &form->definition;
}

uint32_t FormRegistry_GetCount(void) {
    return static_cast<uint32_t>(sForms.size());
}

const S2HFormDefinition* FormRegistry_GetAt(uint32_t index) {
    return index < sForms.size() ? &sForms[index].definition : nullptr;
}

bool FormRegistry_SetActive(const char* key) {
    if (FormTransform_IsActive()) {
        SPDLOG_WARN("[FormRegistry] Ignoring a form change while the transformation plays");
        return false;
    }
    if (key == nullptr || key[0] == '\0') {
        if (sActiveForm != nullptr && FormTransform_Begin(ActiveDefinition(), nullptr)) {
            return true;
        }
        ExitActiveForm();
        return true;
    }
    RegisteredForm* form = FindForm(key);
    if (form == nullptr) {
        return false;
    }
    if (!IsFormItemOwned(*form)) {
        SPDLOG_WARN("[FormRegistry] Form '{}' needs its item '{}'", form->key, form->item);
        return false;
    }
    if (form == sActiveForm) {
        return true;
    }
    if (FormTransform_Begin(ActiveDefinition(), &form->definition)) {
        return true;
    }
    SelectForm(form);
    return true;
}

bool FormRegistry_ApplyActive(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        ExitActiveForm();
        return true;
    }
    RegisteredForm* form = FindForm(key);
    if (form == nullptr) {
        return false;
    }
    if (form != sActiveForm) {
        SelectForm(form);
    }
    return true;
}

bool FormRegistry_Toggle(const char* key) {
    return FormRegistry_IsActive(key) ? FormRegistry_SetActive(nullptr) : FormRegistry_SetActive(key);
}

const char* FormRegistry_GetActiveKey(void) {
    if (Player* player = CustomItemRegistry_GetDrawingPlayer()) {
        const char* key = nullptr;
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveForm>(player, &key);
        return key;
    }
    return sActiveForm == nullptr ? nullptr : sActiveForm->key.c_str();
}

bool FormRegistry_IsActive(const char* key) {
    const char* active = FormRegistry_GetActiveKey();
    return key != nullptr && active != nullptr && std::strcmp(active, key) == 0;
}
