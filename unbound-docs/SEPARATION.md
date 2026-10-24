# Separación host / mods de 2Ship Unbound (espejo de Ship)

Auditoría del 2026-10-23. Todo lo de 2Ship está SIN COMMITEAR sobre el tag 5.0.1 (rama `2ship-unbound-modapi`).

## Modelo de Ship (la referencia)

| Rama | Worktree | Contenido |
|---|---|---|
| `unbound-modapi` | `Shipwright-unbound` | Host (soh + ModApi + Unbound) + `mod-sdk/` completo, `mod-sdk/mods/` solo con `README.md` |
| `unbound-mod-nei` | `Shipwright-unbound-mod-nei` | `unbound-modapi` + SOLO `mod-sdk/mods/*` (71 mods, `packs.json`) + `UNBOUND_REF` (commit del host contra el que se firma) |

La rama de mods se rebasa sobre la del host; sus commits no tocan código del host.
Fugas que hay que no repetir: `mod-sdk/include/z64aiming`, `mod-sdk/docs/{UI,ITEMS}.md` y
`mod-sdk/tools/sync_mm_zapd_assets.py` entraron por la rama de mods; eso es SDK y va al host.

## Plan para 2Ship

Dos ramas en el repo de 2Ship, mismo esquema:

1. `2ship-unbound-modapi` (worktree `2ship/2ship-unbound-modapi`): host + SDK.
2. `2ship-unbound-mod-nei` (worktree nuevo `2ship/2ship-unbound-mod-nei`): host + `mod-sdk/mods/*` + `UNBOUND_REF`.

### Reparto de lo que hay sin commitear (315 entradas en `git status`)

**Host (commit en `2ship-unbound-modapi`):**
- `mm/2s2h/{ModApi,Unbound,Network,ActorTable}/`, `GameInteractor_ModHooks.*`, `SaveManager/SaveSections.*`
- Todo lo modificado en `mm/2s2h/**`, `mm/include/**`, `mm/src/**` (154 overlays, 31 code, audio, kaleido, z_player)
- `mm/assets/custom/shaders/*`, `mm/CMakeLists.txt`, `CMakeLists.txt`, `CMake/copy-existing-otrs.cmake`
- `.gitmodules` + puntero del submódulo `libultraship` (ver "LUS compartido")
- `unbound-docs/`, `tests/`
- `mod-sdk/` salvo `mod-sdk/mods/<mod>/` (ver abajo)

**Mods (commit en `2ship-unbound-mod-nei`):** las 55 carpetas de `mod-sdk/mods/`.

**Se queda en la rama de host dentro de `mod-sdk/mods/`:** solo `README.md` (como Ship).
`mod-sdk/mods/PORT-STATUS.md` es un informe de build → mover a `unbound-docs/`.

**Fuera de cualquier commit:** `mod-sdk/tools/__pycache__/`, `decomp_diff.py`, `build-release.log`,
`2ship.o2r` (los dos últimos ya están en `.gitignore`; `__pycache__` y `decomp_diff.py` no).

**Casos frontera a decidir:**
- `oot_assets`, `oot_items`, `oot_equipment`: son mods de contenido de NEI → rama de mods. Si otros mods
  dependen de `oot_assets` como paquete base, se declara con `requires` en el manifest, igual que `mm_assets` en Ship.
- `cel_shading`, `turbo`, `mic_ocarina`, `trutefel_*`, `couples_mask_healing`: en Ship todos viven en la rama de
  mods; mismo sitio aquí.

### LUS compartido (hecho 2026-10-23)

Ship y 2Ship apuntan al mismo fork y commit: `https://github.com/skijer/libultraship.git`, rama
`unbound-toon`, `869ba4a2`. Ambos parten de LUS 485 (`7cb10226`, el de 2Ship 5.0.1). Frente a la rama local
vieja `2ship-unbound` (`a12e0e5e`, conservada), `unbound-toon` añade los mismos commits de Unbound rebasados,
WASM (solo Emscripten), upstream #1239 (prueba pyramid-like unificada) y el toon de Fast3D, que era
exactamente lo que 2Ship tenía sin commitear en LUS.
Copia del diff local previo: `lus_2ship_local_toon.patch` en el scratchpad de la sesión 1111f89c.
`Shipwright-unbound-mod-nei` sigue con `roborich/libultraship` en su `.gitmodules` porque su rama es anterior;
se arregla al rebasarla sobre `unbound-modapi`, no editándola (la rama de mods no toca el host).

