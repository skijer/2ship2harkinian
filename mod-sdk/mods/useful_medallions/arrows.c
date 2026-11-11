// The elemental shots: the bow and the slingshot each carry a primed element, the shot flies as its vanilla
// arrow type and wears the SW97 arrow actor as its effect. Dark shots drain life, ice shots crack mud walls, and
// light shots wake sun switches, as NEI's SW97 mode does.

#include "z64items.h"
#include "z64wheel.h"

#include <string.h>

#include "sw97_mod.h"
#include "overlays/actors/ovl_Obj_Lightswitch/z_obj_lightswitch.h"

#define ELEMENTS_STORAGE_FIELD "elements"
#define TRACKED_ARROW_COUNT 16
#define LIFESTEAL_SLOTS 16
#define LIFESTEAL_TOTAL_FRAMES 90
#define LIFESTEAL_TICK_FRAMES 20
#define LIFESTEAL_DAMAGE_PER_TICK 4
#define SUN_SWITCH_SLOTS 8
#define SUN_SWITCH_STAYS_LIT_ROOM 25

typedef struct {
    uint8_t bow;
    uint8_t slingshot;
} SavedElements;

typedef struct {
    EnArrow* arrow;
    Sw97Element element;
    bool isSeed;
    bool isShot;
    bool hasBlure;
} TrackedArrow;

typedef struct {
    Actor* actor;
    s16 remainingFrames;
    s16 tickPhase;
} Lifesteal;

typedef struct {
    Actor* actor;
    bool isLitByArrow;
} SunSwitch;

// The address of an exe function is only comparable through its import; &f alone is the DLL's own thunk.
extern HOST_DATA void func_8088ACE0(EnArrow* arrow, PlayState* play);

static const char* const sElementKeys[SW97_ELEM_COUNT] = { NULL, "nei.medallion.fire", "nei.medallion.water", "nei.medallion.light", "nei.medallion.shadow", "nei.medallion.spirit", "nei.medallion.forest" };

// A vanilla elemental arrow unlocks its element too, so fire arrows alone still give the fire entry.
static const int16_t sElementVanillaArrow[SW97_ELEM_COUNT] = {
    -1, ITEM_ARROW_FIRE, ITEM_ARROW_ICE, ITEM_ARROW_LIGHT, -1, -1, -1,
};

// Vanilla's own damage table for ARROW_FIRE..ARROW_0E, which the elemental seeds borrow.
static const u32 sElementDamage[SW97_ELEM_COUNT] = {
    0, 0x00000800, 0x00001000, 0x00002000, 0x00010000, 0x00004000, 0x00008000,
};

static EffectBlureInit2 sBlureDark = {
    0, 4, 0, { 0, 255, 200, 255 }, { 0, 255, 255, 255 }, { 0, 255, 200, 0 }, { 0, 255, 255, 0 }, 16,
    0, 1, 0, { 80, 0, 80, 255 },   { 30, 0, 30, 0 },
};
static EffectBlureInit2 sBlureSoul = {
    0, 4, 0, { 0, 255, 200, 255 },   { 0, 255, 255, 255 }, { 0, 255, 200, 0 }, { 0, 255, 255, 0 }, 16,
    0, 1, 0, { 255, 255, 170, 255 }, { 200, 200, 0, 0 },
};
static EffectBlureInit2 sBlureWind = {
    0, 4, 0, { 0, 255, 200, 255 },   { 0, 255, 255, 255 }, { 0, 255, 200, 0 }, { 0, 255, 255, 0 }, 16,
    0, 1, 0, { 170, 255, 170, 255 }, { 0, 180, 0, 0 },
};

static ColliderJntSphElementInit sSunSwitchLightElementInit[] = {
    {
        {
            ELEM_MATERIAL_UNK0,
            { 0x00000000, 0x00, 0x00 },
            { 0x00202000, 0x00, 0x00 },
            ATELEM_NONE,
            ACELEM_ON,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 19 }, 100 },
    },
};
static ColliderJntSphInit sSunSwitchLightInit = {
    {
        COL_MATERIAL_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_JNTSPH,
    },
    1,
    sSunSwitchLightElementInit,
};


