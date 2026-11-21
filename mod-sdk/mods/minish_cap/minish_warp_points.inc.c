// The soft soils (Obj_Bean) Ezlo's map links, plus the Swamp Spider House door. Indices are save bits: append only.

#define WARP_POINT_COUNT 18
#define SOIL_REACH 40.0f
#define SOIL_HEIGHT_REACH 30.0f
#define VISITED_STORAGE_KEY "visited_soils"

typedef struct {
    s16 sceneId;
    u16 entrance;
    s8 room;
    Vec3f pos;
    s16 yawDegrees;
    s16 mapX;
    s16 mapY;
    const char* name;
    const char* nameTex;
    bool isSoil;
} WarpPoint;

static const WarpPoint sWarpPoints[WARP_POINT_COUNT] = {
    { SCENE_KINSTA1, ENTRANCE(SWAMP_SPIDER_HOUSE, 0), 0, { -40.0f, 120.0f, 60.0f }, 180, 28, 118,
      "Spider House Entrance", NULL, false },
    { SCENE_KINSTA1, ENTRANCE(SWAMP_SPIDER_HOUSE, 0), 1, { -360.0f, 112.0f, -748.0f }, 90, 19, 101,
      "Spider House Hall (West)", NULL, true },
    { SCENE_KINSTA1, ENTRANCE(SWAMP_SPIDER_HOUSE, 0), 1, { 301.0f, 357.0f, -180.0f }, 180, 40, 110,
      "Spider House Hall (Ledge)", NULL, true },
    { SCENE_KINSTA1, ENTRANCE(SWAMP_SPIDER_HOUSE, 0), 1, { 360.0f, 109.0f, -617.0f }, 90, 40, 98,
      "Spider House Hall (East)", NULL, true },
    { SCENE_KINSTA1, ENTRANCE(SWAMP_SPIDER_HOUSE, 0), 2, { -270.0f, 180.0f, -1617.0f }, 0, 19, 84,
      "Spider House North Room", NULL, true },
    { SCENE_KINSTA1, ENTRANCE(SWAMP_SPIDER_HOUSE, 0), 3, { -480.0f, 0.0f, -830.0f }, 270, 9, 93,
      "Spider House Boulder Room", NULL, true },
    { SCENE_00KEIKOKU, ENTRANCE(TERMINA_FIELD, 0), 0, { 3986.0f, 254.0f, 1965.0f }, 318, 128, 70,
      "Termina Field (Southeast)", NULL, true },
    { SCENE_00KEIKOKU, ENTRANCE(TERMINA_FIELD, 0), 0, { 2364.0f, 48.0f, -773.0f }, 0, 124, 61,
      "Termina Field (East)", NULL, true },
    { SCENE_00KEIKOKU, ENTRANCE(TERMINA_FIELD, 0), 0, { 360.0f, -222.0f, 3882.0f }, 14, 116, 78,
      "Termina Field (South)", NULL, true },
    { SCENE_00KEIKOKU, ENTRANCE(TERMINA_FIELD, 0), 0, { 614.0f, 42.0f, 1325.0f }, 30, 119, 71,
      "Termina Field (South Gate)", NULL, true },
    { SCENE_22DEKUCITY, ENTRANCE(DEKU_PALACE, 0), 0, { 1674.0f, 0.0f, 1000.0f }, 0, 113, 115, "Deku Palace",
      gMapPointDekuPalaceENGTex, true },
    { SCENE_KAKUSIANA, ENTRANCE(GROTTOS, 12), 12, { 4188.0f, 0.0f, 1218.0f }, 0, 106, 118, "Bean Seller's Grotto",
      NULL, true },
    { SCENE_30GYOSON, ENTRANCE(GREAT_BAY_COAST, 0), 0, { 1920.0f, 519.0f, -1358.0f }, 351, 45, 72,
      "Great Bay Coast", gMapPointGreatBayCoastENGTex, true },
    { SCENE_F01, ENTRANCE(ROMANI_RANCH, 0), 0, { -1424.0f, 312.0f, -1917.0f }, 203, 101, 82, "Romani Ranch",
      gMapPointRomaniRanchENGTex, true },
    { SCENE_F01_B, ENTRANCE(DOGGY_RACETRACK, 0), 0, { -4089.0f, 155.0f, 1960.0f }, 0, 94, 88, "Doggy Racetrack",
      NULL, true },
    { SCENE_F41, ENTRANCE(STONE_TOWER_INVERTED, 0), 0, { 591.0f, 854.0f, -571.0f }, 0, 172, 48,
      "Inverted Stone Tower (Base)", NULL, true },
    { SCENE_F41, ENTRANCE(STONE_TOWER_INVERTED, 0), 0, { 268.0f, 1454.0f, -289.0f }, 0, 178, 41,
      "Inverted Stone Tower (Top)", NULL, true },
    { SCENE_RANDOM, ENTRANCE(SECRET_SHRINE, 0), 0, { 0.0f, 0.0f, 1170.0f }, 180, 165, 66, "Secret Shrine", NULL,
      true },
};

#define BOULDER_ROOM_POINT 5
#define BOULDER_SWITCH_FLAG 0x0E

// The Boulder Room soil lies under an Obj_Bombiwa until it is blown up this cycle, which sets its switch flag.
static bool IsPointUncovered(PlayState* play, s32 index) {
    if (index != BOULDER_ROOM_POINT) {
        return true;
    }
    if (play->sceneId == SCENE_KINSTA1) {
        return Flags_GetSwitch(play, BOULDER_SWITCH_FLAG);
    }
    return (gSaveContext.cycleSceneFlags[SCENE_KINSTA1].switch0 & (1u << BOULDER_SWITCH_FLAG)) != 0;
}

static u32 GetVisitedPoints(void) {
    u32 visited = 0;

    sApi->StorageGet(MINISH_CAP_OWNER, VISITED_STORAGE_KEY, &visited, sizeof(visited));
    return visited;
}

static bool IsPointVisited(s32 index) {
    return (GetVisitedPoints() & (1u << index)) != 0;
}

static bool IsPlayerAtPoint(Player* player, const WarpPoint* point) {
    Vec3f pointPos = point->pos;

    return Math_Vec3f_DistXZ(&player->actor.world.pos, &pointPos) <= SOIL_REACH &&
           fabsf(player->actor.world.pos.y - point->pos.y) <= SOIL_HEIGHT_REACH;
}

static s8 FindSoilUnderPlayer(Player* player, PlayState* play) {
    for (s32 i = 0; i < WARP_POINT_COUNT; i++) {
        const WarpPoint* point = &sWarpPoints[i];

        if (point->isSoil && point->sceneId == play->sceneId && IsPlayerAtPoint(player, point)) {
            return i;
        }
    }
    return -1;
}

// The Spider House door point has no soil: being anywhere in its room counts as visiting it.
static bool IsPlayerVisiting(Player* player, PlayState* play, const WarpPoint* point) {
    if (point->sceneId != play->sceneId) {
        return false;
    }
    if (!point->isSoil) {
        return play->roomCtx.curRoom.num == point->room;
    }
    return IsPlayerAtPoint(player, point);
}

static void MarkVisitedPoints(Player* player, PlayState* play) {
    u32 visited = GetVisitedPoints();
    u32 updated = visited;

    for (s32 i = 0; i < WARP_POINT_COUNT; i++) {
        if (IsPlayerVisiting(player, play, &sWarpPoints[i])) {
            updated |= 1u << i;
        }
    }
    if (updated != visited) {
        sApi->StorageSet(MINISH_CAP_OWNER, VISITED_STORAGE_KEY, &updated, sizeof(updated));
    }
}
