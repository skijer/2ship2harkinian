// Travel between warp points: a white flash inside one scene, a shrink-to-a-speck scene warp between scenes.

#define WARP_SCALE 0.0005f
#define WARP_SCALE_STEP 0.0001f
#define FLASH_ALPHA_STEP 51
#define ROOM_SETTLE_FRAMES 5

typedef enum {
    MINISH_TRAVEL_IDLE,
    MINISH_TRAVEL_DEPARTING,
    MINISH_TRAVEL_IN_TRANSIT,
    MINISH_TRAVEL_ARRIVING,
    MINISH_TRAVEL_FLASH_IN,
    MINISH_TRAVEL_LOADING_ROOM,
    MINISH_TRAVEL_FLASH_OUT,
} MinishTravelPhase;

static MinishTravelPhase sTravelPhase;
static s8 sTravelDestination = -1;
static s16 sRoomSettleTimer;
static bool sIsVoidDamageWaived;

static bool IsTravelling(void) {
    return sTravelPhase != MINISH_TRAVEL_IDLE;
}

static bool IsFlashing(void) {
    return sTravelPhase == MINISH_TRAVEL_FLASH_IN || sTravelPhase == MINISH_TRAVEL_LOADING_ROOM ||
           sTravelPhase == MINISH_TRAVEL_FLASH_OUT;
}

static void HoldPlayerStill(void) {
    sApi->RequestTimeControl(MINISH_CAP_OWNER, MINISH_CAP_TIME_PRIORITY, 0.0f, true);
    sApi->BlockPlayerInput(MINISH_CAP_OWNER, 0xFFFF, true);
}

static void ReleasePlayer(void) {
    sApi->ReleasePlayerInput(MINISH_CAP_OWNER);
    sApi->ReleaseTimeControl(MINISH_CAP_OWNER);
}

static void FinishTravel(void) {
    if (IsFlashing()) {
        R_PLAY_FILL_SCREEN_ALPHA = 0;
        R_PLAY_FILL_SCREEN_ON = false;
    }
    sTravelPhase = MINISH_TRAVEL_IDLE;
    sTravelDestination = -1;
    ReleasePlayer();
}

// respawnFlag 1 is the "fell in a hole" landing, the only one that honours a hand-written position,
// so the void damage it carries is waived once, for this transition alone.
static void WaiveVoidDamage(bool* should, va_list args) {
    if (!sIsVoidDamageWaived) {
        return;
    }
    sIsVoidDamageWaived = false;
    *should = false;
}

#define BEAN_GROTTO_HOLE_ROOM 1

static const Vec3f sBeanGrottoHolePos = { 1190.0f, 0.0f, 1011.0f };

// A grotto's way out returns to the hole recorded on the way in, so the Bean Seller's one is recorded by hand.
static void RecordBeanGrottoHole(void) {
    RespawnData* hole = &gSaveContext.respawn[RESPAWN_MODE_UNK_3];

    hole->entrance = ENTRANCE(DEKU_PALACE, 9);
    hole->roomIndex = BEAN_GROTTO_HOLE_ROOM;
    hole->pos = sBeanGrottoHolePos;
    hole->yaw = 0;
    hole->playerParams = PLAYER_PARAMS(0xFF, PLAYER_START_MODE_GROTTO);
    hole->data = 0;
}

static void WarpToOtherScene(PlayState* play, const WarpPoint* point) {
    RespawnData* respawn = &gSaveContext.respawn[RESPAWN_MODE_DOWN];

    play->nextEntrance = point->entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_WHITE;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE_FAST;
    respawn->entrance = point->entrance;
    respawn->roomIndex = point->room;
    respawn->pos = point->pos;
    respawn->yaw = DEG_TO_BINANG(point->yawDegrees);
    respawn->playerParams = PLAYER_PARAMS(0xFF, PLAYER_START_MODE_D);
    respawn->tempSwitchFlags = 0;
    respawn->unk_18 = 0;
    respawn->tempCollectFlags = 0;
    gSaveContext.respawnFlag = 1;
    sIsVoidDamageWaived = true;
    if (point->sceneId == SCENE_KAKUSIANA) {
        RecordBeanGrottoHole();
    }
}

static void StartTravel(PlayState* play, s8 destination) {
    sTravelDestination = destination;
    HoldPlayerStill();
    Audio_PlaySfx(NA_SE_PL_MAGIC_WIND_WARP);
    if (sWarpPoints[destination].sceneId == play->sceneId) {
        R_PLAY_FILL_SCREEN_ON = true;
        R_PLAY_FILL_SCREEN_R = 255;
        R_PLAY_FILL_SCREEN_G = 255;
        R_PLAY_FILL_SCREEN_B = 255;
        R_PLAY_FILL_SCREEN_ALPHA = 0;
        sTravelPhase = MINISH_TRAVEL_FLASH_IN;
        return;
    }
    sTravelPhase = MINISH_TRAVEL_DEPARTING;
}