### SDK: lo que tiene Ship y 2Ship no

| Falta en `2ship/.../mod-sdk` | Ship |
|---|---|
| `README.md`, `AGENTS.md`, `CLAUDE.md` | guía del SDK y reglas para agentes |
| `docs/` (ACTORS, ASSETS, BOSSES, CHECKS, DUNGEONS, FORMS, GETTING_STARTED, HOOKS, ITEMS, PUBLISHING, RANDOMIZER, SIGNING) | 2Ship tiene equivalentes sueltos en `unbound-docs/` (BOSS-MODAPI, DUNGEONS) |
| `include/z64boss/z64boss.h` | helper del registro de jefes (2Ship tiene el registro en el host pero no el header) |
| `templates/{boss,form,item}_template` | solo existe `actor_template` |
| `ci/signing/CMakeLists.txt`, `tools/sign_queue.py`, `tools/sign_request.py` | cola de firma del NEI-Store |
| `tests/test_tools.py` | tests de pack/firma/manifest |

Propios de 2Ship (se quedan): `tools/import_oot_assets.py`, `tools/sync_behavior_names.py`.
El `.cmake` se llama `S2HModSdk.cmake` frente a `UnboundModSdk.cmake`: unificar el nombre de la función
(`unbound_add_mod`) facilita compartir los `CMakeLists` de los mods.

## Paridad de la ModApi (tabla)

- Ship 159 servicios, 2Ship 168: **los 159 nombres de Ship existen en 2Ship**. Mismo `MOD_API_MAJOR = 1`.
- Extra solo en 2Ship: `ExtractRom`, `ExtractRomToModsFolder`, `GetHostGame`, `GetModApiMajor`,
  `HasModArchiveFile`, `HasVanillaBehavior`, `RegisterVBByName`, `SetCustomItemIconPath`,
  `IsCustomItemFormAllowed` (equivalente MM de la edad). Para 1:1 real, Ship debería recibir los genéricos
  (`GetHostGame`, `GetModApiMajor`, `HasModArchiveFile`, `HasVanillaBehavior`, `RegisterVBByName`,
  `SetCustomItemIconPath`); así un mod puede preguntar "¿en qué juego estoy?" en los dos.
- La tabla NO es binariamente intercambiable (orden y structs distintos). Compatibilidad = mismo fuente,
  dos builds.

## Paridad de hooks: solo los propios de la ModApi

Los hooks de upstream (HarbourMasters en Ship, el equipo de 2Ship en MM) y las compuertas VB pueden diferir:
son de cada motor. Lo que debe ser 1:1 son **los hooks que añadimos nosotros**.
Método: hooks de `GameInteractor_HookTable.h` que no están en `upstream/develop` (Ship) ni en el tag 5.0.1
(2Ship); autoría comprobada con `git log -S"DEFINE_HOOK(<hook>"`. Ship es la referencia (es lo más probado).

Propios: Ship 65, 2Ship 51. **Faltan en 2Ship 25 hooks propios de Ship:**

| Grupo | Hooks | Mods de Ship que los usan |
|---|---|---|
| Kaleido | `OnKaleidoInput`, `OnKaleidoItemCursor`, `OnKaleidoItemDraw`, `OnKaleidoItemEquip`, `OnKaleidoResolveItemIcon`, `OnKaleidoResolveName`, `OnKaleidoDebugEditor` | useful_medallions, kafei, not_enough_bottles, oot_item_upgrades, spiritual_stones |
| Iconos | `OnInterfaceResolveButtonIcon`, `OnMessageResolveItemIcon` | kafei, fierce_deity, useful_medallions, not_enough_bottles, oot_item_upgrades |
| Get-item | `OnResolveCustomGetItem`, `OnResolveItemGive`, `OnGetItemDraw`, `OnGetItemDrawPost` | rocs, not_enough_bottles, oot_item_upgrades |
| Jugador | `OnPlayerResolveAnimSite`, `OnPlayerResolveHeldCustomItem`, `OnResolveSwordDamage` | byrna, gerudo, pendant, trident, oot_item_upgrades |
| Ocarina | `OnOcarinaPlaybackNote` | formas, mic_ocarina |
| Mundo | `OnBgCheckLineTest`, `OnActorTalk` | oot_item_upgrades |
| Audio | `OnAudioTablesReady` | cucui-ganon |
| Consola | `OnConsoleGive` | — |
| Save Editor | `OnSaveEditorTabs`, `OnSaveEditorInventory`, `OnSaveEditorItemEligibility`, `OnSaveEditorItemPicker` | deku, oot_item_upgrades |

