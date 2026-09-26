# Plan: vista previa de Win64 fiel a la PS2 (el GS emulado sobre OpenGL)

## Objetivo

Lo que muestra el renderer OpenGL de Win64 (plataforma de desarrollo y editor) debe ser lo que dibujará la PS2
(plataforma destino), dentro de una tolerancia medida y comprobada por tests. La paridad debe salir **por
construcción** (los mismos datos, el mismo código de transformación e iluminación y el mismo estado del Graphics
Synthesizer), no por un shader que "se parezca" a la PS2.

## Contexto (hechos del código en 0.20.1)

| Área | Win64 (OpenGL, `Engine/Source/Runtime/Renderer`) | PS2 (`Engine/Platforms/PS2/Source/Runtime/PS2RHI`) |
|---|---|---|
| Qué dibuja | La escena del motor (`FScene`, `FSceneRenderer`): mallas estáticas y esqueléticas, HUD, efectos | Solo lo que el juego pide a `FPS2RHI` en modo inmediato: cajas (`DrawBox`), rectángulos 2D y texto de depuración. `DrawCookedMesh` valida un blob y dibuja un triángulo de relleno |
| Motor | `WITH_ENGINE=1`: `UWorld`, actores, `FScene` | `WITH_ENGINE=0`: el módulo `Engine` no corre en PS2 |
| Transformación | Shaders GLSL en la GPU | EE con `math3d`, por cara |
| Iluminación | Blinn-Phong por píxel, mapas de normales, reflejo de un cielo procedural, especular | Por cara: sol más ambiente |
| Sombras | Shadow map con PCF (comparación de profundidad) | Ninguna |
| Otros | Espejo planar, MSAA (`GL_MULTISAMPLE`), skinning en GPU, mipmaps trilineales (`GL_LINEAR_MIPMAP_LINEAR`), resolución de la ventana | 640x448, z-buffer de 32 bits, `PSMCT32` |
| Texturas | RGBA8 del cook de Win64 | RGBA8 subido como `PSMCT32`; el cook de PS2 es un stub (sin `PSMT8`/`PSMT4` ni `LPS2`) |

Consecuencia: hoy nada garantiza la paridad. El GL muestra cosas que el GS no puede hacer: iluminación por píxel, mapas
de normales, shadow maps, MSAA, texturas sin paleta y resolución libre. Y la PS2 no dibuja la escena del motor, así
que no hay nada con qué comparar.

## Decisiones

