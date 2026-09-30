# ps2-polish: atacar Docs/PENDING.md (0.25.0)

## Contexto

`ps2-shipping` (0.24.0) está en `main`: ShooterGame va a 30 fps en PCSX2 con el arte real y paridad CS. Al jugarlo,
el usuario confirmó que el sonido, la vibración, el control y los 30 fps van bien, y dejó en `Docs/PENDING.md` lo que
falta: cuatro bugs (personajes que desaparecen, bots con cuchillo a distancia, bots sin rumbo, agacharse no mejora la
precisión) y funciones de juego y UI (minimapa real, cielo, menú principal con mapa, dificultad, rondas y número de
bots, selección de bando con reparto, menú de pausa, agacharse en toggle, soltar y recoger armas, fuente y UI nuevas,
tabla para el scoreboard). Single player contra bots; sin pantalla dividida. Fuente elegida: una sans libre al estilo
de CS 1.6 (DejaVu Sans Condensed o Noto Sans, OFL).

Objetivo: cerrar todo eso con buenas prácticas y nada legacy, manteniendo 30 fps en PCSX2, y publicar 0.25.0.

## Forma de trabajo (igual que ps2-shipping)

- Rama `plan/ps2-polish` desde `main`; un commit por fase; sin push; al final se integra a `main` como un único commit
  (squash), como pidió el usuario la vez anterior.
- El plan se copia a `Docs/PLANS/ps2-polish.md` (en español, `### Pn · título (S/M/L) — estado`, `Estado:`,
  `Desviaciones:`); cada fase tacha su punto en `Docs/PENDING.md`.
- Gates por fase: `RunGates.bat -PS2` (Lint, tests, CheckReimport con ShooterGame, Smoke, BotMatch x2, Validate,
  Package PS2); `MeasurePS2` cuando la fase toca render, IA o HUD (no bajar de 30 fps, p95 ≤ 33,5 ms).
- Reglas vigentes: D7 (ninguna función del GS sin escena de conformidad), D10 (lo sustituido se borra en el mismo
  commit y va a `CheckBannedApis.ps1`), API y nombres de UE, CRLF en `.bat/.ps1`, arte por scripts deterministas (D6).
- Paralelizable con agentes en worktrees (P1–P4 independientes; P5 antes de P6–P9).

## Fases

### Bugs

**P1 · Personajes que desaparecen (S) — hecha**
- Causa encontrada: el `Mesh` del `ACharacter` (y `Mesh1P`) se queda `Static` (`SceneComponent.h:50`; solo la cápsula
  es Movable en `Character.cpp:51`). `FScene::GatherPrimitives` (`Renderer/Private/Scene.cpp:288`) solo recalcula la
  celda de lo no estático, así que el cuerpo conserva la celda de su spawn y los portales lo descartan según el ángulo;
  el arma (Movable, `ShooterWeapon.cpp:94`) sigue dibujándose. Después, sin `LastRenderTime`, la pose se congela
  (`SkeletalMeshComponent.cpp:182`) y el arma queda flotando en el socket.
- Arreglo: `ACharacter` crea el `Mesh` como `Movable` (como UE), igual `Mesh1P`, y el proxy skeletal avisa con un
  `ensure` si llega Static a una escena con celdas.
- Test: un peón que cruza de una celda a otra sigue visible desde la celda vecina (`GS.Scene.CellsAndPortals`
  ampliado); `ObjectsCulledByCells` no cuenta peones.

Estado: hecha.
- Causa confirmada: sin el arreglo, el test ampliado falla (el peón que entra en A sigue contado en
  `ObjectsCulledByCells` y no se dibuja) y salta el `ensure` nuevo. En Win64, con la misma semilla y vista
  (`ViewFrom -1100 -1600 150 0 0`, fotograma 1100), un T en el túnel de B no aparece antes del arreglo y sí después; en
  las demás vistas probadas la imagen es idéntica.
- `ACharacter` hace Movable su `Mesh`; ShooterGame, `Mesh1P` y la cámara. El `ensure` está en
  `FScene::GatherPrimitives`: un skeletal Static en un mapa con celdas.