**Hecho en 2Ship (2026-10-23, `cl /Zs` limpio, sin build):** los 25 están en `GameInteractor_HookTable.h` con
ejecutores en `GameInteractor_ModHooks.*`; `OnPlayerResolveCustomItem` pasa a llamarse `OnPlayerResolveHeldCustomItem`.
Diferencias por ser MM (mismo nombre, tipo nativo):
- `OnGetItemDraw(play, s16* drawId, bool* handled)` / `OnGetItemDrawPost(play, s16 drawId)`: MM dibuja por GID, no hay `GetItemEntry`.
- `OnResolveCustomGetItem` recibe `struct S2HGetItemEntry*`; se dispara en `CustomItemRegistry_Give` y en el item soltado.
- `OnPlayerResolveAnimSite` usa `PlayerAnimationHeader**`; sitios: fidget, aterrizaje (2), daño, salto lateral (inicio y final).
- `OnKaleidoItemEquip`: `button` = convención `S2H_ITEM_BUTTON_*` (D-pad 5-8; Ship usa 4-7).
- Hooks de Kaleido también en la página de máscaras, con `slot + ITEM_NUM_SLOTS`.
- `OnResolveSwordDamage`: en `CollisionCheck_ApplyDamage` para `DMG_DEKU_STICK|SWORD|SPIN_ATTACK|SWORD_BEAM` (MM no tiene
  `CollisionCheck_GetSwordDamage`).
- `OnOcarinaPlaybackNote`: la ocarina de MM corre en el hilo de audio; las notas se encolan y el hook corre en el hilo de juego
  al empezar el frame siguiente.
- `OnAudioTablesReady`: una vez, tras inicializar los mods (en MM las tablas ya están listas antes).
- `OnInterfaceResolveButtonIcon`: al dibujar cada botón (C, B y D-pad), no al cargar el icono.
- `OnKaleidoDebugEditor`: si un mod lo toma en el draw, el update del editor vanilla se salta.
- `give custom_equipment "<key>"` llega por `OnConsoleGive`, como en Ship.

**Propios de 2Ship que faltan en Ship (4):** `OnActorDrawBegin`, `OnOcarinaInput`, `OnPlayerResolveCustomItem`
(ya renombrado al de Ship) y `AfterOwlSave` (solo MM).

Hooks de upstream que los mods necesitan y un motor no tiene, a resolver con un hook propio en ambos:
`OnOcarinaNote` (Anchor de SoH; los mods de formas y mic_ocarina lo usan) frente a `OnOcarinaInput` de 2Ship,
y `OnTransitionEnd` (gerudo).

## Paridad de helpers del SDK

- `z64wheel`: idéntico (34/34).
- `z64items`: idéntico salvo `Z64Items_SetAge` (Ship) ↔ `Z64Items_SetForms` (2Ship). Añadir los dos en ambos
  (en MM `SetAge` = humano/niño; en OoT `SetForms` = adulto/niño).
- `z64aiming`: divergido. Ship (commit 474b0c3b6, estilos de retícula): `DrawReticle`, `HoldState`,
  `IsReticleEnabled`. 2Ship: `DrawArrowReticle`, `DrawHookReticle`, `DrawReticleAt`, `FilterInput`,
  `GetAimLine`, `GetReticleScale`, `GetUntintedReticleDL`, `HoldCamera`. Hay que unir ambos conjuntos.
