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
  Since P10 also the `LogGarbage: Display: GC budget:` line (the cost of marking and destroying 2 000 objects) and a
  final collection after the tests; since P11 the `Package budget:` line (a save / load round trip of 50 objects).
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
| P16 | TestPAL (112 tests) | 1 157 KB | 2 328 KB | 450 names, 10 KB used of 32 KB allocated | `TestPAL: PASSED (112 test(s), 0 failed)`: the SHA-1 test and the 5 PakFile tests join; `Reflection: 30 classes, 21 structs, 6 enums, 14 functions, 280 properties, 2 packages; construction heap 41 KB` (the editor-only test class); 107 objects after a final collection (0.340 ms). The peak grows from 849 KB with the pak tests' buffers (a 70 000-byte entry, several copies of the test pak) |
| P16 | ThirdPerson | | 0.9 MB | | 60 FPS, same Draw3D numbers (`boxes=343 culled=291 backfaces=173 tris=278 keep=254 drop=24 clip=0 emit=254` every 30 frames) |
| P13 | ThirdPerson | | 0.9 MB | | the object system starts (`UObject array: 8192 objects, 98304 bytes`, `Object system started: 8192 object slots, transient package /Engine/Transient`); 60 FPS, same Draw3D numbers (`boxes=343 culled=291 backfaces=173 tris=278 keep=254 drop=24 clip=0 emit=254` every 30 frames). The overlay rounds to 0.1 MB: with the 692 KB image, the heap high-water is about 180 to 280 KB (P11: about 90 to 190 KB), 96 KB of it the object array |

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

**P21 ShooterGame as the port's target** (Linux x86-64 Development, headless: `ShooterGame -nullrhi -benchmark
-botmatch -rounds=10 -seed=7`, de_leon, ten bots; the game logs `Botmatch budget:` at the end, the same counters
TestPAL logs on the PS2). The gameplay framework does not run on the PS2 yet, so these are the numbers a PS2
ShooterGame would have to fit, measured where it runs:

| Item | ShooterGame (desktop) | PS2 limit | Notes |
|---|---:|---:|---|
| Reflected types | 129 classes, 37 structs, 17 enums, 8 functions, 751 properties | — | TestPAL on the PS2: 30 classes, 280 properties |
| Reflection construction heap | 199 KB | 400 KB (with the reflection code and tables) | 64-bit pointers: the `FProperty` and `UClass` objects shrink on the EE's 32-bit pointers, but the code and tables of Engine, AIModule, UMG and the game come on top; the reflection budget is the first to watch |
| UObjects alive | peak 2 188 in a frame, 1 377 at the end | 8 192 slots | the peak counts objects pending the next garbage collection too |
| Names | 1 584, 44 KB used | 256 KB of blocks | desktop blocks: 320 KB |
| GMalloc peak | 3 933 KB (current 3 621 KB at the end) | 31 MB of RAM for everything | includes the desktop object array (131 072 slots × 16 bytes = 2 MB; the PS2's is 96 KB); about 1.9 MB is the world, the map's assets, the actors and the reflection |
| Process peak (max RSS) | 10.2 MB | — | the program image and the C++ runtime included |

The PS2 ELF sizes and the TestPAL run in PCSX2 are not measured since P16: the phases after it were done where the
ps2dev image could not be pulled. The PS2 modules changed since then are CoreUObject (P18: a native class's defaults)
and Core and Launch (P21: `FApp::IsBenchmarking`, the requested exit code of `FPlatformMisc::RequestExitWithStatus`);
the next measurement records them. CI still builds both PS2 ELFs on every push.