- Nada dependía de que el cuerpo fuera Static: la luz ya lo trataba como dinámico, no tiene cuerpo de colisión propio, y
  el BotMatch no cambia (`9 round(s), CT 3 - T 6, 55 kill(s)`).
- PCSX2 (`MeasurePS2`, fila «ps2-polish P1» en Budgets.md): 29,95 fps, p50/p95/p99 33,5 ms; la escena 8,53 ms (8,42
  en 0.24.0).

**P2 · Precisión al agacharse, andar y estar quieto (S) — hecha**
- Causa: `CrouchingSpreadMod` = 0,8 para todas las armas y nadie lo ajusta (`ShooterWeapon_Instant.h:97`); andar no
  tiene término propio (`GetCurrentSpread`, `ShooterWeapon_Instant.cpp:156`).
- Arreglo con los factores de CS 1.6 por arma (agachado ≈ 0,5–0,65 del de pie según el arma, andar entre quieto y
  correr, salto penalizado), valores en los constructores de cada arma; la mira dinámica
  (`AShooterHUD::GetCrosshairGap`) ya lee el spread, se comprueba que se cierra al agacharse.
- Tests: tabla de spread por estado y arma; la mira mide menos agachado que de pie y que corriendo.

Estado: hecha.
- Causa confirmada: todas las armas tenían `CrouchingSpreadMod` = 0,8 y andar no tenía término; la mira se abría con el
  spread pero sus 4 px de hueco no cambiaban, así que agachado se cerraba menos de medio píxel.
- `GetCurrentSpread`: el término de movimiento sube con la velocidad hasta `WalkingSpread` en `WalkingSpeed` (los
  140 u/s de CS, 356 cm/s; la tecla de andar siempre queda por debajo) y de ahí a `MovingSpread` a la carrera del arma
  (`MaxWalkSpeed` × `GetSpeedModifier`), en `GetMovementSpread`; el aire suma `JumpingSpread`; agachado multiplica
  todo por `CrouchingSpreadMod`. Cada arma fija `WalkingSpread` y `CrouchingSpreadMod` en su constructor (AK 0,5,
  AWP 0,5, M4A1 0,55, MP5 0,6, pistolas 0,65), con la rama de su `PrimaryAttack` de CS en un comentario.
- Mira: `GetCrosshairGap(ViewHeight)` (se puede probar sin canvas); agachado en el suelo su hueco propio se multiplica
  también por `CrouchingSpreadMod` (el `ACCURACY_DUCK` de CS). AK en 448 líneas: 2,9 px agachado, 5,8 quieto, 10,9
  andando, 29,2 corriendo, 71,8 en el aire.
- Tests: `ShooterGame.Weapons.SpreadByState` (tabla por arma y estado y su orden), `ShooterGame.HUD.DynamicCrosshair`;
  `SpreadModel` pasa a comprobar el término de movimiento (104 tests de ShooterGame).
- BotMatch 10 7 cambia, idéntico dos veces: `Botmatch OK: 10 round(s), CT 4 - T 6, 59 kill(s), seed 7, sides switched
  after round 5` (antes 9 rondas, CT 3 - T 6, 55 bajas).

Desviaciones:
- En CS 1.6 solo las pistolas y el AWP tienen rama `ducking` en el spread (Glock 0,75, USP 0,8, Deagle 0,88 del de pie;
  AWP 0); en los rifles agacharse solo baja el retroceso (`KickBack`). Se sigue el plan (0,5–0,65 en todas) y el
  retroceso agachado queda sin tocar.
- CS no penaliza andar por debajo de 140 u/s con los rifles; aquí andar queda entre quieto y correr en todas las armas,
  como pide el plan.
- La mira no se limita a leer el spread: agachada también cierra su hueco fijo, porque con el spread solo el cambio
  quedaba en un píxel.
- El AWP sin mira suma `UnscopedSpread` después de todo (como el +0,08 de CS), así que agachado sin mira apenas mejora.

