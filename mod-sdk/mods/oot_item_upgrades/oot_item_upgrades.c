#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "z64items.h"
#include "z64aiming.h"
#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ModApi/ModMenu/ModUI.h"
#include "soh/Enhancements/randomizer/randomizerEnums.h"
#include "objects/oot_object_link_boy/object_link_boy.h"
#include "objects/oot_object_link_child/object_link_child.h"
#include "objects/oot_object_ik/object_ik.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_Arms_Hook/z_arms_hook.h"
#include "overlays/actors/ovl_En_Ishi/z_en_ishi.h"
#include "overlays/actors/ovl_Bg_Heavy_Block/z_bg_heavy_block.h"
#include "overlays/actors/ovl_En_Goroiwa/z_en_goroiwa.h"
#include "overlays/actors/ovl_Boss_Goma/z_boss_goma.h"
#include "overlays/actors/ovl_Boss_Dodongo/z_boss_dodongo.h"
#include "objects/oot_object_kingdodongo/object_kingdodongo.h"
#include "z64oot_names.h"
#ifdef _MSC_VER
#pragma warning(error:4013)
#endif
#define MOD_NAME "oot_item_upgrades"
#define KNIFE_KEY "nei.equip.giants_knife"
#define LOOK_CVAR "gMods.OotItemUpgrades.KokiriShape"
enum { CH_HAMMER, CH_KOKIRI, CH_MASTER, CH_BGS, CH_HOOK, CH_COUNT };
static const SOHModApi* sApi;
static Actor* sHeldHammerPickup;
static const char* sHeldHammerKey;
static int sPendingGiveChain=-1;
static const char* sGiveStatus="";
static const char* sChain[CH_COUNT]={"nei.upgrades.hammer","nei.upgrades.kokiri","nei.upgrades.master","nei.upgrades.bgs","nei.upgrades.hookshot"};
static const char* sPickup[CH_COUNT]={"nei.progressive.hammer","nei.progressive.kokiri_sword","nei.progressive.master_sword","nei.progressive.bgs","nei.progressive.hookshot"};
static const char* const sHooks[]={"OnActorDestroy","OnCustomItemReceive","OnLoadFile","OnCustomEquipOwnedChange","OnPlayerUpdate","OnPlayerResolveLimbDraw","OnPlayerPostLimbDraw","OnActorUpdate",
 "OnPlayDrawEnd","OnGetItemDraw","OnOpenText","OnSaveEditorInventory","ShouldActorUpdate","OnBgCheckLineTest","OnInterfaceResolveButtonIcon","OnKaleidoResolveName","OnKaleidoResolveItemIcon","OnCollisionResolveDamage","OnResolveSwordDamage","OnSceneInit","OnResolveCustomGetItem"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sHooks,ARRAY_COUNT(sHooks)};
