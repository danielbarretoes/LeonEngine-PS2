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

**P3 · IA de bots: cuchillo, sin rumbo, recoger armas, escaleras, balance (M) — hecha**
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

Estado: con el cuchillo (`EngageWithKnife`) el bot corre la ruta hasta el enemigo (`MoveToGoal`, nueva ruta cuando se
mueve 1,5 m), dentro del alcance del tajo entra recto por la espalda si el enemigo le da la espalda y apuñala dentro
del alcance de la puñalada (`IsBackstab`), si no lo rodea de lado acercándose y da tajos; nunca corta fuera de
alcance. En combate `EquipBestWeapon(false)` salta las granadas (también ciego, al comprar y tras lanzar). Recoger: la
rama nueva `PickUp` (`UpdatePickupTarget`, dos veces por segundo) lleva a un bot sin primaria cargada a la primaria
con munición más cercana del suelo en 15 m (`PickupSearchDistance`) y, sin munición en nada, a cualquier arma con
munición en 30 m; `AShooterWeapon::CanBePickedUpBy` deja a un bot coger un arma para un hueco cuya arma no tiene
munición (`PickUpWeapon`/`AddWeapon` suelta la gastada); el jugador sigue la regla de CS (hueco libre). Sin enemigo:
`HoldAndLookAround` se borra; los bots vigilan los sitios desde sus puntos de vigilancia
(`AShooterGameMode::GetBombSiteLookouts`: waypoints con flag `Lookout`, tres por sitio en de_leon; sin ellos, los tres
waypoints más cercanos al sitio o su centro), cada uno con direcciones por equipo: primero el acceso principal (el
primer enlace del camino del grafo hacia el spawn contrario), luego las otras; en cada punto giran entre ellas (el
acceso principal el doble de tiempo) y a los 3–7 s pasan a otro punto del sitio; al acercarse miran ya el acceso. Los
T sin bomba escoltan al portador mirando a un lado y, con él a 15 m del sitio, toman los puntos del sitio (apoyo); con
la bomba plantada la guardan desde los puntos de su sitio. Los CT rotan tras `RotateTime` sin contacto o al oír por
radio un enemigo en el otro sitio (`RotateOnReportChance` 1); los T cazan con 30 s o menos de ronda; la caza recorre waypoints del grafo
tras el spawn enemigo. Andando sin enemigo el bot mira por donde va. Escaleras: `AutoLinkWaypoints` enlaza dos
waypoints con flag `Ladder` a menos de 2 m en horizontal (`MaxLadderLinkDistance`) sea cual sea la subida, el
seguidor de rutas no salta a la cima de una escalera y cuenta la subida como avance, `AAIController` expone
`GetCurrentTargetLocation` (UE), el bot trepa mirando a la escalera (`GetLadderNormal`), baja mirando abajo y se suelta
abajo con un salto; se borra `bCanClimbLadders`. de_leon (`make_de_leon.py`, arte determinista): seis puntos de
vigilancia fuera de la línea de las calles largas, pies y cimas de las dos escaleras y un punto en cada tejado, y los
inicios CT fuera de la línea de las puertas de mid (zona de compra CT 7,5 × 16 m). Balance en las semillas 1–24: los T
ganaban el 60,1 % de las rondas (125 de 208, en P2b; 61,3 % en P0) y ahora el 49,3 % (99 de 201; 1–48: 50,2 %,
49–88: 59,5 %). Motor: `USceneComponent::DetachAllChildren` ya no da vueltas para siempre con un hijo que apunta a otro
padre (la recolección de la salida colgaba un BotMatch). Tests nuevos:
`ShooterGame.Bots.KnifeRushesAndKills`, `.PicksUpAWeaponOutOfAmmo`, `.VisitsLookouts` (con la misma semilla, el mismo
recorrido), `.ClimbsALadder` y `System.AIModule.Gameplay.NavigationAutoLinkLadders`; ampliados
`ShooterGame.Map.DeLeonHoldsTheGame`, `.Movement.Ladder` y `System.Engine.Components.AttachmentRulesAndSockets`, y
`Radio.BotsReportEvents` pone un muro delante del compañero (los bots ya miran por donde van) (109 tests de
ShooterGame).
BotMatch 10 7: `Botmatch OK: 8 round(s), CT 2 - T 6, 55 kill(s), seed 7, sides switched after round 5`.
MeasurePS2 (fila «ps2-polish P3» de Budgets.md): 29,26 fps, p50/p95 33,5 ms, mundo 4,71 ms (P5: 4,57 ms); el p99 de
83,5 ms son 12 cuadros de los segundos 12 a 15, con un compañero a 80 cm delante del bot observado: sus lotes con piel
pasan por el emisor del EE (el recorte de PENDING), no por la IA.