static SavedElements sElements;
static TrackedArrow sTrackedArrows[TRACKED_ARROW_COUNT];
static Lifesteal sLifesteals[LIFESTEAL_SLOTS];
static uint8_t sLifestealCount;
static SunSwitch sSunSwitches[SUN_SWITCH_SLOTS];

static bool IsElementOwned(Sw97Element element) {
    if (element == SW97_ELEM_NONE) {
        return true;
    }
    if (element >= SW97_ELEM_COUNT) {
        return false;
    }
    if (gSw97Api->IsCustomItemOwned(sElementKeys[element])) {
        return true;
    }
    return sElementVanillaArrow[element] >= 0 &&
           INV_CONTENT((uint16_t)sElementVanillaArrow[element]) == (uint8_t)sElementVanillaArrow[element];
}

uint8_t Sw97_ElementCount(void) {
    uint8_t count = 0;

    for (uint8_t element = 0; element < SW97_ELEM_COUNT; element++) {
        count += IsElementOwned((Sw97Element)element) ? 1 : 0;
    }
    return count;
}

Sw97Element Sw97_ElementAt(uint8_t index) {
    uint8_t seen = 0;

    for (uint8_t element = 0; element < SW97_ELEM_COUNT; element++) {
        if (!IsElementOwned((Sw97Element)element)) {
            continue;
        }
        if (seen == index) {
            return (Sw97Element)element;
        }
        seen++;
    }
    return SW97_ELEM_NONE;
}

static void StoreElements(void) {
    gSw97Api->StorageSet(SW97_MOD_NAME, ELEMENTS_STORAGE_FIELD, &sElements, sizeof(sElements));
}

// A medallion can be lost after its element was primed, so a stale flag falls back to the plain shot.
Sw97Element Sw97_GetElement(bool isSling) {
    uint8_t* stored = isSling ? &sElements.slingshot : &sElements.bow;

    if (!IsElementOwned((Sw97Element)*stored)) {
        *stored = SW97_ELEM_NONE;
        StoreElements();
    }
    return (Sw97Element)*stored;
}

void Sw97_SetElement(bool isSling, Sw97Element element) {
    if (!IsElementOwned(element)) {
        return;
    }
    if (isSling) {
        sElements.slingshot = element;
    } else {
        sElements.bow = element;
    }
    StoreElements();
}

static void LoadElements(int32_t fileNum) {
    memset(&sElements, 0, sizeof(sElements));
    gSw97Api->StorageGet(SW97_MOD_NAME, ELEMENTS_STORAGE_FIELD, &sElements, sizeof(sElements));
}

static uint8_t HeldButtonItem(Player* player) {
    s32 button = player->heldItemButton;
    if (button >= 0 && button < EQUIP_SLOT_MAX) return GET_CUR_FORM_BTN_ITEM(button);
    if (button >= S2H_ITEM_BUTTON_DPAD_FIRST && button < S2H_ITEM_BUTTON_COUNT) return DPAD_GET_CUR_FORM_BTN_ITEM(button - S2H_ITEM_BUTTON_DPAD_FIRST);
    return ITEM_NONE;
}

static bool IsPrimedBowShot(Player* player) {
    return HeldButtonItem(player) == ITEM_BOW && Sw97_GetElement(false) != SW97_ELEM_NONE;
}

static void ResolveBowItemAction(int32_t item, int8_t* itemAction) {
    Sw97Element element;

    if (item != ITEM_BOW) {
        return;
    }
    element = Sw97_GetElement(false);
    if (element >= SW97_ELEM_FIRE && element <= SW97_ELEM_LIGHT) {
        *itemAction = (int8_t)(PLAYER_IA_BOW + element);
    }
}

// SW97 shots cost no magic in NEI: the fire, ice and light ones ride vanilla's paid arrow types here.
static void WaiveElementalShotCost(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if (IsPrimedBowShot(player)) {
        *should = false;
    }
}

static TrackedArrow* FindTrackedArrow(Actor* arrow) {
    for (uint8_t i = 0; i < TRACKED_ARROW_COUNT; i++) {
        if (sTrackedArrows[i].arrow != NULL && &sTrackedArrows[i].arrow->actor == arrow) {
            return &sTrackedArrows[i];
        }
    }
    return NULL;
}

