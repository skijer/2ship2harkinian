#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ObjectExtension/ObjectExtension.h"

extern "C" {
#include "variables.h"
#include "overlays/actors/ovl_En_Sw/z_en_sw.h"
}

struct ActorRandoCheckId {
    RandoCheckId randoCheckId = RC_UNKNOWN;
};
static ObjectExtension::Register<ActorRandoCheckId> ActorRandoCheckIdRegister;

bool Rando::ActorBehavior::ShouldMatchContainerStyle() {
    return GameInteractor_Should(VB_RANDO_MATCH_CONTAINER_STYLE, CVarGetInteger("gRando.CSMC", 0) != 0);
}

RandoItemType Rando::ActorBehavior::GetContainerItemType(RandoCheckId randoCheckId) {
    return RandoItems_GetContainerType(Rando::ConvertItem(RANDO_SAVE_CHECKS[randoCheckId].randoItemId, randoCheckId));
}

static S2HContainerStyle GetContainerStyle(RandoItemType randoItemType) {
    switch (randoItemType) {
        case RITYPE_JUNK:
            return S2H_CONTAINER_STYLE_JUNK;
        case RITYPE_LESSER:
            return S2H_CONTAINER_STYLE_LESSER;
        case RITYPE_MAJOR:
            return S2H_CONTAINER_STYLE_MAJOR;
        case RITYPE_HEALTH:
            return S2H_CONTAINER_STYLE_HEALTH;
        case RITYPE_SMALL_KEY:
            return S2H_CONTAINER_STYLE_SMALL_KEY;
        case RITYPE_BOSS_KEY:
            return S2H_CONTAINER_STYLE_BOSS_KEY;
        case RITYPE_SKULLTULA_TOKEN:
            return S2H_CONTAINER_STYLE_TOKEN;
        case RITYPE_MASK:
            return S2H_CONTAINER_STYLE_MASK;
        case RITYPE_STRAY_FAIRY:
            return S2H_CONTAINER_STYLE_STRAY_FAIRY;
        default:
            SPDLOG_WARN("[Rando] Item type {} has no container style", static_cast<int>(randoItemType));
            return S2H_CONTAINER_STYLE_JUNK;
    }
}

// A gold Skulltula carries its token's chest flag, which names the check before the token spawns.
static RandoCheckId GetActorRandoCheckId(Actor* actor) {
    if (actor->id == ACTOR_EN_SW && ENSW_GET_3(actor)) {
        return Rando::StaticData::GetCheckFromFlag(FLAG_CYCL_SCENE_CHEST, ENSW_GET_FC(actor), gPlayState->sceneId)
            .randoCheckId;
    }
    return Rando::ActorBehavior::GetObjectRandoCheckId(actor);
}

static uint8_t GetActorContainerStyle(Actor* actor) {
    if (!IS_RANDO || actor == nullptr || !Rando::ActorBehavior::ShouldMatchContainerStyle()) {
        return S2H_CONTAINER_STYLE_NONE;
    }
    const RandoCheckId randoCheckId = GetActorRandoCheckId(actor);
    if (randoCheckId == RC_UNKNOWN) {
        return S2H_CONTAINER_STYLE_NONE;
    }
    const RandoSaveCheck& saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
    if (!saveCheck.shuffled || saveCheck.cycleObtained) {
        return S2H_CONTAINER_STYLE_NONE;
    }
    return GetContainerStyle(Rando::ActorBehavior::GetContainerItemType(randoCheckId));
}

// This is kind of a catch-all for things that are simple enough to not need their own file.
void MiscVanillaBehaviorHandler(GIVanillaBehavior id, bool* should, va_list optionalArg) {
    switch (id) {
        case VB_GIVE_ITEM_FROM_ROMANI:
            *should = CHECK_QUEST_ITEM(QUEST_SONG_EPONA);
            break;
        // TODO: This should be configurable
        case VB_DOOR_HEALTH_CHECK_FAIL:
            *should = false;
            break;
        case VB_GIVE_PENDANT_OF_MEMORIES_FROM_KAFEI:
        case VB_MSG_SCRIPT_DEL_ITEM:
        case VB_GIVE_NEW_WAVE_BOSSA_NOVA:
        case VB_TOILET_HAND_TAKE_ITEM:
            *should = false;
            break;
    }
}

RandoCheckId Rando::ActorBehavior::GetObjectRandoCheckId(void* object) {
    const ActorRandoCheckId* actorRandoCheckId = ObjectExtension::GetInstance().Get<ActorRandoCheckId>(object);
    return actorRandoCheckId != nullptr ? actorRandoCheckId->randoCheckId : RC_UNKNOWN;
}

void Rando::ActorBehavior::SetObjectRandoCheckId(const void* object, RandoCheckId rc) {
    ObjectExtension::GetInstance().Set<ActorRandoCheckId>(object, ActorRandoCheckId{ rc });
}

// Entry point for the module, run once on game boot
void Rando::ActorBehavior::Init() {
    ModApi_PublishHostService("rando.container_style", reinterpret_cast<void*>(GetActorContainerStyle));
}

