#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "z64items.h"
#include "2s2h/GameInteractor/GameInteractor_ModHooks.h"
#include "2s2h/ModApi/ModMenu/ModUI.h"
#include "2s2h/ModApi/OcarinaSongs/OcarinaSongRegistry.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/Rando/Types.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"

#define KEY_SIZE 256
#define DROP_FLAGS ((1 << 2) | (1 << 8))
#define AUTO_CVAR(name) "gMods.ItemDropTester.Auto." name
#define AUTO_TEXT_SIZE 512
#define AUTO_PICKUP_TIMEOUT 200
#define AUTO_PRINT_TIMEOUT 100
#define AUTO_SETTLE_FRAMES 10
#define AUTO_CARRIER_CHECK RC_CLOCK_TOWN_SOUTH_CHEST_UPPER
#define TEXT_HEADER_SIZE 11
#define TEXT_ICON_OFFSET 2
#define TEXT_NEWLINE_CODE 0x11
#define TEXT_END_CODE 0xBF
#define PLAY_EQUIPMENT_PAGE PAUSE_MASK
typedef enum { AUTO_IDLE, AUTO_QUEUED, AUTO_GIVING, AUTO_PLAYING, AUTO_SHOWN, AUTO_CLOSING } AutoPhase;
typedef struct {
    int32_t randoItem;
    const char* name;
    const char* token;
} NativeItem;
static const NativeItem sNative[] = {
#include "rando_catalog.inc.h"
};
void Setup_InitRegs(void);
static const S2HModApi* sApi;
static AutoPhase sAutoPhase;
static int32_t sAutoItem;
static int32_t sAutoFrames;
static int32_t sAutoTextFrames;
static bool sChecklistDirty;
static char sPending[KEY_SIZE];
static char sSelected[KEY_SIZE];
static char sFilter[128];
static const char* sStatus = "Click an item to queue a pickup above Link. Resume gameplay to spawn it.";
static const char* const sHooks[] = { "OnGameFrameUpdate", "OnGameStateMainStart" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sHooks, ARRAY_COUNT(sHooks) };

static const char* DropKey(Actor* actor, PlayState* play) {
    return GameInteractor_ExecuteOnResolveCustomGetItem(actor, play, sApi->GetActorCustomItem(actor));
}
static void ReceiveDrop(EnItem00* item, PlayState* play) {
    const char* key = DropKey(&item->actor, play);
    if (sApi->FindCustomItem(key) == NULL)
        return;
    CustomItemRegistry_PrepareAcquisitionMessage(key);
    CustomItemRegistry_ReceiveGetItem(key);
}
static void DrawDrop(Actor* actor, PlayState* play) {
    Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
    CustomItemRegistry_DrawAcquisition(DropKey(actor, play), play);
}
// Fleet can boot MM straight into a save, past the setup that fills these registers. Without their fade speed a
// scene title card never ends, and the randomizer hands nothing out while one is up.
static void RestoreSetupRegisters(void) {
    if (XREG(73) == 0) {
        Setup_InitRegs();
    }
}

static bool IsMessageActive(PlayState* play) {
    return play->msgCtx.msgMode != MSGMODE_NONE;
}

static bool CanStartPickup(PlayState* play) {
    if (play->pauseCtx.state != PAUSE_STATE_OFF || IsMessageActive(play) ||
        play->transitionTrigger != TRANS_TRIGGER_OFF) {
        return false;
    }
    return !(GET_PLAYER(play)->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_400));
}

static const char* SpawnPendingDrop(PlayState* play) {
    const Vec3f pos = GET_PLAYER(play)->actor.world.pos;
    Actor* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ITEM00, pos.x, pos.y + 80.0f, pos.z, DROP_FLAGS, 0, 0,
                               ITEM00_NOTHING);
    if (actor == NULL) {
        return "Actor allocation failed. Try again after leaving the scene.";
    }
    if (!sApi->BindActorCustomItem(actor, sPending)) {
        Actor_Kill(actor);
        return "Could not bind the selected acquisition key.";
    }
    ((EnItem00*)actor)->actionFunc = ReceiveDrop;
    actor->draw = DrawDrop;
    return "Pickup spawned above Link. Collect it to test the real acquisition path.";
}