Desviaciones: los puntos de vigilancia son waypoints con flag `Lookout` (sin nodos `Ambush` aparte) y sus direcciones
se sacan del grafo, no se escriben a mano. El balance no se logró solo con la IA: con la vista de 60 m los T ganaban
el 73–78 % porque (1) los inicios CT estaban en la línea de las puertas de mid y el arco (el 45 % de las muertes eran
de spawn a spawn a 53 m), y (2) desde la plaza T se ve por las puertas de las calles largas hasta dentro de los dos
sitios (35–45 m), duelos que gana el AK-47. Se movieron los inicios CT (mapa) y la vista de los bots pasa a ser
configurable (`SightRadius`) con 35 m; con 40 m los T ganan el 70 %. La radio hace rotar siempre a los CT. Los bots
solo oyen disparos (no pasos); ninguna bomba explota en las 24 semillas (los CT recuperan o los T mueren), y las
semillas 49–88 siguen del lado T (59,5 %). Un BotMatch (semilla 35, antes de P2b) colgaba al salir: un hijo con otro
padre en la lista de `AttachChildren` (la recolección borra el puntero del hijo) hacía girar `DetachAllChildren`; se
arregla en el motor. Los tejados
con escalera no son puntos de vigilancia (desde ellos no se ve el acceso); los bots suben por la caza, que elige
waypoints al azar.

Estado (P3b, la bomba plantada): el que planta pide "Cover me!"; los T sostienen desde los puntos de vigilancia a menos
de `PostPlantHoldRadius` (10 m) de la bomba (si no hay, desde la bomba mirando al spawn CT), con las direcciones de los
accesos CT; el primero avisa "Hold this position." y no persiguen nada que se oiga o se informe más lejos de la bomba.
El desactivado se oye (`AShooterBomb::DefuseNoiseLoudness`, el c4_disarm de CS) y los T cercanos van a por el que
desactiva. Los CT recuperan juntos: se reúnen en un punto a `RetakeStagingDistance` (15 m) de la bomba hacia su spawn
hasta que llega un compañero o pasa `RetakeWaitTime` (8 s), avisan "Go go go!", y desactivan cuando no han visto a
nadie en `SiteClearTime` (2,5 s) o falta tiempo; se rinden (`RetakeGiveUpAdvantage` 2 en contra, o sin tiempo para
llegar y desactivar), avisan "Team, fall back!" y se salvan en su spawn. Semillas 1–24: los T ganan el 55,6 % (119 de
214; P3: 49,3 %), la bomba explota en 11 rondas (5,1 %, el 9 % de las 121 plantadas; antes ninguna) y se desactiva en
45 (21 %); 25–48: 50,0 %, 9 explosiones. Tests: `ShooterGame.Bots.TerroristsHoldThePlantedBomb`,
`.TerroristsEngageTheDefuser`, `.RetakeGathers` (112 tests de ShooterGame). BotMatch 10 7:
`Botmatch OK: 8 round(s), CT 2 - T 6, 57 kill(s), seed 7, sides switched after round 5`.
Desviaciones (P3b): la rendición no la pedía el encargo, pero sin ella ninguna bomba explotaba (los CT recuperaban
siempre o morían todos, y eso acaba la ronda); con ella los T suben a 55,6 % en 1–24 (con 2 en contra; con 3 casi no
explota ninguna). `PostPlantHoldRadius` de 10 m (a 15 m, 59 %).

