# PS2 budgets

The EE has 32 MB of main RAM (the kernel keeps the first 1 MB) and the GS has 4 MB of VRAM. Every phase that changes
Core or a PS2 module records its numbers here, so growth is caught when it happens rather than when the port runs
out of memory.

## Limits

| Item | Limit | Enforced by |
|---|---|---|
| FName pool | 16 KB blocks, at most 16 (256 KB), 4 096 hash buckets (16 KB) | `FPS2PlatformProperties::NamePool*`; exhausting the pool is a fatal error |
| Reflection data (CoreUObject) | 400 KB | measured from CoreUObject on (P9) |
| UObject array | 8 192 objects (about 96 KB) | CoreUObject (P9) |

## How to measure

- **ELF sections**: `mips64r5900el-ps2-elf-size <elf>` inside the ps2dev image, for example
  `docker run --rm -v <repo>:/leon <image> mips64r5900el-ps2-elf-size /leon/Game/ThirdPerson/Binaries/PS2/ThirdPerson.elf`
  (`<image>` is the digest in `Engine/Platforms/PS2/Source/Programs/LeonBuildTool/LeonBuildPS2.cmake`). The stripped
  size comes from `mips64r5900el-ps2-elf-strip -o /tmp/x.elf <elf>`.
- **Heap / names**: run TestPAL (`Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build`) and read
  the `LogTestPAL` lines in the PCSX2 log (`%USERPROFILE%\Documents\PCSX2\logs\emulog.txt`): GMalloc peak / current,
  process size (program image + heap high-water, `sbrk`) and the name pool.
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

| Version | Program | GMalloc peak | Process | Name pool | Notes |
|---|---|---:|---:|---:|---|
| P2 | TestPAL (27 tests) | 68 KB | 600 KB | 85 names, 32 KB allocated (1 block + hash) | |
| P2 | ThirdPerson | | 0.5 MB | | overlay `RAM 0.5/32.0 MB`, 60 FPS, Draw3D `boxes=343 tris=278 emit=254` |