static RandoSaveCheck* GetCarrierCheck(void) {
    return &gSaveContext.save.shipSaveInfo.rando.randoSaveChecks[AUTO_CARRIER_CHECK];
}

static int32_t FindRandoItem(const char* name) {
    const int32_t modItem = RandoItems_GetRandoGet(name);
    if (modItem > 0) {
        return modItem;
    }
    for (size_t i = 0; i < ARRAY_COUNT(sNative); ++i) {
        if (!strcmp(name, sNative[i].token) || !strcmp(name, sNative[i].name)) {
            return sNative[i].randoItem;
        }
    }
    return RI_UNKNOWN;
}

#include "play_scenarios.inc.c"

static void TakePlayRequest(const char* request) {
    const bool isStarted = StartPlayScenario(request);
    CVarSetString(AUTO_CVAR("Text"), "");
    CVarSetInteger(AUTO_CVAR("Icon"), 0);
    CVarSetString(AUTO_CVAR("State"), isStarted ? "playing" : "unknown");
    sAutoPhase = isStarted ? AUTO_PLAYING : AUTO_IDLE;
    CVarSetString(AUTO_CVAR("Request"), "");
}

static void AdvancePlayScenario(void) {
    if (HasPlayFinished()) {
        CVarSetString(AUTO_CVAR("State"), "shown");
        sAutoPhase = AUTO_SHOWN;
    }
}

static void TakeAutoRequest(void) {
    const char* request = CVarGetString(AUTO_CVAR("Request"), "");
    if (request[0] == '\0') {
        return;
    }
    RestoreSetupRegisters();
    if (IsPlayRequest(request)) {
        TakePlayRequest(request);
        return;
    }
    sAutoItem = FindRandoItem(request);
    const bool isKnown = sAutoItem != RI_UNKNOWN;
    CVarSetString(AUTO_CVAR("State"), isKnown ? "queued" : "unknown");
    sAutoPhase = isKnown ? AUTO_QUEUED : AUTO_IDLE;
    sAutoFrames = 0;
    CVarSetString(AUTO_CVAR("Request"), "");
}

static void AbandonBlockedPickup(PlayState* play) {
    if (++sAutoFrames <= AUTO_PICKUP_TIMEOUT) {
        return;
    }
    char reason[AUTO_TEXT_SIZE];
    snprintf(reason, sizeof(reason), "pause %d, message %d, transition %d, player %08X", play->pauseCtx.state,
             play->msgCtx.msgMode, play->transitionTrigger, GET_PLAYER(play)->stateFlags1);
    CVarSetString(AUTO_CVAR("Text"), reason);
    CVarSetString(AUTO_CVAR("State"), "blocked");
    sAutoPhase = AUTO_IDLE;
}

static bool IsTextboxPrinted(PlayState* play) {
    const u8 state = Message_GetState(&play->msgCtx);
    return IsMessageActive(play) && state != TEXT_STATE_NONE && state != TEXT_STATE_CLOSING &&
           state != TEXT_STATE_FADING;
}

static void ClosePrintedTextbox(PlayState* play) {
    if (IsTextboxPrinted(play)) {
        Message_CloseTextbox(play);
    }
}

