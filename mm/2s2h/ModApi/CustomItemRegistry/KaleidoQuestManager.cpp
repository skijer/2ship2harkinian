#include "KaleidoItemManager.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <cstring>

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/classes.h>

#include "2s2h/GameInteractor/GameInteractor.h"

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

static void QuestWheel(PlayState* play);

namespace {

constexpr uint8_t QuestPointCount = 25;
std::map<std::pair<uint16_t,uint8_t>,std::string> sQuestSelection;
int sQuestHold = 0;
constexpr int32_t QuestIconSize = 32;
constexpr size_t FirstDpadButtonIndex = 3;

struct QuestEquipButton {
    u16 padButton;
    uint8_t heldButton;
};

constexpr std::array<QuestEquipButton, 7> EquipButtons = { {
    { BTN_CLEFT, S2H_ITEM_BUTTON_C_FIRST },
    { BTN_CDOWN, S2H_ITEM_BUTTON_C_FIRST + 1 },
    { BTN_CRIGHT, S2H_ITEM_BUTTON_C_LAST },
    { BTN_DUP, DPAD_TO_HELD_ITEM(EQUIP_SLOT_D_UP) },
    { BTN_DDOWN, DPAD_TO_HELD_ITEM(EQUIP_SLOT_D_DOWN) },
    { BTN_DLEFT, DPAD_TO_HELD_ITEM(EQUIP_SLOT_D_LEFT) },
    { BTN_DRIGHT, DPAD_TO_HELD_ITEM(EQUIP_SLOT_D_RIGHT) },
} };

std::optional<uint16_t> sShownExtraPage;

uint16_t ShownPage() {
    return sShownExtraPage.value_or(S2H_ITEM_PAGE_QUEST_VANILLA);
}

const S2HCustomItemDefinition* FindClaimOnPage(uint16_t page, uint8_t point) {
    if (point >= QuestPointCount) {
        return nullptr;
    }
    const S2HCustomItemDefinition* fallback = nullptr;
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); ++index) {
        const S2HCustomItemDefinition* definition = CustomItemRegistry_GetAt(index);
        S2HCustomItemPlacement placement;
        if (definition == nullptr || !CustomItemRegistry_IsOwned(definition->key) ||
            !KaleidoItemManager_GetPlacement(definition->key, &placement)) {
            continue;
        }
        if (placement.page == page && placement.slot == point) {
            const std::string setting = "gEnhancements.CustomItemManager.Wheel." + std::to_string(page) + "." + std::to_string(point);
            const char* key = CVarGetString(setting.c_str(), "");
            if (*key == 0 || std::strcmp(key,definition->key) == 0) return definition;
            if (fallback == nullptr) fallback = definition;
        }
    }
    return fallback;
}

std::vector<uint16_t> VisibleExtraPages() {
    std::vector<uint16_t> pages;
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); ++index) {
        const S2HCustomItemDefinition* definition = CustomItemRegistry_GetAt(index);
        S2HCustomItemPlacement placement;
        if (definition == nullptr || !KaleidoItemManager_GetPlacement(definition->key, &placement)) {
            continue;
        }
        if (!CustomItemRegistry_IsQuestPage(placement.page) || placement.page == S2H_ITEM_PAGE_QUEST_VANILLA) {
            continue;
        }
        if (std::find(pages.begin(), pages.end(), placement.page) == pages.end()) {
            pages.push_back(placement.page);
        }
    }
    std::sort(pages.begin(), pages.end());
    return pages;
}

// What MM's quest status page paints at each cursor point, which a custom icon must not be drawn over.
bool DoesVanillaDrawPoint(uint8_t point) {
    switch (point) {
        case QUEST_SHIELD:
            return GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) != EQUIP_VALUE_SHIELD_NONE;
        case QUEST_SWORD:
            return GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE;
        case QUEST_QUIVER:
            return GET_CUR_UPG_VALUE(UPG_QUIVER) != 0;
        case QUEST_BOMB_BAG:
            return GET_CUR_UPG_VALUE(UPG_BOMB_BAG) != 0;
        case QUEST_SKULL_TOKEN:
            return false;
        case QUEST_HEART_PIECE:
            return (GET_SAVE_INVENTORY_QUEST_ITEMS >> QUEST_HEART_PIECE_COUNT) != 0;
        case QUEST_SONG_LULLABY:
            return CHECK_QUEST_ITEM(QUEST_SONG_LULLABY) || CHECK_QUEST_ITEM(QUEST_SONG_LULLABY_INTRO);
        default:
            return CHECK_QUEST_ITEM(point);
    }
}

