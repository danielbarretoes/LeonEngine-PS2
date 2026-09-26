# Plan: el motor en PS2 (ShooterGame jugable en la consola)

## Objetivo

ShooterGame corre en la PS2: de_leon cargado desde un pak, el jugador con el mando, los bots, el HUD y la ronda
completa, dibujado por el GS a través de `FGSCommandList` y medido contra los presupuestos de la plataforma. Es el
paso que [ps2-gs-parity](ps2-gs-parity.md) deja fuera ("el motor en PS2") y que ese plan prepara.

## Contexto (hechos del código después de GS P3)

| Área | Hoy |
|---|---|
| Targets | `ShooterGame.Target.cmake` es `PLATFORMS Win64 Linux`. ThirdPerson es el único juego de PS2 y compila con `WITH_ENGINE=0`: sin `UWorld`, actores ni paquetes; su nivel se construye en código |
| Módulos solo de escritorio | Engine, Renderer, RenderCore, UMG, SlateCore, AIModule, AnimationCore, PhysicsCore, AudioMixer y ShooterGame son `PLATFORMS Desktop` |
| Acoplamiento real a GL | Engine llega al render solo por `IRendererModule` (`GetRendererModulePtr`); las demás menciones a OpenGL en Engine son comentarios. Engine ya arranca sin renderer ("headless", `-nullrhi`), que es como corre el botmatch. RenderCore tiene `GLClipSpace.h`; Renderer usa OpenGLDrv y Glad |
| Física | Jolt solo en Win64 (`LEON_WITH_JOLT`); Linux ya usa la colisión propia de PhysicsCore sin Jolt |
| Audio | AudioMixer usa miniaudio (escritorio) |
| Paquetes | CoreUObject, `FLinkerLoad` y PakFile ya corren en el EE (TestPAL, paks en memoria). `FPS2PlatformFile` lee `host:`, `cdrom0:` y `mass:` |
| Cook | El target PS2 del cook es un stub que cocina los formatos de Win64; `BuildCookRun -platform=PS2` se rechaza |
| Render en PS2 | `FPS2RHI` sobre `FGSCommandList` (GS P3): doble buffer `PSMCT16S` con dithering y `PSMZ24`, 1,7 MB de VRAM libres para texturas. Aún sin verificar en PCSX2 |
| Presupuestos | 32 MB de RAM (el kernel reserva 1 MB), 4 MB de VRAM, 256 KB de nombres, 8 192 UObjects, 400 KB de reflexión. En escritorio, una partida de ShooterGame tiene un pico de 3,9 MB de GMalloc, 2 283 objetos vivos y 194 KB de heap de reflexión |

## Decisiones

| # | Decisión | Por qué | Alternativa descartada |
|---|---|---|---|
| D1 | **Un solo motor**: Engine, UMG, AIModule, etc. pasan a compilar para PS2 sin forks ni `#if PLATFORM_PS2` en la lógica de juego; lo específico de la plataforma va en los módulos de plataforma y en los backends | Paridad de comportamiento con Win64 y un solo sitio donde arreglar bugs (la regla del proyecto: UE como referencia) | Un "ShooterGame PS2" reescrito sobre la API de ThirdPerson: duplica el juego |
| D2 | **Primero headless**: el primer hito es el botmatch sin render en el EE (`-nullrhi -botmatch`). Valida carga de paquetes, reflexión, memoria, física, IA y ticks antes de tocar el render | Separa los fallos de lógica y memoria de los de dibujo, y reutiliza el modo que ya existe | Portar todo a la vez: cualquier fallo en PCSX2 sería imposible de localizar |
| D3 | **El Renderer se vuelve multiplataforma a través de GS P5**: `FSceneRenderer` genera `FGSCommandList` en todas las plataformas; OpenGLDrv queda como backend de escritorio. En PS2, `IRendererModule` lo implementa el mismo Renderer, que entrega la lista a `FPS2RHI::Submit` | Un solo renderer de escena (el contrato D1 del plan de paridad) | Un renderer propio de PS2: lo que se valida en Win64 no sería lo que dibuja la consola |
| D4 | **Orden con el plan de paridad: P5 antes que P4.** P4 (el GL como emulador del GS) mejora la vista previa pero no desbloquea la consola. Mientras P4 no llegue, en Win64 el GL actual sigue dibujando y la lista de P5 se valida con la referencia por software | Lo que falta para jugar en PS2 es P5, no P4 | Terminar P4 primero: retrasa la PS2 sin aportarle nada |
| D5 | **Audio al final y nulo mientras tanto**: AudioMixer con un dispositivo nulo en PS2; el audio real (SPU2 por `audsrv`) es la última fase | El audio necesita módulos del IOP y el toolchain del IOP; no bloquea jugar | Portar miniaudio: no existe backend para la SPU2 |
| D6 | **Objetivo de rendimiento: 30 fps estables** en de_leon con los bots del botmatch, medido en PCSX2 con la velocidad del EE sin overclock. Si no se alcanza, se reducen el número de bots, la frecuencia de su IA o el detalle (en ese orden), con datos | 60 fps con esta cantidad de lógica en C++ en un EE de 294 MHz no es realista sin VU1; 30 es habitual en shooters de PS2 | Fijar 60 fps desde el principio |

