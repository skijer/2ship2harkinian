# Cucui Ganon para 2Ship

Actor custom que ocupa la ranura de Majora en `SCENE_LAST_BS`. Activa **Cucui Ganon battle** en Mods > Gameplay y entra de nuevo a la batalla final. Tiene dibujo propio con `gGanondorfSkel`, intro y cÃ¡mara propias, y conserva el combate y la letra de la versiÃ³n de Ship.

Requiere los assets de OoT actualizados. Tras el remate se llama al helper de victoria de MM: en la ranura de Majora inicia los crÃ©ditos; el helper conserva las recompensas nativas para ranuras de mazmorra.

CompilaciÃ³n y enlace verificados. El usuario realiza las pruebas ingame.

## Combate

Ganondorf comienza con **80 puntos internos** y llama a los cuatro Phantom cuando llega a **40**. En fase 1, los golpes y flechas por la espalda quitan 1 punto. Cada flecha por la espalda y cada esfera reflejada que le alcance acumulan un impacto; al tercero cae y queda vulnerable durante 120 actualizaciones. Una esfera que completa el contador quita 5 puntos. La electricidad acumulada se ve antes de caer, y el punto de fijaciÃƒÂ³n baja junto a ÃƒÂ©l durante el aturdimiento.

El idle mira a Link y carga una bola visible en la mano, disparada en el giro siguiente. El giro con ambas manos arriba carga otra bola roja de fuego. Salsa 2 lanza tres bolas por cada mano, con trayectorias parabÃƒÂ³licas **laterales en XZ** que descienden hasta el piso. Las bolas de la vuelta M5 tambiÃƒÂ©n salen de las manos, suben en arcos y caen en ocho direcciones. El avance levanta rocas; el giro inmune manda una onda de explosiones y rocas hacia Link; el dash rÃƒÂ¡pido tiene cono frontal y regresa al centro. Retroceso dispara una bola elÃƒÂ©ctrica mÃƒÂ¡s lenta que el resto. Las bolas tienen fuego, hielo, electricidad o explosiÃƒÂ³n y pueden ser devueltas con la espada. El reflejo acepta un margen de espada de 55 unidades y revisa toda la trayectoria recorrida por la bola en cada actualizaciÃƒÂ³n antes de aplicar daÃƒÂ±o. La colisiÃƒÂ³n de daÃƒÂ±o con Link usa 20 unidades horizontales y solo la altura central de su cuerpo; el alcance de las bolas reflejadas contra Ganondorf permanece igual.

Los cuatro Phantom usan `object_gnd` a 80 % de tamaÃƒÂ±o, sin tridente visible. Sus animaciones de Blender se exportaron al esqueleto original: Salsa, paso lateral, giro, desplazamiento y dos miradas. Lanzan bolas desde las manos y sus vueltas o desplazamientos pueden golpear a Link. Se pueden fijar como objetivo. Cada uno muere por un golpe **desde su espalda**, por una **flecha bomba** del mod Bomb Arrows (la explosiÃƒÂ³n puede alcanzarlo desde cualquier lado) o por una esfera reflejada dirigida a ÃƒÂ©l. Mientras quede alguno, Ganondorf estÃƒÂ¡ protegido. Cuando mueren los cuatro, **una sola esfera reflejada** aturde al jefe; durante esa ventana vuelve a poder recibir golpes por la espalda. Cada recuperaciÃƒÂ³n cierra la ventana.

Al llegar Ganondorf a 1 punto, un golpe de espada por la espalda durante su aturdimiento activa el remate. Link usa la acciÃƒÂ³n de corte final del juego; Ganondorf reproduce animaciones humanas de derrota del castillo, suelta sangre roja, grita mientras sube y se desintegra con partÃƒÂ­culas como un Phantom. El mod notifica la victoria con `ReportBossDefeated` (las estadÃƒÂ­sticas marcan la partida como completada, igual que con Ganon) y devuelve el control a Link. Al entrar, **corren los crÃƒÂ©ditos**, como tras el Ganon vanilla. Esta coreografÃƒÂ­a y el portal requieren prueba ingame de cÃƒÂ¡mara, efectos y transiciÃƒÂ³n.

Al encenderse los focos del intro aparece el cartel original de jefe, con **papi chulo** arriba y **Ganondorf** en grande. Las descripciones C-Up de Tatl para Ganondorf y los Phantom estÃƒÂ¡n en inglÃƒÂ©s. Desde el comienzo del baile de combate se dibuja la letra espaÃƒÂ±ola en una capa visual translÃƒÂºcida independiente. `ganondorf_lyrics.inc` conserva los 169 versos originales; el dibujo simplifica tildes porque la fuente de pantalla de OoT no incluye esos glifos. La capa nunca usa el sistema de mensajes del juego: Tatl, el giro mÃƒÂ¡gico y la pausa pueden funcionar mientras aparece la letra. Cuando aparece un mensaje real del juego, la letra deja de dibujarse y vuelve al terminar.

La primera sincronizaciÃƒÂ³n divide la letra en 610 pulsos a **125 BPM** (4:52.8). Cada frase ocupa 2, 4 o 6 pulsos segÃƒÂºn su longitud. La letra vuelve al principio cuando termina y la mÃƒÂºsica sigue sonando. Los tiempos por verso aÃƒÂºn requieren comprobaciÃƒÂ³n ingame, porque la secuencia MIDI no incluye marcas de voz.

**Jefe final registrado:** el mod se registra con `RegisterBoss` en el hueco de Ganon (`SOH_BOSS_SLOT_GANON`) y pelea en su propia arena. Fuera del randomizer manda la casilla **Cucui Ganon battle**; en una semilla, la pelea entra si la casilla estaba activa al generarla, el spoiler lo apunta en `customBosses` y la partida lo conserva aunque luego se desactive la casilla. La lÃƒÂ³gica es la del Ganon vanilla (mÃƒÂ¡s estricta, nunca rompe la semilla). El Phantom sÃƒÂ³lo aparece invocado por este combate; no se anuncia como enemigo suelto al Enemy Randomizer. El mod no modifica el motor ni el SDK.

Para grabar todo M5 seguido con Link libre, activa tambiÃƒÂ©n **M5 completo para grabar**. Las CVar son `gMods.CucuiGanon.DanceEnabled` y `gMods.CucuiGanon.PlayFullM5`.

## MÃƒÂºsica y fuentes

El mod interrumpe `NA_BGM_OPENING_GANON` cuando el juego lo inicia, reproduce intro RC1066 y entrada, despuÃƒÂ©s una pasada del loop sin las tres pistas `04 RC1066 ch3: Vocal Aahs Ganon`, y luego el loop completo. La frase dura 60 tiempos a 125 BPM; las notas de eco posteriores se trasladan al siguiente ciclo. Usa las fuentes 22, 27 y 32 de `oot.o2r`.

Las animaciones originales y todas las mallas se referencian por rutas OTR para mantener compatibilidad con los mods de recursos.