static TrackedArrow* ClaimTrackedArrow(void) {
    for (uint8_t i = 0; i < TRACKED_ARROW_COUNT; i++) {
        if (sTrackedArrows[i].arrow == NULL) {
            return &sTrackedArrows[i];
        }
    }
    return NULL;
}

static EffectBlureInit2* ElementBlure(Sw97Element element) {
    if (element == SW97_ELEM_DARK) {
        return &sBlureDark;
    }
    if (element == SW97_ELEM_SOUL) {
        return &sBlureSoul;
    }
    return element == SW97_ELEM_WIND ? &sBlureWind : NULL;
}

// Which element a freshly spawned arrow belongs to, or none when it is not a SW97 shot at all.
static Sw97Element ElementOfNewArrow(EnArrow* arrow, Player* player) {
    const char* held=CustomItemRegistry_GetHeldKey();
    if(arrow->actor.params==ARROW_TYPE_SLINGSHOT && held && !strcmp(held,"oot.slingshot"))
        return Sw97_GetElement(true);
    if (arrow->actor.params >= ARROW_TYPE_NORMAL_LIT && arrow->actor.params <= ARROW_TYPE_LIGHT && IsPrimedBowShot(player)) return Sw97_GetElement(false);
    return SW97_ELEM_NONE;
}

static void SpawnElementEffect(PlayState* play, TrackedArrow* tracked) {
    Actor_SpawnAsChild(&play->actorCtx, &tracked->arrow->actor, play, Sw97_GetArrowActorId(tracked->element),
                       tracked->arrow->actor.world.pos.x, tracked->arrow->actor.world.pos.y,
                       tracked->arrow->actor.world.pos.z, 0, 0, 0, 0);
}

// Vanilla's ARROW_0C..0E get no trail at all, so the three SW97-only elements bring their own.
static void TrackNewArrow(Actor* actor) {
    PlayState* play = gPlayState;
    EnArrow* arrow = (EnArrow*)actor;
    Sw97Element element = play == NULL ? SW97_ELEM_NONE : ElementOfNewArrow(arrow, GET_PLAYER(play));
    TrackedArrow* tracked = element == SW97_ELEM_NONE ? NULL : ClaimTrackedArrow();
    EffectBlureInit2* blure;

    if (tracked == NULL) {
        return;
    }
    memset(tracked, 0, sizeof(*tracked));
    tracked->arrow = arrow;
    tracked->element = element;
    tracked->isSeed = arrow->actor.params == ARROW_TYPE_SLINGSHOT;
    if (tracked->isSeed || element >= SW97_ELEM_DARK) {
        arrow->collider.elem.atDmgInfo.dmgFlags = sElementDamage[element];
    }
    blure = tracked->isSeed ? NULL : ElementBlure(element);
    if (blure != NULL) {
        Effect_Destroy(play, arrow->unk_240);
        Effect_Add(play, &arrow->unk_240, EFFECT_BLURE2, 0, 0, blure);
        tracked->hasBlure = true;
    }
    SpawnElementEffect(play, tracked);
}

// Vanilla respawns its own effect whenever the child is gone, so the SW97 one takes its place every time.
static void KeepElementEffect(PlayState* play, TrackedArrow* tracked) {
    Actor* child = tracked->arrow->actor.child;

    if (child != NULL && child->id == Sw97_GetArrowActorId(tracked->element)) {
        return;
    }
    if (child != NULL) {
        Actor_Kill(child);
        tracked->arrow->actor.child = NULL;
    }
    SpawnElementEffect(play, tracked);
}

static void AnnounceShot(PlayState* play, TrackedArrow* tracked) {
    Player* player = GET_PLAYER(play);

    if (tracked->isShot || tracked->arrow->actor.parent != NULL) {
        return;
    }
    tracked->isShot = true;
    if (!tracked->isSeed && tracked->element >= SW97_ELEM_DARK) {
        Player_PlaySfx(player, NA_SE_IT_MAGIC_ARROW_SHOT);
    }
}