Añadido (P2b, tras la revisión, por fidelidad a CS):
- Retroceso por estado: el AK-47, la M4A1 y la MP5 escalan cada patada con las ramas de `KickBack` de CS
  (`MovingRecoilScale`, `JumpingRecoilScale`, `CrouchingRecoilScale`: los argumentos de CS sobre los de pie;
  `GetRecoilScale`). Agachado el AK patea 0,9 veces lo de pie, moviéndose 1,5 y en el aire 2,0; la M4A1 0,92 / 1,54 /
  1,85; la MP5 0,93 / 1,33 / 2,4 (y de lado en proporción). El AK y la M4A1 miran el movimiento antes que el aire, la
  MP5 al revés, como CS. Pistolas y AWP mantienen una sola patada.
- Rifles y MP5 andando tan precisos como quietos (`WalkingSpread` 0), como CS, que en ellos solo mira pasar de 140 u/s;
  el término de andar queda en pistolas y AWP. La mira del AK andando es la de quieto (5,8 px).
- Tests: `ShooterGame.Weapons.KickBackByState` nuevo; `SpreadByState` y `HUD.DynamicCrosshair` al día (el orden completo
  con la Glock). `Bots.RecoilKicksTheAim` admite 2° en vez de 1,5° con compensación total: el bot dispara andando de
  lado y cada patada es la del AK en movimiento (1,5×). 105 tests de ShooterGame.
- BotMatch 10 7, idéntico dos veces: `Botmatch OK: 10 round(s), CT 5 - T 5, 63 kill(s), seed 7, sides switched after
  round 5`.
- Desviación: la MP5 de CS no tiene término de carrera en el spread (solo el aire); aquí correr sigue abriéndolo
  (`MovingSpread` 1,5), como pide el plan.

**P3 · IA de bots: cuchillo, sin rumbo, recoger armas, escaleras, balance (M)**
- Cuchillo: `TaskEngage` (`ShooterAIController.cpp:581`) llama `StandStill()` y solo hace strafe. Con arma cuerpo a
  cuerpo el bot corre hacia el enemigo (MoveToLocation al enemigo, re-path) y ataca solo dentro del alcance
  (`ShooterWeapon_Knife.h`: 122/81 cm); en combate `EquipBestWeapon` para bots salta las granadas.
- Recoger: sin munición, el bot busca el arma o munición caída más cercana en `GetPickups()` del game mode y cambia la
  vacía por ella (soltar y recoger); `TickPickup` pasa a permitir el cambio para bots con el hueco vacío de munición.
- Sin enemigos: se sustituye `HoldAndLookAround` (cpp:1198) por comportamientos de CS: T sin bomba empujan con el
  portador o toman posiciones de apoyo del sitio; CT patrullan puntos de vigilancia por sitio (nodos nuevos
  `Ambush`/`Lookout` en los waypoints del mapa) y miran esquinas y accesos; siguen ruidos y radio; con poco tiempo o
  superioridad pasan a cazar; rotan con la radio.
- Escaleras: enlaces de navegación a través de los volúmenes `Ladder` (flag `Ladder` en `ANavigationWaypoint`,
  `NavigationSystem.cpp:166` deja de rechazarlos) y los bots trepan (se quita el `bCanClimbLadders=false`).
- Balance: volver a medir T vs CT en 24 semillas y ajustar hasta 45–55 %.
- Tests: cuchillo se acerca y mata; bot sin munición recoge un arma; bot sin enemigo visita puntos de vigilancia;
  bot sube una escalera; BotMatch estable.

### Juego y controles

**P4 · Agacharse en toggle y soltar/recoger armas (S) — hecha**
- Crouch en toggle por defecto (opción guardada en `UShooterPersistentUser`: toggle/mantener); `OnCrouchPressed`
  alterna (`ShooterCharacter.cpp:326`); se actualiza `Config.InputAndChannels`.
- Soltar ya existe (G, `DropWeapon`); se añade soltar la bomba (G con la bomba como slot 5, como CS) y recoger al
  pasar por encima cuando el hueco está libre (`TickPickup`) con aviso en el HUD; los muertos ya sueltan primaria y
  bomba (`ShooterCharacter.cpp:1190`).
