#include "CustomEquipRegistry.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/CustomItemRegistry/KaleidoItemManager.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"
// OPEN_DISPS redeclares these inside its body, which would give them C++ linkage here.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

constexpr uint8_t RowCount = 4;
constexpr uint8_t LevelColumnCount = 3;
constexpr uint8_t PassiveColumn = 0;
constexpr int32_t QuestIconSize = 32;

std::optional<uint8_t> sShownPage;

uint8_t ShownPage() {
    return sShownPage.value_or(S2H_EQUIP_PAGE_VANILLA);
}

bool IsOwnedOnPage(const S2HCustomEquipDefinition* definition, uint8_t page) {
    return definition != nullptr && definition->page == page && CustomEquipRegistry_IsOwned(definition->key);
}

const S2HCustomEquipDefinition* FindClaimOnPage(uint8_t page, uint8_t row, uint8_t column) {
    if (row >= RowCount || column > LevelColumnCount) {
        return nullptr;
    }
    for (uint32_t index = 0; index < CustomEquipRegistry_GetCount(); ++index) {
        const S2HCustomEquipDefinition* definition = CustomEquipRegistry_GetAt(index);
        if (!IsOwnedOnPage(definition, page)) {
            continue;
        }
        const bool matches = column == PassiveColumn
                                 ? definition->slot == S2H_EQUIP_SLOT_UPGRADE && definition->row == row
                                 : definition->slot == row && definition->column == column;
        if (matches) {
            return definition;
        }
    }
    return nullptr;
}

const S2HCustomEquipDefinition* FindClaim(uint8_t row, uint8_t column) {
    const bool isVanillaUpgradeColumn = ShownPage() == S2H_EQUIP_PAGE_VANILLA && column == PassiveColumn;
    if (isVanillaUpgradeColumn) {
        return nullptr;
    }
    return FindClaimOnPage(ShownPage(), row, column);
}

std::vector<uint8_t> VisiblePages() {
    std::vector<uint8_t> pages;
    for (uint32_t index = 0; index < CustomEquipRegistry_GetCount(); ++index) {
        const S2HCustomEquipDefinition* definition = CustomEquipRegistry_GetAt(index);
        if (definition == nullptr || definition->page == S2H_EQUIP_PAGE_VANILLA ||
            !CustomEquipRegistry_IsOwned(definition->key)) {
            continue;
        }
        if (std::find(pages.begin(), pages.end(), definition->page) == pages.end()) {
            pages.push_back(definition->page);
        }
    }
    std::sort(pages.begin(), pages.end());
    return pages;
}

void PlayPauseSfx(u16 sfxId) {
    Audio_PlaySfx(sfxId);
}

std::optional<uint8_t> GetQuestPointSlot(int16_t point) {
    switch (point) {
        case QUEST_SWORD:
            return S2H_EQUIP_SLOT_SWORD;
        case QUEST_SHIELD:
            return S2H_EQUIP_SLOT_SHIELD;
        default:
            return std::nullopt;
    }
}

std::vector<const S2HCustomEquipDefinition*> GetOwnedPieces(uint8_t slot) {
    std::vector<const S2HCustomEquipDefinition*> pieces;
    for (uint32_t index = 0; index < CustomEquipRegistry_GetCount(); ++index) {
        const S2HCustomEquipDefinition* definition = CustomEquipRegistry_GetAt(index);
        if (definition->slot == slot && CustomEquipRegistry_IsOwned(definition->key)) {
            pieces.push_back(definition);
        }
    }
    return pieces;
}

// Walks vanilla -> each wearable piece in registration order -> vanilla, so one button reaches every piece.
void CycleSlotPiece(uint8_t slot) {
    const std::vector<const S2HCustomEquipDefinition*> pieces = GetOwnedPieces(slot);
    const char* wornKey = CustomEquipRegistry_GetWorn(slot);
    auto candidate = pieces.begin();
    if (wornKey != nullptr) {
        candidate = std::find_if(pieces.begin(), pieces.end(),
                                 [wornKey](const S2HCustomEquipDefinition* piece) {
                                     return std::string(piece->key) == wornKey;
                                 });
        candidate = candidate == pieces.end() ? pieces.end() : candidate + 1;
    }
    while (candidate != pieces.end() && !CustomEquipRegistry_IsFormAllowed((*candidate)->key)) {
        ++candidate;
    }

    if (candidate != pieces.end()) {
        CustomEquipRegistry_Wear((*candidate)->key);
    } else if (wornKey != nullptr) {
        CustomEquipRegistry_TakeOff(slot);
    } else {
        PlayPauseSfx(NA_SE_SY_ERROR);
        return;
    }
    PlayPauseSfx(NA_SE_SY_DECIDE);
}

void DescribeWornPiece(PlayState* play, uint8_t slot) {
    const S2HCustomEquipDefinition* worn = CustomEquipRegistry_Find(CustomEquipRegistry_GetWorn(slot));
    if (worn == nullptr || worn->pauseText == nullptr) {
        return;
    }
    S2HCustomItemDefinition description = {};
    description.pauseText = worn->pauseText;
    KaleidoItemManager_OpenPauseDescription(play, &description, play->pauseCtx.cursorYIndex[PAUSE_QUEST]);
}

} // namespace