// Vanilla zeroes a seed's rotation when it is shot; the SW97 cone orients itself off it, so it follows the arc.
static void AimSeedEffect(TrackedArrow* tracked) {
    Actor* seed = &tracked->arrow->actor;

    if (!tracked->isShot || tracked->arrow->actionFunc != func_8088ACE0) {
        return;
    }
    // NEI uses OoT Math_Atan2S(x,y). MM's base function takes (y,x);
    // its XY adapter preserves OoT's pitch (horizontal flight must be 0, not 90 degrees).
    seed->shape.rot.x = Math_Atan2S_XY(seed->speed, -seed->velocity.y);
    seed->shape.rot.y = seed->world.rot.y;
    seed->shape.rot.z = 0;
}

static void StartLifesteal(Actor* target) {
    Lifesteal* free = NULL;

    if (target == NULL || target->update == NULL ||
        (target->category != ACTORCAT_ENEMY && target->category != ACTORCAT_BOSS)) {
        return;
    }
    for (uint8_t i = 0; i < LIFESTEAL_SLOTS; i++) {
        if (sLifesteals[i].actor == target) {
            free = &sLifesteals[i];
            break;
        }
        if (free == NULL && sLifesteals[i].actor == NULL) {
            free = &sLifesteals[i];
        }
    }
    if (free == NULL) {
        return;
    }
    if (free->actor == NULL) {
        sLifestealCount++;
    }
    free->actor = target;
    free->remainingFrames = LIFESTEAL_TOTAL_FRAMES;
    free->tickPhase = LIFESTEAL_TICK_FRAMES;
}

static void UpdateTrackedArrow(Actor* actor) {
    PlayState* play = gPlayState;
    TrackedArrow* tracked = FindTrackedArrow(actor);

    if (play == NULL || tracked == NULL) {
        return;
    }
    KeepElementEffect(play, tracked);
    AnnounceShot(play, tracked);
    if (tracked->isSeed) {
        AimSeedEffect(tracked);
        // Seeds retain their native projectile type. Apply Ice Arrow water
        // behavior explicitly rather than losing it with that identity.
        if(tracked->isShot && tracked->element==SW97_ELEM_ICE){
            WaterBox* box; f32 y=actor->world.pos.y;
            if(WaterBox_GetSurface1(play,&play->colCtx,actor->world.pos.x,actor->world.pos.z,&y,&box) &&
               actor->world.pos.y<y && actor->prevPos.y>=y &&
               Sw97_SpawnIcePlatform(play,actor->world.pos.x,y,actor->world.pos.z))Actor_Kill(actor);
        }
    }
    if (tracked->element == SW97_ELEM_DARK && (tracked->arrow->unk_261 & 1)) {
        StartLifesteal(tracked->arrow->collider.base.at);
    }
}

static void ForgetTrackedArrow(Actor* actor) {
    TrackedArrow* tracked = FindTrackedArrow(actor);

    if (tracked == NULL) {
        return;
    }
    tracked->arrow = NULL;
}

static void Drain(Lifesteal* lifesteal) {
    Actor* target = lifesteal->actor;
    s16 drain = MIN(target->colChkInfo.health, LIFESTEAL_DAMAGE_PER_TICK);

    target->colChkInfo.health -= drain;
    // Drained to nothing, the actor's own update has to see a lethal hit to run its death.
    if (target->colChkInfo.health <= 0) {
        target->colChkInfo.health = 0;
        target->colChkInfo.damage = 8;
        target->colChkInfo.damageEffect = 0;
        target->colorFilterTimer = 0;
    }
    gSaveContext.save.saveInfo.playerData.health = MIN(gSaveContext.save.saveInfo.playerData.health + drain, gSaveContext.save.saveInfo.playerData.healthCapacity);
    Actor_SetColorFilter(target, 0x8000, 200, 0x2000, LIFESTEAL_TICK_FRAMES);
}

static void EndLifesteal(Lifesteal* lifesteal) {
    lifesteal->actor = NULL;
    sLifestealCount--;
}