- Tests: toggle, soltar y recoger arma y bomba.

Estado: el crouch alterna por defecto (`bToggleCrouch` en `UShooterPersistentUser`, comando `SetToggleCrouch 0|1`;
con la opción apagada se mantiene; los bots siguen llamando a `Crouch()`/`UnCrouch()`). La bomba sigue fuera del
inventario (`CarriedBomb`), pero se saca como el slot 5 de CS: la acción `Bomb` (5, cruceta abajo) llama a
`AShooterCharacter::DrawBomb`, que guarda el arma y oculta los brazos (el C4 no tiene modelo en primera persona); el
HUD muestra `C4` como arma; cualquier arma la guarda (`EquipWeapon`); G (`OnDropWeapon`) la suelta delante de los pies
(`DropBomb`, el mismo punto que un arma, `GetDropLocation`) y quien la soltó no la recoge hasta pasado
`AShooterBomb::PickupDelay` (1 s; la muerte no cambia, así que el BotMatch es idéntico). Recoger un arma pasa por
`AShooterCharacter::PickUpWeapon` (suena su equipamiento si no se saca al momento) y un arma o la bomba recogidas
muestran `Picked up <item>` en el HUD 2 s (`PickupNoticeDuration`). El arma conserva munición y silenciador. Tests:
`ShooterGame.Input.CrouchToggleAndHold`, `.DropAndPickUpWeapon`, `.DropAndPickUpBomb`, `Settings.RoundTrip` y
`Config.InputAndChannels` ampliados (102 tests de ShooterGame). BotMatch 10 7 sin cambios: `Botmatch OK: 9 round(s),
CT 3 - T 6, 55 kill(s), seed 7, sides switched after round 5`.

Desviaciones: el aviso usa el `WeaponName` del arma (`Picked up m4a1`), como el resto del HUD, no un nombre largo
(`AK-47`); los nombres para mostrar quedan para el HUD nuevo de P6. Disparar con el C4 en la mano no planta (se
planta con E, como antes).

### Interfaz

**P5 · Base de UI: fuentes, texturas en el canvas y widgets (L) — hecha**
- `UFont` (UE): una fuente OFL (DejaVu Sans Condensed / Noto Sans) rasterizada en el cook con stb_truetype a un atlas
  PSMT4 con métricas y kerning (tamaños 10/14/20/32 px); texto por SPRITE texturizado. Sustituye a `stb_easy_font` en
  el HUD y en UMG (D10); `FGSDebugDraw` conserva su fuente de depuración.
- Canvas con textura: `FCanvas::DrawTile` con `UTexture` y UV, `UImage` con brush de textura; ruta texturizada en
  `FGSSceneRenderer::DrawCanvas`; escena de conformidad de sprites texturizados con alfa (D7).
- Widgets UMG que faltan: `UHorizontalBox`, `UButton` con foco y navegación (pad, teclado y ratón),
  `UWidgetSwitcher` y una tabla reutilizable (`UShooterTable` o `UListView`-like en UMG: columnas con cabecera,
  ancho, alineación, orden, fila resaltada).
- Tests: métricas y kerning, render de texto contra referencia, navegación por foco, tabla ordenada.

Estado: hecha.
- Fuente: DejaVu Sans Condensed 2.37 (licencia Bitstream Vera, los cambios de DejaVu en dominio público), vendorizada
  en `Engine/SourceArt/EngineFonts/` con su `LICENSE.txt` y en `Engine/SourceArt/LICENSES.md` y `LIBRARIES.md`.
  `UTrueTypeFontFactory` (LeonEd, `-type=Font`, stb_truetype 1.26 en el módulo STB) la importa a `UFont` (el offline
  font de UE) en 10, 14, 20 y 32 px (`/Engine/EngineFonts/DejaVuSansCondensed*`, `ImportList.ini` del motor): ASCII y
  Latin-1, cobertura en 16 niveles en el alfa de la CLUT de páginas PF_P4 de hasta 256 x 256 (una página por tamaño:
  256 x 64, 128 x 128, 256 x 128 y 256 x 256), avances, apoyos y pares de kerning redondeados a píxel. Reimportar da
  los mismos bytes (G5 y `System.LeonEd.Factories.TrueTypeFont.Import`). `UEngine::GetTinyFont` ... `GetLargeFont`
  (config `TinyFontName` ...) las cargan; sin `GEngine` (tests, commandlets) se cargan al primer uso.