## Fases

Tamaños: S, M, L, XL, como en los planes anteriores.

### E0 · Validar la base (S) — manual

- `Package.bat`: ThirdPerson y `GSConformance` arrancan en PCSX2 (valida GS P3: la lista, el paquete GIF, el doble
  buffer, 16 bits con dithering).
- TestPAL en PCSX2: `TestPAL: PASSED`, y sus números de memoria y nombres a [Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md).

Gate: capturas de PCSX2 de ThirdPerson y de `GSConformance`, y el log de TestPAL.

### E1 · El motor compila y corre headless en el EE (L) — compila; falta el botmatch en PCSX2

- Quitar `PLATFORMS Desktop` de Engine, UMG, SlateCore, AnimationCore, AIModule, PhysicsCore y ShooterGame. Lo que
  no compile se resuelve en su módulo de plataforma o detrás de una interfaz, nunca con `#if` en la lógica (D1).
- RenderCore: separar lo genérico (`PixelFormat.h`, `MeshData.h`) de lo de GL (`GLClipSpace.h` pasa al Renderer).
- Renderer y OpenGLDrv siguen siendo de escritorio en esta fase: en PS2 no hay `IRendererModule` y Engine corre
  headless, como con `-nullrhi`.
- AudioMixer: dispositivo nulo en PS2 (D5); miniaudio solo en escritorio.
- PhysicsCore sin Jolt, como en Linux.
- Launch en PS2 con `WITH_ENGINE=1` para ShooterGame: `FEngineLoop` crea `GEngine` y abre `GameDefaultMap`.
- Staging: `BuildCookRun -platform=PS2` deja de rechazarse. Por ahora cocina con el stub (formatos de Win64, que
  headless no dibuja) y deja el pak junto al ELF para leerlo por `host:`.
- `ShooterGame.Target.cmake` pasa a `PLATFORMS Win64 Linux PS2`, y `Package.bat` lo compila y empaqueta.

Gate:
- ShooterGame compila para PS2 sin warnings (automático).
- En PCSX2, `ShooterGame.elf -nullrhi -benchmark -botmatch -rounds=10 -seed=7` termina con estado 0 (los invariantes
  del botmatch se cumplen) y da el mismo resultado dos veces seguidas. No tiene por qué coincidir con Win64: los
  floats del EE no son IEEE.
- Memoria, objetos, nombres, reflexión y tamaño del ELF medidos y anotados en Budgets.md, dentro de sus límites.

Estado:

- Engine, UMG, SlateCore, AnimationCore, AIModule, PhysicsCore, RenderCore, ShooterGame y stb (header-only, lo usa
  el `FCanvas` para el texto) compilan para PS2. AudioMixer excluye miniaudio en PS2 y la extensión de plataforma pone
  un `FAudioDevice` silencioso.
- Un target compilado contra el motor (`COMPILE_AGAINST_ENGINE`) añade Engine y PakFile a su clausura en cualquier
  plataforma (UE: `bCompileAgainstEngine`). Así Launch no depende de Engine en PS2 y ThirdPerson sigue sin motor.