static const ALIGN_ASSET(2) char sRazorBlade[]="__OTR__objects/gameplay_keep/gRazorSwordBladeDL";
static const ALIGN_ASSET(2) char sRazorHandle[]="__OTR__objects/gameplay_keep/gRazorSwordHandleDL";
static const ALIGN_ASSET(2) char sGildedBlade[]="__OTR__objects/object_link_child/gLinkHumanGildedSwordBladeDL";
static const ALIGN_ASSET(2) char sGildedHandle[]="__OTR__objects/object_link_child/gLinkHumanGildedSwordHandleDL";
static const ALIGN_ASSET(2) char sGfs[]="__OTR__objects/object_link_child/gLinkHumanGreatFairysSwordDL";
static const ALIGN_ASSET(2) char sAxe[]="__OTR__objects/object_nei_upgrades/gGerudoAxeDL";
static const ALIGN_ASSET(2) char sRazorIcon[]="__OTR__icon_item_static_yar/gItemIconRazorSwordTex";
static const ALIGN_ASSET(2) char sGildedIcon[]="__OTR__icon_item_static_yar/gItemIconGildedSwordTex";
static const ALIGN_ASSET(2) char sGfsIcon[]="__OTR__icon_item_static_yar/gItemIconGreatFairysSwordTex";
static const ALIGN_ASSET(2) char sAxeIcon[]="__OTR__textures/icon_item_custom/gGerudoAxeIconTex";
static const ALIGN_ASSET(2) char sUltraIcon[]="__OTR__textures/icon_item_custom/gUltrashotIconTex";
static const ALIGN_ASSET(2) char sHammerIcon[]="__OTR__textures/oot_icon_item_static/gItemIconHammerTex";
static const ALIGN_ASSET(2) char sKokiriIcon[]="__OTR__textures/oot_icon_item_static/gItemIconSwordKokiriTex";
static const ALIGN_ASSET(2) char sMasterIcon[]="__OTR__textures/oot_icon_item_static/gItemIconSwordMasterTex";
static const ALIGN_ASSET(2) char sBgsIcon[]="__OTR__textures/oot_icon_item_static/gItemIconSwordBiggoronTex";
static const ALIGN_ASSET(2) char sKnifeIcon[]="__OTR__textures/oot_icon_item_static/gItemIconSwordBiggoronTex";
static const ALIGN_ASSET(2) char sHookIcon[]="__OTR__textures/oot_icon_item_static/gItemIconHookshotTex";
static const ALIGN_ASSET(2) char sLongIcon[]="__OTR__textures/oot_icon_item_static/gItemIconLongshotTex";
typedef struct {const char* key;int chain,level,gid;const char* icon;const char* text;const char* name;} Stage;
static const Stage sStages[]={
 {"oot.hammer",CH_HAMMER,1,-1,sHammerIcon,"You got the %rMegaton Hammer%w!&Equip it to %y\xB2%w and press %y\xB2%w to&swing. Smash heavy obstacles!","Megaton Hammer"},
 {"nei.weapon.gerudo_axe",CH_HAMMER,2,-1,sAxeIcon,"You got the %yGerudo Axe%w!&A heavy axe with greater reach&and double Hammer damage.^Equip it to %y\xB2%w to swing.&Aim with %c\xB6%w, then press %y\xB2%w&to throw it and bring it back.","Gerudo Axe"},
 {"nei.weapon.kokiri_sword",CH_KOKIRI,1,GID_SWORD_KOKIRI,sKokiriIcon,"You got the %gKokiri Sword%w!&Equip it in Equipment.&Press %g\xB1%w to swing your sword.","Kokiri Sword"},
 {"nei.weapon.razor_sword",CH_KOKIRI,2,-1,sRazorIcon,"You got the %rRazor Sword%w!&Master Sword reach and power&for your Kokiri Sword.^Equip your Kokiri Sword&in Equipment. Press %g\xB1%w&to attack with the new blade.","Razor Sword"},
 {"nei.weapon.gilded_sword",CH_KOKIRI,3,-1,sGildedIcon,"You got the %yGilded Sword%w!&Master Sword reach with&Biggoron Sword power.^Equip your Kokiri Sword&in Equipment. Press %g\xB1%w&to attack with the new blade.","Gilded Sword"},
 {"nei.weapon.master_sword",CH_MASTER,1,GID_SWORD_BGS,sMasterIcon,"You got the %bMaster Sword%w!&Equip it in Equipment.&Press %g\xB1%w to swing your sword.","Master Sword"},
 {"nei.weapon.real_master_sword",CH_MASTER,2,GID_SWORD_BGS,sMasterIcon,"You got the %bReal Master Sword%w!&At full health, sword swings&launch sacred sword beams.^Equip the Master Sword&and press %g\xB1%w to attack.&The beams cost no magic.","Real Master Sword"},
 {"nei.weapon.giants_knife",CH_BGS,1,GID_SWORD_BGS,sKnifeIcon,"You got the %rGiant's Knife%w!&Equip it in Equipment.&Press %g\xB1%w for a two-handed slash.^This heavy blade can break.&Use it carefully!","Giant's Knife"},
 {"nei.weapon.biggoron_sword",CH_BGS,2,GID_SWORD_BGS,sBgsIcon,"You got %rBiggoron's Sword%w!&A mighty blade that&never breaks.^Equip it in Equipment.&Press %g\xB1%w for a powerful&two-handed slash.","Biggoron's Sword"},
 {"nei.weapon.great_fairy_sword",CH_BGS,3,-1,sGfsIcon,"You got the %pGreat Fairy's Sword%w!&Hits restore life and magic.^Equip your great sword&in Equipment. Press %g\xB1%w&to attack with the new blade.","Great Fairy's Sword"},
 {"nei.weapon.hookshot",CH_HOOK,1,GID_HOOKSHOT,sHookIcon,"You got the %rHookshot%w!&Equip it to %y\xB2%w. Hold %y\xB2%w to aim,&then release to fire the hook.","Hookshot"},
 {"nei.weapon.longshot",CH_HOOK,2,GID_LONGSHOT,sLongIcon,"You got the %rLongshot%w!&Its hook reaches farther&than the Hookshot.^Equip it to %y\xB2%w. Hold %y\xB2%w to aim,&then release to fire the hook.","Longshot"},
 {"nei.weapon.ultrashot",CH_HOOK,3,GID_LONGSHOT,sUltraIcon,"You got the %yUltrashot%w!&Four times Hookshot reach&and twice the speed!^Equip it to %y\xB2%w. Hold %y\xB2%w to aim,&then release to fire the hook.","Ultrashot"},
};
GetItemEntry ItemTable_Retrieve(int16_t getItemID);
GetItemEntry ItemTable_RetrieveEntry(int16_t tableId,int16_t getItemID);
void Randomizer_DrawMasterSword(PlayState*,GetItemEntry*);
void GameInteractor_ExecuteOnOpenText(uint16_t*,bool*);
static int16_t VanillaStageGetItem(const Stage* stage);
static GetItemEntry sOriginalMasterEntry;
static uint16_t sOriginalMasterMessage;
static const int sStart[CH_COUNT]={0,2,5,7,10};
static const int sLength[CH_COUNT]={2,3,2,3,3};
// The playable equipment definition is also the acquisition definition for its base stage.
static const char* StagePickupKey(const Stage* stage) {
 if(stage->chain==CH_MASTER && stage->level==1)return "oot.equip.master_sword";
 if(stage->chain==CH_BGS && stage->level==1)return KNIFE_KEY;
 if(stage->chain==CH_BGS && stage->level==2)return "oot.equip.biggoron_sword";
 return stage->key;
}
static const Stage* StageForKey(const char* key) {
 if(!key)return NULL;
 for(int i=0;i<ARRAY_COUNT(sStages);i++)
  if(!strcmp(key,sStages[i].key)||!strcmp(key,StagePickupKey(&sStages[i])))return &sStages[i];
 return NULL;
}
static u16 Level(int c){return sApi->GetProgressiveLevel(sChain[c],SOH_RANDO_INF_SAVE);}
static bool SaveBank(void) { return !RandoInfCustom_IsLogicSimulation(); }
static u16 ReadHammer(const SaveContext* s) {
    if (!SaveBank()) return 0;
    return sApi->IsCustomItemOwned("nei.weapon.gerudo_axe") ? 2 : sApi->IsCustomItemOwned("oot.hammer") ? 1 : 0;
}
static u16 ReadKokiri(const SaveContext* s) {
    return s == &gSaveContext ? MIN(CustomEquipRegistry_GetVanillaBase(S2H_EQUIP_SLOT_SWORD), 3) :
           MIN(s->save.saveInfo.equips.equipment & 15, 3);
}
static u16 ReadMaster(const SaveContext* s) { return SaveBank() && sApi->IsEquipmentOwned("oot.equip.master_sword") ? 1 : 0; }
static u16 ReadBgs(const SaveContext* s) {
    return s->save.saveInfo.inventory.items[SLOT_SWORD_GREAT_FAIRY] == ITEM_SWORD_GREAT_FAIRY ? 3 :
           !SaveBank() ? 0 : sApi->IsEquipmentOwned("oot.equip.biggoron_sword") ? 2 : sApi->IsEquipmentOwned(KNIFE_KEY) ? 1 : 0;
}
static u16 ReadHook(const SaveContext* s) { return s->save.saveInfo.inventory.items[SLOT_HOOKSHOT] == ITEM_HOOKSHOT ? 1 : 0; }
// OoT upgrades replace the item in its slot: the Gerudo Axe is oot.hammer with another look.
static void SyncHammerIcon(void) {
    const char* icon = sApi->IsCustomItemOwned("nei.weapon.gerudo_axe") ? sAxeIcon : sHammerIcon;
    sApi->SetCustomItemIconPath("oot.hammer", icon);
}

