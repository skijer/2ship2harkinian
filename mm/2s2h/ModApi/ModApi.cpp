#include "2s2h/Unbound/UnboundAudio.h"
#include "2s2h/ModApi/ModDungeons/DungeonRegistry.h"
#include "ModApi.h"
#include "ModStore/ModStore.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cmath>
#include <fast/Fast3dWindow.h>
#include <fast/interpreter.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/debug/Console.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "2s2h/ActorTable/CustomActorTable.h"
#include "2s2h/BenPort.h"
#include "2s2h/ShipUtils.h"

extern "C" {
#include "build.h"
}

namespace {

constexpr const char* RegistryFileName = "nocturne-registry.json";

std::string TextOrEmpty(const char* text) {
    return text != nullptr ? text : "";
}

nlohmann::json DescribeItems() {
    nlohmann::json items = nlohmann::json::array();
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); index++) {
        const S2HCustomItemDefinition* item = CustomItemRegistry_GetAt(index);
        if (item == nullptr || item->key == nullptr) {
            continue;
        }
        items.push_back({ { "key", item->key },
                          { "owner", CustomItemRegistry_GetOwner(item->key) },
                          { "icon", TextOrEmpty(item->iconPath) },
                          { "name", TextOrEmpty(item->namePath) },
                          { "displayName", TextOrEmpty(item->displayName) },
                          { "pauseText", TextOrEmpty(item->pauseText) },
                          { "getItemText", TextOrEmpty(item->getItemText) },
                          { "heldModel", TextOrEmpty(item->heldModelPath) },
                          { "getItemModel", TextOrEmpty(item->getItemModelPath) },
                          { "flags", item->flags },
                          { "page", item->preferredPage },
                          { "slot", item->preferredSlot },
                          { "forms", item->formMask },
                          { "replacesItem", item->replacesItem },
                          { "hasAction", item->update != nullptr || item->use != nullptr } });
    }
    return items;
}

// Actor ids depend on the mounted mod set, so tools address types by key only.
nlohmann::json DescribeActors() {
    nlohmann::json actors = nlohmann::json::array();
    for (const S2HRegisteredActor& actor : ActorRegistry_List()) {
        const ActorOverlay* overlay = ActorOverlayTable_Get(actor.actorId);
        const ActorProfile* profile = overlay != nullptr ? overlay->profile : nullptr;
        actors.push_back({ { "key", actor.key },
                           { "owner", actor.owner },
                           { "description", GetActorDescription(actor.actorId) },
                           { "category", profile != nullptr ? profile->type : 0 },
                           { "objectId", profile != nullptr ? profile->objectId : 0 } });
    }
    return actors;
}

bool DumpRegistryCommand(std::shared_ptr<Ship::Console>, const std::vector<std::string>& args, std::string* output) {
    const std::filesystem::path path =
        args.size() > 1 ? std::filesystem::path(args[1])
                        : std::filesystem::path(Ship::Context::GetPathRelativeToAppDirectory(RegistryFileName,
                                                                                             appShortName));
    const bool written = ModApi_WriteRegistry(path);
    if (output != nullptr) {
        *output = written ? "Wrote " + path.string() : "Cannot write " + path.string();
    }
    return !written;
}

void RegisterRegistryDumpCommand() {
    Ship::Context::GetRawInstance()->GetConsole()->AddCommand(
        "registry_dump", { DumpRegistryCommand,
                           "Writes the items and actor types mods registered as JSON (default: nocturne-registry.json)",
                           { { "path", Ship::ArgumentType::TEXT, true } } });
}

// The GameInteractor's registries are inline statics of templates: a mod instantiating them would register into
// private copies. Every registration therefore crosses into the executable through these binders.
struct HookBinders {
    void (*global)(void* callback);
    void (*forId)(int32_t id, void* callback);
    void (*forPtr)(uintptr_t ptr, void* callback);
};

std::string sCurrentMod;
std::unordered_set<std::string> sReportedMissing;
std::unordered_map<std::string, void*> sServices;

uint8_t HasResource(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return 0;
    }
    std::string name = path;
    if (name.rfind("__OTR__", 0) == 0) {
        name = name.substr(7);
    }
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    return archives != nullptr && archives->HasFile(name);
}

