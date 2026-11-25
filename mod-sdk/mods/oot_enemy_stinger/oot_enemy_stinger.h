#ifndef OOT_ENEMY_STINGER_H
#define OOT_ENEMY_STINGER_H

#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define STINGER_KEY "oot.stinger"
#define STINGER_WATER_KEY "oot.stinger_water"
#define EI_OBJECT "objects/oot_object_ei/"
#define EI_ASSET(name) "__OTR__" EI_OBJECT name

#define ANIM(asset) ((AnimationHeader*)(asset))

#define EI_LIMB_ROOT 1
#define EI_LIMB_MAX 19

// MM reuses OoT's ids of these for other actors; these are MM stand-ins.
#define STINGER_SFX_FLY NA_SE_EN_BATTA_FLY
#define STINGER_SFX_DAMAGE NA_SE_EN_BATTA_DAMAGE
#define STINGER_SFX_DEAD NA_SE_EN_BATTA_DEAD
#define STINGER_SFX_ATTACK NA_SE_EN_BATTA_ATTACK
#define STINGER_SFX_CRY NA_SE_EN_PIRANHA_EXIST
#define STINGER_SFX_FLUTTER NA_SE_EN_PIRANHA_ATTACK
#define STINGER_SFX_SINK NA_SE_EN_DAIOCTA_SINK
#define STINGER_SFX_LAND NA_SE_EN_FALL_LAND
#define STINGER_SFX_FREEZE NA_SE_EN_COMMON_FREEZE

#define STINGER_EXPOSED_DMG_FLAGS (~(DMG_NORMAL_SHIELD | DMG_LIGHT_RAY))

typedef enum {
    STINGER_DMGEFF_NONE = 0x0,
    STINGER_DMGEFF_STUN = 0x1,
} StingerDamageEffect;

static const ALIGN_ASSET(2) char sSkel[] = EI_ASSET("gStingerSkel");
static const ALIGN_ASSET(2) char sIdleAnim[] = EI_ASSET("gStingerIdleAnim");
static const ALIGN_ASSET(2) char sHitAnim[] = EI_ASSET("gStingerHitAnim");
static const ALIGN_ASSET(2) char sPopOutAnim[] = EI_ASSET("gStingerPopOutAnim");

static CollisionCheckInfoInit sColChkInfoInit = { 2, 45, 15, 100 };

// MM multiplies the weapon's own damage, so vanilla's per-sword values collapse into the sword row.
static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, STINGER_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(0, STINGER_DMGEFF_STUN),
    /* Normal arrow   */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Goron punch    */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Sword          */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Goron pound    */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Fire arrow     */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Ice arrow      */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Light arrow    */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Goron spikes   */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Deku spin      */ DMG_ENTRY(0, STINGER_DMGEFF_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Normal shield  */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Spin attack    */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Sword beam     */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
    /* Normal Roll    */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, STINGER_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(1, STINGER_DMGEFF_NONE),
};

// OoT's D_80116280: the translucent render mode its fading body selects through segment 8.
static Gfx sXluRenderModeDL[] = {
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU |
                                            FORCE_BL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
    gsDPSetAlphaCompare(G_AC_THRESHOLD),
    gsSPEndDisplayList(),
};

// The host does not export gEmptyDL to mods.
static Gfx sEmptyDL[] = {
    gsSPEndDisplayList(),
};

void SetWaterStingerFunctions(S2HActorDefinition* definition);
void ReplaceWithLandStinger(Actor* waterStinger, PlayState* play);

#endif