static void ApplyHammer(SaveContext* s, u16 level) {
    if (!SaveBank()) {
        return;
    }
    sApi->SetCustomItemOwned("oot.hammer", level > 0);
    sApi->SetCustomItemOwned("nei.weapon.gerudo_axe", level >= 2);
    SyncHammerIcon();
}
static void ApplyKokiri(SaveContext* s, u16 level) {
    s->save.saveInfo.equips.equipment = (s->save.saveInfo.equips.equipment & ~15) | level;
    s->save.saveInfo.equips.buttonItems[0][EQUIP_SLOT_B] = level ? ITEM_SWORD_KOKIRI + level - 1 : ITEM_NONE;
    if (s == &gSaveContext && gPlayState) {
        Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
        Interface_LoadItemIconImpl(gPlayState, EQUIP_SLOT_B);
    }
}
static void ApplyMaster(SaveContext* s, u16 level) {
    if (!SaveBank()) return;
    sApi->SetEquipmentOwned("oot.equip.master_sword", level > 0);
    if (level) sApi->WearEquipment("oot.equip.master_sword");
}
static void ApplyBgs(SaveContext* s, u16 level) {
    if (level >= 3) s->save.saveInfo.inventory.items[SLOT_SWORD_GREAT_FAIRY] = ITEM_SWORD_GREAT_FAIRY;
    if (!SaveBank()) return;
    sApi->SetEquipmentOwned(KNIFE_KEY, level == 1);
    sApi->SetEquipmentOwned("oot.equip.biggoron_sword", level >= 2);
    if (level >= 3) s->save.saveInfo.inventory.items[SLOT_SWORD_GREAT_FAIRY] = ITEM_SWORD_GREAT_FAIRY;
}
static void ApplyHook(SaveContext* s, u16 level) {
    s->save.saveInfo.inventory.items[SLOT_HOOKSHOT] = level ? ITEM_HOOKSHOT : ITEM_NONE;
}
static bool HumanWeapon(Player* p) {
    if (!p || !gPlayState || p != GET_PLAYER(gPlayState) || GET_PLAYER_FORM != PLAYER_FORM_HUMAN) return false;
    if (SOH_MOD_API_HAS(sApi, GetActiveForm)) {
        const char* form = sApi->GetActiveForm();
        if (form && form[0]) return false;
    }
    return true;
}
static int Weapon(Player* p) {
    if (!HumanWeapon(p)) return -1;
    if (p->heldItemAction == PLAYER_IA_CUSTOM) {
        const char* held = sApi->GetEquippedCustomItem(p->heldItemButton);
        bool isAxe = held != NULL && strcmp(held, "oot.hammer") == 0 && Level(CH_HAMMER) >= 2;
        return isAxe ? CH_HAMMER : -1;
    }
    const char* sword = sApi->GetWornEquipment(S2H_EQUIP_SLOT_SWORD);
    if (sword && !strcmp(sword, "oot.equip.master_sword") && Level(CH_MASTER) >= 2) return CH_MASTER;
    if (p->heldItemId == ITEM_SWORD_GREAT_FAIRY && Level(CH_BGS) >= 3) return CH_BGS;
    return -1;
}
static bool GildedLook(void){return CVarGetInteger(LOOK_CVAR,1)!=0;}
static const char* Icon(int c,int l){
 if(c==CH_HAMMER&&l>=2)return sAxeIcon;
 if(c==CH_KOKIRI&&l>=2)return l>=3&&GildedLook()?sGildedIcon:sRazorIcon;
 if(c==CH_BGS&&l>=3)return sGfsIcon;
 if(c==CH_HOOK&&l>=2)return l>=3?sUltraIcon:sLongIcon;
 return NULL;
}
static void ResolveIcon(PlayState* play,u16 item,const char** path){
 if(item==ITEM_SWORD_GILDED&&!GildedLook()&&Level(CH_KOKIRI)>=3){*path=sRazorIcon;return;}
 int c=item==ITEM_HAMMER?CH_HAMMER:item==ITEM_SWORD_KOKIRI?CH_KOKIRI:item==ITEM_SWORD_BGS?CH_BGS:item==ITEM_HOOKSHOT?CH_HOOK:-1;
 if(c>=0){const char* icon=Icon(c,Level(c));if(icon&&sApi->HasResource(icon))*path=icon;}
}
static const ALIGN_ASSET(2) char sUltraName[]="__OTR__textures/item_name_custom/gUltrashotNameTex";
static const ALIGN_ASSET(2) char sAxeName[]="__OTR__textures/item_name_custom/gGerudoAxeNameTex";
static const char sHammerName[]="__OTR__textures/oot_item_name_static/gMegatonHammerItemNameENGTex";
static const ALIGN_ASSET(2) char sRazorName[]="__OTR__item_name_static/gItemNameRazorSwordENGTex";
static const ALIGN_ASSET(2) char sGildedName[]="__OTR__item_name_static/gItemNameGildedSwordENGTex";
static const ALIGN_ASSET(2) char sGfsName[]="__OTR__item_name_static/gItemNameGreatFairysSwordENGTex";
static void ResolveName(PlayState* play,u16 item,const char** name){
 if(item==ITEM_HOOKSHOT&&Level(CH_HOOK)==2)*name="__OTR__textures/oot_item_name_static/gLongshotItemNameENGTex";
 if(item==ITEM_SWORD_GILDED&&!GildedLook()&&Level(CH_KOKIRI)>=3)*name=sRazorName;
 if(item==ITEM_HAMMER&&Level(CH_HAMMER)>=2)*name=sAxeName;
 if(item==ITEM_CUSTOM&&Level(CH_HAMMER)>=2&&*name&&!strcmp(*name,sHammerName))*name=sAxeName;
 if(item==ITEM_HOOKSHOT&&Level(CH_HOOK)>=3)*name=sUltraName;
 if(item==ITEM_SWORD_KOKIRI&&Level(CH_KOKIRI)>=2)*name=Level(CH_KOKIRI)>=3&&GildedLook()?sGildedName:sRazorName;
 if(item==ITEM_SWORD_BGS&&Level(CH_BGS)>=3)*name=sGfsName;
}
static void ResolveButtonIcon(PlayState* play,u8 button,u16 item,const char** path){
 if(sApi->GetEquippedCustomItem(button)!=NULL)return;
 // A custom sword owns its icon even when its MM animation base is Kokiri/Razor/Gilded.
 if(button==S2H_ITEM_BUTTON_B && sApi->GetWornEquipment(S2H_EQUIP_SLOT_SWORD)!=NULL)return;
 ResolveIcon(play,item,path);
}
static void DrawPieces(int c,int l,PlayState* play){
 const char* first=NULL,*second=NULL;
 if(c==CH_HAMMER){first=l>=2?sAxe:"__OTR__objects/oot_object_gi_hammer/gGiHammerDL";Matrix_Translate(-100,-350,0,MTXMODE_APPLY);Matrix_RotateX(DEG_TO_RAD(90),MTXMODE_APPLY);Matrix_Scale(.15f,.15f,.15f,MTXMODE_APPLY);}
 if(c==CH_KOKIRI){bool gilded=l>=3&&GildedLook();first=gilded?sGildedBlade:sRazorBlade;second=gilded?sGildedHandle:sRazorHandle;}
 if(c==CH_BGS)first=l>=3?sGfs:"__OTR__objects/oot_object_gi_longsword/gGiBiggoronSwordDL";
 OPEN_DISPS(play->state.gfxCtx);
 Gfx_SetupDL_25Opa(play->state.gfxCtx);
 gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
 if(first)gSPDisplayList(POLY_OPA_DISP++,(Gfx*)first);
 if(second)gSPDisplayList(POLY_OPA_DISP++,(Gfx*)second);
 CLOSE_DISPS(play->state.gfxCtx);
}
static void DrawAxeGetItem(PlayState* play, GetItemEntry* entry) {
 OPEN_DISPS(play->state.gfxCtx);
 Matrix_Push();
 Matrix_RotateZF(DEG_TO_RAD(45), MTXMODE_APPLY);
 Matrix_Scale(0.008f, 0.008f, 0.008f, MTXMODE_APPLY);
 Gfx_SetupDL25_Opa(play->state.gfxCtx);
 gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
 MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
 gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sAxe);
 Matrix_Pop();
 CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawStage(PlayState* play,GetItemEntry* entry){
 // drawItemId is this mod's explicit callback payload. MM does not assign OoT tableId values.
 int index=entry->drawItemId;
 const Stage* stage=index>=0&&index<ARRAY_COUNT(sStages)?&sStages[index]:NULL;
 if(index>=ARRAY_COUNT(sStages)&&index<ARRAY_COUNT(sStages)+CH_COUNT)
  stage=StageForKey(sApi->GetNextProgressiveItem(sChain[index-ARRAY_COUNT(sStages)],SOH_RANDO_INF_SAVE));
 if(!stage)return;
 Matrix_Push();
 int16_t vanilla=VanillaStageGetItem(stage);
 if(stage->chain==CH_MASTER){
  // Use the equipment mod's complete pickup renderer for both progressive stages.
  const SOHCustomItemDefinition* master=sApi->FindCustomItem("oot.equip.master_sword");
  if(master && master->getItemEntry.drawFunc && master->getItemEntry.drawFunc!=DrawStage){
   GetItemEntry pickup=master->getItemEntry;
   master->getItemEntry.drawFunc(play,&pickup);
   Matrix_Pop();
   return;
  }
  Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
  Matrix_RotateZ(2.1f, MTXMODE_APPLY);
  OPEN_DISPS(play->state.gfxCtx);
  Gfx_SetupDL_25Opa(play->state.gfxCtx);
  gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
  gSPSegment(POLY_OPA_DISP++,0x08,(uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx,0,play->state.frames,0,32,32,1,0,0,32,32));
  gSPDisplayList(POLY_OPA_DISP++, (Gfx*)"__OTR__objects/oot_object_toki_objects/object_toki_objects_DL_001BD0");
  CLOSE_DISPS(play->state.gfxCtx);
 }else if(stage->chain==CH_BGS && stage->level<=2){
  // Knife and Biggoron share the original complete get-item model/material.
  const SOHCustomItemDefinition* bgs=sApi->FindCustomItem("oot.equip.biggoron_sword");
  if(bgs && bgs->getItemEntry.drawFunc){
   GetItemEntry pickup=bgs->getItemEntry;
   bgs->getItemEntry.drawFunc(play,&pickup);
  }
 }else if(vanilla!=GI_NONE){
  GetItemEntry original=ItemTable_Retrieve(vanilla);
  if(original.drawFunc)original.drawFunc(play,&original);else GetItem_Draw(play,original.gid);
 }else if(stage->gid>=0 && stage->chain!=CH_BGS){
  GetItem_Draw(play,stage->gid);
 }else{
  // This is the model shown above Link during receipt as well as in a drop.
  // Keep hand/projectile placement in their separate draw paths unchanged.
  float scale=stage->chain==CH_HAMMER?.07f:.035f;
  Matrix_Scale(scale,scale,scale,MTXMODE_APPLY);Matrix_RotateZ(DEG_TO_RAD(45),MTXMODE_APPLY);DrawPieces(stage->chain,stage->level,play);
 }
 Matrix_Pop();
}