bool IsDrawnByVanilla(uint8_t point) {
    return KaleidoQuestManager_IsVanillaPageShown() && DoesVanillaDrawPoint(point);
}

const S2HCustomItemDefinition* FindDrawnClaim(uint8_t point) {
    return FindClaimOnPage(ShownPage(), point);
}

void PlayPauseSfx(u16 sfxId) {
    Audio_PlaySfx(sfxId);
}

bool IsEquipButtonAllowed(size_t index) {
    return index < FirstDpadButtonIndex || CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0) != 0;
}

} // namespace

bool KaleidoQuestManager_IsVanillaPageShown(void) {
    if (sShownExtraPage.has_value()) {
        const std::vector<uint16_t> pages = VisibleExtraPages();
        if (std::find(pages.begin(), pages.end(), *sShownExtraPage) == pages.end()) {
            sShownExtraPage.reset();
        }
    }
    return !sShownExtraPage.has_value();
}

bool KaleidoQuestManager_IsPointClaimed(uint8_t point) {
    return FindClaimOnPage(ShownPage(), point) != nullptr;
}

bool KaleidoQuestManager_CycleQuestPage(PlayState* play) {
    const std::vector<uint16_t> pages = VisibleExtraPages();
    if (play == nullptr || pages.empty()) {
        return false;
    }

    auto next = pages.begin();
    if (sShownExtraPage.has_value()) {
        next = std::find(pages.begin(), pages.end(), *sShownExtraPage);
        next = next == pages.end() ? pages.end() : next + 1;
    }
    sShownExtraPage = next == pages.end() ? std::optional<uint16_t>() : std::optional<uint16_t>(*next);
    if(sShownExtraPage.has_value()) {
        int first=-1;
        for(int point=0;point<25;++point) if(FindClaimOnPage(ShownPage(),point)){first=point;break;}
        play->pauseCtx.cursorPoint[PAUSE_QUEST]=play->pauseCtx.cursorSlot[PAUSE_QUEST]=first<0?0:first;
        play->pauseCtx.cursorSpecialPos=first<0?PAUSE_CURSOR_PAGE_RIGHT:0;
        const auto* item=first<0?nullptr:FindClaimOnPage(ShownPage(),first);
        CustomItemRegistry_SelectPauseItem(item?item->key:nullptr);
        play->pauseCtx.cursorItem[PAUSE_QUEST]=item?ITEM_CUSTOM:PAUSE_ITEM_NONE;
        play->pauseCtx.namedItem=PAUSE_ITEM_NONE;
    }
    PlayPauseSfx(NA_SE_SY_WIN_SCROLL_RIGHT);
    return true;
}

bool KaleidoQuestManager_EquipPoint(PlayState* play) {
    const S2HCustomItemDefinition* claim =
        FindDrawnClaim(static_cast<uint8_t>(play->pauseCtx.cursorPoint[PAUSE_QUEST]));
    if (claim == nullptr || play->pauseCtx.cursorSpecialPos != 0) {
        return false;
    }

    for (size_t index = 0; index < EquipButtons.size(); ++index) {
        if (!CHECK_BTN_ALL(CONTROLLER1(&play->state)->press.button, EquipButtons[index].padButton) ||
            !IsEquipButtonAllowed(index)) {
            continue;
        }
        const bool isEquipped = CustomItemRegistry_Equip(EquipButtons[index].heldButton, claim->key);
        PlayPauseSfx(isEquipped ? NA_SE_SY_DECIDE : NA_SE_SY_ERROR);
        return true;
    }
    return false;
}