- Canvas: `DrawText(Font, ...)` / `MeasureText(Font, ...)` por las métricas, texto UTF-8, un SPRITE texturizado por
  glifo en coordenadas enteras, muestreado nearest; `FCanvasTextItem` (sombra, contorno, escala) y
  `DrawShadowedString`. `DrawTile` con `UTexture`, UV, color y alfa; `FCanvasTileItem` con rotación sobre un pivote
  (dos triángulos UV, para el minimapa de P7). Todo tile y línea mezcla ya por su alfa. `DrawCanvas` enlaza la textura
  por la caché (TEX0 al cambiar, UV con la V girada, MODULATE, clamp, nearest 1:1 y bilineal si no); una textura no
  residente espera al siguiente frame.
- D7: escena de conformidad `TexturedCanvas` (la 21: PSMT4 con rampa de alfa en la CLUT, MODULATE y mezcla; sprites UV
  en medios píxeles, nearest 1:1, girados en V y escalados bilineales; un quad rotado de triángulos UV), con su test
  `System.GSReference.Texture.TexturedCanvas`; el emulador la iguala (0 píxeles fuera de 2); GSConformance.elf ahora
  pone 4 x 6 celdas. Captura de PCSX2 en `Engine/Platforms/PS2/Documentation/Captures/GSConformance.png`.
- UMG: `UHorizontalBox`/`UHorizontalBoxSlot`, `UButton` (`FButtonStyle`, eventos de UE), `UWidgetSwitcher`,
  `UTableView` (columnas con cabecera, ancho fijo o de relleno y alineación, filas ordenables por columna con números
  como números, fila resaltada que sigue a su fila, anchos de texto cacheados), `UImage` con brush de textura
  (`FSlateBrush`), `UTextBlock` con fuente (`FSlateFontInfo`), sombra y tamaño cacheado. Foco y navegación:
  `AHUD::InputKey` antes que el juego; el widget con foco y sus padres, luego flechas, cruceta y Tab mueven el foco al
  más cercano en esa dirección entre los pintados (`FHittestGrid`, reglas explícitas); Accept (Enter, Espacio, Cruz)
  pulsa y el soltar hace clic; en Win64 el ratón libre hace hover y clic (`AHUD::InputMouseMove`,
  `IRendererModule::WindowToRenderTarget`). En SlateCore: `FGeometry`, `FReply`, `FKeyEvent`/`FPointerEvent`,
  `EUINavigation`, `FNavigationConfig`.
- D10: fuera `stb_easy_font`, `HudFontScale`, `HudLineHeight`, `FCanvas::DrawTextBlock`, la escala del texto del
  canvas y `MeasureTextOnly`, con su regla en `CheckBannedApis.ps1`; STB queda solo en Desktop (LeonEd).
- ShooterGame: HUD, menú de compra y overlay de depuración en la fuente de 14 px, mismo layout; capturas Win64 en
  el scratchpad (`p5_hud_a.png`; `p5_hud_stats.png` con `stat unit`, la radio y el feed; `p5_canvas_reference.png`,
  el frame del test de texto; `p5_gsconformance_pcsx2.png`).
- Tests (588 del motor, 105 de ShooterGame): `System.Engine.Font.MetricsAndKerning`,
  `System.Engine.Canvas.TextGlyphs` y `.TexturedTile`, `System.LeonEd.Factories.TrueTypeFont.Rasterize` e `.Import`,
  `System.GSReference.Texture.TexturedCanvas`, `System.Renderer.GS.Canvas.Text` (el frame de la referencia, su CRC
  tras verlo), `System.Renderer.GSEmulator.CanvasFrame`, `System.UMG.Focus.Navigation`, `System.UMG.Button.Activation`,
  `System.UMG.Panels.HorizontalBoxAndSwitcher`, `System.UMG.Image.Texture`, `System.UMG.TableView.SortAndLayout`;
  `GS.Canvas.Sprites`, `UMG.WidgetTree.LayoutAndPaint` y `ShooterGame.HUD.RoundInfo` actualizados (un sprite por
  glifo).