### P8b · Recorte de lo cercano en la VU1 (M) — hecha

- Los lotes que cruzan el near o la guard band (D8) iban al recortador C++ del EE: con un peón a 80 cm, sus lotes con
  piel; el view model, 13 lotes por frame y 2,4 ms. Objetivo: ningún frame de más de 33,5 ms por lo cercano, 30 fps.
- Opciones a medir: (1) contar como dentro los lotes que solo cruzan la guard band; (2) recortar el near en la VU1
  (CLIP por vértice, Sutherland-Hodgman, triángulos sueltos); (3) lotes con piel más pequeños en el cook.

Estado: hecha.
- Causa real del p99 de P3: no era lo cercano. Los `Frame spike:` de los 12 frames de 83 ms (12 a 15 s) dan `GS Skinned`
  42,9 ms de `GS Emitted Batches` (145 lotes con piel por frame en el emisor del EE) y `GS Opaque` 8,4 ms (25): en un
  tiroteo, los fogonazos junto a la lámpara del túnel daban a los draws con luz de alrededor tres o cuatro luces
  puntuales, la VU1 ilumina dos (N29) y esos draws caían al emisor. Un draw toma ahora las dos luces puntuales que más
  iluminan sus bounds (color × (1 − d / r)² en el punto más cercano; como el límite de luces por primitiva de UE), en la
  VU1 y en la referencia por igual. Test `System.Renderer.GS.Scene.PointLightsPerDraw`.
- Lo cercano, opción (2) ampliada: la VU1 recorta contra los seis planos del recortador C++ (near, far y los cuatro de
  la guard band), no solo el near. La opción (1) no vale: la esfera contra el plano ya es exacta, y un vértice más allá
  de la guard band se sale de los 4096 píxeles del GS (la guard band es justo el margen del scissor); la (3) solo
  reduciría cuántos lotes cruzan (la esfera de la pose es holgada: el peón a 80 cm cruza con 3,5 lotes), no el coste de
  los que cruzan, y el view model cruza siempre.
- `FGSVertexBatch::bClip`: el renderer graba también el lote que cruza; `ClipTriangles.vsi` (macro `CLIP_PROGRAM` que
  `VU1Programs.vsm` y `Skinned.vsm` expanden con su memoria; LeonBuildTool pasa `dvp-as -I` la carpeta). La parte de
  vértices del programa es la de siempre hasta la posición en clip, el color y las coordenadas; con la w de la
  cabecera los guarda con sus outcodes (los signos de tres vectores de distancias, FMAND) en las propias filas de
  los streams del vértice. Luego, cada triángulo de las tiras: fuera de un lado de la vista (los outcodes de la
  referencia, AND) se descarta; dentro de la guard band, near y far va tal cual; si no, Sutherland-Hodgman contra cada
  plano que cruza (hasta 9 vértices), proyección y abanico; siempre con la cara hacia la cámara. Salen como TRIANGLE
  en chunks de paquetes GIF (82 quadwords, 9 triángulos; 53 y 5 con piel) que se alternan con XGKICK.
- Referencia C++: `FGSPrimitiveEmitter::AddClippedVertexBatch` (el bucle del renderer pasa ahí; `AppendExpanded` la
  usa) y `FGSPrimitiveEmitter::GuardExtent`. Constantes nuevas en la memoria compartida de la VU1 (3-14) y un quadword
  de estado (15).
- VU1Conformance: 13 draws recortados (estáticos y con piel, con y sin luz, texturizados, espejados, con niebla y con
  dos luces puntuales, a 40–80 cm de un near de 11 cm), 78 lotes más; sin XGKICK cada chunk lleno para el programa (bit
  E, su dirección en el quadword 15) hasta el MSCNT de VIF1 y se lee. En PCSX2: `VU1Conformance: PASSED (162 batch(es),
  0 failed)`, 851 triángulos recortados comparados en 160 chunks, 477 cortados por el recortador; los de siempre XY 1,
  Z 5, RGBA 1, STQ 4 ulp, F 0; los recortados Z 24 y STQ 0,2 de 2^-16. La imagen alterna los lotes recortados de la VU1
  (con XGKICK) y del emisor: iguales salvo un píxel de 60 800 muestreados.
