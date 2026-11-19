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
    const char* fanfare;
    const char* fanfareFont;
    Color_RGB8 noteColor;
    OcarinaNote melody[MAX_MELODY_NOTES];
    const char* warpQuestion;
    const char* warpPortal;
} OotSong;

#define SONG_WARP_INPUT_OWNER "oot_items.song_warp"
#define YES_NO "\n\xC2%gYes\nNo%w"

typedef void (*DepartFn)(const char* portal);
typedef enum { SONG_WARP_IDLE, SONG_WARP_PLAYED, SONG_WARP_ASKING } SongWarpStage;
static SongWarpStage sSongWarpStage;
static const OotSong* sWarpSong;

static DepartFn FindDeparture(void) {
    return S2H_MOD_API_HAS(sApi, FindService) ? (DepartFn)sApi->FindService("fleet.depart") : NULL;
}

static void WarpToSpawn(PlayState* play) {
    play->nextEntrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
}

static void TakeSongWarp(PlayState* play, const OotSong* song) {
    if (song->warpPortal == NULL) {
        WarpToSpawn(play);
        return;
    }
    DepartFn depart = FindDeparture();
    if (depart != NULL) {
        depart(song->warpPortal);
    }
}

static void AskSongWarp(PlayState* play) {
    if (!sApi->ShowTextbox(play, sWarpSong->warpQuestion, true)) {
        sSongWarpStage = SONG_WARP_IDLE;
        return;
    }
    sApi->BlockPlayerInput(SONG_WARP_INPUT_OWNER, 0xFFFF, true);
    sSongWarpStage = SONG_WARP_ASKING;
}

static void ResolveSongWarp(PlayState* play) {
    const bool isClosed = play->msgCtx.msgMode == MSGMODE_NONE;
    const bool isChosen = Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE && Message_ShouldAdvance(play);
    if (!isClosed && !isChosen) {
        return;
    }
    sSongWarpStage = SONG_WARP_IDLE;
    sApi->ReleasePlayerInput(SONG_WARP_INPUT_OWNER);
    if (!isChosen) {
        return;
    }
    Message_CloseTextbox(play);
    if (play->msgCtx.choiceIndex == 0) {
        TakeSongWarp(play, sWarpSong);
    }
}

static void TickSongWarp(Player* player) {
    PlayState* play = gPlayState;
    if (play == NULL || player != GET_PLAYER(play)) {
        return;
    }
    switch (sSongWarpStage) {
        case SONG_WARP_PLAYED:
            if (play->msgCtx.msgMode == MSGMODE_NONE) {
                AskSongWarp(play);
            }
            break;
        case SONG_WARP_ASKING:
            ResolveSongWarp(play);
            break;
        default:
            break;
    }
}

#define ICON(song)                                                      \
    "__OTR__textures/oot_icon_item_static/gItemIconOotSong" song "Tex", \
        "__OTR__textures/icon_item_custom/gItemIconOotSong" song "Tex"
#define NAME(song) "__OTR__textures/oot_item_name_static/g" song "ItemNameENGTex"
#define NEI_ICON(song)                                                \
    "__OTR__textures/oot_icon_item_static/gItemIconOotSongCustomTex", \
        "__OTR__textures/icon_item_custom/gItemIcon" song "Tex"
#define NEI_NAME(song) "__OTR__textures/item_name_custom/g" song "NameTex"
#define FANFARE(sequence, font) "audio/oot_sequences/" sequence, "audio/oot_fonts/" font
#define WARP_FANFARE(sequence) FANFARE(sequence, "18_Warp_Songs")
#define NO_FANFARE NULL, NULL