// Ticked from the drained actor's own update, so a frozen or culled target stops bleeding too.
static void TickLifesteal(Actor* actor) {
    if (sLifestealCount == 0) {
        return;
    }
    for (uint8_t i = 0; i < LIFESTEAL_SLOTS; i++) {
        Lifesteal* lifesteal = &sLifesteals[i];

        if (lifesteal->actor != actor) {
            continue;
        }
        if (actor->update == NULL) {
            EndLifesteal(lifesteal);
            return;
        }
        if (--lifesteal->tickPhase <= 0) {
            lifesteal->tickPhase = LIFESTEAL_TICK_FRAMES;
            Drain(lifesteal);
        }
        if (--lifesteal->remainingFrames <= 0) {
            EndLifesteal(lifesteal);
        }
        return;
    }
}


static SunSwitch* FindSunSwitch(Actor* actor) {
    for (uint8_t i = 0; i < SUN_SWITCH_SLOTS; i++) {
        if (sSunSwitches[i].actor == actor) {
            return &sSunSwitches[i];
        }
    }
    return NULL;
}

static void OpenSunSwitchToLight(Actor* actor) {
    ObjLightswitch* sunSwitch = (ObjLightswitch*)actor;
    SunSwitch* slot = FindSunSwitch(NULL);

    Collider_SetJntSph(gPlayState, &sunSwitch->collider, actor, &sSunSwitchLightInit, &sunSwitch->elements);
    Collider_UpdateSpheres(0, &sunSwitch->collider);
    if (slot != NULL) {
        slot->actor = actor;
        slot->isLitByArrow = false;
    }
}

static bool AreVanillaLightArrowsSunlit(void) {
    return CVarGetInteger("gEnhancements.SunlightArrows", 0);
}

static bool IsElementalLightShot(Actor* shot) {
    TrackedArrow* tracked = FindTrackedArrow(shot);

    return tracked != NULL && tracked->element == SW97_ELEM_LIGHT;
}

// Runs before the switch's own update, so a refused hit is gone by the time it looks: without the Sunlight Arrows
// option only the SW97 light shots wake it, while mirrors and real sunlight still do.
static void NoteSunSwitchHit(void* actor, bool* result) {
    ObjLightswitch* sunSwitch = (ObjLightswitch*)actor;
    SunSwitch* slot = FindSunSwitch((Actor*)actor);
    Actor* hitter = sunSwitch->collider.base.ac;

    if (slot == NULL || !(sunSwitch->collider.base.acFlags & AC_HIT) || hitter == NULL) {
        return;
    }
    if (hitter->id == ACTOR_EN_ARROW && !IsElementalLightShot(hitter) && !AreVanillaLightArrowsSunlit()) {
        sunSwitch->collider.base.acFlags &= ~AC_HIT;
        return;
    }
    slot->isLitByArrow = hitter->id == ACTOR_EN_ARROW;
}

static void KeepArrowLitSwitchOn(bool* should, va_list args) {
    ObjLightswitch* sunSwitch = va_arg(args, ObjLightswitch*);
    SunSwitch* slot = FindSunSwitch(&sunSwitch->actor);

    if (slot != NULL && slot->isLitByArrow) {
        *should = false;
    }
}

// An arrow only lends the sun for the visit: leaving the room puts the switch out again.
static void ForgetSunSwitch(Actor* actor) {
    SunSwitch* slot = FindSunSwitch(actor);
    s32 type = (actor->params >> 4) & 3;

    if (slot == NULL) {
        return;
    }
    if (slot->isLitByArrow && type != LIGHTSWITCH_TYPE_REGULAR && actor->room != SUN_SWITCH_STAYS_LIT_ROOM) {
        Flags_UnsetSwitch(gPlayState, (actor->params >> 8) & 0x3F);
    }
    slot->actor = NULL;
}

static void ForgetArrowScene(void) {
    memset(sTrackedArrows, 0, sizeof(sTrackedArrows));
    memset(sLifesteals, 0, sizeof(sLifesteals));
    memset(sSunSwitches, 0, sizeof(sSunSwitches));
    sLifestealCount = 0;
}