- Tests Win64: `VertexBatches` graba el suelo como lote recortado (ninguno en el EE); `SkinnedVertexBatches` a 45 cm
  graba lotes recortados; los dos se expanden al mismo frame, 0 píxeles distintos.
- Medida reproducible: `MeasurePS2 -CloseUp` (bots parados, `ViewFrom -2570 0 150 0 180`: el T del inicio central a
  80 cm, 60 s). Antes 29,99 fps, p99 33,5 ms, peor 50,05 (el arranque), escena 3,69 ms (15,5 lotes recortados en el EE,
  1,8 ms); después 29,99 fps, p99 33,5, peor 50,05, escena 1,82 ms, GIF del EE 8,3 → 0,5 KB.
- `MeasurePS2` (la misma partida de P3): 29,26 → 29,97 fps, p99 83,5 → 33,5 ms, peor 88,55 → 50,05 ms, escena 9,32 →
  5,70 ms, GIF del EE 23,4 → 8,6 KB, ningún lote en el EE (filas «ps2-polish P8b» de Budgets.md).

Desviaciones:
- El p99 de P3 no venía del recorte (lo atribuimos mal en P3): se arregla con el límite de dos luces por draw, que no
  estaba en el plan de P8b; lo cercano costaba 1,8 ms y nunca pasaba de 33,5 ms por sí solo.
- Se recortan en la VU1 los seis planos (no solo el near) y la decisión D8 cambia: lo que cruza también va a la VU1.
- Tolerancias medidas para los vértices que crea el recorte: Z ±32 y STQ 2^-16 del valor (mínimo 1), no las de D2;
  lo visto, 24 y 0,2.
- Sin escena nueva de G8/GSConformance: los triángulos TRIANGLE con XYZF2 ya estaban en uso (el recortador del EE).
- El close-up no reproduce los 83 ms (no eran del recorte); mide el coste de lo cercano en el EE.

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

**P8 · Cielo (M) — hecha**
- Generador procedural (script Python versionado, D6): cielo de desierto HDR (degradado, sol, nubes) a cubemap, con
  tone-mapping a 6 caras P8 de 128² en el cook.
- `ASkyBox`/ajuste en `AWorldSettings` con su textura; el renderer lo dibuja tras el clear y antes del mundo, sin Z,
  centrado en el ojo (caras subdivididas o domo para no pasar por el clipper del EE); el color de la niebla sigue al
  horizonte y se activan niebla y LODs en de_leon si no cuestan fps.
- Tests: escena de conformidad del cielo (D7), el cielo no escribe Z, determinismo del generador; `MeasurePS2`.

Estado: hecha.
- Generador: `SourceArt/Sky/make_sky.py` (biblioteca estándar de Python, sin Blender, unos 8 s) escribe
  `Sky_Desert.hdr`, un panorama long-lat de 1 024 x 512 en Radiance RGBE (RLE, sin fecha en la cabecera): cenit azul
  que se aclara a una bruma cálida en el horizonte (más clara hacia el sol), arena lejana bajo él, el sol (disco de
  1,5° de radio, 60 veces el horizonte, con su halo) donde está el sol horneado de de_leon (lee `SUN_DIRECTION` de
  `make_de_leon.py`, que pasa a ser una constante) y nubes de ruido de valor con semilla. `check_art_determinism.py`
  lo ejecuta dos veces con Python y compara el `.hdr` con el versionado (idéntico, 384 731 bytes); de_leon también.
- Asset: `UTextureCube` (UE) con sus seis caras como subobjetos `UTexture2D` (`PosX` ... `NegZ`, orden de `ECubeFace`)
  y `HorizonColor`; `UTextureCubeFactory` (LeonEd, `-type=TextureCube`, `.hdr` por stb_image) muestrea el panorama por
  la dirección de cada texel (4 muestras bilineales), aplica `ExposureBias` (0) y la curva ACES de Narkowicz, codifica
  sRGB y guarda caras de `CubeFaceSize` (128). `UTexture2D` gana `AddressX` / `AddressY` (`ETextureAddress`, UE:
  TextureAddress): las caras van en `Clamp` y `BindMaterial` escribe el CLAMP del GS según la textura.