static void StartAutoPickup(PlayState* play) {
    if (!CanStartPickup(play)) {
        ClosePrintedTextbox(play);
        AbandonBlockedPickup(play);
        return;
    }
    RandoSaveCheck* carrier = GetCarrierCheck();
    carrier->randoItemId = (RandoItemId)sAutoItem;
    carrier->eligible = true;
    carrier->obtained = false;
    carrier->cycleObtained = false;
    sAutoFrames = 0;
    sAutoTextFrames = 0;
    CVarSetString(AUTO_CVAR("Text"), "");
    CVarSetInteger(AUTO_CVAR("Icon"), 0);
    CVarSetString(AUTO_CVAR("State"), "spawned");
    sAutoPhase = AUTO_GIVING;
}

static void ReadMessageText(PlayState* play, char* text, size_t capacity) {
    const MessageContext* msgCtx = &play->msgCtx;
    const u8* raw = msgCtx->font.msgBuf.schar;
    const s32 length = MIN(msgCtx->msgLength, MESSAGE_BUF_SIZE);
    size_t used = 0;
    for (s32 i = TEXT_HEADER_SIZE; i < length && raw[i] != TEXT_END_CODE && used + 1 < capacity; ++i) {
        if (raw[i] == TEXT_NEWLINE_CODE) {
            text[used++] = ' ';
        } else if (raw[i] >= ' ' && raw[i] <= '~') {
            text[used++] = (char)raw[i];
        }
    }
    text[used] = '\0';
}

static void PublishMessage(PlayState* play) {
    char text[AUTO_TEXT_SIZE];
    ReadMessageText(play, text, sizeof(text));
    CVarSetString(AUTO_CVAR("Text"), text);
    CVarSetInteger(AUTO_CVAR("Icon"), play->msgCtx.font.msgBuf.schar[TEXT_ICON_OFFSET]);
}

static void WatchAutoPickup(PlayState* play) {
    ++sAutoFrames;
    if (!IsMessageActive(play)) {
        if (sAutoFrames > AUTO_PICKUP_TIMEOUT) {
            CVarSetString(AUTO_CVAR("State"), "silent");
            sAutoPhase = AUTO_SHOWN;
        }
        return;
    }
    ++sAutoTextFrames;
    const bool isSettled = IsTextboxPrinted(play) && sAutoTextFrames >= AUTO_SETTLE_FRAMES;
    if (!isSettled && sAutoTextFrames < AUTO_PRINT_TIMEOUT) {
        return;
    }
    PublishMessage(play);
    CVarSetString(AUTO_CVAR("State"), "shown");
    sAutoPhase = AUTO_SHOWN;
}

static void ReleaseAutoPickup(PlayState* play) {
    if (!CVarGetInteger(AUTO_CVAR("Release"), 0)) {
        return;
    }
    CVarSetInteger(AUTO_CVAR("Release"), 0);
    sIsPlaying = false;
    if (IsMessageActive(play)) {
        Message_CloseTextbox(play);
    }
    sAutoFrames = 0;
    sAutoPhase = AUTO_CLOSING;
}

static void FinishAutoPickup(PlayState* play) {
    ClosePrintedTextbox(play);
    RandoSaveCheck* carrier = GetCarrierCheck();
    const bool isFree = !carrier->eligible && CanStartPickup(play);
    if (!isFree && ++sAutoFrames <= AUTO_PICKUP_TIMEOUT) {
        return;
    }
    carrier->eligible = false;
    CVarSetString(AUTO_CVAR("State"), isFree ? "done" : "stuck");
    sAutoPhase = AUTO_IDLE;
}

static void AdvanceAuto(PlayState* play) {
    switch (sAutoPhase) {
        case AUTO_IDLE:
            TakeAutoRequest();
            break;
        case AUTO_QUEUED:
            StartAutoPickup(play);
            break;
        case AUTO_GIVING:
            WatchAutoPickup(play);
            break;
        case AUTO_PLAYING:
            AdvancePlayScenario();
            break;
        case AUTO_SHOWN:
            ReleaseAutoPickup(play);
            break;
        case AUTO_CLOSING:
            FinishAutoPickup(play);
            break;
        default:
            sAutoPhase = AUTO_IDLE;
            break;
    }
}

