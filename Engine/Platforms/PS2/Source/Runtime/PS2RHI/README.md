# PS2RHI

PlayStation 2 Graphics Synthesizer RHI (UE: `<Platform>RHI`), a module of the PS2 platform extension.

```cmake
leon_module(PS2RHI
	PLATFORMS PS2
	PUBLIC_DEPENDENCIES Core RHI GSCore
	PUBLIC_SYSTEM_LIBRARIES draw graph dma kernel
)
```

It provides three things:

- **`FPS2DynamicRHI`** (private, `PS2DynamicRHI.cpp`): the `FDynamicRHI` implementation returned by
  `PlatformCreateDynamicRHI()`. `FEngineLoop::PreInit` creates it through `RHIInit()` once `FPS2Window` is up, and
  it is published in `GDynamicRHI`. `GetName()` is `"PS2"`; `GetGPUMemoryStats()` reports the VRAM allocated so far
  against a 4 MB budget (shown as `VRAM` in the debug overlay).
- **`FPS2RHI`** (public, `PS2RHI.h`): the display and the frame. What is drawn is recorded in `FGSCommandList`s (GSCore)
  against the frame's drawing environment and appended with `Submit`: the Renderer's scene and canvas
  (`FPS2RendererModule`), and `FGSDebugDraw`'s text and rectangles (the error screen, GSConformance). `WaitVSync`
  sends the whole frame to VIF1 as one DMA source chain (`FGSGifPacket::BuildChain`: the GS writes in PACKED A+D and
  IMAGE transfers by DIRECT, PATH2; the vertex batches to VU1, PATH1), double buffered: it does not wait for the transfer, and the frame is shown at the next
  `WaitVSync` once the GS's FINISH says it is drawn. The Win64 preview and the reference rasterizer consume the same lists
  ([ps2-gs-parity](../../../../../../Docs/PLANS/ps2-gs-parity.md)).
