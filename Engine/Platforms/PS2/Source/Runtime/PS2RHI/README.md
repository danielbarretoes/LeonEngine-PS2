# PS2RHI

PlayStation 2 Graphics Synthesizer RHI (UE: `<Platform>RHI`), a module of the PS2 platform extension.

```cmake
leon_module(PS2RHI
	PLATFORMS PS2
	PUBLIC_DEPENDENCIES Core RHI GSCore
	PUBLIC_SYSTEM_LIBRARIES draw math3d packet graph dma kernel
)
```

It provides two things:

- **`FPS2DynamicRHI`** (private, `PS2DynamicRHI.cpp`): the `FDynamicRHI` implementation returned by
  `PlatformCreateDynamicRHI()`. `FEngineLoop::PreInit` creates it through `RHIInit()` once `FPS2Window` is up, and
  it is published in `GDynamicRHI`. `GetName()` is `"PS2"`; `GetGPUMemoryStats()` reports the VRAM allocated so far against a 4 MB budget
  (shown as `VRAM` in the debug overlay).
- **`FPS2RHI`** (public, `PS2RHI.h`): a static API that games and the engine overlay call directly. Every call appends
  GS register writes to the frame's `FGSCommandList` (GSCore); `WaitVSync` sends the whole frame to the GIF as one
  PATH3 packet (`FGSGifPacket`, PACKED A+D writes and IMAGE transfers) by DMA and waits for the GS's FINISH. The
  Win64 preview and the reference rasterizer consume the same lists ([ps2-gs-parity](../../../../../../Docs/PLANS/ps2-gs-parity.md)).

Include `PS2RHI.h` (it also includes `PS2RHITypes.h` and `PS2Texture.h`) and add `PS2RHI` to the module's
dependencies. Only PS2 targets can depend on it.

## Frame flow

1. `FPS2RHI::InitDisplay(Width, Height)`: two `PSMCT16S` frame buffers (dithered) and a `PSMZ24` Z buffer in VRAM
   (2.2 MB at 640x448), the CRTC, the drawing environment. Done by `FPS2Window::Create()` (640x448 by default); games
   do not call it.
2. `SetViewTarget()` / `SetDirectionalLight()` / `SetAmbientLightColor()`.
3. `ClearColor()` → `BindMaterial()` → `DrawBox()` ... → 2D / `DrawDebugText()` on top.
4. `FPS2RHI::WaitVSync()`: send the frame's list, wait for the vertical blank, show the buffer just drawn and draw the
   next frame into the other. Done by `FPS2Window::SwapBuffers()` at the end of the `FEngineLoop` frame.

The engine loop also calls `BeginDraw3DStatsFrame()` after present (through `FPS2StatsOverlay::MarkFrameStart()`), so
games do not reset the Draw3D counters themselves.

## `FPS2RHI` API

| Group | Functions |
| --- | --- |
| Display | `InitDisplay(Width, Height, ColorFormat = PSMCT16S, ReservedVramBytes = 0)` (`PSMCT32` and reserved VRAM are for GSConformance), `ClearColor(float R, float G, float B)`, `WaitVSync()` |
| Command lists | `Submit(const FGSCommandList&)`: appends a recorded list to the frame and restores the drawing environment after it; `ScreenVertex(X, Y, Z)`: an `XYZ2` at screen coordinates for such a list |
| View / lights | `SetViewTarget(const FPS2ViewTarget&)`, `SetDirectionalLight(const FPS2DirectionalLight&)`, `SetAmbientLightColor(float R, float G, float B)` |
| Material | `BindMaterial(const FPS2Material&)`: used by the following `DrawBox` calls |
| 3D | `DrawBox(LocationX, LocationY, LocationZ, Yaw256, Pitch256, ScaleX, ScaleY, ScaleZ)`: lit / textured box, rotation in 1/256 turns, scale as half-extents; overload with one uniform `Scale` |
| 2D overlays (screen space, origin at the screen centre, no depth test) | `DrawUnlitRectAlpha(X0, Y0, X1, Y1, R, G, B, Alpha)` (blend `(src - dst) * alpha + dst`) |
| Debug text | `DrawDebugText(X, Y, Text, R, G, B, Scale)`: 5x7 glyphs drawn with rects (A-Z, 0-9, a few symbols). `Scale` 1 = 2 px cells (12 px advance, 14 px tall), 0.5 = 1 px cells |
| Draw3D counters | `BeginDraw3DStatsFrame()`, `GetDraw3DStats(FPS2Draw3DStats&)`, `PrintDraw3DStats(const FPS2Draw3DStats&)` (printf to the PCSX2 EE console) |