#define SONG_CVAR(name) "gMods.ItemDropTester.Song." name
#define SONG_ANSWER_DELAY 20
#define SONG_TIMEOUT 600
typedef enum { SONG_IDLE, SONG_WARP } SongTest;
static SongTest sSongTest;
static s32 sSongFrames;
static bool sSongAsked;

static void PublishSongState(const char* state) {
    CVarSetString(SONG_CVAR("State"), state);
    sSongFrames = 0;
}

static void PublishSongText(PlayState* play) {
    char text[AUTO_TEXT_SIZE];
    ReadMessageText(play, text, sizeof(text));
    CVarSetString(SONG_CVAR("Text"), text);
}

static void PressButton(PlayState* play, u16 button) {
    play->state.input[0].press.button |= button;
    play->state.input[0].cur.button |= button;
    sSongFrames = 0;
}

static bool HasWaitedToAnswer(void) {
    return ++sSongFrames > SONG_ANSWER_DELAY;
}

static void StartSongWarp(PlayState* play, const char* itemKey) {
    const s8 slot = sApi->FindOcarinaSong(itemKey);
    if (slot < 0) {
        PublishSongState("unknown");
        return;
    }
    // z_message records the recognised slot before it performs the song.
    play->msgCtx.songPlayed = (u8)slot;
    OcarinaSongRegistry_Perform(play, (u8)slot);
    sSongAsked = false;
    sSongTest = SONG_WARP;
    PublishSongState("played");
}

static void SetOwlStatues(u16 owlStatues) {
    gSaveContext.save.saveInfo.playerData.owlActivationFlags = owlStatues;
    PublishSongState("armed");
}

static void TakeSongRequest(PlayState* play) {
    const char* request = CVarGetString(SONG_CVAR("Request"), "");
    if (request[0] == '\0') {
        return;
    }
    RestoreSetupRegisters();
    if (!CanStartPickup(play)) {
        if (++sSongFrames == AUTO_PICKUP_TIMEOUT) {
            char reason[AUTO_TEXT_SIZE];
            snprintf(reason, sizeof(reason), "pause %d, message %d, transition %d, player %08X", play->pauseCtx.state,
                     play->msgCtx.msgMode, play->transitionTrigger, GET_PLAYER(play)->stateFlags1);
            CVarSetString(SONG_CVAR("Text"), reason);
            CVarSetString(SONG_CVAR("State"), "blocked");
        }
        return;
    }
    CVarSetString(SONG_CVAR("Text"), "");
    if (!strncmp(request, "warp:", 5)) {
        StartSongWarp(play, request + 5);
    } else if (!strncmp(request, "owls:", 5)) {
        SetOwlStatues((u16)atoi(request + 5));
    } else {
        PublishSongState("unknown");
    }
    CVarSetString(SONG_CVAR("Request"), "");
}

static void AnswerSongWarp(PlayState* play) {
    const bool isChoice = Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE;
    if (isChoice && !sSongAsked) {
        sSongAsked = true;
        PublishSongText(play);
        PublishSongState("asked");
    }
    if (isChoice && HasWaitedToAnswer()) {
        play->msgCtx.choiceIndex = 0;
        PressButton(play, BTN_A);
        return;
    }
    if (sSongAsked && play->msgCtx.msgMode == MSGMODE_NONE) {
        sSongTest = SONG_IDLE;
        PublishSongState("answered");
    } else if (!sSongAsked && ++sSongFrames > SONG_TIMEOUT) {
        sSongTest = SONG_IDLE;
        PublishSongState("never asked");
    }
}

