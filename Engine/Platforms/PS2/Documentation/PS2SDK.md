# The PS2SDK in Leon

What the engine uses of [ps2sdk](https://github.com/ps2dev/ps2sdk), whether it uses it the way the SDK's sources expect,
and what the SDK offers that the engine does not use yet. Reviewed against the ps2sdk sources (`ee/`, `iop/`,
`common/`) and the toolchain of the pinned ps2dev image. Each finding says its evidence: **Verified** (read in the SDK's
source, or run), **Inferred** (from the source, not run), or **Open** (not settled).

## How a PS2 build is checked without a console

- **PCSX2** (needs a console BIOS): the manual checks in [TESTING.md](../../../../Docs/TESTING.md), and the only place
  the picture, the sound and the frame times are measured.
- **Play! headless** (`Engine/Platforms/PS2/Build/PlayRunner/`): `BuildPlayRunner.sh` builds `leonrun` in a Play!
  checkout at a pinned commit. Play! runs an ELF on its HLE BIOS (no console BIOS), with `host:` at the ELF's folder
  and the IOP's stdout (the EE log) on the terminal; its GS draws nothing, and its HLE IOP has no `rom0:LIBSD` (the
  audio falls back to silence). It is how the boot failure below was reproduced, and how a botmatch was first run on
  the EE outside PCSX2:
  `Botmatch OK: 2 round(s), CT 0 - T 2, 15 kill(s), seed 7` with `GMalloc peak 1100 KB` (ShooterGame Development,
  2026-09-26).

## What the engine uses

| Library (link name) | API | Where | For |
|---|---|---|---|
| libkernel (`kernel`) | `SifInitRpc`, `SifIopReset`, `SifIopSync`, `SifLoadModule`, `SifExecModuleBuffer`, `SleepThread`, `GetTimerSystemTime` | Core (`PS2PlatformMisc`, `PS2PlatformTime`), ApplicationCore, AudioMixer | IOP modules, halting, the clock |
| libkernel (`kernel`) | `AddIntcHandler` / `RemoveIntcHandler`, `EnableIntc` / `DisableIntc` (`INTC_VBLANK_S`), `CreateSema`, `iSignalSema`, `WaitSema`, `PollSema`, `DeleteSema` | PS2RHI (`FPS2VerticalBlank`) | the vertical blank's field count and the wait for it (N10) |
| libkernel (`kernel`) | `SyncDCache`, `FlushCache`, `UCAB_SEG`, `EE_SYNCL` | PS2RHI (`BuildFrameChain`, `KickFrameChain`, `FPS2VU1`) | the DMA chain's cache coherence (N11, N14) |
| libkernel (`kernel`) | `CreateThread`, `StartThread`, `ReferThreadStatus`, `ChangeThreadPriority`, `CreateSema`, `WaitSema`, `SignalSema` | Core (`PS2AsyncIO`) | the IO thread of the asynchronous reads and its queue's lock (N24) |
| libpatches (`patches`) | `sbv_patch_enable_lmb`, `sbv_patch_disable_prefix_check` | Core (`FPS2PlatformMisc::InitializeIop`) | loading `audsrv.irx` from EE memory |
| libcglue + newlib | `open` / `read` / `lseek` / `close`, `opendir`, `nanosleep`, `sbrk`, `printf` | Core (`PS2PlatformFile`, `PS2PlatformProcess`, `PS2PlatformMemory`) | files through the ROM's FILEIO (`host:`, `cdrom0:`), sleeping, the heap, the EE log |
| libpad (`pad`) | `padInit`, `padPortOpen`, `padGetState`, `padRead`, `padInfoMode`, `padSetMainMode`, `padInfoAct`, `padSetActAlign`, `padSetActDirect`, `padInfoPressMode`, `padEnterPressMode` | ApplicationCore (`PS2InputInterface`) | the DualShocks on ports 0 and 1: analog mode, vibration, pressure (N24) |
| libmc (`mc`) | `mcInit`, `mcGetInfo`, `mcGetDir`, `mcMkDir`, `mcOpen`, `mcRead`, `mcWrite`, `mcSeek`, `mcClose`, `mcDelete`, `mcSync` | Engine (`PS2SaveGameSystem`) | the saves on the memory card in slot 1 (N24) |
| libaudsrv (`audsrv`) | `audsrv_init`, `audsrv_adpcm_init`, `audsrv_load_adpcm`, `audsrv_free_adpcm`, `audsrv_ch_play_adpcm`, `audsrv_adpcm_set_volume_and_pan` | AudioMixer (`PS2AudioHardware`) | the sounds in SPU2 RAM played on its hardware voices (N19) |
| libgraph (`graph`) | `graph_get_region` (ROMVER), `graph_set_mode`, `graph_set_screen`, `graph_set_bgcolor`, `graph_enable_output`, `graph_vram_allocate`, `graph_vram_clear`, `graph_set_framebuffer_filtered`, `graph_shutdown` | PS2RHI | the region and the CRTC, the VRAM layout, the flip |
| libdma (`dma`) | `dma_channel_initialize`, `dma_channel_send_chain_ucab`, `dma_channel_send_chain`, `dma_channel_wait` | PS2RHI (`KickFrameChain`, `ShowPendingFrame`, `FPS2VU1`) | the frame's VIF1 chain (DIRECT by PATH2, vertex batches through VU1 and PATH1), double buffered (N11, N14); the microprograms' MPG |
| dvp-as (`$PS2DEV/dvp/bin`) | the VU assembler | PS2RHI (`Private/VU1/VU1Programs.vsm`, `LeonPlatform_PS2_ModuleSources`) | VU1's microprograms (N14) |
| libdraw (`draw`) | `draw_wait_finish` | PS2RHI (`ShowPendingFrame`) | waiting for the GS's FINISH before the flip |