- Prefijos: `SOH_REGISTER_HOOK`/`SOH_MOD_EXPORT`/`SOHModApi` frente a `S2H_*`. Un header neutro
  (`unbound_mod.h`: `UNBOUND_REGISTER_HOOK`, `UNBOUND_MOD_EXPORT`, `UnboundModApi`) que mapee a uno u otro
  según el juego permite un único fuente por mod.

## Paridad de contenido (mods)

Ship 71, 2Ship 55. Faltan en 2Ship: `anim_viewer`, `bomb_arrows` (2Ship upstream ya trae Bomb Arrows),
`cucui-ganon` (jefe OoT), `easy_equipment`, `elemental_mirror_shield`, `goron_tunic`, `zora_tunic`,
`lynel_miniboss`, `not_enough_bottles` (MM tiene botellas nativas), `oot_item_upgrades`, `pak_loader`,
y los MM-nativos que no se portan (`deku`, `goron`, `zora`, `fierce_deity`, `mm_assets`, `mm_collectables`,
`mm_items`, `mm_wearables`). 29 mods compartidos no tienen aún la integración de red de Ship
(ver `mod-content-parity-status.json`); `oot_equipment` solo registra propiedad e iconos.
Ningún mod de 2Ship declara extensiones de lógica (`declares_logic_extensions: false` en todos).

## Traductor OoT → MM (primera versión, 2026-10-23)

- `mod-sdk/include/oot_compat/` (entra solo en el include path de todos los mods de MM):
  - `soh_api_names.h` y `soh/ModApi/**.h`: generados por `tools/gen_oot_compat.py` (184 alias `SOH*`→`S2H*`,
    44 cabeceras). Volver a ejecutarlo cuando cambie cualquiera de las dos ModApi.
  - `z64oot_names.h`: nombres de motor de OoT con el mismo contrato en MM (player, anims, matrices, sfx, colliders,
    partes del cuerpo, Item00, `Z64Items_SetAge` → forma humana, espadas de OoT por papel).
  - `soh/ResourceManagerHelpers.h`: los helpers que 2Ship exporta con la misma firma.
- `mod-sdk/tools/translate_mod.py <mod de Ship>`: copia el mod, incluye `z64oot_names.h`, renombra hooks en
  cadenas (`OnActorDraw`→`OnActorDrawBegin`), campos cruzados (`speedXZ`→`speed`, `linearVelocity`→`speedXZ`,
  `sceneNum`→`sceneId`, campos de collider), pone `"game": "mm"` en el manifest y escribe `TRANSLATION.md` con lo
  que necesita una persona: direcciones `func_80…`, constantes de OoT que MM no tiene, cabeceras de OoT, edad,
  layout del save, `GetItemEntry`.
- Prueba con los 71 mods de Ship (salida en scratchpad, `cl /Zs`): `turbo` compila tal cual; 37 se paran en una
  cabecera de OoT o de API que falta; el resto baja a 8-99 errores, casi todos contenido de OoT.

### Segunda tanda (2026-10-23): 77 mods de Ship, 24 compilan limpios, 3144 → 798 errores

Generado por `gen_oot_compat.py` (volver a ejecutarlo con `--ship <Shipwright-unbound>`):
- `oot_ids.h`: ids de OoT que MM no tiene, con valores que nada de MM usa (actores, objetos, escenas, GI y VB desde
  0x7E00; `PLAYER_IA` desde 0x58, porque `itemAction` es s8), SFX de OoT → `NA_SE_NONE` (MM no tiene su banco), y
  los números de `EVENTCHKINF/ITEMGETINF/INFTABLE` con su valor de Ship.
- `oot_rando.h` + `soh/Enhancements/randomizer/randomizer{Types,Enums}.h`: los enums del rando de OoT como `int32_t`
  con los valores que les da Ship (`MOD_NONE` = 0). No se copian las cabeceras de Ship: sus `RC_*` chocan con las
  de `Rando/Types.h`, que `z64save.h` incluye siempre.