- Launch ya no exige escritorio con `WITH_ENGINE=1`. `FPS2PlatformProcess::Sleep` (con `nanosleep`) para el bucle
  headless con pausa.
- Argumentos: PCSX2 y el arranque desde disco solo pasan `argv[0]`. El launch de PS2 añade los de
  `LeonCommandLine.txt`, junto al ELF (UE: `UECommandLine.txt`).
- Staging: `BuildCookRun -platform=PS2` (solo Development) copia el ELF y la carpeta cocinada **suelta** junto a él
  y escribe `-addcmdline` en `LeonCommandLine.txt`. `-run` lo abre en PCSX2 (`RunPCSX2.ps1 -StagedElf`).
  `Package.bat` lo empaqueta.
- ShooterGame.elf: 1,6 MB de código (unos 85 KB de reflexión generada), sin warnings. ThirdPerson crece 2 KB (el
  lector del fichero de argumentos). Win64 y Linux sin cambios: 395 tests del motor, 42 de ShooterGame y el botmatch
  en Linux (CT 6 - T 4, 72 bajas).
- Desviaciones del plan: el pak queda para E3 (en E1 bastan los ficheros sueltos por `host:`, el camino que ya usa la
  config de ThirdPerson). `GLClipSpace.h` se queda en RenderCore porque es solo matemática (una matriz) y Engine la
  usa.
- Riesgo pendiente: la pila del EE es de 128 KB (`linkfile` de ps2sdk). El marco más grande del ELF es de 4 KB (el
  unwinder de libgcc), pero la recursión solo se ve en ejecución.
- Pendiente (manual): el botmatch en PCSX2 dos veces, y los números de `Botmatch budget:` en Budgets.md.

### E2 · Renderer de escena en PS2 (L) — es GS P5; compila, falta verla en PCSX2

- Lo que describe P5 del plan de paridad (transformación, clipping, skinning e iluminación por vértice en C++ que
  generan un `FGSCommandList`), con el Renderer ya compilando en todas las plataformas (D3).
- En PS2, `IRendererModule` entrega la lista del frame a `FPS2RHI::Submit`. UMG y el HUD pintan por `FCanvas` en la
  misma lista.
- En Win64, el GL actual sigue dibujando hasta GS P4. La lista de de_leon se compara en tests con la referencia por
  software (D4).
- Texturas: mientras no haya cook de PS2, se suben como `PSMCT32` con el presupuesto de VRAM de E3 en mente.

Gate: de_leon se ve en PCSX2 con el jugador, los bots, los tracers y el HUD. La lista de un frame fijo coincide con
la referencia dentro de la tolerancia de GS P2. El botmatch headless no cambia.

Estado:

- El Renderer compila en todas las plataformas. El `FGSSceneRenderer` es común. En PS2, `Renderer_PS2.Build.cmake`
  añade `FPS2RendererModule`, que graba el view family y el canvas contra `FPS2RHI::GetDrawEnvironment()` y los añade
  al frame con `FPS2RHI::Submit`. Las texturas van a la arena de VRAM que deja la pantalla
  (`FPS2RHI::AllocateTextureArena`: desde la página 280, 1,8 MB).
- LeonBuildTool añade el Renderer, igual que Engine y PakFile, a todo target compilado contra el motor.
- Se adelantó GS P4: el escritorio ya no tiene renderer GL propio, así que la vista previa es lo que dibuja la PS2.
- Texturas: se suben como `PSMCT32` con la caché de GS P5 hasta el cook de E3.
- Verificado aquí: ShooterGame, ThirdPerson, TestPAL y GSConformance compilan para PS2 sin warnings.
  - ShooterGame.elf: 1,68 MB de código.
  - Tests del motor: 396 en Linux; 395 con `-nodisplay`.
  - Tests de ShooterGame: 42.
  - TestPAL: 128.
  - Botmatch en Linux: CT 6 - T 4, 72 bajas, igual que antes.
  - `System.Renderer.GSEmulator.SceneFrame` compara el frame del renderer con la referencia.
- Pendiente (manual): ver de_leon en PCSX2 (`BuildCookRun ... -platform=PS2 ... -run` sin `-nullrhi`).

### E3 · Cook y datos de PS2 (L) — es GS P6; hecha (falta verla en PCSX2)

