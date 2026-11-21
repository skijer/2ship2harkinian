static bool IsByrnaHitElement(ColliderElement* element) {
    if (element == NULL || !sCollidersReady) {
        return false;
    }
    if (element == &sForward.elem) {
        return true;
    }
    for (s32 i = 0; i < BYRNA_CYL_TRIS; i++) {
        if (element == &sAirElements[i].base) {
            return true;
        }
    }
    return false;
}

// Normal foes take Kokiri Sword power and bosses Fierce Deity power; the damage-table row stays the sword's.
static void ResolveByrnaDamage(Actor* victim, ColliderElement* attack, f32* damage) {
    if (victim == NULL || damage == NULL || !IsByrnaHitElement(attack)) {
        return;
    }
    *damage = victim->category == ACTORCAT_BOSS ? BYRNA_BOSS_DAMAGE : BYRNA_NORMAL_DAMAGE;
}

static void PrepareBossHit(Actor* actor, bool* should) {
    if (*should && actor->category == ACTORCAT_BOSS) {
        StunByrnaBoss(actor);
    }
}
