// MM's pause hooks select the bow element without fixed inventory positions.
#include "sw97_mod.h"
#include <string.h>
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"

static const ALIGN_ASSET(2) char sBowIcons[SW97_ELEM_COUNT][80] = {
    "__OTR__icon_item_static_yar/gItemIconBowTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97FireTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97IceTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97LightTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97DarkTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97SoulTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97WindTex",
};


static const ALIGN_ASSET(2) char sSlingIcons[SW97_ELEM_COUNT][80]={
    "__OTR__textures/oot_icon_item_static/gItemIconSlingshotTex",
    "__OTR__textures/icon_item_custom/gItemIconSlingshotSw97FireTex",
    "__OTR__textures/icon_item_custom/gItemIconSlingshotSw97IceTex",
    "__OTR__textures/icon_item_custom/gItemIconSlingshotSw97LightTex",
    "__OTR__textures/icon_item_custom/gItemIconSlingshotSw97DarkTex",
    "__OTR__textures/icon_item_custom/gItemIconSlingshotSw97SoulTex",
    "__OTR__textures/icon_item_custom/gItemIconSlingshotSw97WindTex"
};
static void ResolveElementIcon(PlayState* play,u8 button,u16 item,const char** icon){
    const char* key=gSw97Api->GetEquippedCustomItem(button);
    bool sling=item==ITEM_CUSTOM && key && !strcmp(key,"oot.slingshot");
    if(!sling && item!=ITEM_BOW)return;
    Sw97Element element=Sw97_GetElement(sling);
    const char* path=sling?sSlingIcons[element]:sBowIcons[element];
    bool(*bombSelected)(void)=(bool(*)(void))gSw97Api->FindService("nei.bomb_arrows.selected");
    static const ALIGN_ASSET(2) char bombIcon[]="__OTR__textures/icon_item_custom/gItemIconBombArrowsTex";
    if(bombSelected && bombSelected())path=bombIcon;
    if(gSw97Api->HasResource(path))*icon=path;
}
void Sw97_RegisterArrowUi(void) {
    S2H_REGISTER_HOOK(gSw97Api, OnInterfaceResolveButtonIcon, ResolveElementIcon);
}
