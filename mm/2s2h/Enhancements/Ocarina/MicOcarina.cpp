// Humming to ocarina songs: YIN pitch detection on a worker thread. A phrase is
// scored against every song the game currently accepts, with the transposition
// fitted out, and the winner is handed to the native recogniser as already played.

#include "MicOcarina.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#ifdef __APPLE__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

extern "C" {
#include "z64.h"
extern PlayState* gPlayState;

// Ocarina recognition gate (code_8019AF00.c). Only ever read from the audio
// thread, which is the thread that owns them.
extern u32 sOcarinaAvailableSongFlags;
extern u8 sFirstOcarinaSongIndex;
extern u8 sLastOcarinaSongIndex;
}

namespace {

constexpr int kSampleRate = 48000;
constexpr int kWindowSize = 2048;
constexpr int kHopSize = 512;
constexpr int kIntegrationSize = kWindowSize / 2;
constexpr int kRingSize = 1 << 15;

// Search range, and the only defence against rumble that costs nothing: fans and
// traffic live below a hummed voice, and nothing above the whistle range is a note.
constexpr int kTauMin = kSampleRate / 1000;
constexpr int kTauMax = kSampleRate / 80;

constexpr float kYinThreshold = 0.15f;
constexpr int kMedianSize = 5;
constexpr int kMaxPhraseNotes = 8;

// A note ends after this much silence. A longer silence drops the rolling note
// window, so an abandoned phrase cannot bleed into the next one.
constexpr int kReleaseHops = 5;
constexpr int kReanchorHops = 90;

// Silence this long after the last note means the player stopped, and the
// phrase is judged as it stands: the runner-up margin is waived and the error
// budget loosens, but nonsense still plays nothing.
constexpr int kPhraseEndHops = 40;
constexpr float kPhraseEndMaxErrorCents = 180.0f;

// "da-da" on one pitch is only two notes if the level dips and recovers.
constexpr float kDipRatio = 0.45f;
constexpr float kRecoverRatio = 0.70f;

// Sustained deviation from the note being held, in cents, before it counts as a
// slide into a new note. The scale's tightest interval is 200 cents (A4 to B4).
constexpr float kJumpCents = 90.0f;
constexpr int kJumpHops = 3;

// A note only commits after this many stable hops (~32 ms), so a click or a
// consonant cannot latch a button.
constexpr int kAttackHops = 3;

// A committed note only enters the phrase once held this long past the attack
// (~130 ms): a glide or a hiccup between two real notes never reaches the matcher.
constexpr int kMinNoteHops = 12;

// The first note of a phrase sets the key, so it has to be sung rather than
// mumbled: it is held about three times longer than any later note.
constexpr int kAnchorHoldHops = 30;

// The gate rides a tracked background level instead of a fixed threshold, so a
// noisy room raises the bar rather than producing notes. The floor follows a
// drop fast and a rise slowly, and never rises while a note is held, or a
// sustained note would drag the floor up behind itself and cut its own tail.
constexpr float kFloorFallRate = 0.25f;
constexpr float kFloorRiseRate = 0.002f;
constexpr float kGateCloseRatio = 0.5f;
constexpr float kNoiseMargin = 4.0f;  // 12 dB over the tracked floor
constexpr float kAbsoluteGate = 0.0056f; // -45 dBFS, the floor under the floor
constexpr float kClarityMin = 0.55f;

// Changes smaller than this are the same relative note. Every real adjacent
// ocarina pitch is at least 200 cents apart.
constexpr float kContourDeadbandCents = 90.0f;

// RMS interval error a phrase may carry and still count as the song, and how far
// clear of the runner-up it must land. Two songs within the margin are a coin
// flip, so nothing fires.
constexpr float kMaxPhraseErrorCents = 120.0f;
constexpr float kMatchMarginCents = 60.0f;
constexpr float kNoMatchError = 1.0e9f;

constexpr int kScaleDegrees[5] = { 0, 3, 7, 9, 12 };
constexpr float kMeterOffsets[5] = { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };

constexpr float kHopSeconds = static_cast<float>(kHopSize) / kSampleRate;
constexpr int kMeterSegmentMax = 24;
constexpr float kMeterWindowSeconds = 6.0f;

struct MeterSegment {
    float startSeconds;
    float endSeconds;
    float cents;
    bool accepted;
};

// Piano roll of the recent notes, written by the worker and copied out whole by
// the draw. A mutex is fine here: the worker is a plain thread, not the capture callback.
struct MeterHistory {
    MeterSegment segments[kMeterSegmentMax];
    int count;
    float nowSeconds;
    float anchorCents;
    bool hasAnchor;
};

float sRing[kRingSize];
std::atomic<uint32_t> sRingWritePos{ 0 };
uint32_t sRingReadPos = 0;

// Which songs the game will accept right now, mirrored off the audio thread so
// the matcher can honour the same gate the native staff check uses.
std::atomic<uint32_t> sAvailableSongFlags{ 0 };
std::atomic<int> sFirstSongIndex{ 0 };
std::atomic<int> sLastSongIndex{ 0 };

std::atomic<int> sMatchedSong{ -1 };
std::atomic<uint32_t> sMatchSequence{ 0 };
std::atomic<bool> sMeterSignalPresent{ false };
std::mutex sMeterMutex;
MeterHistory sMeterHistory;

struct PitchEstimate {
    float hz;
    float clarity;
};

float sDiff[kTauMax + 2];
float sCmndf[kTauMax + 2];

// Ten native MM quest songs plus the twelve songs on the mirrored OoT page.
// Shared OoT songs use their existing MM slots; the other nine use the added
// OoT/custom slots.
constexpr int kRecognisedSongs[] = {
    OCARINA_SONG_SONATA,
    OCARINA_SONG_GORON_LULLABY,
    OCARINA_SONG_NEW_WAVE,
    OCARINA_SONG_ELEGY,
    OCARINA_SONG_OATH,
    OCARINA_SONG_TIME,
    OCARINA_SONG_HEALING,
    OCARINA_SONG_EPONAS,
    OCARINA_SONG_SOARING,
    OCARINA_SONG_STORMS,
    OCARINA_SONG_OOT_MINUET,
    OCARINA_SONG_OOT_BOLERO,
    OCARINA_SONG_OOT_SERENADE,
    OCARINA_SONG_OOT_REQUIEM,
    OCARINA_SONG_OOT_NOCTURNE,
    OCARINA_SONG_OOT_PRELUDE,
    OCARINA_SONG_ZELDAS_LULLABY,
    OCARINA_SONG_NEI_FUGUE_OF_HOME,
    OCARINA_SONG_SARIAS,
    OCARINA_SONG_SUNS,
    OCARINA_SONG_NEI_COMMAND_MELODY,
    OCARINA_SONG_NEI_BALLAD_OF_HERO,
};

float ComputeRms(const float* window, int count) {
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += window[i] * window[i];
    }
    return sqrtf(sum / count);
}