static const char* NativeUpgradeIcon(uint8_t point) {
    int value=0;
    if(point==QUEST_SWORD) {value=GET_CUR_UPG_VALUE(UPG_QUIVER);return value>0&&value<=3?static_cast<const char*>(gItemIcons[ITEM_QUIVER_30+value-1]):nullptr;}
    if(point==QUEST_SHIELD) {value=GET_CUR_UPG_VALUE(UPG_BOMB_BAG);return value>0&&value<=3?static_cast<const char*>(gItemIcons[ITEM_BOMB_BAG_20+value-1]):nullptr;}
    const char* strength[]={"__OTR__textures/oot_icon_item_static/gItemIconGoronsBraceletTex","__OTR__textures/oot_icon_item_static/gItemIconSilverGauntletsTex","__OTR__textures/oot_icon_item_static/gItemIconGoldenGauntletsTex"};
    const char* scale[]={"__OTR__textures/oot_icon_item_static/gItemIconScaleSilverTex","__OTR__textures/oot_icon_item_static/gItemIconScaleGoldenTex"};
    const char* icon=nullptr;
    if(point==QUEST_QUIVER) {value=GET_CUR_UPG_VALUE(UPG_STRENGTH);if(value>0&&value<=3)icon=strength[value-1];}
    if(point==QUEST_BOMB_BAG) {value=GET_CUR_UPG_VALUE(UPG_SCALE);if(value>0&&value<=2)icon=scale[value-1];}
    if(icon && Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HasFile(icon+7))return icon;
    return nullptr;
}

void KaleidoQuestManager_DrawPoints(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    PauseContext* pauseCtx = &play->pauseCtx;

    OPEN_DISPS(gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    for (uint8_t point = 0; point < QuestPointCount; ++point) {
        const S2HCustomItemDefinition* claim = FindDrawnClaim(point);
        const char* icon = claim != nullptr ? CustomItemRegistry_ResolveTexture(claim->key, S2H_ITEM_ICON_INVENTORY)
                                            : (KaleidoQuestManager_IsVanillaPageShown() ? NativeUpgradeIcon(point) : nullptr);
        if (icon == nullptr) {
            continue;
        }
        gDPSetPrimColor(POLY_OPA_DISP++,0,0,255,255,255,KaleidoItemManager_IsPauseEffectActive(claim)?pauseCtx->alpha:pauseCtx->alpha/2);
        gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(&pauseCtx->questVtx[point * 4]), 4, 0);
        KaleidoScope_DrawTexQuadRGBA32(gfxCtx, const_cast<char*>(icon), QuestIconSize, QuestIconSize, 0);
    }

    CLOSE_DISPS(gfxCtx);
}

void KaleidoQuestManager_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) { sShownExtraPage.reset(); });
}

uint16_t KaleidoQuestManager_ResolveCursorItem(uint8_t point, uint16_t vanillaItem) {
    const S2HCustomItemDefinition* claim = FindDrawnClaim(point);
    if (claim == nullptr) {
        return vanillaItem;
    }
    CustomItemRegistry_SelectPauseItem(claim->key);
    return ITEM_CUSTOM;
}

// L turns to the next extra quest page, and a C or D-pad button equips the claim under the cursor.
void KaleidoQuestManager_Update(PlayState* play) {
    if (CHECK_BTN_ALL(CONTROLLER1(&play->state)->press.button, BTN_L) &&
        !CVarGetInteger("gDeveloperTools.DebugEnabled", 0) && play->pauseCtx.cursorSpecialPos == 0) {
        KaleidoQuestManager_CycleQuestPage(play);
    }
    QuestWheel(play);
    KaleidoQuestManager_EquipPoint(play);
}

bool KaleidoQuestManager_ShowDescription(PlayState* play) {
    if (play->pauseCtx.cursorItem[PAUSE_QUEST] != ITEM_CUSTOM) {
        return false;
    }
    KaleidoItemManager_OpenPauseDescription(play, CustomItemRegistry_GetPauseItem(),
                                            play->pauseCtx.cursorYIndex[PAUSE_QUEST]);
    return true;
}