static void PublishSongDebug(PlayState* play) {
    static s32 sDebugFrames;
    const bool isPending = sSongTest != SONG_IDLE || CVarGetString(SONG_CVAR("Request"), "")[0] != '\0';
    if (!isPending || ++sDebugFrames % 10 != 0) {
        return;
    }
    char text[AUTO_TEXT_SIZE];
    ReadMessageText(play, text, sizeof(text));
    char debug[AUTO_TEXT_SIZE * 2];
    snprintf(debug, sizeof(debug),
             "test %d asked %d frames %d msgMode %d textState %d textId %04X pause %d trans %d "
             "gameMode %d player %08X ocarinaMode %d text '%s'",
             sSongTest, sSongAsked, sSongFrames, play->msgCtx.msgMode, Message_GetState(&play->msgCtx),
             play->msgCtx.currentTextId, play->pauseCtx.state, play->transitionTrigger, gSaveContext.gameMode,
             GET_PLAYER(play) != NULL ? GET_PLAYER(play)->stateFlags1 : 0, play->msgCtx.ocarinaMode, text);
    CVarSetString(SONG_CVAR("Debug"), debug);
}

static void AdvanceSongTest(void) {
    PlayState* play = gPlayState;
    if (play == NULL || gGameState != &play->state) {
        return;
    }
    PublishSongDebug(play);
    if (gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return;
    }
    if (sSongTest != SONG_IDLE && CVarGetString(SONG_CVAR("Request"), "")[0] != '\0') {
        sSongTest = SONG_IDLE;
        sSongFrames = 0;
    }
    switch (sSongTest) {
        case SONG_IDLE:
            TakeSongRequest(play);
            break;
        case SONG_WARP:
            AnswerSongWarp(play);
            break;
        default:
            sSongTest = SONG_IDLE;
            break;
    }
}