// d(tau) = e(0) + e(tau) - 2 r(tau), evaluated directly: at this window size the
// O(W * tau) loop runs in well under a millisecond, cheaper than owning an FFT.
PitchEstimate EstimatePitch(const float* window) {
    for (int tau = 1; tau <= kTauMax; tau++) {
        float sum = 0.0f;
        for (int j = 0; j < kIntegrationSize; j++) {
            float delta = window[j] - window[j + tau];
            sum += delta * delta;
        }
        sDiff[tau] = sum;
    }

    // Cumulative mean normalisation: dividing by the running mean of d makes the
    // curve dimensionless (so kYinThreshold is absolute) and removes the bias
    // that otherwise picks tau too small.
    float runningSum = 0.0f;
    sCmndf[0] = 1.0f;
    for (int tau = 1; tau <= kTauMax; tau++) {
        runningSum += sDiff[tau];
        sCmndf[tau] = (runningSum > 0.0f) ? (sDiff[tau] * tau / runningSum) : 1.0f;
    }

    // First local minimum below the threshold, not the global one: the true
    // period dips below it and so do its multiples, so taking the first dip is
    // what keeps the fundamental instead of an octave-down answer.
    int best = -1;
    for (int tau = kTauMin; tau < kTauMax; tau++) {
        if (sCmndf[tau] >= kYinThreshold) {
            continue;
        }
        while ((tau + 1 < kTauMax) && (sCmndf[tau + 1] < sCmndf[tau])) {
            tau++;
        }
        best = tau;
        break;
    }
    if (best < 0) {
        return { 0.0f, 0.0f };
    }

    float prev = sCmndf[best - 1];
    float curr = sCmndf[best];
    float next = sCmndf[best + 1];
    float denom = prev - (2.0f * curr) + next;
    float tauStar = static_cast<float>(best);
    if (denom != 0.0f) {
        tauStar += 0.5f * (prev - next) / denom;
    }
    if (tauStar < 1.0f) {
        return { 0.0f, 0.0f };
    }
    return { kSampleRate / tauStar, 1.0f - curr };
}