- BotMatch 10 7 idéntico dos veces y sin cambios respecto a P2b: `Botmatch OK: 10 round(s), CT 5 - T 5, 63 kill(s),
  seed 7, sides switched after round 5`.
- PCSX2 (`MeasurePS2`, fila «ps2-polish P5» en Budgets.md): 29,98 fps, p50/p95/p99 33,5 ms. El canvas (`Canvas
  Flush`) baja de 2,89 a 1,45 ms (`GS Canvas` 1,33 ms): un glifo es un SPRITE de cuatro escrituras del GS donde
  stb_easy_font dibujaba varias barras; el HUD (la pintura de los widgets) sube de 0,07 a 0,18 ms. La escena 9,15 ms
  (8,53 en P1, ahora sobre P2 y P2b), GMalloc pico 4 289 KB.

Desviaciones:
- Las fuentes se rasterizan en la importación (LeonEd, como el `UTrueTypeFontFactory` de UE) y no en el cook: el
  asset guarda ya las páginas PSMT4 y el cook las copia (no pasan por `FPalettedTextureBuilder`).
- Un `UFont` por tamaño (el offline font de UE es de un tamaño); `FSlateFontInfo::Size` elige el del motor más
  cercano cuando no hay `FontObject`.
- El foco vive en el `UUserWidget` más externo (no hay `FSlateApplication`): un user widget solo toma teclas con algo
  enfocado o con `bIsFocusable`, así el HUD no se come las del juego. La navegación analógica con el stick queda fuera
  (cruceta, flechas y Tab).
- La tabla es `UTableView` (UMG, columnas estilo `SHeaderRow` de UE) en vez de un `UListView` con widgets de entrada;
  no recorta el texto que no cabe.
- El marcador de ShooterGame sigue dibujado a mano con columnas alineadas por espacios, que la fuente proporcional ya no
  alinea: P6 lo pasa a `UTableView`.

**P6 · HUD y scoreboard nuevos (M)**
- HUD rediseñado al estilo CS 1.6 con la fuente nueva e iconos (vida, blindaje, dinero, munición, tiempo, kill feed
  con icono del arma).
- Scoreboard con la tabla de P5: por equipo nombre, bajas, muertes, BOT, muerto, bomba; se borra el dibujo a mano
  (`DrawScoreboard`, `ShooterHUD.cpp:908`).
- Tests de HUD actualizados; `MeasurePS2` para el coste del canvas (hoy 2,9 ms).

**P7 · Minimapa real (M)**
- Commandlet `-run=BuildOverview` en LeonCook: renderiza el mapa desde arriba en ortográfica con `FGSReferenceRasterizer`
  (sin depender de GPU), lo paletiza a P8 256² y guarda `T_<Mapa>_Overview` y su rectángulo en el mundo
  (`AWorldSettings` o un actor `AShooterOverview`); se regenera en la importación del mapa (determinista, G5).
- El radar (`DrawRadar`, `ShooterHUD.cpp:236`) dibuja la imagen rotada y recortada bajo los puntos (quad texturizado
  de P5).
- Tests: el overview es determinista; proyección mundo→imagen; el radar dibuja la textura.

**P8 · Cielo (M)**
- Generador procedural (script Python versionado, D6): cielo de desierto HDR (degradado, sol, nubes) a cubemap, con
  tone-mapping a 6 caras P8 de 128² en el cook.
- `ASkyBox`/ajuste en `AWorldSettings` con su textura; el renderer lo dibuja tras el clear y antes del mundo, sin Z,
  centrado en el ojo (caras subdivididas o domo para no pasar por el clipper del EE); el color de la niebla sigue al
  horizonte y se activan niebla y LODs en de_leon si no cuestan fps.
