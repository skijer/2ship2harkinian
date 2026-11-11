# Mods de 2Ship: instalación local

Actualizado el 2026-10-24. El usuario realiza las pruebas ingame.

## Estado comprobado

- Host Windows Release compilado y enlazado con ModApi major 2; `WindowsExports.def` e import library regenerados.
- Los 70 mods de `../2ship-unbound-mod-nei/mod-sdk/mods` compilan, enlazan y se instalan en `x64/Release/mods`.
- `build/mod-nei-verification.json` comprueba los manifiestos MM/API 2, la integridad ZIP, los tres puntos de entrada,
  los imports contra los exports del EXE y que cada paquete instalado coincide con el compilado.
- La antigua plantilla `actor_template.o2r` también se ha recompilado para API 2.
- La configuración anterior está en `build/mod-port-config-before.json`; los 70 mods actuales están habilitados.

El arranque de diagnóstico se detuvo en el aviso de la plantilla antigua. Se cerró únicamente ese proceso y no
se cargó ninguna partida. Después se recompiló la plantilla. No se volvió a arrancar el juego: las pruebas
ingame corresponden al usuario. Compilar y enlazar no confirma el comportamiento en una partida.

## Compilar de nuevo

Desde este checkout del host, en PowerShell:

```powershell
.\build-all.local.ps1 -Only All -Reconfigure
```

Solo mods, contra el host ya compilado:

```powershell
.\build-all.local.ps1 -Only Mods -Reconfigure
```

El script utiliza el proyecto de host existente y evita su regeneración automática. No actualiza dependencias,
no modifica submódulos y no usa fuentes de Ship como directorio de compilación de mods. Los proyectos del SDK
se generan en `build/mod-nei`. Los logs y el informe de verificación también quedan bajo `build/`.

## Ports completados en fuente

- `easy_equipment`: usa CustomEquipRegistry; admite asignar una pieza custom directamente a C para entrar a la
  rueda y restaura el equipo nativo al quitarla.
- `cucui-ganon`: actor propio, dibujo e intro propios, sustitución de Majora, pistas de Tatl, partículas y cámaras
  de MM, y victoria mediante `z64boss`. No usa el layout de BossGanon2.
- `oot_item_upgrades`: inventario y espadas nativos de MM; Hacha custom; Master Sword y Biggoron mediante el
  registro de equipo; cuchillo con ocho impactos; proyectiles y drops nativos; alcance del Hookshot por nivel.
- `not_enough_bottles`: `[1,5] [2,6] [3,7] [4,8] [Net] [Bottomless]`, conforme a la decisión del usuario.
  Contenidos MM, numeración de botones MM, HUD nativo, guardado y traslado de botellas previas de los slots 5/6.

El host expone `RandoInfCustom_IsLogicSimulation` para evitar que un callback de progresión cambie la propiedad
real de piezas custom durante una simulación. Las cabeceras públicas de ProgressiveItems y RandoItems ya no
arrastran sus dependencias internas de C++ a los mods. También se corrigieron firmas de callbacks del SDK wheel
y del transporte de hadas de Beetle.

## Assets y prueba pendiente del usuario

`oot_assets` incluye XML adicionales para Cucco, Malon, brazalete, Ganondorf/Phantom y los modelos usados por
los nuevos ports, en NTSC 1.0, 1.1 y 1.2. Proceden de los metadatos de assets del checkout autoritativo de Ship;
mantienen `OutName="oot_..." Game="OOT"`. Iconos y nombres existentes se conservan.

El `oot.o2r` ya instalado tiene 2748 entradas y no contiene los modelos nuevos. No se ha regenerado. Al probar,
OoT Asset Loader detectará esa extracción antigua y ofrecerá regenerarla con la ROM personal del usuario.
Después hay que reiniciar para montar el archivo actualizado. La extracción con los XML nuevos todavía no
está verificada.

Upstream y la mod store quedan fuera de esta tarea. No se han hecho commits ni staging en esta sesión.