A mano en `z64oot_names.h`:
- `IS_RANDO` = 0: en un mod de Ship solo protege datos del rando de OoT; el rando de MM llega por la ModApi.
  `Flags_{Get,Set}RandomizerInf`, `Randomizer_*`, `Entrance_PeekNextIndexOverride` responden como una partida sin
  rando. `Flags_*EventChkInf/ItemGetInf/InfTable`: nunca puestos (MM no tiene dónde guardarlos).
- `GetItemEntry` = `S2HGetItemEntry` y `ItemTable_Retrieve{,Entry}` leen la tabla de get-item de MM (ver host).
- Flags de estado del jugador: MM conserva los bits de OoT (mismas escrituras en los dos `z_player.c`; comprobado
  con `Actor_OfferGetItem`, `STATE3_2` = MIDAIR, `STATE3_4` = PAUSE_ACTION_FUNC, `STATE2_80000` = HOPPING). Solo
  `HOSTILE_LOCK_ON` y `START_CHANGING_HELD_ITEM` se movieron a `stateFlags3` en MM: esos y los exclusivos de OoT = 0.
- Medallones y piedras espirituales: `QUEST_MEDALLION_*`/`QUEST_*_EMERALD…` valen 0x40+ y
  `CHECK/SET/REMOVE_QUEST_ITEM` los resuelven contra `nei.medallion.*`/`nei.spiritual_stone.*` (items custom).
- Equipo sobre el sistema de MM (`CustomEquipRegistry`; sus huecos van en el orden de los `EQUIP_TYPE` de OoT):
  `CUR_EQUIP_VALUE` devuelve el número de OoT de la pieza puesta (las `oot.equip.*` de `oot_equipment`, o el
  `vanillaBase` de una pieza NEI); sin pieza, la espada y el escudo de MM por papel (Razor/Dorada = Maestra, la de
  la Fiera Deidad = Biggoron, Héroe = Hylian) y túnica/botas Kokiri. `Inventory_ChangeEquipment` se pone la pieza
  si se tiene, `CHECK_OWNED_EQUIP`/`EQUIP_INV_*` preguntan por la propiedad. `currentTunic` (MM no lo tiene) se lee
  de la túnica puesta. Constantes del jugador por papel: botas de hierro = Goron, en el fondo = Zora bajo el agua,
  Hylian = Héroe. Items que en MM son de `oot_items` (`ITEM_HAMMER`, `ITEM_BOOMERANG`, `ITEM_NAYRUS_LOVE`, 0xE0+):
  `INV_CONTENT` pregunta a `oot.hammer`/`oot.boomerang`/`oot.nayrus_love`.
- Magia: `magicTarget` → `magic - magicToConsume + magicToAdd`. Salas: `ROOM_BEHAVIOR_TYPE1/2_*` = `ROOM_TYPE_*` /
  `ROOM_ENV_*` (mismo orden).
- Envoltorios por firma: `EffectSsDeadDb/Dd_Spawn`, `Matrix_ToMtx`, `Actor_SetDropFlag{,JntSph}`,
  `Camera_SetCameraData`, `Actor_OfferGetItem` (C++), `BgCheck_EntityRaycastFloor5` → `_2`. Alias de tipos de
  curvas (`SkelAnimeCurve` → `SkelCurve`…), `PLAYER_LIMB_*`, `PLAYER_MODELGROUP_*` por papel, `CAM_STAT_*`,
  `ARROW_*`, `BOMB_*`, `HIT_SPECIAL_EFFECT_*`, items de OoT con pareja por papel (`ITEM_NUT`, `ITEM_SWORD_MASTER`…).

`translate_mod.py`:
- Copia a `<mod>/oot_include/` las cabeceras de OoT que MM no tiene (actores, assets), traducidas y con la guarda
  renombrada a `OOT_*` (la de `gameplay_keep.h` es la misma en los dos juegos y escondía la de MM).
  `S2HModSdk.cmake` añade `oot_include` al include path del mod.
- Rutas de assets de OoT (carpetas que existen en `soh/assets` de Ship) → `oot_<nombre>`, el espacio que monta
  `oot_assets`. Un símbolo de asset de OoT que MM también define (`gEffFlash1DL`) pasa a `NOMBRE_oot` en todo el
  mod, sin tocar las cadenas, para que la cabecera de MM (que piden sus actores, `ARROW_LIMB_MAX`) conviva.