float MedianOf(const float* values, int count) {
    float sorted[kMedianSize];
    memcpy(sorted, values, count * sizeof(float));
    for (int i = 1; i < count; i++) {
        float key = sorted[i];
        int j = i - 1;
        while ((j >= 0) && (sorted[j] > key)) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    return sorted[count / 2];
}

struct NoteTracker {
    float centsHistory[kMedianSize];
    int historyCount;
    float anchorCents;
    bool hasAnchor;
    float phraseCents[kMaxPhraseNotes];
    int phraseCount;
    float noteCents;
    bool noteActive;
    bool noteInPhrase;
    int noteHops;
    float notePeakRms;
    bool sawDip;
    int silentHops;
    int deviationHops;
    float pendingCents;
    float pendingRms;
    int pendingHops;
    float noiseFloor;
};

NoteTracker sTracker;

void ClearMeter() {
    std::lock_guard<std::mutex> lock(sMeterMutex);
    sMeterHistory.count = 0;
    sMeterHistory.hasAnchor = false;
}

void BeginMeterSegment(float cents) {
    std::lock_guard<std::mutex> lock(sMeterMutex);
    if (sMeterHistory.count >= kMeterSegmentMax) {
        memmove(sMeterHistory.segments, &sMeterHistory.segments[1], (kMeterSegmentMax - 1) * sizeof(MeterSegment));
        sMeterHistory.count = kMeterSegmentMax - 1;
    }
    float now = sMeterHistory.nowSeconds;
    sMeterHistory.segments[sMeterHistory.count++] = { now, now, cents, false };
}

void AcceptMeterSegment(float cents, float anchorCents) {
    std::lock_guard<std::mutex> lock(sMeterMutex);
    if (sMeterHistory.count == 0) {
        return;
    }
    MeterSegment& live = sMeterHistory.segments[sMeterHistory.count - 1];
    live.cents = cents;
    live.accepted = true;
    sMeterHistory.anchorCents = anchorCents;
    sMeterHistory.hasAnchor = true;
}

// Once per hop, silence included, so the roll keeps scrolling and the held
// note's bar keeps stretching.
void AdvanceMeter(bool noteHeld) {
    std::lock_guard<std::mutex> lock(sMeterMutex);
    sMeterHistory.nowSeconds += kHopSeconds;
    if (noteHeld && (sMeterHistory.count > 0)) {
        sMeterHistory.segments[sMeterHistory.count - 1].endSeconds = sMeterHistory.nowSeconds;
    }
}

void ResetTracker() {
    memset(&sTracker, 0, sizeof(sTracker));
    sMatchedSong.store(-1, std::memory_order_relaxed);
    sMatchSequence.fetch_add(1, std::memory_order_release);
    ClearMeter();
}

// The custom block carries no bitmask bit and the vanilla loops stop before it,
// so the mic is the only route those songs have.
bool IsSongAccepted(int songIndex) {
    if (songIndex >= OCARINA_SONG_NEI_CUSTOM_FIRST) {
        return true;
    }
    if ((songIndex < sFirstSongIndex.load(std::memory_order_relaxed)) ||
        (songIndex >= sLastSongIndex.load(std::memory_order_relaxed))) {
        return false;
    }
    return (sAvailableSongFlags.load(std::memory_order_relaxed) & (1u << songIndex)) != 0;
}

// RMS distance in cents between what was heard and the song, after removing the
// transposition that best fits. Subtracting the mean residual IS that best fit,
// and it is what makes the score independent of the key the player hums in.
float PhraseError(const float* cents, int count, int songIndex) {
    const OcarinaSongButtons& song = gOcarinaSongButtons[songIndex];
    if ((count < 2) || (song.numButtons != count)) {
        return kNoMatchError;
    }

    float residual[kMaxPhraseNotes];
    float mean = 0.0f;
    for (int i = 0; i < count; i++) {
        int degree = song.buttonIndex[i];
        if ((degree < OCARINA_BTN_A) || (degree > OCARINA_BTN_C_UP)) {
            return kNoMatchError;
        }
        residual[i] = cents[i] - (100.0f * kScaleDegrees[degree]);
        mean += residual[i];
    }
    mean /= count;

    float sumSquared = 0.0f;
    for (int i = 0; i < count; i++) {
        float error = residual[i] - mean;
        sumSquared += error * error;
    }
    return sqrtf(sumSquared / count);
}

// Vibrato or a breath can split one held note in two. Merging adjacent notes
// closer than a real interval recovers that, but it also destroys the genuine
// repeats in Serenade and Nocturne, so this is scored as an alternative
// hypothesis rather than applied to the phrase.
int BuildMergedPhrase(const float* cents, int count, float* merged) {
    int write = 0;
    for (int read = 0; read < count; read++) {
        if ((write > 0) && (fabsf(cents[read] - merged[write - 1]) < kContourDeadbandCents)) {
            continue;
        }
        merged[write++] = cents[read];
    }
    return write;
}

// Scores the song against the newest notes only. Two hypotheses: the notes as
// segmented, and a slightly wider window with split notes merged, which recovers
// a note the vibrato guard cut in half.
float SuffixError(int songIndex, int length) {
    if ((length < 2) || (length > sTracker.phraseCount)) {
        return kNoMatchError;
    }
    float raw = PhraseError(&sTracker.phraseCents[sTracker.phraseCount - length], length, songIndex);

    int widened = (length + 2 <= sTracker.phraseCount) ? (length + 2) : sTracker.phraseCount;
    float merged[kMaxPhraseNotes];
    int mergedCount = BuildMergedPhrase(&sTracker.phraseCents[sTracker.phraseCount - widened], widened, merged);
    if (mergedCount < length) {
        return raw;
    }
    return fminf(raw, PhraseError(&merged[mergedCount - length], length, songIndex));
}

struct MatchCandidates {
    int bestSong;
    float bestError;
    float runnerUpError;
};

MatchCandidates RankSongs() {
    MatchCandidates ranked = { -1, kNoMatchError, kNoMatchError };
    for (int songIndex : kRecognisedSongs) {
        if (!IsSongAccepted(songIndex)) {
            continue;
        }
        float error = SuffixError(songIndex, gOcarinaSongButtons[songIndex].numButtons);
        if (error < ranked.bestError) {
            ranked.runnerUpError = ranked.bestError;
            ranked.bestError = error;
            ranked.bestSong = songIndex;
        } else if (error < ranked.runnerUpError) {
            ranked.runnerUpError = error;
        }
    }
    return ranked;
}

void PublishMatch(int songIndex, float error) {
    sMatchedSong.store(songIndex, std::memory_order_relaxed);
    sMatchSequence.fetch_add(1, std::memory_order_release);
    sTracker.phraseCount = 0;
    SPDLOG_DEBUG("MicOcarina: matched song {} at {} cents RMS", songIndex, error);
}

// Run after every accepted note rather than after a silence, so recognition
// lands as the phrase ends. It also makes a spurious note survivable: it only
// shifts the window instead of ruining a whole fixed-length phrase.
void TryMatchSuffix() {
    MatchCandidates ranked = RankSongs();
    if ((ranked.bestSong < 0) || (ranked.bestError > kMaxPhraseErrorCents)) {
        return;
    }
    if ((ranked.runnerUpError - ranked.bestError) < kMatchMarginCents) {
        SPDLOG_DEBUG("MicOcarina: ambiguous, {} and runner-up within {} cents", ranked.bestSong,
                     ranked.runnerUpError - ranked.bestError);
        return;
    }
    PublishMatch(ranked.bestSong, ranked.bestError);
}

void TryMatchPhraseEnd() {
    MatchCandidates ranked = RankSongs();
    if ((ranked.bestSong < 0) || (ranked.bestError > kPhraseEndMaxErrorCents)) {
        return;
    }
    PublishMatch(ranked.bestSong, ranked.bestError);
}

// A rolling window of the last notes, never cleared on overflow: the oldest note
// is dropped so the newest phrase always stays matchable.
void PushPhraseNote(float cents) {
    if (!sTracker.hasAnchor) {
        sTracker.anchorCents = cents;
        sTracker.hasAnchor = true;
    }
    if (sTracker.phraseCount >= kMaxPhraseNotes) {
        memmove(sTracker.phraseCents, &sTracker.phraseCents[1], (kMaxPhraseNotes - 1) * sizeof(float));
        sTracker.phraseCount = kMaxPhraseNotes - 1;
    }
    sTracker.phraseCents[sTracker.phraseCount++] = cents;
}

// The key is anchored by the first note held long enough, never by a candidate
// or a blip: neither may decide the whole phrase's key.
void AcceptHeldNote() {
    sTracker.noteInPhrase = true;
    PushPhraseNote(sTracker.noteCents);
    AcceptMeterSegment(sTracker.noteCents, sTracker.anchorCents);
    TryMatchSuffix();
}

void CommitNote(float cents, float rms) {
    BeginMeterSegment(cents);
    sTracker.noteCents = cents;
    sTracker.noteActive = true;
    sTracker.noteInPhrase = false;
    sTracker.noteHops = 0;
    sTracker.notePeakRms = rms;
    sTracker.sawDip = false;
    sTracker.deviationHops = 0;
    sTracker.pendingHops = 0;
}

// Holds the candidate until it survives kAttackHops without wandering, then
// commits it as a real note.
void ProposeNote(float cents, float rms) {
    if ((sTracker.pendingHops > 0) && (fabsf(cents - sTracker.pendingCents) > kJumpCents)) {
        sTracker.pendingHops = 0;
    }
    if (sTracker.pendingHops == 0) {
        sTracker.pendingCents = cents;
        sTracker.pendingRms = rms;
    }
    sTracker.pendingRms = fmaxf(sTracker.pendingRms, rms);
    sTracker.pendingHops++;
    if (sTracker.pendingHops >= kAttackHops) {
        CommitNote(cents, sTracker.pendingRms);
    }
}

void EndNote() {
    sTracker.noteActive = false;
    sTracker.historyCount = 0;
    sTracker.pendingHops = 0;
}

void ProcessWindow(const float* window) {
    AdvanceMeter(sTracker.noteActive);
    float rms = ComputeRms(window, kWindowSize);

    if (rms < sTracker.noiseFloor) {
        sTracker.noiseFloor += kFloorFallRate * (rms - sTracker.noiseFloor);
    } else if (!sTracker.noteActive) {
        sTracker.noiseFloor += kFloorRiseRate * (rms - sTracker.noiseFloor);
    }

    // Hysteresis: a note already running only has to clear half the opening
    // level, so a wavering hum is not chopped into pieces at the threshold.
    float openLevel = fmaxf(kAbsoluteGate, sTracker.noiseFloor * kNoiseMargin);
    float level = sTracker.noteActive ? (openLevel * kGateCloseRatio) : openLevel;

    PitchEstimate estimate = { 0.0f, 0.0f };
    if (rms > level) {
        estimate = EstimatePitch(window);
    }
    sMeterSignalPresent.store(rms > 1.0e-6f, std::memory_order_relaxed);

    bool voiced = (estimate.clarity > kClarityMin) && (estimate.hz > 0.0f);
    if (!voiced) {
        sTracker.silentHops++;
        if (sTracker.noteActive && (sTracker.silentHops >= kReleaseHops)) {
            EndNote();
        }
        if (sTracker.silentHops == kPhraseEndHops) {
            TryMatchPhraseEnd();
        }
        if (sTracker.silentHops >= kReanchorHops) {
            sTracker.hasAnchor = false;
            sTracker.phraseCount = 0;
            ClearMeter();
        }
        return;
    }
    sTracker.silentHops = 0;

    float cents = 1200.0f * log2f(estimate.hz / 440.0f);
    if (sTracker.historyCount < kMedianSize) {
        sTracker.centsHistory[sTracker.historyCount++] = cents;
    } else {
        memmove(sTracker.centsHistory, &sTracker.centsHistory[1], (kMedianSize - 1) * sizeof(float));
        sTracker.centsHistory[kMedianSize - 1] = cents;
    }
    float smoothed = MedianOf(sTracker.centsHistory, sTracker.historyCount);

    if (!sTracker.noteActive) {
        ProposeNote(smoothed, rms);
        return;
    }

    sTracker.notePeakRms = fmaxf(sTracker.notePeakRms, rms);
    if (rms < (kDipRatio * sTracker.notePeakRms)) {
        sTracker.sawDip = true;
    }
    if (sTracker.sawDip && (rms > (kRecoverRatio * sTracker.notePeakRms))) {
        CommitNote(smoothed, rms);
        return;
    }

    if (fabsf(smoothed - sTracker.noteCents) > kJumpCents) {
        sTracker.deviationHops++;
        if (sTracker.deviationHops >= kJumpHops) {
            CommitNote(smoothed, rms);
        }
        return;
    }
    sTracker.deviationHops = 0;

    // Follow slow drift within the note so a long vowel does not accumulate into
    // a false slide, but keep the published button pinned to the committed pitch.
    sTracker.noteCents += 0.15f * (smoothed - sTracker.noteCents);

    sTracker.noteHops++;
    int requiredHops = sTracker.hasAnchor ? kMinNoteHops : kAnchorHoldHops;
    if (!sTracker.noteInPhrase && (sTracker.noteHops >= requiredHops)) {
        AcceptHeldNote();
    }
}

SDL_AudioDeviceID sCaptureDevice = 0;
std::thread sWorker;
std::atomic<bool> sWorkerRunning{ false };
std::atomic<bool> sRequested{ false };
int sIdleFrames = 0;
int sOpenRetryFrames = 0;

void CaptureCallback(void* userData, Uint8* stream, int len) {
    const float* samples = reinterpret_cast<const float*>(stream);
    int count = len / static_cast<int>(sizeof(float));
    uint32_t write = sRingWritePos.load(std::memory_order_relaxed);
    for (int i = 0; i < count; i++) {
        sRing[(write + i) & (kRingSize - 1)] = samples[i];
    }
    sRingWritePos.store(write + count, std::memory_order_release);
}

void RunWorker() {
    float window[kWindowSize];
    while (sWorkerRunning.load(std::memory_order_acquire)) {
        uint32_t write = sRingWritePos.load(std::memory_order_acquire);
        if ((write - sRingReadPos) < kWindowSize) {
            SDL_Delay(4);
            continue;
        }
        if ((write - sRingReadPos) > (kRingSize / 2)) {
            sRingReadPos = write - kWindowSize;
        }
        for (int i = 0; i < kWindowSize; i++) {
            window[i] = sRing[(sRingReadPos + i) & (kRingSize - 1)];
        }
        sRingReadPos += kHopSize;
        ProcessWindow(window);
    }
}

bool OpenCaptureDevice() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SPDLOG_WARN("MicOcarina: SDL audio subsystem unavailable: {}", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want;
    memset(&want, 0, sizeof(want));
    want.freq = kSampleRate;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = kHopSize;
    want.callback = CaptureCallback;

    // allowed_changes = 0 makes SDL convert to the spec above, so the DSP can
    // assume mono float at kSampleRate whatever the device natively offers.
    SDL_AudioSpec have;
    sCaptureDevice = SDL_OpenAudioDevice(nullptr, 1, &want, &have, 0);
    if (sCaptureDevice == 0) {
        SPDLOG_WARN("MicOcarina: could not open capture device: {}", SDL_GetError());
        return false;
    }

    ResetTracker();
    sRingReadPos = sRingWritePos.load(std::memory_order_acquire);
    sWorkerRunning.store(true, std::memory_order_release);
    sWorker = std::thread(RunWorker);
    SDL_PauseAudioDevice(sCaptureDevice, 0);
    return true;
}

// Never SDL_QuitSubSystem(SDL_INIT_AUDIO) here: the game's own audio output runs
// on the same subsystem and would go silent.
void CloseCaptureDevice() {
    if (sCaptureDevice == 0) {
        return;
    }
    sWorkerRunning.store(false, std::memory_order_release);
    if (sWorker.joinable()) {
        sWorker.join();
    }
    SDL_CloseAudioDevice(sCaptureDevice);
    sCaptureDevice = 0;
    ResetTracker();
}

float SemisToY(float originY, float semis, float pixelsPerSemi) {
    return originY - (semis * pixelsPerSemi);
}

class MicOcarinaMeterWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
    void Draw() override;
};

