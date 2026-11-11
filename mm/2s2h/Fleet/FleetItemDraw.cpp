#include "FleetItemDraw.h"
#include "FleetGame.h"

#include <cstring>
#include <iterator>
#include <string>
#include <unordered_map>
#include <fast/Fast3dGui.h>
#include <fast/resource/type/Texture.h>
#include <ship/Context.h>
#include <ship/GameRegistry.h>
#include <ship/window/Window.h>

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "assets/interface/parameter_static/parameter_static.h"
#include "assets/interface/icon_item_dungeon_static/icon_item_dungeon_static.h"
extern PlayState* gPlayState;
}

namespace {

constexpr const char* kGame = "mm";

// A parked game state keeps the matrix stack and the archives alive; before the first one there is nothing to draw
// with.
bool CanDraw() {
    const auto game = Ship::GameRegistry::Get(kGame);
    return gGameState != nullptr && game != nullptr && game->Resources != nullptr;
}

RandoItemId FindItem(const std::string& name) {
    static std::unordered_map<std::string, RandoItemId> items;
    if (items.empty()) {
        for (const auto& [id, definition] : Rando::StaticData::Items) {
            items.emplace(Rando::StaticData::GetItemDisplayName(id), id);
            if (definition.spoilerName != nullptr) {
                items.emplace(definition.spoilerName, id);
            }
        }
    }
    const auto found = items.find(name);
    return found != items.end() ? found->second : RI_UNKNOWN;
}

// Draw functions read only the graphics context, the frame counters and the billboard from the play state.
PlayState& BorrowPlayState(FleetItemDraw& request, GraphicsContext& gfx) {
    static PlayState play;
    // OPEN_DISPS marks the overlay list too; item drawings never submit to it, so the marks are discarded.
    static Gfx discardedOverlay[1024];
    // Draws that scan back through their list (for the last env color) stop at start: the lent arena.
    gfx.overlay.start = discardedOverlay;
    gfx.overlay.p = discardedOverlay;
    gfx.overlay.d = discardedOverlay + std::size(discardedOverlay);
    gfx.polyOpa.start = request.opaHead;
    gfx.polyOpa.p = static_cast<Gfx*>(request.opaHead);
    gfx.polyOpa.d = request.opaTail;
    gfx.polyXlu.start = request.xluHead;
    gfx.polyXlu.p = static_cast<Gfx*>(request.xluHead);
    gfx.polyXlu.d = request.xluTail;
    play.state.gfxCtx = &gfx;
    play.state.frames = request.frames;
    play.gameplayFrames = request.frames;
    std::memcpy(&play.billboardMtxF, request.billboard, sizeof(play.billboardMtxF));
    return play;
}

void ReturnArenas(FleetItemDraw& request, const GraphicsContext& gfx) {
    request.opaHead = gfx.polyOpa.p;
    request.opaTail = gfx.polyOpa.d;
    request.xluHead = gfx.polyXlu.p;
    request.xluTail = gfx.polyXlu.d;
}

} // namespace

namespace {

struct IconSource {
    const char* path;
    ImVec4 tint;
};

// Pickups the inventory has no icon for borrow the HUD's rupee and heart.
IconSource FindIconSource(RandoItemId item) {
    switch (item) {
        case RI_RUPEE_GREEN:
            return { gRupeeCounterIconTex, ImVec4(0.39f, 1.0f, 0.39f, 1.0f) };
        case RI_RUPEE_BLUE:
            return { gRupeeCounterIconTex, ImVec4(0.39f, 0.39f, 1.0f, 1.0f) };
        case RI_RUPEE_RED:
            return { gRupeeCounterIconTex, ImVec4(1.0f, 0.39f, 0.39f, 1.0f) };
        case RI_RUPEE_PURPLE:
            return { gRupeeCounterIconTex, ImVec4(0.75f, 0.39f, 1.0f, 1.0f) };
        case RI_RUPEE_SILVER:
            return { gRupeeCounterIconTex, ImVec4(0.85f, 0.85f, 0.85f, 1.0f) };
        case RI_RUPEE_HUGE:
            return { gRupeeCounterIconTex, ImVec4(1.0f, 0.75f, 0.2f, 1.0f) };
        case RI_RECOVERY_HEART:
            return { gHeartFullTex, ImVec4(1.0f, 0.27f, 0.27f, 1.0f) };
        case RI_TRAP:
            return { gDungeonMapSkullTex, ImVec4(1.0f, 1.0f, 1.0f, 1.0f) };
        default:
            break;
    }
    const uint32_t tintItem =
        item == RI_PROGRESSIVE_LULLABY ? ITEM_SONG_LULLABY : Rando::StaticData::Items[item].itemId;
    return { Rando::StaticData::GetIconTexturePath(item), Ship_GetItemColorTint(tintItem) };
}

// Mod icons are never preloaded into the GUI: each is loaded on first use, under its own path.
ImTextureID LoadIconTexture(const char* path) {
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if (gui->HasTextureByName(path)) {
        return gui->GetTextureByName(path);
    }
    Ship::GameScope scope(kGame);
    const auto texture = std::dynamic_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path, true));
    if (texture == nullptr) {
        return 0;
    }
    gui->LoadGuiTexture(path, *texture, "", ImVec4(1, 1, 1, 1));
    return gui->GetTextureByName(path);
}

} // namespace