static void ArriveAsMinish(Player* player) {
    ApplyTinyProperties(player);
    sSizePhase = MINISH_SIZE_TINY;
    sPlayerScale = WARP_SCALE;
    sTravelPhase = MINISH_TRAVEL_ARRIVING;
    HoldPlayerStill();
}

// The camera keeps its framing: it moves by the same offset as Link, so it does not sweep across the scene.
static void PlacePlayerAtPoint(Player* player, PlayState* play, const WarpPoint* point) {
    Camera* camera = Play_GetCamera(play, CAM_ID_MAIN);
    Vec3f destination = point->pos;
    Vec3f offset;
    Vec3f eye;
    Vec3f at;

    Math_Vec3f_Diff(&destination, &player->actor.world.pos, &offset);
    Math_Vec3f_Sum(&camera->eye, &offset, &eye);
    Math_Vec3f_Sum(&camera->at, &offset, &at);

    player->actor.world.pos = destination;
    player->actor.prevPos = destination;
    player->actor.velocity.y = 0.0f;
    player->speedXZ = 0.0f;
    player->yaw = DEG_TO_BINANG(point->yawDegrees);
    player->actor.shape.rot.y = player->yaw;
    player->actor.world.rot.y = player->yaw;
    Play_SetCameraAtEye(play, CAM_ID_MAIN, &at, &eye);
}

static bool IsRoomLoaded(PlayState* play, s8 room) {
    return play->roomCtx.curRoom.num == room && play->roomCtx.prevRoom.num < 0;
}

// Mirrors the Deku drowning respawn: unload every room, then request the destination one.
static void UnloadRooms(PlayState* play) {
    play->roomCtx.curRoom.num = -1;
    play->roomCtx.prevRoom.num = -1;
    play->roomCtx.curRoom.segment = NULL;
    play->roomCtx.prevRoom.segment = NULL;
    Room_FinishRoomChange(play, &play->roomCtx);
}

static void UpdateFlashIn(Player* player, PlayState* play) {
    const WarpPoint* point = &sWarpPoints[sTravelDestination];

    R_PLAY_FILL_SCREEN_ALPHA += FLASH_ALPHA_STEP;
    if (R_PLAY_FILL_SCREEN_ALPHA < 255) {
        return;
    }
    R_PLAY_FILL_SCREEN_ALPHA = 255;
    PlacePlayerAtPoint(player, play, point);
    sRoomSettleTimer = ROOM_SETTLE_FRAMES;
    if (IsRoomLoaded(play, point->room)) {
        sTravelPhase = MINISH_TRAVEL_FLASH_OUT;
        return;
    }
    UnloadRooms(play);
    sTravelPhase = MINISH_TRAVEL_LOADING_ROOM;
}

static void UpdateRoomLoad(PlayState* play) {
    if (play->roomCtx.curRoom.num < 0) {
        if (Room_RequestNewRoom(play, &play->roomCtx, sWarpPoints[sTravelDestination].room)) {
            Map_InitRoomData(play, play->roomCtx.curRoom.num);
            Map_SetAreaEntrypoint(play);
        }
        return;
    }
    sRoomSettleTimer--;
    if (sRoomSettleTimer <= 0) {
        sTravelPhase = MINISH_TRAVEL_FLASH_OUT;
    }
}

static void UpdateFlashOut(PlayState* play) {
    R_PLAY_FILL_SCREEN_ALPHA -= FLASH_ALPHA_STEP;
    if (R_PLAY_FILL_SCREEN_ALPHA > 0) {
        return;
    }
    R_PLAY_FILL_SCREEN_ALPHA = 0;
    R_PLAY_FILL_SCREEN_ON = false;
    Play_SetupRespawnPoint(play, RESPAWN_MODE_DOWN, PLAYER_PARAMS(0xFF, PLAYER_START_MODE_D));
    FinishTravel();
}

static void UpdateDeparture(PlayState* play) {
    sPlayerScale -= WARP_SCALE_STEP;
    if (sPlayerScale > WARP_SCALE) {
        return;
    }
    sPlayerScale = WARP_SCALE;
    WarpToOtherScene(play, &sWarpPoints[sTravelDestination]);
    sTravelPhase = MINISH_TRAVEL_IN_TRANSIT;
}

static void UpdateArrival(void) {
    sPlayerScale += WARP_SCALE_STEP;
    if (sPlayerScale < MINISH_SCALE) {
        return;
    }
    sPlayerScale = MINISH_SCALE;
    FinishTravel();
}

static void UpdateTravel(Player* player, PlayState* play) {
    switch (sTravelPhase) {
        case MINISH_TRAVEL_DEPARTING:
            UpdateDeparture(play);
            break;
        case MINISH_TRAVEL_IN_TRANSIT:
            break;
        case MINISH_TRAVEL_ARRIVING:
            UpdateArrival();
            break;
        case MINISH_TRAVEL_FLASH_IN:
            UpdateFlashIn(player, play);
            break;
        case MINISH_TRAVEL_LOADING_ROOM:
            UpdateRoomLoad(play);
            break;
        case MINISH_TRAVEL_FLASH_OUT:
            UpdateFlashOut(play);
            break;
        default:
            FinishTravel();
            break;
    }
}