void MicOcarinaMeterWindow::Draw() {
    if (sCaptureDevice == 0 || gPlayState == nullptr) {
        return;
    }
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    if (gui->GetMenuOrMenubarVisible()) {
        return;
    }

    MeterHistory history;
    {
        std::lock_guard<std::mutex> lock(sMeterMutex);
        history = sMeterHistory;
    }

    ImVec2 display = ImGui::GetIO().DisplaySize;
    float pixelsPerSemi = display.y / 48.0f;
    float originY = display.y * 0.5f;
    float pixelsPerSecond = display.x / kMeterWindowSeconds;
    float barHalfHeight = display.y / 360.0f;
    ImDrawList* draw = ImGui::GetForegroundDrawList();

    for (int i = 0; i < 5; i++) {
        float y = SemisToY(originY, kMeterOffsets[i], pixelsPerSemi);
        ImU32 color = (i == 2) ? IM_COL32(0x7C, 0xE8, 0x8A, 70) : IM_COL32(0xFF, 0xFF, 0xFF, 30);
        draw->AddLine(ImVec2(0.0f, y), ImVec2(display.x, y), color, 1.0f);
    }

    if (!history.hasAnchor) {
        const char* hint = sMeterSignalPresent.load(std::memory_order_relaxed) ? "hold a note to set the key" : "no mic signal";
        draw->AddText(ImVec2(display.x * 0.02f, originY + pixelsPerSemi), IM_COL32(0xFF, 0xD5, 0x66, 220), hint);
    }

    // Bars are born at the left edge and stream right as they age, so a held
    // note draws itself left to right. White until it counts, green once it does.
    for (int i = 0; i < history.count; i++) {
        const MeterSegment& segment = history.segments[i];
        float newestX = (history.nowSeconds - segment.endSeconds) * pixelsPerSecond;
        if (newestX >= display.x) {
            continue;
        }
        float oldestX = fminf(display.x, (history.nowSeconds - segment.startSeconds) * pixelsPerSecond);
        float semis = history.hasAnchor ? ((segment.cents - history.anchorCents) / 100.0f) : 0.0f;
        semis = fminf(fmaxf(semis, -12.5f), 12.5f);
        float y = SemisToY(originY, semis, pixelsPerSemi);
        bool live = (i == history.count - 1) && (segment.endSeconds >= history.nowSeconds);
        ImU32 color = segment.accepted ? IM_COL32(0x7C, 0xE8, 0x8A, live ? 200 : 120)
                                       : IM_COL32(0xFF, 0xFF, 0xFF, live ? 150 : 60);
        draw->AddRectFilled(ImVec2(newestX, y - barHalfHeight), ImVec2(oldestX, y + barHalfHeight), color,
                            barHalfHeight);
    }
}

