// Ezlo's map: the pause world map with a dot per warp point, and an inset for the Swamp Spider House rooms.

#define MAP_LEFT 25
#define MAP_TOP 26
#define MAP_QUARTERS_PER_TEXEL 5
#define MAP_X(x) ((MAP_LEFT << 2) + (s32)(x) * MAP_QUARTERS_PER_TEXEL)
#define MAP_Y(y) ((MAP_TOP << 2) + (s32)(y) * MAP_QUARTERS_PER_TEXEL)
#define WORLD_MAP_WIDTH 216
#define WORLD_MAP_HEIGHT 128
#define WORLD_MAP_STRIP_HEIGHT 9
#define DOT_HALF_SIZE (4 << 2)
#define CURSOR_HALF_SIZE (6 << 2)
#define STICK_DEADZONE 30.0f

#define NAME_TOP 194
#define HINT_TOP 214
#define NAME_TEX_WIDTH 128
#define NAME_TEX_HEIGHT 16
#define FONT_GLYPH_COUNT 156
#define FONT_SCALE 0.75f
#define GLYPH_SIZE 12
#define BUTTON_A_GLYPH 0xB0
#define BUTTON_B_GLYPH 0xB1

extern HOST_DATA const char* fontTbl[];
extern HOST_DATA f32 sNESFontWidths[];

typedef struct {
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;
} QuarterRect;

static const QuarterRect sSpiderHouseInset = { MAP_X(3), MAP_Y(77), MAP_X(51), MAP_Y(125) };

static bool sIsMapOpen;
static s8 sCursorPoint;
static s8 sOriginPoint;
static bool sWasStickHeld;

static bool CanOpenMap(PlayState* play) {
    return play->msgCtx.msgMode == MSGMODE_NONE && !Play_InCsMode(play) && play->pauseCtx.state == PAUSE_STATE_OFF &&
           play->transitionTrigger == TRANS_TRIGGER_OFF;
}

static void OpenMap(s8 soil) {
    sOriginPoint = soil;
    sCursorPoint = soil;
    sWasStickHeld = true;
    sIsMapOpen = true;
    HoldPlayerStill();
    Audio_PlaySfx(NA_SE_SY_WIN_OPEN);
}

static void CloseMap(void) {
    sIsMapOpen = false;
    ReleasePlayer();
}

static f32 WrapAngle(f32 angle) {
    while (angle > M_PIf) {
        angle -= 2.0f * M_PIf;
    }
    while (angle < -M_PIf) {
        angle += 2.0f * M_PIf;
    }
    return angle;
}