| # | Decisión | Por qué | Alternativa descartada |
|---|---|---|---|
| D1 | Un nivel común **GS command list** (`FGSCommandList`, inspirado en `FRHICommandList` de UE): primitivas ya transformadas e iluminadas y los registros del GS que las dibujan (`PRIM`, `RGBAQ`, `ST`/`UV`, `XYZ2`, `TEX0`, `TEX1`, `CLAMP`, `ALPHA`, `TEST`, `FOG`/`FOGCOL`, `ZBUF`, `FRAME`, `SCISSOR`, `DTHE`, `COLCLAMP`). El renderer de escena genera la lista; dos backends la consumen: **PS2** (paquetes GIF por DMA) y **GL** (emulador del GS) | La paridad sale por construcción: los dos backends reciben exactamente lo mismo, y el GL solo rasteriza | Dos renderers con un "perfil PS2" en los shaders: la paridad sería por convención y se degrada con cada cambio |
| D2 | La transformación, el clipping (plano cercano y guard band), el skinning y la iluminación por vértice son **C++ compartido** (la "referencia VU1"): corre en el EE y en el PC | El mismo código produce los mismos vértices en ambas plataformas | La transformación en shaders GLSL: divergiría del EE |
| D3 | La vista previa carga los **formatos cocinados para PS2** (texturas `PSMT8`/`PSMT4` con CLUT, potencias de dos, mips limitados; mallas `LPS2` v2 cuantizadas), cocinados en memoria o en una caché `Saved/Cooked/PS2` | Los colores y el detalle que se ven son los que tendrá la PS2 (la cuantización a paleta es la mayor diferencia visual) | Seguir con RGBA8 en Win64: los colores no coincidirían |
| D4 | Framebuffer emulado a **640x448** (`PS2Engine.ini`), con el formato de color y de z del GS; se presenta escalado por un entero con filtro nearest. No se emula el entrelazado ni el filtro de parpadeo | Mismos píxeles que el GS. El entrelazado es cosa del CRT, no del render | Renderizar a la resolución de la ventana |
| D5 | Features: solo las que el GS puede hacer (ver tabla siguiente). Lo demás se borra del GL, como se hizo con el post-proceso en 0.20.1 | La regla del proyecto: "si no es válido en PS2, no se necesita" | Conservar features de PC bajo un toggle |
| D6 | Oráculo de CI: un **rasterizador de referencia del GS por software** (`FGSReferenceRasterizer`, C++ determinista, en un módulo Developer) implementado según `Docs/PS2OFFICIAL/GS_Users_Manual.pdf`: reglas de rasterizado, subpíxel 12.4, muestreo, CLUT, mezcla, test de alfa y de z, dithering. El GL se compara contra él con tolerancia; la referencia se valida a mano contra capturas de PCSX2 | PCSX2 no puede correr en CI (necesita BIOS) y comparar GL contra GL no demuestra nada | Incluir código del GS de PCSX2: GPL-3.0, incompatible con incluirlo en el motor |

### Qué se conserva, qué cambia y qué se borra en la vista previa

| Feature del GL actual | En el GS | Resultado |
|---|---|---|
| Iluminación por píxel (Blinn-Phong), especular | No hay etapa de píxel programable | Iluminación **por vértice** (un sol, ambiente y hasta N luces puntuales por objeto) calculada en C++ (D2); el GS la interpola con Gouraud y la modula con la textura (regla de modulado: 0x80 = 1.0) |
| Mapas de normales | Solo con trucos multipasada (`Docs/PS2OFFICIAL/ps2_normalmapping.pdf`), demasiado caros para un FPS | Se borra |
| Shadow map con PCF | No hay comparación de profundidad en texturas | Se borra. Se sustituye por iluminación estática horneada en color de vértice para el mapa (importador) y sombra proyectada o blob para personajes (decisión pendiente P0) |
| Espejo planar | Posible (renderizar a textura en VRAM), pero duplica la escena | Se borra (decisión pendiente P0) |
| Reflejo del cielo | Sin mapas de entorno por píxel | Se borra |
| MSAA | El GS no tiene MSAA (solo AA1 en líneas) | Se borra |
| Mipmaps trilineales | El GS elige el LOD con `TEX1` (K, L, MXL) y filtra bilineal o trilineal limitado | Se emula la fórmula de LOD del GS |
| Texturas RGBA8 | `PSMT8`/`PSMT4` con CLUT de `PSMCT32` o `PSMCT16` | Muestreo con paleta en el shader: índice, color de la CLUT y bilineal sobre los colores ya buscados, como hace el GS |
| Mezcla alfa | `(A - B) * C + D` con A, B, D en {Cs, Cd, 0} y C en {As, Ad, FIX}; As = 0x80 es 1.0 (hasta 2.0) | Subconjunto soportado: las combinaciones que GL expresa exactamente (alfa estándar, aditivo, alfa constante); el resto queda prohibido por un check del command list |
| Decals con `glPolygonOffset` | El GS no tiene polygon offset | Sesgo de z en el vértice |
| Tracers aditivos, HUD, texto | Sprites y mezcla del GS | Se conservan, emitidos como primitivas GS |
| Skinning en GPU | VU1 o EE | Skinning en C++ (D2) |
| Niebla | Por vértice (`FOG` y `FOGCOL`) | Se añade (el GS la tiene gratis) |

