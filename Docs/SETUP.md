# Leon Engine — setup

How to get a working Windows machine that builds the engine (Win64), runs the automation tests, and builds and runs
the PS2 game in PCSX2. The build system itself is documented in [BUILD.md](BUILD.md).

## Requirements

| Need | Required for | Notes |
| --- | --- | --- |
| Visual Studio 2026 (folder `18`) or 2022, any edition | Win64 builds | Workloads/components: **Desktop development with C++**, **C++ CMake tools for Windows**, **C++ Clang tools for Windows** (LLVM: `clang-format`, `clangd`). The scripts look in `%ProgramFiles%\Microsoft Visual Studio\18\` then `...\2022\`, editions Community, Professional, Enterprise |
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

`Setup.bat` downloads the pinned third-party archives (GLFW, GLM, miniaudio, nlohmann/json, tinyobjloader, Catch2,
Jolt) into `Engine/Intermediate/ThirdPartyDownloads/`, checks their SHA-256 and extracts them next to their module
rules. Run it again after pulling a change that bumps a library. List of libraries: [LIBRARIES.md](LIBRARIES.md).

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
tests of every module in its closure (`<Module>/Private/Tests/`): first the UE automation tests (Core, 31 today),
then the Catch2 tests of the modules not migrated yet (124 test cases). The exit code is non-zero if either set
fails. `-automation=<filter>` runs only the automation tests whose name contains `<filter>`, `-noautomation` skips
them and `-automationonly` skips Catch2; every other argument is passed to Catch2:

```bat
Engine\Build\BatchFiles\RunTests.bat -automation=System.Core.Containers -automationonly
Engine\Build\BatchFiles\RunTests.bat -noautomation "[physics]"
Engine\Build\BatchFiles\RunTests.bat --list-tests
```

`TestPAL` runs the same Core automation tests without Catch2, on any platform, and ends with
`TestPAL: PASSED (N test(s), 0 failed)` plus memory and name-pool numbers:

```bat
Engine\Build\BatchFiles\Build.bat TestPAL Win64 Development
Engine\Binaries\Win64\TestPAL.exe [-filter=<text>]
```

On PS2 see [Run TestPAL in PCSX2](#run-testpal-in-pcsx2).

## LeonGame

`LeonGame` is the engine's game executable (UE: `UE4Game`). It loads **one level** (`.llev`) and runs the default
game mode (`ADefaultGameMode`) on it:

```bat
Engine\Binaries\Win64\LeonGame.exe [-map=<.llev>] [-nullrhi] [--tick <Hz>] [--show-stats]
```

Without `-map=` it opens `Engine/Content/LevelTemplates/Starter.llev`; a `-map=` path is taken relative to the working
directory, else relative to `Engine/Content` (`-map=LevelTemplates/Blank.llev`). `-nullrhi` runs headless (no window,
silent audio) at `--tick` Hz (default 60); `--show-stats` shows the HUD stats. Flags are parsed in
`Engine/Source/Runtime/Launch/Private/Desktop/GameApplication.cpp`. Levels: [LEVELS.md](LEVELS.md).

## Cook

```bat
Engine\Build\BatchFiles\Cook.bat staticmesh --obj Mesh.obj --out Mesh.lmesh
Engine\Build\BatchFiles\Cook.bat recipe CookRecipe.json
```

`Cook.bat` builds `LeonCook` and passes the arguments through. Modes: `staticmesh`, `recipe`
(run `Cook.bat --help` for the options). Formats: [ASSET_FORMATS.md](ASSET_FORMATS.md). Tool reference:
[TOOLS.md](TOOLS.md).

## PS2

The PS2 game is `Game/ThirdPerson`, an isolated project built against the engine with `-Project=`. With Docker
Desktop running:

```bat
Engine\Build\BatchFiles\Build.bat ThirdPerson PS2 Development -Project=%CD%\Game\ThirdPerson\ThirdPerson.lproj
```

→ `Game\ThirdPerson\Binaries\PS2\ThirdPerson.elf`. The first build pulls the ps2dev image. PS2 builds do not need
Visual Studio. When `PS2DEV` is not set, LeonBuildTool runs itself inside the container; with a local ps2dev install
(`PS2DEV`, `PS2SDK`, and `$PS2DEV/ee/bin` on `PATH`) it builds on the host. From Git Bash, WSL or Linux use
`Engine/Build/BatchFiles/Linux/Build.sh` with the same arguments. Details:
[BUILD.md — PS2 builds in Docker](BUILD.md#ps2-builds-in-docker) and the platform extension
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

TestPAL runs Core's automation tests on the EE (35 on PS2: the `FPaths`, `FLegacyTransform` and glm comparison tests
are desktop-only) and
logs to the EE console. With the EE console enabled (see [PCSX2 notes](#pcsx2-notes)), read
`%USERPROFILE%\Documents\PCSX2\logs\emulog.txt` for the `TestPAL: PASSED (35 test(s), 0 failed)` line and the
`LogTestPAL` memory / name-pool lines; their numbers are tracked in
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

VS Code / Cursor with clangd work out of the box with the committed settings:

| File | Role |
| --- | --- |
| `.clangd` | `CompilationDatabase: .` (root `compile_commands.json`); no diagnostics under `ThirdParty/` |
| `.vscode/settings.json` | clangd `--compile-commands-dir=${workspaceFolder}`, `--query-driver` for MSVC `cl.exe` (system headers), MS C++ IntelliSense off, tabs of width 4, ruler at 120 |
| `.vscode/c_cpp_properties.json` | `Win64` configuration on the root `compile_commands.json` |
| `.clang-tidy` | clang-tidy checks used by clangd |

Re-run `GenerateProjectFiles.bat` after adding modules or files, then **Developer: Restart Language Server**.
The database is Win64 only; PS2-only files (`Engine/Platforms/PS2/`, `Game/ThirdPerson/`) are not in it.

## Formatting and lint

```bat
Engine\Build\BatchFiles\FormatCode.bat            :: clang-format in place
Engine\Build\BatchFiles\FormatCode.bat --check    :: dry run, fails if a file needs formatting
Engine\Build\BatchFiles\Lint.bat                  :: format check + Win64 build of every engine target
```

`FormatCode.bat` uses Visual Studio's LLVM `clang-format` (or one on `PATH`) with the repo's `.clang-format` (Epic
style: tabs, Allman braces) on `Engine\Source`, `Engine\Platforms`, `Engine\Plugins` and `Game`, skipping `ThirdParty`,
`Intermediate` and `Binaries`. Coding rules: [CODING_STANDARD.md](CODING_STANDARD.md).

## Continuous integration

`.github/workflows/ci.yml` runs two jobs on every push to `main` / `master` and on pull requests:

- **ps2**: inside the pinned ps2dev image, builds `ThirdPerson` and `BlankProgram` for PS2 with
  `Engine/Build/BatchFiles/Linux/Build.sh` and uploads `ThirdPerson.elf`.
- **win64**: `Setup.bat`, `RunTests.bat`, then builds `LeonGame` and `LeonCook`.

Formatting is checked locally with `Lint.bat` (the runner's clang-format version may differ from Visual Studio's).

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `vcvars64.bat not found` | Install Visual Studio 18 (2026) or 2022 with the C++ workload |
| `Ninja not on PATH` | `winget install Ninja-build.Ninja` and open a new terminal |
| `cmake` is not recognized (`Setup.bat`) | Install CMake 3.24+ and add it to `PATH` |
| `LeonBuildTool: unknown platform` / `configuration must be ...` | Platforms: `Win64`, `Linux`, `PS2`; configurations: `Debug`, `Development`, `Shipping` |
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