- Colocación: `AWorldSettings::SkySettings` (`FWorldSkySettings::SkyCubemap`) y
  `FWorldFogSettings::bInscatteringColorFromSky` (por defecto sí: la niebla toma `HorizonColor`). El importador de
  mapas lee un nodo vacío `WorldSettings` cuyas extras fijan propiedades de `AWorldSettings` por ruta
  (`SkySettings.SkyCubemap`, `FogSettings.bEnableFog`...) con `ImportText`, cargando los assets nombrados; una clave
  desconocida falla la importación. de_leon lo usa (`WORLD_SETTINGS` en `make_de_leon.py`).
- Render: `FGSSceneRenderer::DrawSky` tras el clear y antes del mundo: una caja (`FSkyBoxGeometry`, LPS2 v2 generado
  en runtime, 12 x 12 quads por cara en lotes de 2 x 2, 216 lotes de a lo sumo 13,3°) escalada por la media
  geométrica de near y far y trasladada al ojo, cada cara por `DrawMeshSection` sin luz ni niebla, ZTST ALWAYS y Z
  enmascarada. En PS2 va por el programa StaticUnlit de VU1; ningún lote pasa por el clipper del EE (probado en 54
  vistas). Unos 36 lotes por frame; 8 KB de GIF cuando los emite el EE.
- Niebla y LODs en de_leon: niebla activada de 30 m al far de 100 m con el color del horizonte (tiñe solo el fondo
  de las vistas largas y oculta el corte del far contra el cielo). LODs no: todas sus mallas son Static horneadas y
  una malla horneada siempre dibuja el LOD 0, así que no cambiarían nada.
- Tests (595 del motor): `System.Renderer.GS.Sky.BoxGeometry`, `.Orientation` (seis vistas de yaw y pitch conocidos
  contra la referencia: la cara y sus cuadrantes donde deben; un cubo a 30 m, detrás de la caja, se ve encima: el
  cielo no escribe Z y va primero; los lotes grabados para VU1 dan los mismos píxeles; ningún lote recortado en 48
  vistas más), `.FogColor`, `System.LeonEd.Factories.TextureCube.FaceBasis` e `.Import` (tone-mapping, exposición,
  horizonte, reimportación idéntica), `System.LeonEd.MapFactory.WorldSettingsFromNode`.
- Capturas Win64 en el scratchpad (`p8_sky_sun.png`, `p8_sky_up.png`, `p8_sky_long.png`, `p8_sky_east.png`,
  `p8_sky_fog.png`) y de PCSX2 en partida (`p8_pcsx2_sky.png`).
- BotMatch 10 7 idéntico dos veces y sin cambios respecto a P3b: `Botmatch OK: 8 round(s), CT 2 - T 6, 57 kill(s),
  seed 7, sides switched after round 5`.
- PCSX2 (`MeasurePS2`, fila «ps2-polish P8» en Budgets.md): 29,20 fps, p50/p95/p99 33,5 / 33,5 / 83,5 ms, como P3
  (29,26, con los mismos tirones); la escena 9,79 ms (9,32 en P3), el cielo 0,28 ms; las caras ocupan 127 KB de VRAM.

Desviaciones:
- Sin escena de conformidad nueva (D7): el cielo no usa ninguna función nueva del GS (textura PSMT8 con STQ, bilineal,
  CLAMP, ZTST ALWAYS y ZMSK ya están en ClampModes, ClutAndFormats y el clear); en su lugar el test de orientación
  compara la referencia con los lotes de VU1 expandidos y la captura de PCSX2 muestra el cielo.
- Caras de 128² y no 256²: con 256² la carga de de_leon llegaba a 1 038 KB del presupuesto `LoadMapMisc` de 1 024 KB
  (error fatal en PS2).
- `AWorldSettings` en vez de un actor `ASkyBox` (como la niebla, que ya vivía ahí), y el nodo `WorldSettings` genérico
  por rutas de propiedad en vez de extras específicas del cielo.
