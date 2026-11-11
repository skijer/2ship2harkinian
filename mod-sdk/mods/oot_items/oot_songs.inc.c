// OoT songs MM lacks, on the OoT Collectables page (NEI z_kaleido_collect.c points 6..12). Saria's and the
// Sun's Song are MM quest songs with native items, ocarina slots and effects, so they stay on MM's page.
#define OOT_QUEST_PAGE (S2H_ITEM_PAGE_QUEST_VANILLA + 1)
#define MAX_MELODY_NOTES 9
#define GLYPH_A "\xB0"
#define GLYPH_CUP "\xB6"
#define GLYPH_CDOWN "\xB7"
#define GLYPH_CLEFT "\xB8"
#define GLYPH_CRIGHT "\xB9"

typedef struct {
    const char* key;
    const char* icon;
    const char* fallbackIcon;
    const char* name;
    const char* getItemText;
    const char* pauseText;
    const char* playedText;
    u8 questPoint;
    s16 effectActorId;
    Color_RGB8 noteColor;
    OcarinaNote melody[MAX_MELODY_NOTES];
} OotSong;

// The tinted note is derived from the OoT Asset Loader's gSongNoteTex; the packaged note covers archives
// extracted before it existed.
#define ICON(song)                                                       \
    "__OTR__textures/oot_icon_item_static/gItemIconOotSong" song "Tex", \
        "__OTR__textures/icon_item_custom/gItemIconOotSong" song "Tex"
#define NAME(song) "__OTR__textures/oot_item_name_static/g" song "ItemNameENGTex"

// Melodies are NEI's sOoTOcarinaSongNotes; effects follow NEI Message_SpawnSongEffect (warps flash WIPE4)
// and the note tints are OoT's quest colors, MM rando's for the Lullaby.
static const OotSong sSongs[] = {
    { "oot.song.minuet", ICON("Minuet"), NAME("MinuetOfForest"),
      "You learned the %gMinuet of Forest%w!",
      "%gMinuet of Forest&" GLYPH_A " " GLYPH_CUP " " GLYPH_CLEFT " " GLYPH_CRIGHT " " GLYPH_CLEFT " " GLYPH_CRIGHT,
      "You played the %gMinuet of Forest%w.", 6, ACTOR_OCEFF_WIPE4, { 150, 255, 100 },
      { { OCARINA_PITCH_D4, 18, 86 }, { OCARINA_PITCH_D5, 18, 92 }, { OCARINA_PITCH_B4, 72, 86 },
        { OCARINA_PITCH_A4, 18, 80 }, { OCARINA_PITCH_B4, 18, 88 }, { OCARINA_PITCH_A4, 144, 86 },
        { OCARINA_PITCH_NONE, 0, 86 } } },
    { "oot.song.bolero", ICON("Bolero"), NAME("BoleroOfFire"),
      "You learned the %rBolero of Fire%w!",
      "%rBolero of Fire&" GLYPH_CDOWN " " GLYPH_A " " GLYPH_CDOWN " " GLYPH_A " " GLYPH_CRIGHT " " GLYPH_CDOWN " "
      GLYPH_CRIGHT " " GLYPH_CDOWN,
      "You played the %rBolero of Fire%w.", 7, ACTOR_OCEFF_WIPE4, { 255, 80, 40 },
      { { OCARINA_PITCH_F4, 15, 80 }, { OCARINA_PITCH_D4, 15, 72 }, { OCARINA_PITCH_F4, 15, 84 },
        { OCARINA_PITCH_D4, 15, 76 }, { OCARINA_PITCH_A4, 15, 84 }, { OCARINA_PITCH_F4, 15, 74 },
        { OCARINA_PITCH_A4, 15, 78 }, { OCARINA_PITCH_F4, 135, 66 }, { OCARINA_PITCH_NONE, 0, 66 } } },
    { "oot.song.serenade", ICON("Serenade"), NAME("SerenadeOfWater"),
      "You learned the %bSerenade of Water%w!",
      "%bSerenade of Water&" GLYPH_A " " GLYPH_CDOWN " " GLYPH_CRIGHT " " GLYPH_CRIGHT " " GLYPH_CLEFT,
      "You played the %bSerenade of Water%w.", 8, ACTOR_OCEFF_WIPE4, { 100, 150, 255 },
      { { OCARINA_PITCH_D4, 36, 60 }, { OCARINA_PITCH_F4, 36, 78 }, { OCARINA_PITCH_A4, 33, 82 },
        { OCARINA_PITCH_NONE, 3, 82 }, { OCARINA_PITCH_A4, 36, 84 }, { OCARINA_PITCH_B4, 144, 90 },
        { OCARINA_PITCH_NONE, 0, 90 } } },
    { "oot.song.requiem", ICON("Requiem"), NAME("RequiemOfSpirit"),
      "You learned the %yRequiem of Spirit%w!",
      "%yRequiem of Spirit&" GLYPH_A " " GLYPH_CDOWN " " GLYPH_A " " GLYPH_CRIGHT " " GLYPH_CDOWN " " GLYPH_A,
      "You played the %yRequiem of Spirit%w.", 9, ACTOR_OCEFF_WIPE4, { 255, 160, 0 },
      { { OCARINA_PITCH_D4, 45, 88 }, { OCARINA_PITCH_F4, 23, 86 }, { OCARINA_PITCH_D4, 22, 84 },
        { OCARINA_PITCH_A4, 45, 86 }, { OCARINA_PITCH_F4, 45, 94 }, { OCARINA_PITCH_D4, 180, 94 },
        { OCARINA_PITCH_NONE, 0, 94 } } },
    { "oot.song.nocturne", ICON("Nocturne"), NAME("NocturneOfShadow"),
      "You learned the %pNocturne of Shadow%w!",
      "%pNocturne of Shadow&" GLYPH_CLEFT " " GLYPH_CRIGHT " " GLYPH_CRIGHT " " GLYPH_A " " GLYPH_CLEFT " "
      GLYPH_CRIGHT " " GLYPH_CDOWN,
      "You played the %pNocturne of Shadow%w.", 10, ACTOR_OCEFF_WIPE4, { 255, 100, 255 },
      { { OCARINA_PITCH_B4, 36, 88 }, { OCARINA_PITCH_A4, 33, 84 }, { OCARINA_PITCH_NONE, 3, 84 },
        { OCARINA_PITCH_A4, 18, 82 }, { OCARINA_PITCH_D4, 18, 60 }, { OCARINA_PITCH_B4, 18, 90 },
        { OCARINA_PITCH_A4, 18, 88 }, { OCARINA_PITCH_F4, 144, 96 }, { OCARINA_PITCH_NONE, 0, 96 } } },
    { "oot.song.prelude", ICON("Prelude"), NAME("PreludeOfLight"),
      "You learned the %yPrelude of Light%w!",
      "%yPrelude of Light&" GLYPH_CUP " " GLYPH_CRIGHT " " GLYPH_CUP " " GLYPH_CRIGHT " " GLYPH_CLEFT " " GLYPH_CUP,
      "You played the %yPrelude of Light%w.", 11, ACTOR_OCEFF_WIPE4, { 255, 240, 100 },
      { { OCARINA_PITCH_D5, 15, 84 }, { OCARINA_PITCH_A4, 45, 88 }, { OCARINA_PITCH_D5, 15, 88 },
        { OCARINA_PITCH_A4, 15, 82 }, { OCARINA_PITCH_B4, 15, 86 }, { OCARINA_PITCH_D5, 60, 90 },
        { OCARINA_PITCH_NONE, 75, 90 }, { OCARINA_PITCH_NONE, 0, 90 } } },
    { "oot.song.lullaby", ICON("Lullaby"), NAME("ZeldasLullaby"),
      "You learned %rZelda's Lullaby%w!",
      "%rZelda's Lullaby&" GLYPH_CLEFT " " GLYPH_CUP " " GLYPH_CRIGHT " " GLYPH_CLEFT " " GLYPH_CUP " " GLYPH_CRIGHT,
      "You played %rZelda's Lullaby%w.", 12, ACTOR_OCEFF_WIPE, { 255, 20, 20 },
      { { OCARINA_PITCH_B4, 51, 84 }, { OCARINA_PITCH_D5, 25, 88 }, { OCARINA_PITCH_A4, 78, 80 },
        { OCARINA_PITCH_B4, 51, 84 }, { OCARINA_PITCH_D5, 25, 88 }, { OCARINA_PITCH_A4, 100, 80 },
        { OCARINA_PITCH_NONE, 0, 90 } } },
};
static const ALIGN_ASSET(2) char sSongNoteDL[] = "__OTR__objects/object_gi_melody/gGiSongNoteDL";

