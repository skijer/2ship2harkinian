// The six medallions as spells: pressing one runs NEI's cast (the OoT magic-spell flow widened to six spells)
// and spawns its SW97 actor. Holding the Shadow Medallion trades three hearts for magic.

#include "z64items.h"

#include <string.h>
#include <stdio.h>

#include "sw97_mod.h"

#define SPELL_ANIM_SPEED 0.83f
#define SPELL_FAST_FARORES_HOLD_FRAMES 10
#define SHADOW_EXCHANGE_HOLD_FRAMES 20
#define SHADOW_EXCHANGE_HEART_COST (3 * 0x10)
#define SHADOW_EXCHANGE_MAGIC_GAIN 24
#define SPELL_ITEM_PAGE (S2H_ITEM_PAGE_QUEST_VANILLA + 1)
#define SPELL_ITEM_FIRST_SLOT 0
#define CAST_BLOCKING_STATES (PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000)

#define ANIMSFX_SHIFT_TYPE(type) ((type) << 11)
#define ANIMSFX_DATA(type, frame) ((ANIMSFX_SHIFT_TYPE(type) | ((frame)&0x7FF)))
#define ANIMSFX_TYPE_GENERAL 1
#define ANIMSFX_TYPE_VOICE 4
#define ANIMSFX_TYPE_RUNNING 6
#define ANIMSFX_TYPE_WALKING 8
#define GAMEPLAY_KEEP_ANIM(name) "__OTR__objects/gameplay_keep/" name

// Layout of z_player.c's private AnimSfxEntry.
typedef struct {
    u16 sfxId;
    s16 data;
} SpellAnimSfx;

typedef struct {
    const char* key;
    Sw97Spell spell;
    const char* iconPath;
    const char* namePath;
    const char* getItemText;
    const char* pauseText;
} MedallionInfo;

void Player_SetAction_PreserveItemAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_PlayAnimSfx(Player* player, SpellAnimSfx* entry);
s32 Player_DecelerateToZero(Player* player);
void func_8082DAD4(Player* player);
void func_80836988(Player* player, PlayState* play);
extern AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char* path);
uint8_t ResourceMgr_FileExists(const char* resName);

static const ALIGN_ASSET(2) char sTameAnim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_tame");
static const ALIGN_ASSET(2) char sKaze1Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_kaze1");
static const ALIGN_ASSET(2) char sKaze2Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_kaze2");
static const ALIGN_ASSET(2) char sKaze3Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_kaze3");
static const ALIGN_ASSET(2) char sHonoo1Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_honoo1");
static const ALIGN_ASSET(2) char sHonoo2Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_honoo2");
static const ALIGN_ASSET(2) char sHonoo3Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_honoo3");
static const ALIGN_ASSET(2) char sTamashii1Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_tamashii1");
static const ALIGN_ASSET(2) char sTamashii2Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_tamashii2");
static const ALIGN_ASSET(2) char sTamashii3Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_tamashii3");

static const ALIGN_ASSET(2) char sForestIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionForestTex";
static const ALIGN_ASSET(2) char sFireIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionFireTex";
static const ALIGN_ASSET(2) char sWaterIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionWaterTex";
static const ALIGN_ASSET(2) char sSpiritIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionSpiritTex";
static const ALIGN_ASSET(2) char sShadowIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionShadowTex";
static const ALIGN_ASSET(2) char sLightIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionLightTex";

static const ALIGN_ASSET(2) char sForestName[] = "__OTR__textures/item_name_static/gForestMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sFireName[] = "__OTR__textures/item_name_static/gFireMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sWaterName[] = "__OTR__textures/item_name_static/gWaterMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sSpiritName[] = "__OTR__textures/item_name_static/gSpiritMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sShadowName[] = "__OTR__textures/item_name_static/gShadowMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sLightName[] = "__OTR__textures/item_name_static/gLightMedallionItemNameENGTex";

