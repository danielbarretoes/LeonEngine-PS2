# Plan: LeonEngine a pleno rendimiento en PS2 (ps2-shipping)

## Objetivo

Que el motor use la PS2 como una PS2 y no como un PC lento, sin nada legacy:

- VU1 y cadenas DMA;
- GS completo (mips, niebla, alpha test, sprites);
- audio en la SPU2;
- IO asíncrona, arenas de memoria y broadphase.

El objetivo de ShooterGame es 30 fps estables en PCSX2 (NTSC; 25 en PAL), con arte real CC0 low-poly texturizado al
estilo de CS 1.6, personajes y brazos animados hechos en Blender, más paridad con CS y una ISO arrancable.

Viene de la auditoría del 2026-09-27. Sus hallazgos quedan como contexto; el detalle está en cada fase.

## Contexto (hechos del código al empezar)

| Área | Hoy |
|---|---|
| Geometría | Transform, iluminación, recorte y skinning en C++ en el EE (`FGSSceneRenderer`). Listas de triángulos sueltos en PACKED A+D (~48 B por vértice). VU0/VU1, VIF y SPR ociosos |
| Envío | `FlushFrame` copia el paquete 3-4 veces y hace un envío DMA síncrono. Espera a FINISH y luego al vsync en bucle activo: el EE y el GS nunca se solapan |
| GS | Solo mip 0, sin niebla, sin alpha test y con TCC=RGB en las paletas. La caché de texturas se vacía entera al llenarse. El modelo de memoria del emulador es lineal, así que las CLUT se pisan en PC |
| Audio | Mezcla software de 24 voces a 48 kHz en el hilo de juego; sin ADPCM ni voces de la SPU2 |
| Runtime | Un solo heap newlib; sin broadphase (cada consulta recorre todos los cuerpos); GC completo cada 61 s; paso variable; animación en matrices de espacio de modelo |
| Contenido | Solo las texturas se cocinan al formato de PS2 (P4/P8). Mallas en float de 48 B; skeletal solo por FBX; ningún personaje animado; arte de cajas |
| Build | La PS2 compila a -O3 (CMake añade `CMAKE_CXX_FLAGS_RELEASE` detrás del -O2 del toolchain). Sin ISO. La CI se quitó a propósito (f06113c): las gates corren en local |
| Medidas | Nunca se ha medido el fps de ShooterGame en PCSX2 ([Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md)) |

## Decisiones

| # | Decisión | Por qué | Alternativa descartada |
|---|---|---|---|
| D1 | **Un solo formato de render.** LPS2 v2 se cocina igual para Win64 y PS2; en Win64 y en los tests lo lee el emisor C++ (la referencia), y en PS2 la VU1 | Lo que se valida en PC es lo que dibuja la consola | Un formato por plataforma |
| D2 | **La VU1 se compara con tolerancia.** Tras FTOI4: XYZ ±1 (1/16 px), ST/Q ±1 ulp, RGBA ±1; los frames completos con la tolerancia de G8 | La VU no es IEEE (sin denormales, inf ni NaN; trunca) | Exigir igualdad bit a bit |
| D3 | **Captura headless del GS.** `leonrun` (Play!) gana `GSH_Capture`, que vuelca las escrituras GIF a `.lgsdump`, y GSReference las reproduce | Prueba EE→VIF→VU1→GIF sin PCSX2 ni GPU | Validar la VU1 solo a ojo en PCSX2 |
| D4 | **Gameplay a paso fijo de 30 Hz.** Acumulador en microsegundos enteros; pawns y cámara interpolados; render al vsync (30 fps en NTSC, 25 en PAL) | Determinismo por plataforma y ritmo estable | Mantener el paso variable con clamp de 0,1 s |
| D5 | **La iluminación se hornea en LeonCook**, no en Blender | Determinista, así que G5 sigue siendo byte a byte | Hornear en Blender |
| D6 | **Los scripts de Blender son la fuente de verdad.** Blender MCP solo sirve para crear: cada llamada MCP es bpy, y todo acaba en un `make_*.py` versionado y determinista | Reproducible y revisable | Versionar solo el `.blend` |
| D7 | **Ninguna función del GS se usa sin escena de conformidad.** Primero va en GSReference y en una escena de GSConformance, después en el renderer. Un validador en `FGSCommandList` rechaza lo que no está emulado | El PC nunca debe mostrar lo que la PS2 no hace | Emular después de usar |
| D8 | **Recorte:** la VU1 descarta los triángulos que quedan fuera y aplica el guard band. Los lotes que cruzan el near o el guard band van al clipper C++ en el EE, que lo decide con una esfera envolvente | Microprogramas pequeños; el recorte completo en VU es caro y arriesgado | Recortar todo en la VU1 |
| D9 | **PS2 a -O2** (`CMAKE_CXX_FLAGS_RELEASE_INIT`) | I-cache de 16 KB; es lo que dicen los docs | -O3 (lo de hoy, por accidente) |
| D10 | **Se borra lo que se sustituye, en el mismo commit.** Ni rutas paralelas, ni `#if` de compatibilidad, ni stopgaps; los símbolos retirados van a `CheckBannedApis.ps1` | Nada legacy (decisión del usuario) | Mantener las dos rutas durante una transición |
| D11 | **Se borran ThirdPerson, la vía inmediata de PS2RHI, Linux, Jolt y FBX.** glTF pasa a ser el único formato, también para skeletal y animaciones | Decisión del usuario: un solo juego, una sola ruta de render, las mismas plataformas y la misma física en PC y PS2 | Portar ThirdPerson al motor |

## Forma de trabajo

- **Rama:** `plan/ps2-shipping`, un commit por fase (N30 en sub-commits a–e), sin push.
- **Antes de cada commit:**
  - `git checkout -- Engine/Content/EngineMaterials/T_Default_Black.lasset` (los tests lo regeneran);
  - `FormatCode.bat`;
  - la entrada en `CHANGELOG.md` `[Unreleased]`.
- **Gates base** en todas las fases:
  - `Lint.bat` (G1, G4, /W4);
  - `RunTests.bat` (LeonAutomationTests, ShooterGameTests y TestPAL);
  - `SmokeTest.bat` (G6);
  - `BotMatch.bat 10 7` (dos pasadas idénticas).
- Cada fase añade su gate propia.

## Fases

### Hito 0.22.0: base medida y limpia

### N0 · Aterrizar el WIP (S) — hecha

- Arreglos de playtest (el jugador muerto vuelve a jugar, fuego automático a 30 fps, unscope de la AWP al recargar,
  bots congelados que no ven), `-Only` en Package y el desglose de `-LogFrameTimes` (`Frame split`, `Present over`).
