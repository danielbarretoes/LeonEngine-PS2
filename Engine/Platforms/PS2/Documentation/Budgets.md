# PS2 budgets

The EE has 32 MB of main RAM (the kernel keeps the first 1 MB) and the GS has 4 MB of VRAM. Every phase that changes
Core or a PS2 module records its numbers here, so growth is caught when it happens rather than when the port runs
out of memory.

## Limits

| Item | Limit | Enforced by |
|---|---|---|
| FName pool | 16 KB blocks, at most 16 (256 KB), 4 096 hash buckets (16 KB) | `FPS2PlatformProperties::NamePool*`; exhausting the pool is a fatal error |
| Reflection data (CoreUObject) | 400 KB | CoreUObject code, generated code and tables, and the heap used to construct the types; measured through TestPAL (P9) |
| UObject array | 8 192 objects (96 KB: 12-byte slots) | `FPS2PlatformProperties::MaxObjectsInGame`; running out is a fatal error that logs the capacity |
| SPU2 RAM for sounds | 2028 KB (2 076 656 bytes: the SPU2's 2 MB less the 0x5010 bytes audsrv keeps below its first sample) | `FSpuAdpcm::SoundRamBytes` ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N19): the cook fails a map whose sounds do not fit (`<Platform>-SoundReport.txt`), and `FAudioDevice` logs an error for a sound that finds no room once the unreferenced buffers on top are freed, on the desktop too |
| SPU2 voices | 24 (core 1): 2 for music, 22 for effects, the last 4 free ones for sounds above the default priority | `FAudioDevice` (a sound that finds no voice is dropped: audsrv cannot key one off) |
| GMalloc's small-block arena | 2 MB reserved at start-up (blocks up to 1 KB), plus a 128 KB tag table outside Shipping | `FPS2PlatformProperties::SmallBlockArenaSize`; a small block that does not fit goes to the system heap and counts as an overflow (`MemoryTags:` `arena_overflows`) |
| GMalloc, all of it | 24 576 KB (`Total`) | `[Core.MemoryBudgets]` of PS2Engine.ini (N17): over 90 % logs a warning once, over the budget is a fatal error naming the tag |
| A map's textures (GS VRAM) | 1 856 KB, the texture arena (`MapVramKB=0`) | the cook's hard budgets ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N23, `[/Script/LeonEd.CookSettings]`, [TOOLS.md](../../../../Docs/TOOLS.md#the-cook)): an error fails the cook (`<Platform>-VramReport.txt`) |
| A map's RAM (estimate) | 24 576 KB (`MapRamKB=0`: `Total`): 1 536 KB + the cooked bytes x 200 % | the same: `<Platform>-RamReport.txt` (de_puerto: 559 KB of its own, 1 978 KB with the common packages, estimated 7 028 KB, measured GMalloc peak 5 176 KB from host:; de_leon: 246 KB cooked with the common packages, estimated 2 028 KB; measured GMalloc peak 2 009 to 2 021 KB from the disc; since N27 and N28's art 1 433 KB, estimated 4 402 KB, measured 4 947 KB from host:) |
| A map's sounds (SPU2 RAM) | 2 028 KB (`MapSoundRamKB=0`) | the same (N19's check, now configurable) |
| A mesh | 4 096 triangles, 64 bones | the same (`MaxMeshTriangles`, `MaxMeshBones`) |
| A texture | 256 texels a side, 8 bits a texel (PSMT8 or PSMT4) | the same (`MaxTextureSize`, `MaxTextureBitsPerPixel`) |
| The disc | a CD: 700 MB (ShooterGame's image: 8 464 384 bytes with de_puerto; 7 784 448 bytes at 0.25.0; 7 122 944 at 0.24.0; 5 056 512 at N23) | `BuildCookRun -iso` (N23) |
| A memory card save | the first: the folder (2 KB), `icon.sys` (1 KB), the icon (33 KB: a 128 x 128 16-bit texture) and the save (1 KB for ShooterGame's settings); then the save alone | `FMemoryCardSaveGameSystem` ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N24): a card with less room fails the save (`NoSpace`) before it writes |
| Memory tags | EngineMisc 2 048 (3 072 from N27 until N14b), UObject 2 048, LoadMapMisc 1 024, Textures 6 144, Meshes 4 096, Animation 2 048, Audio 4 096, Physics 512, AI 256, SceneRender 1 536, GameMisc 1 536, Temporary 256, RenderLists 4 096 (N14b) (KB) | the same section, per `ELLMTag` (`LLM_SCOPE`); the measured peaks are below |

## The release (0.24.0)

[ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N31: ShooterGame on de_leon rebuilt with the real art, a bot
match of two rounds with seed 7 watched through a bot's eyes (`MeasurePS2`), PCSX2 2.8.2. The milestones' rows, from
the tables below:

| Row | Content | fps | p50 / p95 / p99 | world | scene | GIF / frame (the EE's) | GMalloc peak | Compares with |
|---|---|---:|---:|---:|---:|---:|---:|---|
| N1 baseline (0.22.0's start) | the blockout, flat | 25.4 | 33.5 / 50.3 / 66.8 ms | 9.0 ms | 11.8 ms | 21.4 KB | 2 266 KB | by part only |
| N11 (0.22.0) | the blockout | 25.7 | 33.5 / 50.3 / 66.8 ms | 7.9 ms | 13.0 ms | 25.0 KB | 2 385 KB | by part only |
| N18 (0.23.0, the fixed step) | the blockout, baked floor | 29.65 | 33.5 / 33.5 / 50.3 ms | 3.1 ms | 15.6 ms | 45.9 KB | 1 949 KB | N22 |
| N28 (de_leon rebuilt) | the real art | 8.76 | 117.0 / 133.5 / 150.3 ms | 19.7 ms | 83.4 ms | 277.0 KB | 4 947 KB | N14b, N29's steps |
| N29 (30 fps) | the real art | 29.81 | 33.5 / 33.5 / 33.5 ms | 4.9 ms | 8.4 ms | 19.1 KB | 4 261 KB | 0.24.0 |
| **0.24.0** | the real art | **29.95** | 33.5 / 33.5 / 33.5 ms | 4.7 ms | 8.4 ms | 19.1 KB | 4 432 KB | N29 |
| 0.24.0 from the disc (pak in open order) | the real art | 29.96 | 33.5 / 33.5 / 34.3 ms | 5.3 ms | 10.3 ms | 25.7 KB | 4 256 KB | N24b's disc row |
| **0.25.0** ([ps2-polish](../../../../Docs/PLANS/ps2-polish.md), from the main menu) | the real art, the sky | 29.62 | 33.5 / 33.5 / 33.5 ms | 4.1 ms | 6.6 ms | 11.0 KB | 4 794 KB | ps2-polish P8b |
| de_puerto (the second map, `MeasurePS2 -Map /Game/Maps/de_puerto`) | its own art and sky | 29.92 | 33.5 / 33.5 / 34.0 ms | 5.4 ms | 6.9 ms | 9.5 KB | 5 176 KB | another map: by part only |

Which rows compare: before N18 the game stepped by each frame's time, so every row (N1 to N20, N13, N17, N19) plays
another match and compares only by part (the world's, the scene's, the audio's milliseconds). From N18 on a build that
does not change the simulation plays the same match, so the rows compare whole when their content is the same: N18
with N22 (the blockout); N27 with N14 and its `-novu1` row; the N30e base with N15 and `-nospr`; N28 with N14b (on
N28) and N29's steps; N29 with 0.24.0 (the match is the same, a render build apart); and the disc rows of N24b with
0.24.0's. The runs from `host:` and from the disc read differently and play different frames (their scene differs by
the view), so a host row and a disc row do not compare.

At 0.24.0 every frame falls on the second field (33.4 ms): the EE works about 17.5 ms of it (the world 4.7, the scene
8.4, the canvas 2.9, the chain 1.6) and waits 16.1 for the vertical blank. The disc: the first frame 2.93 s after the
engine's start with the pak in its open order (10.08 s in path order), no file opened after it, the worst frame after
it 50.05 ms. The release ISO is 7 122 944 bytes; ShooterGame's ELF 2 102 974 bytes of text. PCSX2 is not the hardware:
it does not charge the EE for the GS's drawing nor model the EE's caches, so the EE / GS overlap, the scratchpad and
the cache misses only show on a console.

## How to measure

- **ELF sections**: `mips64r5900el-ps2-elf-size <elf>` inside the ps2dev image, for example
  `docker run --rm -v <repo>:/leon <image> mips64r5900el-ps2-elf-size /leon/Game/ShooterGame/Binaries/PS2/ShooterGame.elf`
  (`<image>` is the digest in `Engine/Platforms/PS2/Source/Programs/LeonBuildTool/LeonBuildPS2.cmake`). The stripped
  size comes from `mips64r5900el-ps2-elf-strip -o /tmp/x.elf <elf>`.
- **Heap / names**: run TestPAL (`Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build`) and read
  the `LogTestPAL` lines in the PCSX2 log (`%USERPROFILE%\Documents\PCSX2\logs\emulog.txt`): GMalloc peak / current,
  process size (program image + heap high-water, `sbrk`) and the name pool. Since P9 also the reflected type counts
  with the GMalloc bytes allocated while `UObjectBaseInit` and `ProcessNewlyLoadedUObjects` constructed them
  (`GetUObjectReflectionStats`; the object array is not included), and the object array's capacity and live objects.
  Since P10 also the `LogGarbage: Display: GC budget:` line (the cost of marking and destroying 2 000 objects) and a
  final collection after the tests; since P11 the `Package budget:` line (a save / load round trip of 50 objects).
- **Reflection code in the ELF**: `mips64r5900el-ps2-elf-nm -S -C --size-sort` over `TestPAL.elf`, summing the
  CoreUObject symbols, the generated `Z_Construct_*` / `exec*` / `StaticClass` / `RegisterReflection_*` code and the
  `_Statics` tables.
- **In game**: the stats overlay's `RAM` line shows GMalloc's current / peak / budget (since N17; before, the process
  size), and `stat memory` every memory tag. `-LogFrameTimes` ends with `MemoryTags:` (each tag's peak, the arena's
  peak and overflows), which MeasurePS2 adds to its CSV.

## Measurements

All builds are Development (`-O2`). `text` / `data` / `bss` are bytes.

| Version | Target | text | data | bss | Stripped ELF | Notes |
|---|---|---:|---:|---:|---:|---|
| 0.12.0 | ThirdPerson | 421 803 | 6 896 | 36 584 | 490 072 | before the new Core |
| 0.12.0 | BlankProgram | 275 098 | 6 156 | 33 560 | | |
| P2 | ThirdPerson | 429 899 | 6 928 | 37 672 | | new Core (UE_LOG, FTicker delegates), no section GC |
| P2 | ThirdPerson | 361 566 | 6 884 | 36 096 | 371 068 | `-ffunction-sections -fdata-sections -Wl,--gc-sections` |
| P2 | BlankProgram | 266 578 | 6 156 | 33 560 | | section GC |
| P2 | TestPAL | 479 822 | 6 220 | 37 416 | | Core automation tests, section GC |
| P3 | ThirdPerson | 361 566 | 6 884 | 36 096 | 371 068 | Core math unused by the game yet: section GC drops all of it |
| P3 | TestPAL | 571 870 | 6 232 | 38 096 | | Core math and its tests (+92 KB of text) |
| P4 | ThirdPerson | 561 070 | | | | file layer, config, Json, Projects; `__cxa_guard`, the global `operator new` and `__cxa_pure_virtual` pulled in libstdc++'s unwinder and demangler |
| P4 | ThirdPerson | 442 144 | 6 916 | 29 848 | 450 152 | `-fno-threadsafe-statics` and `PS2PlatformRuntime.cpp` (new / delete through `FMemory`, own pure-virtual handler) |
| P4 | BlankProgram | 178 560 | 6 132 | 27 084 | | same fix |
| P4 | TestPAL | 731 204 | 6 268 | 32 392 | | Core services, Json, Projects and their tests |
| P5 | ThirdPerson | 444 240 | 6 924 | 29 864 | 452 328 | ApplicationCore / RHI / PS2RHI / Launch on Core types (UE_LOG, TSharedRef windows, FCString) |
| P5 | BlankProgram | 178 560 | 6 132 | 27 084 | | unchanged |
| P5 | TestPAL | 731 244 | 6 268 | 32 392 | | FIntVector4 test |
| P6 | ThirdPerson | 444 240 | 6 924 | 29 864 | 452 328 | unchanged (the game mode moved to `TUniquePtr`) |
| P6 | BlankProgram | 179 588 | 6 136 | 27 089 | | reports through `UE_LOG` instead of `printf` (+1 KB of text) |
| P6 | TestPAL | 731 244 | 6 268 | 32 392 | | unchanged (the removed glm tests were desktop-only) |
| P7 | ThirdPerson | 444 240 | 6 924 | 29 864 | 452 328 | unchanged (the axes and units switch is desktop-only; the game keeps its own frame) |
| P7 | BlankProgram | 179 588 | 6 136 | 27 089 | | unchanged |
| P7 | TestPAL | 731 244 | 6 268 | 32 392 | | unchanged |
| P8 | ThirdPerson | 444 280 | 6 924 | 29 864 | 452 328 | the module table gains the `RegisterReflection` pointer |
| P8 | BlankProgram | 179 588 | 6 136 | 27 089 | | unchanged |
| P8 | TestPAL | 731 252 | 6 268 | 32 392 | | module table pointer |
| P9 | ThirdPerson | 444 320 | 6 924 | 29 928 | 452 328 | no CoreUObject; only `FModuleManager`'s registration hook: `StartupStaticallyLinkedModules` +40 bytes, the manager +4 bytes (the 64-byte aligned `.bss` grows by 64); stripped size unchanged |
| P9 | BlankProgram | 179 628 | 6 136 | 27 097 | 186 804 | the same hook |
| P9 | TestPAL | 1 019 420 | 6 320 | 36 128 | 1 026 920 | CoreUObject and its 27 tests (+288 168 bytes of text, breakdown below) |
| P10 | ThirdPerson | 444 320 | 6 924 | 29 928 | 452 328 | unchanged: no CoreUObject, and the Core additions (`FExec`, `FSelfRegisteringExec`, the UObject delegate templates) are not referenced, so section GC drops them |
| P10 | BlankProgram | 179 628 | 6 136 | 27 097 | | unchanged |
| P10 | TestPAL | 1 156 972 | 6 348 | 37 840 | 1 164 520 | garbage collection, references, config, Exec and 21 more tests (+137 552 bytes of text; about 30 KB of it is the P10 runtime by symbol: GC 11.6 KB, soft / weak references 8.4 KB, config 5.7 KB, Exec 4.6 KB; the rest is the new tests, their fixtures and template code) |
| P11 | ThirdPerson | 444 376 | 6 924 | 29 928 | 452 328 | still no CoreUObject, but `FArchive` gains two virtuals (`operator<<(UObject*&)`, `GetLinker`) and the package version fields: the two archive vtables it links grow by 8 bytes each, the two 8-byte no-op bodies are linked, and the archive constructors store the version (+56 bytes of text); stripped size unchanged |
| P11 | BlankProgram | 179 628 | 6 136 | 27 097 | 186 804 | unchanged (no archive) |
| P11 | TestPAL | 1 373 348 | 6 376 | 39 136 | 1 380 840 | packages and 14 more tests (+216 376 bytes of text; about 108 KB of it is the P11 runtime by symbol, breakdown below; 70 KB the package tests and their fixtures; the rest template code) |
| P13 | ThirdPerson | 668 000 | 6 984 | 33 880 | 675 944 | the object system boots: InputCore's `FKey` is a reflected struct, so InputCore depends on CoreUObject and the game links and starts it (+223 624 bytes of text, breakdown below) |
| P13 | BlankProgram | 179 628 | 6 136 | 27 097 | 186 804 | unchanged |
| P13 | TestPAL | 1 373 348 | 6 376 | 39 136 | 1 380 840 | unchanged (Core, CoreUObject, Json and Projects did not change) |
| P14 | ThirdPerson | 668 008 | 6 984 | 33 880 | 676 072 | ApplicationCore's window icon takes texels (`FGenericWindow::SetIcon(int32, int32, const uint8*)`) instead of a PNG path (`SetIconFromFile`): the base's 8-byte `return false` body is the same, but under its new name it lands elsewhere among the ApplicationCore functions (right after `SetCursorCaptured`) and the alignment padding of the text grows by 8 bytes (+8 bytes of text, no new code) |
| P14 | BlankProgram | 179 628 | 6 136 | 27 097 | 186 804 | unchanged |
| P14 | TestPAL | 1 373 348 | 6 376 | 39 136 | 1 380 840 | unchanged |
| P16 | ThirdPerson | 668 048 | 6 984 | 33 880 | 676 200 | `UObject::IsEditorOnly` (the cook's editor-only objects): its 8-byte `return false` body and one more slot in each of the 8 CoreUObject vtables the game links (`UObject`, `UField`, `UStruct`, `UScriptStruct`, `UClass`, `UEnum`, `UFunction`, `UPackage`) (+40 bytes of text). The PS2 launch does not link PakFile yet |
| P16 | BlankProgram | 179 628 | 6 136 | 27 097 | 186 804 | unchanged |
| P16 | TestPAL | 1 449 588 | 6 384 | 39 600 | 1 457 128 | the PakFile module (`FPakFile`, `FPakPlatformFile`, `FPakWriter`), `FSHA1` and their tests, which run on paks in memory (+76 240 bytes of text; about 65 KB of it in the `FPak*` and `FSHA1*` symbols, the runtime and the pak tests), and `UObject::IsEditorOnly` with its test fixture |
| 0.20.1 | ThirdPerson | 674 384 | 6 984 | 34 008 | — | measured by the CI of 0.20.1, since removed (its "ELF sizes (G3)" step: `size`, not stripped): P17 to P21 and the audit's Core, CoreUObject and Launch changes (+6 336 bytes of text since P16) |
| 0.20.1 | BlankProgram | 180 156 | 6 136 | 27 225 | — | +528 bytes of text (Core) |
| 0.20.1 | TestPAL | 1 458 584 | 6 384 | 39 808 | — | +8 996 bytes of text: the audit's config, archive, linker and pak fixes and their tests (120 tests) |
| GS P3 | ThirdPerson | 685 842 | 7 056 | 33 984 | 693 992 | measured with an EE toolchain built from ps2toolchain-ee (GCC 15.2.0) and ps2sdk sources, not the pinned image (BlankProgram, unchanged in code, measures 24 bytes less than in 0.20.1). The PS2 RHI draws through `FGSCommandList`: GSCore's register encoders and the GIF packet builder replace libdraw's helpers (+11 458 bytes of text) |
| GS P3 | GSConformance | 221 511 | 6 300 | 27 488 | 228 788 | new: the GS conformance scenes on the PS2 |
| GS P3 | BlankProgram | 180 132 | 6 136 | 27 225 | 187 316 | no code change: the toolchain difference |
| GS P3 | TestPAL | 1 506 600 | 6 380 | 40 384 | 1 514 088 | GSCore and its 8 tests (+48 016 bytes of text) |
| E1 | ShooterGame | 1 615 998 | 6 452 | 43 184 | — | new ([ps2-engine](../../../../Docs/PLANS/ps2-engine.md) E1): the gameplay framework (Engine, UMG, SlateCore, AIModule, AnimationCore, PhysicsCore without Jolt, AudioMixer silent, PakFile) and the game, headless. About 85 KB of it is generated reflection code and tables (`Z_Construct_*`, `StaticClass`, `exec*`, `RegisterReflection_*`, `_Statics`); the construction heap and the runtime numbers wait for the botmatch in PCSX2 |
| E1 | ThirdPerson | 687 890 | 7 056 | 33 984 | — | +2 048 bytes of text: the PS2 launch reads `LeonCommandLine.txt` (`FFileHelper`) |
| E2 | ShooterGame | 1 683 806 | 7 016 | 43 280 | — | the Renderer on the PS2 ([ps2-engine](../../../../Docs/PLANS/ps2-engine.md) E2, [ps2-gs-parity](../../../../Docs/PLANS/ps2-gs-parity.md) P5): the scene, the GS scene renderer (transform, clipping, skinning and lighting on the EE, the texture cache, the world effects) and `FPS2RendererModule` (+67 808 bytes of text) |
| E2 | ThirdPerson | 688 354 | 7 056 | 33 984 | — | `FGSDrawEnvironment` (GSCore) sets up the PS2 frame and `FPS2RHI::AllocateTextureArena` (+464 bytes of text) |
| E2 | GSConformance | 221 967 | 6 300 | 27 488 | — | `FGSDrawEnvironment` (+456 bytes of text) |
| E2 | TestPAL | 1 506 608 | 6 380 | 40 384 | — | unchanged but for the toolchain's alignment (+8 bytes) |
| E3 | ShooterGame | 1 688 830 | 7 016 | 43 280 | — | paletted textures in the texture cache (PSMT8 / PSMT4 with a CLUT, `FGSTextureLayout`) and the pak's device-root paths (+5 024 bytes of text) |
| E3 | ThirdPerson | 688 354 | 7 056 | 33 984 | — | unchanged |
| E3 | GSConformance | 221 967 | 6 300 | 27 488 | — | unchanged |
| E3 | TestPAL | 1 514 864 | 6 380 | 40 592 | — | `FGSTextureLayout`, the pak's device-root paths and their 2 tests (+8 256 bytes of text) |
| E4 | ShooterGame | 1 694 542 | 7 032 | 43 280 | — | the pad through the viewport client, the buy menu's input component, the stick's turn rates, the sync interval and `-LogFrameTimes` (+5 712 bytes of text) |
| E4 | ThirdPerson | 688 482 | 7 072 | 33 984 | — | `FPS2RHI::SetSyncInterval` in the RHI it links (+128 bytes of text) |
| E4 | GSConformance | 222 287 | 6 316 | 27 488 | — | the same (+320 bytes of text) |
| E4 | TestPAL | 1 514 864 | 6 380 | 40 592 | — | unchanged |
| E5 | ShooterGame | 1 710 670 | 7 036 | 67 488 | — | the SPU2 audio: `FSoftwareAudioMixer`, the PS2 `FAudioDevice`, libaudsrv and libpatches (+16 128 bytes of text; +24 208 of bss, libaudsrv's buffers). The sounds stay PCM16: 242 KB cooked |
| E5 | ThirdPerson | 688 482 | 7 072 | 33 984 | — | unchanged (no engine, no audio) |
| E5 | TestPAL | 1 514 864 | 6 380 | 40 592 | — | unchanged |
| V1 | ShooterGame | 1 716 718 | 7 040 | 67 512 | — | [ps2-preview](../../../../Docs/PLANS/ps2-preview.md) V1: the renderer settings (the TV's aspect), the shared audio device and its output, `FDualShockAnalog` (+6 048 bytes of text) |
| V1 | ThirdPerson | 688 466 | 7 072 | 33 984 | — | `FDualShockAnalog` inlined (-16 bytes of text) |
| V1 | TestPAL | 1 514 856 | 6 380 | 40 592 | — | the toolchain's alignment (-8 bytes) |
| N0 (-O3) | ShooterGame | 1 726 238 | 8 672 | 67 696 | — | [ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N0, the last build at -O3: CMake's GNU Release flags (`-O3 -DNDEBUG`) came after the toolchain's `-O2`, so Development and Shipping were -O3 though the docs said -O2 |
| N0 (-O3) | ThirdPerson | 696 418 | 8 608 | 35 456 | — | the same |
| N0 (-O3) | TestPAL | 1 514 424 | 6 388 | 40 680 | — | the same |
| N0 (-O3) | GSConformance | 224 847 | 6 364 | 27 488 | — | the same |
| N1 | ShooterGame | 1 580 566 | 8 672 | 67 696 | — | [ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N1, D9: -O2 at last (the toolchain forces `CMAKE_CXX_FLAGS_RELEASE`): -145 672 bytes of text (-8.4 %) for the EE's 16 KB instruction cache |
| 0.24.0 | ShooterGame | 2 102 974 | 22 132 | 114 792 | — | the release (ps2-shipping N31): VU1, VU0, the DMA chains, the SPU2, the async IO, the memory card, the animation runtime and CS's game since N1 (+522 408 bytes of text); the staged ELF is 4 734 720 bytes with its symbols |
| 0.24.0 | TestPAL | 1 573 688 | 17 704 | 78 552 | — | 171 tests on Win64 |

**P9 reflection in TestPAL** (`nm -S` over the ELF, bytes):

| Part | Size |
|---|---:|
| CoreUObject runtime code (objects, classes, properties, script containers, construction) | 183 288 |
| Generated code (`Z_Construct_*`, exec thunks, `StaticClass`, `RegisterReflection_*`; test fixtures included) | 13 840 |
| Generated tables (`_Statics` params, read-only data; test fixtures included) | 6 922 |
| CoreUObject data (vtables, field classes) | 2 281 |
| Reflection total in the ELF | 206 331 (201 KB) |
| Test fixtures and `System.CoreUObject.*` tests (not part of the budget; overlaps the generated rows) | 90 306 |
| Heap used to construct the compiled-in types and CDOs (TestPAL log) | 19 KB |
| `GUObjectArray` (8 192 slots × 12 bytes, heap) | 96 KB |

Reflection = ELF code and tables + construction heap ≈ 220 KB of the 400 KB budget, for 18 classes (8 of them
intrinsic, 10 test fixtures), 15 structs (13 NoExport Core structs), 2 enums, 8 functions and 117 properties. Each further reflected type
costs its generated code and tables plus its `UClass` / `UScriptStruct` and `FProperty` objects on the heap.

| Version | Program | GMalloc peak | Process | Name pool | Notes |
|---|---|---:|---:|---:|---|
| P2 | TestPAL (27 tests) | 68 KB | 600 KB | 85 names, 32 KB allocated (1 block + hash) | |
| P2 | ThirdPerson | | 0.5 MB | | overlay `RAM 0.5/32.0 MB`, 60 FPS, Draw3D `boxes=343 tris=278 emit=254` |
| P3 | TestPAL (35 tests) | 69 KB | 688 KB | 85 names, 32 KB allocated (1 block + hash) | math tests pass with the Win64 reference values |
| P3 | ThirdPerson | | 0.5 MB | | unchanged; 60 FPS, same Draw3D numbers |
| P4 | TestPAL (46 tests) | 70 KB | 836 KB | 101 names, 32 KB allocated (1 block + hash) | Core, Json and Projects tests; config read from memory |
| P4 | ThirdPerson | | 0.6 MB | | 60 FPS, same Draw3D numbers; PCSX2 host filesystem off, so `Character tuning from compiled defaults` |
| P5 | TestPAL (46 tests) | 70 KB | 836 KB | 101 names | unchanged |
| P5 | ThirdPerson | | 0.6 MB | | 60 FPS, same Draw3D numbers (now logged through LogRHI) |
| P6 | TestPAL (46 tests) | 70 KB | 836 KB | 101 names | unchanged |
| P6 | ThirdPerson | | 0.6 MB | | 60 FPS, same Draw3D numbers |
| P7 | TestPAL (46 tests) | 70 KB | 836 KB | 101 names | unchanged; logs `engine 0.14.0` |
| P7 | ThirdPerson | | 0.6 MB | | 60 FPS, same Draw3D numbers (`boxes=343 ... tris=278 ... emit=254`) |
| P9 | TestPAL (73 tests) | 188 KB | 1 236 KB | 263 names, 5 KB used of 32 KB allocated | `Reflection: 18 classes, 15 structs, 2 enums, 8 functions, 117 properties, 2 packages; construction heap 19 KB`; `UObject array: 8192 slots of 12 bytes (96 KB), 66 objects after registration`; 122 objects after the tests. The peak includes the 96 KB object array |
| P10 | TestPAL (93 tests) | 837 KB | 2 036 KB | 346 names, 8 KB used of 32 KB allocated | `Reflection: 24 classes, 19 structs, 3 enums, 14 functions, 186 properties, 2 packages; construction heap 30 KB`; 89 objects after registration, 91 after the tests, 89 after a final collection (0.33 ms). The peak comes from the GC budget test's 2 000 objects (about 330 bytes each); GMalloc ends at 227 KB |
| P11 | TestPAL (106 tests) | 849 KB | 2 260 KB | 446 names, 10 KB used of 32 KB allocated | `Reflection: 29 classes, 21 structs, 6 enums, 14 functions, 279 properties, 2 packages; construction heap 40 KB` (the package fixtures); 105 objects after registration, 107 after the tests, 105 after a final collection (0.34 ms). The peak is still the GC budget test; GMalloc ends at 241 KB |
| P11 | ThirdPerson | | 0.6 MB | | 60 FPS, same Draw3D numbers (`boxes=343 ... tris=278 ... emit=254` every 30 frames) |
| P13 | TestPAL (106 tests) | 849 KB | 2 260 KB | 446 names, 10 KB used of 32 KB allocated | unchanged: `TestPAL: PASSED (106 test(s), 0 failed)`, 105 objects after a final collection (0.339 ms) |
| P13 | ThirdPerson | | 0.9 MB | | the object system starts (`UObject array: 8192 objects, 98304 bytes`, `Object system started: 8192 object slots, transient package /Engine/Transient`); 60 FPS, same Draw3D numbers (`boxes=343 culled=291 backfaces=173 tris=278 keep=254 drop=24 clip=0 emit=254` every 30 frames). The overlay rounds to 0.1 MB: with the 692 KB image, the heap high-water is about 180 to 280 KB (P11: about 90 to 190 KB), 96 KB of it the object array |
| P16 | TestPAL (112 tests) | 1 157 KB | 2 328 KB | 450 names, 10 KB used of 32 KB allocated | `TestPAL: PASSED (112 test(s), 0 failed)`: the SHA-1 test and the 5 PakFile tests join; `Reflection: 30 classes, 21 structs, 6 enums, 14 functions, 280 properties, 2 packages; construction heap 41 KB` (the editor-only test class); 107 objects after a final collection (0.340 ms). The peak grows from 849 KB with the pak tests' buffers (a 70 000-byte entry, several copies of the test pak) |
| P16 | ThirdPerson | | 0.9 MB | | 60 FPS, same Draw3D numbers (`boxes=343 culled=291 backfaces=173 tris=278 keep=254 drop=24 clip=0 emit=254` every 30 frames) |

**P10 garbage collection** (TestPAL, `System.CoreUObject.GarbageCollection.Budget`: a chain of 2 000 objects from one
rooted head, 2 089 objects alive in total; `FPlatformTime` in PCSX2):

| Step | PS2 | Win64 (reference) |
|---|---:|---:|
| Collection that keeps everything (mark 2 089 objects, 2 000 through one reference each) | 9.98 ms | 0.22 ms |
| Collection that destroys the 2 000 objects: mark | 0.42 ms | 0.01 ms |
| — purge (`BeginDestroy`, `FinishDestroy`, destructors, `FMemory::Free`) | 16.07 ms | 0.22 ms |
| Heap: before / with the objects / after | 169 / 827 / 226 KB | |
| Final collection after the tests (89 objects left, none collected) | 0.33 ms | 0.02 ms |

About 5 µs per reachable object to mark and 8 µs per object to destroy on the EE: a full collection over a few
thousand objects costs a frame or two at 60 FPS, so it belongs where UE puts it (loading a map, restarting a round, a
timer of a minute). The heap left after the purge (57 KB above the start) is the capacity the name hash and the
collector's arrays keep for the next time, not leaked objects.

**P11 packages** (TestPAL, `System.CoreUObject.Package.Budget`: 50 `UPackageTestObject`s with every kind of
property filled and a 256-byte bulk payload each, saved to memory, destroyed and loaded back; `FPlatformTime` in
PCSX2):

| Step | PS2 | Win64 (reference) |
|---|---:|---:|
| Package size | 126 077 bytes (about 2.5 KB per object: tagged properties are verbose, each tag is 25+ bytes) | the same bytes |
| Save (collect exports, imports and names, then write) | 46.9 ms | 0.93 ms |
| Load (read the tables, create and serialize 50 objects, `PostLoad`) | 24.9 ms | 0.45 ms |
| Heap: before / with the objects / after destroying them (the package bytes stay registered) / loaded | 239 / 331 / 490 / 581 KB | |

A load holds the whole file in memory until its `EndLoad` (123 KB here) on top of the objects it creates; the objects
of this fixture cost about 1.8 KB each on the heap. The save runs the export serialization three times (two
collection passes, then the write), so it costs about twice the load. Saving is an editor / cook operation; the PS2
only loads.

P11 runtime code in TestPAL (`nm -S` by symbol, bytes; template instantiations are not attributed):

| Part | Size |
|---|---:|
| Linkers: `FLinker`, `FLinkerLoad`, `FLinkerSave`, the load context, the summary / import / export serializers | 31 600 |
| Save: `UPackage::Save`, the tagging archives, sorting | 30 096 |
| Tagged properties: `FPropertyTag`, `SerializeTaggedProperties`, every `SerializeItem` / `ConvertFromType`, `UObject::Serialize` | 25 132 |
| Package names: `FPackageName`, mount points | 13 520 |
| Loading API: `LoadPackage`, `StaticLoadObject`, `FindPackage`, `ConditionalPostLoad` | 4 856 |
| Bulk data: `FByteBulkData` | 2 800 |
| P11 total | 108 004 (105 KB) |
| Package tests and fixtures (not part of the budget) | 70 481 |

CoreUObject's runtime code in the ELF is now about 318 KB (P9 183 KB, P10 30 KB, P11 105 KB). The save path (about
30 KB) and most of `FPackageName`'s file conversions are only needed by the editor and the cook; a PS2 game build could
leave them out if the budget gets tight.

**P13 object system in ThirdPerson** (`nm -S` over the ELF by symbol name, bytes; template instantiations and the Core
code the object system newly references are not attributed):

| Part | Size |
|---|---:|
| CoreUObject runtime code (objects, classes, properties, GC, config, packages) | 160 584 |
| CoreUObject data | 413 |
| InputCore code (`FKey`, `EKeys`, `FKeyDetails`, the module) | 9 872 |
| InputCore data | 1 380 |
| Generated code (`Z_Construct_*`, `StaticStruct`, `RegisterReflection_*`) | 7 008 |
| Generated tables (`_Statics`) | 3 688 |
| Attributed total | 182 945 (179 KB) |
| `GUObjectArray` (8 192 slots × 12 bytes, heap) | 96 KB |

The rest of the +223 624 bytes of text is template code and the Core services the object system calls (`FExec`,
archives, config), which section GC dropped while nothing referenced them. The game itself does not use UObjects yet;
each class it reflects later adds its generated code, its `UClass` and its `FProperty` objects. The reflection budget
(400 KB) now also holds for ThirdPerson: about 179 KB of code and tables, the object array and the construction heap.

**P21 ShooterGame as the port's target** (Win64 Development, headless, measured by the CI of 0.20.1, since removed:
`ShooterGame -nullrhi -benchmark -botmatch -rounds=10 -seed=7`, de_leon, ten bots; the game logs `Botmatch budget:` at
the end, the same counters TestPAL logs on the PS2; the values below are 0.20.1's). The gameplay framework does not run
on the PS2 yet, so these are the numbers a PS2 ShooterGame would have to fit, measured where it runs:

| Item | ShooterGame (desktop) | PS2 limit | Notes |
|---|---:|---:|---|
| Reflected types | 132 classes, 37 structs, 16 enums, 9 functions, 752 properties | — | TestPAL on the PS2: 30 classes, 280 properties |
| Reflection construction heap | 194 KB | 400 KB (with the reflection code and tables) | 64-bit pointers: the `FProperty` and `UClass` objects shrink on the EE's 32-bit pointers, but the code and tables of Engine, AIModule, UMG and the game come on top; the reflection budget is the first to watch |
| UObjects alive | peak 2 283 in a frame, 1 628 at the end | 8 192 slots | the peak counts objects pending the next garbage collection too |
| Names | 1 572, 44 KB used | 256 KB of blocks | desktop blocks: 320 KB |
| GMalloc peak | 3 902 KB (current 3 676 KB at the end) | 31 MB of RAM for everything | includes the desktop object array (131 072 slots × 16 bytes = 2 MB; the PS2's is 96 KB); about 1.9 MB is the world, the map's assets, the actors and the reflection |

The PS2 ELF sizes are measured with the toolchain's `mips64r5900el-ps2-elf-size` in the ps2dev image when a phase is
recorded (the table above records 0.20.1, measured by the CI of that release, since removed, and GS P3, measured
with a toolchain built from source). TestPAL has not run in
PCSX2 since P16: its GMalloc and name pool numbers wait for the next run on the emulator.

**Texture VRAM** ([ps2-engine](../../../../Docs/PLANS/ps2-engine.md) E3; the cook's
`<Project>/Saved/Cooked/PS2-VramReport.txt`). The GS texture arena is the 232 pages after the two PSMCT16S frames and
the PSMZ24 buffer: 1856 KB. ShooterGame, cooked for PS2 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N4:
without the normal map nothing drew, `T_Default_Bump_N`, and the desktop shaders):

| Map | Textures | VRAM |
|---|---|---:|
| Common (the config's defaults) | `T_Default_D` 128x128 PSMT4 (8 KB), `DefaultTexture` 64x64 PSMT4 (8 KB) | 16 KB |
| `/Game/Maps/de_leon` | none of its own (flat materials) | 16 KB of 1856 KB |

With their mip chains ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N13; each texture one run of blocks, its
levels at their alignment and its CLUT, as the cache allocates it): `T_Default_D` 5 levels, 12 KB; `DefaultTexture`
4 levels, 4 KB (N7's real footprints: its level 0 is 8 blocks, not a page); 15 KB for de_leon.

Since [ps2-polish](../../../../Docs/PLANS/ps2-polish.md) P8 de_leon's sky, the cube map `T_Sky_Desert`, adds six
128 x 128 faces: five PSMT8 of 23 KB with their mips and CLUT and the ground below the horizon PSMT4 (6 colours),
12 KB; 127 KB in all. The cook's report: de_leon's own textures 190 KB, 380 KB with the common ones, of 1 856 KB.
The faces were 256 x 256 first (522 KB, fitting the arena), but the map's load then took 1 038 KB of LoadMapMisc's
1 024 KB (a fatal error on the PS2): 128 x 128 keeps its peak at 710 KB.

**Texture residency** (N13). The cache keeps textures resident by blocks and evicts the least recently used ones of
earlier frames when the arena is full (never the frame's own, never all at once); a frame uploads at most
`TextureUploadBudgetKB` (128 KB: about 20 PSMT8 64x64 textures with their mips and CLUTs, 6.3 KB each). In the
measured bot match the textures stay resident: `tex_resident_kb` peaks at 4 KB (only `DefaultTexture` draws), uploaded
at its first draw; no eviction and no CLUT load afterwards (`tex_uploads`, `tex_upload_kb`, `tex_evictions`,
`clut_loads` 0.00 a frame over 1 655 frames). `System.Renderer.GS.TextureCache.Oversubscribed` checks 3x oversubscription (12 64x64
PSMT8 textures in an arena for 4): two evictions a frame and never a reset.

The cooked content is 397 KB (58 files); its pak, aligned to 2048 bytes, 456 KB. With LPS2 v2
([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N12) they are 405 KB and 464 KB: a vertex is 18 bytes instead of
32, but a vertex two strips share is in each, and the collision triangles (the source's, in float) are saved beside
the render data.

Since N19 (the sounds in the SPU2's
ADPCM): 228 KB (58 files), the pak 292 KB (298 552 bytes).

**Cooked RAM per map** (V2; the cook's `<Project>/Saved/Cooked/PS2-RamReport.txt`, serialized sizes): the common
packages (the default assets, the weapons, the characters, the sounds) 328 KB, 33 packages; `/Engine/Maps/Entry` 3 KB
of its own; `/Game/Maps/de_leon` 47 KB of its own (16 packages: the map 27 KB, its meshes and materials), 375 KB with
the common ones (N4: 32-byte vertices without the tangent).
The sounds weighed most (16-bit PCM: `S_Explosion` 70 KB); since N19 they cook to the SPU2's ADPCM, 3.5 times
smaller: the common packages take 159 KB, de_leon 206 KB with them.

**SPU2 RAM** (N19; the cook's `<Project>/Saved/Cooked/PS2-SoundReport.txt`): ShooterGame's 11 sounds, 22 050 Hz mono
ADPCM, are all common (`/Game/Sounds` is cooked whole): 68 KB (69 232 bytes; `S_Explosion` 20 KB, `S_Reload` 13 KB,
`S_Sniper_Fire` 12 KB) of the 2028 KB, for every map. They were 242 KB of PCM16 mixed on the EE.



**ShooterGame on the EE** ([ps2-preview](../../../../Docs/PLANS/ps2-preview.md) V2; ShooterGame Development, its PS2
pak, run headless on Play!'s HLE BIOS with `leonrun`, 2026-09-26: `-nullrhi -benchmark -botmatch -rounds=2 -seed=7`).
The first numbers measured on the EE rather than on the desktop:

| Item | EE (Play!) | Desktop (Linux, same botmatch, uncooked) | PS2 limit |
|---|---:|---:|---:|
| GMalloc peak | 1 100 KB (current 1 045 KB) | 7 324 KB | the heap: 32 MB less the kernel, the ELF (1.8 MB) and the 128 KB stack |
| UObjects | peak 1 518 | peak 1 279 | 8 192 slots (`gc.MaxObjectsInGame`, the PC's too) |
| Names | 1 574, 43 KB used | 1 580, 44 KB used | 256 KB of blocks |
| Reflection construction heap | 174 KB | 200 KB | 400 KB with the code and tables |

The desktop's GMalloc is no measure of the PS2's: 64-bit pointers, the uncooked content and the desktop's modules
make it about seven times the EE's. So the PC does not emulate the PS2's heap with an arena (V2's first idea): it
shares the engine's own limits (the UObject array's `gc.MaxObjectsInGame`), and the heap is measured on the EE.
The results of the two botmatches differ (CT 0 - T 2 on the EE, CT 2 - T 0 on the desktop): the EE's floats round
differently (V3).

**Frame work** (`-LogFrameTimes`' `Frame work over N frames:` line, V2): the GS work of a frame of de_leon at the
start of a match, the same on both platforms since it is the same scene renderer: about 410 to 450 triangles (peak
491), 1 500 to 1 800 GS register writes, no texture uploads once the textures are resident (EE: 421 / 446 triangles,
1 529 / 1 776 writes; desktop: 411 / 451, 1 494 / 1 818). Play!'s milliseconds are not the EE's (its timing is not
cycle accurate): the cost of that work is PCSX2's to measure.

**Frame time** ([ps2-engine](../../../../Docs/PLANS/ps2-engine.md) E4, D6: a steady 30 fps, `SyncInterval=2`), measured
since [ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N1 by `MeasurePS2.bat`: PCSX2 without its window runs the
staged ShooterGame (Development, PS2 pak) playing a bot match of 2 rounds with seed 7, watched through a bot's eyes
(`-botmatch -rounds=2 -seed=7 -BotMatchSpectate -LogFrameTimes -ExitAfterSeconds=120`), until the game's
`FrameStats Summary:` line. The milliseconds are the emulated console's clock (the EE's timer), not the host's; PCSX2
is not the hardware, so each row names its PCSX2 and the hash of the settings it ran with
(`Engine/Platforms/PS2/Build/PCSX2/Measure.ini`). Three runs of the same build give the same numbers to the frame.

| Build | fps | avg | p50 / p95 / p99 | worst | world | scene | HUD + canvas | audio | present (vblank wait) | tris | GIF / frame | GMalloc peak | UObjects peak | PCSX2, Measure.ini |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| N1 baseline (-O2) | 25.4 | 39.4 ms | 33.5 / 50.3 / 66.8 ms | 220.8 ms | 9.0 ms | 11.8 ms | 0.2 + 4.6 ms | 2.4 ms | 11.1 ms (12.9) | 320 | 21.4 KB | 2 266 KB | 1 786 | 2.8.2, 989212e8 |
| N9 (cycle stats on) | 24.7 | 40.5 ms | 33.5 / 50.3 / 66.8 ms | 219.7 ms | 9.2 ms | 13.0 ms | 0.2 + 4.6 ms | 2.6 ms | 10.5 ms (9.4) | 362 | 24.3 KB | 2 261 KB | 1 800 | 2.8.2, 989212e8 |
| N20 | 27.5 | 36.4 ms | 33.5 / 50.3 / 50.3 ms | 211.4 ms | 3.9 ms | 15.5 ms | 0.1 + 4.5 ms | 2.3 ms | 9.6 ms (7.8) | 475 | 32.7 KB | 1 738 KB | 1 002 | 2.8.2, de7fb888 |
| N5, before N16 | 25.1 | 39.8 ms | 33.5 / 50.3 / 66.8 ms | 228.5 ms | 8.1 ms | 12.9 ms | 0.2 + 4.7 ms | 2.5 ms | 11.0 ms (10.7) | 378 | 25.6 KB | 2 055 KB | 1 776 | 2.8.2, de7fb888 |
| N16 (collision broadphase) | 24.1 | 41.5 ms | 33.5 / 50.3 / 50.5 ms | 225.8 ms | 5.9 ms | 16.6 ms | 0.2 + 4.8 ms | 3.2 ms | 10.3 ms (8.4) | 506 | 34.2 KB | 2 046 KB | 1 634 | 2.8.2, de7fb888 |
| N8 (for N12, the same ini) | 25.1 | 39.8 ms | 33.5 / 50.3 / 66.8 ms | 250.5 ms | 8.2 ms | 12.6 ms | 0.2 + 4.8 ms | 2.3 ms | 11.4 ms (12.1) | 372 | 25.1 KB | 1 973 KB | 1 416 | 2.8.2, de7fb888 |
| N12 LPS2 v2 strips | 24.6 | 40.7 ms | 33.5 / 50.3 / 51.3 ms | 211.9 ms | 8.0 ms | 14.4 ms | 0.2 + 4.9 ms | 3.1 ms | 9.5 ms (7.6) | 518 | 28.6 KB | 1 981 KB | 1 759 | 2.8.2, de7fb888 |
| N10 (vertical blank by interrupt) | 25.5 | 39.3 ms | 33.5 / 50.3 / 66.8 ms | 250.3 ms | 8.1 ms | 11.2 ms | 0.2 + 4.7 ms | 2.3 ms | 12.3 ms (11.2) | 322 | 21.3 KB | 1 974 KB | 1 378 | 2.8.2, 989212e8 |
| N11 (DMA chains, double buffered) | 25.7 | 38.9 ms | 33.5 / 50.3 / 66.8 ms | 211.7 ms | 7.9 ms | 13.0 ms | 0.2 + 4.6 ms | 2.5 ms | 10.2 ms (9.7) | 378 | 25.0 KB | 2 385 KB | 1 831 | 2.8.2, 989212e8 |
| N13 texture residency (with N16, N20) | 29.0 | 34.4 ms | 33.5 / 50.3 / 50.3 ms | 207.9 ms | 2.7 ms | 12.5 ms | 0.1 + 4.7 ms | 2.2 ms | 11.8 ms (10.7) | 432 | 23.7 KB | 1 979 KB | 993 | 2.8.2, 989212e8 |
| N19 (sounds on the SPU2) | 28.8 | 34.7 ms | 33.5 / 50.3 / 50.3 ms | 239.8 ms | 2.6 ms | 14.0 ms | 0.1 + 4.9 ms | 0.05 ms | 12.6 ms (11.4) | 408 | 26.7 KB | 1 332 KB | 1 020 | 2.8.2, 989212e8 |
| N17 (heap 1 412 KB at exit, 26.5 allocations a frame) | 29.9 | 33.5 ms | 33.5 / 33.5 / 33.5 ms | 103.0 ms | 2.6 ms | 12.0 ms | 0.04 + 4.9 ms | 0.06 ms | 13.5 ms (13.0) | 432 | 23.7 KB | 1 527 KB | 980 | 2.8.2, 989212e8 |
| N22 (baked vertex lighting, the floor a grid of 3 m cells) | 29.8 | 33.6 ms | 33.5 / 33.5 / 40.8 ms | 109.8 ms | 2.9 ms | 15.9 ms | 0.04 + 4.9 ms | 0.05 ms | 9.4 ms (8.7) | 794 | 47.6 KB | 1 797 KB | 972 | 2.8.2, 989212e8 |
| N18 (fixed 30 Hz step, timers, incremental GC; on N22 and N25) | 29.65 | 33.7 ms | 33.5 / 33.5 / 50.3 ms | 117.5 ms | 3.1 ms | 15.6 ms | 0.04 + 5.0 ms | 0.06 ms | 9.9 ms (9.2) | 762 | 45.9 KB | 1 949 KB | 934 | 2.8.2, 989212e8 |
| N27 animated characters (on N23, N30c, N30d) | 10.8 | 92.7 ms | 83.5 / 150.3 / 158.3 ms | 451.4 ms | 11.6 ms | 66.7 ms | 0.07 + 5.9 ms | 0.16 ms | 9.6 ms (7.9) | 2 240 | 176.3 KB | 4 183 KB | 1 030 | 2.8.2, 989212e8 |
| N14 (VU1, the skinned batches posed on the EE; on N27) | 15.27 | 65.5 ms | 66.8 / 133.5 / 150.3 ms | 417.8 ms | 8.4 ms | 41.9 ms | 0.07 + 5.9 ms | 0.12 ms | 10.0 ms (8.6) | 4 223 (before VU1 culls) | 50.9 KB (the EE's) | 4 801 KB | 1 030 | 2.8.2, 989212e8 |
| N14 with -novu1 (the EE's emitter, the same build) | 11.11 | 90.0 ms | 83.5 / 142.3 / 150.3 ms | 451.4 ms | 11.4 ms | 62.9 ms | 0.07 + 5.9 ms | 0.16 ms | 10.9 ms (9.1) | 2 237 | 176.1 KB | 4 213 KB | 1 030 | 2.8.2, 989212e8 |
| base N30e (7ebec933, for N15) | 11.52 | 86.84 ms | 83.50 / 150.25 / 167.00 ms | 499.83 ms | 15.97 ms | 55.53 ms | 0.08 + 7.27 ms | 0.32 ms | 9.76 ms (7.88) | 5 037 (before VU1 culls) | 84.7 KB (the EE's) | 5 194 KB | 1 172 | 2.8.2, 989212e8 |
| N15 (VU0, the scratchpad, LODs, cells, fog, blob shadows, the canvas's sprites) | 13.76 | 72.67 ms | 66.75 / 125.00 / 150.25 ms | 482.99 ms | 14.50 ms | 46.65 ms | 0.08 + 3.12 ms | 0.29 ms | 10.80 ms (9.26) | 5 088 (before VU1 culls) | 87.1 KB (the EE's) | 5 100 KB | 1 181 | 2.8.2, 989212e8 |
| N15 with -nospr (the scratchpad in main RAM, the same build) | 13.75 | 72.72 ms | 66.75 / 125.50 / 150.25 ms | 482.96 ms | 14.51 ms | 46.68 ms | 0.08 + 3.12 ms | 0.29 ms | 10.81 ms (9.27) | 5 084 (before VU1 culls) | 87.3 KB (the EE's) | 5 100 KB | 1 181 | 2.8.2, 989212e8 |
| N28 de_leon rebuilt (on N15, N30e) | 8.76 | 114.2 ms | 117.0 / 133.5 / 150.3 ms | 517.7 ms | 19.7 ms | 83.4 ms | 0.08 + 2.8 ms | 0.37 ms | 11.4 ms (8.6) | 4 800 (before VU1 culls) | 277.0 KB (the EE's) | 4 947 KB | 1 513 | 2.8.2, 989212e8 |
| N14b (VU1's Skinned programs, the RenderLists tag; on N28) | 9.78 | 102.3 ms | 100.3 / 133.5 / 133.5 ms | 517.7 ms | 17.7 ms | 73.4 ms | 0.08 + 2.8 ms | 0.35 ms | 11.0 ms (8.2) | 4 760 (before VU1 culls) | 275.0 KB (the EE's) | 4 999 KB | 1 513 | 2.8.2, 989212e8 |
| N14b with -novu1 (the EE's emitter, the same build) | 8.84 | 113.1 ms | 117.0 / 133.5 / 150.3 ms | 517.7 ms | 19.5 ms | 82.1 ms | 0.08 + 2.8 ms | 0.37 ms | 11.7 ms (8.9) | 4 148 | 334.2 KB | 5 069 KB | 1 513 | 2.8.2, 989212e8 |
| N14b on N15 (6307a50c, before N28) | 22.37 | 44.7 ms | 33.5 / 100.3 / 133.5 ms | 466.3 ms | 9.0 ms | 21.9 ms | 0.08 + 3.1 ms | 0.20 ms | 12.1 ms (10.6) | 5 773 (before VU1 culls) | 75.2 KB (the EE's) | 4 939 KB | 1 172 | 2.8.2, 989212e8 |
| N14b with -novu1 on N15 (the same build) | 11.42 | 87.6 ms | 100.3 / 133.5 / 150.3 ms | 516.5 ms | 17.5 ms | 59.4 ms | 0.08 + 3.2 ms | 0.33 ms | 10.5 ms (8.5) | 2 911 | | | 1 172 | 2.8.2, 989212e8 |
| N29 base (N14b on N28 with the scene's breakdown, 914acf3b) | 9.77 | 102.4 ms | 100.3 / 133.5 / 133.5 ms | 517.7 ms | 17.7 ms | 73.5 ms | 0.08 + 2.8 ms | 0.35 ms | 11.0 ms (8.2) | 4 761 | 275.0 KB | 4 967 KB | 1 513 | 2.8.2, 989212e8 |
| N29, each draw's point lights by its bounds | 23.68 | 42.2 ms | 33.5 / 66.8 / 100.3 ms | 467.7 ms | 7.3 ms | 24.2 ms | 0.07 + 2.8 ms | 0.18 ms | 8.8 ms (6.8) | 7 490 | 83.5 KB | 4 357 KB | 1 513 | 2.8.2, 989212e8 |
| N29, point lights on VU1 | 29.85 | 33.5 ms | 33.5 / 33.5 / 33.5 ms | 467.7 ms | 5.8 ms | 10.4 ms | 0.07 + 2.9 ms | 0.15 ms | 15.0 ms (12.9) | 8 386 (before VU1 culls) | 26.5 KB (the EE's) | 3 345 KB | 1 513 | 2.8.2, 989212e8 |
| N29 (on N24 and N30f; the floor slabs as the ground) | 29.81 | 33.6 ms | 33.5 / 33.5 / 33.5 ms | 368.3 ms | 4.9 ms | 8.4 ms | 0.07 + 2.9 ms | 0.17 ms | 17.7 ms (16.1) | 6 292 (before VU1 culls) | 19.1 KB (the EE's) | 4 261 KB | 1 804 | 2.8.2, d29e64bc |
| 0.24.0 (the release, on N24b) | 29.95 | 33.38 ms | 33.50 / 33.50 / 33.50 ms | 50.05 ms | 4.67 ms | 8.42 ms | 0.07 + 2.89 ms | 0.17 ms | 17.74 ms (16.14) | 6 299 (before VU1 culls) | 19.1 KB (the EE's) | 4 432 KB | 1 804 | 2.8.2, d29e64bc |
| ps2-polish P1 (the pawns' bodies Movable) | 29.95 | 33.38 ms | 33.50 / 33.50 / 33.50 ms | 50.05 ms | 4.74 ms | 8.53 ms | 0.07 + 2.89 ms | 0.17 ms | 17.56 ms (15.93) | 6 469 (before VU1 culls) | 19.2 KB (the EE's) | 4 432 KB | 1 804 | 2.8.2.0, d29e64bc |
| ps2-polish P5 (fonts, the textured canvas; on P2b) | 29.98 | 33.36 ms | 33.50 / 33.50 / 33.50 ms | 50.07 ms | 4.57 ms | 9.15 ms | 0.18 + 1.45 ms | 0.18 ms | 18.44 ms (16.79) | 6 723 (before VU1 culls) | 22.1 KB (the EE's) | 4 289 KB | 1 831 | 2.8.2.0, d29e64bc |
| ps2-polish P3 (the bots' knife, pickups, lookouts and ladders; on P5; 12 frames of 83 ms at 12 to 15 s, a teammate 80 cm before the watched bot: its skinned batches through the EE's emitter) | 29.26 | 34.18 ms | 33.50 / 33.50 / 83.50 ms | 88.55 ms | 4.71 ms | 9.32 ms | 0.17 + 1.41 ms | 0.19 ms | 19.06 ms (17.61) | 5 924 (before VU1 culls) | 23.4 KB (the EE's) | 5 075 KB | 1 853 | 2.8.2.0, d29e64bc |
| ps2-polish P8 (the sky: a cube map of six 128-texel PSMT8 faces, 36 VU1 batches a frame, none clipped; de_leon's fog on; on P3, the same hitches) | 29.20 | 34.25 ms | 33.50 / 33.50 / 83.50 ms | 100.10 ms | 4.72 ms | 9.79 ms (the sky 0.28) | 0.17 + 1.41 ms | 0.19 ms | 18.63 ms (17.04) | 6 299 (before VU1 culls) | 23.8 KB (the EE's) | 5 344 KB | 1 865 | 2.8.2.0, d29e64bc |
| ps2-polish P9 (the menus and UE's pause; on P3; the first frame, 1 079 ms, now holds the travel from the main menu to de_leon) | 28.80 | 34.73 ms | 33.50 / 33.50 / 83.50 ms | 1079.33 ms | 4.73 ms | 9.32 ms | 0.17 + 1.41 ms | 0.19 ms | 19.05 ms (17.60) | 5922 | 23.4 KB | 5090 KB | 4866 KB | 109.9 | 1880 | 2.8.2.0, d29e64bc |
| ps2-polish P8b base, close up (P3's build; `MeasurePS2 -CloseUp -Seconds 60`: a terrorist 80 cm before a fixed camera) | 29.99 | 33.35 ms | 33.50 / 33.50 / 33.50 ms | 50.05 ms | 3.52 ms | 3.69 ms | 0.14 + 0.87 ms | 0.01 ms | 25.84 ms (25.58) | 726 (before VU1 culls) | 8.3 KB (the EE's) | 3 412 KB | 1 849 | 2.8.2.0, d29e64bc |
| ps2-polish P8b, close up (the clipping on VU1) | 29.99 | 33.35 ms | 33.50 / 33.50 / 33.50 ms | 50.05 ms | 3.52 ms | 1.82 ms | 0.14 + 0.86 ms | 0.01 ms | 27.72 ms (27.46) | 979 (before VU1 culls) | 0.5 KB (the EE's) | 3 389 KB | 1 849 | 2.8.2.0, d29e64bc |
| ps2-polish P8b (the clipping on VU1, two point lights a draw; on P3, the same match) | 29.97 | 33.37 ms | 33.50 / 33.50 / 33.50 ms | 50.05 ms | 4.60 ms | 5.70 ms | 0.17 + 1.40 ms | 0.18 ms | 21.97 ms (20.47) | 6 303 (before VU1 culls) | 8.6 KB (the EE's) | 4 309 KB | 1 853 | 2.8.2.0, d29e64bc |
| **0.25.0** (ps2-polish P10: P6's HUD and scoreboard and P5b on P8, P9 and P8b; the first frame, 1 137 ms, holds the travel from the main menu to de_leon) | 29.62 | 33.76 ms | 33.50 / 33.50 / 33.50 ms | 1137.24 ms | 4.08 ms | 6.60 ms | 0.19 + 2.24 ms | 0.11 ms | 20.73 ms (18.77) | 7 986 (before VU1 culls) | 11.0 KB (the EE's) | 4 794 KB | 1 925 | 2.8.2.0, d29e64bc |

**de_puerto** (the second map, after 0.25.0; `MeasurePS2 -Map /Game/Maps/de_puerto`: its bot match of two rounds with
seed 7 straight into the map, `-map=`, without the menu's travel). Every frame but two falls on the second field (p50 /
p95 33.5 ms, p99 34.0 ms; the worst 103 ms is the second frame, the match's start), 29.92 fps; the scene 6.94 ms
(`GS Opaque` 1.67, `GS Skinned` 1.58, `GS Sky` 0.28), 80.8 draws and 60.8 objects a frame after the cells (112
pieces, 9 cells), 8 600 triangles before VU1's culling, 22.7 clipped batches. Its own cooked content is 559 KB (96
packages; 1 978 KB with the common ones), its textures 1 856 KB's arena with room to spare (14 of its own and the
coast sky's six faces). The memory tags' peaks: LoadMapMisc 710 KB of 1 024 (the map's load: de_leon's 710 too),
EngineMisc 1 026, UObject 758, Textures 523, Meshes 665, Animation 130, Audio 154, Physics 27, AI 12, SceneRender 181,
GameMisc 465, Temporary 16, RenderLists 1 492 KB, all within their budgets; GMalloc's peak 5 176 KB, the small-block
arena's 1 124 of 2 048 KB, no overflow. From the disc (`-Iso`, the ISO 8 464 384 bytes with both maps, the pak in path
order): the same frame (29.92 fps, p95 33.5 ms), the first frame 5.85 s after the engine starts (no recorded open
order for de_puerto; de_leon's ordered pak takes 2.93 s).

| Build | fps | avg | p50 / p95 / p99 | worst | world | scene | HUD + canvas | audio | present (vblank wait) | tris | GIF / frame | GMalloc peak | heap at exit | allocs / frame | UObjects peak | PCSX2, Measure.ini |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| de_puerto | 29.92 | 33.42 ms | 33.50 / 33.50 / 34.00 ms | 103.25 ms | 5.38 ms | 6.94 ms | 0.20 + 2.35 ms | 0.18 ms | 19.13 ms (16.92) | 8600 | 9.5 KB | 5176 KB | 4950 KB | 13.5 | 2067 | 2.8.2.0, d29e64bc |

**0.25.0** ([ps2-polish](../../../../Docs/PLANS/ps2-polish.md) P10). Every frame after the first falls on the second
field (p50 / p95 / p99 33.5 ms); the average (29.62 fps) is below 30 only by the first frame, which since P9 holds the
travel from the main menu to de_leon (1 137 ms). P6's HUD and scoreboard, measured here for the first time, take the
canvas from 1.41 to 2.24 ms (`GS Canvas` 2.06 ms); the scene is 6.60 ms (P8b's 5.70).

**Near geometry on VU1** ([ps2-polish](../../../../Docs/PLANS/ps2-polish.md) P8b). P3's 12 frames of 83 ms (at 12 to
15 s) were not the close teammate: their `Frame spike:` scopes show `GS Skinned` 42.9 ms of `GS Emitted Batches`
(145 skinned batches a frame on the EE's emitter) and `GS Opaque` 8.4 ms (25), in a firefight: the muzzle flashes
beside the tunnel's lamp gave the lit draws near them three or four point lights, and VU1's programs light two, so
those draws fell to the emitter. A draw now takes the two point lights that light its bounds most. The batches across
the near plane or the guard band (14.5 a frame in the match, 1.8 ms of `GS Clipped Batches` with the view model's
5.1 at 1.1 ms) are clipped on VU1 (`ClipTriangles.vsi`) instead of by the EE's clipper. The match: 29.26 → 29.97
fps, p99 83.5 → 33.5 ms, worst 88.55 → 50.05 ms (the load's first frames), scene 9.32 → 5.70 ms, the EE's GIF 23.4 →
8.6 KB a frame, no batch left on the EE (`batches_ee=0.0 clipped_tris=0.0`). The close-up (`-CloseUp`: a terrorist
80 cm before the camera, bots stopped) never went above 33.5 ms before either (15.5 clipped batches a frame cost the EE
1.8 ms): scene 3.69 → 1.82 ms, the EE's GIF 8.3 → 0.5 KB, p99 33.5 ms before and after.

The GMalloc column is the peak (MeasurePS2's `gmalloc_peak_kb`) from the ps2-polish P3 row on; the ps2-polish P1 and
P5 rows' 4 432 and 4 289 KB are likely the heap at the end (`heap_kb`, printed next to it), so compare those rows with
the P3 row's 4 852 KB heap, not its 5 075 KB peak.

N13 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N13): the textures resident by blocks, mipmapped, their
CLUTs loaded only when they change and the opaque draws grouped by texture. Its base has N16 and N20 too (the world
2.7 ms instead of 8.0), and the match is another one (CT 0 - T 2, 13 kills; D4 in N18): 1 518 GS writes a frame
(1 828 in N12), 23.7 KB of GIF (28.6), 432 triangles (518), the scene 12.5 ms (14.4), 29.0 µs a triangle (27.9);
3.5 GS writes and 56 GIF bytes a triangle (3.5 and 57). de_leon's materials are flat, so the frame has almost no
texture work to save: no texture uploads or CLUT loads once `DefaultTexture` is resident (0.00 a frame). The ini
hashes as N1's and N9's (989212e8) in this checkout, not as N12's; its content has not changed since N1.

N12 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N12): the static meshes are LPS2 v2 strips, sent only for
their drawn triangles. The match is not the same from one build to the next: the game steps by each frame's time (D4
fixes it in N18), so a cheaper or dearer render plays another match and the camera sees other things (N8 draws 372
triangles a frame, N12 518). A scene triangle costs 27.9 µs instead of 33.8 (scene / triangles), and the GIF bytes a
triangle, the HUD's included, fall from 69 to 57; on a fixed frame the strips take 391 GS writes where the triangles
took 946 (`System.Renderer.GS.Scene.StripsDrawTheSource`).

N28 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md), de_leon rebuilt as a textured desert town:
`MeasurePS2 -NoBuild -Label N28`, a host: run on N15's tree): 8.76 fps, p50 / p95 117.0 / 133.5 ms, against the
N15 row's 13.76 with the blockout on the same code (and 6.67 against 11.52 measured the same way on N30e's tree, before
N15: the old map's content cooked into the same build). The scene takes 83.4 ms instead of 46.7 and the EE's GIF
bytes grow from 87 to 277 KB a frame for about as many triangles before the VU1's culling (4 800 against 5 088): the
map's 78 textured, baked pieces (57 meshes, seven textures) cost the EE far more a frame than the 35 flat cubes, and
the world grows from 14.5 to 19.7 ms (123 actors, the traces against 78 boxes). The cells (N15) cut little: the walls
are low and the sky portals keep most cells in view across the map (Win64: facing a wall 1 cell of 7, down a long
all 7). The map is 2 244 triangles in 78 pieces and seven cells (the most in a cell 458; N22's blockout 1 070 in 35
cubes). Its own textures take 66 KB of the GS's arena (seven: sandstone P8, the rest P4), 256 KB with the common
ones, of 1 856 KB; its own cooked packages 309 KB (74; 1 433 KB with the common ones, a run-time estimate of 4 402 KB
of 24 576), no sounds of its own (70 KB with the common ones, of 2 027 KB); the largest mesh 146 triangles (a floor
slab; of 4 096). The pak: 1 829 771 bytes (288 files). GMalloc peaks at 4 947 KB and the UObjects at 1 513. Why a
textured piece costs the EE that much, and the 30 fps, are N29's.

N27 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md), the animated characters: `MeasurePS2 -Label N27`, a
host: run): ten skinned bodies of 828-932 triangles on 23 bones, the watched bot's first-person arms (542) and view
model (356 for the USP, up to 520 for the AWP), the world models in the bodies' hands. A frame draws 2 240 triangles on
average (N18: 762), at about 29 µs each (the C++ skinning and transform on the EE; N22's static triangles cost 20), so
the scene takes 66.7 ms and the frame 92.7 ms: 10.8 fps, p50 83.5 ms (five vertical blanks). The view model alone is
about 900 triangles in every frame. The animation's EE cost has no scope of its own: the world grows from N18's 3.1 ms
to 11.6 ms (with N30c's movement), of which the tick of the meshes (the anim instances' updates and the evaluated
poses) and the end-of-frame updates (the skin matrices to the proxies) are about 5 ms. The frame's GS containers grow
with the triangles: `EngineMisc` peaked at 2 290 KB and its budget went from 2 048 to 3 072 KB (`PS2Engine.ini`). 30 fps
is N29's (LODs, VU1 skinning in N14, fewer view model triangles).

N22 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md), the static lighting baked into the vertices): a Static
mesh draws its baked colours, so the EE lights only what moves (the pawns, the weapons). de_leon's ground became a grid
of 20 x 16 quads so the bake has vertices for the shadows: 640 triangles where the floor had 2, so a frame holds more
(794 against N17's 432, in another match: the PS2 still steps by the frame's time, N18). The scene costs 15.9 ms
against 12.0, but 20.0 µs a triangle against 27.7 (-28 %): the per-vertex Lambert of every static vertex (the sun and
the pooled point lights) is gone. A 2 m grid (30 x 24) gave 874 triangles, 16.4 ms and 26.1 fps (18.8 µs a
triangle); 3 m keeps the frame at 30 fps (p95 33.5 ms). The GMalloc peak is 1 797 KB (N17: 1 527 KB, another match);
the baked colours add 6.3 KB to the map.

What the baseline says (the frame is 33.3 ms at 30 fps): the EE's own work is about 28 ms in an average frame, so a
frame with more on screen or a busier world misses the second vertical blank and waits for the third (50 ms: the p95).
The scene (every vertex transformed, lit and clipped in C++ on the EE: N14, VU1), the world (the bots' traces without a
broadphase, the actor walks: N16, N20), the canvas (the HUD's text as triangles: N15, sprites) and the audio mix
(N19, the SPU2) are the four costs to take off the EE; the GS itself is idle most of the frame (the DMA and the GS
finish take under 1 ms of the present: the rest is the wait).

**The frame's profile** ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N9): the cycle stats
(`SCOPE_CYCLE_COUNTER`, the COP0 Count) of the same `MeasurePS2.bat` run, over its 1 541 frames (the run's
`Profile over N frames` block; `ProfileSummary:` holds the top level). Milliseconds and calls per frame; a scope's time
includes the scopes under it. The cache miss counters (PCR0 / PCR1) read 0: PCSX2 does not emulate them.

| Scope | ms | calls | Under it |
|---|---:|---:|---|
| Input | 0.15 | 1 | |
| Audio | 2.59 | 1 | Audio Mix 2.09 (the software mix, N19) |
| World Tick | 9.19 | 1 | Tick Actors 7.37: Pawn Sensing 4.31 (4.0 calls; its Line Traces 3.47 ms in 42.3 calls, 82 µs each: N16), Bot Tick 0.49 (10 calls), Character Movement 0.90 (6.4 calls; Sweeps 0.53); Character Overlaps 1.01 (2 calls, N5); Physics Step 0.51 |
| Garbage Collection | 0.01 | 1 | |
| Viewport Tick | 0.03 | 1 | |
| Viewport Draw | 13.21 | 1 | Scene 12.81 (GS Scene Render 12.57: N14, VU1), End of Frame Updates 0.20, HUD 0.17, Debug Overlay 0.02 |
| Canvas Flush | 4.55 | 1 | GS Canvas 4.15 (the HUD's text as triangles: N15) |
| Present | 10.46 | 1 | Vertical Blank Wait 9.37, GIF Packet 1.04 (built and copied: N11), GIF DMA 0.04, GS Finish 0.00 |
| **Frame** | **40.46** | | the top level adds up to 40.19 ms; the engine loop 0.03 ms; the rest (0.24 ms) is the tick between the scopes |

The EE's own work is 31.0 ms of the 40.5 ms average frame (the frame less the vertical blank wait and the DMA): the
scene 12.6 ms, the world 9.2 ms (the sensing's brute force traces alone 3.5 ms), the canvas 4.6 ms, the audio mix
2.1 ms. The stats' own cost: a recorded scope takes 133 CPU cycles (451 ns) on the EE (`System.Core.Stats.Cost` in
TestPAL, PCSX2), about 100 scopes a frame, so 45 µs: 0.11 % of the frame. The frame is 1.1 ms longer than N1's because
the scene is bigger (362 triangles against 320, 13.0 ms against 11.8), not because of the stats.

N20 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md), ShooterGame's frame): the world falls from 9.0 to 3.9 ms a
frame. The game mode's registries replace the walks over the level's actors (the bomb sites, the buy zones, the starts,
the pawns for the pickups and the match checker), the bots trace only to living enemies and at most two of them look
in a frame, a noise visits the sensing components only, a shot no longer spawns a light actor nor copies its sound,
and the HUD formats its text only when it changes (HUD 0.2 to 0.1 ms). The UObjects' peak falls from 1 786 to 1 002 and
the heap's from 2 266 to 1 738 KB (no light per shot). The rest of the frame is not N20's: the match and the bot
watched differ from N1's (the bots look at other moments, and N5 and N7 came between), so the view holds more
triangles (475 against 320) and the scene costs more (15.5 ms, N14's work). The Measure.ini is N1's; its hash differs
only by the checkout's line endings (CRLF).

N19 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md), the sounds on the SPU2) takes the audio from 2.1 to 2.6 ms
a frame (N9: the software mix of 24 voices at 48 kHz and its stream through audsrv; 3.2 ms in the N16 row) to 0.05 ms,
0.15 % of the frame: the EE no longer mixes, the SPU2's voices play the ADPCM uploaded at load, and the device's tick
only starts the frame's plays and sends the volumes that changed (each a SIF RPC; a 5-second window with a firefight
peaks at 0.13 ms). The scope is `Audio` (after the world since N19) with `Audio Voices` under it. The EE log shows
audsrv 1.04, `audsrv_adpcm_init()` and each sound `in SPU2 RAM` as it loads (8 of the 11 in this match, 48 KB), with
no error. The GMalloc peak is 1 332 KB (N16: 2 046 KB): the loaded sounds take 174 KB less as ADPCM and no mix or
stream buffers remain; the rest of the difference is the match. The rest of the row is another match (CT 1 - T 1 in 65 s, 408 triangles a frame), so its fps
and average are not comparable with the rows before; the audio's milliseconds are. The Measure.ini hash is the
checkout's (LF).

N16 (the collision broadphase) takes the world from 8.1 to 5.9 ms a frame (-27 %; 9.0 ms at N1), measured against
the same tree without it. The PS2's gameplay still steps with the frame's time (the fixed step is N18), so a faster
world plays a different match: the N16 run watches another fight (506 triangles a frame instead of 378, CT 2 - T 0 in
35 s instead of CT 0 - T 2 in 66 s), whose scene costs more, and its fps and average are not comparable with the row
before; the world's milliseconds are.

**The vertical blank by interrupt** ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N10): the `Vertical Blank
Wait` scope is now a semaphore wait (`WaitSema` on the `INTC_VBLANK_S` handler's signal) until the field
`FGSFieldPacer` picks, not a poll of the GS's `CSR`. The frames are whole fields: the percentiles are the 250 µs
buckets holding 2, 3 and 4 NTSC fields (33.37, 50.05 and 66.73 ms; PCSX2's NTSC field is 16.71 ms). The run is not
N9's frame for frame: the frame times feed the game's variable step, so the same seed plays a different match (322
triangles a frame on average against 362, 1 769 frames against 1 541), and its milliseconds are compared per part. The
EE's own work: 28.1 ms a frame (39.3 less the wait's 11.2 and the DMA's 0.04), the GIF packet 1.03 ms (unchanged: N11).

**The frame by DMA chains** ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N11): the frame is written once, as a
DMA chain, into one of two buffers through the uncached accelerated segment, the uploads' pixels by REF, and kicked
without waiting. The present's parts: `GIF Packet` (the chain's build and the REF'd pixels' write back) **0.51 ms**
against N9's 1.04 and N10's 1.03 for a bigger frame (25.0 KB against 24.3 and 21.3: 0.020 ms a KB against 0.043),
`GIF DMA` 0.00 ms in 2 calls (the kick, and the retired buffer's `dma_channel_wait`), `GS Finish` 0.00 ms and the
`Vertical Blank Wait` 9.66 ms. PCSX2 does not charge the EE's clock for the GS's drawing (GS Finish read 0.00 before
too), so the EE and GS overlap only shows on the hardware; here the gain is the packet's build. The run's match differs
from N10's again (378 triangles, 1 963 frames): the EE's own work is 29.2 ms a frame (38.9 less the wait's 9.7), with
the scene at 13.0 ms.
N17 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md), memory arenas and budgets): GMalloc is `FMallocBinned` on the
EE and the PC, the heap is charged to memory tags with budgets, and a frame's temporaries live on `FMemStack`. The same
`MeasurePS2.bat -Label N17` run, on N19 (the PS2's frame is not a fixed step yet, N18, so a change of the frame's cost
changes the match: CT 0 - T 2 with 13 kills here, CT 1 - T 1 in N19's row; 432 triangles a frame against 408, so the
milliseconds are not comparable with N19's; the heap is):

| Item | EE (PCSX2) | Limit |
|---|---:|---:|
| GMalloc peak / at exit | 1 527 / 1 412 KB (N19: 1 332 KB peak, another match) | 24 576 KB (`Total`) |
| Small-block arena | 504 KB of 2 048 KB at most, no overflow | 2 MB |
| Allocations per frame | 8.3 to 10.4 during a round; about 100 to 120 in the seconds a round starts (the spawns); 26.5 over the run (46.8 before the strip emitter's scratch went on the frame's stack) | — |
| The frame's stack (`FMemStack`) | 14 KB at most (one 16 KB chunk, taken once) | — |
| Load arena (`LoadMapMisc`) | 64 KB at most, returned when the load ends | 1 024 KB |
| Tag peaks (KB) | EngineMisc 574, UObject 396, LoadMapMisc 64, Textures 13, Meshes 44, Animation 0, Audio 52, Physics 43, AI 4, SceneRender 351, GameMisc 72, Temporary 16 | the tag budgets above |

The budgets leave room for the real art (N21 to N28) while catching a leak or a runaway: textures, meshes, sounds and
animations get most of the heap, the rest two to four times today's peak. TestPAL on the EE: 149 tests pass, its
arena peaks at 788 KB (the allocator, memory tag and frame stack tests are `System.Core.Memory.*`).

On Win64 the headless bot match (`BotMatch 10 7`, fixed 60 Hz steps) replays the same match as N20 (CT 4 - T 6, 75
kills) in the same time (1.08 s median against 1.08 s, 11 interleaved runs) with 9.2 heap allocations a frame instead
of 25.1 (the new counter, before the hot paths left the heap): the collision queries' hit arrays, the player input's lists and the per-frame input stack were most of them.
A windowed match (the renderer, the HUD) went from 53 to 18 a frame: the canvas, the impact marks and tracers, the scene
renderer's arrays and the strip emitter's scratch (N12's `FGSPrimitiveEmitter::AddStrips`: 21 a frame), the key
polling, the audio buffers. What is left are spawns, path searches' results and the log.

N18 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md), D4): the world steps at a fixed 1/30 s and the render draws
between the last two steps, so **a build that does not change the simulation plays the same match, and the rows are
comparable from N18 on**: two `MeasurePS2` runs of N18, and its builds on N17, N22 and N25 (a render change, N22's
baked floor, and the animation runtime in between), all played `CT 0 - T 2, 13 kill(s), reasons [3,3]` (before N18 a
cheaper render played another match: N12, N16, N17). The world is 3.1 ms a frame: the timers take 0.1 to 1.1 ms of it
(the bots' sight updates run from them now), the tick groups about 2 ms and the physics step 0.65 ms; 27.1 heap
allocations a frame. On N17 alone (before N22's floor) the same match ran at 29.9 fps with the scene at 11.6 ms.
The EE does not play the Win64 match of the same seed (`CT 2 - T 0, 13 kill(s), reasons [4,4]`): the EE's floats round
otherwise, so the determinism is per platform.

The garbage collector (N18): the periodic full collection (61.1 s) is gone. Every 10 s an incremental one visits 100
objects a step (a count, the same on every machine) and ends at once, checking what changed since each visit:

| Collection (EE) | Objects | Mark | Purge |
|---|---:|---:|---:|
| Full, the map loaded | 893 | 2.37 ms | 0.06 ms |
| Incremental, during a round | 904 | 10 steps of at most 100 objects, then 0.99 ms at the end | 0.13 ms |
| Full, a round's start (`ForceGarbageCollection`; 94 collected) | 1 026 | 2.55 ms | 0.56 ms |

A full mark costs 2.5 µs an object on the EE (Budgets' earlier 5 µs predates N17's allocator); the end of an
incremental collection is 40 % of a full mark (it reads every visited object's recorded references again and asks the
actors for their components again).
**The vertices on VU1** ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N14): the meshes' batches inside the
guard band go to VU1's microprograms (StaticUnlit, StaticLit) through VIF1; the EE only decides per batch (its sphere)
and writes each batch's header and REFs into the chain. A skinned batch is posed on the EE (N25's linear blend) and
handed to the same programs quantized around its sphere (`FGSVertexBatch::bQuantizedPose`), so VU1 transforms, lights,
clips and packs N27's characters too. The same build measured twice on N27, with VU1 and with `-novu1` (the EE's C++
emitter, `MeasurePS2.bat -ExtraArgs -novu1`), on N18's fixed step, so both play the same match (the same rounds and
kills): **the scene falls from 62.9 to 41.9 ms** (GS Scene Render 59.61 → 39.92 ms), 11.1 → 15.3 fps (N27's row: 10.8
fps, scene 66.7 ms), p50 83.5 → 66.8 ms (four vertical blanks instead of five). A slower frame steps the world more
times (World Tick 11.4 → 8.4 ms: the same match, fewer frames). The chain carries 50.9 KB of the EE's writes against
176.1 (VU1 builds the rest in its own memory; the static meshes' baked colours, N22, are REF'd where the instance
keeps them, a posed batch's streams where the frame's list does) and is cheaper to build (Frame Chain 1.84 → 1.42 ms).
`tris` counts the batches' triangles before VU1 culls the back faces and those outside the view (4 223 against 2 237
drawn by the emitter). The lists hold more (the draws, the batches, the posed streams, the batches' room in the
chains): `EngineMisc` peaked at 2 862 KB against 2 299 with `-novu1`, of its 3 072 KB budget. On N18 alone (no
characters, 9dabf1b) the same comparison gave the scene 14.1 → 9.2 ms and 29.56 → 29.85 fps. VU1's run's profile over
its 1 091 frames:

| Scope | ms | calls | Under it |
|---|---:|---:|---|
| Input | 0.14 | 1 | |
| Audio | 0.12 | 1 | Audio Voices 0.12 |
| World Tick | 8.35 | 1 | Tick Actors 5.13 (Pawn Sensing 0.46, Bot Tick 0.49, Character Movement 1.90), Character Overlaps 0.62, End of Frame Updates 1.20 |
| Viewport Draw | 40.76 | 1 | Scene 40.66 (GS Scene Render 39.92: the skinning of every visible batch, the batches across the near plane or the guard band on the EE's clipper, the lines, decals and tracers), HUD 0.06 |
| Canvas Flush | 5.88 | 1 | GS Canvas 5.34 (the HUD's text as triangles: N15) |
| Present | 10.00 | 1 | Vertical Blank Wait 8.57, Frame Chain 1.42, VIF1 DMA 0.00 (2 calls), GS Finish 0.00 |
| **Frame** | **65.47** | | |

What is left of the scene on the EE is the skinning itself (the two-bone blend of each posed vertex, and its
quantization for VU1: a Skinned microprogram with the bone palette in VU1's memory would take it), the batches the
clipper takes (near the camera: the floor, the walls beside it, the view model), the lit draws with a point light (the
muzzle flashes' pooled lights), the canvas. The GS's drawing is not on the EE's clock in PCSX2 (GS Finish 0.00).

**Skinning on VU1** ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N14b): a skinned batch goes to VU1 as it is
stored, with a palette of its bones' skin matrices (3 quadwords a bone, the list's memory), and `Skinned.vsm`'s
SkinnedLit / SkinnedUnlit blend each vertex's two bone matrices, pose its position and normal, then do the static
programs' work (fog and XYZF2 as N15's). The EE builds the palette and places the batch by the sphere its pose keeps
it in (the bind-pose sphere moved by each palette bone), without skinning a vertex; only the batches across a clip
plane are skinned on the EE. On N15's tree (6307a50c, the blockout map), the same build with and without `-novu1`:
**22.37 against 11.42 fps**, the scene 21.9 against 59.4 ms; against the N15 row (13.76 fps, the posed batches of
N14) the scene falls from 46.7 to 21.9 ms and p50 from 66.8 to 33.5 ms. On N28's map (a9361261) the static town
dominates: 9.78 against 8.84 fps with `-novu1`, the scene 73.4 against 82.1 ms (the N28 row: 8.76 fps, 83.4 ms); the
EE's GIF writes stay at 275 KB a frame, the map's batches that the clipper takes (N29's LODs and portals are where
that goes). **Memory**: the GS lists and the DMA chains have their own tag, `RenderLists` (the command lists' writes,
copied images, draws, batches and palettes, `ReserveChain`'s chains): 2 978 KB at most on N28 (3 046 with
`-novu1`, whose emitter writes more), budget 4 096 KB; `EngineMisc` back to 2 048 KB, peaking at 519. Before, the
posed streams of N14 kept `EngineMisc` at 2 811 KB on N30e; the palettes replace them (1 152 bytes for a 24-bone
batch, shared by the batches of one palette). VU1Conformance: 66 batches, 24 of them skinned (1 to 24 bones turned,
scaled and moved; unlit, lit, textured, mirrored and fogged), XY 0, Z 5, RGBA 1, STQ 4 ulp, F 0.

**VU0, the scratchpad and the scene** ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N15), measured on
its base (N30e, 7ebec933, the same match: CS's bots, grenades and smoke) with `MeasurePS2 -Label N15`: **11.52 → 13.76
fps**, p50 83.5 → 66.8 ms, the scene 55.5 → 46.7 ms (GS Scene Render 51.89 → 42.32 ms) and the canvas 7.27 → 3.12 ms
(GS Canvas 6.61 → 2.89 ms). The canvas is the SPRITEs: a tile or a glyph's bar is two vertices where it was six, a
third of the canvas's GS writes and of its EE time. The scene is VU0 (every `FMatrix` product, the pose's palette
products, each primitive's frustum test four planes at a time) and the view model pass's culling; de_leon has no cells
and no LODs yet (N28, N29), its fog is off, and the ten blob shadows cost 20 triangles. **The scratchpad**, measured
by the same build with `-nospr` (its 16 KB in main RAM): 46.68 against 46.65 ms, nothing PCSX2 shows, because PCSX2
does not model the EE's data cache (its profile counts no cache miss either); on a console the frame lists and the
emitter's per-batch vertices it holds would miss no cache line. The ELF's `EngineMisc` peak fell (2 951 KB against the
base's) and `allocs_per_frame` 136.3 → 114.4 (the frame lists no longer grow on the heap). Against N14's 15.27 fps
(on N27, before N30a, N30b and N30e doubled the frame's triangles to about 5 000): 13.76.

**30 fps with the real art** ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N29), on de_leon rebuilt (N28),
`MeasurePS2` (two rounds, seed 7), each step on the one before. The profile's scene has its parts since N29 (`GS
Visibility`, `GS Opaque`, `GS Skinned`, `GS Translucent and Effects`, `GS View Model`, and in them the batches the EE
draws itself, `GS Emitted Batches` and `GS Clipped Batches`, with their calls a frame) and the run's summary counts
them (`SceneWork: objects draws batches_vu1 batches_ee batches_clipped clipped_tris`, which MeasurePS2 prints and adds
to its CSV as `Scene_<key>`):

| Step | fps | p95 | scene | what the profile showed, and the change |
|---|---:|---:|---:|---|
| N14b on N28 (the base) | 9.77 | 133.5 ms | 73.5 ms | `GS Skinned` 47.4 ms: 181 batches a frame through the EE's emitter, none on VU1; `GS Opaque` 12.0 ms (30 emitted) |
| each draw's point lights by its bounds | 23.68 | 66.8 ms | 24.2 ms | the map's three baked point lights (the tunnel's lamps, the mid doors') were in every lit draw, so no microprogram took one (VU1 had the ambient and one sun): a draw now takes only the point lights whose range reaches its bounds (the others light none of its vertices: the same colours). 46 skinned batches a frame were still near a lamp |
| point lights on VU1 | 29.85 | 33.5 ms | 10.4 ms | StaticLit and SkinnedLit add up to two point lights (N.L and the range attenuation squared, the header's LocalToWorld and positions): no batch left on the emitter; the EE's GIF 83.5 → 26.5 KB |
| the floor slabs as the ground, N24 and N30f | 29.81 | 33.5 ms | 8.4 ms | the same frame (the match is another: the seams no longer push, N30f's and N24's bases) |

What is left on the EE (the last run, 3 576 frames): the world 4.9 ms (one fixed step a frame), the scene 8.4 ms (the
13 batches a frame across a clip plane on the clipper, 2.4 ms with the view model's; the palettes and placement of the
skinned batches; the blob shadows, marks and sprites 1.3 ms), the canvas 2.9 ms, the frame's chain 1.6 ms: about
17.5 ms of the 33.3, every frame on the second field (p50, p95 and p99 at 33.5 ms). Not needed for the gate, and
measured or reasoned rather than done:

- Merging de_leon's 78 pieces per cell and material (fewer draws: 69 a frame) would save part of `GS Opaque`'s
  1.3 ms outside the clipper, and needs a mesh with several `UCX_` boxes (Leon folds them into one AABB body; a
  compound body is `FKAggregateGeom`'s boxes as bodies of their own). Left for when the scene needs it.
- The clipped batches (13 a frame, 2.4 ms): the floor and walls beside the camera and the view model, across the near
  plane. Smaller batches or clipping on VU1 would take them; at 30 fps with 16 ms of the frame waiting for the
  vertical blank, not now.
- Cells: `GS Visibility` costs 0.4 ms and the open map's sky portals keep most cells visible (52.7 objects drawn of
  78 pieces and the pawns); the culling stays weak, as N28 found, and the frame does not need it.
- Fog and LODs: de_leon is 60 × 48 m, all inside the far plane; a nearer far plane under fog culls nothing the portals
  do not, and a piece has at most 146 triangles (their LODs would draw the same batches). Both stay off.
- Memory: the `RenderLists` tag peaks at 1 351 KB (2 978 in N14b's run with the characters on the emitter); its budget
  stays 4 096 KB for `-novu1`, whose emitter writes more (3 046 KB). GMalloc peaks at 4 261 KB; the cook's estimate
  (N23: `RuntimeBaseKB` + 2 x the cooked bytes) was 4 402 KB against N28's 4 947 measured, and is calibrated to
  `RuntimeBaseKB=3072` (5 938 KB for de_leon's 1 433 cooked KB).

## The disc (N23)

[ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md) N23: `BuildCookRun -platform=PS2 -build -cook -stage -pak -iso`
makes `Game/ShooterGame/Saved/StagedBuilds/PS2/ShooterGame.iso`, 5 056 512 bytes (2 469 sectors, with the measuring
command line): `SYSTEM.CNF` at LBA 33, the ELF (`SLUS_990.01`, 4 325 228 bytes) from 34, the pak
(`SHOOTERG/CONTENT/PAKS/SHOOTERG.LPA`, 330 432 bytes, its entries on 2 048-byte sectors) from 2 146, then `AUDSRV.IRX`
and `LEONCOMM.TXT`. The same stage gives the same bytes. PCSX2 2.8.2 boots it (`-fastboot -- <iso>`, the `SLUS-99001`
serial is in no GameDB entry): the BIOS reads the ELF from `cdrom0:\SLUS_990.01;1`, the game mounts
`cdrom0:/ShooterGame/Content/Paks/ShooterGame-PS2.lpak` (58 files) and plays the bot match at 30 fps with N22's baked
lighting.

The cooked content: 232 480 bytes against 245 409 before N23 when it was first measured, on N17's content (the staged
ini files without comments or editor sections, 12.1 KB; `SM_CrateStack` without its unused collision triangles,
0.9 KB); 264 422 bytes on N22's to N30d's (the baked colours).

Load time: the engine's start to its first frame, on the EE's clock (`LogLaunch: First frame after ...`,
`MeasurePS2 -Iso`). From the disc the reads take the emulated drive's time; the pak in its open order (`-pakorder=` with
the `LogFileOpenOrder:` lines of a disc run, 47 files) reads forward. Since N18 the first frame follows the map at once
(a fixed step):

| Pak | First frame | Map ready | `LoadMap(de_leon)` | Host log: ELF running → first frame |
|---|---:|---:|---:|---:|
| `host:` (N20's stage, for reference) | | | 0.08 s | |
| Disc, entries in path order | 1.42 s | 1.41 s | 0.74 s | 1.6 s |
| Disc, entries in the open order | 0.90 s | 0.89 s | 0.41 s | 1.2 s |

The two disc runs' frames (`MeasurePS2 -Iso -Seconds 60`, on N30d; the same match, N18):

| Build | fps | avg | p50 / p95 / p99 | worst | world | scene | HUD + canvas | audio | present (vblank wait) | tris | GIF / frame | GMalloc peak | UObjects peak | PCSX2, Measure.ini |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| N23 disc | 29.1 | 34.4 ms | 33.5 / 33.5 / 50.3 ms | 667.9 ms | 3.5 ms | 16.2 ms | 0.07 + 6.0 ms | 0.05 ms | 8.5 ms (7.7) | 819 | 49.9 KB | 2 009 KB | 907 | 2.8.2, 989212e8 |
| N23 disc, pak in open order | 29.2 | 34.2 ms | 33.5 / 33.5 / 50.3 ms | 384.3 ms | 3.3 ms | 16.2 ms | 0.07 + 6.0 ms | 0.05 ms | 8.6 ms (7.8) | 816 | 49.7 KB | 2 021 KB | 907 | 2.8.2, 989212e8 |

The worst frame is the first ones' (the textures and sounds uploaded as the round starts, a sound's first read from the
disc): 668 ms and 384 ms, where `host:` serves the files at once. Loading every sound with the map (N24's asynchronous
IO) is where that goes.

N24 ([ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md), the asynchronous IO): the IO thread reads the disc while
the game runs, and the map's load ends with what the game spawns later (ShooterGame's game mode asks for the soft
paths of its weapons, projectiles, pawn, bomb and player controller, inside structs and arrays too: 101 packages with
the sounds, the surfaces' steps and impacts, the radio, the animations and the skeletal meshes, by `LoadPackageAsync`
in `InitGame`; `UEngine::LoadMap` flushes after `BeginPlay`), so no frame opens a file after the first (the
`-LogFileOpenOrder` run's log: 126 opens after the first frame before N24, none after). Both builds on N30f's content
(de_leon rebuilt, VU1 skinning; every frame of the EE is still about 117 ms of render, so the target of no frame above
50 ms after the first is the render's to meet), each pak in its own open order (`MeasurePS2 -Iso -LogFileOpenOrder`,
then `-PakOrder <that log>`), `-Seconds 60`, the same match; the worst frame after the first (`worst_later_ms`, new in
`FrameStats Summary:`) and the long frames' log lines (`Long frame N`):

| Build | Pak | First frame | `LoadMap(de_leon)` | Worst frame after the first | Which |
|---|---|---:|---:|---:|---|
| N30f (before N24) | path order | 3.34 s | 2.54 s | 5 622.8 ms | the match's start: the pawns', weapons' and sounds' first reads (126 files) |
| N30f (before N24) | open order | 2.39 s | 1.79 s | 3 053.6 ms | the same frame |
| N24 | path order | 10.77 s | 9.91 s | 435.0 ms | no file read; the match's start is frame 4 (ten pawns spawn with their weapons, a collection of 4.9 ms) |
| N24 | open order | 7.79 s | 7.13 s | 435.0 ms | the same |

The load takes what the first frames took, and more (every weapon's and every surface's assets, not only what the
first round uses): 5.4 s more before the first frame for the 3.1 s gone from the match's start (the pak in open
order). At most 384 KB of package bytes are read ahead (`MaxBytesInFlight`, the imports first), so `LoadMapMisc`
peaks at 448 KB of its 1 024 (without the cap the path-order run went over it: the preload's bytes waited there for
their imports). The frames (`MeasurePS2 -Iso -PakOrder`):

| Build | fps | avg | p50 / p95 / p99 | worst | world | scene | HUD + canvas | audio | present (vblank wait) | tris | GIF / frame | GMalloc peak | heap at exit | allocs / frame | UObjects peak | PCSX2, Measure.ini |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| N30f baseline disc, pak in open order | 8.58 | 116.52 ms | 117.00 / 133.50 / 141.75 ms | 3053.60 ms | 25.36 ms | 79.01 ms | 0.08 + 2.94 ms | 0.49 ms | 12.35 ms (9.32) | 5237 | 294.4 KB | 5187 KB | 4790 KB | 299.9 | 1583 | 2.8.2.0, 989212e8 |
| N24 disc, pak in open order | 8.96 | 111.64 ms | 117.00 / 133.50 / 133.75 ms | 434.98 ms | 20.26 ms | 79.19 ms | 0.08 + 2.96 ms | 0.50 ms | 12.23 ms (9.19) | 5245 | 295.3 KB | 5675 KB | 5280 KB | 222.1 | 1807 | 2.8.2.0, d29e64bc |

Measure.ini's hash is new (d29e64bc): the measuring data folder has its own memory card (`Mcd001.ps2`, which PCSX2
makes the first time), so the game's settings never touch the user's cards. The world's 5.1 ms less and 78 fewer
allocations a frame are the reads and spawns' loads gone from the frames; the heap holds the preloaded assets (+490 KB
at exit, 224 more objects).

N24b (the disc's reads made fewer and larger, and the round's start spread; [ps2-shipping](../../../../Docs/PLANS/ps2-shipping.md)):
N24's load read the preload's packages a few KB at a time, and a read of the emulated disc through FILEIO costs about
20 ms before its bytes and 0.7 ms a KB (the flush's log: 187 reads of 943 KB in 4.6 s). Now the IO thread takes the
queued read nearest ahead of the last one and coalesces the close ones (`FAsyncIOSystem::CoalesceBytes`), the game
thread's pak handle reads 64 KB blocks forward, a handle skips the `lseek` it does not need, the map itself loads
through the same queue, and a synchronous load of a package on its way raises its read to the front. The round's start
spent 320 ms in the spawns' asset lookups (`DoesPackageExist` for every config path of every pawn and weapon);
ShooterGame's `LoadShooterObject` resolves what is in memory by a lookup and keeps what it resolved. Both builds on
N29's content (30 fps), the whole preload with the map, each pak in its own open order (`MeasurePS2 -Iso
-LogFileOpenOrder`, then `-PakOrder <that log>`), `-Seconds 60`:

| Build | Pak | First frame | `LoadMap(de_leon)` | Worst frame after the first | Opens after the first frame |
|---|---|---:|---:|---:|---:|
| N29 (N24's loads) | path order | 10.79 s | 9.91 s | 384.95 ms (frame 4, the round's start) | 0 |
| N29 (N24's loads) | open order | 7.72 s | 7.06 s | 384.95 ms | 0 |
| N24b | path order | 10.08 s | 9.11 s | 50.05 ms (three fields) | 0 |
| N24b | open order | 2.93 s | 2.38 s | 50.05 ms | 0 |
| 0.24.0 | path order | 10.08 s | | 50.05 ms | 0 |
| 0.24.0 | open order | 2.93 s | | 50.05 ms | 0 |

With the pak in its open order the IO thread reads 1 754 KB in 49 reads (1.9 s) and the game thread 95 KB in 3 before
the first frame (`The paks until the first frame` lines): the pak is read about once, and the load is the disc's
speed. Before N24 (N30f's content, the synchronous loads) the first frame came at 2.39 s, with the frame the match
starts on reading 126 files (3 053.6 ms); the 0.5 s more are every weapon's assets, read before the first frame. In path
order the reads jump across the pak: the open order is what a shipped disc uses. The frames (`-PakOrder`):

| Build | fps | avg | p50 / p95 / p99 | worst | world | scene | HUD + canvas | audio | present (vblank wait) | tris | GIF / frame | GMalloc peak | heap at exit | allocs / frame | UObjects peak | PCSX2, Measure.ini |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| N29 disc, pak in open order | 29.79 | 33.57 ms | 33.50 / 33.50 / 33.50 ms | 384.95 ms | 5.44 ms | 10.36 ms | 0.08 + 3.01 ms | 0.20 ms | 15.16 ms (13.07) | 8505 | 25.8 KB | 4033 KB | 3988 KB | 67.1 | 1802 | 2.8.2.0, d29e64bc |
| N24b disc, pak in open order | 29.96 | 33.38 ms | 33.50 / 33.50 / 34.25 ms | 50.05 ms | 5.25 ms | 10.33 ms | 0.08 + 2.98 ms | 0.20 ms | 15.21 ms (13.12) | 8484 | 25.7 KB | 4256 KB | 4212 KB | 11.8 | 1802 | 2.8.2.0, d29e64bc |
| 0.24.0 disc, pak in open order | 29.96 | 33.38 ms | 33.50 / 33.50 / 34.25 ms | 50.05 ms | 5.25 ms | 10.33 ms | 0.08 + 2.98 ms | 0.20 ms | 15.21 ms (13.12) | 8484 | 25.7 KB | 4256 KB | 4212 KB | 11.8 | 1802 | 2.8.2.0, d29e64bc |

The allocations a frame fall from 67.1 to 11.8 (the spawns' path strings); GMalloc's peak grows 223 KB (the coalescing
buffer, 128 KB, and the resolved assets' table). `LoadMapMisc` peaks at 710 KB of its 1 024 (`MaxBytesInFlight` 640 KB).