bool PublishService(const char* name, void* function) {
    if (name == nullptr || name[0] == '\0' || function == nullptr) {
        return false;
    }
    const bool published = sServices.emplace(name, function).second;
    if (!published) {
        SPDLOG_ERROR("[ModApi] Mod '{}' tried to publish '{}', which another mod already provides", sCurrentMod, name);
    }
    return published;
}

void* FindService(const char* name) {
    if (name == nullptr) {
        return nullptr;
    }
    auto service = sServices.find(name);
    return service == sServices.end() ? nullptr : service->second;
}

void LogFromMod(const char* message) {
    if (message == nullptr) {
        return;
    }
    SPDLOG_INFO("[{}] {}", sCurrentMod.empty() ? "mod" : sCurrentMod, message);
}

uint64_t HashText(uint64_t hash, const std::string& text) {
    for (unsigned char value : text) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string Hex(uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(16, '0');
    for (size_t i = 0; i < result.size(); ++i) {
        result[result.size() - i - 1] = digits[value & 0xF];
        value >>= 4;
    }
    return result;
}

std::unordered_map<std::string, HookBinders> BuildHookEntries() {
    std::unordered_map<std::string, HookBinders> entries;

#define DEFINE_HOOK(name, args)                                                                                  \
    entries[#name] = {                                                                                           \
        [](void* callback) {                                                                                     \
            GameInteractor::Instance->RegisterGameHook<GameInteractor::name>((S2HCb_##name)callback);            \
        },                                                                                                       \
        [](int32_t id, void* callback) {                                                                         \
            GameInteractor::Instance->RegisterGameHookForID<GameInteractor::name>(id, (S2HCb_##name)callback);   \
        },                                                                                                       \
        [](uintptr_t ptr, void* callback) {                                                                      \
            GameInteractor::Instance->RegisterGameHookForPtr<GameInteractor::name>(ptr, (S2HCb_##name)callback); \
        },                                                                                                       \
    };
#include "2s2h/GameInteractor/GameInteractor_HookTable.h"
#undef DEFINE_HOOK

    return entries;
}

const std::unordered_map<std::string, HookBinders>& HookEntries() {
    static const auto entries = BuildHookEntries();
    return entries;
}

void ReportMissingHook(const char* name) {
    const std::string owner = sCurrentMod.empty() ? "unknown" : sCurrentMod;
    if (sReportedMissing.insert(owner + '\n' + name).second) {
        SPDLOG_ERROR("[ModApi] Mod '{}' requested hook '{}', which was not found", owner, name);
    }
}

const HookBinders* FindHookBinders(const char* name, void* callback) {
    if (name == nullptr || callback == nullptr) {
        return nullptr;
    }
    auto entry = HookEntries().find(name);
    if (entry == HookEntries().end()) {
        ReportMissingHook(name);
        return nullptr;
    }
    return &entry->second;
}

bool RegisterHookByName(const char* name, void* callback) {
    const HookBinders* binders = FindHookBinders(name, callback);
    if (binders == nullptr) {
        return false;
    }
    binders->global(callback);
    return true;
}

bool RegisterHookForIdByName(const char* name, int32_t id, void* callback) {
    const HookBinders* binders = FindHookBinders(name, callback);
    if (binders == nullptr) {
        return false;
    }
    binders->forId(id, callback);
    return true;
}

bool RegisterHookForPtrByName(const char* name, uintptr_t ptr, void* callback) {
    const HookBinders* binders = FindHookBinders(name, callback);
    if (binders == nullptr) {
        return false;
    }
    binders->forPtr(ptr, callback);
    return true;
}

// The va_list is shared by every subscriber of the flag, enhancements included: each mod reads its own copy.
bool RegisterVB(GIVanillaBehavior flag, S2HVbCallback callback) {
    if (callback == nullptr) {
        return false;
    }
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnModVanillaBehavior>(
        flag, [callback](GIVanillaBehavior, bool* should, va_list originalArgs) {
            va_list args;
            va_copy(args, originalArgs);
            callback(should, args);
            va_end(args);
        });
    return true;
}

const std::unordered_map<std::string, GIVanillaBehavior>& VanillaBehaviorEntries() {
    static const std::unordered_map<std::string, GIVanillaBehavior> entries = {
#define UNBOUND_BEHAVIOR(name) { #name, name },
#include "VanillaBehaviorNames.inc"
#undef UNBOUND_BEHAVIOR
    };
    return entries;
}

bool HasVanillaBehavior(const char* name) {
    return name != nullptr && VanillaBehaviorEntries().contains(name);
}

bool RegisterVBByName(const char* name, S2HVbCallback callback) {
    if (name == nullptr || callback == nullptr) {
        return false;
    }
    const auto entry = VanillaBehaviorEntries().find(name);
    return entry != VanillaBehaviorEntries().end() && RegisterVB(entry->second, callback);
}

const char* GetHostGame() {
    return "mm";
}

uint32_t GetModApiMajor() {
    return S2H_MOD_API_MAJOR;
}

bool HasHook(const char* name) {
    return name != nullptr && HookEntries().contains(name);
}

// Filled field by field: two hook table rows end in ';', which a braced initializer would not survive.
void SetCelShadingRamp(float center, float softness, float highlight, float shadow, bool debugBands) {
    if (!std::isfinite(center) || !std::isfinite(softness) || !std::isfinite(highlight) || !std::isfinite(shadow)) return;
    auto* context = Ship::Context::GetRawInstance();
    if (!context) return;
    auto window = std::dynamic_pointer_cast<Fast::Fast3dWindow>(context->GetWindow());
    if (!window) return;
    auto interpreter = window->GetInterpreterWeak().lock();
    if (!interpreter) return;
    auto* renderer = interpreter->GetCurrentRenderingAPI();
    if (renderer) renderer->SetToonRamp(std::clamp(center, 0.0f, 1.0f), std::clamp(softness, 0.001f, 0.5f),
                                      std::clamp(highlight, 0.0f, 2.0f), std::clamp(shadow, 0.0f, 1.0f), debugBands ? 1.0f : 0.0f);
}

S2HModApi BuildTable() {
    S2HModApi api = {};
    api.tableSize = sizeof(S2HModApi);
    api.RegisterHookByName = RegisterHookByName;
    api.RegisterVB = RegisterVB;
    api.HasHook = HasHook;
    api.RegisterCustomItem = CustomItemRegistry_Register;
    api.FindCustomItem = CustomItemRegistry_Find;
    api.GetCustomItemRuntimeId = CustomItemRegistry_GetRuntimeId;
    api.FindCustomItemByRuntimeId = CustomItemRegistry_FindByRuntimeId;
    api.GetCustomItemCount = CustomItemRegistry_GetCount;
    api.GetCustomItemAt = CustomItemRegistry_GetAt;
    api.SetCustomItemOwned = CustomItemRegistry_SetOwned;
    api.IsCustomItemOwned = CustomItemRegistry_IsOwned;
    api.EquipCustomItem = CustomItemRegistry_Equip;
    api.UnequipCustomItem = CustomItemRegistry_Unequip;
    api.GetEquippedCustomItem = CustomItemRegistry_GetEquippedKey;
    api.SelectPauseCustomItem = CustomItemRegistry_SelectPauseItem;
    api.BindActorCustomItem = CustomItemRegistry_BindActor;
    api.UnbindActorCustomItem = CustomItemRegistry_UnbindActor;
    api.GetActorCustomItem = CustomItemRegistry_GetActorItem;
    api.GiveCustomItem = CustomItemRegistry_Give;
    api.RegisterMenuWidget = ModMenu_RegisterWidget;
    api.StorageHas = ModStorage_Has;
    api.StorageGetSize = ModStorage_GetSize;
    api.StorageGet = ModStorage_Get;
    api.StorageSet = ModStorage_Set;
    api.StorageRemove = ModStorage_Remove;
    api.StorageGetString = ModStorage_GetString;
    api.StorageSetString = ModStorage_SetString;
    api.RegisterMenuSidebar = ModMenu_RegisterSidebar;
    api.RegisterMenuWidgetAt = ModMenu_RegisterWidgetAt;
    api.RegisterItemPool = KaleidoItemManager_RegisterPool;
    api.GetItemPoolCount = KaleidoItemManager_GetPoolCount;
    api.GetItemPoolKey = KaleidoItemManager_GetPoolKey;
    api.GetItemPoolSize = KaleidoItemManager_GetPoolSize;
    api.GetItemPoolEntry = KaleidoItemManager_GetPoolEntry;
    api.GetItemPlacement = KaleidoItemManager_GetPlacement;
    api.SetItemPlacement = KaleidoItemManager_SetPlacement;
    api.ResolveCustomItemTexture = CustomItemRegistry_ResolveTexture;
    api.IsCustomItemEditorVisible = CustomItemRegistry_IsEditorVisible;
    api.FindCustomItemReplacement = CustomItemRegistry_FindReplacement;
    api.IsCustomItemFormAllowed = CustomItemRegistry_IsFormAllowed;
    api.GetSlotItemCount = KaleidoItemManager_GetSlotItemCount;
    api.GetSlotItemAt = KaleidoItemManager_GetSlotItemAt;
    api.RegisterHookForIdByName = RegisterHookForIdByName;
    api.RegisterHookForPtrByName = RegisterHookForPtrByName;
    api.RegisterForm = FormRegistry_Register;
    api.FindForm = FormRegistry_Find;
    api.SetActiveForm = FormRegistry_SetActive;
    api.GetActiveForm = FormRegistry_GetActiveKey;
    api.IsFormActive = FormRegistry_IsActive;
    api.BlockPlayerInput = PlayerInput_Block;
    api.ReleasePlayerInput = PlayerInput_Release;
    api.ShowTextbox = CustomItemRegistry_ShowTextbox;
    api.RequestTimeControl = TimeControl_Request;
    api.ReleaseTimeControl = TimeControl_Release;
    api.GetWorldSpeed = TimeControl_GetWorldSpeed;
    api.ToggleForm = FormRegistry_Toggle;
    api.BlockVanillaItem = VanillaItems_Block;
    api.GetVanillaItemCustomKey = VanillaItems_GetCustomKey;
    api.GrantLogicCapability = RandoLogic_GrantCapability;
    api.RegisterLogicRule = RandoLogic_RegisterRule;
    api.RemoveLogicOwner = RandoLogic_RemoveOwner;
    api.LogicCanUseItem = RandoLogic_CanUseItem;
    api.LogicHasItem = RandoLogic_HasLogicItem;
    api.LogicIsChild = RandoLogic_IsChild;
    api.LogicIsAdult = RandoLogic_IsAdult;
    api.LogicIsAtDay = RandoLogic_IsAtDay;
    api.LogicIsAtNight = RandoLogic_IsAtNight;
    api.LogicHasCapability = RandoLogic_HasCapability;
    api.GetRandoItem = RandoItems_GetRandoGet;
    api.RegisterRandoFlag = RandoInfCustom_Register;
    api.GetRandoFlag = RandoInfCustom_GetByKey;
    api.SetRandoFlag = RandoInfCustom_SetByKey;
    api.RequestHazard = Timers_RequestHazard;
    api.ReleaseHazard = Timers_ReleaseHazard;
    api.GetHazard = Timers_GetHazard;
    api.StartCountdown = Timers_StartCountdown;
    api.StopCountdown = Timers_StopCountdown;
    api.GetCountdown = Timers_GetCountdown;
    api.IsCountdownRunning = Timers_IsCountdownRunning;
    api.KeepCustomItemHeld = CustomItemRegistry_HoldHeld;
    api.RegisterActor = ActorRegistry_Register;
    api.GetActorId = ActorRegistry_GetId;
    api.HasResource = HasResource;
    api.SetCustomItemText = CustomItemRegistry_SetText;
    api.RegisterAudioMix = AudioMix_Register;
    api.RegisterRandoOption = RandoOptions_Register;
    api.GetRandoOption = RandoOptions_Get;
    api.ExportArchiveFiles = O2rExtractor_ExportFiles;
    api.ExtractRom = O2rExtractor_Run;
    api.Log = LogFromMod;
    api.RegisterCustomEquipment = CustomEquipRegistry_Register;
    api.GetWornEquipment = CustomEquipRegistry_GetWorn;
    api.IsEquipmentWorn = CustomEquipRegistry_IsWorn;
    api.IsEquipmentOwned = CustomEquipRegistry_IsOwned;
    api.SetEquipmentOwned = CustomEquipRegistry_SetOwned;
    api.WearEquipment = CustomEquipRegistry_Wear;
    api.TakeOffEquipment = CustomEquipRegistry_TakeOff;
    api.IsEquipmentToggleOn = CustomEquipRegistry_IsToggleOn;
    api.SetEquipmentToggle = CustomEquipRegistry_SetToggle;
    api.RegisterAudioMixInGroup = AudioMix_RegisterInGroup;
    api.RegisterLayoutPage = ModLayout_RegisterPage;
    api.PlaceLayoutItem = ModLayout_PlaceItem;
    api.RegisterLayoutVanilla = ModLayout_RegisterVanilla;
    api.PublishService = PublishService;
    api.FindService = FindService;
    api.RegisterMessage = ModMessages_Register;
    api.GetMessageId = ModMessages_GetId;
    api.OpenMicrophone = Microphone_Open;
    api.ReadMicrophone = Microphone_Read;
    api.CloseMicrophone = Microphone_Close;
    api.PickUserFile = UserFiles_Pick;
    api.FreeUserFile = UserFiles_Free;
    api.SaveToModsFolder = UserFiles_SaveToMods;
    api.ExtractRomToModsFolder = UserFiles_ExtractRomToMods;
    api.AskPlayer = ModDialogs_Ask;
    api.TellPlayer = ModDialogs_Tell;
    api.RequestRestart = ModDialogs_RequestRestart;
    api.SetCustomItemIconPath = CustomItemRegistry_SetIconPath;
    api.RegisterBoss = BossRegistry_Register;
    api.GetBossSlot = BossRegistry_GetSlot;
    api.GetSceneBoss = BossRegistry_GetSceneBoss;
    api.GetBossSlotScene = BossRegistry_GetSlotScene;
    api.ReportBossDefeated = BossRegistry_ReportDefeated;
    api.HasModArchiveFile = UserFiles_HasModArchiveFile;
    api.RegisterPickupVariant = CustomItemRegistry_RegisterPickupVariant;
    api.GetPickupVariant = CustomItemRegistry_GetPickupVariant;
    api.SetCelShadingRamp = SetCelShadingRamp;
    api.ReadArchiveFiles = O2rExtractor_ReadFiles;
    api.ListArchiveFiles = O2rExtractor_ListFiles;
    api.MountUserArchive = UserFiles_MountArchive;
    api.HasVanillaBehavior = HasVanillaBehavior;
    api.RegisterVBByName = RegisterVBByName;
    api.GetHostGame = GetHostGame;
    api.GetModApiMajor = GetModApiMajor;
    api.RegisterNetChannel = ModNetwork_RegisterChannel;
    api.SendNet = ModNetwork_Send;
    api.SetNetPeerState = ModNetwork_SetPeerState;
    api.ClearNetPeerState = ModNetwork_ClearPeerState;
    api.GetNetPeerState = ModNetwork_GetPeerState;
    api.SetNetRoomState = ModNetwork_SetRoomState;
    api.ClearNetRoomState = ModNetwork_ClearRoomState;
    api.GetNetRoomState = ModNetwork_GetRoomState;
    api.IsNetConnected = ModNetwork_IsConnected;
    api.IsNetHost = ModNetwork_IsHost;
    api.GetNetOwnClientId = ModNetwork_GetOwnClientId;
    api.GetNetPeerCount = ModNetwork_GetPeerCount;
    api.GetNetPeerAt = ModNetwork_GetPeerAt;
    api.FindNetPeer = ModNetwork_FindPeer;
    api.GetNetPeerIdForActor = ModNetwork_GetPeerIdForActor;
    api.RequestNetTeleport = ModNetwork_RequestTeleport;
    api.SummonNetPeer = ModNetwork_SummonPeer;
    api.StartNetRoomCountdown = ModNetwork_StartRoomCountdown;
    api.StopNetRoomCountdown = ModNetwork_StopRoomCountdown;
    api.SetCustomItemWorn = CustomItemRegistry_SetWorn;
    api.IsCustomItemWorn = CustomItemRegistry_IsWorn;
    api.IsCustomItemAgeAllowed = CustomItemRegistry_IsFormAllowed;
    api.RegisterProgressiveChain = ProgressiveItems_Register;
    api.GetProgressiveLevel = ProgressiveItems_GetLevel;
    api.GetNextProgressiveItem = ProgressiveItems_GetNextItem;
    api.SetLabel = ModLabels_Set;
    api.SetTimeSplits = ModLabels_SetTimeSplits;
    api.SetTimeSplitGroup = ModLabels_SetTimeSplitGroup;
    api.ClearTimeSplitGroups = ModLabels_ClearTimeSplitGroups;
    api.SetItemIcon = ModLabels_SetItemIcon;
    api.GrantLogic = RandoLogic_Grant;
    api.RegisterSequence = Unbound_RegisterSequence;
    api.RegisterSoundFont = Unbound_RegisterSoundFont;
    api.GetSequenceId = Unbound_SequenceIdForPath;
    return api;
}

} // namespace

// Runs once, after ShipInit::InitAll: InitAll re-runs on every preset apply and would load the mods twice.
void ModApi_Init() {
    ProgressiveItems_Init();
    ModStorage_Init();
    CustomItemRegistry_Init();
    KaleidoItemManager_Init();
    KaleidoQuestManager_Init();
    CustomEquipRegistry_Init();
    KaleidoEquipManager_Init();
    VanillaItems_Init();
    Timers_Init();
    AudioMix_Init();
    PlayerInput_Init();
    FormRegistry_Init();
    RandoInfCustom_Init();
    RandoOptions_Init();
    TimeControl_Init();
    Microphone_Init();
    BossRegistry_Init();
    DungeonRegistry_Init();
    ModNetwork_Init();
    ModLoader_LoadMods();
    ModStore_Init();
    RegisterRegistryDumpCommand();
    ModApi_WriteRegistry(Ship::Context::GetPathRelativeToAppDirectory(RegistryFileName, appShortName));
}

// Items and actor types exist only inside mod binaries; tools such as Nocturne read them from this file.
bool ModApi_WriteRegistry(const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file) {
        SPDLOG_ERROR("[ModApi] Cannot write the mod registry to {}", path.string());
        return false;
    }
    // Item messages carry the game's glyph bytes, which need not be UTF-8; only this diagnostic copy replaces them.
    file << nlohmann::json{ { "items", DescribeItems() }, { "actors", DescribeActors() } }.dump(
        1, ' ', false, nlohmann::json::error_handler_t::replace);
    return true;
}

const S2HModApi* ModApi_Get() {
    static const S2HModApi api = BuildTable();
    return &api;
}

std::string ModApi_GetCurrentMod() {
    return sCurrentMod;
}

void ModApi_SetCurrentMod(const std::string& name) {
    sCurrentMod = name;
    ModNetwork_SetCurrentMod(name);
    BossRegistry_SetCurrentMod(name);
    CustomItemRegistry_SetCurrentMod(name);
    CustomEquipRegistry_SetCurrentMod(name);
}

void ModApi_ClearCurrentMod() {
    sCurrentMod.clear();
    ModNetwork_SetCurrentMod("");
    BossRegistry_SetCurrentMod("");
    CustomItemRegistry_ClearCurrentMod();
}

bool ModApi_HasHook(const char* name) {
    return HasHook(name);
}

std::string ModApi_GetCatalogFingerprint() {
    std::vector<std::string> entries;
    entries.reserve(HookEntries().size());
    for (const auto& [name, binder] : HookEntries()) {
        entries.push_back(name);
    }
    std::sort(entries.begin(), entries.end());

    uint64_t hash = 1469598103934665603ULL;
    for (const auto& entry : entries) {
        hash = HashText(hash, entry);
    }
    hash = HashText(hash, gGitCommitHash);
    hash = HashText(hash, __DATE__);
    hash = HashText(hash, __TIME__);
    return Hex(hash);
}