static void DrawSongGetItem(PlayState* play, S2HGetItemEntry* entry) {
    const Color_RGB8* color = &sSongs[entry->drawItemId % ARRAY_COUNT(sSongs)].noteColor;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, color->r, color->g, color->b, 255);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSongNoteDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static uint16_t RegisterPlayedText(const OotSong* song) {
    char key[48];
    snprintf(key, sizeof(key), "%s.played", song->key);
    S2HModMessage message = { 0 };
    message.structSize = sizeof(message);
    message.key = key;
    message.text = song->playedText;
    message.textboxType = TEXTBOX_TYPE_3;
    return sApi->RegisterMessage(&message);
}

// The host's song registry gives each song an ocarina slot, so MM plays, checks and shows it as its own.
static void RegisterSongMelody(const OotSong* song) {
    S2HOcarinaSongDefinition definition = { 0 };
    definition.itemKey = song->key;
    definition.notes = song->melody;
    definition.playedMessageId = RegisterPlayedText(song);
    definition.effectActorId = song->effectActorId;
    sApi->RegisterOcarinaSong(&definition);
}

static void RegisterSong(const OotSong* song, u8 index) {
    const S2HCustomItemRandomizer randomizer = {
        sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, song->key, song->key, "",
    };
    const char* icon = sApi->HasResource(song->icon) ? song->icon : song->fallbackIcon;
    S2HCustomItemDefinition item = Z64Items_Define(song->key, icon, song->name);
    Z64Items_SetPlacement(&item, OOT_QUEST_PAGE, song->questPoint, 0);
    Z64Items_SetTextbox(&item, song->getItemText);
    Z64Items_SetPauseText(&item, song->pauseText);
    Z64Items_SetLogic(&item, &randomizer);
    item.getItemEntry.drawItemId = index;
    item.getItemEntry.drawFunc = DrawSongGetItem;
    Z64Items_Register(sApi, &item);
}

static void RegisterOotSongs(void) {
    bool canPlaySongs = S2H_MOD_API_HAS(sApi, RegisterOcarinaSong) && S2H_MOD_API_HAS(sApi, RegisterMessage);
    for (u8 i = 0; i < ARRAY_COUNT(sSongs); i++) {
        RegisterSong(&sSongs[i], i);
        if (canPlaySongs) {
            RegisterSongMelody(&sSongs[i]);
        }
    }
}