- Miembros renombrados: los de collider con contexto (`.info` → `elem`/`base`), luz ambiental, curvas,
  `WaterBox`, cámara, culling, `Room`, `nextEntranceIndex`…; por tipo de actor (`EnArrow`, `EnBom`, `EnMThunder`,
  `ArmsHook`) solo sobre variables de ese tipo. Si el mod declara un struct propio con un campo renombrado, la
  declaración se renombra también. `ageRequirement = X` → `formMask = OOT_COMPAT_AGE_FORM_MASK(X)`.
- Quita lo que solo existe en Ship: el `trailType` de `EffectBlureInit2`.

Lo que queda (798) es contenido de OoT sin pareja en MM, para portar a mano o con un mod de contenido: martillo,
bumerán, botas y túnicas como items, `EnVm` (el Beamos de MM es otro actor), campos sueltos de `Player` (`unk_868`,
`knockbackType`, `getItemEntry`), `EnvironmentContext` (`gloomySkyMode`, `lightningMode`), `SaveContext.inventory`
plano, `PLAYER_MODELTYPE_*`, firmas de hooks de upstream en C++ (`cel_shading`) y `z64boss.h` (SDK de Ship).

### Tercera tanda (2026-10-23): 291 errores en los 71 mods reales, 32 limpios

Fuera del recuento: `mm_wearables`, `mm_collectables`, `mm_items`, `mm_assets` (traen MM a OoT; en MM ya es nativo),
`pak_loader` y `anim_viewer` (herramientas de Ship). Cambios: `SaveContext*` por puntero, `GID_*` de OoT →
`GID_NONE`, model types y fuerza por papel, `TOTAL_EFFECT_COUNT` = 46, tipo declarado manda en `.info` de
colliders, tiempo de `EnvironmentContext` (mismos offsets), `Player` 0x864-0x880 → 0xB34-0xB50, `knockbackType` →
`unk_B75`, typed maps de `EnBoom`/`EnHorse`/`ObjSyokudai`, shims de `soh/Enhancements/game-interactor/*`.
Corregido: `Magic_RequestChange(…, MAGIC_ADD)` sumaba al revés (el 5 de MM es el gasto Goron/Zora) y
`VB_RECIEVE_FALL_DAMAGE` se llama así también en 2Ship. Huecos de ModApi cerrados en 2Ship: `textGerman/French`
(+`textSpanish`) en `S2HModMessage` (tamaño mínimo, copia por `structSize`, idioma al abrir el texto) y
`poolSources/poolSourceCount` en `S2HCustomItemRandomizer` (ids `RI_*`; el traductor quita los de Ship, que son
ids del rando de OoT). Lo que queda: actores con otro struct en MM (`EnVm` 49 en dominion_rod, `EnTest`, `EnPoh`,
`MirRay`), el sistema de equipo/slots de OoT en `easy_equipment`, entradas de lugares de OoT (`minish_cap`),
regs de la HUD de OoT y campos de UI de Ship.

### Equipo de OoT con comportamiento (2026-10-23, `cl /Zs` limpio, sin build)

- Host (aprobado): UN VB, `VB_PLAYER_USE_FORM_ABILITY(Player*, SOHPlayerAbility)`, al final del enum, despachado
  por `Player_HasFormAbility` en `z_player.c`; el valor por defecto es la comprobación de forma original, así que
  sin mods MM no cambia. Habilidades (`PlayerHookTypes.h`): `HEAT_PROOF` (suelo que quema + lava, antes solo
  Goron), `ZORA_SWIM` (impulso con A en vez de bucear, barrera con R y su dibujo), `WATER_BREATHING` (timer bajo
  el agua), `BOTTOM_WALK` (humano con botas `ZORA_UNDERWATER`: animaciones de tierra y agarrar/usar items en el
  fondo), `ANCHORED` (corrientes y hielo), `SLIDE` (todo suelo resbala). `VanillaBehaviorNames.inc` regenerado.
