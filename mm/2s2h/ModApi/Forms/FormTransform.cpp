#include "FormTransform.h"

#include <string>

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
void func_808388B8(PlayState* play, Player* player, PlayerTransformation playerForm);
void Player_Action_86(Player* player, PlayState* play);
void Player_Action_87(Player* player, PlayState* play);
// OPEN_DISPS redeclares these inside its body, which would give them C++ linkage here.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

constexpr const char* InstantTransformCvar = "gEnhancements.Forms.InstantTransform";
constexpr float MaskFaceFrame = 12.0f;
constexpr float MaskClimaxFrame = 51.0f;
constexpr uint32_t StateLoading = PLAYER_STATE1_200;
constexpr uint32_t StateInputDisabled = PLAYER_STATE1_20;
constexpr uint32_t StateInCutscene = PLAYER_STATE1_20000000;
constexpr uint32_t StateInWater = PLAYER_STATE1_8000000;
constexpr uint32_t StateClimbingLedge = PLAYER_STATE1_4;
constexpr uint32_t StateHanging = PLAYER_STATE1_200000;
constexpr uint32_t StateGettingItem = PLAYER_STATE1_400;
constexpr uint32_t StateDamaged = PLAYER_STATE1_4000000;
constexpr uint32_t StateFirstPerson = PLAYER_STATE1_100000;
constexpr uint32_t BlockingStateFlags1 = PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_CARRYING_ACTOR |
                                         StateLoading | StateInputDisabled | StateInCutscene | StateInWater |
                                         StateClimbingLedge | StateHanging | StateGettingItem | StateDamaged |
                                         StateFirstPerson;

bool sActive = false;
bool sPendingStart = false;
bool sFormApplied = false;
std::string sTargetKey;
const char* sMaskDl = nullptr;
const char* sMaskClimaxDl = nullptr;
bool sMaskClimax = false;

Player* GetPlayer() {
    return gPlayState == nullptr ? nullptr : GET_PLAYER(gPlayState);
}

bool IsRunningMaskCutscene(const Player* player) {
    return player->actionFunc == Player_Action_86 || player->actionFunc == Player_Action_87;
}

// The cutscene re-inits the actor and swallows a worn mask, and it needs solid ground and free hands.
bool CanPlayMaskCutscene(const Player* player) {
    return player->transformation == PLAYER_FORM_HUMAN && player->currentMask == PLAYER_MASK_NONE &&
           player->csAction == PLAYER_CSACTION_NONE && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
           !(player->stateFlags1 & BlockingStateFlags1);
}

void Stop() {
    sActive = false;
    sPendingStart = false;
    sFormApplied = false;
    sMaskDl = nullptr;
    sMaskClimaxDl = nullptr;
    sMaskClimax = false;
}

void ApplyTargetForm() {
    sFormApplied = true;
    sMaskDl = nullptr;
    FormRegistry_ApplyActive(sTargetKey.empty() ? nullptr : sTargetKey.c_str());
}

void StartPendingTransform(Player* player) {
    sPendingStart = false;
    player->prevMask = PLAYER_MASK_NONE;
    func_808388B8(gPlayState, player, PLAYER_FORM_HUMAN);
}

// The vanilla action swaps the actor's update function at the flash peak, which is where the form must change.
void UpdateTransform(Player* player) {
    if (!sActive || player != GetPlayer()) {
        return;
    }
    if (sPendingStart) {
        StartPendingTransform(player);
        return;
    }
    const bool isRebuildingActor = player->actor.update == func_8012301C;
    if (!sFormApplied && isRebuildingActor) {
        ApplyTargetForm();
        return;
    }
    if (isRebuildingActor || IsRunningMaskCutscene(player)) {
        return;
    }
    if (!sFormApplied) {
        ApplyTargetForm();
    }
    Stop();
}

} // namespace

void FormTransform_DrawMask(Player* player, s32 limbIndex) {
    if (!sActive || sMaskDl == nullptr || limbIndex != PLAYER_LIMB_HEAD || !IsRunningMaskCutscene(player) ||
        player->skelAnime.curFrame < MaskFaceFrame) {
        return;
    }
    const bool isClimax = sMaskClimaxDl != nullptr && (sMaskClimax || player->skelAnime.curFrame >= MaskClimaxFrame);
    sMaskClimax = isClimax;
    const char* maskDl = isClimax ? sMaskClimaxDl : sMaskDl;

    OPEN_DISPS(gPlayState->state.gfxCtx);

    Matrix_Push();
    Matrix_Scale(1.0f, 1.0f - player->unk_B10[3], 1.0f - player->unk_B10[2], MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gPlayState->state.gfxCtx);
    Matrix_Pop();
    gSPDisplayList(POLY_OPA_DISP++, reinterpret_cast<Gfx*>(const_cast<char*>(maskDl)));

    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

void FormTransform_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerPostLimbDraw>(FormTransform_DrawMask);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(UpdateTransform);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s8, s8) { Stop(); });
}

bool FormTransform_IsActive(void) {
    return sActive;
}

bool FormTransform_Begin(const S2HFormDefinition* from, const S2HFormDefinition* to) {
    Player* player = GetPlayer();
    const bool isStarted = player != nullptr && !sActive && CVarGetInteger(InstantTransformCvar, 0) == 0 &&
                           CanPlayMaskCutscene(player);
    const S2HFormDefinition* wearer = from != nullptr ? from : to;
    if (!isStarted || wearer == nullptr) {
        return false;
    }

    sActive = true;
    sPendingStart = true;
    sFormApplied = false;
    sTargetKey = to == nullptr ? "" : to->key;
    sMaskDl = wearer->transformMask;
    sMaskClimaxDl = wearer->transformMaskClimax;
    sMaskClimax = false;
    return true;
}