static const OotSong sSongs[] = {
    { "oot.song.minuet",
      ICON("Minuet"),
      NAME("MinuetOfForest"),
      "You learned the %gMinuet of Forest%w!",
      "%gMinuet of Forest&" GLYPH_A " " GLYPH_CUP " " GLYPH_CLEFT " " GLYPH_CRIGHT " " GLYPH_CLEFT " " GLYPH_CRIGHT,
      "You played the %gMinuet of Forest%w.",
      6,
      ACTOR_OCEFF_WIPE4,
      WARP_FANFARE("052_Minuet_of_Forest"),
      { 150, 255, 100 },
      { { OCARINA_PITCH_D4, 18, 86 },
        { OCARINA_PITCH_D5, 18, 92 },
        { OCARINA_PITCH_B4, 72, 86 },
        { OCARINA_PITCH_A4, 18, 80 },
        { OCARINA_PITCH_B4, 18, 88 },
        { OCARINA_PITCH_A4, 144, 86 },
        { OCARINA_PITCH_NONE, 0, 86 } },
      "Warp to the %gSacred Forest Meadow%w?" YES_NO,
      "song.minuet" },
    { "oot.song.bolero",
      ICON("Bolero"),
      NAME("BoleroOfFire"),
      "You learned the %rBolero of Fire%w!",
      "%rBolero of Fire&" GLYPH_CDOWN " " GLYPH_A " " GLYPH_CDOWN " " GLYPH_A " " GLYPH_CRIGHT " " GLYPH_CDOWN
      " " GLYPH_CRIGHT " " GLYPH_CDOWN,
      "You played the %rBolero of Fire%w.",
      7,
      ACTOR_OCEFF_WIPE4,
      WARP_FANFARE("051_Bolero_of_Fire"),
      { 255, 80, 40 },
      { { OCARINA_PITCH_F4, 15, 80 },
        { OCARINA_PITCH_D4, 15, 72 },
        { OCARINA_PITCH_F4, 15, 84 },
        { OCARINA_PITCH_D4, 15, 76 },
        { OCARINA_PITCH_A4, 15, 84 },
        { OCARINA_PITCH_F4, 15, 74 },
        { OCARINA_PITCH_A4, 15, 78 },
        { OCARINA_PITCH_F4, 135, 66 },
        { OCARINA_PITCH_NONE, 0, 66 } },
      "Warp to %rDeath Mountain Crater%w?" YES_NO,
      "song.bolero" },
    { "oot.song.serenade",
      ICON("Serenade"),
      NAME("SerenadeOfWater"),
      "You learned the %bSerenade of Water%w!",
      "%bSerenade of Water&" GLYPH_A " " GLYPH_CDOWN " " GLYPH_CRIGHT " " GLYPH_CRIGHT " " GLYPH_CLEFT,
      "You played the %bSerenade of Water%w.",
      8,
      ACTOR_OCEFF_WIPE4,
      WARP_FANFARE("053_Serenade_of_Water"),
      { 100, 150, 255 },
      { { OCARINA_PITCH_D4, 36, 60 },
        { OCARINA_PITCH_F4, 36, 78 },
        { OCARINA_PITCH_A4, 33, 82 },
        { OCARINA_PITCH_NONE, 3, 82 },
        { OCARINA_PITCH_A4, 36, 84 },
        { OCARINA_PITCH_B4, 144, 90 },
        { OCARINA_PITCH_NONE, 0, 90 } },
      "Warp to %bLake Hylia%w?" YES_NO,
      "song.serenade" },
    { "oot.song.requiem",
      ICON("Requiem"),
      NAME("RequiemOfSpirit"),
      "You learned the %yRequiem of Spirit%w!",
      "%yRequiem of Spirit&" GLYPH_A " " GLYPH_CDOWN " " GLYPH_A " " GLYPH_CRIGHT " " GLYPH_CDOWN " " GLYPH_A,
      "You played the %yRequiem of Spirit%w.",
      9,
      ACTOR_OCEFF_WIPE4,
      WARP_FANFARE("054_Requiem_of_Spirit"),
      { 255, 160, 0 },
      { { OCARINA_PITCH_D4, 45, 88 },
        { OCARINA_PITCH_F4, 23, 86 },
        { OCARINA_PITCH_D4, 22, 84 },
        { OCARINA_PITCH_A4, 45, 86 },
        { OCARINA_PITCH_F4, 45, 94 },
        { OCARINA_PITCH_D4, 180, 94 },
        { OCARINA_PITCH_NONE, 0, 94 } },
      "Warp to the %yDesert Colossus%w?" YES_NO,
      "song.requiem" },
    { "oot.song.nocturne",
      ICON("Nocturne"),
      NAME("NocturneOfShadow"),
      "You learned the %pNocturne of Shadow%w!",
      "%pNocturne of Shadow&" GLYPH_CLEFT " " GLYPH_CRIGHT " " GLYPH_CRIGHT " " GLYPH_A " " GLYPH_CLEFT " " GLYPH_CRIGHT
      " " GLYPH_CDOWN,
      "You played the %pNocturne of Shadow%w.",
      10,
      ACTOR_OCEFF_WIPE4,
      WARP_FANFARE("055_Nocturne_of_Shadow"),
      { 255, 100, 255 },
      { { OCARINA_PITCH_B4, 36, 88 },
        { OCARINA_PITCH_A4, 33, 84 },
        { OCARINA_PITCH_NONE, 3, 84 },
        { OCARINA_PITCH_A4, 18, 82 },
        { OCARINA_PITCH_D4, 18, 60 },
        { OCARINA_PITCH_B4, 18, 90 },
        { OCARINA_PITCH_A4, 18, 88 },
        { OCARINA_PITCH_F4, 144, 96 },
        { OCARINA_PITCH_NONE, 0, 96 } },
      "Warp to the %pGraveyard%w?" YES_NO,
      "song.nocturne" },
    { "oot.song.prelude",
      ICON("Prelude"),
      NAME("PreludeOfLight"),
      "You learned the %yPrelude of Light%w!",
      "%yPrelude of Light&" GLYPH_CUP " " GLYPH_CRIGHT " " GLYPH_CUP " " GLYPH_CRIGHT " " GLYPH_CLEFT " " GLYPH_CUP,
      "You played the %yPrelude of Light%w.",
      11,
      ACTOR_OCEFF_WIPE4,
      WARP_FANFARE("037_Prelude_of_Light"),
      { 255, 240, 100 },
      { { OCARINA_PITCH_D5, 15, 84 },
        { OCARINA_PITCH_A4, 45, 88 },
        { OCARINA_PITCH_D5, 15, 88 },
        { OCARINA_PITCH_A4, 15, 82 },
        { OCARINA_PITCH_B4, 15, 86 },
        { OCARINA_PITCH_D5, 60, 90 },
        { OCARINA_PITCH_NONE, 75, 90 },
        { OCARINA_PITCH_NONE, 0, 90 } },
      "Warp to the %yTemple of Time%w?" YES_NO,
      "song.prelude" },
    { "oot.song.lullaby",
      ICON("Lullaby"),
      NAME("ZeldasLullaby"),
      "You learned %rZelda's Lullaby%w!",
      "%rZelda's Lullaby&" GLYPH_CLEFT " " GLYPH_CUP " " GLYPH_CRIGHT " " GLYPH_CLEFT " " GLYPH_CUP " " GLYPH_CRIGHT,
      "You played %rZelda's Lullaby%w.",
      12,
      ACTOR_OCEFF_WIPE,
      FANFARE("070_Zeldas_Lullaby", "00_Sound_Effects_1"),
      { 255, 20, 20 },
      { { OCARINA_PITCH_B4, 51, 84 },
        { OCARINA_PITCH_D5, 25, 88 },
        { OCARINA_PITCH_A4, 78, 80 },
        { OCARINA_PITCH_B4, 51, 84 },
        { OCARINA_PITCH_D5, 25, 88 },
        { OCARINA_PITCH_A4, 100, 80 },
        { OCARINA_PITCH_NONE, 0, 90 } } },
    { "nei.mm_song_fugue_of_home",
      NEI_ICON("FugueOfHome"),
      NEI_NAME("FugueOfHome"),
      "You learned the %rFugue of Home%w!",
      "%rFugue of Home&" GLYPH_A " " GLYPH_CDOWN " " GLYPH_CRIGHT " " GLYPH_CUP " " GLYPH_CRIGHT " " GLYPH_CLEFT,
      "You played the %rFugue of Home%w.",
      13,
      ACTOR_OCEFF_WIPE4,
      NO_FANFARE,
      { 255, 170, 50 },
      { { OCARINA_PITCH_D4, 10, 86 },
        { OCARINA_PITCH_F4, 10, 86 },
        { OCARINA_PITCH_A4, 10, 88 },
        { OCARINA_PITCH_D5, 10, 88 },
        { OCARINA_PITCH_A4, 10, 88 },
        { OCARINA_PITCH_B4, 40, 90 },
        { OCARINA_PITCH_NONE, 0, 90 } },
      "Return to %rClock Town%w?" YES_NO,
      NULL },
    { "nei.mm_song_ballad_of_hero",
      NEI_ICON("BalladOfHero"),
      NEI_NAME("BalladOfHero"),
      "You learned the %rBallad of Hero%w!",
      "%rBallad of Hero&" GLYPH_A " " GLYPH_CDOWN " " GLYPH_CUP " " GLYPH_CLEFT " " GLYPH_CRIGHT " " GLYPH_CLEFT
      " " GLYPH_CRIGHT,
      "You played the %rBallad of Hero%w.",
      17,
      ACTOR_OCEFF_WIPE4,
      NO_FANFARE,
      { 255, 230, 120 },
      { { OCARINA_PITCH_D4, 19, 86 },
        { OCARINA_PITCH_F4, 19, 86 },
        { OCARINA_PITCH_D5, 58, 90 },
        { OCARINA_PITCH_B4, 20, 88 },
        { OCARINA_PITCH_A4, 39, 90 },
        { OCARINA_PITCH_B4, 19, 88 },
        { OCARINA_PITCH_A4, 58, 92 },
        { OCARINA_PITCH_NONE, 0, 92 } } },
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
    message.icon = 0xFE;
    message.autoFormat = true;
    return sApi->RegisterMessage(&message);
}