std::shared_ptr<MicOcarinaMeterWindow> sMeterWindow = nullptr;

void EnsureMeterRegistered() {
    if (sMeterWindow != nullptr) {
        return;
    }
    auto ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr) {
        return;
    }
    auto window = ctx->GetWindow();
    if (window == nullptr) {
        return;
    }
    auto gui = window->GetGui();
    if (gui == nullptr) {
        return;
    }
    sMeterWindow = std::make_shared<MicOcarinaMeterWindow>("gMicOcarinaMeter", "Mic Ocarina Meter");
    gui->AddGuiWindow(sMeterWindow);
}

// Owns the capture device and the one CVar, on the main thread: the ocarina hook
// runs on the port's audio thread and may only exchange atomics. A stale request
// means the ocarina was put away, so the mic is freed.
void MicOcarinaTick() {
    EnsureMeterRegistered();

    if (sRequested.exchange(false, std::memory_order_relaxed)) {
        sIdleFrames = 0;
    } else {
        sIdleFrames++;
    }

    if (sOpenRetryFrames > 0) {
        sOpenRetryFrames--;
    }

    bool wanted = CVarGetInteger("gEnhancements.MicOcarina.Enabled", 0) && (sIdleFrames < 60);
    if (wanted && (sCaptureDevice == 0)) {
        if ((sOpenRetryFrames == 0) && !OpenCaptureDevice()) {
            sOpenRetryFrames = 300;
        }
        return;
    }
    if (!wanted && (sCaptureDevice != 0)) {
        CloseCaptureDevice();
    }
}