// Resolve world/drop previews against the live SAVE level. An actor may retain an
// entry selected before another copy was acquired; drawing must not acquire it.
static const Stage* PreviewStage(const GetItemEntry* entry, bool receiving) {
 int chain=-1,minimum=0;
 if(entry->modIndex==MOD_NONE && entry->itemId==ITEM_CUSTOM && entry->getItemId==GI_CUSTOM){
  // The held get-item presentation was frozen by PrepareGetItem. Do not turn it
  // into the following upgrade after its ownership callback has already run.
  if(receiving)return NULL;
  const SOHCustomItemDefinition* d=sApi->FindCustomItemByRuntimeId(entry->tableId);
  if(!d)return NULL;
  for(int i=0;i<ARRAY_COUNT(sStages);i++)if(!strcmp(d->key,sStages[i].key)){chain=sStages[i].chain;minimum=sStages[i].level;break;}
  for(int c=0;c<CH_COUNT;c++)if(!strcmp(d->key,sPickup[c])){chain=c;break;}
 }else if(entry->modIndex==MOD_RANDOMIZER){
  switch(entry->getItemId){
   case RG_MEGATON_HAMMER:chain=CH_HAMMER;break;
   case RG_KOKIRI_SWORD:chain=CH_KOKIRI;break;
   case RG_MASTER_SWORD:chain=CH_MASTER;break;
   case RG_BIGGORON_SWORD:case RG_GIANTS_KNIFE:case RG_PROGRESSIVE_GORONSWORD:chain=CH_BGS;break;
   case RG_PROGRESSIVE_HOOKSHOT:case RG_HOOKSHOT:case RG_LONGSHOT:chain=CH_HOOK;break;
  }
 }else if(entry->modIndex==MOD_NONE){
  switch(entry->itemId){
   case ITEM_HAMMER:chain=CH_HAMMER;minimum=1;break;
   case ITEM_SWORD_KOKIRI:chain=CH_KOKIRI;minimum=1;break;
   case ITEM_SWORD_MASTER:chain=CH_MASTER;minimum=1;break;
   case ITEM_HOOKSHOT:chain=CH_HOOK;minimum=1;break;
   case ITEM_LONGSHOT:chain=CH_HOOK;minimum=2;break;
  }
 }
 if(chain<0)return NULL;
 const char* next=sApi->GetNextProgressiveItem(sChain[chain],SOH_RANDO_INF_SAVE);
 for(int i=sStart[chain];i<sStart[chain]+sLength[chain];i++){
  if(next&&!strcmp(next,sStages[i].key)){
   int level=sStages[i].level>minimum?sStages[i].level:minimum;
   return &sStages[sStart[chain]+level-1];
  }
 }
 return NULL;
}
static void DrawProgressivePreview(PlayState* play,GetItemEntry* entry,bool* handled){
 if(*handled)return;
 Player* player=GET_PLAYER(play);
 const Stage* stage=PreviewStage(entry,player&&(player->stateFlags1&PLAYER_STATE1_GETTING_ITEM));
 if(!stage || stage->chain==CH_HAMMER)return;
 const SOHCustomItemDefinition* d=sApi->FindCustomItem(stage->key);
 if(!d)return;
 GetItemEntry drawEntry=d->getItemEntry;
 drawEntry.drawItemId=(int)(stage-sStages);
 DrawStage(play,&drawEntry);
 *handled=true;
}

