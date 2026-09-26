# Leon Engine — setup

How to get a working Windows machine that builds the engine (Win64), runs the automation tests, and builds and runs
the PS2 game in PCSX2. The build system itself is documented in [BUILD.md](BUILD.md).

## Requirements

| Need | Required for | Notes |
| --- | --- | --- |
| Visual Studio 2026 (folder `18`) or 2022, any edition | Win64 builds | Workloads/components: **Desktop development with C++**, **C++ CMake tools for Windows**, **C++ Clang tools for Windows** (LLVM: `clang-format`, `clangd`; the repository is formatted with clang-format 20, see [Formatting and lint](#formatting-and-lint)). The scripts look in `%ProgramFiles%\Microsoft Visual Studio\18\` then `...\2022\`, editions Community, Professional, Enterprise |
| CMake 3.24 or later on `PATH` | everything | `Setup.bat` calls `cmake` directly. The build scripts also prepend `C:\Program Files\CMake\bin` |
| [Ninja](https://ninja-build.org/) on `PATH` | Win64 builds | `Build.bat` fails with `Ninja not on PATH` otherwise: `winget install Ninja-build.Ninja`, then open a new terminal |
| GPU / driver with OpenGL 3.3 | running `LeonGame` | |
| [Docker Desktop](https://www.docker.com/products/docker-desktop/) | PS2 builds | Builds run in the pinned `ghcr.io/ps2dev/ps2dev` image; nothing else to install. Alternative: a local [ps2dev](https://github.com/ps2dev/ps2dev) with `PS2DEV` / `PS2SDK` exported |
| [PCSX2](https://pcsx2.net/) (Qt build) + a PS2 BIOS dump | running the PS2 build | `winget install PCSX2Team.PCSX2` |
| Git | cloning | |

## First-time setup

From the repo root:

```bat
Setup.bat
```

`Setup.bat` downloads the pinned third-party archives (GLFW, miniaudio, tinyobjloader, Jolt) into
`Engine/Intermediate/ThirdPartyDownloads/`, checks their SHA-256 and extracts them next to their module rules. Run it
again after pulling a change that bumps a library. List of libraries: [LIBRARIES.md](LIBRARIES.md).

## Build the engine (Win64)

`Engine\Build\BatchFiles\Build.bat` loads the Visual Studio environment itself, so any terminal works:

```bat
Engine\Build\BatchFiles\Build.bat LeonGame Win64 Development
Engine\Build\BatchFiles\Build.bat LeonCook Win64 Development
Engine\Build\BatchFiles\Build.bat BlankProgram Win64 Development
```

Outputs go to `Engine\Binaries\Win64\` (`LeonGame.exe`, `LeonCook.exe`, `BlankProgram.exe`); build trees to
`Engine\Intermediate\Build\Win64\<Configuration>\`. Other configurations are `Debug` and `Shipping`; their executables
are named `<Target>-Win64-<Configuration>.exe`. `Clean.bat` and `Rebuild.bat` take the same arguments; add
`-KeepGoing` to see every compile error in one run.

## Run the tests

```bat
Engine\Build\BatchFiles\RunTests.bat
```

This builds `LeonAutomationTests` (Win64 Development) and runs it from the repo root. The executable contains the
tests of every module in its closure (`<Module>/Private/Tests/`), all UE automation tests (386, named
`System.<Module>.<Area>.<Name>`). Then it runs the LeonHeaderTool golden tests, builds and runs ShooterGame's test
program (`ShooterGameTests`, 42 tests) and last builds and runs `TestPAL` (120 tests on Win64). The exit code is
non-zero if any test fails. `-automation=<filter>` runs only the tests whose name contains `<filter>` (the engine's and
ShooterGame's):

```bat
Engine\Build\BatchFiles\RunTests.bat -automation=System.Core.Containers
Engine\Build\BatchFiles\RunTests.bat -automation=System.JoltPhysics
```

`TestPAL` runs the Core, CoreUObject, Json, Projects and PakFile automation tests on Win64 and PS2, and ends with
`TestPAL: PASSED (N test(s), 0 failed)` plus memory and name-pool numbers; to run it alone:

```bat
Engine\Build\BatchFiles\Build.bat TestPAL Win64 Development
Engine\Binaries\Win64\TestPAL.exe [-filter=<text>]
```

On PS2 see [Run TestPAL in PCSX2](#run-testpal-in-pcsx2).

## LeonGame

`LeonGame` is the engine's game executable (UE: `UE4Game`). It starts as a UE 4.27 game does: `FEngineLoop` creates
`GEngine` (`[/Script/Engine.Engine] GameEngine=`, a `UGameEngine`) and its game instance opens the startup map
(`UEngine::Browse` → `LoadMap`):

```bat
Engine\Binaries\Win64\LeonGame.exe [<map>[?game=<class>]] [-map=<map>] [-nullrhi] [-tick=<Hz>] [-showstats]
                                   [-AxesGizmo] [-ExecCmds="<command>;<command>"]
                                   [-Screenshot=<file.bmp>] [-ExitAfterFrames=N]
```

The map is the first argument (UE's form) or `-map=` (Leon's alias, which the scripts use); without one it is
`[/Script/EngineSettings.GameMapsSettings] GameDefaultMap`, `/Engine/Maps/Template_Default`. A map is the long package
name of a `.lmap` package (`/Engine/Maps/Entry` is `Engine/Content/Maps/Entry.lmap`) or a `.lmap` path (absolute or
relative to the working directory; one outside the mount points mounts the folder above its `Maps/` folder, as
[LEVELS.md](LEVELS.md#running-a-map) explains). URL options follow the map: `?game=/Script/Engine.GameMode` picks the game
mode, which otherwise comes from the level (its world settings), then `GlobalDefaultGameMode` (`AGameModeBase`, whose
default pawn is `ADefaultPawn`). A map that cannot be opened logs `Failed to enter <map>` and exits with code 1.

`-nullrhi` runs headless (no window, silent audio) at `-tick=` Hz (default 60), paced to the clock unless `-benchmark`
(the steps then run as fast as they can); `-showstats` shows the HUD stats;
`-AxesGizmo` starts with the axes gizmo on; `-ExecCmds=` runs console commands (separated by `;` or `,`) on the first
frame, for example `-ExecCmds="stat unit;FOV 75"`; `-Screenshot=` saves frame `-ExitAfterFrames=` (default 60) as a
24-bit BMP and exits, and `-ExitAfterFrames=N` alone exits after frame N (headless too). In PowerShell quote an
argument that has a dot after `=` (`"-map=D:\Work\Maps\Arena.lmap"`), or PowerShell splits it at the dot.

In the window (`Engine/Config/BaseInput.ini`): mouse look (the cursor is captured), **WASD** or the arrows fly along the
view, **E** / **Q** up / down; the function keys run console commands: **F1** `show Bounds` (mesh AABBs and the shadow
volume), **F2** `show Collision` (the characters' capsules and the physics bodies), **F3** `show Navigation` (the
waypoint graph), **F4** `stat unit` (stats), **F5** `RecompileShaders all`, **F6** `show AxesGizmo` (X red, Y green, Z
blue). The world is UE's: X forward, Y right, Z up, centimetres. Manual checks: [TESTING.md](TESTING.md). Levels:
[LEVELS.md](LEVELS.md).

Every run writes a log file, `Engine/Programs/LeonGame/Saved/Logs/LeonGame.log` (a project target writes to
`<Project>/Saved/Logs/`), keeping the previous run as `-backup-<date>.log`. Config comes from `Engine/Config/Base*.ini` and the project's `Config/Default*.ini`;
a single key can be overridden from the command line with `-ini:Engine:[Section]:Key=Value`, and log verbosity with
`-LogCmds="LogInit Verbose"`.

## Import and cook (LeonCook)

```bat
Engine\Build\BatchFiles\Cook.bat -run=ImportAssets -source=SourceArt\Crate.fbx -dest=/Game/Props
Engine\Build\BatchFiles\Cook.bat -run=ImportAssets -importlist=Engine/SourceArt/ImportList.ini
Engine\Build\BatchFiles\Cook.bat -run=ImportAssets -reimport -all
```

`Cook.bat` builds `LeonCook` and passes the arguments through: `LeonCook [<Project>.lproj] -run=<Commandlet>` runs one
of the editor module's commandlets (`ImportAssets`, maps included, `ResavePackages`, `ValidateAssets`, `Cook`;
`Cook.bat -help` lists them). Formats and the import pipeline: [ASSET_FORMATS.md](ASSET_FORMATS.md). Tool
reference: [TOOLS.md](TOOLS.md).

## PS2

The PS2 game is `Game/ThirdPerson`, an isolated project built against the engine with `-Project=`. With Docker
Desktop running:

```bat
Engine\Build\BatchFiles\Build.bat ThirdPerson PS2 Development -Project=%CD%\Game\ThirdPerson\ThirdPerson.lproj
```

→ `Game\ThirdPerson\Binaries\PS2\ThirdPerson.elf`. The first build pulls the ps2dev image. PS2 builds do not need Visual
Studio. When `PS2DEV` is not set, LeonBuildTool runs itself inside the container; with a local ps2dev install (`PS2DEV`,
`PS2SDK`, and `$PS2DEV/ee/bin` on `PATH`) it builds on the host. From Git Bash or WSL use
`Engine/Build/BatchFiles/Linux/Build.sh` with the same arguments (inside the ps2dev container it builds directly).
Details: [BUILD.md — PS2 builds in Docker](BUILD.md#ps2-builds-in-docker) and the platform extension
[Engine/Platforms/PS2/README.md](../Engine/Platforms/PS2/README.md).

The engine-only `BlankProgram` also builds for PS2 (`Build.bat BlankProgram PS2 Development` →
`Engine\Binaries\PS2\BlankProgram.elf`).

### Run in PCSX2

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Project Game\ThirdPerson          # run the built ELF
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Project Game\ThirdPerson -Build   # build first
```

`-Project` accepts a project folder or a `.lproj` file (default `Game\ThirdPerson`); `-Program <Name>` runs an
engine program instead (`Engine\Binaries\PS2\<Name>.elf`, built with `Build.bat <Name> PS2 <Configuration>`);
`-Configuration` is `Debug`, `Development` (default) or `Shipping`. The script finds PCSX2 through
`$env:LEON_PCSX2`, then `pcsx2-qt.exe` on `PATH`, then the default install folders, and starts it with
`-fastboot -elf <file>`. You can also use PCSX2's **File → Run ELF** directly.

### Run TestPAL in PCSX2

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build
```

TestPAL runs the Core, CoreUObject, Json, Projects and PakFile automation tests on the EE (113 on PS2: Core 44,
CoreUObject 61, Json 2, Projects 1, PakFile 5; the platform-file, config-cache, log-file, real-descriptor, file-package
and SaveConfig tests are desktop-only) and logs to the EE console. With the EE console enabled (see
[PCSX2 notes](#pcsx2-notes)), read `%USERPROFILE%\Documents\PCSX2\logs\emulog.txt` for the
`TestPAL: PASSED (113 test(s), 0 failed)` line and the
`LogTestPAL` reflection / object-array / memory / name-pool lines; their numbers are tracked in
[Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md).

ThirdPerson controls:

| Input | Action |
| --- | --- |
| Left stick | move (camera-relative) |
| Right stick | orbit the camera |
| Cross | jump |
| Start | quit |
| Select | cycle the engine debug overlay: both panels → stats → gamepad → none |

The overlay (FPS, RAM, VRAM, resolution, plus the DualShock widget) is described in the
[PS2 platform README](../Engine/Platforms/PS2/README.md#debug-overlay).

### PCSX2 notes

- **Controller**: bind your pad in the global **Controller Port 1** settings. Bindings made only inside an input
  profile do not reach games unless that profile is the one in use. An Xbox controller shows up as an SDL device
  (`SDL-0`).
- **EE console log**: `UE_LOG` output (stdout) and the remaining `printf` diagnostics go to the EE console (for
  example the `LogThirdPerson` banner, `FPS2RHI::InitDisplay: 640x448 GS + z-buffer ready`, `PS2InputInterface: ...`,
  `FStatsOverlay` visibility changes). Enable the EE console in PCSX2's
  logging settings (`EnableEEConsole = true` under `[Logging]` in `PCSX2.ini`) and read the PCSX2 log window or
  `logs/emulog.txt` in the PCSX2 user folder. Edit `PCSX2.ini` only while PCSX2 is closed; it rewrites the file on exit.
- PCSX2 ignores synthetic keyboard input, so button handling has to be tested with a real pad (or keyboard bindings
  on Pad 1).

## IDE and clangd

```bat
GenerateProjectFiles.bat
```

This writes a Visual Studio solution to `Engine\Intermediate\ProjectFiles\` (for browsing and debugging; keep building
with `Build.bat`) and the root `compile_commands.json` from the `LeonAutomationTests Win64 Development` Ninja tree,
which covers the Win64 engine modules and their tests.

clangd (VS Code, Cursor, or any editor) reads the root `compile_commands.json` through the committed settings:

| File | Role |
| --- | --- |
| `.clangd` | `CompilationDatabase: .` (root `compile_commands.json`); no diagnostics under `ThirdParty/` |
| `.clang-tidy` | clang-tidy checks used by clangd |

`.vscode/` is git-ignored: each developer keeps their own editor settings (for clangd on Win64, `--query-driver` for
MSVC `cl.exe` gives it the system headers; tabs of width 4 and a ruler at 120 match the style).

Re-run `GenerateProjectFiles.bat` after adding modules or files, then **Developer: Restart Language Server**.
The database is Win64 only; PS2-only files (`Engine/Platforms/PS2/`, `Game/ThirdPerson/`) are not in it.

## Formatting and lint

```bat
Engine\Build\BatchFiles\FormatCode.bat            :: clang-format in place
Engine\Build\BatchFiles\FormatCode.bat --check    :: dry run, fails if a file needs formatting
Engine\Build\BatchFiles\Lint.bat                  :: format check + banned APIs (G4) + Win64 build of every engine target
```

`FormatCode.bat` runs the `clang-format` that `LEON_CLANG_FORMAT` names, else Visual Studio's LLVM one, else one on
`PATH`, with the repo's `.clang-format` (Epic style: tabs, Allman braces) on `Engine\Source`, `Engine\Platforms`,
`Engine\Plugins` and `Game`, skipping `ThirdParty`, `Intermediate` and `Binaries`. The repository is formatted with
clang-format 20 (20.1.8 is the reference version: `pip install clang-format==20.1.8`); another major version formats a
few constructs differently, and the script warns when the one it found is not version 20. `Lint.bat` then runs
`Engine\Build\BatchFiles\CheckBannedApis.ps1` (gate G4), which fails on glm, nlohmann, the `std::` containers,
strings, `string_view`, streams, functions and smart pointers and their headers (D2), iostream, the `printf` family
(`vfprintf`, `_snprintf`, ...), the removed legacy math bridges and `FLegacyCoordinateConversion` outside the tests in
engine or game code; only ThirdParty, Core's platform HAL sources, the `printf` family inside `Core/Private`,
LeonHeaderTool and the test program mains are exempt. Last it builds every Win64 engine target and ShooterGame's. Coding
rules: [CODING_STANDARD.md](CODING_STANDARD.md).

## Checks before a push

The gates run locally, from `Engine\Build\BatchFiles\` on Win64:

- `Lint.bat`: the format check (G1, `FormatCode.bat --check`), `CheckBannedApis.ps1` (G4) and the Win64 Development
  build of every engine target and ShooterGame's.
- `RunTests.bat`: `LeonAutomationTests`, the LeonHeaderTool golden tests, `ShooterGameTests` and TestPAL, on Win64.
- `CheckReimport.bat Game\ThirdPerson\ThirdPerson.lproj Game\ShooterGame\ShooterGame.lproj` (G5, on a clean
  checkout of the content): reimporting the content leaves it unchanged.
- `SmokeTest.bat` (G6): ShooterGame headless with `bot_fill`, ten pawns, exit code 0.
- `BotMatch.bat` (10 rounds, seed 7): the headless bot match, played twice with the same result.
- `BuildCookRun.bat`: the staged builds ([BUILD.md — Staging and Shipping](BUILD.md#staging-and-shipping)).
- The root `Package.bat` builds and packages ShooterGame Win64 Shipping into `Packages\Win64\` and ThirdPerson and
  TestPAL PS2 Development (in Docker) into `Packages\PS2\`; `-NoWin64` / `-NoPS2` skip a platform.

The PS2 ELF sizes (G3) are measured with the toolchain's `mips64r5900el-ps2-elf-size` in the ps2dev image when a phase
is recorded ([Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md)). TestPAL on PS2 runs in PCSX2
([Run TestPAL in PCSX2](#run-testpal-in-pcsx2)).

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `vcvars64.bat not found` | Install Visual Studio 18 (2026) or 2022 with the C++ workload |
| `Ninja not on PATH` | `winget install Ninja-build.Ninja` and open a new terminal |
| `cmake` is not recognized (`Setup.bat`) | Install CMake 3.24+ and add it to `PATH` |
| `LeonBuildTool: unknown platform` / `configuration must be ...` | Platforms: `Win64`, `PS2` (LeonBuildTool also registers `Linux`, which is not an official platform); configurations: `Debug`, `Development`, `Shipping` |
| `LeonBuildTool: unknown module 'X' (required by ...)` | A dependency name is misspelled or its `.Build.cmake` is missing |
| `module 'X' is not available on PS2` | A module used on PS2 depends on a Desktop-only module; move the dependency under a `_Desktop` suffix |
| `LeonBuildTool: Docker build failed` | Start Docker Desktop; check that `docker run hello-world` works |
| `PS2DEV is not set` | Only with `-NoDocker` or inside a custom container: export `PS2DEV` / `PS2SDK` |
| `ELF not found` in `RunPCSX2.ps1` | Build first (`-Build`) or check `-Configuration` |
| `PCSX2 not found` | Install it or set `$env:LEON_PCSX2` to `pcsx2-qt.exe` |
| Pad does nothing in PCSX2 | Bind it in the global Controller Port 1 settings (see [PCSX2 notes](#pcsx2-notes)) |
| clangd reports missing includes | Run `GenerateProjectFiles.bat`, then restart the language server |
| A third-party download fails the hash check | Delete the archive in `Engine\Intermediate\ThirdPartyDownloads\` and run `Setup.bat` again |

More: [BUILD.md](BUILD.md) · [ARCHITECTURE.md](ARCHITECTURE.md) · [CODING_STANDARD.md](CODING_STANDARD.md) ·
[LIBRARIES.md](LIBRARIES.md) · [UnrealEngine427/](UnrealEngine427/README.md)