- SDK: `include/z64equipment/z64equipment.h` (solo cabecera). Papeles por pieza: `TUNIC_HEAT` (además anula
  golpes de fuego en `OnCollisionResolveDamage`), `TUNIC_WATER`, `BOOTS_IRON` (fuerza `ZORA_UNDERWATER` en agua
  honda y retoca los REG de botas en tierra cada frame, como NEI), `BOOTS_HOVER` (19 frames a la misma altura al
  salir de un borde, anillo `gHoverBootsCircleDL` de `oot_assets`), `SHIELD_WOOD` (se quema al bloquear fuego).
  Espadas y escudos no necesitan papel: el `vanillaBase` del registro ya los hace los de MM.
- `oot_equipment` (mod-nei): las 7 piezas + Túnica Zora, ya equipables (sin `OWNERSHIP_ONLY`), con textos MM.
  Maestra = Razor sin desgaste (`VB_LOWER_RAZOR_SWORD_DURABILITY`), Biggoron = Dorada a dos manos
  (`meleeWeapon` TWO_HANDED), Deku = Héroe que arde, Espejo = Espejo de MM. Modelos: los de MM por ahora.
- `goron_tunic` (mod-nei): Protección de Daruk traducida del mod de Ship; `Wearing()` corregida a mano (en MM la
  túnica es pieza custom, la condición de Ship "sin pieza custom" sería siempre falsa). Depende de `oot_equipment`.
- Sin probar en juego: hundirse con las de hierro (NEI lo hacía desde dentro de `func_8083BB4C`; aquí lo fuerza el
  mod en `OnPlayerUpdate`), el planeo de las levitadoras y el anillo.

Cambio de host para esto: `S2HGetItemEntry` tiene ya el layout del `GetItemEntry` de Ship (con `drawFunc`, `gi`,
`gid`); `CustomItemRegistry_DrawAcquisition` llama a `drawFunc`, si no hay modelo dibuja `GetItem_Draw(gid)`, y
`VanillaItems_GetItemEntry` rellena la entrada desde `sGetItemTable` de `z_player.c`. Como cambia el layout en medio
de `S2HCustomItemDefinition`, `S2H_MOD_API_MAJOR` pasa de 1 a 2: los mods compilados antes se rechazan y el store
busca builds `2`.

Huecos de API de 2Ship que salen al traducir: `ModMenu/ModUI.h` (widgets con tema), `ActorDB`, `frame_interpolation`,
`ModDungeons/{DungeonEntrancePairs,ModDungeonsHost,RandoEnumNames}.h`, `SOHCustomEnemy`/`SOH_ACTOR_ENEMY*`,
`SOHCustomItemAppearance`, `SOHRegisteredActor`, `SOH_MOD_MENU_COMBOBOX`, `SOH_PLAYER_MOTION_WATER_SPEED`,
`ResourceMgr_{LoadAudioSample,TexIsRaw}`.

**Huecos cerrados (2026-10-23, `cl /Zs` limpio, sin build):**
- Registro de actores: `enemyFlags/enemyParams/enemySpawnHeight/naviHint` (añadidos DETRÁS de `reset`, que 2Ship ya
  tenía), `S2HCustomEnemy`, `FindCustomId`, `GetEnemyCount/At`, `FindEnemyById`, `ActorRegistry_List`.
  `naviHint` = línea de Tatl: `ModMessages_RegisterTatlHint` usa hintId 0xA0-0xFE (texto 0x1900+id).
  Consola: `spawn custom_actor|custom_enemy "<key>" <data>`.
- `ModMenu/ModUI.{h,cpp}` portado (capa C sobre ImGui con el tema del menú); añadido a mano al `2ship.vcxproj`.
- `S2H_MOD_MENU_COMBOBOX` (+ `values/valueCount`); la copia del widget respeta `structSize`.
- `S2HCustomItemAppearance` + `appearance` y `displayName` al final de la definición; los otros jugadores ven la
  apariencia en su hueso (OnPlayerPostLimbDraw). `S2H_CUSTOM_PICKUP_VARIANT_MIN_SIZE`.