static void DrawProgressivePreview_Mm(PlayState* play, s16* drawId, bool* handled) {
    GetItemEntry entry = OotCompat_GetItemEntryForDrawId(*drawId);
    DrawProgressivePreview(play, &entry, handled);
}


static Actor* sThrownAxe;
static void ResetProjectiles(void);
static void ResetProps(void);
static u32 sLastReachFrame;
static bool sPrevSwing;
static u32 sSwingId,sLastHealSwing;
static Actor* sHealed[32];static int sHealedCount;
static void ReplaceHand(Player* p,int32_t limb,Gfx** dl,Gfx* original,Vec3f* pos){
 if(HumanWeapon(p)&&limb==PLAYER_LIMB_L_HAND&&p->heldItemAction==PLAYER_IA_SWORD_GILDED&&
    Level(CH_KOKIRI)>=3&&!GildedLook()&&!CustomItemRegistry_GetHeldKey()&&
    !sApi->GetWornEquipment(S2H_EQUIP_SLOT_SWORD)){
  *dl=(Gfx*)"__OTR__objects/object_link_child/gLinkHumanLeftHandHoldingRazorSwordDL";
  return;
 }
 int c=Weapon(p);if(limb!=PLAYER_LIMB_L_HAND||c<0||c==CH_MASTER||c==CH_BGS)return;
 *dl=(Gfx*)(LINK_IS_ADULT?gLinkAdultLeftHandClosedNearDL:gLinkChildLeftFistNearDL);
}
static void HandDraw(PlayState* play,Player* p,int32_t limb){
 int c=Weapon(p);if(c<0||c==CH_BGS||limb!=PLAYER_LIMB_L_HAND)return;

 if(c!=CH_MASTER&&!(c==CH_HAMMER&&sThrownAxe)){Matrix_Push();
  if(c==CH_HAMMER){
   // NEI's ExtEquip_ApplySwordDLMatrix precedes IKAxe_DrawAxe.
   Matrix_Translate(2028.26f,267.2f,-33.82f,MTXMODE_APPLY);
   Matrix_RotateZYX(-0x8000,0,0x4000,MTXMODE_APPLY);
   Matrix_Scale(5.0f,5.0f,5.0f,MTXMODE_APPLY);
  }
  DrawPieces(c,c==CH_HAMMER?2:Level(c),play);Matrix_Pop();
  Color_RGB8 color=Player_GetTunicColor(OotCompat_CurrentTunic());
  OPEN_DISPS(play->state.gfxCtx);
  Gfx_SetupDL_25Opa(play->state.gfxCtx);
  gDPSetEnvColor(POLY_OPA_DISP++,color.r,color.g,color.b,0);
  gSPLoadGeometryMode(POLY_OPA_DISP++,G_ZBUFFER|G_SHADE|G_CULL_BACK|G_LIGHTING|G_SHADING_SMOOTH);
  CLOSE_DISPS(play->state.gfxCtx);
 }
 if((c==CH_HAMMER||c==CH_KOKIRI)&&p->meleeWeaponState&&sLastReachFrame!=play->gameplayFrames){
  sLastReachFrame=play->gameplayFrames;
  Vec3f origin=p->bodyPartsPos[PLAYER_BODYPART_L_HAND];
  float reach=c==CH_HAMMER?2.0f:1.3333333f;
  for(int q=0;q<2;q++)for(int v=0;v<4;v++){
   Vec3f* point=&p->meleeWeaponQuads[q].dim.quad[v];
   point->x=origin.x+(point->x-origin.x)*reach;point->y=origin.y+(point->y-origin.y)*reach;point->z=origin.z+(point->z-origin.z)*reach;
  }
  EffectBlure* blur=Effect_GetByIndex(p->meleeWeaponEffectIndex[0]);
  if(blur&&blur->numElements>0){
   EffectBlureElement* edge=&blur->elements[blur->numElements-1];
   if(edge->state==1&&edge->timer==1){
    edge->p1.x=origin.x+(edge->p1.x-origin.x)*reach;
    edge->p1.y=origin.y+(edge->p1.y-origin.y)*reach;
    edge->p1.z=origin.z+(edge->p1.z-origin.z)*reach;
   }

  }
 }
}

static void HandDraw_Mm(Player* player, s32 limbIndex) {
    HandDraw(gPlayState, player, limbIndex);
}

