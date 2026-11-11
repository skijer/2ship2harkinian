/**
 * Ship's skill shuffles in Majora's Mask. A chest or an NPC that the player may not use yet withdraws the
 * offer it just made to Link, as Ship's VB_OPEN_CHEST and VB_SPEAK do. Each form speaks its own language.
 */

#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "z64items.h"
#include "z64logic.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "2s2h/Rando/Types.h"
#include "overlays/actors/ovl_En_Sob1/z_en_sob1.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define OWNER "oot_skills"
#define OPEN_CHEST_OPTION "oot.shuffle_open_chest"
#define CLIMB_OPTION "oot.shuffle_climb"
#define SPEAK_OPTION "oot.shuffle_speak"
#define EXTRA_SPEAK_OPTION "oot.shuffle_extra_speak"
#define AGONY_OPTION "oot.csmc_agony"
// Fleet's shared families (SharedItems.h) name MM's copy of each skill by these keys.
#define OPEN_CHEST_KEY "nei.skill.open_chests"
#define CLIMB_KEY "nei.skill.climb"
#define STONE_OF_AGONY_KEY "oot.stone_of_agony"
#define RESCUE_CVAR "gCheats.OoTSkills.Rescue"

#define ICON(name) "__OTR__textures/icon_item_custom/" name

static const char sChestModel[] = "__OTR__objects/object_gi_key/gGiSmallKeyDL";
static const char sClimbModel[] = "__OTR__objects/object_gi_bean/gGiBeanDL";
static const char sNutModel[] = "__OTR__objects/object_gi_nuts/gGiNutDL";

