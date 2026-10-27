#ifndef S2H_MOD_API_H
#define S2H_MOD_API_H

// Append table entries without changing this; bump for incompatible ABI changes.
#define S2H_MOD_API_MAJOR 3

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "z64.h"
#ifdef __cplusplus
}
#endif

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/ActorRegistry/ActorRegistry.h"
#include "2s2h/ModApi/AudioMix/AudioMix.h"
#include "2s2h/ModApi/Network/ModNetwork.h"
#include "2s2h/ModApi/Labels/ModLabels.h"
#include "2s2h/ModApi/ProgressiveItems/ProgressiveItems.h"
#include "2s2h/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/CustomItemRegistry/KaleidoItemManager.h"
#include "2s2h/ModApi/Forms/FormRegistry.h"
#include "2s2h/ModApi/Layout/ModLayout.h"
#include "2s2h/ModApi/Microphone/Microphone.h"
#include "2s2h/ModApi/ModDialogs/ModDialogs.h"
#include "2s2h/ModApi/ModMenu/ModMenu.h"
#include "2s2h/ModApi/ModMessages/ModMessages.h"
#include "2s2h/ModApi/ModStorage/ModStorage.h"
#include "2s2h/ModApi/O2rExtractor/O2rExtractor.h"
#include "2s2h/ModApi/Player/PlayerHookTypes.h"
#include "2s2h/ModApi/PlayerInput/PlayerInput.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/ModApi/RandoLogic/RandoLogic.h"
#include "2s2h/ModApi/RandoOptions/RandoOptions.h"
#include "2s2h/ModApi/TimeControl/TimeControl.h"
#include "2s2h/ModApi/Timers/Timers.h"
#include "2s2h/ModApi/UserFiles/UserFiles.h"
#include "2s2h/ModApi/VanillaItems/VanillaItems.h"

#ifdef _WIN32
#define S2H_MOD_EXPORT __declspec(dllexport)
#else
#define S2H_MOD_EXPORT __attribute__((visibility("default")))
#endif