`DrawBox` pipeline: whole-box frustum cull → per-face backface cull → per-face lighting → per-triangle trivial
reject / homogeneous clip (near plane + guard band) → GS.

## Types

`PS2RHITypes.h`:

| Type | Fields |
| --- | --- |
| `FPS2ViewTarget` | camera: `LocationX/Y/Z`, `Pitch`, `Yaw` (radians) |
| `FPS2DirectionalLight` | sun: `Intensity`, `LightColorR/G/B`, `Yaw256`, `Pitch256` (1/256 turn; high pitch = from above) |
| `EMaterialShadingModel` | `DefaultLit`, `Unlit` |
| `FPS2Material` | `BaseColorR/G/B`, `BaseColorMap` (`const FPS2Texture*`), `ShadingModel`, `bUseFaceAlbedo` (per-face colours when there is no map) |
| `FPS2Draw3DStats` | `Boxes`, `CulledBoxes`, `BackFaces`, `InTris`, `Keep3`, `Drop0`, `Clipped`, `Emitted`, `PacketQwordsPeak` |

`PS2Texture.h`: `FPS2Texture` is a move-only GS VRAM texture (RGBA8 uploaded as `PSMCT32` with the frame, before the
draws that sample it).

| Member | Meaning |
| --- | --- |
| `static Create(Width, Height, const unsigned char* Rgba)` | upload an RGBA8 image |
| `static CreateChecker(int Size = 64)` / `CreateGrid(int Size = 64)` | procedural `T_Checker_D` / `T_Grid_D` |
| `Bind()` | set TEX0 / sampling for the next textured draws |
| `Valid()`, `Destroy()`, `GetWidth()`, `GetHeight()`, `GetVramAddress()` | |

## Files

| File | Role |
| --- | --- |
| `Public/PS2RHI.h` | `FPS2RHI` |
| `Public/PS2RHITypes.h` | view, light, material, stats types |
| `Public/PS2Texture.h` | `FPS2Texture` |
| `Private/PS2RHIModule.cpp` | `IMPLEMENT_MODULE(FDefaultModuleImpl, PS2RHI)` |
| `Private/PS2DynamicRHI.cpp` | `FPS2DynamicRHI`, `PlatformCreateDynamicRHI()`, `InitDisplay`, `ClearColor`, `Submit`, `WaitVSync` |
| `Private/PS2GSContext.h/.cpp` | frame and Z buffers, the frame's command list, the drawing environment, the GIF packet's DMA, VRAM bookkeeping (`Leon::PS2::FPS2GSContext`) |
| `Private/PS2SceneState.h/.cpp` | view target, sun, ambient, bound material and texture (`Leon::PS2::FPS2SceneState`) |
| `Private/PS2Draw3D.cpp` | `DrawBox` (`math3d`), clipping, Draw3D counters |
| `Private/PS2DrawPrimitives.cpp` | `DrawUnlitRectAlpha` |
| `Private/PS2DebugText.cpp` | `DrawDebugText` |
| `Private/PS2Texture.cpp` | `FPS2Texture` upload and bind |

Platform overview: [Engine/Platforms/PS2/README.md](../../../README.md).