static std::vector<const S2HCustomItemDefinition*> QuestCell(uint16_t page, uint8_t slot) {
    std::vector<const S2HCustomItemDefinition*> result;
    for (uint32_t i=0;i<CustomItemRegistry_GetCount();++i) {
        const auto* item=CustomItemRegistry_GetAt(i); S2HCustomItemPlacement p;
        if (CustomItemRegistry_IsOwned(item->key) && KaleidoItemManager_GetPlacement(item->key,&p) && p.page==page && p.slot==slot) result.push_back(item);
    }
    return result;
}
static void QuestWheel(PlayState* play) {
    auto* pause=&play->pauseCtx; int slot=pause->cursorPoint[PAUSE_QUEST];
    auto contents=QuestCell(ShownPage(),slot);
    auto* input=CONTROLLER1(&play->state);
    if (contents.size()<2) {
        if(contents.size()==1 && (input->press.button & BTN_A))KaleidoItemManager_ToggleEquipment(contents.front());
        sQuestHold=0;return;
    }
    auto turn=[&](int step) {
        const auto* previous=FindClaimOnPage(ShownPage(),slot);
        auto it=std::find(contents.begin(),contents.end(),previous);
        int index=it==contents.end()?0:it-contents.begin();
        const auto* next=contents[(index+step+contents.size())%contents.size()];
        KaleidoItemManager_SetWheelSelection(ShownPage(),slot,next->key);
        for (uint8_t button=1;button<S2H_ITEM_BUTTON_COUNT;++button) {
            const char* key=CustomItemRegistry_GetEquippedKey(button);
            if (key && previous && std::strcmp(key,previous->key)==0) CustomItemRegistry_Equip(button,next->key);
        }
        CustomItemRegistry_SelectPauseItem(next->key);
        pause->namedItem=PAUSE_ITEM_NONE;Audio_PlaySfx(NA_SE_SY_CURSOR);
    };
    if (input->press.button & BTN_A) turn(1);
    if (input->cur.button & BTN_A) {
        if (++sQuestHold>=10) {
            int step=pause->stickAdjX>30?1:pause->stickAdjX < -30?-1:0;
            if (step) turn(step);
            pause->stickAdjX=pause->stickAdjY=0;
        }
    } else {if(sQuestHold>=10)KaleidoItemManager_ToggleEquipment(FindClaimOnPage(ShownPage(),slot));sQuestHold=0;}
}

bool KaleidoQuestManager_UpdateExtra(PlayState* play) {
    auto* pause=&play->pauseCtx;
    auto* input=CONTROLLER1(&play->state);
    if (pause->state != PAUSE_STATE_MAIN || pause->mainState != PAUSE_MAIN_STATE_IDLE || pause->pageIndex!=PAUSE_QUEST || pause->itemDescriptionOn)
        return !KaleidoQuestManager_IsVanillaPageShown();
    if (KaleidoQuestManager_IsVanillaPageShown()) return false;
    if ((input->press.button & BTN_L) && !CVarGetInteger("gDeveloperTools.DebugEnabled",0)) {
        KaleidoQuestManager_CycleQuestPage(play);
        pause->namedItem=PAUSE_ITEM_NONE;sQuestHold=0;
        if (KaleidoQuestManager_IsVanillaPageShown()) {pause->cursorPoint[PAUSE_QUEST]=pause->cursorSlot[PAUSE_QUEST]=0;pause->cursorItem[PAUSE_QUEST]=PAUSE_ITEM_NONE;CustomItemRegistry_SelectPauseItem(nullptr);return true;}
    }
    int slot=std::clamp<int>(pause->cursorPoint[PAUSE_QUEST],0,24);
    if(!FindClaimOnPage(ShownPage(),slot)) {
        int first=-1;
        for(int point=0;point<25;++point)if(FindClaimOnPage(ShownPage(),point)){first=point;break;}
        if(first<0) {
            pause->cursorItem[PAUSE_QUEST]=PAUSE_ITEM_NONE;
            CustomItemRegistry_SelectPauseItem(nullptr);
            KaleidoScope_MoveCursorToSpecialPos(play,PAUSE_CURSOR_PAGE_RIGHT);
            return true;
        }
        slot=first;
        pause->cursorPoint[PAUSE_QUEST]=slot;
    }
    QuestWheel(play);
    int dx=pause->stickAdjX>30?1:pause->stickAdjX < -30?-1:0;
    int dy=pause->stickAdjY>30?1:pause->stickAdjY < -30?-1:0;
    if (pause->cursorSpecialPos!=0) {
        if ((pause->cursorSpecialPos==PAUSE_CURSOR_PAGE_LEFT && dx>0)||(pause->cursorSpecialPos==PAUSE_CURSOR_PAGE_RIGHT && dx<0)) pause->cursorSpecialPos=0;
        else {pause->cursorItem[PAUSE_QUEST]=PAUSE_ITEM_NONE;return true;}
    } else if (dx || dy) {
        S2HLayoutCell from;ModLayout_GetCell(ShownPage(),slot,&from);
        float best=1e9f;int target=-1;
        for(int i=0;i<25;++i) {
            if(i==slot || !FindClaimOnPage(ShownPage(),i))continue;S2HLayoutCell cell;ModLayout_GetCell(ShownPage(),i,&cell);
            float x=cell.x+cell.width/2.0f-from.x-from.width/2.0f;
            float y=cell.y-cell.height/2.0f-from.y+from.height/2.0f;
            float along=dx?x*dx:y*dy;if(along<=0)continue;
            float score=along+std::abs(dx?y:x)*3;
            if(score<best){best=score;target=i;}
        }
        if(target>=0)slot=target;
        else if(dx)KaleidoScope_MoveCursorToSpecialPos(play,dx<0?PAUSE_CURSOR_PAGE_LEFT:PAUSE_CURSOR_PAGE_RIGHT);
        pause->namedItem=PAUSE_ITEM_NONE;Audio_PlaySfx(NA_SE_SY_CURSOR);
    }
    pause->cursorPoint[PAUSE_QUEST]=pause->cursorSlot[PAUSE_QUEST]=slot;
    pause->cursorYIndex[PAUSE_QUEST]=slot/6;
    const auto* item=FindClaimOnPage(ShownPage(),slot);
    CustomItemRegistry_SelectPauseItem(item?item->key:nullptr);
    pause->cursorItem[PAUSE_QUEST]=item?ITEM_CUSTOM:PAUSE_ITEM_NONE;
    pause->cursorColorSet=PAUSE_CURSOR_COLOR_SET_YELLOW;
    KaleidoQuestManager_EquipPoint(play);
    if(item && (input->press.button & BTN_CUP)) KaleidoItemManager_OpenPauseDescription(play,item,slot/6);
    return true;
}

