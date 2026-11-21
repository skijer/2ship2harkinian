static void ResetByrnaState(void) {
    memset(&sByrna, 0, sizeof(sByrna));
    sCollidersReady = false;
    sFairiesReady = false;
    sTrail = -1;
    sTrailDrawing = false;
    memset(sProjectiles, 0, sizeof(sProjectiles));
}

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    ResetByrnaState();
}

static void PutCaneAway(const char* key) {
    if (gPlayState != NULL && sByrna.active) {
        EndMove(gPlayState, GET_PLAYER(gPlayState));
    }
    if (gPlayState != NULL && sTrail >= 0) {
        Effect_Destroy(gPlayState, sTrail);
    }
    ResetByrnaState();
}

static const char* const sRequiredHooks[] = {
    "OnGameFrameUpdate",
    "OnPlayerUpdate",
    "OnPlayerActionHandler",
    "OnPlayerFilterInput",
    "OnPlayerResolveAnim",
    "OnPlayerResolveAnimSite",
    "OnPlayerResolveModelGroup",
    "OnPlayerResolveLimbDraw",
    "OnPlayerPostLimbDraw",
    "OnActorDrawEnd",
    "OnActorUpdate",
    "OnActorKill",
    "ShouldActorUpdate",
    "OnSceneInit",
    "OnCollisionResolveDamage",
};

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    S2HCustomEquipDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = BYRNA_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rCane of Byrna&%w%y\xB1%w: slash  Run + %y\xB1%w: dash&%y\xB0%w replaces roll with a vault.^"
                           "In air: %y\xB1%w strikes; %y\xB0%w dashes;&%y\xB4%w pounds. Enemy hits rebound.^"
                           "Ground %y\xB4%w: magic guard, all sides.&Costs %g2 MP%w every 20 frames.&Other moves use "
                           "no magic.^"
                           "Normal foes: Kokiri Sword power.&Bosses: Fierce Deity power;&a blow stuns Odolwa.";
    definition.getItemText = "You got the %rCane of Byrna%w!&Slash, vault and rebound&through the air.^"
                             "%y\xB0%w while moving vaults;&%y\xB0%w standing still puts it away.&%y\xB4%w guards at "
                             "%g2 MP%w / 20 frames.";
    definition.slot = S2H_EQUIP_SLOT_SWORD;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_SWORD;
    definition.column = 1;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.vanillaBase = EQUIP_VALUE_SWORD_KOKIRI;
    definition.toggles = 1;
    definition.onUnequip = PutCaneAway;
    definition.getItemModelPath = sCaneGiveDL;
    definition.getItemEntry.drawFunc = DrawGetItem;
    static const S2HCustomItemRandomizer randomizer = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .type = S2H_CUSTOM_ITEM_TYPE_EQUIP,
        .poolCount = 1,
        .logicKey = BYRNA_KEY,
        .hintKey = BYRNA_KEY,
    };
    definition.randomizer = &randomizer;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickByrna);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_MELEE, TakeMeleeButton);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_ROLL, TakeAButton);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_ZTARGET_A, TakeAButton);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_SHIELD, TakeShieldButton);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, KeepButtonsFromVanilla);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveAnim, ResolveStaffAnimation);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveAnimSite, ResolveStaffHop);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveModelGroup, WieldWithBothHands);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, GripCane);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawCane);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawByrnaEffects);
    S2H_REGISTER_HOOK(sApi, OnActorUpdate, CacheProjectileColliders);
    S2H_REGISTER_HOOK(sApi, OnActorKill, ForgetProjectile);
    S2H_REGISTER_HOOK(sApi, OnCollisionResolveDamage, ResolveByrnaDamage);
    S2H_REGISTER_HOOK(sApi, ShouldActorUpdate, PrepareBossHit);
    S2H_REGISTER_HOOK(sApi, ShouldActorUpdate, ReflectProjectile);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    sApi->RegisterVB(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, InterceptDraw);
}