- `SOH_PLAYER_MOTION_WATER_SPEED` en el mismo punto que Ship.
- `ResourceMgr_LoadAudioSample` (tipo nativo `Sample`) y `ResourceMgr_TexIsRaw` exportados.
- `registry_dump` + `nocturne-registry.json` tras cargar mods (con `forms` en lugar de `age`).
- Compat: `soh/ActorDB.h` (entrada = `ActorProfile` vivo), `soh/frame_interpolation.h`, `SoundFontSample`→`Sample`.

Quedan sin pareja, a propósito: ranuras de jefes de OoT (MM tiene las suyas) y las 3 cabeceras internas del rando de
Ship (`DungeonEntrancePairs`, `ModDungeonsHost`, `RandoEnumNames`). Lo que MM tiene y Ship no (para el 1:1 inverso):
`reset` en la definición de actor, `onPauseToggle/isPauseActive/getItemScale` en items, `getItemScale` en variantes.

Otros cambios de la misma tanda: `z64aiming` de MM con la API de Ship (8 mods migrados; el host quita el stick a
Link mientras apunta con un item custom, en `CustomItemRegistry`) y `OnOcarinaNote` en 2Ship (encolado como
`OnOcarinaPlaybackNote`).

## Traductor OoT → MM en el SDK: diseño inicial

Objetivo: escribir un mod de items/equipo/coleccionables una vez (contra Ship) y obtener la build de MM sin
reescribirlo. No es posible a nivel binario (structs, tabla y exports distintos): es un único fuente con dos builds.

1. `mod-sdk/include/unbound/unbound_mod.h` (idéntico en ambos SDK): `UNBOUND_GAME_OOT`/`UNBOUND_GAME_MM`,
   `UnboundModApi`, `UNBOUND_REGISTER_HOOK*`, `UNBOUND_MOD_EXPORT`, mapeados a `SOH_*` o `S2H_*`.
2. `mod-sdk/include/z64game/` (capa de motor, header-only): nombre neutro → símbolo de cada juego. Primera tanda
   = la tabla MM→OoT ya medida: `Player_SetupAction`↔`Player_SetAction`, `Player_ZeroSpeedXZ`↔
   `Player_StopHorizontalMovement`, `LinkAnimation_PlayOnce`↔`Player_Anim_PlayOnce`, sfx en posición,
   `Gfx_SetupDL_25Opa`↔`Gfx_SetupDL25_Opa`, `Matrix_RotateY`↔`Matrix_RotateYF`, `actorLists.head`↔`.first`,
   tipos de Item00, edad↔forma (`Z64Items_SetAge`/`SetForms` en los dos). Fuente: `nei_oot_compat.h`,
   `nei_player_shims.c` y el mapa soh⇄mm de las notas del proyecto.
3. `mod-sdk/tools/translate_mod.py`: reescribe un mod de Ship a `unbound_mod.h` + `z64game`, y lista lo que no
   sabe traducir (funciones `func_80xxxxxx` de z_player, actores/escenas de OoT, IDs de rando) para dejarlo bajo
   `#if UNBOUND_GAME_MM`. El CI compila cada mod para los dos juegos y publica `oot:`/`mm:` en el store.

Requisito previo: hooks propios 1:1 (sección anterior). Sin ellos un mod traducido compila pero no se engancha.

## Orden de trabajo propuesto

1. Commitear el host en `2ship-unbound-modapi` (LUS ya en `skijer/unbound-toon`; los mods ya no están en él).
2. Hecho: `mod-sdk/tools/split_mods_to_branch.py` movió los mods al worktree `2ship/2ship-unbound-mod-nei` (rama
   `2ship-unbound-mod-nei`, sin commitear). Tras commitear el host: rebasar esa rama sobre él y volver a ejecutar el
   script para refrescar `UNBOUND_REF`. Mientras tanto se compilan con
   `python mod-sdk/build_mods.py --mods-dir ../2ship-unbound-mod-nei/mod-sdk/mods --game <carpeta>`.
3. SDK: docs, templates, `z64boss`, firma/CI, tests (desde Ship).
4. Hecho: los 25 hooks propios en 2Ship. Pendiente: `OnActorDrawBegin`/`OnOcarinaInput` en Ship; `z64aiming`/`z64items` unidos.
5. Traductor: `unbound_mod.h` → `z64game` → `translate_mod.py`; luego mods que falten y red.
