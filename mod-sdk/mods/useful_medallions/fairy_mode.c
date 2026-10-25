// Spirit possession uses a controllable native MM fairy, owned by this mod.
#include "sw97_mod.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include <string.h>

void EnElf_Init(Actor* actor, PlayState* play);
void EnElf_Destroy(Actor* actor, PlayState* play);
void EnElf_Draw(Actor* actor, PlayState* play);
void func_8083B1A0(Player* player, PlayState* play);
void Player_SetAction_PreserveItemAction(PlayState* play, Player* player, PlayerActionFunc action, s32 flags);

static struct {
    bool active;
    s16 timer;
    Actor* fairy;
    ActorFunc draw;
    PlayerActionFunc action;
    u32 stateFlags;
} sFairy;
static s16 sFairyId = -1;

bool Sw97_IsFairyModeActive(void) { return sFairy.active; }

static void HoldBody(Player* player, PlayState* play) {
    player->speedXZ = player->actor.speed = 0.0f;
    player->actor.velocity = (Vec3f){ 0, 0, 0 };
    player->actor.draw = NULL;
    if (sFairy.fairy != NULL) {
        player->actor.world.pos = sFairy.fairy->world.pos;
        player->actor.focus.pos = sFairy.fairy->world.pos;
    }
}

static void FairyInit(Actor* actor, PlayState* play) {
    actor->params = FAIRY_TYPE_5;
    EnElf_Init(actor, play);
    actor->room = -1;
}

static void FairyUpdate(Actor* actor, PlayState* play) {
    EnElf* fairy = (EnElf*)actor;
    Input* input = &play->state.input[0];
    s16 yaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play));
    f32 forward = input->cur.stick_y * 0.08f;
    f32 sideways = input->cur.stick_x * 0.08f;
    actor->world.pos.x += Math_SinS(yaw) * forward + Math_CosS(yaw) * sideways;
    actor->world.pos.z += Math_CosS(yaw) * forward - Math_SinS(yaw) * sideways;
    actor->world.pos.y += (input->cur.button & BTN_A ? 3.0f : 0.0f) - (input->cur.button & BTN_R ? 3.0f : 0.0f);
    SkelAnime_Update(&fairy->skelAnime);
}

static void EndFlight(Player* player, PlayState* play) {
    if (player != NULL) {
        player->actor.draw = sFairy.draw;
        player->stateFlags1 = (player->stateFlags1 & ~PLAYER_STATE1_20) | (sFairy.stateFlags & PLAYER_STATE1_20);
        Player_SetAction_PreserveItemAction(play, player, sFairy.action != NULL ? sFairy.action : func_8083B1A0, 0);
        player->invincibilityTimer = 20;
    }
    if (sFairy.fairy != NULL) Actor_Kill(sFairy.fairy);
    sFairy.fairy = NULL;
    sFairy.active = false;
}

void Sw97_StartFairyMode(void) {
    PlayState* play = gPlayState;
    Player* player = play == NULL ? NULL : GET_PLAYER(play);
    if (sFairy.active || player == NULL || sFairyId < 0) return;
    Actor* fairy = Actor_Spawn(&play->actorCtx, play, sFairyId, player->actor.world.pos.x,
        player->actor.world.pos.y + Player_GetHeight(player) + 5.0f, player->actor.world.pos.z, 0, 0, 0, FAIRY_TYPE_5);
    if (fairy == NULL) return;
    memset(&sFairy, 0, sizeof(sFairy));
    sFairy.fairy = fairy;
    sFairy.draw = player->actor.draw;
    sFairy.action = player->actionFunc;
    sFairy.stateFlags = player->stateFlags1;
    sFairy.active = true;
    Player_SetAction_PreserveItemAction(play, player, HoldBody, 0);
}

static void TickFlight(Player* player) {
    if (!sFairy.active || gPlayState == NULL) return;
    u32 buttons = BTN_B;
    static const u32 masks[S2H_ITEM_BUTTON_COUNT] = { BTN_B, BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0, BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };
    for (u8 i = 0; i < S2H_ITEM_BUTTON_COUNT; i++) {
        const char* key = gSw97Api->GetEquippedCustomItem(i);
        if (key != NULL && strcmp(key, "nei.medallion.spirit") == 0) buttons |= masks[i];
    }
    if (sFairy.fairy == NULL || (player->stateFlags1 & PLAYER_STATE1_DEAD) ||
        (++sFairy.timer > 10 && (gPlayState->state.input[0].press.button & buttons))) {
        EndFlight(player, gPlayState);
        return;
    }
    HoldBody(player, gPlayState);
}

static void ForgetFairy(Actor* actor) {
    if (actor == sFairy.fairy) sFairy.fairy = NULL;
}
static void ForgetScene(void) { memset(&sFairy, 0, sizeof(sFairy)); }

void Sw97_RegisterFairyMode(void) {
    S2HActorDefinition actor = { 0 };
    actor.structSize = sizeof(actor);
    actor.key = "nei.medallion.spirit_fairy";
    actor.description = "Spirit Medallion fairy";
    actor.category = ACTORCAT_ITEMACTION;
    actor.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor.objectId = GAMEPLAY_KEEP;
    actor.instanceSize = sizeof(EnElf);
    actor.init = FairyInit;
    actor.destroy = EnElf_Destroy;
    actor.update = FairyUpdate;
    actor.draw = EnElf_Draw;
    sFairyId = gSw97Api->RegisterActor(&actor);
    S2H_REGISTER_HOOK(gSw97Api, OnPlayerUpdate, TickFlight);
    S2H_REGISTER_HOOK(gSw97Api, OnActorDestroy, ForgetFairy);
    S2H_REGISTER_HOOK(gSw97Api, OnPlayDestroy, ForgetScene);
}