bool FleetItemDraw_Icon(const char* name, FleetItemIcon* icon) {
    if (name == nullptr || icon == nullptr) {
        return false;
    }
    const RandoItemId item = FindItem(name);
    if (item == RI_UNKNOWN) {
        return false;
    }
    const IconSource source = FindIconSource(item);
    if (source.path == nullptr) {
        return false;
    }
    const ImTextureID texture = LoadIconTexture(source.path);
    if (texture == 0) {
        return false;
    }
    const ImVec4 tint = source.tint;
    *icon = { reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(texture)),
              { 0.0f, 0.0f },
              { 1.0f, 1.0f },
              { tint.x, tint.y, tint.z, tint.w } };
    return true;
}

bool FleetItemDraw_Draw(FleetItemDraw* request) {
    if (request == nullptr || request->item == nullptr || !CanDraw()) {
        return false;
    }
    const RandoItemId item = FindItem(request->item);
    if (item == RI_UNKNOWN) {
        return false;
    }
    Ship::GameScope scope(kGame);
    static GraphicsContext gfx;
    PlayState& play = BorrowPlayState(*request, gfx);
    PlayState* runningPlay = gPlayState;
    gPlayState = &play;
    Matrix_Push();
    Matrix_Put(reinterpret_cast<MtxF*>(request->model));

    bool submitted = false;
    OPEN_DISPS(&gfx);
    gSPComboRMPush(POLY_OPA_DISP++, kGame);
    gSPComboRMPush(POLY_XLU_DISP++, kGame);
    const Gfx* opaBeforeDraw = POLY_OPA_DISP;
    const Gfx* xluBeforeDraw = POLY_XLU_DISP;
    Rando::DrawItem(item);
    submitted = POLY_OPA_DISP != opaBeforeDraw || POLY_XLU_DISP != xluBeforeDraw;
    gSPComboRMPop(POLY_OPA_DISP++);
    gSPComboRMPop(POLY_XLU_DISP++);
    CLOSE_DISPS(&gfx);

    Matrix_Pop();
    gPlayState = runningPlay;
    if (!submitted) {
        return false;
    }
    ReturnArenas(*request, gfx);
    return true;
}

bool FleetItemDraw_DrawForeign(PlayState* play, const char* owner, const char* item) {
    const auto draw = reinterpret_cast<FleetDrawForeignItemFn>(FleetGame_GetHostFunction("DrawForeignItem"));
    if (draw == nullptr) {
        return false;
    }
    GraphicsContext* gfx = play->state.gfxCtx;
    FleetItemDraw request = { .owner = owner,
                              .item = item,
                              .opaHead = gfx->polyOpa.p,
                              .opaTail = gfx->polyOpa.d,
                              .xluHead = gfx->polyXlu.p,
                              .xluTail = gfx->polyXlu.d,
                              .frames = play->state.frames };
    MtxF model;
    Matrix_Get(&model);
    std::memcpy(request.model, &model, sizeof(request.model));
    std::memcpy(request.billboard, &play->billboardMtxF, sizeof(request.billboard));
    if (!draw(&request)) {
        return false;
    }
    gfx->polyOpa.p = static_cast<Gfx*>(request.opaHead);
    gfx->polyOpa.d = request.opaTail;
    gfx->polyXlu.p = static_cast<Gfx*>(request.xluHead);
    gfx->polyXlu.d = request.xluTail;
    return true;
}
