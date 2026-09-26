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
| libpatches (`patches`) | `sbv_patch_enable_lmb`, `sbv_patch_disable_prefix_check` | Core (`FPS2PlatformMisc::InitializeIop`) | loading `audsrv.irx` from EE memory |
| libcglue + newlib | `open` / `read` / `lseek` / `close`, `opendir`, `nanosleep`, `sbrk`, `printf` | Core (`PS2PlatformFile`, `PS2PlatformProcess`, `PS2PlatformMemory`) | files through the ROM's FILEIO (`host:`, `cdrom0:`), sleeping, the heap, the EE log |
| libpad (`pad`) | `padInit`, `padPortOpen`, `padGetState`, `padRead`, `padSetMainMode` | ApplicationCore (`PS2InputInterface`) | the DualShock on port 0 |
| libaudsrv (`audsrv`) | `audsrv_init`, `audsrv_set_format`, `audsrv_play_audio`, `audsrv_available` | AudioMixer (`PS2AudioOutput`) | the EE's mix streamed to the SPU2 |
| libgraph (`graph`) | `graph_initialize`, `graph_vram_allocate`, `graph_vram_clear`, `graph_set_framebuffer_filtered`, `graph_wait_vsync`, `graph_shutdown` | PS2RHI | the CRTC, the VRAM layout, the flip |
| libdma (`dma`) | `dma_channel_initialize`, `dma_channel_fast_waits`, `dma_channel_send_normal`, `dma_wait_fast` | PS2RHI (`FlushFrame`) | the frame's GIF packet (PATH3) |
| libpacket (`packet`) | `packet_init`, `packet_free` | PS2RHI | the DMA buffer |
| libdraw (`draw`) | `draw_wait_finish` | PS2RHI | waiting for the GS's FINISH |
| libmath3d (`math3d`) | `create_view_screen` | PS2RHI (`PS2Draw3D`) | the legacy `DrawBox` path's projection |

The GIF packets themselves are built by `FGSGifPacket` (GSCore) from `FGSCommandList`, not by libdraw: the GS's
registers are the engine's own contract, shared with the reference rasterizer and the desktop's emulator.

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

### 4. The frame is sent synchronously — Open (performance)

`FlushFrame` builds the GIF packet into a `TArray`, copies it into the `packet_t` (`memcpy`), sends it with
`dma_channel_send_normal` (which writes back the data cache: `SyncDCache`, so there is no coherency bug), then waits
for the DMA and for the GS's FINISH before the EE starts the next frame. The EE and the GS never work at the same time,
and the frame is copied twice. The SDK supports the usual alternative: two packets (libpacket2 or two `packet_t`),
the EE building frame N+1 while the DMA sends frame N, waiting only before a packet is reused
(`dma_channel_wait`). Expected gain: the GS's drawing time, up to a few milliseconds a frame. Measure with
`-LogFrameTimes` in PCSX2 before and after.

### 5. The vertical blank is polled — Open (pacing)

`graph_wait_vsync` spins on the CSR's VSINT bit (`ee/graph/src/graph.c`), and `WaitVSync` decides which field it is
from the clock. A vertical blank interrupt handler (`AddIntcHandler(INTC_VBLANK_S, ...)` counting fields and
signalling a semaphore) gives the exact field count for `SyncInterval` and lets the EE thread sleep instead of
spinning; it is also what a second thread (streaming, audio) would need.

### 6. All of the transform and lighting run on the EE's core — Open (the biggest performance lever)

`FGSSceneRenderer` transforms, clips, skins and lights every vertex in C++ on the EE core; VU0 and VU1 are idle. The
SDK has the pieces for the PS2's own pipeline: VIF1 packets (`packet2`, `packet2_vif.h`, `vif_codes.h`), VU1
microprograms uploaded with `packet2_vif_add_micro_program` (the `draw3d` / `libvux` samples), and the `dvp-as`
assembler in the ps2dev toolchain (`$PS2DEV/dvp/bin`; the local build in `/tmp` of this review has none). A VU1
program per vertex format (static lit, skinned, unlit) reading the GS registers the command list already defines is
the path to the PS2's polygon counts. It is a design change of the renderer (the reference and the emulator stay the
oracle), to plan as its own phase.

### 7. Files go through the ROM's FILEIO — Open

libcglue routes `open("host:...")` to `fioOpen` (`ee/libcglue/src/ps2sdkapi.c`, `__fioOpenHelper`), the ROM's FILEIO
RPC: correct, but the slowest path, limited in open files and without 64-bit offsets. `fileXio` (IOMANX and FILEXIO
modules from the SDK, `libfileXio`) is faster and is what disc and USB loaders expect; `libcdvd`'s `sceCdRead` /
streaming (`sceCdSt*`) is how a game reads its disc at full speed. For the pak on a disc: one `sceCdRead` of the index,
then reads by sector, fits the pak's 2 KB alignment (`LeonPak -align=2048`).

### 8. The EE mixes every voice — Open (CPU)

`FSoftwareAudioMixer` mixes 24 voices at 48 kHz on the EE and streams the result through audsrv. The SPU2 has 48
hardware voices that play ADPCM from its own 2 MB: audsrv's `audsrv_load_adpcm` / `audsrv_ch_play_adpcm` play the
sound effects without EE time. The PC then needs the same model (ADPCM in the cook, a voice limit) to keep D4 of
[ps2-preview](../../../../Docs/PLANS/ps2-preview.md): the PC changes to match the PS2.

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
  buffer is 256 bytes, 64-byte aligned, as required. Not used: the actuators (`padSetActAlign`, `padSetActDirect`:
  vibration) and the multitap (`libmtap`).
- **The scratchpad** (16 KB at `0x70000000`, `.spad` in the linkfile) is unused: the place for the renderer's
  per-batch vertex buffers once the transform moves (finding 6), or for the GIF packet chain.
- **Memory card** (`libmc`) is unused: saving is a later milestone (the PS2 platform file refuses writes).
- **`libmath3d`** backs only the legacy `DrawBox` projection; the scene renderer has its own math.

## What to do next

| Priority | Change | Where |
|---|---|---|
| 1 | Confirm the IOP reset in PCSX2 and on hardware (uLaunchELF), the pad included | TESTING.md, PS2 checklist |
| 2 | Double-buffer the frame packet, no `TArray` copy (finding 4) | `PS2GSContext.cpp`, `FGSGifPacket` |
| 3 | A vertical blank handler and semaphore for `WaitVSync` (finding 5) | `PS2DynamicRHI.cpp` |
| 4 | `leonrun` in the Linux CI: build ShooterGame for PS2, run the headless botmatch, compare `Botmatch OK` | CI workflow |
| 5 | VU1 transform, a phase of its own (finding 6) | Renderer, PS2RHI |
| 6 | fileXio or libcdvd for the disc; ADPCM voices on the SPU2 | Core, AudioMixer |