#define DEFINE_HOOK(name, args) \
    typedef void(*S2HCb_##name) args; \
    static inline S2HCb_##name S2H_CheckHook_##name(S2HCb_##name callback) { return callback; }
#include "2s2h/GameInteractor/GameInteractor_HookTable.h"
#undef DEFINE_HOOK

typedef void (*S2HVbCallback)(bool* should, va_list args);

#include "2s2h/ModApi/ModDungeons/BossRegistry.h"

typedef struct {
    uint32_t structSize;
    const char* const* requiredHooks;
    uint32_t requiredHookCount;
    const char* const* requiredBehaviors;
    uint32_t requiredBehaviorCount;
} S2HModRequirements;

// Same rows, in the same order, as Ship's SOHModApi; rows Majora's Mask adds go at the end. Append only.
typedef struct {
    uint32_t tableSize;
    bool (*RegisterHookByName)(const char* name, void* callback);
    bool (*RegisterVB)(GIVanillaBehavior flag, S2HVbCallback callback);
    bool (*HasHook)(const char* name);
    bool (*RegisterCustomItem)(const S2HCustomItemDefinition* definition);
    const S2HCustomItemDefinition* (*FindCustomItem)(const char* key);
    uint16_t (*GetCustomItemRuntimeId)(const char* key);
    const S2HCustomItemDefinition* (*FindCustomItemByRuntimeId)(uint16_t runtimeId);
    uint32_t (*GetCustomItemCount)(void);
    const S2HCustomItemDefinition* (*GetCustomItemAt)(uint32_t index);
    bool (*SetCustomItemOwned)(const char* key, bool owned);
    bool (*IsCustomItemOwned)(const char* key);
    bool (*EquipCustomItem)(uint8_t button, const char* key);
    void (*UnequipCustomItem)(uint8_t button);
    const char* (*GetEquippedCustomItem)(uint8_t button);
    bool (*SelectPauseCustomItem)(const char* key);
    bool (*BindActorCustomItem)(Actor* actor, const char* key);
    void (*UnbindActorCustomItem)(Actor* actor);
    const char* (*GetActorCustomItem)(Actor* actor);
    bool (*GiveCustomItem)(PlayState* play, const char* key);
    bool (*RegisterMenuWidget)(const S2HModMenuWidget* widget);
    bool (*StorageHas)(const char* mod, const char* key);
    uint32_t (*StorageGetSize)(const char* mod, const char* key);
    uint32_t (*StorageGet)(const char* mod, const char* key, void* output, uint32_t capacity);
    bool (*StorageSet)(const char* mod, const char* key, const void* data, uint32_t size);
    bool (*StorageRemove)(const char* mod, const char* key);
    uint32_t (*StorageGetString)(const char* mod, const char* key, char* output, uint32_t capacity);
    bool (*StorageSetString)(const char* mod, const char* key, const char* value);
    bool (*RegisterMenuSidebar)(const char* section, const char* sidebar, const char* selectionCvar, uint8_t columns);
    bool (*RegisterMenuWidgetAt)(const S2HModMenuWidget* widget, const char* anchor, bool after);
    bool (*RegisterItemPool)(const char* key, const S2HItemPoolEntry* entries, uint32_t count);
    uint32_t (*GetItemPoolCount)(void);
    const char* (*GetItemPoolKey)(uint32_t index);
    uint32_t (*GetItemPoolSize)(const char* key);
    bool (*GetItemPoolEntry)(const char* key, uint32_t index, S2HItemPoolEntry* entry);
    bool (*GetItemPlacement)(const char* key, S2HCustomItemPlacement* placement);
    bool (*SetItemPlacement)(const char* key, const S2HCustomItemPlacement* placement);
    const char* (*ResolveCustomItemTexture)(const char* key, S2HItemIconSurface surface);
    bool (*IsCustomItemEditorVisible)(const char* key);
    const S2HCustomItemDefinition* (*FindCustomItemReplacement)(uint16_t vanillaItem);
    bool (*IsCustomItemFormAllowed)(const char* key);
    uint32_t (*GetSlotItemCount)(uint16_t page, uint8_t slot);
    const char* (*GetSlotItemAt)(uint16_t page, uint8_t slot, uint32_t index);
    bool (*RegisterHookForIdByName)(const char* name, int32_t id, void* callback);
    bool (*RegisterHookForPtrByName)(const char* name, uintptr_t ptr, void* callback);
    bool (*RegisterForm)(const S2HFormDefinition* definition);
    const S2HFormDefinition* (*FindForm)(const char* key);
    bool (*SetActiveForm)(const char* key);
    const char* (*GetActiveForm)(void);
    bool (*IsFormActive)(const char* key);
    bool (*BlockPlayerInput)(const char* owner, uint32_t buttons, bool blockStick);
    void (*ReleasePlayerInput)(const char* owner);
    bool (*ShowTextbox)(PlayState* play, const char* text, bool autoFormat);
    bool (*RequestTimeControl)(const char* owner, int32_t priority, float worldSpeed, bool freezeClock);
    void (*ReleaseTimeControl)(const char* owner);
    float (*GetWorldSpeed)(void);
    bool (*ToggleForm)(const char* key);
    bool (*BlockVanillaItem)(uint16_t vanillaItem, int32_t randoItem);
    const char* (*GetVanillaItemCustomKey)(uint16_t vanillaItem);
    bool (*GrantLogicCapability)(const char* owner, const char* capability, S2HLogicCondition condition,
                                 void* userData);
    bool (*RegisterLogicRule)(const S2HLogicRule* rule);
    void (*RemoveLogicOwner)(const char* name);
    bool (*LogicCanUseItem)(int32_t randoGet);
    bool (*LogicHasItem)(int32_t randoGet);
    bool (*LogicIsChild)(void);
    bool (*LogicIsAdult)(void);
    bool (*LogicIsAtDay)(void);
    bool (*LogicIsAtNight)(void);
    bool (*LogicHasCapability)(const char* capability);
    int32_t (*GetRandoItem)(const char* key);
    uint16_t (*RegisterRandoFlag)(const char* key);
    bool (*GetRandoFlag)(const char* key);
    void (*SetRandoFlag)(const char* key, bool state);
    bool (*RequestHazard)(const char* owner, int32_t priority, int16_t hazard, uint8_t requiredTunic);
    void (*ReleaseHazard)(const char* owner);
    int16_t (*GetHazard)(void);
    bool (*StartCountdown)(const char* owner, uint16_t seconds);
    void (*StopCountdown)(const char* owner);
    uint16_t (*GetCountdown)(const char* owner);
    bool (*IsCountdownRunning)(const char* owner);
    void (*KeepCustomItemHeld)(void);
    int16_t (*RegisterActor)(const S2HActorDefinition* definition);
    int16_t (*GetActorId)(const char* key);
    uint8_t (*HasResource)(const char* path);
    bool (*SetCustomItemText)(const char* key, S2HItemTextKind kind, const char* text);
    bool (*RegisterAudioMix)(S2HAudioMixFunc callback);
    bool (*RegisterRandoOption)(const S2HModRandoOption* option);
    uint8_t (*GetRandoOption)(const char* key);
    uint32_t (*ExportArchiveFiles)(const char* searchMask, const char* destinationDir);
    bool (*ExtractRom)(const S2HO2rExtractRequest* request);
    void (*Log)(const char* message);
    bool (*RegisterCustomEquipment)(const S2HCustomEquipDefinition* definition);
    const char* (*GetWornEquipment)(uint8_t slot);
    bool (*IsEquipmentWorn)(const char* key);
    bool (*IsEquipmentOwned)(const char* key);
    bool (*SetEquipmentOwned)(const char* key, bool owned);
    bool (*WearEquipment)(const char* key);
    void (*TakeOffEquipment)(uint8_t slot);
    bool (*IsEquipmentToggleOn)(const char* key);
    bool (*SetEquipmentToggle)(const char* key, bool on);
    bool (*RegisterAudioMixInGroup)(S2HAudioMixFunc callback, uint8_t group, const char* volumeCvar);
    uint16_t (*RegisterSequence)(const char* path);
    int32_t (*RegisterSoundFont)(const char* path);
    uint16_t (*GetSequenceId)(const char* path);
    bool (*RegisterLayoutPage)(const S2HLayoutPage* page);
    bool (*PlaceLayoutItem)(const char* itemKey, const char* pageKey, uint8_t cell);
    bool (*RegisterLayoutVanilla)(const S2HLayoutVanilla* entry);
    bool (*PublishService)(const char* name, void* function);
    void* (*FindService)(const char* name);
    uint16_t (*RegisterMessage)(const S2HModMessage* message);
    uint16_t (*GetMessageId)(const char* key);
    S2HMicrophoneStatus (*OpenMicrophone)(uint32_t sampleRate);
    uint32_t (*ReadMicrophone)(float* samples, uint32_t capacity);
    void (*CloseMicrophone)(void);
    bool (*PickUserFile)(const char* title, const char* filterName, const char* filterPatterns, S2HUserFile* file);
    void (*FreeUserFile)(S2HUserFile* file);
    bool (*SaveToModsFolder)(const char* fileName, const void* data, uint64_t size);
    bool (*ExtractRomToModsFolder)(const S2HRomExtractRequest* request);
    bool (*AskPlayer)(const char* title, const char* message);
    void (*TellPlayer)(const char* title, const char* message);
    bool (*RequestRestart)(const char* reason);
    bool (*SetCustomItemIconPath)(const char* key, const char* path);
    bool (*RegisterBoss)(const S2HBossDefinition* definition);
    int32_t (*GetBossSlot)(const char* key);
    const char* (*GetSceneBoss)(int16_t sceneId);
    int16_t (*GetBossSlotScene)(int32_t slot);
    void (*ReportBossDefeated)(Actor* boss);
    bool (*HasModArchiveFile)(const char* fileName);
    bool (*RegisterPickupVariant)(const S2HCustomPickupVariant* variant);
    const S2HCustomPickupVariant* (*GetPickupVariant)(const char* key);
    // Frame-global renderer tuning; selection and actor scoping belong to the mod.
    void (*SetCelShadingRamp)(float center, float softness, float highlight, float shadow, bool debugBands);
    uint32_t (*ReadArchiveFiles)(const char* searchMask, S2HArchiveFileFunc visit, void* userData);
    uint32_t (*ListArchiveFiles)(const char* searchMask, S2HArchivePathFunc visit, void* userData);
    bool (*MountUserArchive)(const char* title);
    bool (*HasVanillaBehavior)(const char* name);
    bool (*RegisterVBByName)(const char* name, S2HVbCallback callback);
    const char* (*GetHostGame)(void);
    uint32_t (*GetModApiMajor)(void);
    bool (*RegisterNetChannel)(const char* channel, uint16_t version, S2HNetMessageFunc onMessage);
    bool (*SendNet)(const char* channel, uint32_t to, const void* data, uint32_t size);
    bool (*SetNetPeerState)(const char* channel, const void* data, uint32_t size);
    void (*ClearNetPeerState)(const char* channel);
    uint32_t (*GetNetPeerState)(uint32_t clientId, const char* channel, void* output, uint32_t capacity);
    bool (*SetNetRoomState)(const char* channel, const void* data, uint32_t size);
    void (*ClearNetRoomState)(const char* channel);
    uint32_t (*GetNetRoomState)(const char* channel, void* output, uint32_t capacity);
    bool (*IsNetConnected)(void);
    bool (*IsNetHost)(void);
    uint32_t (*GetNetOwnClientId)(void);
    uint32_t (*GetNetPeerCount)(void);
    bool (*GetNetPeerAt)(uint32_t index, S2HNetPeer* peer);
    bool (*FindNetPeer)(uint32_t clientId, S2HNetPeer* peer);
    uint32_t (*GetNetPeerIdForActor)(const Actor* actor);
    bool (*RequestNetTeleport)(uint32_t clientId);
    bool (*SummonNetPeer)(uint32_t clientId);
    bool (*StartNetRoomCountdown)(const char* owner, uint16_t seconds);
    void (*StopNetRoomCountdown)(const char* owner);
    bool (*SetCustomItemWorn)(const char* key, bool worn);
    bool (*IsCustomItemWorn)(const char* key);
    bool (*IsCustomItemAgeAllowed)(const char* key);
    bool (*RegisterProgressiveChain)(const S2HProgressiveChain* chain);
    uint16_t (*GetProgressiveLevel)(const char* key, S2HRandoInfBank bank);
    const char* (*GetNextProgressiveItem)(const char* key, S2HRandoInfBank bank);
    bool (*SetLabel)(S2HLabelKind kind, int32_t language, int32_t index, const char* name);
    bool (*SetTimeSplits)(const S2HTimeSplit* splits, uint32_t count);
    bool (*SetTimeSplitGroup)(uint32_t id, const uint32_t* members, uint32_t count);
    void (*ClearTimeSplitGroups)(void);
    bool (*SetItemIcon)(uint32_t item, uint32_t source);
    bool (*GrantLogic)(const S2HLogicGrant* grant);
} S2HModApi;

#define S2H_REGISTER_HOOK(api, name, callback) ((api)->RegisterHookByName(#name, (void*)S2H_CheckHook_##name(callback)))
#define S2H_REGISTER_HOOK_FOR_ID(api, name, id, callback) \
    ((api)->RegisterHookForIdByName(#name, (id), (void*)S2H_CheckHook_##name(callback)))
#define S2H_MOD_API_MEMBER_SIZE(member) (offsetof(S2HModApi, member) + sizeof(((S2HModApi*)0)->member))
#define S2H_MOD_API_MATCHES(api) ((api) != NULL && (api)->tableSize >= S2H_MOD_API_MEMBER_SIZE(HasHook))
#define S2H_MOD_API_HAS(api, member) ((api) != NULL && (api)->tableSize >= S2H_MOD_API_MEMBER_SIZE(member))

typedef void (*S2HModSetApiFunc)(const S2HModApi* api);
typedef const S2HModRequirements* (*S2HModGetRequirementsFunc)(void);
typedef void (*S2HModInitFunc)(void);

#ifdef __cplusplus
extern "C" {
#endif

const S2HModApi* ModApi_Get(void);
bool ModApi_HasHook(const char* name);

#ifdef __cplusplus
}

#include <filesystem>
#include <string>

void ModApi_Init();
void ModLoader_LoadMods();
std::string ModLoader_DescribeTrust(const std::string& archivePath);
std::filesystem::path ModLoader_GetModsDirectory();
void ModApi_SetCurrentMod(const std::string& name);
void ModApi_ClearCurrentMod();
std::string ModApi_GetCurrentMod();
bool ModApi_WriteRegistry(const std::filesystem::path& path);
std::string ModApi_GetCatalogFingerprint();
#endif

#endif