The GIF packets and their DMA chains are built by `FGSGifPacket` (GSCore) from `FGSCommandList`, not by libdraw or
libpacket2: the GS's registers are the engine's own contract, shared with the reference rasterizer and the desktop's
emulator, and one encoder writes the packet both ways (an array for the tests, a chain for the DMA). The VIF1 codes
of the vertex batches are written by `FPS2VU1BatchEncoder` (PS2RHI), not libpacket2's `packet2_vif`, for the same
reason (one encoder of the chain).

## Findings

Ordered by impact. "Fixed" means fixed in the change that added this document.

### 1. A game that could not read `host:` stopped on a black screen — Fixed, Verified

Without its config (PCSX2 without *Enable Host Filesystem*, or an ELF booted away from its pak) the window asked for
the base config's desktop size (1280x896), `graph_vram_allocate` could not fit two frame buffers in the 4 MB, the
engine returned from `main` and the TV stayed black. Reproduced with `leonrun` on an ELF alone in its folder. Now:
the PS2 window keeps to the TV's modes, PreInit stops with an error naming the folder when a console build has no
Engine config, and `FPS2ErrorScreen` draws the log's last errors and what to check.

### 2. `padInit` waited forever when PADMAN was missing — Fixed, Verified in the source

libpad binds its RPC servers in loops that only end when the server answers (`ee/rpc/pad/src/libpad.c`,
`while (!padsif[1].server) { if (sceSifBindRpc(...) < 0) ...; nopdelay(); }`). The engine logged a failed
`SifLoadModule("rom0:PADMAN")` and called `padInit` anyway: a hang with no message. `PS2InputInterface` now skips the
pad when either module does not load.

### 3. The IOP was never reset — Fixed, Verified (Play!); Open on hardware

