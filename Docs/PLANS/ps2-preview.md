# Plan: el PC reproduce las condiciones de la PS2

## Objetivo

Leon sigue la arquitectura de UE 4.27 y su plataforma objetivo es la PS2. El juego en Windows (y en Linux), el export
de Win64 y el futuro visor del editor deben verse, oírse, controlarse y costar lo que cuesta en la consola. Así, lo que
se prueba en el PC es lo que se juega en la PS2, y las diferencias que queden están medidas y documentadas.

## Contexto (hechos del código en 0.21.0)

| Área | PC hoy | PS2 | Diferencia |
|---|---|---|---|
| Imagen | El emulador del GS dibuja el mismo frame de 640x448 que la PS2 ([ps2-gs-parity](ps2-gs-parity.md) P4) | El GS | Ninguna en el frame |
| Aspecto | La proyección usa 640/448 = 1,43 (`UGameViewportClient::Draw`) y el PC muestra píxeles cuadrados | La TV muestra el frame a 4:3: píxeles un 7 % más estrechos | En la PS2 la escena sale aplastada, y el PC no enseña lo que enseña la TV |
| Mando | GLFW no lee mandos: teclado y ratón | DualShock por libpad: ejes de 8 bits y zona muerta de 0,18 | En el PC no se juega como en la consola |
| Ritmo | Tantos fps como dé el PC (12 ms) | `SyncInterval=2`: 30 fps fijos | Otro DeltaTime, otra sensación |
| Audio | miniaudio con su propia atenuación y sus voces | `FSoftwareAudioMixer` a 48 kHz, 24 voces, a la SPU2 | Otra mezcla, otra atenuación |
| Texturas | RGBA8 desde el contenido sin cocinar y en el cook de Win64 | Paleta de 8 o 4 bits (E3), 256x256 como máximo | Otros colores y otra resolución |
| Memoria | Sin límite | 32 MB de RAM, 4 MB de VRAM, 2 MB de SPU2 | El PC no avisa |
| Coste | Sin medida de lo que costaría | Un EE de 294 MHz | El PC no dice si la PS2 llega a 30 fps |
| Carga | Instantánea | Lector de DVD: ~4 MB/s, ~100 ms por búsqueda | El PC no enseña las esperas |
| Floats | IEEE 754 | Sin denormales, sin inf/NaN, redondeo hacia cero | La jugabilidad puede divergir |

## Decisiones

| # | Decisión | Por qué | Alternativa descartada |
|---|---|---|---|
| D1 | **Las condiciones de la PS2 son el comportamiento por defecto** en todas las plataformas, en la config base (`BaseEngine.ini`). Lo nativo del PC es la excepción para depurar (`-benchmark` ya quita el ritmo) | El producto es un juego de PS2; un modo aparte acaba sin usarse y deja de estar probado | Un modo de vista previa opcional |
| D2 | **Sin sistema de device profiles mientras haya un solo perfil**: los valores van en las secciones de siempre (`[/Script/Engine.RendererSettings]` como en UE, con sus nombres de UE donde existen: `SyncInterval` es `rhi.SyncInterval`) | Un DeviceProfiles.ini con un único perfil es una capa sin uso; se introduce cuando haya un segundo (por ejemplo, un "PC nativo" para herramientas) | Portar los Device Profiles de UE ahora |
| D3 | **La emulación vive en los backends y en las capas de plataforma**, detrás de las interfaces que ya existen (`IInputInterface`, `IRendererModule`, la salida de audio, `IPlatformFile`), nunca con `#if PLATFORM_PS2` en la lógica | La regla D1 del plan del motor: un solo código de juego | Ramas por plataforma en Engine |
| D4 | **Cuando PC y PS2 difieren, cambia el PC** para parecerse a la consola (el aspecto, la mezcla, las texturas), salvo que la PS2 tenga un bug, que se arregla en los dos | La consola es la referencia | Converger "a medio camino" |
| D5 | **Lo que no puede ser exacto se mide y se declara**: cada aproximación (floats, coste, carga) lleva su tolerancia y un test o un procedimiento que la compruebe | Sin medida, "parecido" no significa nada | Aproximaciones sin verificar |
| D6 | **Las constantes de coste y de carga se calibran con PCSX2** (con el renderer software, y a ser posible con una PS2 real) antes de fijarse; hasta entonces el overlay dice que son estimaciones | PCSX2 no es exacto en ciclos | Constantes tomadas de la documentación |