uint32_t sLastSeenMatch = 0;

} // namespace

extern "C" void MicOcarina_Update(void) {
    // Runs on the port's audio thread, so it may only exchange atomics with the
    // worker: opening the device, reading CVars and drawing happen in
    // MicOcarinaTick. The ocarina globals below are owned by this same thread.
    sRequested.store(true, std::memory_order_relaxed);
    sAvailableSongFlags.store(sOcarinaAvailableSongFlags, std::memory_order_relaxed);
    sFirstSongIndex.store(sFirstOcarinaSongIndex, std::memory_order_relaxed);
    sLastSongIndex.store(sLastOcarinaSongIndex, std::memory_order_relaxed);

    uint32_t sequence = sMatchSequence.load(std::memory_order_acquire);
    if (sequence == sLastSeenMatch) {
        return;
    }
    sLastSeenMatch = sequence;

    int song = sMatchedSong.load(std::memory_order_relaxed);
    if ((song < 0) || (song >= OCARINA_SONG_MAX)) {
        return;
    }

    // The player already performed the song, so the staff never needs to hear it:
    // hand it straight to the native recognition flow, which runs the fanfare and
    // the song's real effect.
    AudioOcarina_ForceSongPlayed((u8)song);
}

static void RegisterMicOcarina() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnInterfaceDrawStart>([]() { MicOcarinaTick(); });
}

static RegisterShipInitFunc initFunc(RegisterMicOcarina, {});