- UE guarda las seis caras como slices de un único platform data; aquí son seis `UTexture2D` para que el cook y la
  caché de texturas del GS las traten como cualquier textura.

**P9 · Menú principal, selección de bando y pausa (L)**
**P9 · Menú principal, selección de bando y pausa (L) — hecha**
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

Estado: hecha.
- Motor: la pausa de UE. `AGameModeBase::SetPause` / `ClearPause` / `AllowPausing` / `IsPaused` con `FCanUnpause`,
  `AWorldSettings::GetPauserPlayerState`, `UWorld::IsPaused`, `APlayerController::SetPause`, `IsPaused`, `CanUnpause` y
  el comando `Pause`, `UGameplayStatics::SetGamePaused` / `IsGamePaused`. Un mundo en pausa hace un paso
  `LEVELTICK_PauseTick`: su tiempo, los timers, la física y los efectos se paran y solo corren las tick functions con
  `bTickEvenWhenPaused` (los player controllers, que entonces solo procesan su input, y los HUDs con sus widgets);
  `GetRealTimeSeconds` sigue y el audio también. `UGameplayStatics::OpenLevel` (sobre `SetClientTravel`) y
  `GetPlayerController`; `APlayerController::SetInputMode` con `FInputModeUIOnly` / `GameAndUI` / `GameOnly` (la captura
  del ratón del viewport; el cursor libre de la UI no mira). LeonEd: `MapsWithoutRequiredTags` exime un mapa del
  proyecto de las etiquetas obligatorias.
- Menús (UMG, `UShooterMenuWidget`: panel centrado en el oliva y ámbar de CS 1.6, título de 32 px, líneas de 20 px,
  `UShooterMenuButton` con valor que izquierda/derecha cambian; modal: se queda toda tecla que no sea de navegación, y
  el controller apila un input component que bloquea el resto; dibujado con una depth sort key por delante del HUD).
  Principal (`UShooterMainMenuWidget`, mapa `MainMenu`: `GameDefaultMap`, un rincón de pueblo desértico hecho por
  `SourceArt/Maps/make_main_menu.py`, solo biblioteca estándar; `AShooterGame_Menu` por `GameModeMapPrefixes`,
  `AShooterPlayerController_Menu` sin pawn, la cámara balanceándose despacio): mapa (`+MapNames=`, de_leon), dificultad,
  rondas para ganar (3, 5, 8, 16; por defecto 3), bots (1–9), opciones, empezar y salir (solo Win64). Bando
  (`UShooterTeamMenuWidget`): CT, T, Auto, Espectador. Pausa (`UShooterPauseMenuWidget`, Esc / Start): continuar,
  cambiar de bando, opciones, volver al menú principal, con la línea del partido (mapa, marcador, ronda, rondas para
  ganar). Opciones (`FShooterOptionsPage`): sensibilidad, invertir Y, volumen, agacharse (toggle / mantener); se aplican
  al momento y se guardan una vez al salir de la página.
- Partido: `?bots=N?difficulty=Hard?winrounds=3` (`FShooterMatchSettings::GetURLOptions`) que `InitGame` lee
  (`MaxRounds = 2 N − 1`; el descanso sigue en `MaxRounds / 2`, con valores impares la segunda mitad es la larga: 2 + 3
  en un mejor de 5). Sin `?team=` el jugador espera en el warmup con el menú de bando; `?team=CT|T|Auto|Spectate` elige
  directamente. `RebalanceBots` reparte `NumBots` con los equipos lo más parejos posible contando al jugador (9 bots
  con el jugador en CT: 4 CT y 5 T; el impar va al rival del jugador, a T si no hay jugador) y mueve bots al otro lado
  al empezar la ronda siguiente a un cambio. Cambio de bando con la regla de CS: durante una ronda en juego el jugador
  vivo muere (cuenta la muerte) y juega en el nuevo bando desde la ronda siguiente; en warmup o freeze reaparece al
  momento. Las elecciones del menú se guardan en `UShooterPersistentUser` (tarjeta de memoria).
