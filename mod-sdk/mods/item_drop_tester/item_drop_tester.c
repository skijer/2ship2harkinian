#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "z64items.h"
#include "2s2h/GameInteractor/GameInteractor_ModHooks.h"
#include "2s2h/ModApi/ModMenu/ModUI.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"

#define KEY_SIZE 256
#define DROP_FLAGS ((1 << 2) | (1 << 8))
static const S2HModApi* sApi;
static bool sChecklistDirty;
static char sPending[KEY_SIZE];
static char sSelected[KEY_SIZE];
static char sFilter[128];
static const char* sStatus = "Click an item to queue a pickup above Link. Resume gameplay to spawn it.";
static const char* const sHooks[] = { "OnGameFrameUpdate" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sHooks, 1 };

static const char* DropKey(Actor* actor, PlayState* play) {
    return GameInteractor_ExecuteOnResolveCustomGetItem(actor, play, sApi->GetActorCustomItem(actor));
}
static void ReceiveDrop(EnItem00* item, PlayState* play) {
    const char* key = DropKey(&item->actor, play);
    if (sApi->FindCustomItem(key) == NULL) return;
    CustomItemRegistry_PrepareAcquisitionMessage(key);
    CustomItemRegistry_ReceiveGetItem(key);
}
static void DrawDrop(Actor* actor, PlayState* play) {
    Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
    CustomItemRegistry_DrawAcquisition(DropKey(actor, play), play);
}
static void Tick(void) {
    if (sChecklistDirty) { CVarSave(); sChecklistDirty = false; }
    PlayState* play = gPlayState;
    if (!sPending[0] || play == NULL || play->pauseCtx.state != PAUSE_STATE_OFF ||
        play->msgCtx.msgMode != MSGMODE_NONE || play->transitionTrigger != TRANS_TRIGGER_OFF) return;
    Player* player = GET_PLAYER(play);
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_400)) return;
    Vec3f pos = player->actor.world.pos;
    Actor* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ITEM00,
                              pos.x, pos.y + 80.0f, pos.z, DROP_FLAGS, 0, 0, ITEM00_NOTHING);
    if (actor != NULL) {
        if (sApi->BindActorCustomItem(actor, sPending)) {
            ((EnItem00*)actor)->actionFunc = ReceiveDrop;
            actor->draw = DrawDrop;
            sStatus = "Pickup spawned above Link. Collect it to test the real acquisition path.";
        } else {
            Actor_Kill(actor);
            sStatus = "Could not bind the selected acquisition key.";
        }
    } else sStatus = "Actor allocation failed. Try again after leaving the scene.";
    sPending[0] = '\0';
}
static void DrawChecklist(const char* key) {
    static const char* const labels[] = { "Pickup", "Icon", "Acquisition model", "Held model", "Use", "Save/load", "Anchor" };
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
    if (count > 10000) return;
    const size_t capacity = (size_t)count * (KEY_SIZE + 160) + 256;
    char* text = malloc(capacity);
    if (text == NULL) return;
    size_t used = snprintf(text, capacity, "# 2Ship custom item checklist\n\n| Item / subtype | Pickup | Icon | Acquisition | Held | Use | Save/load | Anchor |\n|---|---|---|---|---|---|---|---|\n");
    static const char* const fields[] = { "pickup", "icon", "acquisition", "held", "use", "save", "anchor" };
    for (uint32_t i = 0; i < count; ++i) {
        const S2HCustomItemDefinition* item = sApi->GetCustomItemAt(i);
        if (item == NULL || item->key == NULL || strlen(item->key) >= KEY_SIZE) continue;
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
    ModUI_TextWrapped("Click Drop for an actual collectible above Link. This does not grant ownership until pickup. Registered subtypes are separate rows.");
    ModUI_TextWrapped(sStatus);
    if (ModUI_Button("Copy full checklist (Markdown)", 0, 0)) CopyChecklist();
    ModUI_InputTextWithHint("Filter", "Item key or subtype", sFilter, sizeof(sFilter), 0);
    if (ModUI_Button("Cancel queued drop", 0, 0)) sPending[0] = '\0';
    bool visible = ModUI_BeginChild("DropCatalog", 0, 280, true, 0);
    if (visible) for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i) {
        const S2HCustomItemDefinition* item = sApi->GetCustomItemAt(i);
        if (item == NULL || item->key == NULL || (sFilter[0] && strstr(item->key, sFilter) == NULL)) continue;
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
            ModUI_Text("Parent item:"); ModUI_Text(subtype->itemKey);
            ModUI_Text("Subtype:"); ModUI_Text(subtype->variantKey);
        }
        ModUI_Text("Icon:"); ModUI_Text(selected->iconPath ? selected->iconPath : "(none)");
        ModUI_Text("Acquisition model:"); ModUI_Text(selected->getItemModelPath ? selected->getItemModelPath :
            selected->getItemEntry.drawFunc ? "Custom draw callback" : "Native draw ID / icon fallback");
        ModUI_Text("Held model:"); ModUI_Text(selected->heldModelPath ? selected->heldModelPath : "(custom drawing or none)");
    }
}
S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) { sApi = api; }
S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) { return &sRequirements; }
S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, GetPickupVariant)) return;
    sApi->RegisterMenuSidebar("Mods", "Item Drop Tester", "gMods.Menu.Page", 1);
    S2HModMenuWidget widget = {0};
    widget.structSize = sizeof(widget);
    widget.section = "Mods";
    widget.sidebar = "Item Drop Tester";
    widget.type = S2H_MOD_MENU_CUSTOM;
    widget.label = "Item Drop Tester";
    widget.onChange = DrawMenu;
    sApi->RegisterMenuWidget(&widget);
    S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, Tick);
}