- **`FPS2VU1`** (public, `PS2VU1.h`) and the microprograms (`Private/VU1/VU1Programs.vsm` and `Skinned.vsm`, assembled by `dvp-as` in
  the PS2 build: [ps2-shipping](../../../../../../Docs/PLANS/ps2-shipping.md) N14): `UploadPrograms` (MPG, at
  `InitDisplay`), `IsEnabled` (not with `-novu1`) and `RunBatchForTest` (one batch without XGKICK, its GIF packet read
  back from VU1's memory: VU1Conformance). `FPS2VU1BatchEncoder` (`Private/PS2VU1Encoder.h`) writes each vertex batch
  into the frame's chain: its header in a CNT (UNPACK V4-32 at TOPS), its four streams by REF where the mesh keeps them
  (UNPACK V3-16, V4-8, V4-8 unsigned, V2-16) and MSCAL of StaticUnlit or StaticLit; a skinned batch's longer header,
  its palette by REF (UNPACK V4-32) and its fifth stream, the skin (V4-8 unsigned), then SkinnedUnlit or SkinnedLit.

### VU1 memory

| Quadwords | What |
| --- | --- |
| 0-2 | shared: the screen's scale and offset (clip space to the GS's 12.4 pixels and Z / 16; the scale's w 16, XYZF2's Z shift), the limits (255, 0.5, Z max / 16, 2048: XYZF2's ADC after FTOI4) |
| 16 (BASE), 520 (BASE + OFFSET 504) | the double buffer, a batch in each: |
| +0 | ints: vertices, XGKICK, the lighting (0 unlit, 1 lit, 2 or 3 lit with one or two point lights, N29) |
| +1-4 | the quantized position to clip space (the mesh's scale and bias folded into LocalToClip) |
| +5-8 | the colour scale (the material's colour to RGBAQ's bytes), the UV offset and scale with the fog's scale and offset in 7's zw (N15: F = offset + scale x w; 0 and 255 without fog), the GIFtag (NLOOP, EOP, PRE and the draw's PRIM with FGE, PACKED ST RGBAQ XYZF2) |
| +9-14 | StaticLit: the normal to the world (rows), the sun's reversed direction and colour, the ambient share |
| +15-18 | StaticLit with point lights (N29): the quantized position to the world (the scale and bias folded into LocalToWorld) |
| +19-22 | the point lights: position with 1 / range in w, colour (two at most) |
| +24, +88, +152, +216 | the positions, normals with the strip flags, baked colours and texture coordinates (64 at most) |
| +280 | the GIF packet VU1 builds and kicks: the GIFtag, then ST, RGBAQ and XYZF2 a vertex (Z at bit 4 of its third word, F at bit 4 of its fourth, ADC its bit 15 where no triangle is drawn) |

A skinned batch's buffer (`VU1SkinnedMemory`, `Skinned.vsm`):

| Quadwords | What |
| --- | --- |
| +0-22 | as a static batch's, but +1-4 is LocalToClip and +15-18 LocalToWorld themselves (the program dequantizes and poses the position first) |
| +23, +24 | the quantization's scale (W: the weights' 1/255) and bias |
| +25 | the palette: 3 quadwords a bone (`FGSSkinMatrix`), 24 bones at most |
| +97, +145, +193, +241, +289 | the positions, normals with the strip flags, colours, texture coordinates and skin (48 at most) |
| +337 | the GIF packet |

### PATH1 and PATH2

PATH3 is not used: the GIF takes one path at a time and keeps it until the path's packet ends, and one VIF1 chain keeps
the frame in its order. The CPU's writes go by DIRECT (PATH2); a DIRECT section before a batch ends its GIF packet
(EOP) so that VU1's XGKICK (PATH1) can take the GIF, and the first DIRECT after batches starts with a FLUSH (the
microprograms ended and their PATH1 packets sent), so the writes after a batch reach the GS after it. The batches
themselves overlap: VIF1 unpacks batch N + 1 into the other buffer while VU1 runs batch N, and its XGKICK waits for
the packet before. An upload's IMAGE packet goes across two DIRECTs (its GIFtag in the CNT, its pixels by REF); no
batch runs between them. `InitDisplay` resets the GIF (GIF_CTRL.RST): a PATH3 packet left open before the ELF would
otherwise hold it and stall every DIRECT.

A skinned batch's palette is in the frame list's own memory (`FGSCommandList::AllocateSkinPalette`, copied there by
`Submit`'s `Append`, once for the batches that share it), which lives until the list's `Reset` after the frame's chain
has retired.

The chain reads the meshes' LPS2 blobs in place a frame after they are recorded, so the frames are numbered
(`FPS2GSContext::RecordingFrame`, `PendingFrame`, `CompletedFrame`; `BeginNextFrame` after each kick or drop,
`RetirePendingFrame` when the GS's FINISH comes) and handed to `FRHIDeferredRelease` (RHI): a blob a mesh gives up
while frame N is recorded is freed once N has completed, without a wait. A cooked texture's levels, held in place by
`UploadImageInPlace` (N23), go the other way: `RetireInPlaceImages` copies them into the recording list and waits for
the pending frame's VIF1 DMA before the texture frees them.

Only PS2 targets can depend on it; nothing draws on the GS except through a command list (the immediate `DrawBox` /
`BindMaterial` / `FPS2Texture` path and ThirdPerson went in [ps2-shipping](../../../../../../Docs/PLANS/ps2-shipping.md)
N2).

## Frame flow

1. `FPS2RHI::InitDisplay(Width, Height)`: two `PSMCT16S` frame buffers (dithered) and a `PSMZ24` Z buffer in VRAM
   (2.2 MB at 640x448), the CRTC in the console's television mode (NTSC, or PAL with the frame centred), the vertical
   blank interrupt handler, the drawing environment. Done by `FPS2Window::Create()` (640x448 by default).
2. `ClearColor()`, then `Submit()` the frame's lists, recorded against `GetDrawEnvironment()` (its `PixelVertex` takes
   pixels from the frame's top left).
3. `FPS2RHI::WaitVSync()`: build the frame's DMA chain (`FGSGifPacket::BuildChain` into the frame's buffer, through
   the uncached accelerated segment, the uploads and the meshes' streams by REF, the vertex batches through
   `FPS2VU1BatchEncoder`); wait for the GS to finish the frame before (CSR.FINISH);
   sleep until the vertical blank the sync interval asks for (`FGSFieldPacer` on the field count of
   `FPS2VerticalBlank`'s interrupt handler, a semaphore wait) and show that frame; write the data cache back (`FlushCache`), kick this frame's
   chain on the VIF1 channel (the tags' VIFcodes: TTE) and return, recording the next frame into the other list, chain and frame buffer while the GS draws this
   one. Done by `FPS2Window::SwapBuffers()` at the end of the `FEngineLoop` frame.
4. `FPS2RHI::ShutdownDisplay()` (`FPS2Window::Destroy()`): the handler goes, then the CRTC.

## `FPS2RHI` API

| Group | Functions |
| --- | --- |
| Display | `InitDisplay(Width, Height, ColorFormat = PSMCT16S, ReservedVramBytes = 0)` (`PSMCT32` and reserved VRAM are for GSConformance), `ShutdownDisplay()`, `ClearColor(float R, float G, float B)`, `WaitVSync()`, `SetSyncInterval(Interval)` (fields: 2 is 30 fps on NTSC, 25 on PAL) |
| Command lists | `Submit(const FGSCommandList&)`: appends a recorded list to the frame and restores the drawing environment after it; `GetDrawEnvironment()` |
| Textures | `AllocateTextureArena(OutFirstBlock, OutNumBlocks)`: the VRAM the display leaves, once, for the Renderer's texture cache |

## Files

| File | Role |
| --- | --- |
| `Public/PS2RHI.h` | `FPS2RHI` |
| `Public/PS2VU1.h`, `Private/PS2VU1.cpp`, `Private/PS2VU1Encoder.h` | `FPS2VU1`, `Leon::PS2::FPS2VU1BatchEncoder`, the VU1 memory layout (`Leon::PS2::VU1Memory`) |
| `Private/VU1/VU1Programs.vsm` | the microprograms StaticLit and StaticUnlit (one loop), assembled by `dvp-as` (`LeonPlatform_PS2_ModuleSources`, `LeonBuildPS2.cmake`) |
| `Private/VU1/Skinned.vsm` | the microprograms SkinnedLit and SkinnedUnlit (N14b: a vertex posed by its two palette bones, then StaticLit's work), uploaded after the static ones |
| `Private/PS2RHIModule.cpp` | `IMPLEMENT_MODULE(FDefaultModuleImpl, PS2RHI)` |
| `Private/PS2DynamicRHI.cpp` | `FPS2DynamicRHI`, `PlatformCreateDynamicRHI()`, `InitDisplay` (the region: ROMVER, `-PAL` / `-NTSC`), `ShutdownDisplay`, `ClearColor`, `Submit`, `WaitVSync` |
| `Private/PS2VerticalBlank.h/.cpp` | `Leon::PS2::FPS2VerticalBlank`: the `INTC_VBLANK_S` handler that counts the fields and signals a semaphore, and the wait for a field |
| `Private/PS2GSContext.h/.cpp` | frame and Z buffers, the two frames' command lists and DMA chains (`FPS2GifChain`), the drawing environment, the chain's build, kick and retirement and the flip (`BuildFrameChain`, `ShowPendingFrame`, `KickFrameChain`), VRAM bookkeeping (`Leon::PS2::FPS2GSContext`) |

Platform overview: [Engine/Platforms/PS2/README.md](../../../README.md).