- Dificultad (D10): presets por dificultad en `DefaultGame.ini` (`+DifficultyPresets`, `FShooterBotSkill`: reacción,
  error de puntería inicial, asentamiento y mínimo, giro, control del retroceso, memoria), al estilo de los bot profiles
  de CS; `AShooterAIController::ApplyDifficulty` al añadir el bot. Normal es la habilidad de antes: el BotMatch no
  cambia. El alcance de la vista (`SightRadius`, 35 m, P3) es igual en todas.
- Pad: Start abre la pausa; el menú de compra pasa a la cruceta abajo donde se puede comprar (fuera de zona o de tiempo
  la cruceta abajo sigue sacando la C4, acción `BuyMenuOrBomb`). Esc abre la pausa si no está abierto el menú de compra
  o el de radio.
- D10: fuera `bFillTeamsWithBots` y el `Difficulty` único (regla en `CheckBannedApis.ps1`).
- `-botmatch` sin mapa: el game mode del menú viaja a de_leon al instante (BotMatch.bat y MeasurePS2 no cambian).
  SmokeTest arranca con `"/Game/Maps/de_leon?team=CT"` (los nueve bots entran repartidos) y sigue dando 10 peones, CT 5,
  T 5.
- Tests (591 del motor, 118 de ShooterGame): `System.Engine.World.Pause`, `System.Engine.Travel.OpenLevel`,
  `System.LeonEd.MapFactory.EngineMapsSkipRequiredTags` ampliado; `ShooterGame.Menu.MatchOptions`, `.DifficultyPresets`,
  `.BotSplit`, `.TeamChoiceAndChange`, `.PauseMenu`, `.MainMenuToMatchAndBack`; `ShooterGame.Settings.RoundTrip`,
  `Config.InputAndChannels`, `Input.BuyMenuTakesItsKeys`, `Map.DeLeonHoldsTheGame` y `.TenPawnsOnDeLeon` actualizados.
- BotMatch 10 7 idéntico dos veces y sin cambios respecto a P3b: `Botmatch OK: 8 round(s), CT 2 - T 6, 57 kill(s),
  seed 7, sides switched after round 5`. RunGates -PS2 en verde.
- PCSX2: el ELF arranca en el menú principal a 30 fps (0,38 s hasta el primer frame); `/Game/Maps/de_leon?bots=9?
  difficulty=Hard?winrounds=3 "-ExecCmds=JoinTeam CT"` elige CT, entran 4 CT y 5 T, el partido es a 5 rondas con el
  descanso tras la 2, a 30 fps; sin `JoinTeam` el warmup espera; con el menú de pausa abierto y el mundo parado, 30 fps
  (el canvas 2,4 ms). El manejo con el mando en PCSX2 no se puede automatizar: queda para la prueba del usuario.
  MeasurePS2 (sobre P3, antes del rebase a P3b, que solo toca bots; tras él PCSX2 estaba ocupado por otro agente;
  fila «ps2-polish P9» en Budgets.md): 28,80 fps, p50/p95 33,5 ms, p99 83,5 ms (los mismos frames de 83 ms que P3); el
  primer frame, 1079 ms, incluye ahora el viaje del menú a de_leon (`-botmatch` pasa por el menú), fuera de los
  percentiles.
- Capturas Win64 en el scratchpad (`polish/p9_main_menu.png`, `p9_team_menu.png`, `p9_pause_menu.png`).

Desviaciones:
- La lista de mapas es de config (`+MapNames=`), no del asset registry (Leon no tiene); `-botmatch` salta el menú desde
  su game mode (el mapa del menú se carga y se viaja en el primer frame), no desde la línea de comandos del motor.
- `ChooseTeam` en Auto no desempata por marcador (el menor equipo, CT en empate, como antes).
- El impar del reparto va al rival del jugador (con 4 bots y el jugador en CT: 2 contra 3).
- Pausar un mundo sin world settings (los mundos de test) falla: los tests crean los suyos.
- El menú no navega con el stick (la navegación de UMG de P5 es cruceta, flechas y Tab).

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
