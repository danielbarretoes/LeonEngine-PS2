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

## How to measure

- **ELF sections**: `mips64r5900el-ps2-elf-size <elf>` inside the ps2dev image, for example
  `docker run --rm -v <repo>:/leon <image> mips64r5900el-ps2-elf-size /leon/Game/ThirdPerson/Binaries/PS2/ThirdPerson.elf`
  (`<image>` is the digest in `Engine/Platforms/PS2/Source/Programs/LeonBuildTool/LeonBuildPS2.cmake`). The stripped
  size comes from `mips64r5900el-ps2-elf-strip -o /tmp/x.elf <elf>`.
- **Heap / names**: run TestPAL (`Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build`) and read
  the `LogTestPAL` lines in the PCSX2 log (`%USERPROFILE%\Documents\PCSX2\logs\emulog.txt`): GMalloc peak / current,
  process size (program image + heap high-water, `sbrk`) and the name pool. Since P9 also the reflected type counts
  with the GMalloc bytes allocated while `UObjectBaseInit` and `ProcessNewlyLoadedUObjects` constructed them
  (`GetUObjectReflectionStats`; the object array is not included), and the object array's capacity and live objects.
- **Reflection code in the ELF**: `mips64r5900el-ps2-elf-nm -S -C --size-sort` over `TestPAL.elf`, summing the
  CoreUObject symbols, the generated `Z_Construct_*` / `exec*` / `StaticClass` / `RegisterReflection_*` code and the
  `_Statics` tables.
- **In game**: the stats overlay's `RAM` line (Select cycles it) shows the same process size.

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