// NEI's six-spell tables: 0..2 keep vanilla's Farore/Din/Nayru rows, 3..5 are the SW97 additions.
static const char* const sCastAnims[SW97_SPELL_COUNT] = {
    sKaze1Anim, sHonoo1Anim, sTamashii1Anim, sKaze1Anim, sTamashii1Anim, sHonoo1Anim,
};
static const char* const sHoldAnims[SW97_SPELL_COUNT] = {
    sKaze2Anim, sHonoo2Anim, sTamashii2Anim, sKaze2Anim, sTamashii2Anim, sHonoo2Anim,
};
static const char* const sReleaseAnims[SW97_SPELL_COUNT] = {
    sKaze3Anim, sHonoo3Anim, sTamashii3Anim, sKaze3Anim, sTamashii3Anim, sHonoo3Anim,
};
static const u8 sHoldFrames[SW97_SPELL_COUNT] = { 70, 10, 10, 70, 10, 10 };
static const u8 sSpellCosts[SW97_SPELL_COUNT] = { 12, 24, 24, 12, 24, 12 };

static SpellAnimSfx sCastSfx[] = {
    { NA_SE_PL_SKIP, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 20) },
    { NA_SE_VO_LI_SWORD_N, ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 20) },
    { NA_SE_PL_WALK_GROUND, -ANIMSFX_DATA(ANIMSFX_TYPE_RUNNING, 26) },
};

static SpellAnimSfx sSpellSfx[SW97_SPELL_COUNT][2] = {
    {
        { NA_SE_PL_WALK_GROUND, ANIMSFX_DATA(ANIMSFX_TYPE_WALKING, 20) },
        { NA_SE_VO_LI_MAGIC_FROL, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 30) },
    },
    {
        { NA_SE_PL_WALK_GROUND, ANIMSFX_DATA(ANIMSFX_TYPE_WALKING, 20) },
        { NA_SE_VO_LI_MAGIC_NALE, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 44) },
    },
    {
        { NA_SE_VO_LI_MAGIC_ATTACK, ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 20) },
        { NA_SE_IT_SWORD_SWING_HARD, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 20) },
    },
    {
        { NA_SE_PL_WALK_GROUND, ANIMSFX_DATA(ANIMSFX_TYPE_WALKING, 20) },
        { NA_SE_VO_LI_MAGIC_FROL, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 30) },
    },
    {
        { NA_SE_VO_LI_MAGIC_ATTACK, ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 20) },
        { NA_SE_IT_SWORD_SWING_HARD, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 20) },
    },
    {
        { NA_SE_PL_WALK_GROUND, ANIMSFX_DATA(ANIMSFX_TYPE_WALKING, 20) },
        { NA_SE_VO_LI_MAGIC_NALE, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 44) },
    },
};

static const MedallionInfo sMedallions[SW97_SPELL_COUNT] = {
    { "nei.medallion.forest", SW97_SPELL_WIND, sForestIcon, sForestName,
      "You got the %gForest Medallion%w!&A tornado that drags enemies in&and grinds them.",
      "%gForest Medallion&%wWind spell, 12 MP. A tornado that&drags enemies in and grinds them." },
    { "nei.medallion.fire", SW97_SPELL_FIRE, sFireIcon, sFireName,
      "You got the %rFire Medallion%w!&A column of flame that burns&harder the longer it stands.",
      "%rFire Medallion&%wFire spell, 12 MP. A column of flame&that burns harder the longer it stands." },
    { "nei.medallion.water", SW97_SPELL_ICE, sWaterIcon, sWaterName,
      "You got the %cWater Medallion%w!&Freezes every enemy it touches&for 6 seconds.",
      "%cWater Medallion&%wIce spell, 12 MP. Freezes every enemy&it touches for 6 seconds." },
    { "nei.medallion.spirit", SW97_SPELL_SOUL, sSpiritIcon, sSpiritName,
      "You got the %ySpirit Medallion%w!&Turns you into a fairy until&you cast it again.",
      "%ySpirit Medallion&%wSoul spell, 24 MP. Turns you into a&fairy until you cast it again." },
    { "nei.medallion.shadow", SW97_SPELL_DARK, sShadowIcon, sShadowName,
      "You got the %pShadow Medallion%w!&A shield that blocks all damage&for a minute.",
      "%pShadow Medallion&%wDark spell, 24 MP. A shield that blocks&all damage for a minute. Hold the C&button to trade 3 hearts for magic." },
    { "nei.medallion.light", SW97_SPELL_LIGHT, sLightIcon, sLightName,
      "You got the %yLight Medallion%w!&Undead freeze and you heal&6 hearts.",
      "%yLight Medallion&%wLight spell, 24 MP. Undead freeze for&30 seconds and you heal 6 hearts." },
};