### Presupuesto de VRAM que condiciona el framebuffer

- 640x448 a 32 bits ocupa 1,09 MB.
  - Con doble buffer y z de 32 bits: 3,28 MB, y quedan 0,72 MB para texturas.
- Con color de 16 bits (`PSMCT16S`, con dithering) y z de 24 bits: 2,29 MB, y quedan 1,7 MB para texturas.

La recomendación es la segunda configuración, porque un FPS con mapa texturizado necesita la VRAM. Es la decisión P0-3
y se aplica igual en los dos backends.

## Fases

Los tamaños siguen la convención del plan anterior (S, M, L, XL).

### P0 · Decisiones abiertas (S)

1. Sombras de personajes: proyectada (un render de la silueta a textura, recomendada) o blob.
2. Espejo planar: borrar (recomendado) o conservar como render a textura.
3. Formato del framebuffer: 16 bits con dithering y z de 24 (recomendado) o 32 bits.

Hecho cuando: las tres respuestas quedan escritas en este plan.

### P1 · Contrato del GS (M)

- Módulo compartido (`GSCore`, en todas las plataformas, sin dependencias fuera de Core) con:
  - `GSTypes.h`: los registros y formatos de D1 como tipos C++ con los campos del manual.
  - `FGSCommandList`: la lista, que valida que las combinaciones del subconjunto soportado son legales.
  - `FGSVertex`: XYZ en 12.4 más el offset de ventana, RGBAQ, ST o UV y fog.
- Tests de codificación contra el manual: bits de cada registro y conversiones de formato.

Gate: un test por registro.

### P2 · Rasterizador de referencia (M-L)

- `FGSReferenceRasterizer` (módulo Developer, solo host), un modelo de la VRAM a nivel de píxel. No emula el
  swizzle de páginas: basta con tener direcciones lógicas.
- Tests de reglas: cobertura top-left, subpíxel, perspectiva con STQ, CLUT de 4 y 8 bits, LOD, las mezclas del
  subconjunto, `ATST`/`AFAIL`, z-test y z-write, dithering 4x4 (`DIMX`) y `COLCLAMP`.
- Programa PS2 `GSConformance`: dibuja las mismas listas de comandos que los tests (escenas de conformidad) y CI lo
  compila.
- Validación manual: capturas de PCSX2 de esas escenas, guardadas como PNG de 640x448 en fixtures, que la referencia
  debe reproducir dentro de la tolerancia.

Gate: la referencia coincide con las capturas de PCSX2.

### P3 · Backend PS2 (M)

- `FPS2RHI` ejecuta un `FGSCommandList` en paquetes GIF por PATH3 (con `packet` y `dma` de PS2SDK).
- `DrawBox` y ThirdPerson se reescriben sobre él, y la API inmediata que queda sin uso se borra.

Gate: el ELF de ThirdPerson entra en su presupuesto (G3) y las escenas de conformidad se ven igual que en P2.

### P4 · Backend GL, el emulador del GS (L)

- Un FBO de 640x448 con el formato de P0-3. Texturas de índices (`R8UI`) más una textura de paleta, con el muestreo y
  el bilineal sobre colores hechos en el shader.
- Mezcla por la función fija cuando es exacta. Dithering, `COLCLAMP`, niebla y alfa test en el shader.
- Presentación escalada por un entero con filtro nearest.
- **Primera prueba de extremo a extremo:** ThirdPerson compila también para Win64 sobre este backend, y su frame debe
  coincidir con la captura del mismo frame en PCSX2.
- CI:
  - GL contra la referencia en el job `win64`, con un GL por software fijado (Mesa llvmpipe para Windows, descargado
    y verificado en el paso Setup), porque los runners no tienen GPU.
  - Tolerancia medida por píxel, más una cota del número de píxeles distintos.

Gate: GL contra la referencia dentro de la tolerancia en todas las escenas de conformidad.