static s8 sSongSlots[ARRAY_COUNT(sSongs)];

static const OotSong* FindPlayedSong(u16 slot) {
    for (u8 i = 0; i < ARRAY_COUNT(sSongs); i++) {
        if (sSongSlots[i] == slot) {
            return &sSongs[i];
        }
    }
    return NULL;
}

// Runs inside the song's success, where msgCtx.songPlayed still names its ocarina slot.
static void QueueSongWarp(PlayState* play) {
    const OotSong* song = FindPlayedSong(play->msgCtx.songPlayed);
    if (song == NULL || (song->warpPortal != NULL && FindDeparture() == NULL)) {
        return;
    }
    sWarpSong = song;
    sSongWarpStage = SONG_WARP_PLAYED;
}

extern SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path);

static uint16_t RegisterFanfare(const OotSong* song) {
    bool canRegister = S2H_MOD_API_HAS(sApi, RegisterSequence) && S2H_MOD_API_HAS(sApi, RegisterSoundFont);
    if (!canRegister || song->fanfare == NULL || !sApi->HasResource(song->fanfare)) {
        return 0;
    }
    int32_t font = sApi->RegisterSoundFont(song->fanfareFont);
    SequenceData* sequence = ResourceMgr_LoadSeqPtrByName(song->fanfare);
    if (font < 0 || sequence == NULL) {
        return 0;
    }
    sequence->fonts[0] = (u8)font;
    return sApi->RegisterSequence(song->fanfare);
}

static s8 RegisterSongMelody(const OotSong* song) {
    S2HOcarinaSongDefinition definition = { 0 };
    definition.itemKey = song->key;
    definition.notes = song->melody;
    definition.playedMessageId = RegisterPlayedText(song);
    definition.effectActorId = song->effectActorId;
    definition.fanfareSeqId = RegisterFanfare(song);
    definition.onPlayed = song->warpQuestion != NULL ? QueueSongWarp : NULL;
    return sApi->RegisterOcarinaSong(&definition);
}

static void RegisterSongMelodies(void) {
    for (u8 i = 0; i < ARRAY_COUNT(sSongs); i++) {
        sSongSlots[i] = RegisterSongMelody(&sSongs[i]);
    }
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
        sSongSlots[i] = -1;
    }
    if (!canPlaySongs) {
        return;
    }
    RegisterSongMelodies();
    S2H_REGISTER_HOOK(sApi, OnAudioTablesReady, RegisterSongMelodies);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickSongWarp);
}