static s8 sPendingSpell = -1;
static s16 sShadowHoldFrames;
static bool sIsShadowExchanged;

static void CastAction(Player* player, PlayState* play);

static const MedallionInfo* FindMedallion(Sw97Spell spell) {
    for (uint8_t i = 0; i < SW97_SPELL_COUNT; i++) {
        if (sMedallions[i].spell == spell) {
            return &sMedallions[i];
        }
    }
    return NULL;
}

// Water rides Farore's item action in NEI, so Fast Farore's Wind speeds it up too.
static bool IsFastFarores(s8 spell) {
    return spell == SW97_SPELL_ICE && CVarGetInteger("gEnhancements.FastFarores", 0);
}

// The OoT cast clips live in the companion archive; without it the cast plays the idle pose instead of crashing.
static PlayerAnimationHeader* LoadCastAnim(const char* path) {
    PlayerAnimationHeader* header = NULL;
    char loaderPath[160];
    const char* prefix = "__OTR__objects/gameplay_keep/";
    if (strncmp(path, prefix, strlen(prefix)) == 0) {
        snprintf(loaderPath, sizeof(loaderPath), "__OTR__objects/oot_gameplay_keep/%s", path + strlen(prefix));
        if (gSw97Api->HasResource(loaderPath)) path = loaderPath;
    }
    if (gSw97Api->HasResource(path)) {
        header = (PlayerAnimationHeader*)ResourceMgr_LoadAnimByName(path);
    }
    if (header == NULL || header->common.frameCount <= 0) {
        return (PlayerAnimationHeader*)gPlayerAnim_link_normal_wait;
    }
    return header;
}

static Actor* SpawnSpell(PlayState* play, Player* player, Sw97Spell spell) {
    return Actor_Spawn(&play->actorCtx, play, Sw97_GetSpellActorId(spell), player->actor.world.pos.x,
                       player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, 0);
}

static void PlayCastAnim(PlayState* play, Player* player, const char* animation, bool isFast) {
    PlayerAnimation_PlayOnceSetSpeed(play, &player->skelAnime, LoadCastAnim(animation),
                                     SPELL_ANIM_SPEED * (isFast ? 2 : 1));
}

static void StartCast(PlayState* play, Player* player, Sw97Spell spell) {
    bool isFast = IsFastFarores(spell);

    Player_SetAction_PreserveItemAction(play, player, CastAction, 0);
    player->av1.actionVar1 = spell;
    player->av2.actionVar2 = 0;
    Magic_Consume(play, sSpellCosts[spell], MAGIC_CONSUME_WAIT_PREVIEW);
    PlayCastAnim(play, player, sTameAnim, isFast);
    func_8082DAD4(player);
}

static void LeaveCast(Player* player, PlayState* play) {
    gSw97Api->ReleaseTimeControl("medallion.cast");
    func_80836988(player, play);
    Camera_SetFinishedFlag(Play_GetCamera(play, CAM_ID_MAIN));
}

static void AdvanceCast(Player* player, PlayState* play, Sw97Spell spell, bool isFast) {
    if (player->av2.actionVar2 == 0) {
        PlayCastAnim(play, player, sCastAnims[spell], isFast);
        if (SpawnSpell(play, player, spell) != NULL) {
            player->stateFlags1 |= CAST_BLOCKING_STATES;
            gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP;
        } else {
            Magic_Reset(play);
        }
    } else {
        PlayerAnimation_PlayLoopSetSpeed(play, &player->skelAnime, LoadCastAnim(sHoldAnims[spell]),
                                         SPELL_ANIM_SPEED * (isFast ? 2 : 1));
    }
    player->av2.actionVar2++;
}

static void PlayCastSounds(Player* player, PlayState* play, Sw97Spell spell, bool isFast) {
    if (player->av2.actionVar2 == 0) {
        Player_PlayAnimSfx(player, sCastSfx);
        return;
    }
    if (player->av2.actionVar2 == 1) {
        Player_PlayAnimSfx(player, sSpellSfx[spell]);
        if (PlayerAnimation_OnFrame(&player->skelAnime, 30.0f)) {
            player->stateFlags1 &= ~CAST_BLOCKING_STATES;
        }
        return;
    }
    if ((isFast ? SPELL_FAST_FARORES_HOLD_FRAMES : sHoldFrames[spell]) < player->av2.actionVar2++) {
        PlayCastAnim(play, player, sReleaseAnims[spell], isFast);
        player->yaw = player->actor.shape.rot.y;
        player->av1.actionVar1 = -1;
    }
}