// Map rows grow downward, so a point above the cursor sits toward stick-up.
static s8 FindPointTowardStick(s8 from, s16 stickX, s16 stickY) {
    f32 stickAngle = atan2f((f32)stickX, (f32)stickY);
    s8 best = -1;
    f32 bestScore = 0.0f;

    for (s32 i = 0; i < WARP_POINT_COUNT; i++) {
        f32 deltaX = sWarpPoints[i].mapX - sWarpPoints[from].mapX;
        f32 deltaY = sWarpPoints[from].mapY - sWarpPoints[i].mapY;
        f32 distance = sqrtf(SQ(deltaX) + SQ(deltaY));

        if (i == from || distance < 1.0f) {
            continue;
        }
        f32 angleDifference = fabsf(WrapAngle(atan2f(deltaX, deltaY) - stickAngle));
        if (angleDifference > M_PIf / 2.0f) {
            continue;
        }
        f32 score = cosf(angleDifference) / distance;
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

static void MoveCursor(Input* input) {
    f32 stickMagnitude = sqrtf(SQ((f32)input->rel.stick_x) + SQ((f32)input->rel.stick_y));

    if (stickMagnitude <= STICK_DEADZONE) {
        sWasStickHeld = false;
        return;
    }
    if (sWasStickHeld) {
        return;
    }
    sWasStickHeld = true;
    s8 next = FindPointTowardStick(sCursorPoint, input->rel.stick_x, input->rel.stick_y);
    if (next >= 0) {
        sCursorPoint = next;
        Audio_PlaySfx(NA_SE_SY_CURSOR);
    }
}

static void ChooseDestination(PlayState* play) {
    if (sCursorPoint == sOriginPoint || !IsPointVisited(sCursorPoint) || !IsPointUncovered(play, sCursorPoint)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    sIsMapOpen = false;
    Audio_PlaySfx(NA_SE_SY_DECIDE);
    StartTravel(play, sCursorPoint);
}

static void UpdateMap(PlayState* play) {
    Input* input = &play->state.input[0];

    MoveCursor(input);
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        ChooseDestination(play);
        return;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_B) || CHECK_BTN_ALL(input->press.button, BTN_START)) {
        CloseMap();
        Audio_PlaySfx(NA_SE_SY_CANCEL);
    }
}

static void SetupSolidRects(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetOtherMode(OVERLAY_DISP++,
                    G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_CLD_SURF | G_RM_CLD_SURF2);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void FillRect(PlayState* play, const QuarterRect* rect, Color_RGBA8 color) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, color.r, color.g, color.b, color.a);
    gSPWideTextureRectangle(OVERLAY_DISP++, rect->left, rect->top, rect->right, rect->bottom, G_TX_RENDERTILE, 0, 0, 0,
                            0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void FrameRect(PlayState* play, const QuarterRect* rect, Color_RGBA8 color) {
    const s32 thickness = 1 << 2;
    QuarterRect top = { rect->left, rect->top, rect->right, rect->top + thickness };
    QuarterRect bottom = { rect->left, rect->bottom - thickness, rect->right, rect->bottom };
    QuarterRect left = { rect->left, rect->top, rect->left + thickness, rect->bottom };
    QuarterRect right = { rect->right - thickness, rect->top, rect->right, rect->bottom };

    FillRect(play, &top, color);
    FillRect(play, &bottom, color);
    FillRect(play, &left, color);
    FillRect(play, &right, color);
}

// The texture is loaded by path, a strip of rows at a time, so HD packs can replace it.
static void DrawWorldMap(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetTextureFilter(OVERLAY_DISP++, G_TF_POINT);
    gDPLoadTLUT_pal256(OVERLAY_DISP++, gWorldMapImageTLUT);
    gDPSetTextureLUT(OVERLAY_DISP++, G_TT_RGBA16);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);

    for (s16 firstRow = 0; firstRow < WORLD_MAP_HEIGHT; firstRow += WORLD_MAP_STRIP_HEIGHT) {
        s16 height = MIN(WORLD_MAP_STRIP_HEIGHT, WORLD_MAP_HEIGHT - firstRow);
        s32 top = MAP_Y(firstRow);
        s32 bottom = MAP_Y(firstRow + height);

        gDPLoadMultiTile(OVERLAY_DISP++, gWorldMapImageTex, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b,
                         WORLD_MAP_WIDTH, WORLD_MAP_HEIGHT, 0, firstRow, WORLD_MAP_WIDTH - 1, firstRow + height - 1, 0,
                         G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                         G_TX_NOLOD);
        gDPSetTileSize(OVERLAY_DISP++, G_TX_RENDERTILE, 0, 0, (WORLD_MAP_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                       (height - 1) << G_TEXTURE_IMAGE_FRAC);
        gSPWideTextureRectangle(OVERLAY_DISP++, MAP_X(0), top, MAP_X(WORLD_MAP_WIDTH), bottom, G_TX_RENDERTILE, 0, 0,
                                (1 << 12) / MAP_QUARTERS_PER_TEXEL, (1 << 12) / MAP_QUARTERS_PER_TEXEL);
    }
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSpiderHouseInset(PlayState* play) {
    SetupSolidRects(play);
    FillRect(play, &sSpiderHouseInset, (Color_RGBA8){ 20, 30, 50, 200 });
    FrameRect(play, &sSpiderHouseInset, (Color_RGBA8){ 215, 235, 235, 255 });
}

static QuarterRect GetPointRect(s32 index, s32 halfSize) {
    s32 centerX = MAP_X(sWarpPoints[index].mapX);
    s32 centerY = MAP_Y(sWarpPoints[index].mapY);
    QuarterRect rect = { centerX - halfSize, centerY - halfSize, centerX + halfSize, centerY + halfSize };

    return rect;
}

static void DrawTexturedRect(PlayState* play, const QuarterRect* rect, s16 texWidth, s16 texHeight) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPWideTextureRectangle(OVERLAY_DISP++, rect->left, rect->top, rect->right, rect->bottom, G_TX_RENDERTILE, 0, 0,
                            texWidth * (1 << 12) / (rect->right - rect->left),
                            texHeight * (1 << 12) / (rect->bottom - rect->top));
    CLOSE_DISPS(play->state.gfxCtx);
}

// Visited points use the pause map's region dot colours; the rest are greyed out.
static void DrawPointDots(PlayState* play, u32 visited) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPLoadTextureBlock(OVERLAY_DISP++, gWorldMapDotTex, G_IM_FMT_IA, G_IM_SIZ_8b, 8, 8, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    for (s32 i = 0; i < WARP_POINT_COUNT; i++) {
        QuarterRect rect = GetPointRect(i, DOT_HALF_SIZE);

        if (visited & (1u << i)) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 255, 255);
            gDPSetEnvColor(OVERLAY_DISP++, 255, 255, 0, 0);
        } else {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 70, 70, 70, 255);
            gDPSetEnvColor(OVERLAY_DISP++, 150, 150, 150, 0);
        }
        DrawTexturedRect(play, &rect, 8, 8);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawCursor(PlayState* play) {
    QuarterRect rect = GetPointRect(sCursorPoint, CURSOR_HALF_SIZE);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);
    gDPLoadTextureBlock(OVERLAY_DISP++, sPecoriTex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawTexturedRect(play, &rect, 16, 16);
}

static f32 MeasureText(const char* text) {
    f32 width = 0.0f;

    for (const u8* c = (const u8*)text; *c != '\0'; c++) {
        width += sNESFontWidths[*c - ' '] * FONT_SCALE;
    }
    return width;
}

static void DrawGlyph(PlayState* play, s32 glyph, s32 x, s32 y) {
    QuarterRect rect = { x << 2, y << 2, (x + GLYPH_SIZE) << 2, (y + GLYPH_SIZE) << 2 };

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPLoadTextureBlock_4b(OVERLAY_DISP++, fontTbl[glyph], G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawTexturedRect(play, &rect, 16, 16);
}

static Color_RGBA8 GetGlyphColor(u8 character, Color_RGBA8 textColor) {
    if (character == BUTTON_A_GLYPH) {
        return (Color_RGBA8){ 80, 150, 255, textColor.a };
    }
    if (character == BUTTON_B_GLYPH) {
        return (Color_RGBA8){ 100, 255, 120, textColor.a };
    }
    return textColor;
}

// Drawn with the message font itself, the same glyphs and widths the textboxes use, centred on the screen.
static void DrawCenteredText(PlayState* play, const char* text, s32 y, Color_RGBA8 color) {
    f32 x = (SCREEN_WIDTH - MeasureText(text)) / 2.0f;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    for (const u8* c = (const u8*)text; *c != '\0'; c++) {
        s32 glyph = *c - ' ';
        Color_RGBA8 glyphColor = GetGlyphColor(*c, color);

        if (glyph < 0 || glyph >= FONT_GLYPH_COUNT) {
            continue;
        }
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, color.a);
        DrawGlyph(play, glyph, (s32)x + 1, y + 1);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, glyphColor.r, glyphColor.g, glyphColor.b, glyphColor.a);
        DrawGlyph(play, glyph, (s32)x, y);
        x += sNESFontWidths[glyph] * FONT_SCALE;
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawNameTexture(PlayState* play, const char* nameTex, Color_RGBA8 color) {
    s32 left = (SCREEN_WIDTH - NAME_TEX_WIDTH) / 2;
    QuarterRect rect = { left << 2, NAME_TOP << 2, (left + NAME_TEX_WIDTH) << 2, (NAME_TOP + NAME_TEX_HEIGHT) << 2 };

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, color.r, color.g, color.b, color.a);
    gDPLoadTextureBlock_4b(OVERLAY_DISP++, nameTex, G_IM_FMT_IA, NAME_TEX_WIDTH, NAME_TEX_HEIGHT, 0,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawTexturedRect(play, &rect, NAME_TEX_WIDTH, NAME_TEX_HEIGHT);
}

static void DrawCursorName(PlayState* play, u32 visited) {
    const WarpPoint* point = &sWarpPoints[sCursorPoint];
    bool isVisited = (visited & (1u << sCursorPoint)) != 0;
    Color_RGBA8 color = isVisited ? (Color_RGBA8){ 255, 255, 255, 255 } : (Color_RGBA8){ 130, 130, 130, 255 };

    if (point->nameTex != NULL) {
        DrawNameTexture(play, point->nameTex, color);
    } else {
        DrawCenteredText(play, point->name, NAME_TOP + 2, color);
    }
    if (!isVisited) {
        DrawCenteredText(play, "Not visited yet", HINT_TOP, color);
    } else if (!IsPointUncovered(play, sCursorPoint)) {
        DrawCenteredText(play, "A boulder covers this soil", HINT_TOP, color);
    } else if (sCursorPoint == sOriginPoint) {
        DrawCenteredText(play, "You are here    \xB1 Close", HINT_TOP, color);
    } else {
        DrawCenteredText(play, "\xB0 Travel    \xB1 Close", HINT_TOP, color);
    }
}

static void DrawMap(PlayState* play) {
    static s16 sBlinkTimer;

    if (!sIsMapOpen || play == NULL) {
        return;
    }
    u32 visited = GetVisitedPoints();
    QuarterRect screen = { 0, 0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2 };
    QuarterRect border = { MAP_X(0) - (2 << 2), MAP_Y(0) - (2 << 2), MAP_X(WORLD_MAP_WIDTH) + (2 << 2),
                           MAP_Y(WORLD_MAP_HEIGHT) + (2 << 2) };
    sBlinkTimer++;

    SetupSolidRects(play);
    FillRect(play, &screen, (Color_RGBA8){ 0, 0, 0, 170 });
    FillRect(play, &border, (Color_RGBA8){ 40, 60, 100, 255 });
    DrawWorldMap(play);
    DrawSpiderHouseInset(play);
    DrawPointDots(play, visited);
    if ((sBlinkTimer % 16) < 11) {
        DrawCursor(play);
    }
    DrawCursorName(play, visited);
}