### P5 · Renderer de escena sobre el command list (L)

- `FSceneRenderer` pasa a generar un `FGSCommandList`:
  - Culling por frustum y la transformación, el clipping, el skinning y la iluminación de D2.
  - Orden opaco y translúcido; view model; HUD, canvas y líneas de depuración como primitivas GS.
- Se borra lo que D5 excluye: shaders por píxel, mapas de normales, shadow maps, espejo planar, reflejo del cielo, MSAA
  y skinning en GPU.
- Los tests de golden view y el render de de_leon pasan a comparar contra la referencia.
- La iluminación horneada en color de vértice llega con el importador de mapas (LeonEd).

Gate: de_leon en la vista previa y en la referencia coinciden, y los tests del motor pasan.

### P6 · El cook de PS2 de verdad (L)

- Texturas: `PSMT8` y `PSMT4` con CLUT, con cuantización determinista (median cut con orden fijo), redimensionado a
  potencias de dos, cadena de mips limitada e informe de VRAM por mapa.
- Mallas: `LPS2` v2 (tiras, posiciones cuantizadas, color de vértice), y `DrawCookedMesh` las dibuja de verdad.
- La vista previa de Win64 carga el cook de PS2 (D3).
- G5 (reimportación estable) cubre también la salida de PS2.

Gate: el cook es reproducible byte a byte, y el informe de VRAM de de_leon cabe en el presupuesto de P0-3.

### P7 · Validación, documentación y release (M)

- Nuevo gate G8, **paridad GS**: GL contra la referencia en CI, más un procedimiento manual de captura en PCSX2
  documentado en `Docs/TESTING.md` con fixtures versionados.
- Documentación: ARCHITECTURE (el nivel GS), LeonMapping (`FGSCommandList` frente a `FRHICommandList`), Budgets
  (VRAM y paquetes) y CHANGELOG.

Fuera de este plan (siguiente plan): **el motor en PS2** (`WITH_ENGINE=1` en el EE: `UWorld`, `FScene` y ShooterGame
sobre este renderer). Este plan deja listo el contrato para que, cuando el motor llegue al EE, lo que dibuje sea lo que
ya se validó en Win64.

## Verificación por fase

| Fase | Comprobación |
|---|---|
| P1 | Tests de codificación de registros (TestPAL en Win64 y PS2) |
| P2 | Tests de reglas contra la referencia; comparación manual con PCSX2 |
| P3 | CI compila `GSConformance` y ThirdPerson para PS2; G3 |
| P4 | CI compara el GL con la referencia (llvmpipe); ThirdPerson en Win64 frente a la captura de PCSX2 |
| P5 | Tests del motor y de ShooterGame, de_leon contra la referencia, botmatch (sin cambios) |
| P6 | Cook reproducible (G5) e informe de VRAM |
| P7 | Todo lo anterior en CI verde y release |

## Riesgos

| Riesgo | Mitigación |
|---|---|
| GL no expresa algunas mezclas del GS (las que usan Cd como factor o As > 1.0) | Un subconjunto soportado, validado por el command list; ampliarlo exige un camino de framebuffer fetch o ping-pong, y se decide con datos |
| El LOD de mip del GS no coincide con el de GL | Emular la fórmula del GS en el shader y seleccionar el nivel a mano |
| Los floats del EE no son IEEE (sin denormales ni infinitos) | Tolerancia de subpíxel en las comparaciones; la referencia usa la aritmética de 12.4 del GS después de la transformación |
| Capturas de PCSX2 solo manuales (BIOS) | Son fixtures versionados; el oráculo de CI es la referencia por software |
| Mesa llvmpipe en el runner de Windows | Versión fijada con hash en el paso Setup (como el resto de descargas de terceros) |
| El rendimiento del EE al transformar en C++ | Fuera del alcance de la paridad; la VU1 llega después y debe coincidir con la referencia C++ de D2 |