- Texturas `PSMT8`/`PSMT4` con CLUT, en potencias de dos y con mips limitados; mallas `LPS2` v2; cook reproducible
  (G5).
- VRAM: 1,7 MB para texturas. Si el conjunto de de_leon no cabe, se suben por frame las texturas visibles por PATH3
  (su coste se mide en el frame), con la decisión tomada según el informe de VRAM del cook.
- El pak se lee de `host:` en desarrollo y de `cdrom0:` en una imagen de disco.

Gate: el cook de PS2 es reproducible, el informe de VRAM está en Budgets.md, y de_leon con las texturas cocinadas
para PS2 se ve igual que la referencia.

Estado:

- `TextureCompressor` (Developer, el homólogo del módulo de UE) convierte RGBA8 a `PF_P8` / `PF_P4` (la paleta y luego
  los índices; `EPixelFormat` gana los dos formatos).
  - Lados en potencias de dos entre 8 y 256, promediando los texels que cubren.
  - Hasta 16 colores da `PSMT4` exacto, hasta 256 `PSMT8` exacto; con más, median cut determinista.
- El cook de PS2 (el formato de textura "Paletted" de `FPS2TargetPlatform`) guarda así las texturas y deja las
  cargadas como estaban. Escribe `<Project>/Saved/Cooked/PS2-VramReport.txt`: las texturas comunes (los assets por
  defecto de la config) y las de cada mapa contra la arena de 1856 KB. El layout sale de `FGSTextureLayout` (GSCore,
  el mismo que usa la caché).
- `FGSTextureCache` sube `PF_P8` / `PF_P4` tal cual: índices `PSMT8` / `PSMT4` y una CLUT `PSMCT32` en CSM1 al final
  de la arena, que TEX0 carga (CLD 1). Las RGBA8 siguen subiendo como `PSMCT32`.
- de_leon: sus materiales no tienen texturas. Las comunes (`T_Default_D`, `DefaultTexture`, `T_Default_Bump_N`) suman
  81 KB de 1856 KB (Budgets.md). El cook de ShooterGame para PS2 es igual byte a byte dos veces seguidas. El juego de
  Linux arrancado desde un pak de ese cook dibuja de_leon como con el contenido de escritorio (12 píxeles distintos,
  los del texto del tiempo).