- `Package.ps1`: la comprobación «nothing to package» va detrás de `-Only`, y se borra el bloque del `Packages\`
  antiguo de la raíz.
- Los tiempos de `-LogFrameTimes` pasan a ciclos enteros (`FFrameTimeClock`, `Cycles64`), y el timer solo se lee con
  el flag.
- README de ShooterGame y TESTING.md (49 tests).

Gate: base y `Package.bat`.

### N1 · Medición automática en PCSX2 (M) — hecha

- D9: la PS2 compila a -O2.
- `FFrameStats` separa juego, render, submit y espera del GS con cycles del EE; percentiles p50/p95/p99 y una línea
  final `LogFrameStats: Summary`.
- `-ExitAfterSeconds=N` y `-BotMatchSpectate`, una cámara fija para medir con render.
- `RunPCSX2.ps1`:
  - ShooterGame por defecto;
  - `-Headless -Timeout`;
  - ini de PCSX2 fijado en `Engine/Platforms/PS2/Build/PCSX2/`;
  - lee `emulog.txt` hasta el resumen y cierra PCSX2.
- `MeasurePS2.bat`: CSV en `Saved/Profiling/` y la fila de Budgets.md.
- `RunGates.bat`: todas las gates locales en orden.

Gate: 3 pasadas de `MeasurePS2.bat` dentro de ±3 %; fila «0.22 baseline» en Budgets.md.

Estado: hecha.
- La línea base, idéntica en las tres pasadas (PCSX2 es determinista en tiempo emulado): **25,4 fps**; p50/p95/p99
  33,5 / 50,3 / 66,8 ms. El frame se reparte en mundo 9,0 ms, escena 11,8 ms, HUD + canvas 4,8 ms, audio 2,4 ms, y el
  present con la espera del vblank 11,1 ms. El GS está casi ocioso.
- -O2 quita 145 672 bytes de texto al ELF de ShooterGame (−8,4 %).

Desviaciones:
- Los tiempos usan `FPlatformTime::Cycles64` (el reloj del bus, `FFrameTimeClock`) y no el Count del COP0; los
  contadores PCCR llegan con el profiler (N9).
- `-BotMatchSpectate` se apoya en `ViewNextPlayer` (spec_next de CS), que adelanta el espectador de compañeros de N30d.
- `ClientRestart` vuelve a ver el pawn propio, como en UE.
- `-datapath` de PCSX2 es el padre de su carpeta de datos, y el `printf` del EE sale por el `tty:` del IOP, así que
  `Measure.ini` activa la consola del IOP.

### N2 · Borrar ThirdPerson y la vía inmediata (M) — hecha

- Se borran:
  - `Game/ThirdPerson`;
  - `PS2Draw3D.cpp`, `PS2SceneState.*` y `PS2Texture.*`;
  - `DrawBox`/`BindMaterial`/`SetViewTarget` y sus tipos;
  - `BeginDraw3DStatsFrame` y el enlace a math3d/draw.
- `FGSDebugDraw` en GSCore: fuente PSMT4 con SPRITE y rectángulos, sobre `FGSCommandList`. StatsOverlay, ErrorScreen y
  GSConformance pasan a usarlo.

Gate: G3, G8, `Package.bat`, GSConformance y ErrorScreen en PCSX2.

Estado: hecha. GSConformance y la pantalla de error arrancan en PCSX2 con su texto dibujado por `FGSDebugDraw`
(capturas de la ventana).

Desviaciones:
- La fuente sigue siendo la de celdas 5x7 dibujadas con sprites; el atlas PSMT4 llega con el HUD en sprites (N15).
- También se borran:
  - la rama `WITH_ENGINE=0` de Launch y los `PlatformEngineLoopHooks`;
  - `FPS2StatsOverlay`: el panel de «EE ms» y el mando, que solo se dibujaba en ThirdPerson, así que ShooterGame
    nunca lo mostraba;
  - `FStatsOverlay` de Core: la visibilidad de `stat unit` pasa a `UEngine`;
  - la opción `COMPILE_AGAINST_ENGINE`: los juegos compilan siempre contra el motor.
- R3 alterna `stat unit` en el DualShock (antes L3 + R3 ciclaba el panel borrado).

### N3 · Retirar Linux y Jolt (S) — hecha

- Plataforma Linux (se mantiene la detección del host Docker), plugin JoltPhysics y `LEON_WITH_JOLT`.

Gate: base (el botmatch da el mismo resultado sin Jolt) y G3.

Estado: hecha. El botmatch da CT 7 - T 3 igual que antes y se repite idéntico; G3 compila los tres ELF.

Desviaciones: con Jolt desaparece también la capa de backends de física que solo existía para él (`IPhysicsBackend`,
`EPhysicsBackend`, el backend Arcade vacío, las ramas rígidas y de narrow phase de `FPhysScene`,
`UWorld::SetPhysicsBackend`). `FPhysScene` es la única implementación, igual en Win64 y PS2. El sistema de plugins
se queda, aunque el motor ya no trae ninguno; su test crea un plugin de muestra. Las herramientas de host de Linux
(LeonHeaderTool en Docker, leonrun) siguen.

### N4 · Datos muertos y documentación rancia (S) — hecha

- `FMaterial`:
  - fuera `Specular`, `Metallic`, `Shininess`, `Roughness`, `bPlanarMirror` y `NormalMap`;
  - `bCastsShadows` pasa a `bCastBlobShadow`;
  - `BlinnPhong` pasa a `Lit`.
- Fuera `FVertex::Tangent`, `LegacyContentYaw` y la «legacy fly camera».
- El pak de PS2 deja de llevar GLSL y normal maps.
- Documentación rancia (TOOLS, CookNotes, BUILD, TESTING/ARCHITECTURE, ps2-engine).

Gate: G5 y base.

Estado: hecha.
- `FVertex` baja de 48 a 32 bytes y el vértice skinned de 80 a 64.
- `VER_LEON_REMOVE_VERTEX_TANGENT` es la versión mínima cargable.
- Se reimportó y se volvió a guardar todo el contenido (65 paquetes); un segundo reimport da los mismos hashes.
- El pak de PS2 pasa de 568 a 456 KB (sin GLSL ni normal maps) y el informe de VRAM de 81 a 16 KB.

Desviaciones:
- Nada leía `bCastsShadows`, así que se borra toda la cadena sin crear `bCastBlobShadow`; lo añadirá N15 junto con su
  usuario.
- La «legacy fly camera» era el `ADefaultPawn` de UE, una función real: solo se corrige el comentario.
- Se borran también `M_SolidMetal` y `T_Default_Bump_N`.
- `ITargetPlatform::GetAllTargetedShaderFormats` decide si se escenifican los shaders.

### N5 · Bugs confirmados del motor (M) — hecha

- TCC=RGBA con TEXA.
- `audsrv` comprueba el hueco antes de encolar.
- Reconexión del pad.
- Escena:
  - `ResolveCharacterOverlaps` una sola vez;
  - el tick sin copias de componentes;
  - `BindEffectsMask` cacheado;
  - translúcidos transformados una sola vez.

Gate: G8 y `MeasurePS2`.

Estado: hecha.
- Las paletas usan TCC=RGBA: el alfa cocinado de la CLUT llega al blend.
- El pad vuelve a pedir el modo analógico tras reconectarse: `FDualShockConnection`, neutral de plataforma y con test,
  y lo que se comprueba es el modo actual (`padInfoMode`), no una bandera.
- El dispositivo de audio cuenta el tiempo en ciclos enteros, sin doubles.
- `Queue` de PS2 solo encola lo que cabe en el búfer de audsrv, en lugar de bloquear el hilo de juego.
- El tick de los actores y la resolución de solapes no reservan memoria.
- La máscara de efectos se construye una vez y los translúcidos de una malla se transforman una vez.
- Tests nuevos: `System.ApplicationCore.DualShock.Reconnect`, `System.AudioMixer.SoftwareMixer.NoDrift` y el alfa en
  `System.Renderer.GS.TextureCache.Paletted`.

Desviaciones:
- `ResolveCharacterOverlaps` se sigue llamando dos veces, porque la primera también manda las cápsulas a sus cuerpos
  antes del Step. Cuesta unos 135 pares por llamada y ya no reserva memoria; el grid de N16 lo sustituye.
- `MeasurePS2` se repite cuando se integra N9, el profiler.

### N6 · Bugs de ShooterGame (S) — hecha

- Arma vacía de los bots.
- C4: ~4445 cm y respeta el blindaje.
- Retroceso de los bots.
- Menú de compra: fin del tiempo de compra, espectador, y `BuyEndTime` desde el fin del freeze.

Gate: base, con un test por bug.

Estado: hecha. Un test por bug, que falla sin el arreglo:
- `EquipBestWeapon` salta las armas sin munición (cargador y reserva vacíos) y, si todas lo están, deja la que hay en
  la mano (`ShooterGame.Weapons.BestWeaponSkipsEmpty`).
- La C4 llega a 4445 cm (1750 unidades) y usa el `ArmorRatio` de la HE, 1 (`ShooterGame.Bomb.ExplosionRadius`,
  `ShooterGame.Bomb.ExplosionRespectsArmor`).
- El bot gira su propia puntería y el retroceso va encima; `RecoilCompensation` (0,5 por `Difficulty`) baja esa parte
  de cada patada (`ShooterGame.Bots.RecoilKicksTheAim`).
- El menú de compra solo se abre si el jugador puede comprar (vivo, sin espectar, en su zona y en tiempo), se cierra
  solo cuando deja de poder y el HUD dice por qué (`ShooterGame.Buy.MenuFollowsTheRules`).
- `BuyEndTime` cuenta desde el fin del freeze (`ShooterGame.Buy.BuyTimeAfterTheFreeze`).

El botmatch con semilla 7 pasa de CT 7 - T 3 a CT 8 - T 2 (78 bajas) y se repite idéntico.

Desviaciones: `TurnToward` era del bot del juego (`AShooterAIController`), no del `AAIController` del motor, así que
el motor no cambia. Salir de la zona de compra también cierra el menú.

### N7 · Memoria local del GS con swizzle real (L) — hecha

- Tablas de página, bloque y columna de todos los PSM (manual del GS §8), compartidas por el emulador y GSReference.
- Asignación de texturas según su huella real.
- Escena «dos PSMT8».

Gate: G8 y GSConformance.elf en PCSX2 igual que Win64.

Estado: hecha. `FGSLocalMemory` (GSCore) direcciona como el GS los 13 PSM: páginas de 8 KB, bloques colocados por la
tabla de cada formato, columnas y el orden de píxeles de las figuras del §8; PSMCT24 y los formatos H comparten las
palabras de PSMCT32, y la memoria da la vuelta en 4 MB. La usan las transferencias, `FGSTexelDecoder` (CLUT en CSM1),
`FGSTextureLayout`, el emulador GL y GSReference. Una CLUT de PSMT8 ocupa 4 bloques y una de PSMT4 1, así que las
CLUT ya no se pisan en PC.
- Las escenas de conformidad anteriores y SceneFrame dan los mismos bytes que antes, en el emulador y en la referencia.
- La escena nueva `TwoPalettes` queda sin ningún píxel fuera de ±2, y en PCSX2 se ve igual que en Win64.
- Tests: `System.GSCore.LocalMemory.*` (5), `System.GSReference.Texture.TwoPalettes` y
  `System.Renderer.GS.TextureCache.TwoPalettes` (423 del motor, TestPAL 137).

Desviaciones:
- CSM2 sigue fuera del contrato (`FGSCommandList` lo rechaza), así que solo se carga CSM1.
- La caché empaqueta las texturas que caben en una página desde cualquier bloque (una PSMT8 de 16x16 ocupa 1 bloque);
  las mayores empiezan en página.
- El informe de VRAM del cook cuenta los bloques reales y redondea los KB hacia arriba.
- GSConformance pasa a 4 columnas a escala 2 para que quepan las 10 escenas.

### N8 · Paridad del emulador GS (L) — hecha

- MIPTBP y LOD dinámico desde Q.
- ATST, FOG, TEXA, REGION_REPEAT.
- SPRITE, TRISTRIP/TRIFAN con ADC.
- PABE/FBA, COLCLAMP.
- CLD 0–5.
- Validador en `FGSCommandList` (D7).

Gate: G8 con todas las escenas y capturas de PCSX2.

Estado: hecha. El emulador GL hace lo que la referencia en todo lo que `FGSCommandList::IsSupported` acepta, y cada
cosa tiene su escena de conformidad (20, diez nuevas) y su test de GSReference con los valores del manual.
- MIPMAP: una capa por nivel (MIPTBP1 / MIPTBP2) y el LOD del GS por píxel en el shader desde la Q interpolada, con
  MMAG / MMIN, MXL, L, K y el trilineal por la fracción del LOD.
- Mezcla: la ecuación entera (Cd como factor, Ad, FIX y As por encima de 0x80, resultados negativos), COLCLAMP con
  vuelta, el dither después de la mezcla contra el destino de 16 bits, PABE, FBA, DATE y FBMSK bit a bit. Lo que lee
  el framebuffer va por grupos de primitivas que no comparten píxel, sobre una copia del frame; el AFAIL que escribe,
  en una segunda pasada del grupo.
- CLD 0–5 con CBP0 / CBP1 en `FGSClutBuffer::Update`, compartido; REGION_CLAMP / REGION_REPEAT en cualquier nivel;
  líneas y puntos paso a paso con `GSStepLine` (GSCore), igual que la referencia.
- La referencia interpola con una sola división (valores enteros exactos donde el GS los da enteros).
- `System.Renderer.GSEmulator.Conformance`: 18 escenas sin ningún píxel a más de 2 niveles; StripsAndSprites tiene 3
  (centros sobre un lado poco inclinado del abanico) y MipmapLod 3 (un peso bilineal minificado). SceneFrame, igual
  que antes: 29 píxeles más allá de un paso de 5 bits.
- Tests: 433 del motor (10 nuevos de GSReference), TestPAL 137.
- PCSX2 (renderer software, desde una carpeta de datos propia): 14 escenas iguales píxel a píxel a Win64 (AlphaTest,
  Fog, TexAAndFunctions, BlendEquation, PabeFbaDate, ClutLoads con su CLUT caducada incluidos).

Desviaciones:
- Lo que PCSX2 hace distinto, sin resolver contra una consola: trunca el color Gouraud (la referencia redondea: un
  paso de 5 bits en Dither16Blend); lee DIMX traspuesto (Dither16, Dither16Blend); no filtra en bilineal los sprites
  UV/STQ ampliados (TextureSampling, ClampModes, MipmapLod); con LOD fijo (LCM 1) el trilineal le sale mal en azul; y
  sus líneas caen un píxel distinto que `GSStepLine`. En los píxeles del abanico que el GL pinta mal, PCSX2 da la
  razón a la referencia.
- REGION_REPEAT desplaza sus campos con el nivel como REGION_CLAMP: el manual solo lo dice de REGION_CLAMP y PCSX2
  lo hace así.
- Un LOD de exactamente 0 usa MMAG (3.4.12); la tabla de TEX1 dice MMIN desde 0. Se queda como dice el texto.
- La cobertura sigue siendo la de la GPU con la muestra desplazada 1/256: un centro justo sobre un lado poco inclinado
  o un vértice puede ir al otro lado. El emulador tiene un solo frame de 640x448 (no lee un FRAME como textura).
- El renderer no usa todavía nada de lo nuevo, así que el botmatch no cambia.

### N9 · Profiler (M) — hecha

- `SCOPE_CYCLE_COUNTER`/`DECLARE_CYCLE_STAT`; PCCR en PS2 y QPC en Win64; página en el overlay.

Gate: desglose en `MeasurePS2`, con overhead menor del 1 %.

Estado: hecha.
- `Stats/Stats.h` en Core: `DECLARE_STATS_GROUP`, `DECLARE_CYCLE_STAT(_EXTERN)`/`DEFINE_STAT`, `SCOPE_CYCLE_COUNTER`,
  `FThreadStats` (un árbol fijo de 256 nodos y una pila de 16, sin memoria por scope) y `FCycleStatsWindow`. Solo
  graba con `-LogFrameTimes` o `stat cycles` (F7); apagado, un scope cuesta un salto. 6 tests `System.Core.Stats.*`,
  también en TestPAL sobre el EE.
- El desglose del EE en PCSX2 (`MeasurePS2.bat -Label N9`, idéntico en tres pasadas): **24,7 fps**, 40,5 ms de media.
  Escena 12,6 ms, mundo 9,2 ms (los line traces del sensing, 3,5 ms en 42 trazas por frame), canvas 4,6 ms, mezcla de
  audio 2,1 ms, paquete GIF 1,0 ms y espera del vblank 9,4 ms: el EE trabaja 31,0 ms por frame.
- Coste: un scope grabado son 133 ciclos (451 ns) en el EE; unos 100 por frame, el 0,11 % del frame. En Win64, el
  botmatch headless (`BotMatch 10 7`, frames de 75 µs sin render) gasta un 2,6 % más de CPU con los stats (1,797 s →
  1,844 s de mediana en 15 pasadas; un scope, 30 ns: dos lecturas de QPC de 12,7 ns) y un 3,4 % con todo
  `-LogFrameTimes`; con ventana, a 33 ms por frame, es el 0,01 %.

Desviaciones:
- El tiempo sale del Count del COP0 (el reloj de la CPU, un `mfc0`), no del evento de ciclos de PCR0: en un PCR llegaría
  al bit 31 en 7,3 s (una carga larga) y el desbordamiento lanza la excepción de contador, que no se puede enmascarar.
  Así PCR0 y PCR1 cuentan los fallos de la I$ y de la D$ a la vez; PCSX2 no los emula (leen 0).
- Los stats sustituyen al desglose a mano: fuera `FFrameTimeClock`, `FEngineFrameSplit`, `FViewport::FDrawTimes`,
  `FGameViewportDrawTimes` y el `Present over` del PS2 (a `CheckBannedApis.ps1`). `Frame split` y `FrameStats Summary:`
  salen de los scopes con las mismas claves.
- `-LogFrameTimes` funciona también headless, y al salir registra el `Profile` de toda la partida. El hash de
  `Measure.ini` de MeasurePS2 ignora los finales de línea del checkout.
- El objetivo del 1 % se cumple en el EE y con ventana, no en el botmatch headless de Win64.

### N10 · Vblank por interrupción y región (S) — hecha

- ROMVER decide PAL o NTSC; `AddIntcHandler` con semáforo; conteo de campos.

Gate: histograma en múltiplos de vsync y ningún spin.

Estado: hecha.
- `FPS2VerticalBlank` (PS2RHI): un manejador de `INTC_VBLANK_S` cuenta los campos y hace `iSignalSema` sobre un
  semáforo de un solo aviso; `WaitVSync` duerme en `WaitSema` hasta el campo que decide `FGSFieldPacer` (GSCore, enteros,
  probado en Win64 y en el EE): `SyncInterval` campos después del último flip o, si ese blanco ya empezó, el siguiente.
  Fuera `graph_wait_vsync`, el `FieldSeconds` de 59,94 Hz y `LastFlipSeconds` (a `CheckBannedApis.ps1`, con
  `graph_initialize` y los demás sondeos del CSR de libgraph).
- La región sale de ROMVER (`graph_get_region`; `-PAL` / `-NTSC` la fuerzan). PAL mantiene el frame de 640x448
  centrado en sus 512 líneas (32 negras arriba y abajo): el mismo frame, la misma VRAM y la misma arena de texturas en
  las dos regiones; 25 fps en PAL y 30 en NTSC con `SyncInterval=2`. `FPS2RHI::ShutdownDisplay` quita el manejador.
- PCSX2 (`MeasurePS2.bat -Label N10`, idéntico en dos pasadas): 25,5 fps, 39,3 ms de media, p50 / p95 / p99 de 33,5 / 50,3 / 66,8 ms (los cubos
  de 250 µs de 2, 3 y 4 campos); la espera del vblank (11,2 ms) es ya una espera de semáforo. El EE trabaja 28,1 ms por
  frame.
- Tests: 2 `System.GSCore.FieldPacer.*` (439 del motor, TestPAL 145 en Win64 y 138 en el EE); GSConformance dibuja
  igual en PCSX2.

Desviaciones:
- El frame PAL de 512 líneas queda para cuando la arena lo permita (N13): costaría 0,3 MB de VRAM de texturas y un
  14 % más de relleno; con 448 centradas la imagen sale un 12,5 % más achatada en una tele PAL.
- La partida medida no es la de N9 frame a frame: los tiempos de frame alimentan el paso variable, así que la misma
  semilla juega otra partida (322 triángulos de media contra 362); se compara por partes.

### N11 · Cadenas DMA con doble búfer (M) — hecha

- `packet2` en modo CHAIN, uncached-accelerated, texturas por REF, construir N+1 mientras se transfiere N.

Gate: test de tags en TestPAL, GSConformance sin cambios y submit más bajo.

Estado: hecha.
- `FGSGifPacket::BuildChain` (GSCore): el mismo codificador que `Build` (una plantilla, dos escritores) escribe el frame
  como cadena DMA de origen: secciones CNT de hasta 0xffff quadwords con los GIFtags y las escrituras A+D, un REF a los
  píxeles de cada subida en la copia del propio `FGSCommandList` (alineada a 16 bytes, sin copiarlos al paquete) y END.
  Con `GetChainCapacity`, `MakeDmaTag`, `GetDmaAddress` y `EGSDmaTag`.
- PS2RHI: dos listas, dos búferes de cadena (`FPS2GifChain`, 128 bytes de alineación y líneas enteras) y los dos frame
  buffers. `WaitVSync` construye la cadena del frame N mientras el DMA y el GS siguen con N - 1, espera el FINISH de
  N - 1 justo antes del flip (`draw_wait_finish`, su scope `GS Finish`), lo muestra en su campo y lanza N con
  `dma_channel_send_chain_ucab` sin esperar; el búfer se retira (`dma_channel_wait`) solo antes de reutilizarlo. Un
  frame sale a pantalla un frame después de grabarse.
- Coherencia: la cadena se escribe solo por el segmento uncached-accelerated (`UCAB_SEG`): no ocupa la caché de datos
  de 8 KB ni hay que volcarla; un `SyncDCache` al reservar el búfer y `sync.l` antes del kick. Los píxeles por REF se
  escribieron con caché, así que se vuelcan con `SyncDCache` antes del kick.
- Fuera `FlushFrame`, el `TArray` estático, el `memcpy`, los envíos síncronos por trozos y libpacket (a
  `CheckBannedApis.ps1`).
- PCSX2 (`MeasurePS2.bat -Label N11`, lo mismo en dos pasadas): 25,7 fps, 38,9 ms de media, p50 / p95 / p99 de 33,5 / 50,3 / 66,8 ms; el
  paquete GIF baja de 1,04 ms (N9) a 0,51 ms por frame con un frame mayor (25,0 KB: 0,020 ms por KB frente a 0,043).
  Present 10,2 ms, espera del vblank 9,7 ms.
- Tests: `System.GSCore.GifPacket.Chain` (QWC, ID y ADDR de cada tag, la alineación de los píxeles y que la cadena,
  seguida como el DMAC, entrega los bytes de `Build`; también en TestPAL sobre el EE): 440 del motor, TestPAL 146 en
  Win64 y 139 en el EE. GSConformance en PCSX2: igual píxel a píxel que con N10.

Desviaciones:
- No se usa `packet2_t`: sus búferes van alineados a 64 bytes, con 0xffff quadwords como máximo y fuera de `GMalloc`, y
  sus ayudas de tags serían un segundo codificador de la cadena. Los búferes son de `FMemory` a 128 bytes.
- PCSX2 no carga al reloj del EE el dibujo del GS (`GS Finish` da 0,00 ms antes y después), así que el solapamiento del
  EE y el GS no se ve en estas medidas: solo en el hardware. La partida medida vuelve a ser otra (378 triángulos).
- Los píxeles de las subidas se siguen copiando del emisor a la lista del frame (`FGSCommandList::Append`); lo que ya no
  se copia es el paquete.

### Hito 0.23.0: VU1 y datos nativos (botmatch a 30 fps en PCSX2)

### N12 · LPS2 v2 (L) — hecha

- Lotes para la VU1, tiras con ADC, int16, normales 3×int8, RGBA8 horneado, UV 4.12.
- Skinned con 2 pesos y paleta de ≤ 24 huesos.
- meshoptimizer (MIT, Developer).

Gate: G5, doble cook idéntico y error ≤ 0,5 cm.

Estado: hecha. Los static meshes se dibujan desde LPS2 v2 en Win64, en los tests y en la PS2 (D1), y el formato está
en [ASSET_FORMATS.md](../ASSET_FORMATS.md#lps2-v2).
- Formato (`FLPS2Mesh`, RenderCore): lotes de ≤ 64 vértices, porque 3 + 7 × 64 = 451 de las 496 quadwords de medio
  búfer de la VU1 (entrada desempaquetada y salida GIF). Cada lote son tiras seguidas con ADC (flag `0x80`) en los
  vértices que no cierran triángulo y `0x01` en los impares. Posiciones int16 × 3 con escala y bias por malla, normales
  int8 × 3 con los flags en W, RGBA8 horneado (blanco), UV 4.12 con un offset entero por lote y material por sección.
  Cada stream va alineado a quadword para su UNPACK (V3-16, V4-8, V4-8, V2-16): 18 bytes por vértice frente a 32.
- Cook: `FLPS2MeshBuilder` (MeshUtilities, meshoptimizer 1.2) suelda, ordena para tiras, hace `meshopt_stripify` con
  restart y reparte tiras enteras en lotes. `UStaticMesh::BuildFromMeshData` lo pide a `IMeshBuilderModule`, que solo
  enlazan el editor y los tests. El mismo source da los mismos bytes; un segundo reimport de todo el contenido, también.
- Error: 0,0016 cm como máximo en las mallas de ShooterGame y 0,059 cm en el mundo de de_leon, con la escala de sus
  actores (`ShooterGame.Content.MeshQuantization`); una malla de 655 m queda en 0,498 cm.
- Emisor: la esfera de cada lote contra los planos decide (D8). Si queda fuera, el lote se salta. Si está dentro de la
  guard band y de near y far, va como TRISTRIP y solo manda los vértices de los triángulos dibujados (XYZ3 en los
  demás). Si cruza un plano, sus triángulos pasan por el clipper.
- El mismo frame: `System.Renderer.GS.Scene.StripsDrawTheSource` da los mismos píxeles que los triángulos del source
  uno a uno, con 391 escrituras del GS en vez de 946. SceneFrame sigue en 29 píxeles.
- La colisión son los triángulos del source en float, guardados al lado (`FTriMeshCollisionData`). El botmatch no
  cambia: CT 8 - T 2, 78 bajas, repetido idéntico.
- Contenido: `VER_LEON_LPS2_MESH` (3) es la versión mínima cargable, y se reimportaron y guardaron los 65 paquetes.
- PS2 (PCSX2 2.8.2, Measure.ini de7fb888): 24,6 fps, escena 14,4 ms, 518 triángulos, 1 828 escrituras y 28,6 KB de
  GIF por frame. N8 con el mismo ini da 25,1 fps, 12,6 ms, 372, 1 606 y 25,1 KB; la línea base N1, 25,4 fps, 11,8 ms,
  320, 1 371 y 21,4 KB. Cada triángulo de la escena cuesta 27,9 µs en vez de 33,8.
- Pak de PS2: 464 KB (456 KB con N8).
- Tests: 437 del motor (6 nuevos: `System.MeshUtilities.LPS2.*` y `System.Renderer.GS.Scene.StripsDrawTheSource`),
  57 de ShooterGame y TestPAL 137.

Desviaciones:
- En PCSX2 el botmatch no se repite entre builds. El juego avanza con el tiempo de cada frame (D4 llega en N18), así
  que cambiar el coste del render cambia la partida: N8 mata 18, N12 14, y la cámara ve otra cosa. Por eso se compara
  por triángulo y con el frame fijo del test.
- La colisión guardada en float y los vértices que dos tiras comparten pesan más de lo que ahorra el vértice de 18
  bytes: +8 KB de pak. El BVH cocinado de N16 sustituirá esa colisión.
- Skinned solo queda documentado: 48 vértices por lote con una paleta de 24 huesos. `USkeletalMesh` sigue en float
  hasta su import glTF (N21).
- La simplificación de meshoptimizer está disponible, pero no se usa hasta los LODs (N15).
- MeshUtilities depende de Engine para implementar `IMeshBuilderModule`, como en UE.
- Hasta N14 no hay VU1: el EE transforma en C++ cada vértice de tira de los lotes visibles.

### N13 · Residencia de texturas (M) — hecha

- LRU por páginas, presupuesto de subida, CLD 4/5, mips, draws agrupados por TEX0.

Gate: 3× sobresuscripción sin ningún reset.

Estado: hecha.
- Residencia: `FGSTextureCache` da a cada textura un tramo de bloques de la arena (un mapa de bits), con sus niveles
  a su alineación real y la CLUT detrás (`FGSTextureLayout::GetFootprint`, lo mismo que cuenta el informe de VRAM del
  cook). Cuando no cabe, expulsa las texturas menos usadas de frames anteriores, de la más antigua a la más nueva;
  nunca una que el frame ya ha usado ni la arena entera. Si las del propio frame la llenan, la siguiente se dibuja
  plana.
- Presupuesto: `[/Script/Engine.RendererSettings] TextureUploadBudgetKB=128`; la primera subida del frame siempre va.
  Lo que no cabe sube su nivel más pequeño y su CLUT y se dibuja con él, o se dibuja con el color medio de sus
  téxeles; el resto sube en los frames siguientes.
- CLUT: la caché sigue lo que tienen CBP0 / CBP1 y el búfer (PSMT8 con CBP0 en CSA 0, PSMT4 con CBP1 en CSA 1 o CBP0
  en CSA 0) y escribe CLD 4 / 5, así que el GS solo carga cuando cambia. Escribe CLD 2 / 3 cuando un registro nombra
  una CLUT que el búfer ya no tiene: la pisó una carga PSMT8, o una textura nueva ocupó los bloques de otra expulsada.
- Mips: el cook genera la cadena de P4/P8 hasta 8 téxeles en el lado corto, con una sola paleta. Promedia en caja en
  espacio lineal y pondera el color por el alfa. El nivel 0 decide el formato y queda exacto; los colores de los mips
  llenan las entradas libres por median cut. Los mips 1 en adelante son solo índices
  (`GetPixelFormatMipDataSize`, `UTexture2D::AddMip`); el formato del paquete ya llevaba los mips, así que la versión
  no cambia y no hubo que reguardar contenido (un reimport sigue dando los mismos bytes). El render usa
  LINEAR_MIPMAP_LINEAR, MXL, MIPTBP1 / MIPTBP2 y LCM 0 con L = 0 y K = log2(téxeles por cm / píxeles por cm a 1 cm)
  + `LodBias`. La densidad de UV de cada sección sale del blob LPS2 al cargarlo; `UMaterial` gana `bMipmaps` y
  `LodBias`.
- Orden: las secciones opacas se agrupan por textura, en el orden en que aparece cada una en la escena, y TEX0,
  MIPTBP, TEX1 y CLAMP solo se escriben cuando cambian.
- GS: ClutAndFormats dibuja además niveles PSMT8 / PSMT4 con una sola CLUT, trilineal, cargada con CLD 2 a 4. El
  emulador queda sin ningún píxel fuera de ±2 y `System.GSReference.Texture.ClutAndFormats` comprueba los valores.
  SceneFrame pasa a usar la conversión del cook (PSMT8 con mips): 20 píxeles más allá de un paso de 5 bits, antes 29.
- Tests: 456 del motor (`System.Renderer.GS.TextureCache.Oversubscribed`, `.EvictionOrder`, `.UploadBudget`,
  `.ClutLoads`, `System.Renderer.GS.Scene.DrawsGroupedByTexture`, `System.TextureCompressor.Paletted.MipChain`), 63 de
  ShooterGame y TestPAL 143. Con 3× de sobresuscripción, cada frame expulsa dos texturas y nunca se reinicia; seis
  cubos de dos texturas escriben TEX0 dos veces y cargan dos CLUT.
- PS2 (PCSX2 2.8.2, `MeasurePS2 -Label N13`): 29,0 fps, escena 12,5 ms, 432 triángulos, 1 518 escrituras y 23,7 KB
  de GIF por frame (N12: 24,6 fps, 14,4 ms, 518, 1 828 y 28,6 KB). Son 29,0 µs por triángulo frente a 27,9, y
  3,5 escrituras por triángulo en los dos. Ninguna subida de textura ni carga de CLUT tras la primera
  (`tex_resident_kb=4`).

Desviaciones:
- La base incluye N16 y N20 y la partida es otra (CT 0 - T 2 frente a la de N12), así que la comparación es por
  triángulo. El ini da el hash 989212e8 (el de N1 y N9) en este checkout, no de7fb888; su contenido no cambia desde N1.
- de_leon no tiene texturas propias: en el botmatch la residencia, el presupuesto y los mips apenas trabajan. Se
  miden con el arte real (N27 / N28).
- Una textura sin área de UV (o un material sin `bMipmaps`) muestrea el nivel 0 en bilineal; la densidad de un mesh
  skinned se calcula en su primer draw.
- ClutAndFormats cambia, así que la captura de GSConformance en PCSX2 queda pendiente de repetir.

### N14 · Microprogramas VU1 y PATH1 (XL) — hecha

- StaticUnlit, StaticLit y Skinned, con CLIP y guard band (D8).
- Doble búfer en la memoria de la VU; XGKICK.
- `VU1Conformance`; `GSH_Capture` (D3).

Gate: `VU1Conformance: PASSED` en leonrun y PCSX2, replay dentro de ±2 y fps.

Estado: hecha. Los lotes de los static meshes dentro de la guard band se transforman, iluminan, recortan por CLIP y
se empaquetan para el GS en la VU1.
- Comando: `FGSCommandList::DrawVertexBatch` (GSCore) graba el lote de LPS2 v2 (`FGSVertexBatch`, los streams donde
  los guarda la malla) con su draw (`FGSVertexDraw`: cuantización, LocalToClip, LocalToWorld y normal, material,
  luces) como escritura `EGSRegister::VertexBatch`. El emisor C++ pasa a GSCore y es la referencia
  (`FGSPrimitiveEmitter::TransformVertexBatch` / `AddVertexBatch`, `FGSCommandList::AppendExpanded`); el renderer
  graba el comando solo con `SetVertexBatches` (la PS2 con VU1) y si un microprograma hace su luz (sin luz, o ambiente
  y un sol); si no, lo emite él. Win64 no cambia: `System.Renderer.GS.Scene.VertexBatches` da 0 píxeles distintos entre
  el comando expandido y el emisor directo.
- Microprogramas: `PS2RHI/Private/VU1/VU1Programs.vsm` (StaticUnlit y StaticLit, un solo bucle, 124 instrucciones),
  escrito a mano para `dvp-as`; LeonBuildTool ensambla los `Private/VU1/*.vsm` de cada módulo en el build de PS2
  (`LeonPlatform_PS2_ModuleSources`) y `FPS2VU1::UploadPrograms` los sube con MPG. Por vértice: la matriz con la
  escala y el bias de la malla dentro, 1/w (DIV), el píxel 12.4 con FTOI4 (redondeado como `GSToFixed4`), Z, STQ,
  RGBAQ redondeado, CLIP de los tres vértices y el signo del área en pantalla (FMAND) para el ADC. Memoria: 3
  quadwords compartidos, BASE 16 y OFFSET 504, cabecera de 9 (15 con luz), streams en +16/+80/+144/+208 y el paquete
  GIF en +272.
- Cadena: el frame entero va a VIF1 (`FGSGifPacket::BuildChain` con `IGSVertexBatchEncoder`, TTE): las escrituras
  del EE por DIRECT (PATH2), cada lote como CNT con la cabecera (UNPACK en TOPS), cuatro REF a los streams sin copia y
  MSCAL; el paquete antes de un lote cierra con EOP y el primer DIRECT tras lotes lleva FLUSH. PATH3 ya no se usa
  (`DMA_CHANNEL_GIF` y los helpers de packet2 a `CheckBannedApis.ps1`). `FlushCache` antes del kick en vez de
  `SyncDCache` por imagen.
- D8: la esfera de cada lote decide; fuera se salta, cruzando near o guard band va al clipper C++, dentro va a la VU1.
- `-novu1` hace que el renderer emita cada lote con el mismo emisor C++ de Win64.
- Luz horneada (N22): una sección con luz de un componente Static va a la VU1 como StaticUnlit con los colores
  horneados de la instancia (`FLPS2ColorStreams`, alineados a quadword) en el REF del stream de color en vez de los de
  la malla; solo lo Movable es un draw con luz (StaticLit: el ambiente del mapa, RGB, y un sol). VIF1 lee como
  VIFcodes las palabras del último quadword de un REF que su UNPACK no consume, y el relleno de esos colores es 0xff:
  los streams de 4 bytes por vértice se desempaquetan enteros (vértices redondeados a 4) o el juego se colgaba.
- Los lotes skinned de N21 (paleta de 24 huesos, 48 vértices) van a la VU1 tal como se guardan, con su paleta (N14b,
  abajo).
- La memoria que lee un frame en vuelo: el blob LPS2 que una malla suelta (destruida, reasignada o cargada encima) pasa
  por `FRHIDeferredRelease` (RHI), igual que los colores horneados de una instancia, que lo libera cuando termina el frame que se grababa; la PS2 numera sus frames
  (`BeginNextFrame`, `RetirePendingFrame`). En el escritorio se libera al momento. Test:
  `System.MeshUtilities.LPS2.ReleasedAfterTheFramesInFlight`. Las texturas cocinadas de N23, subidas en su sitio,
  siguen con `RetireInPlaceImages` (copia en la lista que se graba y espera al DMA, ahora el de VIF1).
- El scratch del emisor para los lotes va en la pila del frame (`TMemStackAllocator`, dentro del `FMemMark` de
  `Render`, como el de N17): sin memoria de heap por frame.
- Verificación: `VU1Conformance` (programa de PS2) pasa 30 lotes (sin luz y con luz, texturizados, espejados, fuera
  de la vista; 3 a 64 vértices) por la VU1 sin XGKICK, lee el paquete de la memoria de la VU1 y lo compara triángulo a
  triángulo con el emisor en el mismo ELF: `VU1Conformance: PASSED (30 batch(es), 0 failed)` en PCSX2, 318
  triángulos, diferencias máximas XY 0 (1/16 px), Z 5, RGBA 1, STQ 3 ulp; luego dibuja VU1 y emisor lado a lado,
  iguales. Tests: `System.GSCore.GifPacket.ChainBatches`, `System.GSCore.GifPacket.Chain` (sigue la cadena como el
  DMAC y VIF1), `System.Renderer.GS.Scene.VertexBatches` (541 del motor, 76 de ShooterGame, TestPAL 162).
- PS2 (PCSX2 2.8.2, Measure.ini 989212e8, el mismo build sobre N27 con y sin `-novu1`, la misma partida por el paso
  fijo): 15,3 fps frente a 11,1 (la fila N27: 10,8 fps, escena 66,7 ms); p50 66,8 ms frente a 83,5; escena 41,9 ms
  frente a 62,9 (GS Scene Render 39,92 frente a 59,61); Frame Chain 1,42 frente a 1,84 ms; 50,9 KB de escrituras del
  EE por frame frente a 176,1. `EngineMisc` llega a 2 862 KB de sus 3 072 (2 299 con `-novu1`). Sobre N18 solo
  (9dabf1b, sin personajes): escena 9,2 ms frente a 14,1, 29,85 fps frente a 29,56. ShooterGame se ve igual con y sin
  `-novu1` en PCSX2, la luz horneada incluida.

Desviaciones:
- Todo el frame va por VIF1 (DIRECT por PATH2) en vez de intercalar PATH3 y PATH1 con MSKPATH3: una sola cadena
  mantiene el orden y no hay dos canales que sincronizar. El GIF se resetea en `InitDisplay`: PCSX2 dejaba un paquete
  de PATH3 abierto antes del ELF y bloqueaba todos los DIRECT.
- Hasta N14b el EE posaba los lotes skinned y la VU1 hacía el resto; N14b lleva el skinning a la VU1. Tampoco van a la VU1 los draws con luz puntual o un segundo
  sol (los fogonazos del arma) ni la niebla (el renderer C++ no la tiene aún: N15).
- `GSH_Capture` (D3) no se hizo y `leonrun` no está construido en esta máquina: VU1Conformance cubre las cuentas y se
  corrió en PCSX2.
- Tolerancias medidas en vez de las de D2 a la letra: Z ±8 (1/2^21 del rango) y STQ ±8 ulp (el GS se queda con 16 bits
  de mantisa); lo observado es Z 4 y STQ 3. Un triángulo que un lado dibuja y el otro no debe tener menos de 1 px²
  (no hubo ninguno).
- Los `tris` de la VU1 cuentan los triángulos de los lotes antes de su culling (4 223 frente a 2 237).

Estado N14b (skinning en la VU1, hecho): `Skinned.vsm` (SkinnedLit y SkinnedUnlit, dvp-as como `VU1Programs.vsm`; se
sube tras los estáticos) lee el lote skinned de LPS2 v2 tal como se guarda: posiciones int16 con la escala y el bias de
la malla, normales int8, la piel (dos índices de paleta y dos pesos por vértice) y la paleta del lote (3 quadwords por
hueso, `FGSSkinMatrix`, hasta 24) por UNPACK V4-32 desde la memoria de la lista. Mezcla las matrices de los dos huesos
con sus pesos (w / 255), posa la posición y la normal, y sigue como StaticLit: transformación, luz (ambiente y un sol),
niebla y XYZF2 como en N15, CLIP y guard band (D8), doble búfer y XGKICK. Memoria de un búfer: cabecera 17, paleta 72,
cinco streams de 48 y el paquete: 474 de 504 quadwords.
- El EE solo construye la paleta (`MakeSkinPalette`: las skin matrices de la pose, identidad donde falta un hueso) y
  coloca el lote por la esfera en que la pose lo mantiene: la esfera del lote en la pose de bind movida por cada hueso
  de su paleta, con su escala, y la esfera que las envuelve; no posa ningún vértice para colocarlo. Los lotes que
  cruzan un plano siguen en el EE (el emisor posa con la misma paleta, `FGSPrimitiveEmitter::TransformVertexBatch`).
- D10: fuera `bQuantizedPose`, `AllocatePosedStreams`, `QuantizePose`, `SkinBatch` y `PosedPositions` /
  `PosedNormals`; queda `FGSCommandList::AllocateSkinPalette` (bloques de 16 KB que `Reset` conserva; `Append` copia
  cada paleta una vez a la lista del frame del RHI).
- Memoria: etiqueta LLM propia `RenderLists` (las listas de comandos del GS: escrituras, imágenes copiadas, draws,
  lotes y paletas; y las cadenas DMA del frame), presupuesto 4 096 KB en PS2Engine.ini (pico medido 2 978 KB en N28,
  3 046 con `-novu1`); `EngineMisc` vuelve a 2 048 KB (pico 519). La superposición de memoria y el resumen de
  MeasurePS2 la muestran (`RenderLists_peak_kb`). Superar un presupuesto duro sigue siendo fatal.
- Verificación: VU1Conformance pasa 66 lotes en PCSX2, 24 skinned (1 a 24 huesos girados, escalados y movidos; sin
  luz, con luz, texturizados, espejados y con niebla) contra la referencia C++: XY 0, Z 5, RGBA 1, STQ 4 ulp, F 0.
  Test: `System.Renderer.GS.Scene.SkinnedVertexBatches` (el comando expandido y el emisor, 0 píxeles distintos; las
  paletas en quadword y copiadas al hacer `Append`). 557 tests del motor, 94 de ShooterGame, TestPAL 164; BotMatch
  sin cambios.
- PS2 (PCSX2 2.8.2, el mismo build con y sin `-novu1`): sobre N15 (6307a50c, el mapa de bloques) 22,37 fps frente a
  11,42, escena 21,9 ms frente a 59,4 (la fila N15: 13,76 fps, escena 46,7 con los lotes posados de N14). Sobre N28
  (a9361261) 9,78 fps frente a 8,84, escena 73,4 ms frente a 82,1 (la fila N28: 8,76 fps, 83,4 ms): allí manda el
  mapa estático, cuyos lotes cruzando van al recortador del EE (275 KB de escrituras del GIF por frame).
- Desviación: el programa es `.vsm` a mano para dvp-as, no `.vcl` (no hay VCL en la cadena), igual que los estáticos.
  La paleta va con 3 quadwords por hueso (3x4) y la matriz mezclada se aplica a la posición y a la normal (la
  referencia C++ transforma con cada hueso y mezcla: la diferencia de redondeo cabe en D2).

### N15 · VU0, scratchpad y escena (L) — hecha

- VU0 en macro mode y SPR.
- Culling de skeletal y view models skinned.
- Blob shadows, niebla, HUD con SPRITE, LODs.
- Celdas y portales desde glTF.

Gate: VU0 frente a escalar ≤ 1 ulp y tests de culling.

Estado: hecha.
- VU0 en macro mode (`FVectorMath`, `Math/VectorMath.h`; en la PS2 `PS2VectorMath.cpp` con COP2 desde el EE: `LQC2`,
  `VMULA` / `VMADDA` en ACC, `VMINI`, `SQC2` / `QMFC2`; en el resto la referencia escalar `FVectorMathFPU`, las mismas
  sumas en el mismo orden): matriz × matriz, matriz × vector y la caja o la esfera contra planos de cuatro en cuatro
  (`FVectorPlaneSet`, que guarda `FFrustum`, con `IntersectsSphere` nuevo). `FMatrix::operator*` y `TransformFVector4`
  pasan por él, así que también los productos de la paleta de la pose (`GetSkinMatrices`,
  `FillUpComponentSpaceTransforms`) sin tocar el skinning; el culling por objeto usa el frustum de VU0.
- Scratchpad (`Misc/Scratchpad.h`: `FScratchpad`, `FScratchpadMark`, `TScratchpadAllocator`; los 16 KB del EE en
  `0x70000000`, `FPlatformMemory::GetOnChipScratchpad`; un búfer estático del mismo tamaño en el PC y con `-nospr`): las
  listas del frame del renderer (`FSceneRenderList`: los primitivos reunidos, las secciones opacas y translúcidas, los
  que proyectan sombra) y el scratch por lote del emisor. Lo que no cabe va a la pila del frame y se cuenta.
- Culling de skeletal: el proxy ya tenía los límites de la pose desde N21 (esferas del radio de cada hueso alrededor de
  los huesos posados, `USkeletalMesh::GetPoseBounds`, en el proxy y no en el camino de los lotes); N15 añade el culling
  del pase de view model (estáticos y skinned) contra el frustum de su propia proyección.
- Blob shadows: `UPrimitiveComponent::bCastBlobShadow` (los cuerpos de ShooterGame) traza hacia abajo contra los
  cuerpos WorldStatic cada vez que se manda el transform (`UpdateBlobShadowFloor`, la broadphase de N16; solo con escena,
  así que el botmatch headless no traza) y el renderer pone un cuadrado oscuro y suave en ese suelo
  (`FWorldEffectsGeometry::PlaceBlobShadow`: 1,2 veces el medio ancho de los límites, opacidad 0,6 que se apaga a 1,5 m
  del suelo), mezclado con la máscara de los efectos como las marcas de impacto: dos triángulos por personaje, nada
  nuevo del GS.
- Niebla: `AWorldSettings::FogSettings` (`FWorldFogSettings`: lineal en la profundidad de la vista de `StartDistance` a
  `EndDistance`, `FogInscatteringColor`, apagada por defecto) es la niebla del GS: FOGCOL una vez por frame y la F de
  cada vértice (`FGSVertexFog`) como XYZF2 con FGE, en el emisor y en StaticUnlit / StaticLit de la VU1 (la cabecera
  lleva la recta en el quadword 7, zw; Z pasa al bit 4 con la w de la escala de pantalla, 16; el ADC es 2048 más en el
  carril de F, la w de los límites). Con niebla: las mallas, las sombras, las marcas y los sprites del pase del mundo; sin
  ella: los tracers, las líneas, el view model y el canvas.
- Canvas con SPRITE: `FCanvas::GetPrimitives` da tiradas de rectángulos (tiles, líneas en un eje y cada barra de los
  glifos de stb_easy_font) y triángulos (líneas inclinadas); los rectángulos van como SPRITE de dos vértices. El canvas
  no tiene imágenes (el `UImage` de UMG es un tile).
- LODs: `UStaticMesh::SourceModels` (de UE; `ReductionSettings.PercentTriangles` y `ScreenSize`) se simplifica desde el
  LOD 0 al construir la malla (`IMeshBuilderModule::SimplifyMesh`, `FLPS2MeshBuilder::Simplify`, `meshopt_simplify` sin
  límite de error) y cada LOD es su blob LPS2 v2, guardado tras los triángulos de colisión; una malla de un LOD guarda los
  mismos bytes que antes (ningún paquete cambia). ImportList.ini: `LODs=<share>@<size>,...`. El renderer elige por el
  tamaño en pantalla (`ComputeStaticMeshLOD`, `ComputeBoundsScreenSize`, `SceneManagement.h`, los de UE) con
  `[/Script/Engine.RendererSettings] StaticMeshLODDistanceScale` (`r.StaticMeshLODDistanceScale` de UE).
- Celdas y portales: reglas del motor `VIS_` (`AVisibilityCellVolume`, `CellName`) y `PORTAL_` (`AVisibilityPortal`,
  `CellA` / `CellB` partidos donde los dos lados son celdas del mapa, `Corners` del rectángulo del quad). La escena los
  reúne en un `FVisibilityCellGraph` (RenderCore, 64 celdas) cuando se inicializan o se van, asigna cada primitivo a las
  celdas que tocan sus límites (los que se mueven, cada frame), y en cada frame recorre los portales desde la celda del
  ojo, estrechando el rectángulo de pantalla portal a portal (el quad recortado a lo que está delante del ojo; un portal a
  menos de 60 cm está abierto entero). Un mapa sin celdas o un ojo fuera de todas lo dibuja todo, como antes.
- Tests: `System.Core.Math.VectorMathVU0`, `System.Core.Memory.Scratchpad`, `System.RenderCore.VisibilityCells.Traversal`,
  `.Narrowing`, `.EdgeCases`, `System.RenderCore.Frustum.IntersectsAabb` (esferas), `System.Renderer.GS.Scene.LODByScreenSize`,
  `.Fog`, `.BlobShadows`, `.CellsAndPortals`, `.ViewModelCulling`, `System.Renderer.GS.Canvas.Sprites` (0 píxeles
  distintos de los triángulos, 282 vértices frente a 834), `System.MeshUtilities.LPS2.SimplifiedLODs`,
  `System.Engine.Assets.StaticMeshLODsRoundTrip`, `System.LeonEd.GLTFImport.StaticMeshLODs`,
  `System.LeonEd.MapFactory.CellsAndPortals` (fixture `CellsFixture.gltf` de `MakeCellsFixture.py`). 557 del motor, 94 de
  ShooterGame, TestPAL 164 en Win64 y 157 en PCSX2 (VU0: 2 unidades como mucho). `VU1Conformance: PASSED (42 batch(es),
  0 failed)` en PCSX2 (XY 0, Z 5, RGBA 1, STQ 4 ulp, F 0). BotMatch 10 7: "Botmatch OK: 7 round(s), CT 6 - T 1, 45
  kill(s), seed 7, sides switched after round 5", idéntico dos veces. `RunGates.bat -PS2` OK.
- PS2 (`MeasurePS2 -Label N15`, la misma partida sobre su base N30e 7ebec933): 11,52 → 13,76 fps, p50 83,5 → 66,8 ms,
  escena 55,5 → 46,7 ms (GS Scene Render 51,89 → 42,32), canvas 7,27 → 3,12 ms (GS Canvas 6,61 → 2,89: los SPRITE),
  `allocs_per_frame` 136,3 → 114,4. Frente a los 15,27 fps de N14 (sobre N27; N30a, N30b y N30e doblaron los
  triángulos del frame hasta unos 5 000): 13,76.

Desviaciones:
- Tolerancia de VU0 medida en lugar de 1 ulp: 2 unidades en el último lugar de la suma de las magnitudes de los
  productos. VU0 acumula en ACC y el FPU del EE redondea cada producto y cada suma; en PCSX2 se ven exactamente 2
  (16297,1787 frente a 16297,1816) y nunca más. Las pruebas de caja y esfera coinciden con la referencia salvo a menos de
  0,01 cm de un plano.
- El scratchpad no se nota en PCSX2 (46,68 ms con `-nospr` frente a 46,65): PCSX2 no emula la caché de datos del EE
  (su perfil no cuenta fallos de caché). La ganancia real solo se mediría en una consola.
- Los límites de la pose ya existían (N21) con radios por hueso, más ajustados que la esfera por malla que pedía la fase;
  se conservan. Las skeletal meshes no tienen LODs: el blob skinned tiene uno (lo toca N14b).
- Las LODs y las celdas están, pero ningún contenido las usa aún: de_leon no tiene nodos `VIS_` / `PORTAL_` (N28) ni
  mallas con `LODs=` (N29, que ajustará también la niebla; las armas de mundo son de N30a). El mapa de prueba es el
  fixture glTF y las escenas construidas en código.
- La niebla usa funciones del GS que ya tenían escena de conformidad (Fog, N8), y los SPRITE planos y las sombras con
  máscara también (StripsAndSprites, las marcas): ninguna escena nueva en GSConformance y ningún rebaseline de G8 (la
  niebla está apagada por defecto y el canvas da los mismos píxeles).
- Para N14b: la VU1 ahora escribe XYZF2 (Z desplazada 4 bits y F en el cuarto word); la escala de pantalla y los límites
  compartidos solo ganaron sus w (16 y 2048). Un programa Skinned nuevo debe escribir el mismo formato (el GIFtag del
  encoder dice XYZF2) o pedir XYZ2 para sí.
- `FCanvas::GetTriangles` y `FScene::GatherStaticMeshes` / `GatherSkeletalMeshes` se borran (D10) y van a
  `CheckBannedApis.ps1`.

### N16 · Broadphase de colisión (M) — hecha

- BVH estático cocinado (`UCX_`) y grid dinámico.

Gate: BVH igual a fuerza bruta en un test aleatorio, y BotMatch.

Estado: hecha.
- `FPhysSceneBroadphase`: los cuerpos que no se mueven van en un árbol AABB (`FAabbTree`, PhysicsCore: nodos e ítems
  en arrays planos, sin punteros), que se construye cuando se añaden cuerpos estáticos (una vez en todo el botmatch);
  los que se mueven (dinámicos, componentes movibles como la cápsula del personaje, y un estático la primera vez que se
  mueve), en una lista ordenada en X. Cada malla de triángulos tiene su propio árbol de triángulos.
- Trazas, barridos, solapes, `QuerySupportZ`, `ResolveCapsuleSides`, `ApplyCapsuleSweepPush` y el Step solo prueban lo
  que encuentra la broadphase, con los mismos impactos en el mismo orden que la fuerza bruta: los cuerpos conservan el
  orden en que se añadieron (su serie), aunque un borrado mueva el último a su hueco. Las `*Single*` no reservan memoria
  y podan lo que queda más allá del impacto más cercano; el Step ya no empareja estáticos. `FindComponentBody` busca por
  el id único (TMap) y el borrado es O(1).
- El botmatch da lo mismo (CT 8 - T 2, 78 bajas, razones [2,4,4,4,2,4,4,3,4,3]) y se repite idéntico. En Win64 tarda
  1,15 s en lugar de 1,55 s (mediana de 7 pasadas alternas).
- PS2 (`MeasurePS2`): el mundo baja de 8,1 a 5,9 ms por frame (−27 %; 9,0 ms en N1), medido contra el mismo árbol sin
  N16. La media sale 41,5 ms (24,1 fps) frente a 39,8 ms (25,1 fps), pero no es comparable: en PS2 el juego aún avanza
  con el tiempo del frame (el paso fijo es N18), así que un mundo más rápido juega otra partida (506 triángulos por
  frame en lugar de 378).
- Tests: `System.Engine.PhysScene.Broadphase.*` (6: trazas, contactos, Step, muchos cuerpos, componentes que se
  añaden, se mueven y se borran, y pares de personajes, todos contra una fuerza bruta que solo vive en el test) y
  `System.PhysicsCore.Triangle.MeshTreeMatchesEveryTriangle` (428 del motor, ShooterGame 56, TestPAL 137).

Desviaciones:
- El árbol estático se construye al cargar el mapa; cocinarlo con el nivel queda para N23 (el formato ya es plano).
  Nodos en float, sin cuantizar.
- Sin grid uniforme: los cuerpos que se mueven van en una lista ordenada en X (sort and sweep); son pocos.
- `ResolveCharacterOverlaps` saca los pares de un sort and sweep sobre las posiciones de los personajes, no de sus
  cuerpos, que las siguen al final.
- Un cuerpo de malla ocupa en la broadphase su caja y sus triángulos, que pueden salirse de ella; una traza de cápsula
  agranda la broadphase en radio + media altura en todos los ejes.
- Cuando un empujón (cápsula, Step o personajes) mueve algo más de un margen, los pares o cuerpos que quedan se buscan
  otra vez, para dar lo mismo que la fuerza bruta.
- El Step ya no pone a cero la velocidad de los cuerpos estáticos (nunca tienen).
- `SegmentTriangleMesh` construye el árbol de una malla la primera vez que la consulta (caché `mutable`), y los cuerpos
  editados con `GetBodies()` (tests) rehacen la broadphase en la consulta siguiente.

### N17 · Arenas de memoria (M) — hecha

- `FMallocPS2` con tags, `FMemStack`, arena por nivel, pools, presupuesto duro.

Gate: TestPAL y el pico en Budgets.md.

Estado: hecha. En PCSX2 (`MeasurePS2 -Label N17`, sobre N19): 29,9 fps, media 33,5 ms, p50/p95/p99 33,5 / 33,5 /
33,5 ms (N19: 28,8 fps y 34,7 ms, con otra partida). Pico de GMalloc 1 527 KB (1 412 KB al salir), 504 KB de los 2 MB
de la arena; de 8,3 a 10,4 asignaciones por frame durante una ronda (26,5 de media con los arranques de ronda; 46,8
antes de pasar a la pila del frame el scratch de las tiras de N12). TestPAL en el EE: 149 tests.
- `FMallocBinned` (el de UE) es GMalloc en el EE y en el PC: 64 clases de 16 en 16 bytes hasta 1 KB en páginas de
  4 KB de una arena reservada al arrancar (`FPlatformProperties::SmallBlockArenaSize`: 2 MB en el EE, 16 MB en Win64),
  O(1) y determinista, alineación de 16, 64 y 128 bytes; lo grande y lo que no cabe en la arena va al heap del sistema.
  Stats: actual, pico, vivos, asignaciones desde el arranque, páginas y cada clase. `FMallocAnsi` se va (G4).
- Tags de memoria (LLM de UE): `LLM_SCOPE(ELLMTag::X)` con 12 tags; cada bloque se carga a su tag (un byte por cada
  16 de la arena, o la cabecera). Presupuestos en `[Core.MemoryBudgets]` de PS2Engine.ini (KB por tag y `Total`): al
  90 % un aviso, por encima un error fatal que nombra el tag (los tests usan un hook). Fuera de Shipping.
- `FMemStack`, `FMemMark` y `TMemStackAllocator`: la pila del frame, vaciada por `FEngineLoop::Tick`, que comprueba
  que ni una marca ni un contenedor sobreviven al frame. La usan las consultas de colisión (Single y las cápsulas del
  personaje), la entrada, el canvas, las marcas de impacto y trazadoras y la búsqueda de caminos.
- Arena de carga: los bytes de un paquete van a `FLinkerLoad::GetLoadArena()` en vez del heap; se sueltan en cuanto
  sus exports están serializados y la arena devuelve sus chunks al acabar la carga más externa.
- `stat unit` muestra la RAM de GMalloc (actual / pico / presupuesto) y `stat memory` (F8) los tags; `-LogFrameTimes`
  añade `heap_kb`, `allocs_per_frame` y la línea `MemoryTags:`, que MeasurePS2 pasa a su CSV.
- Tests `System.Core.Memory.*` (10: clases, alineación, realloc, stats, desbordamiento de la arena, estrés con
  checksum, tags, presupuestos, marcas y contenedores de `FMemStack`) y la arena de carga en `Package.Budget`: 447 del
  motor, 63 de ShooterGame, TestPAL 156 en Win64 (sobre N19: 479 del motor).

Desviaciones:
- Un solo asignador en las dos plataformas (`FMallocBinned`, no un `FMallocPS2`): el PC asigna como el EE.
- Los UObjects siguen asignándose uno a uno (el GC los libera sueltos), así que la «arena por nivel» es la de los
  bytes de los paquetes durante la carga, no la de los objetos del mapa.
- El pico de GMalloc (1 527 KB) es mayor que el de N19 (1 332 KB), pero la partida es otra: en el EE el paso aún no es
  fijo (N18) y el coste del frame cambia la partida. En Win64 (paso fijo) la partida es idéntica (CT 4 - T 6, 75 bajas).
- La arena del EE se queda en 2 MB aunque el pico sea 504 KB: es el margen para el arte real.
- Con N16 las consultas Single ya no reservan: sobre su código solo quedan la sobrecarga de `CapsuleTraceMultiByChannel`
  en la pila del frame (las cápsulas del personaje) y las vistas de depuración; el scratch de `FGSPrimitiveEmitter`
  (N12) pasa a la pila del frame.

### N18 · Tick groups, timers, paso fijo y GC (L) — hecha

- `FTickFunction` por grupos, `FTimerManager`, D4, GC incremental.

Gate: BotMatch determinista en Win64 y leonrun.

Estado: hecha.
- Tick: `FTickFunction` con la API de UE (`PrimaryActorTick`, `PrimaryComponentTick`, `bCanEverTick`, `TickGroup`,
  `TickInterval`, `bStartWithTickEnabled`, `AddPrerequisite`, `SetActorTickEnabled`) y el `FTickTaskManager` del mundo:
  `TG_PrePhysics`, el paso de física, `TG_DuringPhysics`, `TG_PostPhysics`, las cámaras y `TG_PostUpdateWork`. Cada
  grupo guarda solo lo habilitado, en el orden del nivel y con los componentes de un actor justo antes que él, así que
  la geometría, las luces y los volúmenes no cuestan nada (como en UE, nada tickea sin `bCanEverTick`). El intervalo
  arrastra el resto; el pawn y sus componentes esperan a su controlador (`AddPawnTickDependency`). Solo con esto el
  botmatch dio la misma partida que antes (CT 4 - T 6, 75 bajas).
- Timers: `FTimerManager` (API de UE) del mundo, con un reloj entero de 3 MHz (1/30, 1/25 y 1/60 s son unidades
  enteras), que corre al principio de cada paso: lo que cambia un plazo lo ven los actores de ese paso, como cuando cada
  uno miraba su plazo en su tick. Pasan a timers `SetLifeSpan`, el volumen de daño, la vista del sensing (una mirada
  aplazada al paso siguiente enciende el tick del componente un paso), el pool de luces (timers sin delegado: nada se
  reserva por disparo), `ElapsedTime` (el `DefaultTimer` de UE) y, en ShooterGame, las fases de la ronda, el tiempo de
  compra, `mp_restartgame`, la bomba (explosión, desactivación, pitidos), la plantación, la mecha de la granada, sacar el arma y
  recargar, con los tiempos de CS. La cadencia de disparo se queda en el tick del arma (ya compensaba el resto del
  frame). Con los timers la partida cambió (CT 5 - T 5, 73 bajas): los plazos caen al principio del paso y no en el
  tick de su actor.
- Paso fijo (D4): `FFixedStepClock` convierte los microsegundos enteros de `Cycles64` en pasos de 1/30 s, con el resto
  exacto (unidades de 1/30 000 000 s), como mucho 4 por frame (lo que sobra se descarta y se cuenta: la guarda de la
  espiral de la muerte). El render dibuja entre los dos últimos pasos: los proxies guardan dos transformaciones y el
  viewport interpola la cámara del jugador en una cámara suya; nada vuelve a la simulación (un salto de más de 3 m se
  dibuja de golpe). PAL dibuja a 25 fps los mismos pasos de 30 Hz (1, 1, 1, 1, 2). Sin ventana se duerme hasta el paso;
  `-benchmark` da un paso por frame sin esperar; una captura, un paso por frame sin interpolar. Se borran el clamp de
  0,1 s, la ruta en `double` y `-tick=`. El tiempo del mundo cuenta en las unidades de los timers.
- GC: cada 10 s de juego empieza una recolección incremental que visita 100 objetos por paso (una cuenta, no un
  tiempo: avanza igual en cualquier máquina) con sus propias marcas (los weak pointers y los iteradores ven todos los
  objetos entretanto). Cada visita guarda los punteros que leyó (el valor, o una copia de la cabecera de un contenedor y
  de un array de punteros); el final, de una vez, vuelve a visitar los objetos cuyos punteros cambiaron, los objetos
  nuevos, las raíces y lo que reportan los `AddReferencedObjects`, y barre y purga. Una carga de nivel recoge entera, y
  ShooterGame pide una entera al empezar cada ronda (`ForceGarbageCollection`). Se va el GC completo cada 61 s.
- Win64 (sobre N25): `BotMatch 10 7` da `CT 8 - T 2, 70 kill(s), reasons [4,4,4,3,4,4,4,4,3,4]` y se repite
  idéntico; 2 rondas con ventana, a 30 fps en tiempo real e interpolando (y con el pose throttling de N25), dan la
  misma línea que sin ventana (`CT 2 - T 0, 13 kill(s), reasons [4,4]`).
- PS2 (`MeasurePS2 -Label N18`, dos veces, sobre N22 y N25): 29,65 fps, media 33,7 ms, p50/p95/p99 33,5 / 33,5 /
  50,3 ms, mundo 3,1 ms (los timers 0,1 a 1,1 ms, con la vista de los bots), escena 15,6 ms (762 triángulos: el suelo
  horneado de N22), pico de GMalloc 1 949 KB, 27,1 asignaciones por frame y 934 UObjects. Las dos pasadas, y los builds
  sobre N17, N22 y N25 (un cambio de render y el runtime de animación de por medio), juegan la misma partida
  (`CT 0 - T 2, 13 kill(s), reasons [3,3]`): desde N18 las filas de Budgets.md se comparan. El EE no juega la partida de
  Win64 (`CT 2 - T 0, 13 kill(s), reasons [4,4]`, 2 rondas): sus floats redondean distinto (determinismo por
  plataforma). Un marcado completo de 893 objetos cuesta 2,4 ms en el EE; el incremental los visita en 10 pasos y
  termina en 1,0 ms (purga 0,13 ms); el completo del inicio de ronda, 3,1 ms.
- Tests: `System.Engine.Tick.*` (6), `System.Engine.Timers.*` (5), `System.Engine.FixedStep.*` (2) y
  `System.CoreUObject.GarbageCollection.Incremental*` (3: igual que la recolección completa en grafos aleatorios,
  referencias movidas entre objetos visitados y sin visitar, objetos nuevos y destruidos entre porciones, weak
  pointers). 530 del motor, ShooterGame 64, TestPAL 159.

Desviaciones:
- Los timers corren al principio del paso (UE los corre tras `TG_PostPhysics`), para que un plazo se vea en el paso en
  que vence como antes; las cámaras se actualizan tras la física (antes, antes de ella), como en UE.
- El GC incremental no usa barreras de escritura (UE 5 las pone en `TObjectPtr`; los `UPROPERTY` de Leon son punteros
  crudos): comprueba al final lo que cambió. Un objeto que pasa a pending kill tras ser alcanzado vive hasta la
  siguiente recolección. El presupuesto es de objetos, no de 0,5 ms, por el determinismo.
- leonrun no estaba construido (Play! en Linux): la comparación con el EE es la partida de `MeasurePS2`.

### N19 · Audio en la SPU2 (M) — hecha

- `AudioCompressor` (VAG/ADPCM), voces de audsrv, música en streaming; en PS2 desaparece el mezclador software.

Gate: SNR del round trip, escucha y menos del 1 % del EE.

Estado: hecha.
- `AudioCompressor` (Developer): `FSpuAdpcmEncoder` hace el ADPCM de la SPU2 (bloques de 16 bytes y 28 muestras; en
  cada bloque prueba los 5 filtros y los 13 shifts contra la aritmética del decodificador y se queda con el de menor
  error cuadrático), mono, remuestreado con una sinc enventanada (Blackman) a `CompressionSampleRate` (22 050 Hz por
  defecto; nunca sube), con el bucle en un bloque (su primer bloque sin predicción) y los flags de inicio, repetición
  y fin. Determinista: la CRC de los bytes está fijada en un test. El decodificador (`FSpuAdpcm`) es de AudioMixer.
- SNR del round trip (22 050 Hz): seno de 440 Hz a media escala 56,2 dB, seno de 3 kHz a escala completa 32,1 dB, seno
  de 50 Hz flojo exacto, ruido blanco 23,5 dB, clics 25,6 dB, silencio exacto. Remuestreo 44,1 → 22,05 kHz: un tono
  de 1 kHz sale a 80,8 dB y uno de 15 kHz queda 57,4 dB por debajo; estéreo a 44,1 kHz → ADPCM a 22,05 kHz, 49,9 dB.
- Cook: Win64 y PS2 tienen el mismo formato de onda, `SPU2ADPCM` (D1). El paquete cocinado guarda el formato, la
  frecuencia, el inicio del bucle y los bloques como bulk data, sin el PCM; el cook escribe
  `<Platform>-SoundReport.txt` y falla si los sonidos de un mapa no caben en la SPU2 RAM. El juego de escritorio hace
  el mismo ADPCM de un sonido sin cocinar (Engine depende de AudioCompressor en Desktop, como el Renderer de
  TextureCompressor), así que Win64 reproduce el ADPCM decodificado.
- `FAudioDevice` lleva el modelo de la SPU2 en todas las plataformas y `FAudioHardware` lo ejecuta: buffers residentes
  desde la carga del sonido (compartidos por ruta, con referencias, en pila como los reparte audsrv desde 0x5010:
  2028 KB), 24 voces (2 para la música, las 4 últimas libres para prioridad mayor que 1: las de la bomba tienen 2),
  los plays se arrancan en su tick (ahora después del mundo), y el volumen y el pan de cada voz se calculan desde el
  listener con los 26 pasos de audsrv y solo se envían cuando cambian. En PS2: `audsrv_load_adpcm`,
  `audsrv_adpcm_set_volume_and_pan` y `audsrv_ch_play_adpcm`; en Win64, `FSoftwareAudioMixer` mezcla las mismas voces
  con el pitch de la SPU2 y los niveles de audsrv en miniaudio.
- PS2 (`MeasurePS2 -Label N19`): el audio pasa de 2,1-2,6 ms (3,2 en la fila de N16) a **0,05 ms** por frame (0,15 %);
  28,8 fps, media 34,7 ms, p50/p95/p99 33,5 / 50,3 / 50,3 ms (otra partida: no comparable con N16/N20). El log del EE
  muestra audsrv 1.04, `audsrv_adpcm_init()` y cada sonido `in SPU2 RAM` al cargarse (8 de 11, 48 KB), sin errores.
  Los sonidos cocinados pasan de 242 a 68 KB y el pak de PS2 a 292 KB (456 KB en N4).
- Tests: `System.AudioCompressor.SpuAdpcm.*` (4), `System.AudioMixer.SpuAdpcm.*` (3), `System.AudioMixer.Device.*`
  (4), `System.AudioMixer.SoftwareMixer.SpuPitch`, `System.LeonEd.Cook.SpuAdpcmSounds` (454 del motor, ShooterGame 62,
  TestPAL 143). El botmatch da lo mismo (CT 4 - T 6, 75 bajas) y se repite idéntico.
- **Falta escuchar** (el usuario, en PCSX2 y en Win64): disparos, recargas, el pitido, la plantación y la explosión de
  la bomba; que suenen como antes salvo algo menos de agudos (22 050 Hz), sin clics al empezar o acabar, que el pan
  siga a la cámara (un disparo a la derecha suena por el altavoz derecho) y que Win64 y PCSX2 suenen igual. El ADSR de
  las voces es el que deja libsd (audsrv no lo toca): si el ataque o la caída suenan mal, es ahí.

Desviaciones:
- Todo es mono: una voz de la SPU2 es mono y los sonidos espaciales son puntos. La música estéreo (dos voces) queda
  para cuando haya música.
- Sin música en streaming: audsrv no rellena un sample residente (una segunda carga del mismo id se ignora) ni encola
  ADPCM en una voz, así que hace falta un módulo del IOP propio que meta los bloques desde el disco en una región de la
  SPU2 con doble búfer (con la IO de N24). ShooterGame no tiene música: `PlayMusic` / `StopMusic` se quedan, con la
  música como un buffer residente en bucle en las voces de música.
- audsrv no puede apagar una voz: no se roba ninguna (un sonido sin voz libre se descarta, también en Win64) y parar la
  música la silencia; la voz sigue ocupada hasta el fin de su sonido.
- Se borran los tonos procedurales de la UI (PCM generado en runtime): una señal sin sonido en la config es muda.
- Las voces ocupadas se llevan con el reloj del dispositivo (la duración al pitch de la SPU2), sin RPC por frame; si la
  SPU2 aún no llegó al final de una voz, audsrv la rechaza y se prueba otra.
- El paquete sin cocinar sigue guardando el PCM (editor-only, el RawData de UE) con los ajustes; `ImportList.ini` pone
  `Priority=2` a los sonidos de la bomba.

### N20 · ShooterGame: rendimiento (M) — hecha

- Registros de actores, sensing filtrado a 5 Hz, pool de luces, cachés, HUD sin reformatear.

Gate: tiempo de juego en `MeasurePS2`.

Estado: hecha. El mundo baja de 9,0 a 3,9 ms por frame en PCSX2 (`MeasurePS2 -Label N20`: 27,5 fps, media 36,4 ms,
p50/p95/p99 33,5 / 50,3 / 50,3 ms; la línea base de N1 era 25,4 fps y 39,4 ms). En Win64, 36 000 frames de botmatch
headless pasan de 2,7 a 1,8 s.
- El game mode guarda registros en el orden del nivel: los trigger volumes y los player starts del mapa (se buscan una
  vez al crearse y se añaden al spawnear con `UWorld::AddOnActorSpawnedHandler`), y los pawns, los pickups, las bombas
  y las granadas (de su BeginPlay a su EndPlay). Los nombres de los sitios se ordenan una vez; `CountAlive` cuenta una
  vez por frame.
- Los bots solo trazan a enemigos vivos (`UShooterPawnSensingComponent` sobre `ShouldCheckVisibilityOf`,
  `ShouldCheckAudibilityOf`, `OnTimer` y `SetTimer`, los de UE, nuevos en el motor), y miran por turnos: cada uno a
  10 Hz, repartidos en el intervalo con los equipos intercalados, y como mucho `MaxSensingUpdatesPerFrame` (2) por
  frame; el resto espera en cola.
- Un ruido solo visita los componentes de sensing registrados; los fogonazos y las explosiones usan un pool de 8 luces;
  un sonido se reproduce desde sus muestras sin copiarlas (`USoundWave::LockPCM`, que sustituye a `GetPCMView`); las
  clases de armas se listan una vez; el HUD y el menú de compra solo formatean una línea cuando cambia lo que muestra.
- Tests nuevos: `ShooterGame.Registry.MapOnDeLeon`, `.PickupsAndPawns`, `ShooterGame.Bots.SensingFilter`,
  `.SensingStagger`, `ShooterGame.Effects.MuzzleFlashPool` y `ShooterGame.HUD.TextCache` (62 de ShooterGame, 421 del
  motor, TestPAL 137).

El botmatch con semilla 7 pasa de CT 8 - T 2 (78 bajas) a CT 4 - T 6 (75 bajas) y se repite idéntico. Con todos los bots
mirando en el mismo frame, como antes, el resto de cambios da exactamente el mismo resultado; lo único que cambia es
el momento de cada mirada dentro de los 0,1 s. Su pico de UObjects baja de 2 557 a 1 177.

Desviaciones:
- El sensing sigue a 10 Hz por bot y no a 5 Hz. Con 24 semillas, a 5 Hz los CT ganaban el 37 % de las rondas, frente al
  60 % de antes; a 10 Hz escalonado ganan el 52 %. Además, así la reacción no cambia (`ReactionTime` y las ventanas de
  «a la vista»).
- El tope de 2 por frame se cumple con una cola por turnos: a 60 fps no espera nadie, y a los 30 fps de la PS2 cada bot
  mira algo menos de 10 Hz, pero todos igual de a menudo.
- El pool de luces es del motor (`UWorld::AcquirePooledPointLight`), y lo usa `SpawnPointLightAtLocation`.
- Un mundo sin `AShooterGameMode` no tiene registros: en él nadie recoge armas.
- La escena del frame medido sube a 15,5 ms: el partido y el bot observado son otros, con 475 triángulos a la vista
  frente a 320. Eso es trabajo de N14.

### Hito 0.24.0: contenido real, animación y paridad CS

### N21 · glTF skeletal y animaciones; retirar FBX (L) — hecha

- Skins, `JOINTS_0`/`WEIGHTS_0`, canales TRS, imágenes embebidas.
- Cuaterniones smallest-three de 48 bits.
- Fuera todo FBX.

Gate: fixtures y G5.

Estado: hecha.
- Import (`UGLTFImportFactory`, `LoadSkeletalMeshFromGltf` / `LoadAnimSequencesFromGltf`): `-type=SkeletalMesh` hace
  `SK_` sobre `Skeleton=` o un `SKEL_` al lado; `-type=Animation`, un `A_<Animación>` por animación glTF, que guarda su
  `AnimationName` para el reimport. Los joints del skin van con el padre antes que los hijos (los nodos que no son
  joints entre medias se pliegan en la pose de referencia), `inverseBindMatrices`, `JOINTS_n`/`WEIGHTS_n` normalizados
  y reducidos a los 2 mayores, renormalizados a pasos de 1/255 (`FSkinWeightInfo`). Los `SOCKET_` bajo un joint son
  sockets del esqueleto. Canales TRS LINEAR y STEP muestreados a 30 Hz; CUBICSPLINE falla con un error claro; un
  esqueleto que no coincide (nombres, orden, padres) también. Las imágenes embebidas (buffer view de un `.glb` o URI
  `data:`) pasan a `T_`, también en los static meshes. `ImportList.ini` sigue igual (`Type=SkeletalMesh` /
  `Animation`, `Source=*.glb`).
- Formato de animación (`FCompressedAnimSequence`, AnimationCore, corre en el EE): pistas locales; rotación
  smallest-three de 48 bits (error < 0,006°), traslación int16 con escala y bias por pista (float si el rango rompe la
  tolerancia: más de 16 m a 0,05 cm), escala solo si no es 1; cada clave con su frame `uint16`: 8 bytes por clave de
  rotación o traslación, frente a 64 por hueso y frame de las matrices. Reducción de claves con cota de error sobre los
  valores ya cuantizados (`[/Script/Engine.AnimationSettings]`: 0,1°, 0,05 cm, 0,001): el clip de prueba de 3 huesos
  × 90 frames ocupa 2 094 bytes (17 280 antes). El muestreo da transformadas locales, las anim instances mezclan en
  espacio local (nlerp) y el componente construye las matrices de modelo una vez por update; los sockets van por
  índice de hueso sobre esa pose cacheada.
- Render: `USkeletalMesh` es un blob LPS2 v2 skinned (flag en la cabecera, paleta de ≤ 24 huesos y stream de skin por
  lote, 48 vértices por lote, lotes partidos por paleta) y el emisor C++ lo skinnea lote a lote; los skeletal meshes se
  descartan por los bounds de su pose.
- Fuera: `UFbxFactory`, el import FBX y OBJ de MeshUtilities, los módulos UFBX y TinyObjLoader (y su descarga en
  `Setup.bat`), la base Z-up, `Cube.obj` y los tests de FBX/OBJ, y la animación en matrices; a `CheckBannedApis.ps1`.
- Contenido: `VER_LEON_SKELETAL_LPS2_ANIM_TRACKS` (4) es la versión mínima cargable; se volvieron a guardar los 65
  paquetes (solo cambia el byte de versión) y un reimport da los mismos bytes.
- Tests: 489 del motor, ShooterGame 63, TestPAL 156. Fixtures `Cube.glb` y `SkinnedArm.glb` de
  `MakeSkinnedFixture.py` (Python estándar, los mismos bytes cada vez).

Desviaciones:
- OBJ también se borra: solo lo usaban los tests; la identidad del import pasa a `Cube.glb`.
- Un `STEP` cae en el frame de 30 Hz igual o siguiente: el salto dura un frame (33 ms).
- Las animaciones de nodos que no son huesos por encima del esqueleto se toman en reposo.
- Las texturas embebidas no tienen import data: las hace el import de su malla (como las de un mapa).
- El skinning sigue en el EE (C++); VU1 llega en N14. La mezcla de poses es nlerp, no slerp.

### N22 · Iluminación horneada por vértice (M) — hecha

- `KHR_lights_punctual`, ambiente, AO y sombras contra el BVH, en COLOR_0.

Gate: doble cook idéntico.

Estado: hecha.
- Horneado (`FStaticLightingSystem`, LeonEd; UE: Lightmass): para cada vértice de cada static mesh Static del nivel,
  el cielo de `AWorldSettings::LightmassSettings` (`FLightmassWorldInfoSettings` de UE: `EnvironmentColor` ×
  `EnvironmentIntensity`, azul claro × 0,35) por la parte del hemisferio, con peso coseno, que no tapa ninguna geometría
  a menos de `MaxOcclusionDistance` (300 cm): 64 direcciones estratificadas de una semilla fija, las mismas en cada
  vértice; y cada luz no Movable (las de `KHR_lights_punctual` importadas; todas proyectan sombra) con Lambert y la
  atenuación de rango al cuadrado, salvo que un rayo hacia ella choque antes con la geometría estática. Los oclusores
  son los triángulos de colisión de los static meshes Static visibles en un `FAabbTree` (PhysicsCore, el de N16). Un
  hilo y un orden fijo: el mismo mapa da los mismos bytes.
- Datos: cada componente guarda sus propios streams de color para los lotes LPS2 v2 de su malla
  (`UStaticMeshComponent::BakedVertexColors`, `FLPS2ColorStreams`; UE: `OverrideVertexColors`), con la misma forma que
  el stream de color de la malla (RGBA8, 255 = 1, alineado a quadword) para que la VU1 lo desempaquete por referencia en
  su lugar, y el CRC del blob para el que se hicieron. Van en el mapa detrás del transform
  (`VER_LEON_BAKED_VERTEX_COLORS`, la versión mínima cargable: se volvieron a guardar los 65 paquetes). de_leon: 1 562
  vértices, 6,3 KB.
- Cuándo: el import del mapa hornea como último paso, `ResavePackages -buildlighting` (el switch de UE) vuelve a hornear
  un mapa sin fuente (Entry, Template_Default) y el cook avisa de un mapa cuya luz hay que reconstruir. de_leon: 35
  mallas, 1 070 triángulos, 101 098 rayos en menos de 0,1 s.
- Runtime: un static mesh Static dibuja sus colores horneados por el albedo sin ninguna cuenta de luz por frame (sin
  horneado, los colores de la malla: plano); lo Movable (los cuerpos de los pawns, que pasan a Movable, las armas, los
  proyectiles, la bomba) sigue con la luz por vértice, con el cielo del mapa como ambiente en lugar del 0,10 del albedo.
  Fuera la luz por frame de los static meshes.
- de_leon: el suelo pasa a una rejilla de 20 × 16 celdas de 3 m (`make_de_leon.py`), para que la luz por vértice tenga
  dónde poner las sombras y la oclusión de muros y cajas. Un segundo reimport de todo el contenido da los mismos bytes.
- Tests: `System.LeonEd.StaticLighting.ShadowsAndOcclusion` (plano, caja y sol: la sombra más oscura que el suelo al
  sol, el pie de la caja ocluido), `.Deterministic`, `System.LeonEd.MapFactory.BakesStaticLighting` (import, guardado y
  carga) y `System.Renderer.GS.Scene.StaticLighting`; SceneFrame dibuja un suelo horneado y cubos Movable (29 píxeles más
  allá de un paso de 5 bits; la referencia y el emulador ven los mismos colores). 483 del motor, 63 de ShooterGame,
  TestPAL 156. El botmatch no cambia (CT 4 - T 6, 75 bajas) y se repite idéntico.
- PS2 (`MeasurePS2 -Label N22`): 29,8 fps, media 33,6 ms, p50/p95/p99 33,5 / 33,5 / 40,8 ms; escena 15,9 ms con 794
  triángulos, 20,0 µs por triángulo frente a los 27,7 de N17 (12,0 ms con 432, otra partida): el suelo en rejilla duplica
  los triángulos y los static meshes ya no cuestan luz. Con celdas de 2 m eran 874 triángulos, 16,4 ms y 26,1 fps; con
  3 m el frame se queda en 30 fps. En la captura de PCSX2 se ven los muros con su oclusión al pie, las caras de las cajas
  en sombra más oscuras y la sombra de los muros en el suelo.

Desviaciones:
- Los colores no van en el COLOR_0 de la malla sino en cada instancia: las instancias de una malla (todo de_leon son
  cubos compartidos) reciben luz distinta. El stream de la malla sigue blanco.
- Sin sonda de luz para lo que se mueve: los pawns toman el cielo sin oclusión y las luces sin sombra; una rejilla de
  sondas horneada queda para más adelante. Las luces Movable (los fogonazos) ya no iluminan el mundo estático.
- Todo se satura en 1 (255): el suelo al sol con el cielo pasa de 1 y se queda en el albedo.

### N23 · Cook de PS2 e ISO (L) — hecha

- Mips, blobs swizzleados, presupuestos duros, pak en orden de carga, cook incremental, ISO con SYSTEM.CNF.

Gate: la ISO arranca y un presupuesto excedido hace fallar el cook.

Estado: hecha.
- Texturas load-in-place: los datos de una textura paletizada son la imagen de la CLUT tal como la lee el GS (CSM1,
  alfa en 0..0x80, `FGSTextureLayout::MakeClutImage` en el cook) y, detrás, los índices de cada nivel como el payload
  exacto de la transferencia IMAGE; todo en quadwords enteros y en memoria alineado a 128 bytes
  (`FByteBulkData::SetPayloadAlignment`). La caché de texturas los sube con `FGSCommandList::UploadImageInPlace`: la
  cadena DMA de N11 hace REF a los bytes de la propia textura, sin conversión ni copia. Se borran la reordenación y el
  escalado de la paleta en cada subida y la copia de cada nivel en la lista (`GetImageData`, en `CheckBannedApis.ps1`).
  Si una textura residente se libera mientras una lista la tiene, `FPS2RHI::RetireInPlaceImages` copia las imágenes del
  frame que se graba y espera al DMA del que se envía. Win64 cocina y convierte lo mismo, y el emulador lee esos bytes.
- Se quita lo que el target no lee: los ini staged sin comentarios ni secciones del editor o del cook, y la malla que
  colisiona con sus formas simples sin sus triángulos de colisión. El cook de PS2 de ShooterGame pasó de 245 409 a
  232 480 bytes con el contenido de N17 (264 422 con el de N22 a N30d).
- Presupuestos duros (`[/Script/LeonEd.CookSettings]`, `FCookBudgets`, sobreescribibles con `-<Clave>=`): por mapa la
  VRAM (el arena, 1 856 KB), la RAM estimada (1 536 KB + 2 x los bytes cocinados, contra `[Core.MemoryBudgets] Total`)
  y la SPU2 RAM; por malla triángulos (4 096) y huesos (64); por textura lado (256) y bits por téxel (8). Pasarse es un
  error que nombra el asset, la cifra y la clave, y el cook falla (`System.LeonEd.Cook.Budgets`).
- Orden del pak: `-LogFileOpenOrder` (`FPlatformFileOpenLog`) apunta cada fichero la primera vez que se abre, con su
  ruta staged (Win64: `Saved/Logs/FileOpenOrder-<Platform>.txt`; PS2: las líneas `LogFileOpenOrder:` del log del EE), y
  `LeonPak -order=` / `BuildCookRun -pakorder=` coloca esas entradas primero; el índice no cambia.
- Cook incremental (`-iterate`, por defecto; `-full`): `<Project>/Intermediate/CookCache/<Platform>/`, con la clave
  SHA-1 del paquete y de lo que alcanzan sus imports duros, la versión del cooker y los ajustes de la plataforma. 2,4 s
  completo, 0,12 s desde la caché, los mismos bytes (`System.LeonEd.Cook.Cache`). Cubre la luz horneada de N22: los
  colores van en el paquete del mapa (sus luces también) y la malla para la que se hornearon es uno de sus imports, así
  que cambiar una luz o una malla vuelve a cocinar el mapa; un mapa con la luz por reconstruir repite el aviso desde la
  caché. Los presupuestos por malla cuentan también las skeletal meshes (triángulos y huesos del esqueleto).
- ISO: la imagen de Docker gana xorriso; `BuildCookRun -platform=PS2 -iso` hace `ShooterGame.iso` (5 056 512 bytes,
  determinista) con SYSTEM.CNF (`BOOT2 = cdrom0:\SLUS_990.01;1`, `VER`, `VMODE` según `-region`), el ELF como
  `SLUS_990.01`, el pak justo detrás y luego `AUDSRV.IRX` y `LEONCOMM.TXT`, con nombres ISO 9660 8.3
  (`FPaths::ToIso9660Path`, que la capa de ficheros de PS2 aplica a `cdrom0:`: mayúsculas, 8.3, `;1`).
- Arranque en PCSX2 (`MeasurePS2 -Iso`): la BIOS carga `cdrom0:\SLUS_990.01;1`, el juego monta
  `cdrom0:/ShooterGame/Content/Paks/ShooterGame-PS2.lpak` y juega el botmatch a 30 fps (29,1 fps de media, con la luz horneada de N22). Carga
  (`First frame after`, reloj del EE): 1,42 s desde el arranque del motor con el pak en orden de rutas, 0,90 s con el
  orden de apertura (`LoadMap` 0,74 y 0,41 s; desde `host:` 0,08 s).
- Tests: `System.LeonEd.Cook.Budgets`, `.Cache`, `.StripConfig`, `System.PakFile.Format.OpenOrder`,
  `System.Core.HAL.PlatformFileOpenLog` y los de las texturas en su sitio (535 del motor, 70 de ShooterGame, TestPAL
  161). El botmatch da lo mismo que N30d (7 rondas, CT 1 - T 6, 47 bajas) y se repite idéntico.

Desviaciones:
- Los niveles no van pre-swizzleados: con transferencias IMAGE el GS los swizzlea al recibirlos y el EE no hace nada;
  swizzlear en el cook solo movería trabajo del GS.
- Los mips ya estaban desde N13.
- La estimación de RAM es una regla (base + 2 x bytes cocinados); la medida sigue siendo GMalloc en el EE.
- La clave del cook no incluye el ejecutable del cooker: un cambio que altere la salida debe subir
  `UCookCommandlet::CookerVersion`.
- El peor frame desde el disco (668 y 384 ms) es la primera lectura de los sonidos al empezar la ronda: la IO asíncrona
  es N24.
- Sin logo de licencia ni sectores de sistema: la ISO arranca en PCSX2, no en una consola sin modchip.

### N24 · CDVD asíncrono, tarjeta de memoria y pad (M) — hecha

- fileXio/libcdvd, `LoadPackageAsync`, libmc, dos puertos, presión y vibración.

Gate: carga medida y round trip de guardado.

Estado: hecha.
- IO asíncrona (API de UE): `IAsyncReadFileHandle` / `IAsyncReadRequest` (`SizeRequest`, `ReadRequest` con prioridad,
  `PollCompletion`, `WaitCompletion`, `Cancel`, `GetReadResults`) desde `IPlatformFile::OpenAsyncRead`; la cola es
  `FAsyncIOSystem` (Core): la prioridad más alta primero, en orden de petición dentro de una, en trozos de 64 KB, y los
  callbacks en el hilo de juego. En el EE la lee un hilo del kernel (`PS2AsyncIO`) una prioridad por encima del juego,
  con semáforos, que duerme en cada RPC del IOP mientras el juego sigue; no reserva ni escribe al log. Win64 no tiene
  hilo: el hilo de juego lee la cola en su orden (en `Tick` y en las esperas), así que una partida es la misma. El pak
  lee sus entradas por un segundo handle del `.lpak` (el del hilo de juego nunca se mueve) y `FPlatformFileOpenLog`
  apunta también las aperturas asíncronas.
- `LoadPackageAsync` / `ProcessAsyncLoading` / `FlushAsyncLoading` / `CancelAsyncLoading` / `IsAsyncLoading` /
  `GetNumAsyncPackages` (CoreUObject): los bytes de un paquete y de los que importa (`GetImportedPackageNames`) llegan
  por la IO asíncrona, y el hilo de juego serializa cada paquete cuando su cierre está en memoria, con el cargador
  síncrono (los mismos objetos y el mismo `PostLoad`); un `LoadPackage` de un paquete en camino toma sus bytes (cada
  fichero se lee una vez; como mucho 384 KB leídos por delante, las importaciones primero: sin ese tope la precarga
  pasaba el presupuesto de `LoadMapMisc`). `UGameEngine::Tick` lo procesa antes del mundo (`AsyncLoadingTimeLimit`:
  8 ms en PS2), y `UEngine::LoadMap` hace flush tras `BeginPlay`: ShooterGame pide en `InitGame` los soft paths de sus
  armas, proyectiles, peón, bomba y controlador, también dentro de structs y arrays (101 paquetes: sonidos, pasos e
  impactos de cada superficie, la radio, animaciones, mallas skinned; `PreloadedAssets` los retiene) y llegan con el
  mapa.
- Disco (`MeasurePS2 -Iso`, ahora con `-LogFileOpenOrder` y `-PakOrder`; sobre el contenido de N30f): antes de N24 el
  frame en que empieza la partida leía 126 ficheros, 5 622,8 ms (3 053,6 con el pak ordenado); con N24 ninguno se
  abre tras el primer frame y el peor frame después del primero es de 435,0 ms (el arranque de la partida: diez peones
  aparecen con sus armas). El primer frame llega a 7,79 s (2,39 antes) con el pak ordenado y a 10,77 s (3,34) sin
  ordenar: la carga se lleva lo que se leía jugando, y más (todas las armas y superficies). `LoadMapMisc` llega a
  448 KB de 1 024. Filas en Budgets.md.
- Tarjeta de memoria: `USaveGame` y `UGameplayStatics::CreateSaveGameObject` / `SaveGameToMemory` /
  `LoadGameFromMemory` / `SaveGameToSlot` / `LoadGameFromSlot` / `DoesSaveGameExist` / `DeleteGameInSlot` (cabecera
  GVAS de UE y las propiedades con tags), sobre el `ISaveGameSystem` de `IPlatformFeaturesModule`: Win64
  `FGenericSaveGameSystem` (`Saved/SaveGames/<Slot>.sav`), PS2 `FMemoryCardSaveGameSystem` (neutral, probado con una
  tarjeta en memoria) sobre libmc en `mc0:` (MCMAN/MCSERV de la ROM): la carpeta del juego con `icon.sys` (título en
  Shift-JIS) y un icono (un cuadro texturizado de 128 × 128), cabecera con CRC, y el porqué de cada fallo
  (`ESaveGameResult`: sin tarjeta, sin formatear, llena, retirada, dañada). ShooterGame guarda sus opciones en el slot
  `Settings` (`UShooterPersistentUser`: sensibilidad, Y invertida, volumen y color de la mira) con `SetSensitivity`,
  `SetInvertY`, `SetVolume` y `SetCrosshairColor`, y las carga al jugar con pantalla. En PCSX2 la tarjeta propia de la
  carpeta de medida (`Measure.ini`) se crea, libmc arranca y la partida la usa.
- Pad: `IInputInterface` con ids de controlador (dos en PS2 y en Win64); el segundo DualShock es el controlador 1 (el
  viewport lo manda al jugador local de ese id; ShooterGame tiene uno). Presión: `padEnterPressMode` y los bytes como
  ejes (`Gamepad_LeftTriggerAxis` / `Gamepad_RightTriggerAxis` de UE y los `Gamepad_*Axis` de Leon;
  `FDualShockPressure`). Vibración: `SetForceFeedbackChannelValue(s)` de UE, `UForceFeedbackEffect` y
  `APlayerController::ClientPlayForceFeedback`; en PS2 `padSetActAlign` y `padSetActDirect` (`FDualShockForceFeedback`,
  `FDualShockActuators`), parada al retirar el pad y al salir. `FDualShockConnection` pide, un comando por frame, el
  modo analógico, los motores y la presión, y todo otra vez tras reconectar (PCSX2 lo muestra: `VS: Normal`,
  `RB: D+A+P`). ShooterGame vibra al disparar (motor pequeño), al recibir daño y con una explosión cerca (grande).
- Tests (12 nuevos): `System.Core.AsyncIO.Order` / `.Completion` / `.Cancel`, `System.PakFile.PlatformFile.AsyncRead`,
  `System.CoreUObject.AsyncLoading.Delegates` / `.Files`, `System.Engine.SaveGame.RoundTrip` / `.MemoryCard`,
  `System.ApplicationCore.DualShock.ForceFeedback` / `.Pressure` (y `.Reconnect` ampliado),
  `System.Engine.ForceFeedback.PlayerController` y `ShooterGame.Settings.RoundTrip`; 572 del motor, 99 de ShooterGame y TestPAL 170 en
  Win64. BotMatch:
  `Botmatch OK: 10 round(s), CT 5 - T 5, 65 kill(s), seed 7, sides switched after round 5` y se repite idéntico.

Desviaciones:
- No hay fileXio ni `sceCdRead`: el hilo de IO lee por la FILEIO de la ROM (newlib), que ya bloquea cada llamada; lo
  que cuesta es el disco, y ya no lo paga ningún frame. Pasar a fileXio o libcdvd sigue abierto (PS2SDK.md).
- Dos llamadas al IOP desde los dos hilos a la vez tumbaron PCSX2 una vez (`_request_end` de sifrpc con el cliente nulo
  mientras cargaban MCMAN/MCSERV): `FPS2PlatformMisc::LockIop` serializa ahora las lecturas, la carga de módulos y las
  llamadas de libmc, y la tarjeta arranca con el motor, antes del hilo de IO.
- Los callbacks de lectura y los delegates de carga corren en el hilo de juego (en UE, en el hilo que acaba); un paquete
  se serializa entero (sin event driven loader ni troceo dentro del paquete).
- La precarga es de ShooterGame (`RequestGameplayAssets`) sobre `LoadPackageAsync`, no un `FStreamableManager`.
- Sin música en streaming: ShooterGame no tiene música y audsrv no rellena una voz (N19); la IO asíncrona queda lista
  para el módulo del IOP que haga falta.
- El objetivo de ningún frame por encima de 50 ms tras el primero no se cumple por el render: con el contenido de
  N30f cada frame del EE dura unos 117 ms. Lo que es del disco sí: ningún frame lee.
- La carga crece más de lo que ahorra la partida (5,4 s más antes del primer frame por 3,1 s menos en el arranque de la
  ronda, pak ordenado): se precarga todo lo que la partida puede pedir, no solo lo de la primera ronda.
- Win64 no vibra: su entrada es GLFW, sin motores; `FGLFWInputInterface` guarda los valores (tests).
- `UForceFeedbackEffect` usa una recta por canal (`StartIntensity` a `EndIntensity`), no un `FRuntimeFloatCurve`, y los
  efectos de ShooterGame son subobjetos del controlador, no assets.
- La tarjeta se usa bloqueando (`mcSync(0)`): se guarda al cambiar una opción, no en cada frame.

### N24b · Carga del disco más rápida y el pico del arranque de ronda (M) — hecha

- Averiguar por qué la carga asíncrona tarda 3 veces lo que las lecturas síncronas y arreglarlo (lecturas grandes y
  en orden, peticiones juntas, prioridad y esperas del hilo de IO); repartir el trabajo del arranque de ronda; vibración
  en Win64 con XInput.

Estado: hecha.
- La causa: cada lectura del disco emulado por FILEIO cuesta unos 20 ms antes de sus bytes (las RPC de `lseek` y
  `read`, la búsqueda de la unidad) y 0,7 ms por KB, y N24 leía la precarga de pocos KB en pocos KB (187 lecturas de
  943 KB en 4,6 s). Ahora el hilo de IO toma la lectura en cola más cercana por delante de la última (el barrido de UE)
  y junta las cercanas en una (`FAsyncIOSystem::CoalesceBytes`, 128 KB, huecos de hasta 16 KB); el handle del pak del
  hilo de juego lee bloques de 64 KB hacia delante (`FPakBlockCacheHandle`); un handle del PS2 no hace el `lseek` que
  no hace falta; el mapa se carga por la misma cola (`UEngine::LoadMap`: `LoadPackageAsync` y flush); una carga
  síncrona de un paquete en camino sube su lectura al frente (`FAsyncIOSystem::RaisePriority`), o la empieza si
  esperaba sitio. El tope de 640 KB leídos por delante sigue (sin él `LoadMapMisc` pasa de 1 MB).
- Medidas: `FlushAsyncLoading:` y `The paks until the first frame` en el log (lecturas, KB, ms, bloques), y
  `-LogFrameTimes` escribe los scopes de un frame que dobla el anterior (`Frame spike:`), con nuevos scopes de spawn,
  construcción, `BeginPlay`, registro de componentes, `Possess` y mallas skinned.
- Disco (`MeasurePS2 -Iso`, contenido de N29, pak en su orden de apertura): el primer frame pasa de 7,72 s a 2,93 s
  (`LoadMap` de 7,06 a 2,38 s) y el peor frame tras el primero de 384,95 a 50,05 ms (tres campos); ningún fichero se
  abre tras el primer frame. Antes de N24 era 2,39 s leyendo 126 ficheros en el arranque de la partida; los 0,5 s de
  más son las armas de todos, que se leen antes del primer frame.
- El pico del arranque de ronda: 320 ms en los `DoesPackageExist` de cada ruta de config de cada peón y arma;
  `LoadShooterObject` (ShooterGame) resuelve lo que está en memoria con una búsqueda y guarda lo resuelto (las tres
  copias de `LoadOptionalAsset` se van, D10). Las asignaciones por frame bajan de 67,1 a 11,8.
- Win64 vibra: `FXInputForceFeedback` (ApplicationCore, junto a la entrada de GLFW) carga `xinput1_4.dll` (o
  `xinput9_1_0.dll`) en tiempo de ejecución y manda los canales grandes al motor izquierdo y los pequeños al derecho
  con `XInputSetState`, solo cuando cambian, y los para al acabar.
- Tests (3 nuevos): `System.Core.AsyncIO.Coalesce`, `System.ApplicationCore.Windows.XInputForceFeedback` y `.Order`
  con el barrido; 575 del motor, 99 de ShooterGame y TestPAL 171 en Win64. BotMatch:
  `Botmatch OK: 9 round(s), CT 3 - T 6, 55 kill(s), seed 7, sides switched after round 5` y se repite idéntico.

Desviaciones:
- El primer frame queda 0,5 s por encima de la carga síncrona de antes de N24: se precarga todo lo que la partida puede
  pedir (el enfoque de UE para un mapa pequeño). Probé precargar solo la primera ronda y leer el resto en la primera
  congelación: el primer frame llegaba a 2,09 s, pero serializar el resto daba frames de 100 a 236 ms; se queda la
  precarga entera.
- Sin fileXio ni `sceCdRead`: la lectura sigue por FILEIO, más grande y en orden.
- XInput no dice qué pad de GLFW es cuál: el controlador N es el N-ésimo pad de XInput (GLFW lista los de XInput
  primero); un DualShock por DirectInput junto a un pad de Xbox puede recibir la vibración del otro.
### N25 · Runtime de animación (L) — hecha

- Pose cache, blend spaces, notifies, montages, capas, aim offset, brazos skinned.

Gate: tests de AnimationCore.

Estado: hecha.
- Pose: `USkeletalMeshComponent` evalúa como mucho una vez por update, con los temporales en la pila del frame
  (`FAnimationRuntime` trabaja sobre vistas), y solo reenvía las skin matrices tras una evaluación nueva. El anim
  instance se actualiza siempre (montages, notifies y la máquina de estados siguen a tiempo), pero la evaluación se
  salta: `AlwaysTickPose` si la malla no se dibujó en 0,2 s (`LastRenderTime`, que pone el renderer) y la URO de UE
  (`bEnableUpdateRateOptimizations`) cada 1 a 4 frames según la distancia a la vista más cercana
  (`ViewLocationsRenderedLastFrame`, pasos de 15 m, escalonado por id). Se mide con `GetNumPoseEvaluations`.
- Blend spaces: `UBlendSpace` 2D (Delaunay sobre los ejes normalizados, baricéntricas dentro, la arista más cercana
  fuera) y `UBlendSpace1D`, los dos por `GetSamplesFromBlendInput` (tres muestras como mucho, sin reservar) y
  sincronizados por tiempo normalizado. Se crean desde `ImportList.ini` sin `Source` (`Type=BlendSpace`,
  `BlendSpace1D`, `AimOffsetBlendSpace1D`, `AnimMontage`; `+Sample=A_Idle,0,0`), con los mismos bytes cada vez.
- Notifies: en los `extras` de la animación glTF (`{"notifies": [{"name", "time"}]}`; Blender los exporta desde una
  propiedad de la action que un `make_*.py` rellena con sus pose markers). Uno por cruce, también en el wrap y varios en
  un update; ninguno con delta 0. Van a `UAnimNotify::Notify` y a `UAnimInstance::OnAnimNotify`, que enlaza el actor.
- Montages (`UAnimMontage`, subconjunto de UE): un slot, un clip, secciones, blend in/out, play rate, `OnMontageEnded`.
  `DefaultSlot` mueve todo el cuerpo; `UpperBody`, desde el hueso rama (blend por hueso), así que las piernas siguen con
  la locomoción. Aim offset aditivo (`UAimOffsetBlendSpace1D`, de −90 a 90) sobre la misma rama.
- View models skinned (brazos en primera persona) en la pasada de view model, solo para su dueño.
- ShooterGame: los caminos están (cuerpo skinned con `UCharacterAnimInstance`, `Mesh1P`, el arma en el socket `Grip`,
  los montages de disparo, recarga, desenfunde, plantar y desactivar, pasos y cargador) detrás de nombres de config
  vacíos hasta el arte de N27; sin ellos todo sigue como antes. `ShooterGame.Animation.SkinnedPawn` los prueba con el
  personaje de test del motor.
- Medida: 10 personajes de 32 huesos con todo el grafo (locomoción 2D de 3 clips, montage de torso, aim offset de 5
  poses), ~70 µs por frame en Win64 (7 µs cada uno); en el EE, ~80 veces más lento (N20), ~0,5 ms por personaje: con
  el throttling, 1,5-2,5 ms en una ronda de 10 bots. `MeasurePS2` lo medirá con el arte de N27.
- Tests: 21 nuevos del motor, 1 de ShooterGame. El botmatch da lo mismo (CT 4 - T 6, 75 bajas) y se repite idéntico.

Desviaciones:
- Los notifies llegan al actor por un delegate (`OnAnimNotify`), no por funciones `AnimNotify_<Name>` (no hay
  blueprints).
- El blend space 2D interpola triángulos en vez de hornear una rejilla como UE 4.27.
- El aim offset guarda poses completas y calcula el aditivo en runtime contra `BasePose` (UE importa clips aditivos).
- Sin estadísticas del EE: la cifra de PS2 es una estimación hasta que haya personajes animados en ShooterGame.

### N26 · Blender MCP y especificación de arte (S) — hecha

- `Docs/ART_PIPELINE.md` con presupuestos, nombres, animaciones, exportador y la regla CC0.

Gate: un asset de MCP convertido en `make_*.py`, byte a byte igual dos veces.

Estado: hecha.
- [ART_PIPELINE.md](../ART_PIPELINE.md): presupuestos por clase de asset (personaje 600–1 000 triángulos, brazos
  400–600, arma 100–300 en el mundo y 300–600 en primera persona, props hasta 300, 1 500 por celda), 2 pesos y ≤ 24
  huesos por paleta, texturas P4 / P8 de 64–128 px y la densidad de téxeles; los nombres (`SOCKET_Weapon_R`, `UCX_`,
  `VIS_` / `PORTAL_`, luces); el esqueleto común de CT y T (23 huesos con los nombres del maniquí de UE) y el de los
  brazos (11); las animaciones de tercera y primera persona a 30 fps, con su loop y sus notifies; las opciones fijas del
  exportador glTF; cómo se hacen las texturas; la regla CC0 y el flujo MCP → `make_*.py`.
- `Game/ShooterGame/SourceArt/leon_art.py`, las ayudas de Blender compartidas: la escena vacía a 30 fps, imágenes
  pintadas por código con lados de P4 / P8, materiales, el export con las opciones fijas (`GLTF_OPTIONS`), armaduras
  (`HUMANOID_BONES`, `ARMS_BONES`), sockets, pesos recortados a 2 y clips a 30 fps (claves LINEAR, una pista NLA cada
  uno, `loop` y `notifies` en los extras de la animación). `make_team_bodies.py` y `make_de_leon.py` lo usan y
  exportan los mismos bytes que antes.
- Gate: `Samples/make_art_samples.py` hace una caja con tapa (36 triángulos, textura pintada de 64 × 64 y 5 colores,
  skin de 2 huesos, el socket `Top` y el clip `Open`) y un maniquí sobre el esqueleto común (264 triángulos, el socket
  `Weapon_R` y el clip `Idle`). `check_art_determinism.py` exporta cada script dos veces y compara cada `.glb` con el
  de la otra pasada y con el del repositorio: idénticos los cinco (Body_CT, Body_T, de_leon y las dos muestras).
  LeonCook importa las muestras en un proyecto de prueba en `Engine/Saved` sin errores ni avisos (SK_, SKEL_ con sus
  sockets, M_, T_, `A_Open`, `A_Idle`); el contenido de ShooterGame no cambia.
- MCP: la extensión *MCP* oficial de Blender Lab (1.0.0, `bl_ext.user_default.mcp`) está instalada y habilitada en
  Blender 5.2 y carga en 5.2.0 LTS; su servidor escucha en `localhost:9876` solo en un Blender interactivo. El
  cliente es un puente para Cursor; Claude Code no tiene el servidor MCP de Blender configurado, así que N26 usó los
  scripts por línea de comandos (la alternativa del riesgo «blender-mcp con Blender 5.2»).

Desviaciones:
- El asset del gate no sale de una sesión MCP (el MCP no estaba cargado): las muestras se escribieron directamente como
  script, que es donde acaba el flujo.
- El loop y los notifies viajan en los extras de la animación glTF. Los notifies los lee el import desde N25; el loop
  aún no (todo clip entra en bucle), lo leerá N27.
- `VIS_` / `PORTAL_` son una propuesta hasta que N15 añada su regla de import.
- Los `.blend` no son deterministas (cada guardado cambia sus bytes): solo se comparan los `.glb`. El exportador
  escribe su versión en el `.glb` (`Khronos glTF Blender I/O v5.2.39`), así que Blender queda fijado en 5.2.
- El exportador avisa «more than 2 joint vertex influences» aunque el `.glb` lleve como mucho 2 pesos no nulos por
  vértice (comprobado en las dos muestras).

### N27 · Personajes, brazos y armas (L) — hecha

- CT y T con esqueleto común, animaciones de tercera persona, brazos de primera persona y armas texturizadas.

Gate: G5 y cook con presupuestos.

Estado: hecha.
- Arte ([ART_PIPELINE.md](../ART_PIPELINE.md#the-characters-arms-and-weapons)), low-poly con texturas pintadas por
  código al estilo de CS 1.6, todo de scripts de Blender con `leon_art` (D6, CC0): el CT (`SK_Body_CT`, 932
  triángulos: camuflaje gris azulado, chaleco, casco) y el T (`SK_Body_T`, 828: chaqueta marrón, pantalón oliva,
  pasamontañas, mochila) sobre `SKEL_Body` (23 huesos, una paleta), sus brazos (`SK_Arms_CT` 542, `SK_Arms_T` 588)
  sobre `SKEL_Arms` (11), texturas 128 × 128 P8, 2 pesos por vértice; y de cada arma un modelo de mundo (116-260
  triángulos, 64 × 64 P4) y uno de primera persona (308-556, 128 × 64 P8): cuchillo, Glock, USP, Deagle, AK-47,
  M4A1, MP5, AWP, HE, flash, smoke y C4. `leon_art` gana `MeshBuilder` (lofts, cajas, cilindros y prismas
  achaflanados sobre un atlas, pesados al hacerlos), `Rig` / `Pose` (FK sobre la pose de reposo e IK de dos huesos que
  pone el socket de la mano en la empuñadura) y `hand_socket_frame`.
- Animaciones a 30 fps posadas por código: tercera persona (`anim_body.py`: idle, walk y run en 4 direcciones con
  pasos, crouch idle y walk, las tres del salto, aim offsets de tres posturas a 5 pitches, fire, reload, throw,
  plant, defuse y dos muertes) y primera persona por arma (`anim_arms.py`: idle, draw, fire, reload con `MagOut` /
  `MagIn`, pin y throw de la granada, slash del cuchillo, plant del C4). Las duraciones son las del juego (recarga
  2,7 / 2,5 / 3,7 s, desenfunde 1 s). `ImportList.ini`: `BS_Locomotion` (2D), `BS_Crouch`, `AO_Rifle` / `AO_Pistol` /
  `AO_Grenade` (todos medidos desde la pose del rifle), los `BS_<Arma>_Idle` de los brazos y 26 montages;
  `DefaultGame.ini` los nombra y ShooterGame usa los cuerpos, los brazos, los sockets y los montages. Pistola → USP,
  rifle → AK-47, sniper → AWP, granada → HE; el resto espera a N30.
- Motor: `UCharacterAnimInstance` con locomoción agachada (blend space propio, crossfade, notifies solo del que
  suena); `AShooterCharacter` con los clips del salto, el crouch, montages de muerte (de espaldas si le disparan de
  frente, de bruces por detrás; una última sección que se repite los mantiene en el suelo), el aim offset del arma
  (ninguno plantando, desactivando o muerto), el idle de los brazos, los brazos del equipo (también tras el cambio de
  lado de N30d) y el modelo de primera persona del arma (`FirstPersonMeshName`); `Mesh1P` pasa a tickear con el
  peón (desde N18 no tickeaba) y solo se evalúa cuando se dibuja. `UGLTFImportFactory::NewSkeletonName`.
- Las tres inconsistencias: los notifies salen de `leon_art` como lista `{"name", "time"}` (segundos; las
  propiedades ID de Blender guardan listas de grupos) y un `notifies` que no es lista falla el import; el import lee
  `loop` (`bLoop`: los one-shots mantienen su último frame); el socket es `Weapon_R` en todas partes (código, config,
  docs, muestras y el personaje de test del motor).
- Además: el import glTF da la vuelta a v (glTF empieza arriba, las texturas del motor abajo): desde N21 toda
  textura glTF salía invertida y nadie lo vio hasta las primeras pintadas; de_leon y AxisTest se reimportaron (sus
  UV, nada visible). `S_Footstep` para los pasos.
- Fuera (D10): `SM_Body_*` y sus materiales, `BodyMesh`, `make_team_bodies.py`, las armas de cajas y sus
  materiales, el `make_weapons.py` de la librería estándar; los nombres viejos a `CheckBannedApis.ps1`.
- Tests: `System.MeshUtilities.GltfSkeletal.AnimationLoopFlag`, `System.Engine.Animation.CharacterAnimInstance.Crouch`,
  `ShooterGame.Animation.CharacterArt`, y comprobaciones de las UV, el esqueleto compartido y las mallas tras el
  medio tiempo: 537 del motor, 76 de ShooterGame, TestPAL 161; el test de pasos de N30c da su locomoción también
  al agachado. `check_art_determinism.py`: los 5 scripts idénticos dos veces y con lo commiteado; `CheckReimport`
  da los mismos bytes; el cook de PS2 pasa los presupuestos de N23. BotMatch 10 7: "Botmatch OK: 10 round(s), CT 6
  - T 4, 76 kill(s), seed 7, sides switched after round 5" (sin el arte, "9 round(s), CT 3 - T 6, 65 kill(s)": los
  pasos de los cuerpos animados hacen ruido para los bots, N30c), idéntico dos veces.
- PS2 (`MeasurePS2 -Label N27`, host:): 10,8 fps, media 92,7 ms, p50 / p95 / p99 83,5 / 150,3 / 158,3 ms; 2 240
  triángulos por frame (N18: 762) a unos 29 µs cada uno (skinning y transform en C++ en el EE), escena 66,7 ms; solo
  el view model (brazos y arma) son unos 900. La animación no tiene scope propio: el mundo pasa de 3,1 a 11,6 ms (con el
  movimiento de N30c), unos 5 ms de ellos el tick de las mallas y el envío de las skin matrices. `EngineMisc` de 2 048 a
  3 072 KB (pico 2 290). Los 30 fps son de N29 (LODs, VU1 de N14, menos triángulos en el view model).
- Capturas Win64 (`-BotMatchSpectate`): los brazos del CT con la USP en primera persona; los cuerpos en tercera, con
  una cámara de persecución temporal que no se commitea.

Desviaciones:
- `EngineMisc` sube de 2 048 a 3 072 KB en la PS2: los contenedores del GS del frame crecen con los triángulos y
  llegaron a 2 290 KB (con 2 048 el juego paraba con un error fatal a los 26 s).
- Una sola postura de locomoción (rifle); pistola y granada salen de su aim offset, y sus montages de torso se
  guardan en el espacio del rifle. Los montages de cuerpo entero van sin aim offset.
- El desenfunde del AWP dura 38 frames (1,27 s) y no los 1,25 s de CS, que no es un número entero de frames.
- Las muertes son montages con una sección final en bucle (no hay ragdoll ni estado de muerte en el anim instance).
- Los pies deslizan algo: los ciclos de andar y correr no cubren del todo 330 y 560 cm/s.
- En primera persona el arma sigue en la mano al plantar (el C4 no es un arma todavía); la desactivación no tiene
  clip de brazos.
- `MagOutSoundName` / `MagInSoundName` siguen vacíos (no hay sonidos de cargador; suena el de recarga).

### N28 · Reconstruir de_leon (L) — hecha

- Texturas, luces para el horneado, `UCX_`, celdas, sitios, buy zones y spawns.

Gate: 30 fps en `MeasurePS2`.

Estado: hecha.
- de_leon es un pueblo del desierto de 60 × 48 m al estilo de de_dust de CS 1.6
  ([LEVELS.md](../LEVELS.md#worked-example-de_leon), [ART_PIPELINE.md](../ART_PIPELINE.md#the-map-de_leon)), todo de
  `Maps/make_de_leon.py` con `leon_art` (D6, CC0, sin arte externo): muros y casas de arenisca con remate más oscuro,
  el arco de entrada a mid, la puerta de A long, el túnel de B long (techado a 3 m, dos lámparas), las mid doors con
  sus hojas de madera abiertas, cajas, las letras de los sitios en sus muros y dos escaleras (volúmenes `Ladder_A` /
  `_B`, N30c) a los tejados de los bloques norte. La distribución es la de antes (T al sur, CT al norte, A al este, B
  al oeste, mid y las dos largas con sus atajos): los sitios, las buy zones y las salidas no se mueven.
- Siete texturas pintadas téxel a téxel (arenisca 128 × 128 P8 de 30 colores; remate, arena, losas, madera, la caja
  de CS y las letras en P4; 64 téxeles por metro): 66 KB de VRAM propios, 256 KB con los comunes, de 1 856.
- 78 piezas, 2 244 triángulos (el blockout de N22: 1 070 en 35 cubos) en siete celdas: `TSpawn` 446, `Mid` 160,
  `LongA` 398, `LongB` 458, `CTSpawn` 234, `SiteA` 266, `SiteB` 282; el script falla si una pasa de 1 500. Cada pieza
  es una malla propia (`SM_<Celda>_<Pieza>`) con su caja `UCX_`, en ejes del mundo alrededor de su centro, con las
  caras cortadas en una rejilla de 3 m del mundo (y a 1,2 m en los muros, para la oclusión del pie) y sin las caras
  ocultas; las cajas comparten `SM_Crate` (1,1 m) y `SM_CrateBig` (1,6 m). Horneado: el sol, el cielo y tres luces
  puntuales (las dos del túnel y la de las mid doors), 3 471 vértices y 224 885 rayos en 0,07 s.
- Celdas de N15 con las convenciones de su fixture (`MakeCellsFixture.py`): `VIS_<Celda>` (7 cajas hasta 6 m) y
  `PORTAL_<A>_<B>` (24 quads: los huecos y las calles, y el cielo sobre los muros de cada frontera desde 3,5 m, el
  techo del muro más bajo; un segundo quad entre dos celdas es una copia `.001`). En Win64 (`-LogFrameTimes`, cuyo
  `Frame work` cuenta ahora las celdas vistas y los objetos que dejan fuera): mirando un muro en una salida o un sitio
  se ve 1 celda y se quitan 65-92 objetos; a lo largo de mid 6 y 23; desde el túnel 5 y 31; por una larga o cruzando
  una salida las 7. Las capturas con celdas son las mismas que sin ellas (nada desaparece).
- Navegación: 22 waypoints (se añaden `LongAGate`, `TunnelB`, `ALongEnd` y `BLongEnd`; el import añade 38 enlaces). El
  script comprueba que los enlaces a mano, las salidas y el centro de cada sitio (los objetivos de los bots) dejan
  libre la cápsula frente a todo lo que está a la altura de andar.
- `ViewFrom` mantiene su cámara tras el spawn de la ronda (la posesión volvía a mirar al pawn) hasta `ViewPawn`, que la
  borra: `-ExecCmds="ViewFrom X Y Z Pitch Yaw" -Screenshot=` saca las capturas del mapa. Tests:
  `ShooterGame.Map.DeLeonHoldsTheGame` cuenta las dos escaleras y `TenPawnsOnDeLeon` espera los pawns en el suelo (ya
  no hay plataformas de 1 cm): 557 del motor, 94 de ShooterGame, TestPAL 164.
- Fuera (D10): los cubos y plataformas del blockout (`SM_Wall`, `SM_Floor`, `SM_CrateStack`, `SM_Pad*`) y sus
  materiales planos (`M_Wall`, `M_Floor`, `M_Site*`, `M_Spawn*`).
- Gates: `RunGates.bat -PS2` entero OK (con el cook de PS2 y sus presupuestos: mapa 309 KB propios y 1 433 KB con los
  comunes, RAM estimada 4 402 de 24 576 KB, SPU2 70 de 2 027 KB, la malla mayor 146 triángulos de 4 096);
  `check_art_determinism.py` (con de_leon) PASSED; dos imports seguidos dan los mismos bytes (el horneado incluido) y
  `CheckReimport.bat Game\ShooterGame\ShooterGame.lproj` reimporta 107 assets sin cambios. BotMatch 10 7: "Botmatch
  OK: 10 round(s), CT 5 - T 5, 65 kill(s), seed 7, sides switched after round 5, reasons [1,3,3,3,3,1,3,3,3,3]",
  idéntico dos veces (sobre N30e; con el blockout, "7 round(s), CT 6 - T 1, 45 kill(s)"): los T ganan las diez rondas.
  Con las semillas 1 a 24 los T ganan el 61 % de las rondas (216), con 6,1 bajas por ronda; con el blockout, el 59 %
  y 6,2: el mapa apenas mueve el equilibrio.
- PS2 (`MeasurePS2 -NoBuild -Label N28`, host:, sobre N15): 8,76 fps (con el blockout, la fila de N15: 13,76), media
  114,2 ms, p50 / p95 / p99 117,0 / 133,5 / 150,3 ms, escena 83,4 ms (46,7), GIF del EE 277 KB por frame (87), mundo
  19,7 ms (14,5), pico de GMalloc 4 947 KB, pak de 1 829 771 bytes. Antes de N15, sobre N30e y con el mismo ELF: 6,67
  frente a 11,52 fps con el contenido viejo cocinado.
- Capturas Win64 desde las salidas, mid, las mid doors, A long, el túnel, los sitios, una escalera y desde arriba.

Desviaciones:
- La gate de 30 fps no se cumple: 8,76 fps, y el mapa nuevo baja el frame de 13,76 a 8,76 fps con el mismo código. Las
  78 piezas texturizadas y horneadas cuestan al EE mucho más GIF por frame (277 KB frente a 87) que los 35 cubos
  planos, con casi los mismos triángulos antes del recorte de la VU1; las celdas recortan poco en un mapa abierto.
  Averiguar por qué y el ajuste son de N29, como se pidió.
- Suelo: `COL_Ground`, una caja de colisión bajo todo el mapa, es el suelo por el que se anda; las cajas `UCX_` de las
  losas quedan 10 cm hundidas dentro. Con una caja por losa los bots se atascaban en las juntas (sus techos difieren
  en float un pelo y la cápsula chocaba con el canto): 4 rondas acababan por tiempo sin plantar.
- Una caja `UCX_` por malla (Leon funde las cajas de una malla en su AABB): un arco son dos pilares y un dintel, y cada
  muro es un nodo; 57 mallas en lugar de 8. Las escaleras, las letras y las lámparas llevan su caja dentro del muro.
- Los muros son bajos (3,5-5,5 m) y el mapa abierto: los portales del cielo dejan ver casi todo al mirar a lo largo
  del mapa y las celdas recortan poco ahí; subir el cielo o cerrar más el mapa es de N29.
- El cielo es el color de fondo del renderer (gris oscuro) y la sombra la da el cielo del horneado (0,35): no hay
  skybox ni forma de fijar `LightmassSettings` desde el glTF.
- El pico medido de GMalloc (4 947 KB) supera la estimación de RAM del cook (4 402 KB).
- Una cara con UVs de más de 14 repeticiones hace que `FLPS2MeshBuilder` sujete las UVs y LeonCook se cuelgue
  ("the LPS2 v2 blob it made is not valid" y un access violation); el script evita esas caras (la cara inferior del
  suelo mapea una sola repetición), el fallo del motor queda sin arreglar.

### N29 · Ajustes con el arte real (S) — hecha

- Niebla, LODs y rebaseline de las capturas.

Gate: G8.

Estado: hecha. 30 fps estables en NTSC con el arte real: `MeasurePS2` en de_leon (N28) da 29,81 fps, p50 / p95 / p99
33,5 / 33,5 / 33,5 ms (la fila N28: 8,76 fps, p95 133,5 ms), escena 8,4 ms (83,4), GIF del EE 19,1 KB por frame (277).
Pasos, cada uno medido sobre el anterior ([Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md), *30 fps
with the real art*):
- El perfil de la escena por partes (`GS Visibility`, `GS Opaque`, `GS Skinned`, `GS Translucent and Effects`, `GS
  View Model`, y dentro `GS Emitted Batches` y `GS Clipped Batches` con sus llamadas por frame) y el resumen
  `SceneWork:` (objetos, draws y lotes por destino: VU1, emisor, recortador), que `MeasurePS2` imprime y guarda como
  `Scene_<clave>`. Base (N14b sobre N28): 9,77 fps, escena 73,5 ms, de ellos 47,4 en `GS Skinned`: 181 lotes por frame
  por el emisor del EE y ninguno en la VU1.
- La causa: las tres luces puntuales horneadas del mapa (las lámparas del túnel y la de las mid doors) estaban en cada
  draw con luz y ningún microprograma aceptaba una. Cada draw toma ahora solo las luces puntuales cuyo alcance llega a
  sus bounds (las demás no iluminan ninguno de sus vértices: los mismos colores, `DrawMeshSection` con `WorldBounds`):
  23,68 fps, escena 24,2 ms.
- Luces puntuales en la VU1: StaticLit y SkinnedLit suman hasta dos (N·L y la atenuación (1 − d / r)², la posición de
  mundo con la LocalToWorld de la cabecera; el nivel de luz en la z de la cabecera: 0, 1, 2 o 3); cabecera con luz de
  23 quadwords y los streams desplazados (estáticos desde +24, paquete en +280; skinned desde +97, paquete en +337).
  `FGSVertexDraw::MaxVU1PointLights` = 2. VU1Conformance: 84 lotes, con draws de una y dos luces puntuales estáticos y
  skinned, XY 0, Z 5, RGBA 1, STQ 4 ulp, F 0. 29,85 fps, p95 33,5 ms, ningún lote en el emisor.
- El suelo de de_leon son las cajas `UCX_` de sus losas, con sus materiales (la arena es tierra, el pavimento
  baldosa): fuera `COL_Ground` y `SM_Ground`. El movimiento acepta las juntas: `FPhysScene::ResolveCapsuleSides` no
  empuja por una caja cuyo techo está en los pies (dentro de la piel) y `ACharacter::IsFloorEdgeHit` no deja que el
  barrido de la cápsula se pare en su canto (el barrido agranda la caja y ve su lado). Test
  `System.Engine.CharacterMovement.WalksAcrossFloorSeams` (antes se quedaba en X = −39 cm, ante la primera junta);
  `ShooterGame.Map.TenPawnsOnDeLeon` espera tierra en el spawn T y baldosa en el CT. El mapa se regeneró
  (`make_de_leon.py`, `check_art_determinism.py` PASSED) y se reimportó dos veces con los mismos bytes.
- Una cara con UVs de más de 14 repeticiones hace fallar el build con un error que lo dice (`FLPS2MeshBuilder`,
  test en `System.MeshUtilities.LPS2.Errors`), y los importadores de mallas y de mapas lo propagan con el nombre del
  asset (`StaticMeshImport::BuildStaticMesh` devuelve bool): LeonCook ya no sigue con una malla sin datos.
- La estimación de RAM del cook se calibra con lo medido: `RuntimeBaseKB=3072` (5 938 KB para los 1 433 KB cocinados
  de de_leon, frente a los picos de 4 947 KB de N28 y 5 069 con `-novu1`).
- Resultado (sobre N24 y N30f): 29,81 fps, 3 576 frames todos en el segundo campo; lo que queda en el EE son unos
  17,5 ms de los 33,3 (mundo 4,9, escena 8,4, canvas 2,9, cadena 1,6). BotMatch 10 7 con el mapa nuevo: "Botmatch OK:
  9 round(s), CT 3 - T 6, 55 kill(s), seed 7, sides switched after round 5", idéntico dos veces (antes "10 round(s),
  CT 5 - T 5, 65 kill(s)": las juntas ya no frenan a nadie). Gates: `RunGates.bat -PS2` entero.

Desviaciones:
- Niebla y LODs apagados: de_leon (60 × 48 m) cabe entero en el plano lejano y una pieza tiene como mucho 146
  triángulos; un plano lejano más cerca con niebla no quita nada que no quiten los portales. Con 16 ms de espera por
  frame no hace falta.
- No se juntaron las 78 piezas por celda y material (ni la colisión compuesta de varias `UCX_` por malla que haría
  falta): `GS Opaque` fuera del recortador es 1,3 ms; queda para cuando la escena lo pida.
- Celdas y portales sin tocar: `GS Visibility` 0,4 ms y el recorte flojo del mapa abierto no importa a 30 fps.
- Las capturas (G8, GSConformance) no cambian: el recorte de luces por bounds da los mismos colores y las luces en la
  VU1 son de la PS2; el cambio del suelo es solo de colisión.
- Los lotes que cruzan un plano (13 por frame, 2,4 ms con los del view model) siguen en el recortador del EE.

### N30 · Paridad CS (XL, a–e) — hecha

- **a:** armas, economía, penetración de paredes, hitgroups — hecha.
- **b:** flash y smoke — hecha.
- **c:** movimiento — hecha.
- **d:** rondas y HUD — hecha.
- **e:** bots — hecha.
- **f:** materiales físicos y sonidos por superficie — hecha (añadida a petición del usuario el 2026-09-29).

Gate de cada sub-fase: tests y BotMatch.

Estado (a): hecha.
- Armas con los valores de CS 1.6 (daño, range modifier, armor ratio, cadencia, cargador y reserva, precio,
  velocidad, penetración y caja de munición; 1 unidad = 2,54 cm) y el arte de N27: el cuchillo (`AShooterWeapon_Knife`,
  ranura 3: tajo 15, puñalada 65, x3 por la espalda), la Glock-18 (la de salida de los T, ráfaga de tres con el botón
  secundario), la USP (la de los CT, silenciador), la Desert Eagle, la MP5, el AK-47 (solo T), la M4A1 (solo CT,
  silenciador) y el AWP. Sustituyen a `AShooterWeapon_Pistol`, `_Rifle` y `_Sniper` (D10, en `CheckBannedApis.ps1`).
  Las pistolas comparten los clips de la pistola y la MP5 y los rifles los del rifle; una recarga dura el tiempo de CS
  (`ReloadDuration`) y su montage se reproduce al ritmo que encaja.
- Economía: un arma comprada trae solo su cargador y la reserva se compra por cajas del calibre (`primammo` /
  `secammo`, las teclas `,` y `.`, `BuyAmmo1` / `BuyAmmo2`); la pistola de salida trae dos cargadores más (12/24,
  20/40). El menú de compra tiene las páginas de CS (Pistolas, SMGs, Rifles, munición primaria y secundaria, Equipo) y
  solo lista lo que el equipo puede comprar; una compra negada dice por qué. Los bots compran el rifle de su equipo,
  su munición hasta llenarla y el casco sobre el kevlar que conservan.
- Penetración (`FireBullets3` de CS): una bala atraviesa hasta `PenetrationCount - 1` cosas; un cuerpo con 3/4 del
  daño, una superficie si sale de ella dentro de su potencia (el material la recorta para el resto del disparo:
  hormigón a 1/4, metal a 15 %) con la parte del daño del material (madera 0,6, metal 0,2, el resto 0,5); más allá de
  `PenetrationDistance` no atraviesa nada. La superficie sale del primer material de la malla golpeada con una tabla
  de config (`SurfaceMaterials`: las cajas de de_leon madera, sus paredes, suelos y plataformas hormigón).
- Hitgroups de CS en bandas de altura y lados de la cápsula (de pie o agachado): cabeza x4, pecho y brazos x1,
  estómago x1,25, piernas x0,75; el blindaje cubre todo menos las piernas (la cabeza con casco).
- `BotMatch 10 7`: `Botmatch OK: 9 round(s), CT 3 - T 6, 63 kill(s), seed 7, sides switched after round 5, reasons
  [4,4,4,3,4,2,4,3,3]`, idéntico dos veces (antes 65 bajas y ninguna desactivación).
- Tests: `ShooterGame.Arsenal.StatsTable`, `.SilencerAndBurst`, `.KnifeBackstab`, `ShooterGame.Weapons.Penetration`,
  `ShooterGame.Damage.HitGroups`, `ShooterGame.Buy.AmmoAndPrices` y `.TeamRestrictions`: ShooterGame 83, motor 541,
  TestPAL 162 (sobre N14).

Desviaciones:
- Sin `UPhysicalMaterial` en el motor: la superficie es una tabla de ShooterGame por nombre de material (no se toca el
  mapa, que rehace N28); solo cuenta el primer material de la malla golpeada. La salida de una pared se busca con una
  traza hacia atrás desde la profundidad de la potencia (CS salta esa distancia y sigue): una pared más gruesa que la
  potencia detiene la bala, y el tramo siguiente empieza en la salida, no a la distancia de la potencia. El cristal
  es el material por defecto de CS (potencia entera, la mitad del daño).
- Hitgroups por bandas de la cápsula, no por huesos: las poses de los bots no se evalúan si no se dibujan (N25), así
  que unas cajas por hueso irían con la pose de referencia en el botmatch. Los brazos son el lateral de la banda del
  pecho.
- Una recarga dura `ReloadDuration` de CS y el montage se ajusta a ella (antes duraba el montage); los desenfundes
  siguen durando su montage.
- Los valores de spread y retroceso son los del modelo del repo (grados), elegidos por arma a partir de CS, no una
  copia de sus fórmulas de accuracy. El tajo del cuchillo es siempre 15 (CS da 20 al primero tras una pausa).
- Una pistola de la salida para un peón sin equipo es la de los CT; los peones reciben la del equipo al poseerlos
  (antes de eso solo tienen el cuchillo).

Estado (b): hecha.
- Las tres granadas de CS 1.6 en la ranura 4: un arma de cada una (`AShooterWeapon_HEGrenade`, que sustituye a
  `AShooterWeapon_Grenade`, `_Flashbang` y `_SmokeGrenade`), con los límites de CS (dos flashes a $200, una HE y una
  smoke a $300); comprar una que ya se lleva añade una granada, y la tecla de la ranura las recorre (HE, flash, smoke).
  El menú de compra las lista en Equipo. Los proyectiles comparten `AShooterProjectile` y cambian su `Detonate`.
- Flash (`AShooterProjectile_Flashbang`, el `RadiusFlash` de CS): a cada jugador vivo cuyos ojos ve la explosión
  (traza de Visibility: las paredes tapan) dentro de 1500 unidades; la fuerza baja de 4 a 0 con la distancia y, según
  el ángulo entre la vista y el flash, la pantalla se queda blanca 1,5 / 0,45 / 0,2 veces la fuerza y se desvanece en
  3 / 1,75 / 1 (de frente, de lado, de espaldas; 255 o 200 de blanco). El HUD la dibuja como un solo tile blanco a
  pantalla completa (dos triángulos con alpha) bajo el resto del HUD. Un bot queda ciego un tercio del desvanecimiento
  (el `Blind` de CS): su sensing no mira a nadie y se queda quieto sin disparar (rama `Blind`).
- Smoke (`AShooterProjectile_Smoke`, `AShooterSmokeCloud`): una esfera de 325 cm durante 18 s (CS 1.6) que se espesa
  en 1 s y se aclara en los 3 últimos; mientras es espesa, la línea de visión de los bots que la cruza falla
  (`UShooterPawnSensingComponent::HasLineOfSightTo`, virtual como en UE). Se dibuja con seis sprites grises del mundo
  de cara a la cámara (`UWorld::EffectSprites`, `UGameplayStatics::SpawnEffectSprite`, nuevo en el motor: dos
  triángulos con alpha y la máscara de los efectos cada uno, del más lejano al más cercano; 12 triángulos por nube y
  ninguna función nueva del GS: la mezcla y la máscara de las marcas de impacto). La limpieza de la ronda la quita.
- `ThrowGrenade` (depuración: saca la ranura de granadas y lanza) para las capturas.
- `BotMatch 10 7`: `Botmatch OK: 9 round(s), CT 3 - T 6, 63 kill(s), seed 7, sides switched after round 5, reasons
  [4,4,4,3,4,2,4,3,3]`, idéntico dos veces (los bots no compran granadas: N30e).
- Tests: `ShooterGame.Grenades.FlashIntensity`, `.FlashBlindsBots`, `.SmokeBlocksSight`, `.CarryLimits` y
  `System.Renderer.Effects.EffectSprites`: ShooterGame 87, motor 542, TestPAL 162. Captura Win64 de una smoke.

Desviaciones:
- El flash no lo tapa el humo (en CS 1.6 tampoco; en CS:GO sí). Un flash más débil que lo que queda del anterior no
  cambia nada, y la ceguera de un bot dura hasta el final de la más larga.
- Los bots cegados se quedan quietos en vez de disparar al azar. Los bots no compran ni lanzan flashes ni smokes
  (N30e).
- La nube es una esfera fija (CS la hace crecer) y deja de tapar a mitad de su aclarado; la smoke estalla a los 1,5 s
  aunque siga rodando (CS espera a que pare). Los sprites no se ordenan con las mallas translúcidas: se dibujan después
  de ellas.
- Sin sonido propio de la smoke (sin asset); el flash usa el de la explosión.

Estado (c): hecha.
- El motor gana el modo propio de UE (`EMovementMode::Custom`, `ACharacter::GetCustomMovementMode`,
  `UCharacterMovementComponent::PhysCustom` y su `SafeMoveUpdatedComponent` en 3D), `ACharacter::Landed` (con la
  velocidad de la caída aún en `GetVelocityZ`), `OnJumped` y `Jump` virtual. ShooterGame los usa en
  `UShooterCharacterMovement`, con los valores de CS a 2,54 cm por unidad:
  - Daño por caída: al aterrizar a más de 580 u/s (1473 cm/s) quita `(v - 1473) × 100 / (2601 - 1473)` (el
    `DAMAGE_FOR_FALL_SPEED` de CS; letal a 1024 u/s). Es daño del mundo: sin instigador ni causante, así que la
    armadura no lo absorbe, no frena y en el killfeed sale `(world)`.
  - Escaleras: un trigger volume con el tag `Ladder` (un nodo `Ladder*` del mapa, `DefaultEditor.ini`; D15) es una
    escalera, una caja fina contra su pared cuya cara mira por el eje horizontal más estrecho. Mientras la cápsula la
    toca, el personaje va en el modo propio `Ladder`: sin gravedad y como el `PM_LadderMove` de CS (el avance por la
    vista con su pitch, la parte que entra en la cara se vuelve subida): 200 u/s (508 cm/s) mirando la escalera, más
    rápido mirando hacia arriba (como en CS), bajando mirando abajo; sin teclas se queda quieto. Saltar lo despega a
    270 u/s y cae; no vuelve a agarrarse hasta dejar de tocar escaleras. Seguir subiendo pasado el borde lo deja en la
    cornisa. Los bots no trepan: sus pawns atraviesan las escaleras (`bCanClimbLadders`).
  - Estamina del salto (`fuser2`): un salto cuesta 1,3158 s que se descuentan con el tiempo; en el suelo, cada 10 ms
    (un comando de CS) de un paso escala la velocidad horizontal por `1 - estamina × 0,19` (`ratio^(dt / 10 ms)` por
    paso, igual a cualquier paso fijo): 0,75 al saltar, 0,87 al aterrizar un salto normal. Al aterrizar de un salto
    corriendo se baja al 60 % y se recupera en medio segundo: el bunny hop no gana nada.
  - Tagging (`m_flVelocityModifier`): un disparo que hiere sin matar reduce a la mitad la velocidad y la máxima, que
    vuelven linealmente en 1 s. El daño del mundo no frena.
  - Pasos: los notifies `Footstep_L` / `Footstep_R` de la locomoción del cuerpo animado suenan (`FootstepSoundName`) y
    hacen un ruido que oyen los bots (`MakeNoise`, `FootstepNoiseLoudness` 0,5: 12,5 m a través de paredes, 25 m a la
    vista) solo por encima de 150 u/s (381 cm/s), como en CS: andar (Shift) y agachado son silenciosos.
  - En la escalera las armas tienen la dispersión del aire (fuera del suelo, como en CS).
- `BotMatch 10 7` da `Botmatch OK: 9 round(s), CT 3 - T 6, 65 kill(s), seed 7, sides switched after round 5, reasons
  [4,4,4,3,4,4,4,3,3]` y se repite idéntico (antes `7 round(s), CT 1 - T 6, 47 kill(s)`): la estamina y el tagging
  cambian los duelos; nadie cae desde altura en de_leon y sin cuerpos animados no hay pasos.
- Tests: `ShooterGame.Movement.FallDamage`, `.Ladder`, `.JumpStamina`, `.Tagging` y `.Footsteps`
  (`ShooterMovementTests.cpp`, al paso fijo de 30 Hz): ShooterGame 75, motor 535, TestPAL 161.

Desviaciones:
- La fórmula de caída es la pedida; las reglas multijugador de CS 1.6 (`FlPlayerFallDamage`) la multiplican además por
  1,25 (letal a unos 935 u/s). No se aplica.
- La estamina solo frena en horizontal; CS también baja con ella la altura del siguiente salto (`PM_Jump`), y no se
  hace. Empieza al saltar, como en CS, no al aterrizar.
- El tagging es 0,5 para todo disparo y vuelve lineal en 1 s; CS usa 0,65 con empujón en los impactos grandes (AWP,
  escopetas) y recupera 0,01 por frame del servidor. Aquí limita la velocidad máxima en lugar de multiplicar la
  velocidad en cada frame.
- La escalera no admite agacharse (CS sí, más lenta) y no suena. Los bots no la usan (N30e).
- No había pasos por distancia que mantener: sin cuerpo animado (hasta N27) no hay pasos.
- No hay test del motor para `PhysCustom` ni `Landed`: los cubren los de ShooterGame.

Estado (d): hecha.
- Descanso (`AShooterGameMode::bHalftime`, `mp_halftime`; `mp_maxrounds` fija `MaxRounds`): tras la ronda
  `MaxRounds / 2` todos los jugadores, bots incluidos, pasan al otro equipo; los marcadores siguen a los equipos
  (`AShooterGameState::BeginSecondHalf`, `IsSecondHalf`, `GetHalftimeRound`), el dinero vuelve a `StartMoney`, las
  rachas de derrotas a cero y todos los pawns reaparecen en las salidas del lado nuevo con la pistola. La partida sigue
  acabando con la mayoría o tras `MaxRounds`. `FShooterMatchChecker` comprueba el cambio (la ronda, cada jugador en el
  otro equipo, los marcadores cambiados, nadie con más dinero que el inicial) y que ocurra.
- Radar arriba a la izquierda (`RadarSize`, `RadarRange`): compañeros vivos, para los T el portador o la bomba en el
  suelo o plantada, las letras de los sitios, girando con el yaw de la vista; 10 primitivas en un 5v5 (20 como mucho)
  en la memoria del frame, sin reservar. Indicador de dirección del daño: un arco hacia la fuente (el tirador, la
  granada o la bomba) que se estrecha y oscurece en `DamageIndicatorDuration` (1 s).
- Cámara de muerte (`AShooterPlayerController::StartDeathCam`, `DeathCamDuration` 2 s): desde los ojos del cadáver (el
  `ASpectatorPawn`, quieto) mirando al asesino; después los compañeros vivos por sus ojos (Fire o `ViewNextPlayer` el
  siguiente, el botón derecho o `ViewPrevPlayer` el anterior, Jump la cámara libre), el siguiente cuando muere el
  observado, y la cámara libre sin nadie; el HUD nombra al asesino o al observado. `-BotMatchSpectate` sigue igual.
- `-botmatch -rounds=N` es una partida de N rondas (`MaxRounds`), así que el descanso entra en el botmatch:
  `BotMatch 10 7` da `CT 1 - T 6, 47 kill(s), seed 7, sides switched after round 5, reasons [4,4,4,3,4,3,3]` en 7
  rondas (el equipo que empezó de CT ganó 4 - 1 al descanso y 6 - 1 de T) y se repite idéntico.
- Tests: `ShooterGame.Rounds.HalftimeSwitchesSides`, `.MatchEndsAtTheMajority`,
  `ShooterGame.Spectate.DeathCamThenTeammates`, `.CyclingSkipsTheDead`, `ShooterGame.HUD.Radar` y `.DamageIndicator`
  (ShooterGame 70, motor 530, TestPAL 159); `ShooterGame.Bots.MatchOnDeLeon` cruza un descanso.

Desviaciones:
- El cambio de lado es el de CS:GO (`mp_halftime`); CS 1.6 no lo tiene de serie, sus partidas competitivas lo hacían a
  mano. Los bots conservan su nombre en la segunda mitad (N30e les da nombres de CS neutros de equipo y siembra su
  azar con su orden de creación, no con el nombre).
- No hay `SetViewTargetWithBlend` en el motor y no se añade: la cámara de muerte y el paso a los compañeros cortan sin
  mezcla (UE ShooterGame también corta).
- Un botmatch de 10 rondas puede acabar antes por la mayoría (hoy en la 7): la línea del gate cambia y juega menos
  rondas que antes.

Estado (e): hecha.
- Economía (`AShooterGameMode::GetTeamBuyPlan`, `ChooseBuyPlan`, `EShooterBuyPlan`): cada equipo decide su plan una
  vez al empezar la ronda. Pistolas en la primera de cada mitad; compra completa si la mitad del equipo lleva primaria
  o puede pagar el rifle del equipo con kevlar y casco ($3500 los T, $4100 los CT); si no, force-buy tras
  `ForceBuyLossStreak` (2) derrotas seguidas, tras una victoria o en la última ronda de una mitad, y eco en otro caso
  (en eco solo compra el bot que puede pagar la compra completa). Los bots compran por el plan (`BuyForRound`): el
  rifle, si no una MP5, si no una Desert Eagle, siempre con dinero para el kevlar; la munición, el blindaje, el kit
  del CT y, con lo que sobra, las granadas de `GrenadeBuyOrder` (flash, HE, humo y la segunda flash) dentro de los
  límites de N30b. El kit ya existía ($200, desactivar en 5 s en vez de 10).
- Granadas (`ConsiderGrenade`, `ThrowGrenadeAt`, rama `ThrowGrenade`): el T que se acerca al sitio de la ronda (18 m)
  lanza su flash (si no, su humo) al sitio y el CT que se acerca a la bomba plantada su flash (si no, su HE), una vez
  por ronda si lo dice el sorteo de la ronda (`GrenadeChance`, 0,4); un punto donde se vio, se oyó o un compañero avisó
  de un enemigo a 9-22 m recibe la HE (si no, una flash; si no, el humo) si lo dice su sorteo. Arco bajo
  (`ComputeThrowPitch`) con hasta 1 m de error del stream del bot, nunca contra una pared a 3 m, y la espalda a su
  propia flash hasta que estalla. Una granada sacada se lanza aunque aparezca un enemigo; todo lanzamiento dice «Fire
  in the hole!».
- Combate: strafe a izquierda y derecha al paso de andar, 0,4-1 s por lado sacados del stream; con rifle a 15 m o más
  se agacha y se para; con el AWP, quieto. Cegado por una flash dispara al azar (25 grados) alrededor de donde vio al
  enemigo, en lugar de quedarse quieto.
- Radio (`SendRadioMessage`, `EShooterRadioMessage`: los tres menús de CS 1.6 y «Fire in the hole!», «Bomb has been
  planted.»): un mensaje cada 1,5 s y 60 por ronda por jugador, solo a su equipo. Los bots dicen «Enemy spotted.» (con
  el sitio: un compañero libre a menos de 30 m va a mirar), «Need backup.» (el bot más cercano contesta «Affirmative.»
  y acude), «Sector clear.», «Bomb has been planted.» y «Fire in the hole!», y no repiten lo que un compañero dijo en
  los últimos 3 s. El HUD enseña los mensajes del equipo encima del dinero con el color del equipo; el jugador abre los
  menús con Z, X y C (`radio1`-`radio3`) y envía con los números (la C ya no agacha).
- Caída: el 1,25 de `FlPlayerFallDamage` (`FallDamageScale`), letal a unos 935 u/s (13,9 m).
- Nombres (petición del usuario durante la fase): los de BotProfile de CS (`BotNames`: Albert, Allen, Bert...) por
  orden de creación, neutros de equipo; el stream de cada bot se siembra con su índice de creación (`SetBotIndex`),
  no con su nombre, así que renombrar no cambia la partida.
- `BotMatch 10 7`: `Botmatch OK: 7 round(s), CT 6 - T 1, 45 kill(s), seed 7, sides switched after round 5, reasons
  [3,3,4,3,3,4,4]`, idéntico dos veces (el equipo que empezó de T ganó 4 - 1 al descanso y 6 - 1 de CT; antes 9
  rondas, CT 3 - T 6, 63 bajas). Con las semillas 1 a 24 los T ganan el 59 % de las rondas, con 6,2 bajas por ronda.
- Tests: `ShooterGame.Bots.EcoAndForceBuy`, `.BuysGrenadesWithinLimits`, `.ThrowsGrenades`, `.StrafeCrouchAndStand`,
  `ShooterGame.Radio.BotsReportEvents`, `.SectorClear` y `.PlayerMenu`; cambian `Grenades.FlashBlindsBots`,
  `Movement.FallDamage`, `Bots.Buy`, `Config.InputAndChannels` y los nombres de `Spectate.DeathCamThenTeammates` y
  `HUD.TextCache`: ShooterGame 94, motor 542, TestPAL 162. Captura Win64 de la radio en el HUD.

Desviaciones:
- El plan es una decisión de equipo al empezar la ronda, sin mirar el dinero del rival ni la puntuación; en eco un bot
  rico compra igual. El force-buy prefiere una MP5 o una Deagle con kevlar a un rifle sin él.
- El lado T sale favorecido (59 % de las rondas en 24 semillas): las granadas a puntos conocidos, la flash de entrada y
  el fuego a ciegas lo empujan; con `GrenadeChance` 0,6 los T pasaban del 70 %, y se dejó en 0,4. Los bots usan pocas
  de las granadas que compran (4 lanzamientos en la partida de la semilla 7): mueren antes o no llegan a la distancia.
- Los bots contestan al instante (CS espera un momento) y solo el más cercano; las órdenes que no son «Need backup.» ni
  «Taking fire» solo se contestan. Un bot que sostiene su sitio no va a mirar lo que avisa un compañero.
- La radio no suena (no hay voces) y el pad no tiene menú de radio (sus botones están ocupados).
- El fuego a ciegas no usa el cuchillo ni granadas; los bots siguen sin trepar escaleras (N30c): lo decidirá el mapa
  de N28.

### N30f · Materiales físicos y sonidos por superficie (M) — hecha

Añadida a petición del usuario el 2026-09-29 («la mejor práctica y que acerque a la experiencia CS»): sustituir la tabla
de nombres de material de ShooterGame por materiales físicos de verdad al estilo de UE, con pasos e impactos por
superficie y sonidos de radio.

Gate: tests, BotMatch, CheckReimport, determinismo del arte y `RunGates.bat -PS2` (cook de PS2 con el presupuesto de la
SPU2).

Estado: hecha.
- Motor (el subconjunto de UE): `UPhysicalMaterial` (PhysicsCore, `PhysicalMaterials/PhysicalMaterial.h`, assets `PM_`)
  con `SurfaceType` (`EPhysicalSurface`: `SurfaceType_Default` y `SurfaceType1..62`) y `DetermineSurfaceType`;
  `UPhysicsSettings` (`[/Script/Engine.PhysicsSettings] +PhysicalSurfaces=(Type=SurfaceType1,Name=Concrete)`) nombra
  los tipos; `UMaterial::PhysMaterial` (`UMaterialInterface::GetPhysicalMaterial`). Las consultas con
  `FCollisionQueryParams::bReturnPhysicalMaterial` rellenan `FHitResult::PhysMaterial` (y siempre `FaceIndex`): el
  material del triángulo golpeado (su slot en `FTriMeshCollisionData::MaterialIndices`,
  `UPrimitiveComponent::GetMaterialFromCollisionFaceIndex`) o el primer material del componente en una forma simple
  (las cajas `UCX_`). Sin el flag no se busca nada.
- Formato: `VER_LEON_COLLISION_MATERIAL_INDICES` (6), la versión mínima cargable: los triángulos de colisión guardan su
  slot. Se volvieron a guardar los 293 paquetes (`ResavePackages`, leyendo los viejos con slot 0) y se reimportó todo;
  el payload de una malla se lee y escribe con la versión de su paquete (`SerializeBulkPayload`).
- Import: el glTF lleva el material físico en los extras del material (`{"physMaterial":
  "/Game/PhysicalMaterials/PM_Wood"}`); `leon_art.make_material(..., surface="Wood")` los escribe y el import lo pone en
  el `M_` que crea o encuentra (el script manda, D6; los demás valores del material no se tocan); uno que no existe es
  un error. `UPhysicalMaterialFactoryNew` crea los `PM_` desde `ImportList.ini` (`Type=PhysicalMaterial`,
  `SurfaceType=`), antes de las mallas.
- ShooterGame: los tipos de textura de CS que usa (`SHOOTER_SURFACE_*` en `ShooterGame.h`, como el ShooterGame de UE):
  Concrete, Dirt, Metal, Wood, Tile, Glass, Computer, Flesh, con sus `PM_`. de_leon: arenisca, remate y letreros
  hormigón, arena tierra, losas baldosa, madera y cajas madera, lámparas metal; las armas metal (el C4 computer), los
  cuerpos y brazos flesh. Se exportaron de nuevo de_leon, los personajes, los brazos y las armas (solo cambian los
  extras de los materiales). La penetración lee la superficie del `PhysMaterial` del impacto y gana la baldosa (0,65 /
  0,2) y el computer (0,4 / 0,45) de `FireBullets3`.
- Sonidos (`make_sounds.py`, CC0, 22 050 Hz mono, 0,16-0,6 s): pasos por superficie, pie izquierdo y derecho
  (hormigón, tierra, baldosa, metal, madera; `FootstepSounds` sobre la superficie de una línea de 50 cm bajo los pies),
  los de la escalera cada 0,35 s alternando dos (`pl_ladder`), impactos por superficie (esquirla, polvo, cerámica,
  rebote en metal, madera, cristal) con su marca teñida y escalada, los `bhit_flesh` / `_kevlar` / `_helmet` de CS en
  un personaje, y la radio: un chasquido y un patrón de tonos por menú, y un aviso propio para «Fire in the hole!» y
  «Bomb has been planted.», solo para los jugadores locales del equipo. Pasos e impactos con `Priority=0.5`: el
  dispositivo deja libres las 10 últimas voces a un sonido de prioridad menor que la normal
  (`FAudioDevice::NumLowPriorityVoices`), la radio a 1,5; un sonido espacial inaudible (nivel 0 de audsrv, más allá de
  unos 218 m) no toma voz. SPU2 del mapa en el cook de PS2: 37 sonidos, 155 KB de 2 027 KB (70 antes).
- Tests: `System.Engine.PhysicalMaterial.TraceSurface` y `.SurfaceNames`, `System.LeonEd.Factories.PhysicalMaterials`,
  `System.AudioMixer.Device.LowPriorityAndInaudible`, `ShooterGame.Surfaces.Footsteps`, `.LadderSteps`, `.Impacts`,
  `ShooterGame.Radio.Sounds`; `ShooterGame.Weapons.Penetration` con paredes de material físico (más baldosa y sin
  material físico) y `ShooterGame.Map.TenPawnsOnDeLeon` con las superficies de de_leon: 561 del motor, 98 de
  ShooterGame, TestPAL 164. BotMatch 10 7: "Botmatch OK: 10 round(s), CT 5 - T 5, 65 kill(s), seed 7, sides switched
  after round 5, reasons [1,3,3,3,3,1,3,3,3,3]", idéntico dos veces y al de N28. `check_art_determinism.py` PASSED,
  `CheckReimport` sin cambios, `RunGates.bat -PS2` OK (Lint, tests, G5, G6, botmatch, validación y el paquete de PS2).
  Paquete 6: `SM_Cube` de identidad 2 372 bytes, SHA-256 `2D2B59B8…407C56`.
- **Falta escuchar** (el usuario, en Win64 y PCSX2): los pasos por superficie, la escalera, los impactos y los `bhit_`,
  y la radio.

Desviaciones:
- Sin fricción ni restitución en `UPhysicalMaterial` (la física de Leon no tiene dónde usarlas), sin material físico
  por defecto del motor (ninguno es la superficie por defecto), sin `PhysMaterial` en el body setup ni override por
  componente: una forma simple toma el de su primer material. `EPhysicalSurface` vive en `PhysicalMaterial.h` (UE:
  `Chaos/ChaosEngineInterface.h`). `FaceIndex` se rellena siempre (UE: con `bReturnFaceIndex`).
- El suelo de de_leon sigue siendo una caja (`COL_Ground`) con el material de la arena: todo el suelo es tierra para
  pasos y balas, también las losas del spawn CT y de los sitios (su baldosa queda en las losas, bajo la caja). Partir
  el suelo en arena y losas (dos cajas a Z = 0 exacta, o una de losas sobre la de arena) cambiaba el botmatch (9 rondas,
  CT 6 - T 3, 63 bajas, dos rondas por tiempo): la cápsula que cruza el borde de una caja a la altura del suelo se
  empuja hacia atrás, como en las juntas de N28. Queda para N29 (o para una resolución de lados que acepte bordes al
  nivel de los pies).
- Una variante por superficie en los impactos y dos (izquierdo y derecho) en los pasos, sin azar (CS elige al azar
  entre cuatro); la escalera de de_leon es de madera y su sonido es un golpe de peldaño de madera, no el metal de CS.
  La radio son tonos, no voz.
- La culling por distancia casi no actúa en de_leon (hace falta más de 218 m para el nivel 0 de audsrv); lo que
  protege las voces de los disparos es la prioridad baja de pasos e impactos.

### N31 · Documentación y release (S) — hecha

Gate: `RunGates.bat` y la fila final de `MeasurePS2`.

Estado: hecha.
- Releases en el CHANGELOG: lo que estaba en `[Unreleased]` se reparte en tres secciones fechadas el 2026-09-29, cada
  una con su resumen y su fila de PCSX2: **0.22.0** (N0–N11: la fila «N11», 25,7 fps), **0.23.0** (N12–N20 con N14b:
  las filas «N17» y «N18», 29,9 y 29,65 fps en el mapa de bloques) y **0.24.0** (N21–N31 con N24b y N30a–f: la fila
  «0.24.0»). `[Unreleased]` queda vacío. Las fases no se hicieron en el orden de los hitos (N14, N14b y N15 llegaron
  tras el arte), así que alguna entrada nombra fases de una release posterior.
- Versión: `Engine/Build/Build.version` pasa de 0.21.0 a 0.24.0. La versión va en el resumen de cada paquete, así que
  se volvió a guardar todo el contenido del motor y de ShooterGame (`ResavePackages`), se reimportó entero
  (`ImportAssets -reimport -all`) y se volvieron a hornear los mapas sin fuente (`ResavePackages -buildlighting`); un
  segundo reimport da los mismos bytes. La identidad del import (`SM_Cube`) pasa a
  `A24A188795C94680A639E1BC18822206EA48DE8135B9625E97C9FAC9630ED1FC` (2 372 bytes; ASSET_FORMATS y TOOLS). El hash
  dorado de `System.CoreUObject.Package.Deterministic` no cambia: se calcula sin la versión del motor.
- Documentación al día con el código: ARCHITECTURE (mapa de módulos y el frame con VU1 / VU0 / SPR / cadenas DMA, tick
  groups y paso fijo, IO asíncrona, materiales físicos), ASSET_FORMATS, TESTING (los recuentos finales), BUILD, SETUP,
  TOOLS, LEVELS, ART_PIPELINE, CODING_STANDARD (la regla D7), los README (raíz, PS2, ShooterGame y módulos) y
  Budgets.md (una tabla final con las filas que se pueden comparar; la tabla de tiempos de frame vuelve a ser una
  sola). Se arreglan también un enlace roto de NextSteps a ARCHITECTURE y el comentario de `GSSceneRenderer.h` sobre
  la luz de la VU1 (ya son hasta dos luces puntuales).
- D10: `CheckBannedApis.ps1` gana las reglas de lo que borraron N14b (`bQuantizedPose`, `AllocatePosedStreams`,
  `QuantizePose`, `SkinBatch`, `PosedPositions` / `PosedNormals`) y N24b (`LoadOptionalAsset`), que faltaban.
- Verificación: `RunGates.bat -PS2` entero OK (Lint, 575 tests del motor, 35 casos dorados de LeonHeaderTool, 99 de
  ShooterGame, TestPAL 171 en Win64, CheckReimport, SmokeTest, BotMatch, la validación del contenido y el paquete de
  PS2). BotMatch 10 7: "Botmatch OK: 9 round(s), CT 3 - T 6, 55 kill(s), seed 7, sides switched after round 5",
  idéntico dos veces y al de N29 (la versión no cambia la partida).
- PS2 (PCSX2 2.8.2, Measure.ini d29e64bc): `MeasurePS2 -Label 0.24.0` (host:) da **29,95 fps**, media 33,38 ms,
  p50 / p95 / p99 33,5 / 33,5 / 33,5 ms, el peor frame 50,05 ms; mundo 4,67 ms, escena 8,42 ms, canvas 2,89 ms, GIF
  del EE 19,1 KB por frame, pico de GMalloc 4 432 KB (la fila N29: 29,81 fps, escena 8,4 ms). La ISO: `MeasurePS2 -Iso
  -LogFileOpenOrder` (pak en orden de rutas) llega al primer frame a los 10,08 s, y con ese orden
  (`-PakOrder`) a los **2,93 s**, con 29,96 fps y ningún frame tras el primero por encima de 50,05 ms: lo mismo que N24b.
  La ISO de release (`BuildCookRun -platform=PS2 -build -cook -stage -pak -iso -pakorder=<ese log>`, sin la línea de
  comandos de medida) mide **7 122 944 bytes** (3 478 sectores; el ELF 4 734 720 bytes); la de medida, 7 124 992, arranca
  en PCSX2 desde `cdrom0:` y juega el botmatch.

Desviaciones:
- La ISO de release se construyó y se midió su tamaño, pero lo que arrancó en PCSX2 fue la de medida (la misma
  build y el mismo pak; solo cambia `LEONCOMM.TXT`).
- No se crean tags de git ni se hace push: quedan para el usuario.
- TestPAL no se volvió a correr en el EE (la última cuenta en PCSX2 es la de N15, 157).

## Verificación por fase

**Automática**:
- `RunGates.bat`;
- `leonrun` (TestPAL, `VU1Conformance` y el botmatch en el EE);
- `MeasurePS2.bat` (PCSX2 headless; filas de Budgets.md etiquetadas con el hash del ini de PCSX2).

**Manual**:
- escuchar el audio;
- el DualShock (vibración, presión, reconexión);
- aprobar el arte a la vista.

Las capturas se comparan siempre en la misma GPU.

## Riesgos

| Riesgo | Mitigación |
|---|---|
| Depurar la VU1 | Programas pequeños de uno en uno; VU1Conformance con lectura de vuelta; `GSH_Capture`; depurador de VU de PCSX2 |
| blender-mcp con Blender 5.2 | Se verifica en N26; si falla, scripts CLI (D6); un export glTF sin timestamps |
| Determinismo con paso fijo | Tiempo entero, orden de tick estable, la interpolación no realimenta; determinismo por plataforma |
| PCSX2 no es hardware | Filas de Budgets etiquetadas |
| 32 MB con arte real | Presupuestos de cook (N23) y arenas (N17) antes del arte |

## Resultado

Frente al Objetivo, en 0.24.0:

- **La PS2 como una PS2.** Los static meshes y los personajes se transforman, iluminan (ambiente, un sol y hasta dos
  luces puntuales), recortan y empaquetan en la VU1 (StaticUnlit / StaticLit y SkinnedUnlit / SkinnedLit, con la paleta
  de huesos en su memoria); el EE solo decide por lote y recorta en C++ los que cruzan el near o la guard band (D8). El
  frame es una cadena DMA de VIF1 con doble búfer; el vblank llega por interrupción; VU0 en macro mode hace las
  matrices y el culling; el scratchpad guarda las listas del frame. El GS usa mips, niebla, alpha test, sprites y CLUT
  con CLD, y cada función tiene su escena de conformidad (D7). El audio son voces ADPCM de la SPU2; la IO es asíncrona
  con un hilo del EE; la memoria va en arenas con presupuestos por tag; la colisión tiene broadphase. Nada legacy:
  ThirdPerson, la vía inmediata, Linux, Jolt, FBX, la mezcla software y el paso variable se borraron (D10, D11).
- **30 fps.** ShooterGame en de_leon con el arte real da 29,95 fps en PCSX2 (NTSC), con p50 / p95 / p99 de 33,5 ms: cada
  frame en el segundo campo. El EE trabaja unos 17,5 ms de los 33,3 (N29).
- **La ISO arranca en PCSX2.** `BuildCookRun -platform=PS2 -iso` da `ShooterGame.iso` (7 122 944 bytes); la BIOS de PCSX2
  arranca el ELF desde `cdrom0:`, el juego monta su pak del disco y juega el botmatch (29,96 fps desde el disco). El primer frame llega
  a los 2,93 s con el pak en su orden de apertura y ningún fichero se abre después.
- **El arte.** Low-poly CC0 texturizado al estilo de CS 1.6, todo de scripts de Blender versionados (D6): el CT y el T
  sobre un esqueleto común con sus animaciones de tercera persona, los brazos de primera persona, doce armas con su
  modelo de mundo y de primera persona, y de_leon reconstruido como un pueblo del desierto de 78 piezas en siete
  celdas, con la luz horneada por vértice en LeonCook (D5) y materiales físicos. `check_art_determinism.py` repite
  cada `.glb` byte a byte.
- **Paridad con CS.** Armas de CS 1.6 con economía, munición, penetración y hitgroups; HE, flash y smoke; daño por
  caída, escaleras, estamina del salto, tagging y pasos por superficie; descanso con cambio de lado, radar, dirección
  del daño y cámara de muerte; bots con eco y force-buy, granadas, strafe y radio; materiales físicos con pasos,
  impactos y penetración por superficie.

Las medidas de PCSX2 (`MeasurePS2`, dos rondas con la semilla 7 vistas por los ojos de un bot; PCSX2 2.8.2; el detalle
y las filas intermedias en [Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md)):

| Fila | Contenido | fps | p50 / p95 / p99 (ms) | Mundo | Escena | GIF del EE por frame | Pico de GMalloc |
|---|---|---:|---:|---:|---:|---:|---:|
| N1 (línea base, -O2) | de_leon de bloques | 25,4 | 33,5 / 50,3 / 66,8 | 9,0 ms | 11,8 ms | 21,4 KB | 2 266 KB |
| N11 (fin de 0.22.0) | de_leon de bloques | 25,7 | 33,5 / 50,3 / 66,8 | 7,9 ms | 13,0 ms | 25,0 KB | 2 385 KB |
| N18 (paso fijo; 0.23.0) | de_leon de bloques, suelo horneado | 29,65 | 33,5 / 33,5 / 50,3 | 3,1 ms | 15,6 ms | 45,9 KB | 1 949 KB |
| N27 (personajes animados, EE) | cuerpos y brazos skinned | 10,8 | 83,5 / 150,3 / 158,3 | 11,6 ms | 66,7 ms | 176,3 KB | 4 183 KB |
| N28 (de_leon reconstruido) | el arte real | 8,76 | 117,0 / 133,5 / 150,3 | 19,7 ms | 83,4 ms | 277,0 KB | 4 947 KB |
| N29 (30 fps con el arte) | el arte real | 29,81 | 33,5 / 33,5 / 33,5 | 4,9 ms | 8,4 ms | 19,1 KB | 4 261 KB |
| 0.24.0 (final) | el arte real | 29,95 | 33,5 / 33,5 / 33,5 | 4,7 ms | 8,4 ms | 19,1 KB | 4 432 KB |

Hasta N18 cada fila juega otra partida (el paso variable), así que se comparan por partes; desde N18 una build que no
toca la simulación juega la misma, y las filas con el mismo contenido se comparan enteras.

## Pendiente / fuera de alcance

Resumen en inglés en [PENDING.md](../PENDING.md).

Lo que las Desviaciones de las fases dejan abierto:

- **Render.**
  - Los lotes que cruzan el near o la guard band siguen en el recortador C++ del EE (13 por frame, 2,4 ms con el view
    model; N29); recortar en la VU1 o partir más los lotes no hizo falta a 30 fps.
  - Las celdas recortan poco en el mapa abierto (los portales del cielo dejan ver casi todo; `GS Visibility` 0,4 ms).
  - Niebla y LODs apagados en de_leon: están en el motor y probados, pero el mapa cabe en el plano lejano y ninguna
    pieza pasa de 146 triángulos (N29).
  - Las 78 piezas de de_leon no se juntan por celda y material, y una malla solo tiene una caja `UCX_` (Leon funde sus
    cajas en un AABB): hace falta colisión compuesta.
  - Sin sondas de luz para lo que se mueve (los peones toman el cielo sin oclusión), sin skybox, y las luces Movable no
    iluminan el mundo estático (N22, N28).
  - `GSH_Capture` (D3) no se hizo; la VU1 se valida con VU1Conformance en PCSX2 y con tolerancias medidas (D2).
  - El frame PAL de 512 líneas (hoy 448 centradas) y CSM2 siguen fuera.
- **Disco y memoria.**
  - La carga es del disco: 2,9 s hasta el primer frame con el pak ordenado (N24b), 0,5 s más que la carga síncrona de
    antes de N24, porque se precarga todo lo que la partida puede pedir.
  - Sin fileXio ni `sceCdRead`: se lee por la FILEIO de la ROM. Sin música en streaming (hace falta un módulo del IOP).
  - Sin logo de licencia ni sectores de sistema: la ISO arranca en PCSX2, no en una consola sin modchip.
  - Los UObjects se asignan uno a uno (la arena por nivel es la de los bytes de los paquetes) y el GC incremental no usa
    barreras de escritura (N17, N18).
- **Juego.**
  - Hitgroups por bandas de la cápsula y no por huesos; spread y retroceso elegidos por arma, no las fórmulas de CS.
  - Los bots no trepan escaleras ni usan todas las granadas que compran; el lado T gana el 59-61 % de las rondas.
  - La radio son tonos, no voces, y el pad no tiene menú de radio; una variante de sonido por superficie; sin sonido de
    smoke ni de cargador; el humo es una esfera fija y no tapa el flash.
  - Una sola postura de locomoción (rifle), los pies deslizan algo, sin clip de brazos para desactivar y el C4 no es un
    arma en primera persona.
- **Comprobaciones humanas pendientes.**
  - Escuchar el audio en Win64 y PCSX2: disparos, recargas, la bomba, los pasos e impactos por superficie, la escalera,
    los `bhit_` y la radio (N19, N30f).
  - El DualShock 2 en una consola o con un pad real: vibración, presión y reconexión (N24).
  - La tarjeta de memoria en el navegador de la BIOS: el icono y el título de `icon.sys` (N24).
  - La vibración de un pad de Xbox en Win64 (N24b).
  - PAL sin probar: la región sale de ROMVER (`-PAL` la fuerza) y daría 25 fps, pero nunca se ha corrido ni medido.
  - Aprobar el arte a la vista.
- **PCSX2 no es hardware.** No carga al reloj del EE el dibujo del GS ni emula la caché de datos ni los contadores de
  rendimiento: el solapamiento del EE y el GS, la ganancia del scratchpad y los fallos de caché solo se verán en una
  consola. Todas las filas de Budgets.md llevan la versión de PCSX2 y el hash de `Measure.ini`.