## Fases

### V1 · Lo que se ve, se oye y se toca (M) — hecha

- **Aspecto 4:3**: `[/Script/Engine.RendererSettings] DisplayAspectRatio=1.333333` (el de la TV; 16:9 para un juego
  anamórfico). `IRendererModule::GetDisplayAspectRatio` lo da (el aspecto del frame para un renderer sin TV), la
  proyección del visor lo usa, y el emulador presenta el frame con ese aspecto: escalado entero en vertical (las
  líneas) y lineal en horizontal (la señal analógica de una línea de TV).
- **Mando en el PC**: `FGLFWInputInterface` lee el primer mando de GLFW (el mapeo estándar: Xbox, DualShock 4, etc.)
  con el esquema del DualShock (Cruz = A, Círculo = B, ...; L2 / R2 digitales como los reporta libpad). Los ejes pasan
  por el mismo modelo que la PS2 (`FDualShockAnalog`: cuantizados a 8 bits y con la zona muerta de 0,18), compartido
  con `FPS2InputInterface`.
- **Ritmo**: `[/Script/Engine.RendererSettings] SyncInterval=2` también en el PC: el renderer de escritorio retiene cada
  frame hasta el segundo campo de 59,94 Hz desde el anterior, como `FPS2RHI::WaitVSync`. `-benchmark` no espera. La PS2
  lo lee de la misma sección (deja `PS2Settings`).
- **Audio**: un solo `FAudioDevice` sobre `FSoftwareAudioMixer` en todas las plataformas; lo específico es la salida
  (`FAudioOutput`): la PS2 empuja a audsrv, el PC a un dispositivo de miniaudio a 48 kHz a través de su buffer circular
  sin bloqueos (`ma_pcm_rb`). La mezcla se hace en el hilo del juego en los dos, con el mismo adelanto (66 ms).
- **Texturas**: el cook de Win64 hace las texturas de paleta como el de PS2 (el export de Windows lleva los datos de la
  consola), y el renderer del PC convierte con el mismo `FPalettedTextureBuilder` las texturas RGBA8 del contenido sin
  cocinar la primera vez que las dibuja (lo que hace el editor de UE con la plataforma de vista previa).

Gate: tests del aspecto, del modelo del DualShock, del ritmo, del audio (la salida recibe lo que mezcla el mezclador)
y de la conversión de texturas; una captura del PC con contenido sin cocinar igual a la del pak de PS2; los tests y el
botmatch sin cambios.

Estado:

- **Aspecto:**
  - Implementación: `FRendererSettings` (Renderer, todas las plataformas) lee `DisplayAspectRatio` y `SyncInterval` de
    `[/Script/Engine.RendererSettings]`. `IRendererModule::GetDisplayAspectRatio` los expone, y
    `UGameViewportClient::Draw` proyecta con 4:3.
  - `FGSOpenGLEmulator::Present` escala las líneas por un entero y estira cada línea en horizontal. En 1280x896 el
    frame ocupa 1195x896.
  - En la PS2 la escena deja de salir aplastada.
- **Mando:**
  - `FDualShockAnalog` (ApplicationCore, público) tiene los bytes de libpad y la zona muerta; lo usan
    `FPS2InputInterface` y `FGLFWInputInterface`.
  - `FGLFWInputInterface` lee el primer mando estándar de GLFW, y `FGLFWApplication` lo sondea.
- **Ritmo:** `FFramePacer` retiene el frame del escritorio hasta el campo que toca (con 30 fps, 34,1 ms de media en
  ShooterGame incluida la carga). La PS2 lee `SyncInterval` de la misma sección, que sale de `PS2Settings`.
  `-benchmark` no espera.
- **Audio:**
  - Un solo `FAudioDevice` sobre `FSoftwareAudioMixer`; la salida es `FAudioOutput`.
  - En PS2 es `FPS2AudioOutput` (audsrv); en escritorio, `FMiniAudioOutput`: un dispositivo de miniaudio con
    `ma_pcm_rb`, sin locks.
  - Desaparecen la atenuación y el paneo propios de miniaudio.
- **Texturas:**
  - El cook de Win64 hace las texturas de paleta.
  - `FGSTextureCache::SetTextureConverter`: el escritorio convierte con `ConvertTextureAsPS2Cook` las texturas RGBA8
    del contenido sin cocinar, una vez por textura.
  - Una captura de de_leon sin cocinar solo difiere de la del pak de PS2 en los 44 píxeles del reloj del HUD.