- Pak: `BuildCookRun -platform=PS2 -stage -pak` mete el cook en `<Project>/Content/Paks/<Project>-PS2.lpak`.
  - Rutas relativas a la carpeta del ELF y entradas alineadas a 2048 bytes (sectores de CD).
  - La raíz de dispositivo (`host:`, `cdrom0:\`) se compara igual con o sin `/` (`FPakFile::NormalizePath`).
  - Si el dispositivo no lista carpetas, el pak se busca por su nombre.
- Tests nuevos:
  - `System.TextureCompressor.Paletted.*` (3).
  - `System.GSCore.TextureLayout`.
  - `System.Renderer.GS.TextureCache.Paletted`.
  - `System.LeonEd.Cook.PalettedTextures`.
  - `System.PakFile.Format.DeviceRootMountPoint`.
- Desviaciones:
  - Mallas `LPS2` v2 no: el renderer lee los datos de malla del motor, y la memoria de de_leon no lo pide. Queda para
    cuando un mapa lo necesite.
  - Sin mips: la caché muestrea el nivel 0.
  - La imagen de disco (`cdrom0:`) no se genera. El pak ya está alineado y se encuentra por nombre, pero una ISO
    necesita nombres ISO 9660 y una herramienta de imagen.
- Pendiente (manual): ShooterGame en PCSX2 con `-pak`.

### E4 · Jugable (M) — hecha en el código; falta medirla en PCSX2

- Mando: el mapeo de ShooterGame (mover, mirar, disparar, recargar, comprar, plantar/desactivar) sobre
  `PS2InputInterface`, con la sensibilidad y la zona muerta en la configuración.
- Menú de compra y HUD usables a 640x448 con el mando.
- Rendimiento (D6): medir el frame en el overlay de estadísticas y en el log, y ajustar hasta 30 fps estables.

Gate: una partida completa de de_leon contra bots en PCSX2 a 30 fps estables, con los tiempos del frame anotados en
Budgets.md.

Estado:

- Mando:
  - `UGameViewportClient` lee el `IInputInterface` de la aplicación (`SetInputInterface`, desde `UGameEngine::Init`),
    como los eventos de mando de Slate en UE. Los cambios de botón llegan como `InputKey` con `bGamepad`, y los sticks
    como una muestra de `InputAxis` por frame (0 incluido, para que el eje se pare).
  - `DefaultInput.ini` de ShooterGame mapea el DualShock: sticks para mover y mirar, R2 dispara, L2 hace zoom,
    Cruz/Círculo/Cuadrado/Triángulo saltan, agachan, recargan y usan, R1/L1/cruceta eligen arma, L3 camina, Select
    muestra la tabla y Start abre el menú de compra.
  - Mirar con el stick va por `TurnRate` / `LookUpRate` a `BaseTurnRate` / `BaseLookUpRate` grados por segundo (150 y
    100, como las plantillas de UE).
- Menú de compra con el mando: mientras está abierto, su propio `UInputComponent` va arriba de la pila
  (`PushInputComponent`). La cruceta mueve la selección (`>` en el HUD), Cruz compra y Círculo cierra, sin que el
  personaje salte ni se agache.
  - Esto arregla un bug previo: `MenuItem1..7` estaba en el componente del controlador y consumía 1, 2 y 4 siempre,
    así que con el menú cerrado esas teclas no elegían arma.
- 30 fps: `SyncInterval=2` en `[/Script/PS2RHI.PS2Settings]` de PS2Engine.ini (el `rhi.SyncInterval` de UE).
  `FPS2RHI::WaitVSync` muestra cada frame como pronto en el segundo blanco vertical desde el anterior.
- Medida: `-LogFrameTimes` escribe cada 5 s la media y el peor frame, y la parte del mundo y la de dibujo y
  presentación. En escritorio (llvmpipe): 12,9 ms de media con diez bots.
- HUD a 640x448: se comprobó en la captura del emulador; el menú de compra y la tabla caben.
- Tests nuevos:
  - `System.Engine.Viewport.Gamepad`.
  - `ShooterGame.Input.Pad`.
  - `ShooterGame.Input.BuyMenuTakesItsKeys`.
- Pendiente (manual): una partida en PCSX2 con el mando, con `-LogFrameTimes`, y sus tiempos en Budgets.md. Si no
  llega a 30 fps, el perfil dirá qué recortar (D6): la transformación en C++ del renderer, la IA o la física.

### E5 · Audio (M) — hecha en el código; falta oírla en PCSX2

- Backend de AudioMixer sobre la SPU2 con `audsrv` (módulos del IOP cargados desde el ELF). Necesita el toolchain del
  IOP, que tiene la imagen de ps2dev.
- Cook de sonidos a ADPCM.

Gate: disparos, pasos y avisos de la bomba suenan en PCSX2 sin cortes y sin bajar de 30 fps.

Estado:

- `FSoftwareAudioMixer` (AudioMixer, todas las plataformas) mezcla en la CPU:
  - Voces PCM16 copiadas, remuestreo lineal a la frecuencia de salida y estéreo de 16 bits.
  - Voces 2D al volumen dado.
  - Voces espaciales con los valores por defecto de miniaudio en escritorio: 1 / distancia en metros pasado 1 m, y
    paneo sobre la derecha del oyente.
  - Tiene tests en el host.
- El `FAudioDevice` de PS2 mezcla a 48 kHz (la frecuencia de la SPU2) y lo envía con `audsrv_play_audio`.
  - Cada `Tick` pone en cola lo que la SPU2 tocó desde el anterior, con 66 ms de adelanto inicial y como mucho 0,1 s
    por tick.
  - Carga `rom0:LIBSD` y `audsrv.irx` desde la carpeta del ELF (`SifExecModuleBuffer`, con el parche LMB de
    libpatches).
  - Si algo falla, avisa en el log y sigue en silencio.
- LeonBuildTool añade `RUNTIME_DEPENDENCIES` (el `RuntimeDependencies` de UE). La extensión de AudioMixer declara
  `$PS2SDK/iop/irx/audsrv.irx`, que el build de Docker copia junto al ELF y BuildCookRun lleva al staging.
- El tono de los avisos de UI (`BuildUiTone`) se comparte entre escritorio y PS2.
- Desviación: sin cook a ADPCM ni voces hardware de la SPU2. Los sonidos de ShooterGame son 242 KB de PCM16, pocos
  para los 32 MB; la mezcla en el EE es un solo camino, probado en el host. ADPCM queda para cuando la memoria lo
  pida.
- Desviación: aquí no se pudo enlazar con `audsrv.irx`, porque el toolchain local no tiene el del IOP. Se comprobó
  que el ELF compila y enlaza con `libaudsrv`, y el build avisa de la IRX que falta.
- Pendiente (manual): oír la partida en PCSX2, comprobar el log (`PS2 audio: audsrv, 48000 Hz stereo mixed on the
  EE`) y que `-LogFrameTimes` sigue en 30 fps.

### E6 · Cierre (S) — hecha (0.21.0)

- Docs (ARCHITECTURE, BUILD, SETUP, TESTING, el README de PS2), recuentos de tests, CHANGELOG y release.

Estado: docs al día.
- ARCHITECTURE, BUILD, TOOLS, TESTING, ASSET_FORMATS, LeonMapping, NextSteps, los README de PS2 y ShooterGame, y
  Budgets.
- TESTING tiene la validación en PCSX2 fase a fase (qué ejecutar, cuándo pasa, qué anotar).
- Recuentos de tests: 407 del motor en Linux (416 en Win64), 44 de ShooterGame, TestPAL 130 (123 en PS2).
- CHANGELOG [0.21.0], `Build.version` 0.21.0 y el contenido del motor y de ShooterGame reguardado.
- Lo que queda es manual: la tabla de validación en PCSX2 de TESTING.

## Verificación por fase

| Fase | Automática (aquí o en `RunTests.bat` / `Package.bat`) | Manual (PCSX2) |
|---|---|---|
| E0 | — | ThirdPerson, `GSConformance` y TestPAL |
| E1 | ShooterGame compila para PS2; tests del motor y de ShooterGame sin cambios; tamaño del ELF | Botmatch headless: estado 0 y reproducible; memoria |
| E2 | La lista de de_leon contra la referencia; botmatch sin cambios en Win64 | de_leon dibujado |
| E3 | Cook reproducible (G5); informe de VRAM | Texturas cocinadas |
| E4 | — | Partida completa a 30 fps |
| E5 | Cook de ADPCM | Audio |
| E6 | `Lint.bat`, `RunTests.bat`, `Package.bat` en verde | — |

## Riesgos

| Riesgo | Mitigación |
|---|---|
| Rendimiento del EE (294 MHz, lógica, IA, física y transformación en C++) | D6: medir desde E1 (headless ya dice cuánto cuesta la lógica) y recortar con datos; la VU1 queda como optimización posterior que debe coincidir con la referencia en C++ |
| Presupuesto de reflexión (400 KB): Engine, AIModule, UMG y ShooterGame añaden código y tablas generadas | Medirlo en E1 con `nm` como en P9; si se pasa, reducir lo reflejado solo para editor antes de subir el límite |
| Memoria (32 MB) con el ELF del motor completo y los paquetes cargados | Budgets.md en cada fase; el pico de escritorio (3,9 MB) incluye un array de objetos de 2 MB que en PS2 es de 96 KB |
| Floats del EE no IEEE (sin denormales ni infinitos): NaN o divergencias en física y movimiento | El botmatch headless de E1 lo destapa pronto; la reproducibilidad se exige en PS2, no igualdad con Win64 |
| Las pruebas en PCSX2 son manuales (necesita BIOS) | Cada gate manual tiene un log o una captura concretos que se piden; lo automático cubre compilación, tamaños, tests en el host y comparación con la referencia |
| Carga lenta desde `host:` o `cdrom0:` | Medir el tiempo de carga de de_leon en E1; un pak comprimido o un orden de lectura secuencial si hace falta |
| El toolchain construido desde las fuentes no es el de la imagen fijada | Los tamaños oficiales se miden con la imagen cuando esté disponible; el de las fuentes sirve para compilar y comparar |