The SDK's samples start with `SifIopReset` / `SifIopSync` / `SifInitRpc` and the LOADFILE patches
(`ee/rpc/filexio/samples/main.c`, `reset_IOP`). The engine did not: from a launcher (uLaunchELF, OPL) the launcher's
own modules (XSIO2MAN, XPADMAN, its file system drivers) stay loaded, and loading the ROM's SIO2MAN and PADMAN on top
of them is what the PS2 homebrew scene reports as pads that never answer. `FPS2PlatformMisc::InitializeIop` now
reboots the IOP to its ROM modules first thing in `main` (before any file opens: a reboot closes the IOP's files;
libkernel's `fioInit` rebinds after a reboot on its own, `HasIopRebootedSinceLastCall`), and `-NoIopReset` keeps a
debugger's `host:` (ps2link loads its own IOP modules). Verified with `leonrun`; PCSX2's host filesystem is emulated
below the IOP's I/O manager and is expected to survive the reboot, which the PCSX2 checklist confirms.

### 4. The frame was sent synchronously — Fixed ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N11), Verified (PCSX2, TestPAL on the EE)

`FlushFrame` built the GIF packet into a `TArray`, copied it into a `packet_t` (`memcpy`), sent it with
`dma_channel_send_normal` in 0xffff-quadword pieces with `dma_wait_fast` after each, then waited for the GS's FINISH
before the EE started the next frame: the EE and the GS never worked at the same time, and the frame (texture pixels
included) was copied twice. Now `FGSGifPacket::BuildChain` writes the frame straight into one of two DMA buffers as a
source chain (CNT / END sections, the uploads' pixels by REF to the command list's copy), through the uncached
accelerated segment (no data cache write back for the chain; `SyncDCache` for the REF'd pixels), and
`dma_channel_send_chain_ucab` kicks it without waiting. The next `WaitVSync` builds the next frame into the other
buffer while the DMA and the GS run, waits for FINISH only right before the flip and retires the buffer
(`dma_channel_wait`) only before its reuse. PCSX2: the GIF packet's build fell from 1.04 to 0.51 ms a frame. PCSX2 does
not charge the EE's clock for the GS's drawing (GS Finish reads 0.00 ms before and after), so the overlap itself is
measured only on the hardware.

### 5. The vertical blank was polled — Fixed ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N10), Verified (PCSX2)

`graph_wait_vsync` spins on the CSR's VSINT bit (`ee/graph/src/graph.c`), and `WaitVSync` decided which field it was
from the clock (59.94 Hz hardcoded, so PAL's 50 fields were counted wrong). Now an `INTC_VBLANK_S` handler
(`FPS2VerticalBlank`: `AddIntcHandler`, then `EnableIntc`) counts the fields and signals a semaphore (`iSignalSema`, at
most one signal; `ExitHandler` before it returns), and `WaitVSync` sleeps in `WaitSema` until the field `FGSFieldPacer`
picks from the count. The EE thread no longer spins, a frame lasts a whole number of fields in either region, and the
count is what a second thread (streaming, audio) can wait on too. The region comes from ROMVER (`graph_get_region`;
libgraph's `graph_initialize` did the same, without centring PAL's picture).

### 6. All of the transform and lighting ran on the EE's core — Fixed ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N14), Verified (PCSX2: VU1Conformance, ShooterGame)

`FGSSceneRenderer` transformed, clipped, skinned and lit every vertex in C++ on the EE core, and VU0 and VU1 were idle.
Now the static meshes' batches inside the guard band (almost all of a frame's) go to VU1: `Private/VU1/VU1Programs.vsm`
(StaticUnlit and StaticLit, hand-written for `dvp-as`, which LeonBuildTool runs in the PS2 build) transforms the LPS2 v2
streams, lights them with the ambient share and one sun, culls the triangles outside a side of the view (CLIP) or
facing away, and XGKICKs a PACKED packet (PATH1) while VIF1 unpacks the next batch into the other half of its memory.
The frame is one VIF1 chain: the CPU's writes by DIRECT (PATH2), the batches' streams by REF where the meshes keep them,
FLUSH where a DIRECT follows batches; PATH3 is not used, and the GIF is reset at `InitDisplay` (a PATH3 packet left by
what ran before the ELF held the GIF in PCSX2 and stalled every DIRECT). `VU1Conformance` checks the programs against
the C++ emitter on the EE (plan D2), and `-novu1` routes everything through the emitter. Still on the EE: the batches
across the near plane or the guard band (the C++ clipper, D8), the lit draws with a second directional or a point light,
skinned meshes (their LPS2 format is not built yet, N21), lines, decals and the canvas.

### 7. Files go through the ROM's FILEIO — Partly fixed ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N24)

libcglue routes `open("host:...")` to `fioOpen` (`ee/libcglue/src/ps2sdkapi.c`, `__fioOpenHelper`), the ROM's FILEIO
RPC: correct, but the slowest path, limited in open files and without 64-bit offsets. `fileXio` (IOMANX and FILEXIO
modules from the SDK, `libfileXio`) is faster and is what disc and USB loaders expect; `libcdvd`'s `sceCdRead` /
streaming (`sceCdSt*`) is how a game reads its disc at full speed. For the pak on a disc: one `sceCdRead` of the index,
then reads by sector, fits the pak's 2 KB alignment (`LeonPak -align=2048`).

N24 took the reads off the game thread instead of changing the driver: `FAsyncIOSystem`'s IO thread (`PS2AsyncIO`,
one priority above the game's) reads the pak through a second FILEIO handle in 64 KB chunks and sleeps in each RPC,
so the disc's time is no frame's; `LoadPackageAsync` and the map's preload use it. fio and libcglue lock each call
(`_fio_io_sema`, `__fdman_sema`), so the two threads may both call them. fileXio or `sceCdRead` would still read
faster.

N24b measured what a read of the emulated disc costs through FILEIO: about 20 ms before its bytes (the `lseek` and
`read` RPCs, the drive's seek) and 0.7 ms a KB. Reading a package's few KB at a time left the load bound by the calls,
so the reads became fewer and larger: the IO thread takes the queued read nearest ahead of the last one and coalesces
the ones close to it (`FAsyncIOSystem::CoalesceBytes`, 128 KB, gaps up to 16 KB), the game thread's pak handle reads
64 KB blocks forward and serves the next entries from them, a handle skips the `lseek` when it is already there, and
the map loads through the same queue. With the pak in its open order the load reads each byte about once.

### 8. The EE mixes every voice — Fixed ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N19)

`FSoftwareAudioMixer` mixed 24 voices at 48 kHz on the EE and streamed the result through audsrv. The sounds are now
the SPU2's ADPCM, uploaded once to its RAM (`audsrv_load_adpcm`) and played on core 1's 24 hardware voices
(`audsrv_ch_play_adpcm`, `audsrv_adpcm_set_volume_and_pan`), without EE time; the PC keeps the same model (the ADPCM
decoded, the same voices and volume steps), so it still sounds like the PS2 ([ARCHITECTURE.md](../../../../Docs/ARCHITECTURE.md#audio)).
audsrv's limits: a voice cannot be keyed off (a sound that finds no voice is dropped; the music is muted), a sample id
is its descriptor's EE address, a second load of an id is ignored (no streaming into a resident sample), and SPU2 RAM
is allocated as a stack from 0x5010.

### 9. Smaller notes

- **Stack and heap** — Verified: the SDK's linkfile gives a 128 KB stack (`_stack_size`) at the top of RAM and the
  heap everything between the ELF and the stack (`_heap_size = -1`). The engine's largest stack frame is 4 KB
  (`objdump`: the libgcc unwinder, then `CheckFailedImpl` at 2 KB); 128 KB is enough for the engine's call depth.
- **Doubles** — Verified: the EE's FPU is single precision; a `double` is a libgcc software call. ShooterGame has about
  250 call sites of the `__*df*` helpers (time, config parsing), none in the per-vertex paths. Keep `double` out of the
  renderer and the physics.
- **The clock** — Verified: `GetTimerSystemTime` counts the bus clock (147.456 MHz), which `FPS2PlatformTime` uses.
  `nanosleep` is a kernel alarm plus a semaphore (`ee/libcglue/src/sleep.c`), so `FPlatformProcess::Sleep` does
  sleep.
- **libpad** — Verified: the same libpad talks to the ROM's PADMAN and to XPADMAN (`PAD_BIND_RPC_ID*_OLD/NEW`); the
  buffer is 256 bytes, 64-byte aligned, as required. The actuators (`padSetActAlign` once the pad is stable in
  DualShock mode, then `padSetActDirect`) and the pressure mode are used since N24; not the multitap (`libmtap`).
- **The scratchpad** (16 KB at `0x70000000`, `.spad` in the linkfile; nothing of the SDK uses it): Core's
  `FScratchpad` since [ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N15, the scene renderer's frame lists and
  the emitter's per-batch vertices. The chain never REFs into it (the DMA controller reaches it only through the
  fromSPR / toSPR channels).
- **VU0** runs no microprogram: the EE uses it in macro mode (COP2) for `FVectorMath` (N15), as libvux / math3d do,
  without any setup.
- **Memory card** (`libmc`, N24): the ROM's MCMAN / MCSERV after SIO2MAN (`FPlatformMisc::LoadIopModule`: once each,
  the pads share SIO2MAN); the transfers go through a 64-byte aligned buffer; each call waits with `mcSync(0)`.
- **`libmath3d`** is no longer linked: the legacy `DrawBox` path it backed went in
  [ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N2; the scene renderer has its own math.

## What to do next

| Priority | Change | Where |
|---|---|---|
| 1 | Confirm the IOP reset in PCSX2 and on hardware (uLaunchELF), the pad included | TESTING.md, PS2 checklist |
| 2 | ~~Double-buffer the frame packet, no `TArray` copy (finding 4)~~ Done in N11 | `PS2GSContext.cpp`, `FGSGifPacket` |
| 3 | ~~A vertical blank handler and semaphore for `WaitVSync` (finding 5)~~ Done in N10 | `PS2VerticalBlank.cpp` |
| 4 | `leonrun` in the local gates (`RunGates.bat`; the repository has no CI): build ShooterGame for PS2, run the headless botmatch, compare `Botmatch OK` | RunGates |
| 5 | ~~VU1 transform, a phase of its own (finding 6)~~ Done in N14 (static meshes; skinned with N21) | Renderer, PS2RHI |
| 6 | fileXio or libcdvd for the disc (the reads are off the game thread since N24); ~~ADPCM voices on the SPU2~~ Done in N19 | Core, AudioMixer |
