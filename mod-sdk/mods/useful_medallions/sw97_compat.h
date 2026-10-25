// Bridges the SW97 decomp's 1997 names onto Majora's Mask's.
// Original actors: z64proto/sw97 team (Spaceworld '97 Experience).

#ifndef SW97_COMPAT_H
#define SW97_COMPAT_H

#include <math.h>

#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "gfx.h"

typedef PlayState GlobalContext;

#define PLAYER GET_PLAYER(Effect_GetPlayState())

#define MATRIX_TO_MTX(dest, file, line) Matrix_ToMtx(dest, file, line)

#define func_800D20CC Matrix_MtxFToYXZRotS
#define func_80093D84 Gfx_SetupDL25_Opa
#define func_80093D18 Gfx_SetupDL25_Opa
#define func_8010BDBC Message_GetState
#define func_80106BC8 Message_ShouldAdvance
#define func_8010B720 Message_ContinueTextbox
#define func_800876C8 Magic_Reset
#define func_800937C0 Gfx_SetupDL57
#define func_800773A8 Environment_AdjustLights
#define func_8002F7DC(actor, sfxId) Player_PlaySfx((Player*)(actor), sfxId)
#define func_8004356C DynaPolyActor_IsPlayerOnTop
#define Effect_GetGlobalCtx Effect_GetPlayState
#define Matrix_RotateRPY Matrix_RotateZYX
#define Gfx_CallSetupDL Gfx_SetupDL
#define Audio_PlaySoundAtPosition SoundSource_PlaySfxAtFixedWorldPos

// MM renamed these; Actor_TalkOfferAccepted takes the GameState, which PlayState begins with.
#define Actor_IsTalking(actor, play) Actor_TalkOfferAccepted((actor), &(play)->state)
#define Actor_RequestToTalkInRange(actor, play, range) Actor_OfferTalk(actor, play, range)
#define Actor_MoveForwardXZ Actor_MoveWithGravity

// The names the SW97 actors were written against; MM spells them differently.
#define Audio_PlayActorSound2(actor, sfxId) Actor_PlaySfx(actor, sfxId)
#define Actor_PlaySfx_Flagged(actor, sfxId) Actor_PlaySfx_Flagged2(actor, sfxId)
#define Gfx_SetupDL_25Xlu(gfxCtx) Gfx_SetupDL25_Xlu(gfxCtx)
#define Gfx_SetupDL_25Opa(gfxCtx) Gfx_SetupDL25_Opa(gfxCtx)
#define Matrix_NewMtx(gfxCtx, file, line) Matrix_Finalize(gfxCtx)
#define Matrix_RotateX(angle, mode) Matrix_RotateXF(angle, mode)
#define Matrix_RotateY(angle, mode) Matrix_RotateYF(angle, mode)
#define Matrix_RotateZ(angle, mode) Matrix_RotateZF(angle, mode)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// OoT's generic soft-sprite spawn is MM's EffectSsDust_Spawn, which keeps the same arguments.
#define func_8002836C(play, pos, vel, accel, prim, env, scale, step, life) \
    EffectSsDust_Spawn((play), 0, (pos), (vel), (accel), (prim), (env), (scale), (step), (life), 0)

// OoT enemies the suction blacklist names that MM lacks: ids that never match.
#ifndef ACTOR_EN_DH
#define ACTOR_EN_DH 0x7F61
#endif
#ifndef ACTOR_EN_DHA
#define ACTOR_EN_DHA 0x7F62
#endif
#ifndef ACTOR_EN_ANUBICE
#define ACTOR_EN_ANUBICE 0x7F63
#endif
#ifndef ACTOR_EN_GELDB
#define ACTOR_EN_GELDB 0x7F64
#endif

// The decomp's debug prints need a log level the SDK does not expose.
#undef LOG_STRING
#define LOG_STRING(string) ((void)0)
#undef osSyncPrintf
#define osSyncPrintf(...) ((void)0)

// MM has no f32 Matrix_RotateY/X/Z; the float variants end in F.
#define SW97_Matrix_RotateY_s(binang, mode) Matrix_RotateYF(BINANG_TO_RAD(binang), mode)
#define SW97_Matrix_RotateX_s(binang, mode) Matrix_RotateXF(BINANG_TO_RAD(binang), mode)
#define SW97_Matrix_RotateZ_s(binang, mode) Matrix_RotateZF(BINANG_TO_RAD(binang), mode)
#define SW97_Matrix_RotateY_f(degf, mode) Matrix_RotateYF(DEG_TO_RAD(degf), mode)
#define SW97_Matrix_RotateX_f(degf, mode) Matrix_RotateXF(DEG_TO_RAD(degf), mode)
#define SW97_Matrix_RotateZ_f(degf, mode) Matrix_RotateZF(DEG_TO_RAD(degf), mode)

#ifndef BINANG_TO_RADF
#define BINANG_TO_RADF(binang) ((f32)(binang) * (M_PI / 32768.0f))
#endif

MtxF* Matrix_GetCurrent(void);

// SW97 helper with no MM equivalent: projects along the current matrix's Z axis.
static inline void Matrix_MultZ(f32 scale, Vec3f* dst) {
    MtxF* current = Matrix_GetCurrent();

    dst->x = current->wx + current->zx * scale;
    dst->y = current->wy + current->zy * scale;
    dst->z = current->wz + current->zz * scale;
}

// OoT Bg_Ice_Shelter has no MM actor and SW97 never spawns one: a sentinel id that never matches and a no-op melt.
#ifndef ACTOR_BG_ICE_SHELTER
#define ACTOR_BG_ICE_SHELTER 0x7F60
#endif

// OoT boss and event sounds with no MM sibling stay silent.
#ifndef NA_SE_EN_GANON_DARKWAVE_M
#define NA_SE_EN_GANON_DARKWAVE_M NA_SE_NONE
#endif
#ifndef NA_SE_EN_TWINROBA_MASIC_SET
#define NA_SE_EN_TWINROBA_MASIC_SET NA_SE_NONE
#endif
#ifndef NA_SE_EV_ELEVATOR_STOP
#define NA_SE_EV_ELEVATOR_STOP NA_SE_NONE
#endif
#ifndef NA_SE_EV_FANTOM_WARP_L
#define NA_SE_EV_FANTOM_WARP_L NA_SE_NONE
#endif
#ifndef NA_SE_EV_FANTOM_WARP_S
#define NA_SE_EV_FANTOM_WARP_S NA_SE_NONE
#endif
#ifndef NA_SE_EV_NABALL_VANISH
#define NA_SE_EV_NABALL_VANISH NA_SE_NONE
#endif
#ifndef NA_SE_EV_SPIRIT_STONE
#define NA_SE_EV_SPIRIT_STONE NA_SE_NONE
#endif
#ifndef NA_SE_EV_TRIFORCE_FLASH
#define NA_SE_EV_TRIFORCE_FLASH NA_SE_NONE
#endif

// OoT's cull-zone fields are MM's culling-volume fields.
#ifndef uncullZoneForward
#define uncullZoneForward cullingVolumeDistance
#endif
#ifndef uncullZoneScale
#define uncullZoneScale cullingVolumeScale
#endif
#ifndef uncullZoneDownward
#define uncullZoneDownward cullingVolumeDownward
#endif

// Nothing in MM sets the OoT "grabbed by enemy" state.
#ifndef PLAYER_STATE2_GRABBED_BY_ENEMY
#define PLAYER_STATE2_GRABBED_BY_ENEMY 0
#endif

#endif // SW97_COMPAT_H
