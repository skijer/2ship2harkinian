// OoT's cucco, kept by this mod: MM's EnNiw has other fields and its limb animation is private to its overlay.
// Cojiro's models replace OoT's 16 cucco limbs, so the OoT skeleton (oot_assets) and this animation go with them.
#define OOT_NIW_LIMB_COUNT 16
#define OOT_NIW_LIMB_LEFT_WING 7
#define OOT_NIW_LIMB_RIGHT_WING 11
#define OOT_NIW_LIMB_NECK 13
#define OOT_NIW_LIMB_HEAD 15

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[OOT_NIW_LIMB_COUNT];
    Vec3s morphTable[OOT_NIW_LIMB_COUNT];
    s16 timer1;
    s16 timer2;
    f32 targetRot[10];
    s16 headBobCount;
    s16 wingFlapPhase;
    f32 leftWingRot[3];
    f32 rightWingRot[3];
    f32 neckRotY;
    f32 headRotY;
    ColliderCylinder collider;
} OotNiw;

static void OotNiw_StepRotation(f32* rotation, f32 target, f32 scale, f32 step) {
    if (*rotation != target) {
        Math_ApproachF(rotation, target, scale, step);
    }
}

// OoT's func_80AB5BF8: state 0 idles, 1 runs, 2 flaps in the air.
static void OotNiw_Animate(OotNiw* this, s16 state) {
    if (this->timer1 == 0) {
        this->targetRot[0] = (state == 0) ? 0.0f : -10000.0f;
        this->headBobCount++;
        this->timer1 = 3;
        if (!(this->headBobCount & 1)) {
            this->targetRot[0] = 0.0f;
            if (state == 0) {
                this->timer1 = Rand_ZeroFloat(30.0f);
            }
        }
    }
    if (this->timer2 == 0) {
        this->wingFlapPhase = (this->wingFlapPhase + 1) & 1;
        switch (state) {
            case 0:
                this->targetRot[1] = this->targetRot[2] = 0.0f;
                break;
            case 1:
                this->timer2 = 3;
                this->targetRot[1] = this->targetRot[2] = (this->wingFlapPhase == 0) ? 0.0f : 7000.0f;
                break;
            case 2:
                this->timer2 = 2;
                this->targetRot[1] = this->targetRot[2] = -10000.0f;
                this->targetRot[7] = this->targetRot[5] = (this->wingFlapPhase == 0) ? 8000.0f : 25000.0f;
                this->targetRot[8] = this->targetRot[6] = 6000.0f;
                break;
            default:
                break;
        }
    }
    OotNiw_StepRotation(&this->headRotY, this->targetRot[9], 0.5f, 4000.0f);
    OotNiw_StepRotation(&this->neckRotY, this->targetRot[0], 0.5f, 4000.0f);
    OotNiw_StepRotation(&this->leftWingRot[2], this->targetRot[2], 0.8f, 7000.0f);
    OotNiw_StepRotation(&this->leftWingRot[1], this->targetRot[7], 0.8f, 7000.0f);
    OotNiw_StepRotation(&this->leftWingRot[0], this->targetRot[8], 0.8f, 7000.0f);
    OotNiw_StepRotation(&this->rightWingRot[2], this->targetRot[1], 0.8f, 7000.0f);
    OotNiw_StepRotation(&this->rightWingRot[1], this->targetRot[5], 0.8f, 7000.0f);
    OotNiw_StepRotation(&this->rightWingRot[0], this->targetRot[6], 0.8f, 7000.0f);
}

// OoT's EnNiw_OverrideLimbDraw.
static void OotNiw_RotateLimb(OotNiw* this, s32 limbIndex, Vec3s* rot) {
    if (limbIndex == OOT_NIW_LIMB_NECK) {
        rot->y += (s16)this->neckRotY;
    }
    if (limbIndex == OOT_NIW_LIMB_HEAD) {
        rot->y += (s16)this->headRotY;
    }
    if (limbIndex == OOT_NIW_LIMB_RIGHT_WING) {
        rot->x += (s16)this->rightWingRot[0];
        rot->y += (s16)this->rightWingRot[1];
        rot->z += (s16)this->rightWingRot[2];
    }
    if (limbIndex == OOT_NIW_LIMB_LEFT_WING) {
        rot->x += (s16)this->leftWingRot[0];
        rot->y += (s16)this->leftWingRot[1];
        rot->z += (s16)this->leftWingRot[2];
    }
}