static void SwordDamage(PlayState* play,int32_t flags,u8* damage){
 Player* p=GET_PLAYER(play);int c=Weapon(p);
 if(c==CH_KOKIRI){bool jump=(flags&(DMG_JUMP_KOKIRI|DMG_JUMP_MASTER|DMG_JUMP_GIANT))!=0;*damage=(Level(CH_KOKIRI)>=3?4:2)*(jump?2:1);}
 if(c==CH_HAMMER)*damage=MAX(*damage,8);
}
static uint8_t sKnifeUses = 8;
static u32 sKnifeHitSwing = UINT32_MAX;
static void SaveKnifeUses(void) { sApi->StorageSet(MOD_NAME,"knife_uses",&sKnifeUses,sizeof(sKnifeUses)); }
static void LoadKnifeUses(s32 file) {
    sKnifeUses = 8;
    sApi->StorageGet(MOD_NAME,"knife_uses",&sKnifeUses,sizeof(sKnifeUses));
    sKnifeHitSwing = UINT32_MAX;
}
static void KnifeOwned(const char* key, bool owned) {
    if (owned && !strcmp(key, KNIFE_KEY)) { sKnifeUses = 8; SaveKnifeUses(); }
}
static bool RegisterKnife(void) {
    S2HCustomEquipDefinition knife = { 0 };
    knife.structSize = sizeof(knife); knife.key = KNIFE_KEY;
    knife.iconPath = sKnifeIcon; knife.slot = S2H_EQUIP_SLOT_SWORD;
    knife.page = 0; knife.row = 0; knife.column = 5;
    knife.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    knife.vanillaBase = EQUIP_VALUE_SWORD_NONE;
    knife.meleeWeapon = Z64ITEMS_MELEE_GREAT_FAIRY_SWORD;
    knife.pauseText = "%rGiant's Knife&%wA heavy two-handed blade.&Its edge breaks after eight hits.";
    knife.getItemText = sStages[sStart[CH_BGS]].text;
    knife.getItemModelPath = "__OTR__objects/oot_object_gi_longsword/gGiBiggoronSwordDL";
    knife.getItemEntry.drawFunc = DrawStage;
    knife.getItemEntry.drawItemId = sStart[CH_BGS];
    return CustomEquipRegistry_Register(&knife);
}
static void CollisionDamage(Actor* victim,ColliderInfo* attack,float* damage){
 if(!gPlayState)return;Player* p=GET_PLAYER(gPlayState);int c=Weapon(p);
 if(attack!=&p->meleeWeaponQuads[0].elem&&attack!=&p->meleeWeaponQuads[1].elem)return;
 const char* sword=sApi->GetWornEquipment(S2H_EQUIP_SLOT_SWORD);
 if(sword&&!strcmp(sword,KNIFE_KEY)&&*damage>0&&sKnifeHitSwing!=sSwingId&&sKnifeUses){
  sKnifeHitSwing=sSwingId;
  if(--sKnifeUses==0){sApi->TakeOffEquipment(S2H_EQUIP_SLOT_SWORD);sApi->SetEquipmentOwned(KNIFE_KEY,false);}
  SaveKnifeUses();
 }
 if(c==CH_HAMMER)*damage=MAX(*damage,8.0f);
 if(c==CH_KOKIRI)*damage=MAX(*damage,Level(CH_KOKIRI)>=3?4.0f:2.0f);
 if(c==CH_MASTER&&gSaveContext.save.saveInfo.playerData.health>=gSaveContext.save.saveInfo.playerData.healthCapacity&&victim->category==ACTORCAT_BOSS)*damage=MAX(*damage,4.0f);
 if(c==CH_BGS&&*damage>0){
  if(sLastHealSwing!=sSwingId){sLastHealSwing=sSwingId;sHealedCount=0;}
  for(int i=0;i<sHealedCount;i++)if(sHealed[i]==victim)return;
  if(sHealedCount>=ARRAY_COUNT(sHealed))return;
  sHealed[sHealedCount++]=victim;Health_ChangeBy(gPlayState,16);gSaveContext.save.saveInfo.playerData.magic=MIN(gSaveContext.magicCapacity,gSaveContext.save.saveInfo.playerData.magic+4);
 }
}
static void Update(void);
static void UpdateActor(void* ptr);
static void SmashProp(Actor* actor);
static void AdvanceSwing(Player* player,PlayState* play);
static void Forget(s16 scene, s8 spawn){sHeldHammerPickup=NULL;sHeldHammerKey=NULL;sPrevSwing=false;sHealedCount=0;sPendingGiveChain=-1;sGiveStatus="";ResetProjectiles();ResetProps();}

// The registered beam and tomahawk actors are self-contained in this mod.
#include "upgrade_projectiles.inc.c"
#include "upgrade_props.inc.c"
// Native MM bosses consume DMG_SWORD_BEAM through their own damage tables.

