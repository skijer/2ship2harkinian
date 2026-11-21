// Odolwa's own damage enum lives in his overlay source; 1 is ODOLWA_DMGEFF_STUN.
#define ODOLWA_DMGEFF_STUN 1

extern HOST_DATA void Boss01_Stunned(Boss01* boss, PlayState* play);
extern HOST_DATA void Boss01_Dazed(Boss01* boss, PlayState* play);
extern HOST_DATA void Boss01_DeathCutscene(Boss01* boss, PlayState* play);

static bool IsOdolwaHitByByrna(Boss01* boss) {
    for (s32 i = 0; i < boss->bodyCollider.count; i++) {
        ColliderElement* element = &boss->bodyCollider.elements[i].base;
        if ((element->acElemFlags & ACELEM_HIT) && IsByrnaHitElement(element->acHitElem)) {
            return true;
        }
    }
    return false;
}

// A cane blow freezes Odolwa through his native stun; once he is stunned or dazed, blows deal damage again.
static void StunOdolwa(Boss01* boss) {
    if (ODOLWA_GET_TYPE(&boss->actor) != ODOLWA_TYPE_ODOLWA || boss->actor.colChkInfo.health == 0 ||
        boss->actionFunc == Boss01_Stunned || boss->actionFunc == Boss01_Dazed ||
        boss->actionFunc == Boss01_DeathCutscene || !IsOdolwaHitByByrna(boss)) {
        return;
    }
    boss->actor.colChkInfo.damageEffect = ODOLWA_DMGEFF_STUN;
}

static void StunByrnaBoss(Actor* actor) {
    switch (actor->id) {
        case ACTOR_BOSS_01:
            StunOdolwa((Boss01*)actor);
            break;
        default:
            break;
    }
}