// The OoT spell action as NEI runs it for a medallion: every spell fires and forgets, none saves a warp.
static void CastAction(Player* player, PlayState* play) {
    s8 spell = (s8)player->av1.actionVar1;
    bool isFast = IsFastFarores(spell);
    player->invincibilityTimer = MAX(player->invincibilityTimer, 2);

    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        if (spell < 0) {
            LeaveCast(player, play);
        } else {
            AdvanceCast(player, play, (Sw97Spell)spell, isFast);
        }
    } else if (spell >= 0) {
        PlayCastSounds(player, play, (Sw97Spell)spell, isFast);
    }
    Player_DecelerateToZero(player);
}

static bool CanAffordSpell(Sw97Spell spell) {
    return gSaveContext.magicCapacity != 0 && gSaveContext.magicState == MAGIC_STATE_IDLE &&
           gSaveContext.save.saveInfo.playerData.magic >= sSpellCosts[spell];
}

// The press lands inside vanilla's item handling, which would replace the action func the same frame: the
// cast starts at the end of the player's update instead.
static void PressMedallion(Sw97Spell spell) {
    if (!CanAffordSpell(spell)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    sPendingSpell = spell;
}

static void PressForest(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_WIND);
}

static void PressSpirit(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_SOUL);
}

static void PressShadow(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_DARK);
}

static void PressWater(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_ICE);
}

static void PressLight(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_LIGHT);
}

static void PressFire(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_FIRE);
}

// Vanilla only casts from the ground or the water surface.
static bool CanStartCast(Player* player, PlayState* play) {
    bool isGrounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    bool isSwimming = (player->stateFlags1 & PLAYER_STATE1_8000000) != 0;

    return (isGrounded || isSwimming) && player->actionFunc != CastAction && !Player_InBlockingCsMode(play, player);
}

static void StartPendingCast(Player* player, PlayState* play) {
    s8 spell = sPendingSpell;

    if (spell < 0) {
        return;
    }
    sPendingSpell = -1;
    if (CanStartCast(player, play)) {
        StartCast(play, player, (Sw97Spell)spell);
    }
}

static uint32_t FindShadowButtons(void) {
    static const u32 sCButtons[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };
    const char* shadowKey = FindMedallion(SW97_SPELL_DARK)->key;
    uint32_t buttons = 0;

    for (uint8_t button = S2H_ITEM_BUTTON_C_FIRST; button <= S2H_ITEM_BUTTON_C_LAST; button++) {
        const char* equipped = gSw97Api->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, shadowKey) == 0) {
            buttons |= sCButtons[button - S2H_ITEM_BUTTON_C_FIRST];
        }
    }
    return buttons;
}

static void ResetShadowExchange(void) {
    sShadowHoldFrames = 0;
    sIsShadowExchanged = false;
}

// Outside the cast on purpose: the trade is what a player with an empty meter reaches for.
static void TickShadowExchange(Player* player, PlayState* play) {
    uint32_t buttons = FindShadowButtons();

    if (buttons == 0 || !(play->state.input[0].cur.button & buttons)) {
        ResetShadowExchange();
        return;
    }
    sShadowHoldFrames++;
    if (sIsShadowExchanged || sShadowHoldFrames < SHADOW_EXCHANGE_HOLD_FRAMES ||
        gSaveContext.save.saveInfo.playerData.health <= SHADOW_EXCHANGE_HEART_COST) {
        return;
    }
    gSaveContext.save.saveInfo.playerData.health -= SHADOW_EXCHANGE_HEART_COST;
    gSaveContext.save.saveInfo.playerData.magic =
        MIN(gSaveContext.save.saveInfo.playerData.magic + SHADOW_EXCHANGE_MAGIC_GAIN, gSaveContext.magicCapacity);
    Actor_PlaySfx(&player->actor, NA_SE_SY_GET_RUPY);
    sIsShadowExchanged = true;
}