bool KaleidoQuestManager_DrawExtra(PlayState* play) {
    if(KaleidoQuestManager_IsVanillaPageShown())return false;
    auto* pause=&play->pauseCtx;
    pause->questVtx=static_cast<Vtx*>(GRAPH_ALLOC(play->state.gfxCtx,25*4*sizeof(Vtx)));
    for(int slot=0;slot<25;++slot)for(int corner=0;corner<4;++corner) {
        Vtx v=VTX(0,0,0,(corner&1)?32<<5:0,(corner&2)?32<<5:0,255,255,255,255);
        pause->questVtx[slot*4+corner]=v;
    }
    ModLayout_ApplyCells(play,ShownPage(),pause->questVtx,25);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL42_Opa(play->state.gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++,G_CC_MODULATEIA_PRIM,G_CC_MODULATEIA_PRIM);
    for(int slot=0;slot<25;++slot) {
        const auto* item=FindClaimOnPage(ShownPage(),slot);bool owned=item!=nullptr;
        if(!item)continue;
        const char* icon=CustomItemRegistry_ResolveTexture(item->key,S2H_ITEM_ICON_INVENTORY);if(!icon)continue;
        gDPSetPrimColor(POLY_OPA_DISP++,0,0,255,255,255,KaleidoItemManager_IsPauseEffectActive(item)?pause->alpha:pause->alpha/2);
        gSPVertex(POLY_OPA_DISP++,reinterpret_cast<uintptr_t>(&pause->questVtx[slot*4]),4,0);
        KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx,const_cast<char*>(icon),32,32,0);
    }
    KaleidoScope_SetCursorVtxPos(pause,std::clamp<int>(pause->cursorSlot[PAUSE_QUEST],0,24)*4,pause->questVtx);
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

uint16_t KaleidoQuestManager_GetShownPage(void) { return ShownPage(); }
void** KaleidoQuestManager_GetBackground(void** vanilla) {
    if(KaleidoQuestManager_IsVanillaPageShown())return vanilla;
    static const char* names[]={
        "__OTR__textures/oot_icon_item_nes_static/gPauseQuestStatus00ENGTex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus01Tex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus02Tex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus03Tex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus04Tex",
        "__OTR__textures/oot_icon_item_nes_static/gPauseQuestStatus10ENGTex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus11Tex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus12Tex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus13Tex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus14Tex",
        "__OTR__textures/oot_icon_item_nes_static/gPauseQuestStatus20ENGTex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus21Tex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus22Tex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus23Tex",
        "__OTR__textures/oot_icon_item_static/gPauseQuestStatus24Tex"
    };
    static void* textures[15];
    auto archives=Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    for(int i=0;i<15;++i){if(!archives->HasFile(names[i]+7))return vanilla;textures[i]=const_cast<char*>(names[i]);}
    return textures;
}