bool KaleidoEquipManager_IsVanillaPageShown(void) {
    if (sShownPage.has_value()) {
        const std::vector<uint8_t> pages = VisiblePages();
        if (std::find(pages.begin(), pages.end(), *sShownPage) == pages.end()) {
            sShownPage.reset();
        }
    }
    return !sShownPage.has_value();
}

bool KaleidoEquipManager_IsCellClaimed(uint8_t row, uint8_t column) {
    return FindClaim(row, column) != nullptr;
}

void* KaleidoEquipManager_GetCellIcon(uint8_t row, uint8_t column) {
    const S2HCustomEquipDefinition* claim = FindClaim(row, column);
    if (claim == nullptr || claim->iconPath == nullptr) {
        return nullptr;
    }
    return const_cast<char*>(claim->iconPath);
}

bool KaleidoEquipManager_IsCellFormAllowed(uint8_t row, uint8_t column) {
    const S2HCustomEquipDefinition* claim = FindClaim(row, column);
    return claim != nullptr && CustomEquipRegistry_IsFormAllowed(claim->key);
}

bool KaleidoEquipManager_IsCellEquipped(uint8_t row, uint8_t column) {
    const S2HCustomEquipDefinition* claim = FindClaim(row, column);
    if (claim == nullptr) {
        return false;
    }
    return claim->slot == S2H_EQUIP_SLOT_UPGRADE ? CustomEquipRegistry_IsToggleOn(claim->key)
                                                 : CustomEquipRegistry_IsWorn(claim->key);
}

bool KaleidoEquipManager_EquipCell(PlayState* play, uint8_t row, uint8_t column) {
    const S2HCustomEquipDefinition* claim = FindClaim(row, column);
    if (claim == nullptr) {
        return false;
    }
    if (!CustomEquipRegistry_IsFormAllowed(claim->key)) {
        PlayPauseSfx(NA_SE_SY_ERROR);
        return true;
    }

    const bool wasOn = KaleidoEquipManager_IsCellEquipped(row, column);
    if (claim->slot == S2H_EQUIP_SLOT_UPGRADE) {
        CustomEquipRegistry_SetToggle(claim->key, !wasOn);
    } else if (wasOn) {
        CustomEquipRegistry_TakeOff(claim->slot);
    } else {
        CustomEquipRegistry_Wear(claim->key);
    }
    PlayPauseSfx(NA_SE_SY_DECIDE);
    return true;
}

bool KaleidoEquipManager_CycleEquipPage(PlayState* play) {
    const std::vector<uint8_t> pages = VisiblePages();
    if (play == nullptr || pages.empty()) {
        return false;
    }

    auto next = pages.begin();
    if (sShownPage.has_value()) {
        next = std::find(pages.begin(), pages.end(), *sShownPage);
        next = next == pages.end() ? pages.end() : next + 1;
    }
    sShownPage = next == pages.end() ? std::optional<uint8_t>() : std::optional<uint8_t>(*next);
    PlayPauseSfx(NA_SE_SY_WIN_SCROLL_RIGHT);
    return true;
}

void KaleidoEquipManager_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) { sShownPage.reset(); });
}

// C-Right steps the cursor's sword or shield through the owned custom pieces, and C-Up describes the worn one.
void KaleidoEquipManager_Update(PlayState* play) {
    const PauseContext* pauseCtx = &play->pauseCtx;
    const std::optional<uint8_t> slot = GetQuestPointSlot(pauseCtx->cursorPoint[PAUSE_QUEST]);
    const bool isCursorIdle = pauseCtx->state == PAUSE_STATE_MAIN && pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE &&
                              pauseCtx->cursorSpecialPos == 0;
    if (!slot.has_value() || !isCursorIdle || !KaleidoQuestManager_IsVanillaPageShown()) {
        return;
    }

    const u16 pressed = CONTROLLER1(&play->state)->press.button;
    if (CHECK_BTN_ALL(pressed, BTN_CRIGHT)) {
        CycleSlotPiece(*slot);
    } else if (CHECK_BTN_ALL(pressed, BTN_CUP)) {
        DescribeWornPiece(play, *slot);
    }
}

// Outside the anonymous namespace: OPEN_DISPS redeclares the interpolation hooks in its body, and inside one they
// get C++ linkage and fail to link.
void KaleidoEquipManager_DrawPoints(PlayState* play) {
    if (!KaleidoQuestManager_IsVanillaPageShown()) {
        return;
    }
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    PauseContext* pauseCtx = &play->pauseCtx;

    OPEN_DISPS(gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    const uint8_t slots[] = { S2H_EQUIP_SLOT_SWORD, S2H_EQUIP_SLOT_SHIELD };
    const uint8_t points[] = { QUEST_SWORD, QUEST_SHIELD };
    for (size_t index = 0; index < sizeof(slots); ++index) {
        const S2HCustomEquipDefinition* worn = CustomEquipRegistry_Find(CustomEquipRegistry_GetWorn(slots[index]));
        if (worn == nullptr || worn->iconPath == nullptr) {
            continue;
        }
        gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(&pauseCtx->questVtx[points[index] * 4]), 4, 0);
        KaleidoScope_DrawTexQuadRGBA32(gfxCtx, const_cast<char*>(worn->iconPath), QuestIconSize, QuestIconSize, 0);
    }

    CLOSE_DISPS(gfxCtx);
}