static void TickSpells(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player == NULL) {
        return;
    }
    StartPendingCast(player, play);
    if (player->actionFunc == CastAction) {
        gSw97Api->RequestTimeControl("medallion.cast", 90, 1.0f, true);
    } else {
        gSw97Api->ReleaseTimeControl("medallion.cast");
    }
    TickShadowExchange(player, play);
}

static void ForgetSpellState(s16 fileNum) {
    gSw97Api->ReleaseTimeControl("medallion.cast");
    sPendingSpell = -1;
    ResetShadowExchange();
}

static bool RegisterMedallion(const MedallionInfo* medallion, uint8_t index, S2HCustomItemInitFunc press) {
    S2HCustomItemDefinition item = Z64Items_Define(medallion->key, medallion->iconPath, medallion->namePath);

    static const char* const loaderIcons[] = {
        "__OTR__textures/oot_item_icons/gItemIconMedallionForestTex",
        "__OTR__textures/oot_item_icons/gItemIconMedallionFireTex",
        "__OTR__textures/oot_item_icons/gItemIconMedallionWaterTex",
        "__OTR__textures/oot_item_icons/gItemIconMedallionSpiritTex",
        "__OTR__textures/oot_item_icons/gItemIconMedallionShadowTex",
        "__OTR__textures/oot_item_icons/gItemIconMedallionLightTex",
    };
    if (gSw97Api->HasResource(loaderIcons[index])) item.iconPath = loaderIcons[index];
    static const char* const loaderNames[] = {
        "__OTR__textures/oot_item_name_static/gForestMedallionItemNameENGTex",
        "__OTR__textures/oot_item_name_static/gFireMedallionItemNameENGTex",
        "__OTR__textures/oot_item_name_static/gWaterMedallionItemNameENGTex",
        "__OTR__textures/oot_item_name_static/gSpiritMedallionItemNameENGTex",
        "__OTR__textures/oot_item_name_static/gShadowMedallionItemNameENGTex",
        "__OTR__textures/oot_item_name_static/gLightMedallionItemNameENGTex",
    };
    static const char* const loaderModels[] = {
        "__OTR__objects/oot_loader_models/gForestMedallionDL",
        "__OTR__objects/oot_loader_models/gFireMedallionDL",
        "__OTR__objects/oot_loader_models/gWaterMedallionDL",
        "__OTR__objects/oot_loader_models/gSpiritMedallionDL",
        "__OTR__objects/oot_loader_models/gShadowMedallionDL",
        "__OTR__objects/oot_loader_models/gLightMedallionDL",
    };
    if (gSw97Api->HasResource(loaderNames[index])) {
        item.namePath = loaderNames[index];
        Z64Items_SetGetItemModel(&item, loaderModels[index]);
    }
    Z64Items_SetButtons(&item, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(&item, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(&item, SPELL_ITEM_PAGE, SPELL_ITEM_FIRST_SLOT + index, 0);
    Z64Items_SetTextbox(&item, medallion->getItemText);
    Z64Items_SetPauseText(&item, medallion->pauseText);
    Z64Items_SetAction(&item, press, NULL);
    item.flags |= S2H_CUSTOM_ITEM_INSTANT;
    return Z64Items_Register(gSw97Api, &item);
}

static void PauseCastWorld(Actor* actor, bool* should) {
    if (gPlayState == NULL) return;
    Player* player = GET_PLAYER(gPlayState);
    if (player->actionFunc == CastAction && actor->id != ACTOR_PLAYER && actor->id < ACTOR_ID_MAX) *should = false;
}

bool Sw97_RegisterSpells(void) {
    static const S2HCustomItemInitFunc sPresses[SW97_SPELL_COUNT] = {
        PressForest, PressSpirit, PressShadow, PressWater, PressLight, PressFire,
    };

    for (uint8_t spell = 0; spell < SW97_SPELL_COUNT; spell++) {
        if (!RegisterMedallion(FindMedallion((Sw97Spell)spell), spell, sPresses[spell])) {
            return false;
        }
    }
    S2H_REGISTER_HOOK(gSw97Api, OnPlayerUpdate, TickSpells);
    S2H_REGISTER_HOOK(gSw97Api, ShouldActorUpdate, PauseCastWorld);
    S2H_REGISTER_HOOK(gSw97Api, OnSaveLoad, ForgetSpellState);
    return true;
}