#define ARROW_WHEEL_OWNER "nei.arrow_wheel"
static bool sArrowWheelOpen;
static u32 sArrowWheelClosedFrame=(u32)-1;
static bool sArrowWheelIsSling;
static uint8_t sArrowChoices[8], sArrowChoiceCount, sArrowHighlight;
void Player_UseItem(PlayState* play, Player* player, s32 item);
static bool BombAvailable(void) {
    bool (*available)(void) = (bool (*)(void))gSw97Api->FindService("nei.bomb_arrows.available");
    return available != NULL && available();
}
static bool BombSelected(void) {
    bool (*selected)(void)=(bool(*)(void))gSw97Api->FindService("nei.bomb_arrows.selected");
    return selected && selected();
}
static void BombSelect(bool selected) {
    void (*select)(bool) = (void (*)(bool))gSw97Api->FindService("nei.bomb_arrows.select");
    if (select != NULL) { select(selected); }
}
static bool ArrowWheelCapturesInput(void) {
    return sArrowWheelOpen || (gPlayState && sArrowWheelClosedFrame==gPlayState->state.frames);
}
static void HoldLoadedShot(Input* input) {
    if(!ArrowWheelCapturesInput() || !gPlayState)return;
    static const u16 buttons[S2H_ITEM_BUTTON_COUNT]={BTN_B,BTN_CLEFT,BTN_CDOWN,BTN_CRIGHT,0,BTN_DRIGHT,BTN_DLEFT,BTN_DDOWN,BTN_DUP};
    Player* player=GET_PLAYER(gPlayState);
    u16 held=(u32)player->heldItemButton<S2H_ITEM_BUTTON_COUNT?buttons[player->heldItemButton]:0;
    input->cur.button=held;input->prev.button=held;
    input->press.button=input->rel.button=0;
    input->cur.stick_x=input->cur.stick_y=input->rel.stick_x=input->rel.stick_y=0;
}
static void CloseArrowWheel(bool commit) {
    if(sArrowWheelOpen && gPlayState)sArrowWheelClosedFrame=gPlayState->state.frames;
    if (sArrowWheelOpen && commit && gPlayState != NULL && sArrowChoiceCount != 0) {
        Player* player = GET_PLAYER(gPlayState);
        uint8_t chosen = sArrowChoices[sArrowHighlight];
        Sw97_SetElement(sArrowWheelIsSling, chosen == 7 ? SW97_ELEM_NONE : (Sw97Element)chosen);
        // Retune the loaded projectile without replacing the custom slingshot.
        BombSelect(false);
        if(!sArrowWheelIsSling){
            // Native Fire/Ice/Light Arrow buttons must return to the bow as the
            // weapon identity, or their old item action would override this choice.
            u8 button=player->heldItemButton;
            if(button<EQUIP_SLOT_MAX){
                gSw97Api->UnequipCustomItem(button);
                SET_CUR_FORM_BTN_ITEM(button,ITEM_BOW);
                SET_CUR_FORM_BTN_SLOT(button,SLOT_BOW);
                Interface_LoadItemIconImpl(gPlayState,button);
            }else if(button>=S2H_ITEM_BUTTON_DPAD_FIRST && button<S2H_ITEM_BUTTON_COUNT){
                gSw97Api->UnequipCustomItem(button);
                DPAD_SET_CUR_FORM_BTN_ITEM(button-S2H_ITEM_BUTTON_DPAD_FIRST,ITEM_BOW);
                DPAD_SET_CUR_FORM_BTN_SLOT(button-S2H_ITEM_BUTTON_DPAD_FIRST,SLOT_BOW);
                Interface_Dpad_LoadItemIconImpl(gPlayState,button-S2H_ITEM_BUTTON_DPAD_FIRST);
            }
            Player_UseItem(gPlayState,player,ITEM_BOW);
        }
        Actor* held=player->heldActor;
        if(held && held->id==ACTOR_EN_ARROW){
            if(held->child){Actor_Kill(held->child);held->child=NULL;}
            TrackedArrow* tracked=FindTrackedArrow(held);
            if(tracked)tracked->arrow=NULL;
            Sw97Element element=Sw97_GetElement(sArrowWheelIsSling);
            if(!sArrowWheelIsSling)held->params=element>0&&element<=SW97_ELEM_LIGHT?ARROW_TYPE_FIRE+element-1:ARROW_TYPE_NORMAL;
            ((EnArrow*)held)->collider.elem.atDmgInfo.dmgFlags=element?sElementDamage[element]:(sArrowWheelIsSling?DMG_DEKU_NUT:DMG_NORMAL_ARROW);
            if(element)TrackNewArrow(held);
        }
        BombSelect(chosen == 7);
        Z64Wheel_PlaySfx(NA_SE_SY_DECIDE);
    }
    gSw97Api->ReleasePlayerInput(ARROW_WHEEL_OWNER);
    gSw97Api->ReleaseTimeControl(ARROW_WHEEL_OWNER);
    sArrowWheelOpen = false;
}
static void TickArrowWheel(void) {
    PlayState* play = gPlayState;
    if (play == NULL || play->pauseCtx.state != 0 || play->msgCtx.msgMode != MSGMODE_NONE) { CloseArrowWheel(false); return; }
    Player* player = GET_PLAYER(play);
    const char* heldKey=CustomItemRegistry_GetHeldKey();
    bool isSling = heldKey && !strcmp(heldKey,"oot.slingshot");
    bool aiming = (player->stateFlags1 & PLAYER_STATE1_100000) &&
        (isSling || (player->heldItemAction >= PLAYER_IA_BOW && player->heldItemAction <= PLAYER_IA_BOW_LIGHT));
    Input* input = &play->state.input[0];
    if (!aiming || (sArrowWheelOpen && isSling != sArrowWheelIsSling)) { CloseArrowWheel(false); return; }
    if (!sArrowWheelOpen && (input->press.button & BTN_L)) {
        sArrowWheelIsSling = isSling;
        sArrowChoiceCount = 0; sArrowHighlight = 0;
        for (uint8_t element = 0; element < SW97_ELEM_COUNT; ++element) {
            if (IsElementOwned((Sw97Element)element)) {
                if (element == Sw97_GetElement(sArrowWheelIsSling)) { sArrowHighlight = sArrowChoiceCount; }
                sArrowChoices[sArrowChoiceCount++] = element;
            }
        }
        if (BombAvailable()) {
            if(BombSelected())sArrowHighlight=sArrowChoiceCount;
            sArrowChoices[sArrowChoiceCount++]=7;
        }
        sArrowWheelOpen = true;
        gSw97Api->BlockPlayerInput(ARROW_WHEEL_OWNER, Z64WHEEL_BLOCKED_BUTTONS, true);
        gSw97Api->RequestTimeControl(ARROW_WHEEL_OWNER, 1000, 0.0f, true);
        Z64Wheel_PlaySfx(NA_SE_SY_WIN_OPEN);
    }
    if (!sArrowWheelOpen) { return; }
    if (input->press.button & BTN_B) { CloseArrowWheel(false); return; }
    float x = input->cur.stick_x, y = input->cur.stick_y;
    if (x*x+y*y > 400.0f) {
        int slot = (int)lroundf(atan2f(x,y) * sArrowChoiceCount / 6.28318531f) % sArrowChoiceCount;
        if (slot < 0) { slot += sArrowChoiceCount; }
        if (slot != sArrowHighlight) { sArrowHighlight = slot; Z64Wheel_PlaySfx(NA_SE_SY_CURSOR); }
    }
    if (!(input->cur.button & BTN_L) || (input->press.button & BTN_A)) { CloseArrowWheel(true); }
}
static void DrawArrowWheel(PlayState* play) {
    if (!sArrowWheelOpen) { return; }
    static const ALIGN_ASSET(2) char icons[8][80] = {
        "__OTR__icon_item_static_yar/gItemIconBowTex", "__OTR__icon_item_static_yar/gItemIconFireArrowTex",
        "__OTR__icon_item_static_yar/gItemIconIceArrowTex", "__OTR__icon_item_static_yar/gItemIconLightArrowTex",
        "__OTR__textures/icon_item_custom/gItemIconMedallionShadowTex", "__OTR__textures/icon_item_custom/gItemIconMedallionSpiritTex",
        "__OTR__textures/icon_item_custom/gItemIconMedallionForestTex", "__OTR__textures/icon_item_custom/gItemIconBombArrowsTex"
    };
    Z64Wheel_DimScreen(play, 110);
    for (uint8_t i = 0; i < sArrowChoiceCount; ++i) {
        uint8_t element = sArrowChoices[i];
        static const ALIGN_ASSET(2) char slingIcon[] = "__OTR__textures/oot_icon_item_static/gItemIconSlingshotTex";
        const char* icon = element == 0 && sArrowWheelIsSling ? slingIcon : icons[element];
        // Medallion icons take priority; vanilla arrows still unlock their entry when no medallion is owned.
        if (element > 0 && element < SW97_ELEM_COUNT && gSw97Api->IsCustomItemOwned(sElementKeys[element])) {
            static const ALIGN_ASSET(2) char medallionIcons[7][80] = {
                "", "__OTR__textures/icon_item_custom/gItemIconMedallionFireTex",
                "__OTR__textures/icon_item_custom/gItemIconMedallionWaterTex",
                "__OTR__textures/icon_item_custom/gItemIconMedallionLightTex",
                "__OTR__textures/icon_item_custom/gItemIconMedallionShadowTex",
                "__OTR__textures/icon_item_custom/gItemIconMedallionSpiritTex",
                "__OTR__textures/icon_item_custom/gItemIconMedallionForestTex"
            };
            icon = medallionIcons[element];
        }
        float angle = 6.28318531f * i / sArrowChoiceCount;
        int size = i == sArrowHighlight ? 32 : 24;
        Z64Wheel_DrawIcon(play, icon, (int16_t)(SCREEN_WIDTH/2 + sinf(angle)*52-size/2),
            (int16_t)(SCREEN_HEIGHT/2 - cosf(angle)*52-size/2), size, 255, i==sArrowHighlight?255:170);
    }
}
static void ForgetArrowWheel(void) { CloseArrowWheel(false);sArrowWheelClosedFrame=(u32)-1; }