void Rando::ActorBehavior::OnFileLoad() {
    Rando::ActorBehavior::InitDmChar01Behavior();
    Rando::ActorBehavior::InitDmChar05Behavior();
    Rando::ActorBehavior::InitDmChar08Behavior();
    Rando::ActorBehavior::InitDmHinaBehavior();
    Rando::ActorBehavior::InitDmStkBehavior();
    Rando::ActorBehavior::InitDoorWarp1VBehavior();
    Rando::ActorBehavior::InitEnAkindonutsBehavior();
    Rando::ActorBehavior::InitEnAlBehavior();
    Rando::ActorBehavior::InitEnAnBehavior();
    Rando::ActorBehavior::InitEnAob01Behavior();
    Rando::ActorBehavior::InitEnAzBehavior();
    Rando::ActorBehavior::InitEnBabaBehavior();
    Rando::ActorBehavior::InitEnBalBehavior();
    Rando::ActorBehavior::InitEnBjtBehavior();
    Rando::ActorBehavior::InitEnBomBowlManBehavior();
    Rando::ActorBehavior::InitEnBoxBehavior();
    Rando::ActorBehavior::InitEnButteBehavior();
    Rando::ActorBehavior::InitEnCowBehavior();
    Rando::ActorBehavior::InitEnDaiBehavior();
    Rando::ActorBehavior::InitEnDnhBehavior();
    Rando::ActorBehavior::InitEnElfgrpBehavior();
    Rando::ActorBehavior::InitEnElforgBehavior();
    Rando::ActorBehavior::InitEnemyDropBehavior();
    Rando::ActorBehavior::InitEnFish2Behavior();
    Rando::ActorBehavior::InitEnFsnBehavior();
    Rando::ActorBehavior::InitEnFuBehavior();
    Rando::ActorBehavior::InitEnGamelupyBehavior();
    Rando::ActorBehavior::InitEnGb2Behavior();
    Rando::ActorBehavior::InitEnGegBehavior();
    Rando::ActorBehavior::InitEnGgBehavior();
    Rando::ActorBehavior::InitEnGinkoBehavior();
    Rando::ActorBehavior::InitEnGirlABehavior();
    Rando::ActorBehavior::InitEnGKBehavior();
    Rando::ActorBehavior::InitEnGoBehavior();
    Rando::ActorBehavior::InitEnGsBehavior();
    Rando::ActorBehavior::InitEnHgBehavior();
    Rando::ActorBehavior::InitEnInBehavior();
    Rando::ActorBehavior::InitEnInvadepohBehavior();
    Rando::ActorBehavior::InitEnItem00Behavior();
    Rando::ActorBehavior::InitEnJgameTsnBehavior();
    Rando::ActorBehavior::InitEnJgBehavior();
    Rando::ActorBehavior::InitEnJsBehavior();
    Rando::ActorBehavior::InitEnKgyBehavior();
    Rando::ActorBehavior::InitEnKitanBehavior();
    Rando::ActorBehavior::InitEnKnightBehavior();
    Rando::ActorBehavior::InitEnKujiyaBehavior();
    Rando::ActorBehavior::InitEnLiftNutsBehavior();
    Rando::ActorBehavior::InitEnMa4Behavior();
    Rando::ActorBehavior::InitEnMaYtoBehavior();
    Rando::ActorBehavior::InitEnMinifrogBehavior();
    Rando::ActorBehavior::InitEnMnkBehavior();
    Rando::ActorBehavior::InitEnNbBehavior();
    Rando::ActorBehavior::InitEnOsnBehavior();
    Rando::ActorBehavior::InitEnOtBehavior();
    Rando::ActorBehavior::InitEnOwlBehavior();
    Rando::ActorBehavior::InitEnPmBehavior();
    Rando::ActorBehavior::InitEnRuppecrowBehavior();
    Rando::ActorBehavior::InitEnRzBehavior();
    Rando::ActorBehavior::InitEnScopenutsBehavior();
    Rando::ActorBehavior::InitEnSellnutsBehavior();
    Rando::ActorBehavior::InitEnShnBehavior();
    Rando::ActorBehavior::InitEnSiBehavior();
    Rando::ActorBehavior::InitEnSob1Behavior();
    Rando::ActorBehavior::InitEnSshBehavior();
    Rando::ActorBehavior::InitEnStoneheishiBehavior();
    Rando::ActorBehavior::InitEnSyatekiManBehavior();
    Rando::ActorBehavior::InitEnTabBehavior();
    Rando::ActorBehavior::InitEnTakarayaBehavior();
    Rando::ActorBehavior::InitEnTalkBehavior();
    Rando::ActorBehavior::InitEnTimeTagBehavior();
    Rando::ActorBehavior::InitEnTotoBehavior();
    Rando::ActorBehavior::InitEnTrtBehavior();
    Rando::ActorBehavior::InitEnYbBehavior();
    Rando::ActorBehavior::InitEnZogBehavior();
    Rando::ActorBehavior::InitEnZotBehavior();
    Rando::ActorBehavior::InitEnZowBehavior();
    Rando::ActorBehavior::InitItemBHeartBehavior();
    Rando::ActorBehavior::InitItemGetBehavior();
    Rando::ActorBehavior::InitObjCombBehavior();
    Rando::ActorBehavior::InitObjKibakoBehavior();
    Rando::ActorBehavior::InitObjGrassBehavior();
    Rando::ActorBehavior::InitObjMoonStoneBehavior();
    Rando::ActorBehavior::InitObjSnowballBehavior();
    Rando::ActorBehavior::InitObjTaruBehavior();
    Rando::ActorBehavior::InitObjTreeBehavior();
    Rando::ActorBehavior::InitObjTsuboBehavior();
    Rando::ActorBehavior::InitObjWarpstoneBehavior();
    Rando::ActorBehavior::InitPlayerBehavior();
    Rando::ActorBehavior::InitSoulsBehavior();
    Rando::ActorBehavior::InitTrapsBehavior();
    Rando::ActorBehavior::InitWonderItemsBehavior();

    COND_HOOK(ShouldVanillaBehavior, IS_RANDO, MiscVanillaBehaviorHandler);
}