// Native stages keep the original message, object and item-get animation metadata.
// Master Sword reuses the randomizer original; descriptions belong to new upgrades.
GetItemEntry ItemTable_Retrieve(int16_t getItemID);
static int16_t VanillaStageGetItem(const Stage* stage){
 if(stage->chain==CH_HAMMER)return GI_NONE;
 if(stage->chain==CH_KOKIRI)return stage->level==1?GI_SWORD_KOKIRI:stage->level==2?GI_SWORD_RAZOR:GI_SWORD_GILDED;
 if(stage->chain==CH_BGS && stage->level==3)return GI_SWORD_GREAT_FAIRY;
 if(stage->chain==CH_HOOK&&stage->level==1)return GI_HOOKSHOT;
 if(stage->chain==CH_HOOK)return GI_HOOKSHOT;
 return GI_NONE;
}
static SOHCustomItemRandomizer sPools[CH_COUNT];
static const int32_t sPoolSources[CH_COUNT] = {
 RG_MEGATON_HAMMER, RG_KOKIRI_SWORD, RG_MASTER_SWORD, RG_BIGGORON_SWORD, RG_PROGRESSIVE_HOOKSHOT
};
static bool RegisterOriginalMaster(void){
 memset(&sOriginalMasterEntry,0,sizeof(sOriginalMasterEntry));
 SOHModMessage message={0};message.structSize=sizeof(message);message.key="nei.upgrades.original_master_sword";
 // Outside rando there is no original GI text for pulling the pedestal sword.
 // Use its original randomizer wording; in rando the native builder runs below.
 message.text="You found the %gMaster Sword%w!";
 message.textGerman="Du hast das %gMaster-Schwert%w gefunden!";
 message.textFrench="Vous avez trouvé l'%gÉpée de Legende%w!";
 message.textboxType=TEXTBOX_TYPE_BLUE;message.autoFormat=true;
 sOriginalMasterMessage=sApi->RegisterMessage(&message);
 if(sOriginalMasterMessage==SOH_MOD_MESSAGE_INVALID)return false;

 return true;
}
// Keep direct stage debug gives equivalent to their playable equipment identities.
static void ReceiveStageAlias(const char* key) {
 const Stage* stage=StageForKey(key);
 if(stage && strcmp(key,StagePickupKey(stage)))
  CustomItemRegistry_ReceiveGetItem(StagePickupKey(stage));
}
static bool RegisterItems(void){
 for(int i=0;i<ARRAY_COUNT(sStages);i++){
  const Stage* stage=&sStages[i];SOHCustomItemDefinition d=Z64Items_Define(stage->key,stage->icon,NULL);
  if(stage->chain==CH_HAMMER&&stage->level==1)continue; // OoT Items owns the playable Hammer definition.
  int16_t vanilla=VanillaStageGetItem(stage);
  if(vanilla!=GI_NONE)d.getItemEntry=ItemTable_Retrieve(vanilla);
  if(stage->chain==CH_MASTER){
   d.getItemEntry=sOriginalMasterEntry;d.getItemEntry.modIndex=MOD_NONE;
   d.getItemEntry.textId=stage->level==1?sOriginalMasterMessage:0;
  }
  d.presentationFlags=SOH_ITEM_HIDE_FROM_KALEIDO|SOH_ITEM_HIDE_FROM_SAVE_EDITOR;d.getItemText=stage->text;d.displayName=stage->name;d.getItemEntry.drawFunc=DrawStage;d.getItemEntry.drawItemId=i;
  if (stage->chain==CH_HAMMER && stage->level==2) {
      d.namePath=sAxeName;
      d.getItemModelPath=sAxe;
      d.getItemEntry.drawFunc=DrawAxeGetItem;
  }
  if(strcmp(stage->key,StagePickupKey(stage)))d.onReceive=ReceiveStageAlias;
  if(!sApi->RegisterCustomItem(&d))return false;
 }
 for(int c=0;c<CH_COUNT;c++){
  sPools[c].structSize=sizeof(SOHCustomItemRandomizer);sPools[c].flags=SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT|SOH_CUSTOM_ITEM_RANDO_PROGRESSIVE;
  sPools[c].type=SOH_CUSTOM_ITEM_TYPE_ITEM;sPools[c].poolCount=(c==CH_KOKIRI||c==CH_HOOK||c==CH_BGS)?2:1;sPools[c].progressionGroup=sChain[c];

  SOHCustomItemDefinition d=Z64Items_Define(sPickup[c],sStages[sStart[c]].icon,NULL);
  const SOHCustomItemDefinition* first=sApi->FindCustomItem(StagePickupKey(&sStages[sStart[c]]));
  if(first)d.getItemEntry=first->getItemEntry;
  d.presentationFlags=SOH_ITEM_HIDE_FROM_KALEIDO|SOH_ITEM_HIDE_FROM_SAVE_EDITOR;d.getItemText=first&&first->getItemText?first->getItemText:sStages[sStart[c]].text;d.displayName=sStages[sStart[c]].name;d.getItemEntry.drawFunc=DrawStage;d.getItemEntry.drawItemId=ARRAY_COUNT(sStages)+c;d.randomizer=&sPools[c];
  if(c==CH_HAMMER&&first){
   d.getItemEntry=first->getItemEntry;
   d.getItemText=first->getItemText;
   d.getItemModelPath=first->getItemModelPath;
   d.getItemScale=first->getItemScale;
  }
  if(!sApi->RegisterCustomItem(&d))return false;
 }
 return true;
}
static void RegisterChains(void){
 static const SOHProgressiveAlias kokiri[]={{RI_SWORD_KOKIRI,ITEM_SWORD_KOKIRI,1},
  {RI_SWORD_RAZOR,ITEM_SWORD_RAZOR,2},{RI_SWORD_GILDED,ITEM_SWORD_GILDED,3}};
 // The Great Fairy's own sword is one more copy of the chain: Knife, then Biggoron's, then the healing GFS.
 static const SOHProgressiveAlias bgs[]={{RI_GREAT_FAIRY_SWORD,ITEM_SWORD_GREAT_FAIRY,1}};
 static const SOHProgressiveAlias hook[]={{RI_HOOKSHOT,ITEM_HOOKSHOT,1}};
 const SOHProgressiveAlias* aliases[]={NULL,kokiri,NULL,bgs,hook};
 const u16 aliasCount[]={0,ARRAY_COUNT(kokiri),0,ARRAY_COUNT(bgs),ARRAY_COUNT(hook)};
 SOHProgressiveReadLevel reads[]={ReadHammer,ReadKokiri,ReadMaster,ReadBgs,ReadHook};
 SOHProgressiveApplyLevel applies[]={ApplyHammer,ApplyKokiri,ApplyMaster,ApplyBgs,ApplyHook};
 for(int c=0;c<CH_COUNT;c++){
  SOHProgressiveLevel levels[3];for(int l=0;l<sLength[c];l++)levels[l].itemKey=StagePickupKey(&sStages[sStart[c]+l]);
  SOHProgressiveChain d={0};d.structSize=sizeof(d);d.key=sChain[c];d.pickupKey=sPickup[c];d.levels=levels;
  d.levelCount=sLength[c];d.firstLevel=1;d.aliases=aliases[c];d.aliasCount=aliasCount[c];d.readLevel=reads[c];d.applyLevel=applies[c];
  if(!sApi->RegisterProgressiveChain(&d))fprintf(stderr, "[oot_item_upgrades] Cannot register %s\n", d.key);
 }
}
static bool sReticleQuery;
static void ExtendReticle(CollisionContext* col,Vec3f* start,Vec3f* end,Vec3f* hit,CollisionPoly** poly,int32_t* bg,Actor* actor,uint32_t flags,bool* result){
 if(sReticleQuery||actor||!gPlayState||Level(CH_HOOK)<2)return;
 Player* p=GET_PLAYER(gPlayState);
 if(p->heldItemAction!=PLAYER_IA_LONGSHOT||CustomItemRegistry_GetHeldKey())return;
 float length=Math_Vec3f_DistXYZ(start,end),expected=776.0f*CVarGetFloat("gCheats.HookshotReachMultiplier",1.0f);
 if(fabsf(length-expected)>.5f)return;
 float factor=Level(CH_HOOK)>=3?4.0f:2.0f;
 Vec3f extended={start->x+factor*(end->x-start->x),start->y+factor*(end->y-start->y),start->z+factor*(end->z-start->z)};
 sReticleQuery=true;*result=BgCheck_AnyLineTest3(col,start,&extended,hit,poly,1,1,1,1,bg)!=0;sReticleQuery=false;
}
static void EasyGive(SaveContext* save){
 static int32_t selected;
 static const char* const choices[]={"Hammer / Gerudo Axe","Kokiri / Razor / Gilded Sword","Master / Real Master Sword","Giant's Knife / Biggoron / Great Fairy Sword","Hookshot / Longshot / Ultrashot"};
 if(!ModUI_BeginSection("OoT Item Upgrades - Easy Give"))return;
 ModUI_Combo("Weapon chain##nei_upgrade_give",&selected,choices,CH_COUNT);
 int chain=selected>=0&&selected<CH_COUNT?selected:0;
 const char* next=sApi->GetNextProgressiveItem(sChain[chain],SOH_RANDO_INF_SAVE);
 const Stage* stage=NULL;
 for(int i=0;i<ARRAY_COUNT(sStages);i++)if(next&&!strcmp(next,sStages[i].key)){stage=&sStages[i];break;}
 bool maxed=Level(chain)>=sLength[chain];
 char label[128];snprintf(label,sizeof(label),"Give %s##nei_upgrade_give",stage?stage->name:"item");
 if(maxed)ModUI_Text("Fully upgraded.");
 ModUI_BeginDisabled(!gPlayState||!stage||maxed||sPendingGiveChain>=0);
 if(ModUI_Button(label,0,0)){sPendingGiveChain=chain;sGiveStatus="Queued; resume the game to receive the item.";}
 ModUI_EndDisabled();
 if(sGiveStatus[0])ModUI_Text(sGiveStatus);
 ModUI_EndSection();
}
static void Menu(void){
 static const char* const shapes[]={"Razor Sword","Gilded Sword"};
 if(!SOH_MOD_API_HAS(sApi,RegisterMenuSidebar)||!SOH_MOD_API_HAS(sApi,RegisterMenuWidget))return;
 sApi->RegisterMenuSidebar("Mods","Equipment","gMods.Menu.Page",1);
 SOHModMenuWidget w={0};w.structSize=sizeof(w);w.section="Mods";w.sidebar="Equipment";
 w.type=SOH_MOD_MENU_COMBOBOX;w.label="Final Kokiri Sword shape";w.cvar=LOOK_CVAR;w.values=shapes;w.valueCount=2;w.defaultInt=1;
 w.tooltip="Appearance only. The final upgrade retains Gilded Sword damage with either shape.";sApi->RegisterMenuWidget(&w);
}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api){sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void){return &sRequirements;}
static void UpdatePlayer(Player* player) { Update(); }