static void Tick(void) {
    if (sChecklistDirty) {
        CVarSave();
        sChecklistDirty = false;
    }
    PlayState* play = gPlayState;
    if (play == NULL) {
        return;
    }
    if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
        AdvanceAuto(play);
    }
    if (sPending[0] == '\0' || !CanStartPickup(play)) {
        return;
    }
    sStatus = SpawnPendingDrop(play);
    sPending[0] = '\0';
}
static void DrawChecklist(const char* key) {
    static const char* const labels[] = { "Pickup",    "Icon",  "Acquisition model", "Held model", "Use",
                                          "Save/load", "Anchor" };
    static const char* const fields[] = { "pickup", "icon", "acquisition", "held", "use", "save", "anchor" };
    ModUI_Text("Checklist (manual results, saved locally):");
    for (int i = 0; i < 7; ++i) {
        char cvar[KEY_SIZE + 64];
        snprintf(cvar, sizeof(cvar), "gMods.ItemDropTester.Check.%s.%s", key, fields[i]);
        bool passed = CVarGetInteger(cvar, 0) != 0;
        if (ModUI_Checkbox(labels[i], &passed)) {
            CVarSetInteger(cvar, passed);
            sChecklistDirty = true;
        }
    }
}
static void CopyChecklist(void) {
    const uint32_t count = sApi->GetCustomItemCount();
    if (count > 10000)
        return;
    const size_t capacity = (size_t)count * (KEY_SIZE + 160) + 256;
    char* text = malloc(capacity);
    if (text == NULL)
        return;
    size_t used = snprintf(text, capacity,
                           "# 2Ship custom item checklist\n\n| Item / subtype | Pickup | Icon | Acquisition | Held | "
                           "Use | Save/load | Anchor |\n|---|---|---|---|---|---|---|---|\n");
    static const char* const fields[] = { "pickup", "icon", "acquisition", "held", "use", "save", "anchor" };
    for (uint32_t i = 0; i < count; ++i) {
        const S2HCustomItemDefinition* item = sApi->GetCustomItemAt(i);
        if (item == NULL || item->key == NULL || strlen(item->key) >= KEY_SIZE)
            continue;
        used += snprintf(text + used, capacity - used, "| %s |", item->key);
        for (int f = 0; f < 7; ++f) {
            char cvar[KEY_SIZE + 64];
            snprintf(cvar, sizeof(cvar), "gMods.ItemDropTester.Check.%s.%s", item->key, fields[f]);
            used += snprintf(text + used, capacity - used, " %s |", CVarGetInteger(cvar, 0) ? "OK" : "pending");
        }
        used += snprintf(text + used, capacity - used, "\n");
    }
    ModUI_SetClipboardText(text);
    free(text);
    sStatus = "Markdown checklist copied to clipboard.";
}
static void DrawMenu(void) {
    ModUI_TextWrapped("Click Drop for an actual collectible above Link. This does not grant ownership until pickup. "
                      "Registered subtypes are separate rows.");
    ModUI_TextWrapped(sStatus);
    if (ModUI_Button("Copy full checklist (Markdown)", 0, 0))
        CopyChecklist();
    ModUI_InputTextWithHint("Filter", "Item key or subtype", sFilter, sizeof(sFilter), 0);
    if (ModUI_Button("Cancel queued drop", 0, 0))
        sPending[0] = '\0';
    bool visible = ModUI_BeginChild("DropCatalog", 0, 280, true, 0);
    if (visible)
        for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i) {
            const S2HCustomItemDefinition* item = sApi->GetCustomItemAt(i);
            if (item == NULL || item->key == NULL || (sFilter[0] && strstr(item->key, sFilter) == NULL))
                continue;
            char label[KEY_SIZE + 32];
            snprintf(label, sizeof(label), "Drop %s", item->key);
            ModUI_BeginDisabled(gPlayState == NULL || sPending[0] != '\0' || strlen(item->key) >= KEY_SIZE);
            if (ModUI_Button(label, 0, 0)) {
                snprintf(sPending, sizeof(sPending), "%s", item->key);
                snprintf(sSelected, sizeof(sSelected), "%s", item->key);
                sStatus = "Queued. Close the menu and resume gameplay.";
            }
            ModUI_EndDisabled();
        }
    ModUI_EndChild();
    const S2HCustomItemDefinition* selected = sApi->FindCustomItem(sSelected);
    if (selected != NULL) {
        ModUI_Text(selected->key);
        DrawChecklist(selected->key);
        const S2HCustomPickupVariant* subtype = sApi->GetPickupVariant(selected->key);
        if (subtype != NULL) {
            ModUI_Text("Parent item:");
            ModUI_Text(subtype->itemKey);
            ModUI_Text("Subtype:");
            ModUI_Text(subtype->variantKey);
        }
        ModUI_Text("Icon:");
        ModUI_Text(selected->iconPath ? selected->iconPath : "(none)");
        ModUI_Text("Acquisition model:");
        ModUI_Text(selected->getItemModelPath        ? selected->getItemModelPath
                   : selected->getItemEntry.drawFunc ? "Custom draw callback"
                                                     : "Native draw ID / icon fallback");
        ModUI_Text("Held model:");
        ModUI_Text(selected->heldModelPath ? selected->heldModelPath : "(custom drawing or none)");
    }
}
S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}
S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}
S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, GetPickupVariant))
        return;
    sApi->RegisterMenuSidebar("Mods", "Item Drop Tester", "gMods.Menu.Page", 1);
    S2HModMenuWidget widget = { 0 };
    widget.structSize = sizeof(widget);
    widget.section = "Mods";
    widget.sidebar = "Item Drop Tester";
    widget.type = S2H_MOD_MENU_CUSTOM;
    widget.label = "Item Drop Tester";
    widget.onChange = DrawMenu;
    sApi->RegisterMenuWidget(&widget);
    S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, Tick);
    S2H_REGISTER_HOOK(sApi, OnGameStateMainStart, PressPlayButtons);
    if (S2H_MOD_API_HAS(sApi, FindOcarinaSong)) {
        S2H_REGISTER_HOOK(sApi, OnGameStateMainStart, AdvanceSongTest);
    }
}