void Sw97_RegisterArrows(void) {
    gSw97Api->PublishService("nei.arrow_wheel.captures_input",(void*)ArrowWheelCapturesInput);
    S2H_REGISTER_HOOK(gSw97Api, OnPassPlayerInputs, HoldLoadedShot);
    S2H_REGISTER_HOOK(gSw97Api, OnGameFrameUpdate, TickArrowWheel);
    S2H_REGISTER_HOOK(gSw97Api, OnInterfaceDrawEnd, DrawArrowWheel);
    S2H_REGISTER_HOOK(gSw97Api, OnPlayDestroy, ForgetArrowWheel);

    gSw97Api->RegisterVB(VB_PLAYER_ARROW_MAGIC_CONSUMPTION, WaiveElementalShotCost);
    gSw97Api->RegisterVB(VB_LIGHTSWITCH_OFF, KeepArrowLitSwitchOn);
    S2H_REGISTER_HOOK(gSw97Api, OnLoadFile, LoadElements);
    S2H_REGISTER_HOOK(gSw97Api, OnPlayDestroy, ForgetArrowScene);
    S2H_REGISTER_HOOK(gSw97Api, OnPlayerResolveItemAction, ResolveBowItemAction);
    S2H_REGISTER_HOOK(gSw97Api, OnActorUpdate, TickLifesteal);
    S2H_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorInit, ACTOR_EN_ARROW, TrackNewArrow);
    S2H_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorUpdate, ACTOR_EN_ARROW, UpdateTrackedArrow);
    S2H_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorDestroy, ACTOR_EN_ARROW, ForgetTrackedArrow);
    S2H_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorInit, ACTOR_OBJ_LIGHTSWITCH, OpenSunSwitchToLight);
    S2H_REGISTER_HOOK_FOR_ID(gSw97Api, ShouldActorUpdate, ACTOR_OBJ_LIGHTSWITCH, NoteSunSwitchHit);
    S2H_REGISTER_HOOK_FOR_ID(gSw97Api, OnActorDestroy, ACTOR_OBJ_LIGHTSWITCH, ForgetSunSwitch);
}