// Actor storage is recycled after a pickup; its address alone is not an identity.
static void ForgetUpgradePickup(Actor* actor) {
 if(actor==sHeldHammerPickup){sHeldHammerPickup=NULL;sHeldHammerKey=NULL;}
}

static void ResolveHammerPickup(Actor* actor, PlayState* play, S2HGetItemEntry* entry, const char** key) {
 if(!key||!*key)return;
 const Stage* stage=StageForKey(*key);
 int chain=stage?stage->chain:-1;
 for(int c=0;c<CH_COUNT;c++)if(!strcmp(*key,sPickup[c]))chain=c;
 if(chain<0)return;
 bool held=actor && (actor->parent || (actor->id==ACTOR_EN_ITEM00 && actor->params==ITEM00_NOTHING &&
                                      ((EnItem00*)actor)->unk152>=0));
 if(held && actor==sHeldHammerPickup){*key=sHeldHammerKey;return;}
 const char* query=stage?StagePickupKey(stage):sPickup[chain];
 const char* next=sApi->GetNextProgressiveItem(query,S2H_RANDO_INF_SAVE);
 if(next)*key=next;
 if(held){sHeldHammerPickup=actor;sHeldHammerKey=*key;}
}

// Randomizer gives enter through the registry before applying the progressive level.
static void PrepareUpgradeMessage(const char* key) {
 if(!SaveBank())return;
 const Stage* stage=StageForKey(key);
 int chain=stage?stage->chain:-1;
 for(int c=0;c<CH_COUNT;c++)if(key&&!strcmp(key,sPickup[c]))chain=c;
 if(chain<0)return;
 const char* next=sApi->GetNextProgressiveItem(stage?StagePickupKey(stage):sPickup[chain],S2H_RANDO_INF_SAVE);
 if(next)CustomItemRegistry_PrepareAcquisitionMessage(next);
}

static void LoadHammerLook(s32 fileNum) {
    sHeldHammerPickup = NULL;
    SyncHammerIcon();
}

SOH_MOD_EXPORT void ModInit(void){
 if(!SOH_MOD_API_HAS(sApi,RegisterProgressiveChain)||!SOH_MOD_API_HAS(sApi,GetProgressiveLevel)||!SOH_MOD_API_HAS(sApi,GetNextProgressiveItem)||!SOH_MOD_API_HAS(sApi,HasResource))return;
 if(!sApi->HasResource(sRazorBlade)||!sApi->HasResource(sGildedBlade)||!sApi->HasResource(sGfs))return;
 // Parse the packaged NEI mesh now, before its first world/hand draw.
 if(!ResourceMgr_LoadGfxByName(sAxe))return;
 if(!SOH_MOD_API_HAS(sApi,RegisterMessage)||!RegisterOriginalMaster()||!RegisterItems()||!RegisterKnife())return;
 RegisterChains();
 RegisterProjectiles();Menu();LoadKnifeUses(0);
 SOH_REGISTER_HOOK(sApi,OnLoadFile,LoadKnifeUses);
 SOH_REGISTER_HOOK(sApi,OnCustomEquipOwnedChange,KnifeOwned);
 SOH_REGISTER_HOOK(sApi,OnGetItemDraw,DrawProgressivePreview_Mm);
 SOH_REGISTER_HOOK(sApi,OnResolveCustomGetItem,ResolveHammerPickup);
 SOH_REGISTER_HOOK(sApi,OnActorDestroy,ForgetUpgradePickup);
 SOH_REGISTER_HOOK(sApi,OnCustomItemReceive,PrepareUpgradeMessage);
 SOH_REGISTER_HOOK(sApi,OnLoadFile,LoadHammerLook);
 SOH_REGISTER_HOOK(sApi,OnSaveEditorInventory,EasyGive);
 SOH_REGISTER_HOOK(sApi,OnBgCheckLineTest,ExtendReticle);

 SOH_REGISTER_HOOK(sApi,OnPlayDrawEnd,DrawAxeAim);
 SOH_REGISTER_HOOK(sApi,OnPlayerUpdate,UpdatePlayer);SOH_REGISTER_HOOK(sApi,OnActorUpdate,UpdateActor);
 SOH_REGISTER_HOOK(sApi,OnPlayerResolveLimbDraw,ReplaceHand);SOH_REGISTER_HOOK(sApi,OnPlayerPostLimbDraw,HandDraw_Mm);
 SOH_REGISTER_HOOK(sApi,OnKaleidoResolveName,ResolveName);
 SOH_REGISTER_HOOK(sApi,OnInterfaceResolveButtonIcon,ResolveButtonIcon);SOH_REGISTER_HOOK(sApi,OnKaleidoResolveItemIcon,ResolveIcon);
 SOH_REGISTER_HOOK(sApi,OnResolveSwordDamage,SwordDamage);SOH_REGISTER_HOOK(sApi,OnCollisionResolveDamage,CollisionDamage);
 SOH_REGISTER_HOOK(sApi,OnSceneInit,Forget);
}

