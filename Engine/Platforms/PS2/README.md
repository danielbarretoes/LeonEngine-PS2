# PS2 platform extension

PlayStation 2 (Emotion Engine + Graphics Synthesizer) support, organised as a UE 4.27 **platform extension**
(`Engine/Platforms/<Platform>/`). All PS2 implementation code lives here (shared engine code only sees generic
macros such as `PLATFORM_PS2`, defaulted to 0 in `HAL/Platform.h`): this folder registers the platform with
LeonBuildTool, merges PS2 code into the engine modules of the same name, and adds the `PS2RHI` module. Build-system
details: [Docs/BUILD.md](../../../Docs/BUILD.md).

At 0.24.0 ([ps2-shipping](../../../Docs/PLANS/ps2-shipping.md)) the engine uses the PS2 as a PS2:

- **GS and VU1**: the frame is one DMA chain to VIF1, double buffered, sent without waiting; VU1 microprograms
  transform, light (a sun and up to two point lights), cull and pack the static and skinned meshes' batches (PATH1);
  the EE clips only the batches across the near plane or the guard band ([VU1](#vu1),
  [PS2RHI](Source/Runtime/PS2RHI/README.md)).
- **EE**: VU0 in macro mode for the matrix and culling math, the 16 KB scratchpad, the vertical blank by interrupt
  ([Core](#core--sourceruntimecore), [The display](#the-display-and-the-vertical-blank)).
- **Sound**: SPU2 ADPCM on the hardware voices through audsrv ([AudioMixer](#audiomixer--sourceruntimeaudiomixer)).
- **IO**: an IO thread reads the pak asynchronously from `host:` or the disc; the memory card keeps the saves; the
  DualShock 2 on two ports with pressure and vibration.
- **Shipping**: a bootable ISO ([The ISO](#the-iso)), measured unattended in PCSX2
  ([Measuring in PCSX2](#measuring-in-pcsx2)); PCSX2 is not the hardware.

## Layout

```
Engine/Platforms/PS2/
  Build/
    BatchFiles/
      RunPCSX2.ps1            launch a project's (or engine program's) ELF in PCSX2 (optionally build it first)
      MeasurePS2.ps1          measure a game's frame in PCSX2, unattended (MeasurePS2.bat; -Iso boots the disc)
      DockerEntry.sh          entry point inside the ps2dev container (re-runs LeonBuildTool)
    Docker/Dockerfile         the build image (built on first use): pinned ps2dev + CMake, Ninja, g++, xorriso
    PCSX2/Measure.ini         the PCSX2 settings MeasurePS2 lays over the user's (console timings, host: on)
    PlayRunner/               leonrun: a PS2 ELF headless on Play!'s HLE BIOS (no console BIOS)
  Config/PS2Engine.ini        platform config layer (resolution, stats, async loading budget)
  Documentation/Budgets.md    ELF size, memory, frame and load measures per phase
  Documentation/PS2SDK.md     what the engine uses of ps2sdk, the review of it, what to use next
  Source/
    Programs/
      LeonBuildTool/
        LeonBuildPS2.cmake    leon_register_platform(PS2 ...)
        PS2Toolchain.cmake    CMake toolchain for the EE GCC (mips64r5900el-ps2-elf-)
      GSConformance/          GSCore's conformance scenes drawn on the GS
      VU1Conformance/         the VU1 microprograms against the C++ emitter
    Runtime/
      Core/                   extension of Core             (Core_PS2.Build.cmake: HAL, VU0, scratchpad, IO thread)
      ApplicationCore/        extension of ApplicationCore  (ApplicationCore_PS2.Build.cmake: window, DualShock 2)
      AudioMixer/             extension of AudioMixer       (AudioMixer_PS2.Build.cmake: the SPU2 through audsrv)
      Engine/                 extension of Engine           (Engine_PS2.Build.cmake: the memory card)
      Launch/                 extension of Launch           (Launch_PS2.Build.cmake)
      Renderer/               extension of Renderer         (Renderer_PS2.Build.cmake: the GS scene renderer on PS2RHI)
      PS2RHI/                 PS2-only module               (PS2RHI.Build.cmake; Private/VU1/*.vsm: VU1 microcode)
```

## Platform registration

`Source/Programs/LeonBuildTool/LeonBuildPS2.cmake` registers `PS2` with LeonBuildTool:

| Setting | Value |
| --- | --- |
| Groups (folder names and keyword suffixes that apply) | `PS2`, `Console` |
| Header folder / `LBT_COMPILED_PLATFORM` | `PS2` (`PLATFORM_IS_EXTENSION=1`: headers sit at the root of `Public/`, e.g. `PS2PlatformMemory.h`) |
| Definitions | `PLATFORM_PS2=1` |
| C++ standard | 17 |
| Executable suffix | `.elf` |
| RHI module | `PS2RHI` |
| Build types | Debug → `Debug`; Development and Shipping → `Release` (`-O2`, no debug info) |
| Docker image | `ghcr.io/ps2dev/ps2dev@sha256:79c24d3762f5cfeee0beabeacc94480fe580d379d3e48a0bfbac681a8895daf6`, the base of `Build/Docker/Dockerfile` (`leon/ps2-build:<hash>`) |
| SDK variable | `PS2DEV` — when it is not set on the host, the build re-runs inside the image |

`PS2Toolchain.cmake` requires `PS2DEV` and `PS2SDK`, uses `$PS2DEV/ee/bin/mips64r5900el-ps2-elf-{gcc,g++}`, compiles
with `-D_EE -G0 -O2 -Wall -ffunction-sections -fdata-sections` (C++: `-fno-exceptions -fno-rtti
-fno-threadsafe-statics`; Release forced to `-O2`, plan D9) and links with `$PS2SDK/ee/startup/linkfile` and
`-Wl,--gc-sections`, so unused functions and data are dropped (ELF sizes:
[Documentation/Budgets.md](Documentation/Budgets.md)). The engine adds `-Wall -Wextra -Werror=shadow -Werror=double-promotion` to every Leon module on PS2.

## Module extensions

Each folder under `Source/Runtime/` with the same relative path as an engine module is merged into that module when
building for PS2 (its `Public/` and `Private/` join the module's). Its `<Module>_PS2.Build.cmake` calls
`leon_module_extend()` to add PS2-only dependencies and PS2SDK libraries.

### Core — `Source/Runtime/Core/`

Adds `kernel` (EE timer). Implements the HAL types that `HAL/Platform*.h` select through
`COMPILED_PLATFORM_HEADER()`:

| Type | Header | What it does |
| --- | --- | --- |
| `FPS2PlatformTypes` → `FPlatformTypes` | `PS2Platform.h` | EE is ILP32: 32-bit `SIZE_T`, `PTRINT`, `UPTRINT`; `PLATFORM_DESKTOP 0`, `PLATFORM_64BITS 0` |
| `FPS2PlatformProperties` → `FPlatformProperties` | `PS2PlatformProperties.h` | `PlatformName()` = `"PS2"`, `IsGameOnly()` = true; `FName` pool of 16 KB blocks, at most 16 (256 KB), 4096 hash buckets |
| `FPS2PlatformMisc` → `FPlatformMisc` | `PS2PlatformMisc.h` | debug output goes to the EE console; a forced `RequestExit` halts the EE thread so the console keeps the last messages |
| `FPS2PlatformAtomics` → `FPlatformAtomics` | `PS2PlatformAtomics.h` | the generic non-atomic operations: the game runs on one EE thread (the IO thread shares only the IO queue, under its semaphore) |
| `FPS2PlatformMemory` → `FPlatformMemory` | `PS2PlatformMemory.h` | `GetStats()`: program image + heap (newlib break above `0x00100000`) of 32 MB EE RAM; `GetOnChipScratchpad()`: the EE's 16 KB scratchpad at `0x70000000`, Core's `FScratchpad` (N15; `-nospr` leaves it for main RAM) |
| `FVectorMath` (Core's `Math/VectorMath.h`) | `Private/PS2VectorMath.cpp` | VU0 in macro mode (N15): the matrix product, the matrix times a vector and the box / sphere tests against planes four at a time (`LQC2`, `VMULA`, `VMADDA`, `VMINI`, `SQC2` / `QMFC2` from the EE); `FVectorMathFPU` is the reference TestPAL compares it with |
| `FPS2PlatformTime` → `FPlatformTime` | `PS2PlatformTime.h` | `Cycles64()` from `GetTimerSystemTime()` (BUSCLK, 147.456 MHz), `Seconds()`, `CyclesToMicroseconds()` |
| `FPS2PlatformMath` → `FPlatformMath` | `PS2PlatformMath.h` | `Sin256()` / `Cos256()`: quarter-wave table on a 1/256-turn angle, no libm (soft-float `double` is slow on the EE) |
| `CreatePlatformAsyncIOWorker` | `Private/PS2AsyncIO.cpp` | the IO thread of `FAsyncIOSystem` ([ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N24): an EE thread one priority above the game's that reads the asynchronous requests (`IPlatformFile::OpenAsyncRead`, the pak's second handle of the disc) nearest ahead first, the close ones coalesced (N24b), in 64 KB chunks through fio; it sleeps in the IOP's calls, so the game thread runs meanwhile, and it allocates and logs nothing |
| `FPS2PlatformMisc::LoadIopModule` | `Private/PS2PlatformMisc.cpp` | loads a ROM module (`rom0:SIO2MAN`, `PADMAN`, `MCMAN`, `MCSERV`) once: the pads and the memory card share SIO2MAN |

`Private/PS2PlatformFile.cpp` is the platform file over the device prefixes: `host:` (PCSX2's host filesystem, the
ELF's folder), `cdrom0:` (the disc, each path asked for by its ISO 9660 name, `FPaths::ToIso9660Path`) and `mass:`.

`Core.Build.cmake` excludes no source on PS2, and Core has no third-party dependency on any platform.

### ApplicationCore — `Source/Runtime/ApplicationCore/`

Adds `PS2RHI` (private) and `pad`.

| Type | File | What it does |
| --- | --- | --- |
| `FPS2PlatformApplicationMisc` → `FPlatformApplicationMisc` | `Public/PS2PlatformApplicationMisc.h` | `CreateApplication()` returns an `FPS2Application` |
| `FPS2Application` | `Private/PS2Application.cpp` | `GenericApplication`: `MakeWindow()` → `FPS2Window`; `PollGameDeviceState()` reads the pad; owns the `FPS2InputInterface` |
| `FPS2Window` | `Private/PS2Window.h/.cpp` | `FGenericWindow` over the GS display: `Create()` calls `FPS2RHI::InitDisplay()` (default 640x448) and creates the RHI; `PollEvents()` is empty (no OS message queue); `SwapBuffers()` waits for vsync |
| `FPS2InputInterface` | `Public/PS2InputInterface.h` | `IInputInterface` for the DualShocks on ports 0 and 1, controller ids 0 and 1 (libpad; UE homologue: `XInputInterface`; [ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N24). Loads `rom0:SIO2MAN` and `rom0:PADMAN`, and each pad gets, one command a frame when it is stable (`FDualShockConnection`), the locked analog mode, its motors aligned (`padSetActAlign`) and the pressure mode (`padEnterPressMode`), again after a reconnection; a 0.18 stick dead zone; the pressure bytes as the `Gamepad_*Axis` keys (`FDualShockPressure`); the player controllers' force feedback on the small and large motors (`padSetActDirect` when it changes, `FDualShockActuators`), stopped when the pad is pulled out and at exit. The second pad reaches a local player of controller id 1; ShooterGame has one player, so it drives no one |

### AudioMixer — `Source/Runtime/AudioMixer/`

Adds `audsrv` and stages `$PS2SDK/iop/irx/audsrv.irx` beside the ELF. `Private/PS2AudioHardware.cpp`: the
`FAudioHardware` of `FAudioDevice` is the SPU2's voices through audsrv's ADPCM calls
([ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N19): each sound is cooked to SPU2 ADPCM, uploaded once to SPU2
RAM and played on a hardware voice (24, 2 kept for music); nothing is mixed on the EE. Without `audsrv.irx` the game
runs silent and logs why.

### Renderer — `Source/Runtime/Renderer/`

Adds `PS2RHI` (private). `Private/PS2RendererModule.cpp`: `IRendererModule` on the GS scene renderer (the same code as
the desktop's OpenGL GS emulator), whose lists `FPS2RHI` sends at `WaitVSync`; the textures live in the VRAM the
display leaves (`FPS2RHI::AllocateTextureArena`).

### Engine — `Source/Runtime/Engine/`

Adds `mc` (libmc). `Private/PS2SaveGameSystem.cpp`: the platform's `ISaveGameSystem` is `FMemoryCardSaveGameSystem`
(Engine) over the memory card in slot 1 (`mc0:`, the ROM's `MCMAN` / `MCSERV`;
[ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N24): a slot is a file in the game's folder (`[MemoryCard]
Directory` of the game config, ShooterGame's `BASLUS-99001SHOOTER`) with `icon.sys` (the title, `[MemoryCard] Title`)
and `icon.ico` (a textured tile) for the browser, written with the folder; every call waits for the card (`mcSync`),
and a missing, unformatted, full or pulled out card gives its `ESaveGameResult`.

### Launch — `Source/Runtime/Launch/`

Adds `PS2RHI` (private).

| File | What it does |
| --- | --- |
| `Private/LaunchPS2.cpp` | `main()` → `GuardedMain()` (UE: `Launch<Platform>.cpp`): resets the IOP, appends the arguments of `LeonCommandLine.txt` to `argv`, and shows the error screen when the game returns an error |
| `Private/PS2ErrorScreen.h/.cpp` | `FPS2ErrorScreen`: the log's last errors on a red screen when the game stops before it plays or on a fatal error, recorded with `FGSDebugDraw` (GSCore; the game's font compiled in, uploaded to the start of the texture arena, the paragraphs word-wrapped at the margins) into an `FGSCommandList` and submitted to `FPS2RHI` |

### PS2RHI — `Source/Runtime/PS2RHI/`

A PS2-only module (`PLATFORMS PS2`, depends on `Core`, `RHI` and `GSCore`, links `draw graph dma kernel`):
the GS backend (`FDynamicRHI` implementation returned by `PlatformCreateDynamicRHI()`) plus the static `FPS2RHI` API
for the display and the frame: what is drawn arrives as `FGSCommandList`s (`Submit`), which `WaitVSync` sends to
VIF1: the GS writes by DIRECT and the static and skinned meshes' vertex batches to VU1 (`FPS2VU1`, see [VU1](#vu1)).
See [its README](Source/Runtime/PS2RHI/README.md).

## Frame order

Every PS2 game is compiled against the engine (`WITH_ENGINE=1`): `FEngineLoop`
(`Engine/Source/Runtime/Launch/Private/LaunchEngineLoop.cpp`) runs the same loop as on the desktop, with `GEngine`
([Docs/ARCHITECTURE.md §9](../../../Docs/ARCHITECTURE.md#9-launch-and-the-engine-loop)).

Startup (`main` → `GuardedMain` → `FEngineLoop::PreInit` and `Init`):

1. `main` installs the error screen, resets the IOP and adds the arguments of `LeonCommandLine.txt`.
2. The project, the pak platform file (the staged `<Project>/Content/Paks/<Project>-PS2.lpak`), the config and the
   log.
3. `FPlatformApplicationMisc::CreateApplication()` → `FPS2Application` (initializes the pad).
4. `MakeWindow()` + `Create()` at `DefaultResolutionX/Y` (640x448 in `Config/PS2Engine.ini`) → GS display (double
   buffered `PSMCT16S`, `PSMZ24`) in the console's television mode, and the vertical blank interrupt handler (see
   [The display and the vertical blank](#the-display-and-the-vertical-blank)).
5. `RHIInit()` → `FPS2DynamicRHI` in `GDynamicRHI`.
6. `FModuleManager::StartupStaticallyLinkedModules()`, then `Init` creates `GEngine` (`UGameEngine`): the renderer
   (`FPS2RendererModule`) takes the texture arena (`FPS2RHI::AllocateTextureArena`) and the sync interval, and
   `Start` opens `GameDefaultMap`.

Each `FEngineLoop::Tick()`:

1. `FTicker::GetCoreTicker().Tick()`; `UGameEngine::Tick` then runs `ProcessAsyncLoading` before the world: the
   packages the IO thread read are serialized (8 ms a frame at most, `AsyncLoadingTimeLimit` of
   `Config/PS2Engine.ini`).
2. `Application->PollGameDeviceState()` — read the DualShock (`FPS2InputInterface::SendControllerEvents()`);
   `MainWindow->PollEvents()` is a no-op on PS2.
3. `GEngine->TickDeferredCommands()`, then `GEngine->Tick()`: input, the world, then the viewport's draw, where the GS
   scene renderer records the scene and the canvas (HUD, stats) into `FGSCommandList`s and `FPS2RHI::Submit`s them.
4. The present: `FPS2Window::SwapBuffers()` → `FPS2RHI::WaitVSync()` builds the frame's DMA chain, shows the frame
   before it at the vertical blank (`SyncInterval=2`: a frame every two fields) and kicks this one to VIF1 without
   waiting for it (see [The frame's DMA chains](#the-frames-dma-chains)): the GS draws frame N while the EE ticks
   frame N + 1.

### The display and the vertical blank

[ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N10.

- **The region.** `FPS2RHI::InitDisplay` sets the CRTC to the console's mode: PAL when ROMVER's region letter is `E`
  (`rom0:ROMVER`, read by libgraph's `graph_get_region`), NTSC otherwise; `-PAL` or `-NTSC` on the command line
  chooses. Both are interlaced, in field mode, with the flicker filter. The frame stays 640x448 in both: NTSC shows
  448 lines, and PAL, whose picture has 512, shows the same 448 centred (32 black lines above and below: the same
  frame, VRAM and texture arena on both, at the price of a picture 12.5 % flatter on a PAL TV). A 512-line PAL frame
  would cost 0.3 MB of the texture arena and 14 % more fill; it waits for the arena's budget (N13).
- **The fields.** An `INTC_VBLANK_S` handler (`FPS2VerticalBlank`, installed once) counts the vertical blanks and
  signals a semaphore that holds one signal at most; `WaitVSync` sleeps on it (`WaitSema`) until the field
  `FGSFieldPacer` (GSCore) picks: `SyncInterval` fields after the last flip, or the next blank when that one has
  already begun, so a frame lasts a whole number of fields: 33.37 ms or 50.05 ms at `SyncInterval=2` on NTSC (30 fps),
  40 or 60 ms on PAL (25 fps). The flip (`DISPFB`) is written right after the blank starts. No code polls the GS's
  `CSR` for the blank (G4 bans libgraph's polls), and the time comes from the count, not from the clock.
- `FPS2Window::Destroy` calls `FPS2RHI::ShutdownDisplay`: the handler and the semaphore go, then the CRTC.

### The frame's DMA chains

[ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N11. The EE and the GS work at the same time: `WaitVSync` of frame
N builds N's chain, waits for the GS to finish frame N - 1, shows it at its field, kicks N and returns; the EE ticks
N + 1 while the DMA and the GS draw N. A frame reaches the screen one frame after it is recorded.

- **Two of everything.** Two command lists (`FrameLists`), two chain buffers (`Chains`) and the two frame buffers: the
  EE records and builds into one while the DMA reads the other. A list is reset only once its chain is retired (the
  GS's FINISH seen, `dma_channel_wait`), since the chain reads the list's image data in place.
- **The chain** (`FGSGifPacket::BuildChain`, GSCore, the same encoder as `Build` and its tests), sent to VIF1 with the
  tags' VIFcodes (TTE, N14): the GIFtags and register writes in CNT sections (QWC up to 0xffff) that VIF1 hands the GIF
  by DIRECT (PATH2), each upload's pixels by a REF tag to the command list's own copy (quadword aligned, no copy into
  the packet) with its own DIRECT, the vertex batches for VU1 (below), and END. The GS's FINISH write is the last GIF
  data.
- **Cache coherence.** Each chain buffer is 128-byte aligned whole 128-byte lines (the UCAB's and the DMAC's burst),
  written only through the uncached accelerated segment (`UCAB_SEG`, 0x30000000): the writes stream out through the
  UCAB, never allocate in the 8 KB data cache (the scene renderer's working set stays), and need no write back; one
  `SyncDCache` when a buffer is allocated drops whatever the heap left cached there, and `sync.l` before the kick
  drains the UCAB. The REF'd pixels (the list's copy) and meshes' streams were written through the cache, so
  `FlushCache(WRITEBACK_DCACHE)` writes the whole 8 KB cache back before the kick (`dma_channel_send_chain_ucab`, TTE
  on). libpacket2's `packet2_t` is not used: its buffers are
  64-byte aligned, at most 0xffff quadwords, allocated outside `GMalloc`, and its tag helpers would be a second encoder
  of the chain.
- **Waits.** Only where the next step needs them, each in its own cycle stat under Present: `GS Finish`
  (`draw_wait_finish`, CSR.FINISH, right before the flip), `VIF1 DMA` (the kick, and `dma_channel_wait` when a buffer is
  retired), `Vertical Blank Wait` (the semaphore). `Frame Chain` is the chain's build.

### VU1

[ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N14. The meshes' batches inside the guard band are transformed, lit,
culled and packed for the GS by VU1, not the EE (the EE decides per batch with its bounding sphere: outside, skipped;
across the near plane or the guard band, the C++ clipper; inside, VU1: D8). A skinned batch goes with its palette,
the pose's skin matrices the EE puts in the list's memory, and VU1 poses its vertices (N14b); its sphere comes from
the pose without skinning a vertex.

- **The microprograms** are `Source/Runtime/PS2RHI/Private/VU1/VU1Programs.vsm`: StaticUnlit and StaticLit (one loop;
  the ambient share and one directional light), and `Skinned.vsm`: SkinnedUnlit and SkinnedLit (the same after
  posing each vertex with its two palette bones, uploaded after the static ones), hand-written for `dvp-as` (one upper and one lower instruction a
  line). LeonBuildTool assembles every module's `Private/VU1/*.vsm` in a PS2 build (`LeonPlatform_PS2_ModuleSources`,
  `LeonBuildPS2.cmake`) and archives the object with the module; the ELF carries the code (`.vutext`) and
  `FPS2VU1::UploadPrograms` sends it with MPG when the display starts.
- **The data** (the [PS2RHI README](Source/Runtime/PS2RHI/README.md#vu1-memory) has the layout): a batch is a CNT with
  its header unpacked at VIF1's TOPS, four REFs to the LPS2 v2 streams where the mesh keeps them (no copy) with an
  UNPACK each, and MSCAL. VU1's memory is double buffered (BASE 16, OFFSET 504), so VIF1 unpacks batch N + 1 while
  VU1 runs batch N; each program XGKICKs its PACKED GIF packet (PATH1).
- **PATH1, PATH2 and PATH3.** One chain keeps the frame in order: the CPU's writes go by DIRECT (PATH2), each section
  before a batch ends its GIF packet (EOP) and the first after batches starts with FLUSH. PATH3 is not used (no
  MSKPATH3 dance): the GIF is reset at `InitDisplay` so that no PATH3 packet left from before the ELF holds it.
- **Checks.** `VU1Conformance` (below) compares the programs with the C++ emitter on the EE; `-novu1` on the game's
  command line routes every batch through the emitter (the same one the desktop uses), for comparing frames.

The loop ends when `RequestEngineExit()` is called or the window reports `ShouldClose()`; `FEngineLoop::Exit()` ends
the engine, shuts the modules down, releases the RHI (`RHIExit`) and destroys the window. A game that returns an error
shows `FPS2ErrorScreen`.

## Debug overlay

The PS2 has the engine's own debug overlay (UE: `stat unit` + `AddOnScreenDebugMessage`), drawn by the canvas as on
every platform:

- **The engine's stats** (top-right, `UGameViewportClient::UpdateHudStats`): `FPS` and `MS` (the paced frame), `RAM`
  (`GMalloc`'s current / peak / budget in MB, the `Total` of `[Core.MemoryBudgets]`; `stat memory` lists the memory
  tags: [ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N17), `VRAM` (the GS's 4 MB), `TRIS` and `OBJ <visible>/<total>`
  (after culling), `RES`, and the frame's GS register writes and texture uploads. On the PS2 they show from the start
  (`bShowStatsByDefault=True` in `Config/PS2Engine.ini`); `UEngine` keeps the visibility (`SetHudStatsVisible`).
- **R3** (`Gamepad_RightThumbstick`) toggles `stat unit`, as F4 does on the desktop (`DebugExecBindings` in
  `Engine/Config/BaseInput.ini`).
- `-LogFrameTimes` puts the frame times in the EE log every 5 seconds (the world's share and the draw and present's),
  and `MeasurePS2.bat` measures them unattended ([Docs/TESTING.md](../../../Docs/TESTING.md)).
- **Profiling** ([ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N9): the engine's cycle stats
  (`SCOPE_CYCLE_COUNTER`, [ARCHITECTURE.md §6](../../../Docs/ARCHITECTURE.md#cycle-stats-the-profiler)) time the frame
  with the COP0 Count (the CPU clock, 294.912 MHz, one `mfc0` a reading) and count the instruction and data cache misses
  of each top level scope with the performance counters (`FPS2PlatformTime`: PCCR sets PCR0 to event 6, I$ misses, and
  PCR1 to event 6, D$ misses, in every mode; they are zeroed every frame, far from the overflow bit that would raise the
  counter exception). With `-LogFrameTimes` the EE log gets a `Profile over N frames (ms, calls):` block every 5
  seconds (the scopes as a hierarchy: world, bots, collision queries, scene (by part: visibility, opaque, skinned,
  translucent and effects, view model), canvas, audio voices, and the present's GIF packet, DMA, GS finish and
  vertical blank wait) and a `ProfileSummary:` line at exit, which `MeasurePS2.bat` adds to its CSV; a frame twice as
  long as the one before logs its scopes (`Frame spike:`, N24b). `stat cycles` (`-ExecCmds="stat cycles"` on the
  PS2, F7 on the desktop) shows the top scopes on screen. PCSX2 does not emulate the cache misses: they read 0 there,
  so the miss columns mean something only on the hardware.

The PS2-only panel (`EE ms` and the gamepad widget, cycled with L3 + R3) went in
[ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N2.

## Measuring in PCSX2

`Engine\Build\BatchFiles\MeasurePS2.bat [-Project <dir>] [-Rounds 2] [-Seed 7] [-Seconds 120] [-NoBuild] [-Label <text>]
[-Iso] [-PakOrder <file>] [-LogFileOpenOrder] [-ExtraArgs <args>]` (`Build/BatchFiles/MeasurePS2.ps1`) stages the game
with a bot match watched through a bot's eyes and `-LogFrameTimes`, runs PCSX2 without its window from a private data
folder (the user's `PCSX2.ini` with `Build/PCSX2/Measure.ini` on top), reads the EE log until `ProfileSummary:`,
closes PCSX2, appends a row to `<Project>\Saved\Profiling\PS2Frame.csv` and prints the
[Budgets.md](Documentation/Budgets.md) row and the last `Profile over` block. `-Iso` boots the disc instead of the ELF;
`-PakOrder` lays the pak out in a recorded open order and `-LogFileOpenOrder` records one. The game's clock is the
emulated console's, so the numbers do not depend on the host.

**PCSX2 is not the hardware.** Rows are labelled with the PCSX2 version and the hash of `Measure.ini`; PCSX2 does not
emulate the caches (the miss counters read 0), its disc timing is an emulated drive's, and the ISO has no license
logo nor system sectors (it boots in PCSX2, not on a console without a modchip); the figures are PCSX2's, not a
console's. At 0.24.0 ShooterGame on de_leon measures 29.95 fps, p50 / p95 / p99 33.5 ms (N31; 29.96 fps from the disc,
N24b), and the first frame from the disc comes 2.93 s after the engine starts, with the pak in its open order.

## Targeting PS2 from a game

The reference is `Game/ShooterGame`:

```cmake
# Game/ShooterGame/Source/ShooterGame.Target.cmake
leon_target(ShooterGame TYPE Game
	PLATFORMS Win64 PS2
)

# Game/ShooterGame/Source/ShooterGame/ShooterGame.Build.cmake
leon_module(ShooterGame
	PUBLIC_DEPENDENCIES Core CoreUObject InputCore Engine UMG SlateCore
	PRIVATE_DEPENDENCIES AIModule
)
```

- `PLATFORMS ... PS2` and `"TargetPlatforms": [ ..., "PS2" ]` in the `.lproj`.
- A game target is compiled against the engine (`WITH_ENGINE=1`): LeonBuildTool links `Engine`, `Renderer` and
  `PakFile`, and the loop described above runs `GEngine`. Only modules that are allowed on PS2 can be in the closure
  (no Developer or Editor module, no plugin without PS2 in its `PlatformAllowList`).
- The game is the same code as on the desktop: gameplay classes, the GS scene renderer, input through the viewport
  client (`IInputInterface`: the DualShock). It reads its content from its pak, so PCSX2 boots the staged build.

Build, cook, stage, pak and run:

```bat
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -pak -run
```

(`Build.bat ShooterGame PS2 Development -Project=%CD%\Game\ShooterGame\ShooterGame.lproj` builds the ELF alone.)

`RunPCSX2.ps1 [-Project <dir|file.lproj>] [-Configuration Debug|Development|Shipping] [-Build] [-StageOnly]` resolves
`<Project>\Binaries\PS2\<Name>.elf` (`<Name>-PS2-<Configuration>.elf` outside Development; the default project is
`Game\ShooterGame`), stages the config beside it, finds PCSX2 through `$env:LEON_PCSX2`, `PATH` or the default install
folders, and starts it with `-fastboot -elf`. With `-StagedElf <file.elf>` it starts an ELF already staged with its
content as it is (BuildCookRun's `-run` does). With `-Program <Name>` it runs an engine program instead
(`Engine\Binaries\PS2\<Name>.elf`, built with `Build.bat <Name> PS2 <Configuration>`). With `-StageOnly` it stages the
config next to the ELF and returns without starting PCSX2 (the root `Package.bat` uses it for the PS2 **dev-loop**
artifacts). Full cook/stage packages use `Saved\StagedBuilds\PS2\` instead —
[BUILD.md — PS2 staging matrix](../../../Docs/BUILD.md#ps2-staging-matrix). PCSX2 setup notes:
[Docs/SETUP.md](../../../Docs/SETUP.md#pcsx2-notes).

The Core, CoreUObject, Json, Projects, PakFile and GSCore automation tests run on the EE through the `TestPAL` program
(the pak tests on paks in memory):

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build
```

`UE_LOG` output goes to the EE console; read `%USERPROFILE%\Documents\PCSX2\logs\emulog.txt` for
`TestPAL: PASSED (<N> test(s), 0 failed)` (171 on Win64 at 0.24.0) and the `LogTestPAL` reflection / object-array /
memory / name-pool lines.

The `VU1Conformance` program (`Source/Programs/VU1Conformance`) runs fixed batches (static and skinned, unlit and lit
with none, one or two point lights, textured and not, mirrored, off the view) through VU1 without XGKICK, reads each
GIF packet back from VU1's memory (`FPS2VU1::RunBatchForTest`) and compares its triangles with the C++ emitter's in
the same ELF within plan D2's tolerances; the EE log ends with `VU1Conformance: PASSED (...)` or `FAILED`, then it draws the batches, VU1's (the
frame's own chain, with XGKICK) on the left and the emitter's on the right:

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program VU1Conformance -Build
```

The `GSConformance` program (`Source/Programs/GSConformance`) draws GSCore's GS conformance scenes on the GS, three
times their size in a grid with their names, on a 32-bit screen: what the reference rasterizer's tests check pixel by
pixel, to compare with and capture in PCSX2 ([ps2-gs-parity](../../../Docs/PLANS/ps2-gs-parity.md), P2 and P3); the
names are recorded with `FGSDebugDraw` in its 10-pixel font (compiled in, uploaded above the display: [ps2-polish](../../../Docs/PLANS/ps2-polish.md) P5b). It needs no staged config.

### ShooterGame on the EE

ShooterGame builds for PS2 with the whole gameplay framework (`WITH_ENGINE=1`: Engine, UMG, AIModule, PhysicsCore,
AudioMixer, AnimationCore) and the Renderer, whose GS scene renderer draws the world, the view model and the HUD into
`FPS2RHI`'s frame (`Renderer_PS2.Build.cmake`; [ps2-engine](../../../Docs/PLANS/ps2-engine.md) E1 and E2): the same
code the desktop runs on its OpenGL GS emulator. `-nullrhi` keeps it headless (no scene). BuildCookRun stages it for
PCSX2: the ELF with the cooked folder loose beside it (`host:` is that folder), or with `-pak` in one pak (E3), and
the arguments in `LeonCommandLine.txt`, which the PS2 launch appends to `argv` (UE: `UECommandLine.txt`), since PCSX2
passes the ELF none:

```bat
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -run "-addcmdline=-nullrhi -benchmark -botmatch -rounds=10 -seed=7"
```

With PCSX2's host filesystem on, the EE log ends with `Botmatch OK: 10 round(s), ...` and the `Botmatch budget:` line
(objects, names, GMalloc). The result does not have to match Win64's (the EE's floats are not IEEE); two runs must
match each other.

Without `-nullrhi` the game draws de_leon through the GS scene renderer (E2), from its pak (E3) with `-pak`:

```bat
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -pak -run "-addcmdline=-ExecCmds=bot_fill"
```

The EE log shows `Mounted ... ShooterGame-PS2.lpak`, `PS2 renderer: GS scene renderer, 1856 KB of texture VRAM, a frame
every 2 vertical blank(s)` and `PS2 audio: the SPU2's voices through audsrv` with a line per sound `in SPU2 RAM`
([AudioMixer](#audiomixer--sourceruntimeaudiomixer)). The game is played with the DualShock (E4, the controls in the
[ShooterGame README](../../../Game/ShooterGame/README.md#controls)) at a steady 30 fps on NTSC (25 on PAL):
`SyncInterval=2` in `[/Script/Engine.RendererSettings]` of `BaseEngine.ini`, as on the desktop (UE: `rhi.SyncInterval`;
1 for 60 Hz); [Measuring in PCSX2](#measuring-in-pcsx2) has the figures. With `-LogFrameTimes` the EE log gets `Frame
times over N frames: ... ms average (... fps), ... ms worst; world ... ms, draw and present ... ms`, the `Frame split`
and the `Profile over N frames` block every 5 seconds. The player's settings are saved on the memory card
([Engine](#engine--sourceruntimeengine)); the DualShock 2 vibrates
([ApplicationCore](#applicationcore--sourceruntimeapplicationcore)).

### The ISO

`BuildCookRun -platform=PS2 -iso` ([ps2-shipping](../../../Docs/PLANS/ps2-shipping.md) N23; `-region=NTSC|PAL`,
`-discserial=`) makes `<Project>\Saved\StagedBuilds\PS2\<Project>.iso` with xorriso in the build image (ISO 9660 level
1, system `PLAYSTATION`), laid out in this order: `SYSTEM.CNF` (`BOOT2 = cdrom0:\SLUS_990.01;1`, `VER`, `VMODE`), the
ELF as `SLUS_990.01` (`SLES_990.01` for PAL), the pak, then `AUDSRV.IRX` and the command line (`LEONCOMM.TXT`); every
name in ISO 9660 8.3 as `FPaths::ToIso9660Path` makes it. The game mounts `cdrom0:/.../ShooterGame-PS2.lpak` and reads
it through the asynchronous IO thread. `-pakorder=<file>` lays the pak's entries out in the order a
`-LogFileOpenOrder` run opened them (the first frame 2.93 s after the engine starts, against 7.72 s before N24b):

```bat
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -pak -iso
Engine\Build\BatchFiles\MeasurePS2.bat -Iso
```

The root `Package.bat` builds and packages ShooterGame under `Game\ShooterGame\Packages\PS2\` (its staged build),
and `TestPAL`, `GSConformance` and `VU1Conformance` under `Engine\Packages\PS2\<Name>\` (Development, in Docker); the
ELF sizes (gate G3) are measured with the toolchain's `mips64r5900el-ps2-elf-size` in the ps2dev image when a phase is
recorded ([Budgets.md](Documentation/Budgets.md)).

## Reference

- Sony manuals and PS2 architecture notes: [Docs/PS2OFFICIAL/](../../../Docs/PS2OFFICIAL/README.md)
- UE 4.27 platform extension layout: [Docs/UnrealEngine427/SourceLayout.md](../../../Docs/UnrealEngine427/SourceLayout.md)
