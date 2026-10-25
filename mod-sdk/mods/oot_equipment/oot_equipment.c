#include "z64items.h"
#include "z64equipment.h"

#define ICON(name) "__OTR__textures/oot_icon_item_static/" name
#define NAME(name) "__OTR__textures/oot_item_name_static/" name
#define MASTER_SWORD_KEY "oot.equip.master_sword"

static const S2HModApi* sApi;
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), NULL, 0 };

typedef struct {
    const char* key;
    const char* icon;
    const char* name;
    const char* pauseText;
    const char* getItemText;
    u8 slot;
    u8 column;
    u16 vanillaBase;
    u8 meleeWeapon;
    Z64EquipmentRole role;
} OotPiece;

// Swords and shields stand for MM's own by role (vanillaBase); tunics and boots act through z64equipment.
static const OotPiece sPieces[] = {
    { MASTER_SWORD_KEY, ICON("gItemIconSwordMasterTex"), NAME("gMasterSwordItemNameENGTex"),
      "%rMaster Sword&%wThe Blade of Evil's Bane. Its edge never dulls.",
      "You got the %rMaster Sword%w!&The sacred blade that seals the darkness. Unlike the Razor Sword, it never "
      "loses its edge.",
      S2H_EQUIP_SLOT_SWORD, 4, EQUIP_VALUE_SWORD_RAZOR, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_ROLE_NONE },
    { "oot.equip.biggoron_sword", ICON("gItemIconSwordBiggoronTex"), NAME("gBiggoronsSwordItemNameENGTex"),
      "%rBiggoron's Sword&%wSwing it with %y\xA0%w using both hands. No shield can be held with it.",
      "You got %rBiggoron's Sword%w!&A masterpiece forged by the giant Goron smith. Its two-handed swing hits "
      "harder than any blade.",
      S2H_EQUIP_SLOT_SWORD, 5, EQUIP_VALUE_SWORD_GILDED, PLAYER_MELEEWEAPON_SWORD_TWO_HANDED,
      Z64EQUIPMENT_ROLE_NONE },
    { "oot.equip.deku_shield", ICON("gItemIconShieldDekuTex"), NAME("gDekuShieldItemNameENGTex"),
      "%rDeku Shield&%wHold %yR%w to block. It burns if it blocks fire.",
      "You got a %rDeku Shield%w!&A wooden shield from the forest. Fire will turn it to ash.",
      S2H_EQUIP_SLOT_SHIELD, 4, EQUIP_VALUE_SHIELD_HERO, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_SHIELD_WOOD },
    { "oot.equip.mirror_shield", ICON("gItemIconShieldMirrorTex"), NAME("gMirrorShieldItemNameENGTex"),
      "%rMirror Shield&%wHold %yR%w to block and reflect light.",
      "You got the %rMirror Shield%w!&The polished shield of the Spirit Temple reflects light and magic.",
      S2H_EQUIP_SLOT_SHIELD, 5, EQUIP_VALUE_SHIELD_MIRROR, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_ROLE_NONE },
    { "oot.equip.goron_tunic", ICON("gItemIconTunicGoronTex"), NAME("gGoronTunicItemNameENGTex"),
      "%rGoron Tunic&%wFire, burning floors and lava cannot hurt you.",
      "You got a %rGoron Tunic%w!&Made by the Gorons to withstand heat. Flames and lava will not harm you while "
      "you wear it.",
      S2H_EQUIP_SLOT_TUNIC, 4, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_TUNIC_HEAT },
    { "oot.equip.zora_tunic", ICON("gItemIconTunicZoraTex"), NAME("gZoraTunicItemNameENGTex"),
      "%rZora Tunic&%wBreathe underwater. Press %y\x9F%w while swimming to speed ahead.",
      "You got a %rZora Tunic%w!&Wear it and you can breathe underwater and swim like a Zora.",
      S2H_EQUIP_SLOT_TUNIC, 5, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_TUNIC_WATER },
    { "oot.equip.iron_boots", ICON("gItemIconBootsIronTex"), NAME("gIronBootsItemNameENGTex"),
      "%rIron Boots&%wSo heavy you sink in water and walk along the bottom. Currents cannot push you.",
      "You got the %rIron Boots%w!&Too heavy to run in, but they will take you to the bottom of deep water.",
      S2H_EQUIP_SLOT_BOOTS, 4, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_BOOTS_IRON },
    { "oot.equip.hover_boots", ICON("gItemIconBootsHoverTex"), NAME("gHoverBootsItemNameENGTex"),
      "%rHover Boots&%wWalk off a ledge to float for a moment. The ground is slippery in them.",
      "You got the %rHover Boots%w!&Their magic lets you walk on air for a short while, but they grip nothing.",
      S2H_EQUIP_SLOT_BOOTS, 5, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_BOOTS_HOVER },
};

static void RegisterPiece(const OotPiece* piece) {
    S2HCustomEquipDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = piece->key;
    definition.iconPath = piece->icon;
    definition.namePath = piece->name;
    definition.pauseText = piece->pauseText;
    definition.getItemText = piece->getItemText;
    definition.slot = piece->slot;
    definition.row = piece->slot;
    definition.column = piece->column;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.vanillaBase = piece->vanillaBase;
    definition.meleeWeapon = piece->meleeWeapon;
    if (CustomEquipRegistry_Register(&definition) && piece->role != Z64EQUIPMENT_ROLE_NONE) {
        Z64Equipment_SetRole(piece->key, piece->role);
    }
}

// The Master Sword stands for the Razor Sword but keeps its edge.
static void KeepMasterSwordEdge(bool* should, va_list args) {
    if (CustomEquipRegistry_IsWorn(MASTER_SWORD_KEY)) {
        *should = false;
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    for (u32 i = 0; i < ARRAY_COUNT(sPieces); i++) {
        RegisterPiece(&sPieces[i]);
    }
    Z64Equipment_Install(sApi);
    sApi->RegisterVB(VB_LOWER_RAZOR_SWORD_DURABILITY, KeepMasterSwordEdge);
}