- Tests: escena de conformidad del cielo (D7), el cielo no escribe Z, determinismo del generador; `MeasurePS2`.

**P9 · Menú principal, selección de bando y pausa (L)**
- Mapa de menú (`MainMenu` como `GameDefaultMap`) con `UShooterMainMenuWidget`: mapa (lista de mapas del proyecto),
  dificultad (Fácil, Normal, Difícil, Experto → presets por bot de `ReactionTime`, `AimError`, `AimTurnRate`,
  memoria; hoy `Difficulty` es un solo valor), rondas para ganar (por defecto mejor de 5 = gana quien llega a 3;
  `MaxRounds = 2 × N − 1`), número total de bots (1–9, 10 jugadores como máximo por el presupuesto de PS2). Las
  opciones se guardan en `UShooterPersistentUser` (tarjeta de memoria).
- `UGameplayStatics::OpenLevel` con opciones de URL (`?bots=&difficulty=&winrounds=`), que `InitGame` lee.
- Al entrar: pantalla de bando (CT, T, Auto) y después `RebalanceBots` reparte los bots para dejar los equipos lo más
  parejos posible contando al jugador.
- Menú de pausa (Esc / Start): continuar, cambiar de bando, opciones (sensibilidad, invertir Y, volumen, agacharse
  toggle), volver al menú principal. Pausa real del mundo (`SetGamePaused`: ticks y timers parados). El menú de compra
  pasa de Start a otro botón del pad.
- Tests: opciones → URL → game mode; reparto de bots; pausa detiene timers; volver al menú; guardar opciones.

### Cierre

**P10 · Documentación y release 0.25.0 (S)**
- `Docs/PENDING.md` actualizado (lo hecho fuera; lo que quede), README de ShooterGame, TESTING, ARCHITECTURE,
  Budgets (fila 0.25.0), CHANGELOG 0.25.0, versión del motor 0.25.0 y contenido re-guardado; `RunGates -PS2`,
  `MeasurePS2`, la ISO arranca; squash a `main`.

## Fuera de este plan (se queda en PENDING)

Recorte en la VU1, `GSH_Capture`, PAL de 512 líneas y CSM2, fileXio/música en streaming, sectores de licencia,
arenas de UObjects y barreras del GC, hitboxes por hueso y fórmulas exactas de retroceso de CS, voces de radio,
variantes de sonido, posturas de locomoción, fusión de piezas y colisión compuesta.

## Archivos clave

| Área | Archivos |
|---|---|
| Bug de cuerpos | `Engine/Private/GameFramework/Character.cpp`, `Renderer/Private/Scene.cpp`, `SkeletalMeshComponent.cpp` |
| Precisión | `ShooterWeapon_Instant.cpp/.h`, `ShooterHUD.cpp` (mira) |
| Bots | `ShooterAIController.cpp`, `AIModule/NavigationSystem.cpp`, `NavigationWaypoint.h`, `ShooterWeapon.cpp` (pickup), `make_de_leon.py` (puntos) |
| UI | `Engine/Runtime/UMG`, `CanvasTypes.h/Canvas.cpp`, `GSSceneRenderer.cpp` (`DrawCanvas`), cook de fuentes en LeonEd |
| HUD/menús | `ShooterHUD.cpp`, `ShooterPlayerController.cpp`, `ShooterGameMode.cpp`, `ShooterPersistentUser.h`, `DefaultInput.ini`, `DefaultGame.ini` |
| Cielo/overview | `WorldSettings.h`, `GSSceneRenderer.cpp`, `Developer/GSReference`, `PalettedTexture` |

## Verificación

- Cada fase: `RunGates.bat -PS2` en verde y su test propio; BotMatch idéntico en dos pasadas (la línea cambia en P2/P3,
  se anota).
- `MeasurePS2` tras P1, P3, P6, P8 y P10: 30 fps, p95 ≤ 33,5 ms.
- Capturas Win64 del HUD, menú, radar y cielo en el scratchpad para aprobación visual; comparar en la misma GPU.
- Prueba del usuario al final: jugar desde el menú principal en PCSX2 con el mando.