- **Tests nuevos:**
  - `System.Renderer.PS2Preview.*` (4).
  - `System.ApplicationCore.DualShock.Analog`.
  - `System.ApplicationCore.Desktop.GamepadAsDualShock`.
  - `System.AudioMixer.Device.QueuesTheMix`.
  - Linux: 414 del motor; ShooterGame 44; TestPAL 130; el botmatch sin cambios.
- **Desviación:** el renderer del escritorio enlaza TextureCompressor (Developer) también en Shipping, aunque el
  contenido cocinado nunca lo usa. UE no deja módulos Developer en Shipping. Se revisará si LeonBuildTool añade
  dependencias por configuración.
- **Pendiente (manual):**
  - Oír el audio del PC con un dispositivo real (aquí no hay tarjeta de sonido).
  - Jugar con un mando en Windows.

### V2 · Presupuestos (M)

- **Memoria**: en el PC, `GMalloc` sobre una arena del tamaño de la RAM que la PS2 deja al juego (32 MB menos el ELF y
  el kernel), que falla igual que en la consola; `-NoMemoryLimit` para herramientas.
- **Informe de RAM por mapa en el cook**, junto al de VRAM: los paquetes que carga cada mapa, cocinados.
- **Modelo de coste** en el overlay de estadísticas y en `-LogFrameTimes`: vértices transformados, escrituras al GS,
  bytes de GIF, subidas de textura, voces, ticks de IA y física, convertidos en milisegundos estimados del EE y del GS
  con constantes calibradas (D6). Aviso por encima de 33 ms.

Gate: el pico de memoria de una partida en el PC coincide con el de la PS2 (Budgets.md); la estimación de coste está
dentro de un 20 % de lo medido en PCSX2 en de_leon.

### V3 · Fidelidad fina (M)

- **Carga**: una capa de `IPlatformFile` que simula el lector de DVD (búsqueda y ancho de banda, calibrados) en el PC.
- **Floats**: el PC arranca con flush-to-zero, denormals-are-zero y redondeo hacia cero (lo que el EE hace; inf/NaN
  siguen siendo IEEE). Un test compara la traza de un botmatch del PC con la de la PS2 y declara la divergencia.
- **Salida de TV**: opcional, el entrelazado y el filtro de parpadeo del CRTC al presentar.

Gate: los tiempos de carga de de_leon en el PC dentro de un 20 % de los de PCSX2; la traza del botmatch documentada.

### V4 · Editor (L)

- El visor del futuro editor dibuja con el emulador del GS y los valores de V1, con overlays de presupuesto (VRAM de la
  arena, triángulos, escrituras al GS, milisegundos estimados).
- Avisos al importar: texturas que pierden colores en la paleta, mallas y sonidos por encima del presupuesto.

Gate: el editor enseña lo que la captura de PCSX2.

### V5 · Validación continua (S)

- Capturas de PCSX2 por mapa como fixtures (`Engine/Platforms/PS2/Documentation/Captures/`) comparadas con las del PC.
- Las constantes de V2 y V3 recalibradas cuando cambie el motor de forma notable.

## Verificación por fase

| Fase | Automática | Manual (PCSX2) |
|---|---|---|
| V1 | Tests de aspecto, DualShock, ritmo, audio y texturas; captura PC sin cocinar = pak de PS2 | La misma escena en la TV y en el PC con el mismo aspecto; el mando se siente igual |
| V2 | Arena de memoria; informe de RAM | Calibración del modelo de coste |
| V3 | Traza del botmatch | Tiempos de carga |
| V4 | — | El editor frente a la captura |
| V5 | Capturas comparadas | Capturas nuevas |

## Riesgos

| Riesgo | Mitigación |
|---|---|
| Los mandos del PC tienen otro recorrido y otra zona muerta física que el DualShock | El modelo cuantiza a 8 bits y aplica la zona muerta de la PS2 sobre la señal del mando; se ajusta con la calibración de V2 |
| El PC con ritmo de 30 fps hace más lentos los tests con ventana y las capturas | `-benchmark` no espera; las capturas desatendidas siguen dando el mismo frame |
| La conversión de texturas al vuelo en el PC tarda con texturas grandes | Solo en el contenido sin cocinar, una vez por textura; el export de Win64 ya viene convertido |
| Un modelo de coste mal calibrado da confianza falsa | D6: calibrado con PCSX2 y marcado como estimación hasta entonces |