static const char* const sRequiredHooks[] = { "OnActorUpdate" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;

typedef enum {
    SPEECH_NONE,
    SPEECH_TERMINAN,
    SPEECH_DEKU,
    SPEECH_GORON,
    SPEECH_ZORA,
    SPEECH_GERUDO,
    SPEECH_KEATON,
    SPEECH_ANIMAL,
    SPEECH_GOSSIP,
    SPEECH_MAX,
} SpokenLanguage;

typedef struct {
    const char* nutKey;
    const char* nutName;
    const char* listener;
    // Extra languages are free unless Extra Jabber Nuts is on.
    bool isExtra;
    // A form that speaks it natively: a vanilla mask or a custom form mod's mask key.
    RandoItemId nativeMask;
    const char* nativeFormMask;
    s32 nativeForm;
    const char* nativeCustomForm;
    const char* icon;
} LanguageInfo;

static const LanguageInfo sLanguages[SPEECH_MAX] = {
    [SPEECH_TERMINAN] = { "nei.skill.speak_terminan", "Terminan Jabber Nut", "the people of Termina", false,
                          RI_UNKNOWN, "nei.mask_kafei", -1, "nei.kafei", ICON("gItemIconJabberNutTerminanTex") },
    [SPEECH_DEKU] = { "nei.skill.speak_deku", "Deku Jabber Nut", "the Deku", false, RI_MASK_DEKU, NULL,
                      PLAYER_FORM_DEKU, NULL, ICON("gItemIconJabberNutDekuTex") },
    [SPEECH_GORON] = { "nei.skill.speak_goron", "Goron Jabber Nut", "the Gorons", false, RI_MASK_GORON, NULL,
                       PLAYER_FORM_GORON, NULL, ICON("gItemIconJabberNutGoronTex") },
    [SPEECH_ZORA] = { "nei.skill.speak_zora", "Zora Jabber Nut", "the Zoras", false, RI_MASK_ZORA, NULL,
                      PLAYER_FORM_ZORA, NULL, ICON("gItemIconJabberNutZoraTex") },
    [SPEECH_GERUDO] = { "nei.skill.speak_gerudo", "Gerudo Jabber Nut", "the Gerudo", false, RI_UNKNOWN,
                        "nei.gerudo_form_mask", -1, "nei.gerudo", ICON("gItemIconJabberNutGerudoTex") },
    [SPEECH_KEATON] = { "nei.skill.speak_animal", "Animal Jabber Nut", "animals", true, RI_UNKNOWN,
                        "nei.keaton_form_mask", -1, "nei.keaton", ICON("gItemIconJabberNutAnimalTex") },
    [SPEECH_ANIMAL] = { "nei.skill.speak_animal", "Animal Jabber Nut", "animals", true, RI_UNKNOWN, NULL, -1, NULL,
                        ICON("gItemIconJabberNutAnimalTex") },
    [SPEECH_GOSSIP] = { "nei.skill.speak_gossip", "Gossip Jabber Nut", "the Gossip Stones", true, RI_UNKNOWN,
                        "nei.garo_form_mask", -1, "nei.garo", ICON("gItemIconJabberNutGossipTex") },
};

static bool IsRandoSave(void) {
    return gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO;
}

static bool IsOptionOn(const char* key) {
    return sApi->GetRandoOption(key) != 0;
}

static bool IsRescueOn(void) {
    return CVarGetInteger(RESCUE_CVAR, 0) != 0;
}

static bool IsLanguageGated(SpokenLanguage language) {
    if (language == SPEECH_NONE || !IsOptionOn(SPEAK_OPTION)) {
        return false;
    }
    return !sLanguages[language].isExtra || IsOptionOn(EXTRA_SPEAK_OPTION);
}

static bool IsSpeakingNatively(const LanguageInfo* info) {
    const char* customForm = sApi->GetActiveForm();
    const bool hasCustomForm = customForm != NULL && customForm[0] != '\0';

    if (info->nativeCustomForm != NULL && hasCustomForm) {
        return sApi->IsCustomItemOwned(info->nativeFormMask) &&
               strcmp(customForm, info->nativeCustomForm) == 0;
    }
    return !hasCustomForm && info->nativeForm >= 0 && GET_PLAYER_FORM == info->nativeForm;
}

static bool CanSpeakNow(SpokenLanguage language) {
    if (!IsLanguageGated(language)) {
        return true;
    }
    const LanguageInfo* info = &sLanguages[language];
    return sApi->IsCustomItemOwned(info->nutKey) || IsSpeakingNatively(info);
}

static bool CanOpenChestsNow(void) {
    return !IsOptionOn(OPEN_CHEST_OPTION) || sApi->IsCustomItemOwned(OPEN_CHEST_KEY);
}

static bool CanClimbNow(void) {
    return !IsOptionOn(CLIMB_OPTION) || sApi->IsCustomItemOwned(CLIMB_KEY) || IsRescueOn();
}

static SpokenLanguage GetShopLanguage(Actor* actor) {
    switch (ENSOB1_GET_SHOPTYPE(actor)) {
        case ZORA_SHOP:
            return SPEECH_ZORA;
        case GORON_SHOP:
        case GORON_SHOP_SPRING:
            return SPEECH_GORON;
        case BOMB_SHOP:
            return SPEECH_TERMINAN;
        default:
            return SPEECH_NONE;
    }
}

// The Happy Mask Salesman, the Moon children and other story speakers stay free so no cycle can stall.
static SpokenLanguage GetActorLanguage(Actor* actor) {
    switch (actor->id) {
        case ACTOR_EN_SOB1:
            return GetShopLanguage(actor);
        case ACTOR_EN_AKINDONUTS:
        case ACTOR_EN_SELLNUTS:
        case ACTOR_EN_SCOPENUTS:
        case ACTOR_EN_LIFT_NUTS:
        case ACTOR_EN_GUARD_NUTS:
        case ACTOR_EN_DNO:
        case ACTOR_EN_DNP:
        case ACTOR_EN_DNQ:
        case ACTOR_EN_DNS:
        case ACTOR_EN_DNK:
            return SPEECH_DEKU;
        case ACTOR_EN_GO:
        case ACTOR_EN_GK:
        case ACTOR_EN_GEG:
        case ACTOR_EN_RG:
        case ACTOR_EN_S_GORO:
        case ACTOR_EN_JG:
        case ACTOR_EN_DAI:
        case ACTOR_EN_GG:
        case ACTOR_EN_GG2:
            return SPEECH_GORON;
        case ACTOR_EN_ZO:
        case ACTOR_EN_ZOB:
        case ACTOR_EN_ZOD:
        case ACTOR_EN_ZOG:
        case ACTOR_EN_ZOS:
        case ACTOR_EN_ZOT:
        case ACTOR_EN_ZOV:
        case ACTOR_EN_ZOW:
        case ACTOR_EN_TOTO:
            return SPEECH_ZORA;
        case ACTOR_EN_GE1:
        case ACTOR_EN_GE2:
        case ACTOR_EN_GE3:
            return SPEECH_GERUDO;
        case ACTOR_EN_KITAN:
            return SPEECH_KEATON;
        case ACTOR_EN_FG:
        case ACTOR_EN_MINIFROG:
        case ACTOR_EN_MNK:
        case ACTOR_EN_OT:
        case ACTOR_EN_AZ:
        case ACTOR_EN_RACEDOG:
        case ACTOR_EN_OWL:
        case ACTOR_EN_SCOPECROW:
            return SPEECH_ANIMAL;
        case ACTOR_EN_GS:
            return SPEECH_GOSSIP;
        case ACTOR_DM_AH:
        case ACTOR_EN_AH:
        case ACTOR_EN_AL:
        case ACTOR_DM_AN:
        case ACTOR_EN_AN:
        case ACTOR_EN_AND:
        case ACTOR_EN_ANI:
        case ACTOR_EN_AOB_01:
        case ACTOR_EN_BABA:
        case ACTOR_EN_BAISEN:
        case ACTOR_EN_BAL:
        case ACTOR_DM_BAL:
        case ACTOR_EN_BBA_01:
        case ACTOR_EN_BJI_01:
        case ACTOR_EN_BOJ_01:
        case ACTOR_EN_BOJ_02:
        case ACTOR_EN_BOJ_03:
        case ACTOR_EN_BOJ_04:
        case ACTOR_EN_BOJ_05:
        case ACTOR_EN_BOM_BOWL_MAN:
        case ACTOR_EN_BOMBERS:
        case ACTOR_EN_BOMBERS2:
        case ACTOR_EN_BOMJIMA:
        case ACTOR_EN_BOMJIMB:
        case ACTOR_EN_CNE_01:
        case ACTOR_EN_DAIKU:
        case ACTOR_EN_DAIKU2:
        case ACTOR_EN_DT:
        case ACTOR_EN_FSN:
        case ACTOR_EN_FU:
        case ACTOR_EN_GB2:
        case ACTOR_EN_GINKO_MAN:
        case ACTOR_EN_GM:
        case ACTOR_EN_GURUGURU:
        case ACTOR_EN_HEISHI:
        case ACTOR_EN_STONE_HEISHI:
        case ACTOR_EN_STOP_HEISHI:
        case ACTOR_EN_HGO:
        case ACTOR_EN_HS:
        case ACTOR_EN_IN:
        case ACTOR_EN_JA:
        case ACTOR_EN_JGAME_TSN:
        case ACTOR_EN_TSN:
        case ACTOR_EN_KENDO_JS:
        case ACTOR_EN_KGY:
        case ACTOR_EN_KBT:
        case ACTOR_EN_KUJIYA:
        case ACTOR_EN_MA4:
        case ACTOR_EN_MA_YTO:
        case ACTOR_EN_MA_YTS:
        case ACTOR_EN_MK:
        case ACTOR_EN_MM3:
        case ACTOR_EN_MS:
        case ACTOR_EN_MUTO:
        case ACTOR_EN_NB:
        case ACTOR_DM_NB:
        case ACTOR_EN_OSSAN:
        case ACTOR_EN_PAMERA:
        case ACTOR_EN_PM:
        case ACTOR_EN_RECEPGIRL:
        case ACTOR_EN_RZ:
        case ACTOR_EN_SHN:
        case ACTOR_EN_SSH:
        case ACTOR_EN_STH:
        case ACTOR_EN_STH2:
        case ACTOR_EN_SUTTARI:
        case ACTOR_EN_SYATEKI_MAN:
        case ACTOR_EN_TAB:
        case ACTOR_EN_TAKARAYA:
        case ACTOR_EN_TEST3:
        case ACTOR_EN_TG:
        case ACTOR_EN_TK:
        case ACTOR_EN_TRT:
        case ACTOR_EN_TRT2:
        case ACTOR_EN_TRU:
        case ACTOR_EN_TRU_MT:
        case ACTOR_EN_YB:
            return SPEECH_TERMINAN;
        default:
            return SPEECH_NONE;
    }
}

// Actors update after Player, so the offer an actor just made reaches Link on the next frame.
static void WithdrawLockedOffer(Actor* actor) {
    if (gPlayState == NULL || !IsRandoSave()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == NULL) {
        return;
    }
    if (actor->id == ACTOR_EN_BOX && player->interactRangeActor == actor && !CanOpenChestsNow()) {
        player->interactRangeActor = NULL;
        player->getItemId = GI_NONE;
        return;
    }
    if (player->talkActor != actor || (actor->flags & ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED)) {
        return;
    }
    // A conversation already under way keeps its actor: Player_Action_Talk dereferences talkActor until it closes.
    if (player->stateFlags1 & PLAYER_STATE1_TALKING) {
        return;
    }
    if (!CanSpeakNow(GetActorLanguage(actor))) {
        player->talkActor = NULL;
        player->talkActorDistance = FLT_MAX;
        player->exchangeItemAction = PLAYER_IA_NONE;
    }
}

// Ship's VB_CLIMB: without the skill Link holds on to a ladder or a wall but cannot climb up it.
static void RestrictClimb(bool* should, va_list args) {
    if (!IsRandoSave() || CanClimbNow()) {
        return;
    }
    s32* xStick = va_arg(args, s32*);
    s32* yStick = va_arg(args, s32*);
    *xStick = 0;
    if (*yStick > 0) {
        *yStick = 0;
    }
}

static void RequireStoneOfAgony(bool* should, va_list args) {
    if (sApi->GetRandoOption(AGONY_OPTION) != 0 && !sApi->IsCustomItemOwned(STONE_OF_AGONY_KEY)) {
        *should = false;
    }
}

static bool CanOpenChestsInLogic(void* userData) {
    return !IsOptionOn(OPEN_CHEST_OPTION) || Z64Logic_HasKey(OPEN_CHEST_KEY);
}

static bool CanSpeakInLogic(void* userData) {
    const SpokenLanguage language = (SpokenLanguage)(intptr_t)userData;
    if (!IsLanguageGated(language)) {
        return true;
    }
    const LanguageInfo* info = &sLanguages[language];
    if (Z64Logic_HasKey(info->nutKey)) {
        return true;
    }
    if (info->nativeMask != RI_UNKNOWN && sApi->LogicHasItem(info->nativeMask)) {
        return true;
    }
    return info->nativeFormMask != NULL && Z64Logic_HasKey(info->nativeFormMask);
}

static const int32_t sTerminanChecks[] = {
    RC_ASTRAL_OBSERVATORY_MOON_TEAR,
    RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS,
    RC_BOMB_SHOP_ITEM_01,
    RC_BOMB_SHOP_ITEM_02,
    RC_BOMB_SHOP_ITEM_03,
    RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM,
    RC_CLOCK_TOWN_BOMBERS_NOTEBOOK,
    RC_CLOCK_TOWN_EAST_POSTMAN_HAT,
    RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_HIGH_SCORE,
    RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_PERFECT_SCORE,
    RC_CLOCK_TOWN_EAST_TREASURE_CHEST_GAME_DEKU,
    RC_CLOCK_TOWN_EAST_TREASURE_CHEST_GAME_GORON,
    RC_CLOCK_TOWN_EAST_TREASURE_CHEST_GAME_HUMAN,
    RC_CLOCK_TOWN_EAST_TREASURE_CHEST_GAME_ZORA,
    RC_CLOCK_TOWN_LAUNDRY_GURU_GURU,
    RC_CLOCK_TOWN_NORTH_BOMB_LADY,
    RC_CLOCK_TOWN_NORTH_TINGLE_MAP_01,
    RC_CLOCK_TOWN_NORTH_TINGLE_MAP_02,
    RC_CLOCK_TOWN_WEST_BANK_ADULTS_WALLET,
    RC_CLOCK_TOWN_WEST_BANK_INTEREST,
    RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART,
    RC_CLOCK_TOWN_WEST_LOTTERY,
    RC_CLOCK_TOWN_WEST_POSTMAN_MINIGAME,
    RC_CLOCK_TOWN_WEST_SISTERS_PIECE_OF_HEART,
    RC_CREMIA_ESCORT,
    RC_CURIOSITY_SHOP_SPECIAL_ITEM,
    RC_DOGGY_RACETRACK_PIECE_OF_HEART,
    RC_GORMAN_MILK_PURCHASE,
    RC_GORMAN_TRACK_GARO_MASK,
    RC_GREAT_BAY_COAST_FISHERMAN_MINIGAME,
    RC_GREAT_BAY_COAST_MARINE_LAB_FISH_PIECE_OF_HEART,
    RC_GREAT_BAY_COAST_TINGLE_MAP_01,
    RC_GREAT_BAY_COAST_TINGLE_MAP_02,
    RC_HAGS_POTION_SHOP_ITEM_01,
    RC_HAGS_POTION_SHOP_ITEM_02,
    RC_HAGS_POTION_SHOP_ITEM_03,
    RC_HAGS_POTION_SHOP_KOTAKE,
    RC_IKANA_CANYON_GHOST_HUT_PIECE_OF_HEART,
    RC_IKANA_CANYON_TINGLE_MAP_01,
    RC_IKANA_CANYON_TINGLE_MAP_02,
    RC_KAFEIS_HIDEOUT_KEATON_MASK,
    RC_KAFEIS_HIDEOUT_LETTER_TO_MAMA,
    RC_KAFEIS_HIDEOUT_PENDANT_OF_MEMORIES,
    RC_MAYORS_OFFICE_KAFEIS_MASK,
    RC_MAYORS_OFFICE_PIECE_OF_HEART,
    RC_MILK_BAR_CIRCUS_LEADER_MASK,
    RC_MILK_BAR_MADAME_AROMA,
    RC_MILK_BAR_PURCHASE_CHATEAU,
    RC_MILK_BAR_PURCHASE_MILK,
    RC_MILK_ROAD_TINGLE_MAP_01,
    RC_MILK_ROAD_TINGLE_MAP_02,
    RC_MOUNTAIN_VILLAGE_SMITHY_GILDED_SWORD,
    RC_MOUNTAIN_VILLAGE_SMITHY_RAZOR_SWORD,
    RC_MUSIC_BOX_HOUSE_FATHER,
    RC_OCEAN_SPIDER_HOUSE_WALLET,
    RC_ROAD_TO_IKANA_STONE_MASK,
    RC_ROAD_TO_SOUTHERN_SWAMP_TINGLE_MAP_01,
    RC_ROAD_TO_SOUTHERN_SWAMP_TINGLE_MAP_02,
    RC_ROMANI_RANCH_ALIENS,
    RC_ROMANI_RANCH_EPONAS_SONG,
    RC_ROMANI_RANCH_GROG,
    RC_STOCK_POT_INN_COUPLES_MASK,
    RC_STOCK_POT_INN_GRANDMA_LONG_STORY,
    RC_STOCK_POT_INN_GRANDMA_SHORT_STORY,
    RC_STOCK_POT_INN_LETTER_TO_KAFEI,
    RC_STOCK_POT_INN_ROOM_KEY,
    RC_SWAMP_SHOOTING_GALLERY_HIGH_SCORE,
    RC_SWAMP_SHOOTING_GALLERY_PERFECT_SCORE,
    RC_SWAMP_SPIDER_HOUSE_MASK_OF_TRUTH,
    RC_SWORDSMAN_SCHOOL_PIECE_OF_HEART,
    RC_TERMINA_FIELD_KAMARO_MASK,
    RC_TOURIST_INFORMATION_ARCHERY,
    RC_TOURIST_INFORMATION_GOOD_PHOTO,
    RC_TOURIST_INFORMATION_PICTOBOX,
    RC_TRADING_POST_SHOP_ITEM_01,
    RC_TRADING_POST_SHOP_ITEM_02,
    RC_TRADING_POST_SHOP_ITEM_03,
    RC_TRADING_POST_SHOP_ITEM_04,
    RC_TRADING_POST_SHOP_ITEM_05,
    RC_TRADING_POST_SHOP_ITEM_06,
    RC_TRADING_POST_SHOP_ITEM_07,
    RC_TRADING_POST_SHOP_ITEM_08,
    RC_TWIN_ISLANDS_TINGLE_MAP_01,
    RC_TWIN_ISLANDS_TINGLE_MAP_02,
};

static const int32_t sDekuChecks[] = {
    RC_CLOCK_TOWN_SCRUB_DEED,
    RC_DEKU_PLAYGROUND_ALL_DAYS,
    RC_DEKU_PLAYGROUND_ANY_DAY,
    RC_DEKU_SHRINE_MASK_OF_SCENTS,
    RC_GORON_VILLAGE_SCRUB_BOMB_BAG,
    RC_GORON_VILLAGE_SCRUB_DEED,
    RC_IKANA_CANYON_SCRUB_HUGE_RUPEE,
    RC_IKANA_CANYON_SCRUB_POTION_REFILL,
    RC_SOUTHERN_SWAMP_SCRUB_BEANS,
    RC_SOUTHERN_SWAMP_SCRUB_DEED,
    RC_TERMINA_FIELD_GROTTO_SCRUB,
    RC_ZORA_HALL_SCRUB_DEED,
    RC_ZORA_HALL_SCRUB_POTION_REFILL,
};

static const int32_t sGoronChecks[] = {
    RC_GORON_GRAVEYARD_DARMANI,
    RC_GORON_RACETRACK_GOLD_DUST,
    RC_GORON_SHOP_ITEM_01,
    RC_GORON_SHOP_ITEM_02,
    RC_GORON_SHOP_ITEM_03,
    RC_GORON_SHRINE_FULL_LULLABY,
    RC_GORON_VILLAGE_MEDIGORON,
    RC_MOUNTAIN_VILLAGE_DON_GERO_MASK,
    RC_PATH_TO_GORON_VILLAGE_LULLABY_INTRO,
};

static const int32_t sZoraChecks[] = {
    RC_GREAT_BAY_COAST_MIKAU,
    RC_ZORA_HALL_SCENE_LIGHTS,
    RC_ZORA_SHOP_ITEM_01,
    RC_ZORA_SHOP_ITEM_02,
    RC_ZORA_SHOP_ITEM_03,
};

static const int32_t sKeatonChecks[] = {
    RC_KEATON_QUIZ,
};

static const int32_t sAnimalChecks[] = {
    RC_CLOCK_TOWN_LAUNDRY_FROG,
    RC_DEKU_KINGS_CHAMBER_MONKEY,
    RC_GREAT_BAY_TEMPLE_GEKKO_FROG,
    RC_MOUNTAIN_VILLAGE_FROG_CHOIR,
    RC_PINNACLE_ROCK_REUNITE_SEAHORSE,
    RC_SOUTHERN_SWAMP_FROG,
    RC_WATERFALL_RAPIDS_BEAVER_RACE_01,
    RC_WATERFALL_RAPIDS_BEAVER_RACE_02,
    RC_WOODFALL_TEMPLE_GEKKO_FROG,
};

typedef struct {
    const char* ruleName;
    SpokenLanguage language;
    const int32_t* checks;
    uint32_t checkCount;
} SpeakChecks;

static const SpeakChecks sSpeakChecks[] = {
    { OWNER ".speak_terminan", SPEECH_TERMINAN, sTerminanChecks, ARRAY_COUNT(sTerminanChecks) },
    { OWNER ".speak_deku", SPEECH_DEKU, sDekuChecks, ARRAY_COUNT(sDekuChecks) },
    { OWNER ".speak_goron", SPEECH_GORON, sGoronChecks, ARRAY_COUNT(sGoronChecks) },
    { OWNER ".speak_zora", SPEECH_ZORA, sZoraChecks, ARRAY_COUNT(sZoraChecks) },
    { OWNER ".speak_keaton", SPEECH_KEATON, sKeatonChecks, ARRAY_COUNT(sKeatonChecks) },
    { OWNER ".speak_animal", SPEECH_ANIMAL, sAnimalChecks, ARRAY_COUNT(sAnimalChecks) },
};

// The host replaces a rule registered under a name it already holds, so every rule needs its own name.
static void RequireAt(const char* name, const S2HLogicSelector* selector, S2HLogicCondition condition,
                      void* userData) {
    S2HLogicRule rule = { sizeof(S2HLogicRule) };
    rule.name = name;
    rule.effect = S2H_LOGIC_REQUIRE;
    rule.condition = condition;
    rule.userData = userData;
    rule.include = selector;
    rule.includeCount = 1;
    sApi->RegisterLogicRule(&rule);
}

// The host copies a rule's selectors, so the stack selectors here need not outlive the call.
static void RegisterLogicRules(void) {
    S2HLogicSelector chests = Z64Logic_SelectAny();
    chests.targets = S2H_LOGIC_TARGET_LOCATIONS;
    chests.checkType = RCTYPE_CHEST;
    RequireAt(OWNER ".open_chest", &chests, CanOpenChestsInLogic, NULL);

    for (uint32_t i = 0; i < ARRAY_COUNT(sSpeakChecks); i++) {
        S2HLogicSelector speakers = Z64Logic_SelectAny();
        speakers.targets = S2H_LOGIC_TARGET_LOCATIONS;
        speakers.checks = sSpeakChecks[i].checks;
        speakers.checkCount = sSpeakChecks[i].checkCount;
        RequireAt(sSpeakChecks[i].ruleName, &speakers, CanSpeakInLogic, (void*)(intptr_t)sSpeakChecks[i].language);
    }
}

static const char* const sOffOn[] = { "Off", "On" };

static const S2HModRandoOption sOptions[] = {
    { sizeof(S2HModRandoOption), OPEN_CHEST_OPTION, "Shuffle Open Chests",
      "Chests stay shut until the Open Chests skill is found.", sOffOn, 2, 0, S2H_MOD_RANDO_WIDGET_COMBOBOX },
    { sizeof(S2HModRandoOption), CLIMB_OPTION, "Shuffle Climb",
      "Link cannot climb up ladders and walls until the Climb skill is found.", sOffOn, 2, 0,
      S2H_MOD_RANDO_WIDGET_COMBOBOX },
    { sizeof(S2HModRandoOption), SPEAK_OPTION, "Shuffle Speak",
      "Each form speaks its own language. Jabber Nuts teach the others: Terminan, Deku, Goron, Zora and Gerudo.",
      sOffOn, 2, 0, S2H_MOD_RANDO_WIDGET_COMBOBOX },
    { sizeof(S2HModRandoOption), EXTRA_SPEAK_OPTION, "Extra Jabber Nuts",
      "Animals, Keaton and the Gossip Stones need their own Jabber Nut too. Keaton and Garo speak with them.",
      sOffOn, 2, 0, S2H_MOD_RANDO_WIDGET_COMBOBOX },
    { sizeof(S2HModRandoOption), AGONY_OPTION, "Containers of Agony",
      "Containers only match their contents once the Stone of Agony is found.", sOffOn, 2, 0,
      S2H_MOD_RANDO_WIDGET_COMBOBOX },
};

typedef struct {
    const char* key;
    const char* name;
    const char* icon;
    const char* model;
    const char* text;
    const char* poolOption;
} SkillItem;

static bool RegisterSkillItem(const SkillItem* skill) {
    static S2HCustomItemRandomizer pools[SPEECH_MAX + 2];
    static uint32_t poolCount;
    if (poolCount >= ARRAY_COUNT(pools)) {
        return false;
    }
    S2HCustomItemRandomizer* pool = &pools[poolCount++];
    *pool = (S2HCustomItemRandomizer){ .structSize = sizeof(S2HCustomItemRandomizer),
                                       .flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT,
                                       .type = S2H_CUSTOM_ITEM_TYPE_ITEM,
                                       .poolCount = 1,
                                       .logicKey = skill->key,
                                       .hintKey = skill->key,
                                       .poolModeOption = skill->poolOption,
                                       .poolModeValue = 1 };

    S2HCustomItemDefinition definition = Z64Items_Define(skill->key, skill->icon, NULL);
    definition.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    definition.displayName = skill->name;
    definition.getItemScale = 1.0f;
    Z64Items_SetTextbox(&definition, skill->text);
    Z64Items_SetGetItemModel(&definition, skill->model);
    Z64Items_SetLogic(&definition, pool);
    return Z64Items_Register(sApi, &definition);
}

static void RegisterJabberNuts(void) {
    static char texts[SPEECH_MAX][160];
    const char* registered[SPEECH_MAX] = { 0 };
    uint32_t registeredCount = 0;

    for (uint32_t language = SPEECH_TERMINAN; language < SPEECH_MAX; language++) {
        const LanguageInfo* info = &sLanguages[language];
        bool isDuplicate = false;
        for (uint32_t i = 0; i < registeredCount; i++) {
            isDuplicate |= strcmp(registered[i], info->nutKey) == 0;
        }
        if (isDuplicate) {
            continue;
        }
        registered[registeredCount++] = info->nutKey;
        snprintf(texts[language], sizeof(texts[language]), "You got a %%r%s%%w!&Now you can speak with %s.",
                 info->nutName, info->listener);
        const SkillItem nut = { info->nutKey, info->nutName, info->icon, sNutModel, texts[language],
                                info->isExtra ? EXTRA_SPEAK_OPTION : SPEAK_OPTION };
        RegisterSkillItem(&nut);
    }
}

static void RegisterRescueCheat(void) {
    S2HModMenuWidget rescue = { sizeof(S2HModMenuWidget) };
    rescue.section = "Enhancements";
    rescue.sidebar = "Cheats";
    rescue.type = S2H_MOD_MENU_CHECKBOX;
    rescue.label = "Grant Climb (stuck seed)";
    rescue.cvar = RESCUE_CVAR;
    rescue.tooltip = "Lets Link climb without the Climb skill, for seeds that need a climb logic misses.";
    sApi->RegisterMenuWidget(&rescue);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterVBByName) || !S2H_MOD_API_HAS(sApi, RegisterRandoOption) ||
        !S2H_MOD_API_HAS(sApi, RegisterLogicRule) || !S2H_MOD_API_HAS(sApi, GetActiveForm) ||
        !S2H_MOD_API_HAS(sApi, RegisterMenuWidget) || !Z64Logic_Init(sApi)) {
        fprintf(stderr, "[oot_skills] The host lacks the skill API; the mod stays off\n");
        return;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(sOptions); i++) {
        sApi->RegisterRandoOption(&sOptions[i]);
    }
    const SkillItem openChest = { OPEN_CHEST_KEY, "Open Chests", ICON("gItemIconOpenChestsTex"), sChestModel,
                                  "You got %rOpen Chests%w!&Now you can open treasure chests.", OPEN_CHEST_OPTION };
    const SkillItem climb = { CLIMB_KEY, "Climb", ICON("gItemIconClimbTex"), sClimbModel,
                              "You got %rClimb%w!&Now you can climb ladders and walls.", CLIMB_OPTION };
    RegisterSkillItem(&openChest);
    RegisterSkillItem(&climb);
    RegisterJabberNuts();
    RegisterLogicRules();
    RegisterRescueCheat();

    sApi->RegisterVBByName("VB_CLIMB", RestrictClimb);
    if (!sApi->RegisterVBByName("VB_RANDO_MATCH_CONTAINER_STYLE", RequireStoneOfAgony)) {
        fprintf(stderr, "[oot_skills] The host has no VB_RANDO_MATCH_CONTAINER_STYLE; Containers of Agony is off\n");
    }
    S2H_REGISTER_HOOK(sApi, OnActorUpdate, WithdrawLockedOffer);
}
