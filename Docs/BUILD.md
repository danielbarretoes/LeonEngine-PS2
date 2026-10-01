# LeonBuildTool — build system reference

LeonEngine builds with **LeonBuildTool**, our homologue of Unreal Engine 4.27's UnrealBuildTool (UBT). It is written
entirely in CMake: a script-mode driver reads the module and target rules, then generates and drives a Ninja build tree.
There is no root `CMakeLists.txt`; you always go through LeonBuildTool (usually via the batch files).

| Piece | Path |
| --- | --- |
| Driver (command line, `UnrealBuildTool.exe` equivalent) | `Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake` |
| Build-tree generator (configured by the driver, never by hand) | `Engine/Source/Programs/LeonBuildTool/CMakeLists.txt` |
| Rules API | `Engine/Source/Programs/LeonBuildTool/Configuration/{ModuleRules,TargetRules,BuildTarget,CompileEnvironment}.cmake` |
| Discovery, descriptors, downloads | `Engine/Source/Programs/LeonBuildTool/System/{ModuleDiscovery,PlatformRegistry,ProjectDescriptor,PluginDescriptor,ThirdPartyDependencies}.cmake` |
| Built-in platform | `Engine/Source/Programs/LeonBuildTool/Platform/Windows/LeonBuildWindows.cmake` |
| PS2 platform extension | `Engine/Platforms/PS2/Source/Programs/LeonBuildTool/{LeonBuildPS2,PS2Toolchain}.cmake` |
| Batch files | `Engine/Build/BatchFiles/` and the repo root (`Setup.bat`, `GenerateProjectFiles.bat`) |

Requires CMake 3.24 or later and Ninja. Host setup is in [SETUP.md](SETUP.md).

## UE ↔ Leon mapping

| Unreal Engine 4.27 | LeonEngine |
| --- | --- |
| UnrealBuildTool (C#) | LeonBuildTool (`cmake -P LeonBuildTool.cmake -- ...`) |
| `Engine/Build/BatchFiles/Build.bat`, `Clean.bat`, `Rebuild.bat` | same names and arguments |
| Root `Setup.bat` / `GitDependencies` | root `Setup.bat` (`-Mode=Setup`: pinned downloads) |
| Root `GenerateProjectFiles.bat` | root `GenerateProjectFiles.bat` (`-Mode=GenerateProjectFiles`) |
| `<Module>.Build.cs` (`ModuleRules`) | `<Module>.Build.cmake` → `leon_module()` |
| `PublicDependencyModuleNames` / `PrivateDependencyModuleNames` | `PUBLIC_DEPENDENCIES` / `PRIVATE_DEPENDENCIES` |
| `CircularlyReferencedDependentModules` | `CIRCULAR_DEPENDENCIES` |
| `PublicDefinitions`, `PublicIncludePaths`, `PublicSystemLibraries` | `PUBLIC_DEFINITIONS`, `PUBLIC_INCLUDE_PATHS`, `PUBLIC_SYSTEM_LIBRARIES` |
| `RuntimeDependencies` | `RUNTIME_DEPENDENCIES` (copied beside the executable) |
| `if (Target.Platform == ...)` inside a `Build.cs` | `_<Platform>` or `_<Group>` keyword suffix (`PUBLIC_DEPENDENCIES_Desktop`) |
| `Type = ModuleType.External` (ThirdParty) | `TYPE External` (default under `Source/ThirdParty/`) |
| Editor modules (`Engine/Source/Editor`, descriptor module type `Editor`) | `TYPE Editor` (default under `Source/Editor/`): desktop only, rejected in a `Game` target |
| Platform-extension module rules under `Engine/Platforms/<P>/` | `<Module>_<P>.Build.cmake` → `leon_module_extend()` |
| `<Target>.Target.cs` (`TargetRules`) | `<Target>.Target.cmake` → `leon_target()` |
| `TargetType.Game` / `TargetType.Program` | `TYPE Game` / `TYPE Program` |
| `LaunchModuleName`, `ExtraModuleNames` | `LAUNCH_MODULE`, `EXTRA_MODULE_NAMES` |
| `EnablePlugins` / `DisablePlugins` | `ENABLE_PLUGINS` / `DISABLE_PLUGINS` |
| `bCompileAgainstEngine` → `WITH_ENGINE` (its defaults: games on, programs off) | `WITH_ENGINE`: 1 for `TYPE Game`, 0 for `TYPE Program` (no option) |
| `UE4Game.Target.cs` | `Engine/Source/LeonGame.Target.cmake` |
| `RunUAT BuildCookRun` (AutomationTool) | `Engine/Build/BatchFiles/BuildCookRun.bat` ([Staging and Shipping](#staging-and-shipping)) |
| `.uproject` / `.uplugin` | `.lproj` / `.lplugin` (JSON, same field names) |
| `UBT_COMPILED_PLATFORM` | `LBT_COMPILED_PLATFORM` |
| `UEBuildPlatform` / `UnrealTargetPlatform` | `leon_register_platform()` (platform registry) |
| Statically linked module list (monolithic) | generated `<Target>.ModuleInit.gen.cpp` |

More mappings (modules, types, deviations): [UnrealEngine427/LeonMapping.md](UnrealEngine427/LeonMapping.md).

## Command line

```
cmake -P Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake -- <Target> <Platform> <Configuration> [options]
cmake -P Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake -- -Mode=Setup
cmake -P Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake -- -Mode=GenerateProjectFiles [-Project=<file>]
```

The `--` is required: without it CMake would parse `-Project=...` as its own `-P` option.

| Argument | Values |
| --- | --- |
| `<Target>` | a target from a `*.Target.cmake` (`LeonGame`, `LeonCook`, `LeonPak`, `LeonAutomationTests`, `TestPAL`, `BlankProgram`, or a project's target such as `ShooterGame`) |
| `<Platform>` | a registered platform: `Win64` (development and editor) or `PS2` (the target) |
| `<Configuration>` | `Debug`, `Development`, `Shipping` |
| `-Project=<file.lproj>` | build a project's target instead of an engine target. Relative paths are resolved from the current directory |
| `-Mode=Build` | default: configure if needed, then build the target |
| `-Mode=Clean` (or `-Clean`) | delete the target's build tree |
| `-Mode=Rebuild` | Clean, then Build |
| `-Mode=GenerateClangDatabase` | configure only, then copy the tree's `compile_commands.json` to the repo root (for clangd) |
| `-Mode=GenerateProjectFiles` | IDE project files (no positional arguments needed) |
| `-Mode=Setup` | download every pinned third-party dependency (no positional arguments) |
| `-NoDocker` | never re-launch inside the platform's Docker image (the SDK must be installed on the host) |
| `-KeepGoing` | keep compiling after errors (`ninja -k 0`), so you see every error in one run |

Anything else starting with `-` is rejected (`unknown option`).

### What a build does

1. Loads the platform registry and checks the platform and configuration.
2. If the platform has a Docker image (PS2) and its SDK variable (`PS2DEV`) is not set, it re-runs itself inside the
   container (see [PS2 builds in Docker](#ps2-builds-in-docker)) and stops.
3. Builds the host tools (`System/HostTools.cmake`): `Engine/Source/Programs/LeonHeaderTool` with the host compiler
   and Ninja, Release, into `Engine/Intermediate/Build/HostTools/<Win64|Linux|LinuxMusl>/`. It is configured once
   and is a Ninja no-op when up to date. Inside the PS2 Docker image the host is Alpine (`LinuxMusl`, g++).
4. Configures the build tree with `-G Ninja`, `LEON_PLATFORM`, `LEON_CONFIGURATION`, `LEON_PROJECT_FILE`,
   `LEON_HEADER_TOOL` (the tool built in step 3) and the platform's `CMAKE_BUILD_TYPE`. The arguments are stored in
   `<tree>/LeonBuildTool.args`, and the tree is reconfigured only when they change. The Ninja build picks up new or
   removed source files and headers by itself through `CONFIGURE_DEPENDS` globs.
5. Runs `cmake --build <tree> --target <Target>`.

| Platform | Debug | Development | Shipping |
| --- | --- | --- | --- |
| Win64 | `Debug` | `RelWithDebInfo` | `Release` |
| PS2 | `Debug` | `Release` (EE `-O2 -DNDEBUG`, no debug info: the toolchain forces `CMAKE_CXX_FLAGS_RELEASE`, since CMake's GNU default appends `-O3`, whose unrolling and inlining grow the code for the EE's 16 KB instruction cache; [ps2-shipping](PLANS/ps2-shipping.md) D9) | `Release` |

### Build trees and outputs

| | Engine targets (no `-Project`) | Project targets (`-Project=`) |
| --- | --- | --- |
| Build tree | `Engine/Intermediate/Build/<Platform>/<Configuration>/`, one for every target (below) | the same tree |
| Executable | `Engine/Binaries/<Platform>/` | `<Project>/Binaries/<Platform>/` |
| Targets configured | every engine target allowed on the platform | only the project's targets |
| CMake project / solution name | `LeonEngine` | the `.lproj` file name |

**One tree per platform and configuration** (UE: the shared build environment, where the engine's intermediates are
the same for every game). The engine modules compile to the same commands whatever the project, so every target of a
platform and configuration builds in `Engine/Intermediate/Build/<Platform>/<Configuration>/`: building another project
or an engine program reconfigures the tree (LeonBuildTool compares the configure arguments, the project included) and
compiles only what that target adds (its game modules, its `Launch` with the project's macros). Measured on PS2
Development, the four targets `Package.bat` builds compiled 705 units in three trees and compile 475 in one. The
per-project trees of older builds (`<Project>/Intermediate/Build/`) are no longer used and can be deleted. Two builds
of the same platform and configuration must not run at the same time (they share the tree); `Clean` and `Rebuild`
remove the whole tree.

The executable is named `<OutputName><Suffix>` in `Development` and `<OutputName>-<Platform>-<Configuration><Suffix>` in
other configurations. Suffix: `.exe` on Win64, `.elf` on PS2. Examples:

- `Engine/Binaries/Win64/LeonGame.exe`, `Engine/Binaries/Win64/LeonGame-Win64-Debug.exe`
- `Game/ShooterGame/Binaries/PS2/ShooterGame.elf`

Other generated folders:

- `Engine/Intermediate/ThirdPartyDownloads/`: downloaded archives.
- `Engine/Intermediate/ProjectFiles/`: IDE solution.
- `Engine/Intermediate/Build/HostTools/<Host>/`: LeonHeaderTool.
- `<tree>/Generated/<Target>.ModuleInit.gen.cpp`.
- `<tree>/Inc/<Module>/`: reflection code for reflected modules.
`Binaries/`, `Intermediate/`, `Saved/`, `Packages/` (under each game or `Engine/`) and the root `compile_commands.json` are ignored by git.
`DerivedDataCache/` is reserved (UE layout) and also ignored; Leon has no DDC pipeline yet — do not create or commit one.

### Generated folders (what belongs where)

| Folder | Role | Who writes it |
| --- | --- | --- |
| `<Engine\|Project>/Binaries/<Platform>/` | Executables / ELFs | LeonBuildTool |
| `Engine/Intermediate/Build/<Platform>/<Config>/` | CMake/Ninja tree (every target), reflection, module init | LeonBuildTool |
| `Engine/Intermediate/Build/HostTools/<Host>/` | LeonHeaderTool | HostTools.cmake |
| `<Project>/Saved/Logs/` | Desktop log file (`FOutputDeviceFile`) | Launch / LeonCook |
| `<Project>/Saved/Cooked/<Platform>/` | Cook output | LeonCook |
| `<Project>/Saved/StagedBuilds/<Platform>/` | BuildCookRun stage (release layout) | BuildCookRun.ps1 |
| `<Project>/Saved/Config/` | User ini layer (desktop) | `GConfig::Flush` |
| `Game/<Project>/Packages/<Platform>/` | Distributable archive for that game (`Package.bat`) | Package.ps1 |
| `Engine/Packages/PS2/<Program>/` | Distributable archive for engine PS2 programs (TestPAL, GSConformance, VU1Conformance) | Package.ps1 |
| `Engine/Programs/<App>/Saved/` | Runtime only when a program runs **without** a `.lproj` (UE fallback: `ProjectDir` = `Engine/Programs/<App>/`). Not source — gitignored like any `Saved/` | Launch / LeonCook |

Desktop log override (default layout unchanged): `-LogDir=<path>` or environment `LEON_LOG_DIR` (command line wins). Useful in CI so logs do not land under `Engine/Programs/*/Saved/`. Applied by `FPaths::ApplyLogDirectoryOverrides` before the log file opens.

### PS2 staging matrix

Two staging paths exist on purpose; `Package.ps1` publishes every artifact through `Publish-Package` into the owner's `Packages\` folder:

| Use | Writer | Layout on disk | Artifacts → package destination |
| --- | --- | --- | --- |
| **Dev loop** | `RunPCSX2.ps1` (`-StageOnly` or before launch) | Config next to the build ELF: `<…>/Binaries/PS2/Engine/…`, `<…>/Binaries/PS2/<Project>/…` (`host:` = that folder) | TestPAL / GSConformance / VU1Conformance → `Engine/Packages/PS2/<Name>/` (the conformance programs have no config to stage) |
| **Release stage** | `BuildCookRun.bat -platform=PS2 -stage` | `<Project>/Saved/StagedBuilds/PS2/`: ELF at the **root**, cooked loose or `.lpak`, optional `LeonCommandLine.txt` | ShooterGame → `Game/ShooterGame/Packages/PS2/` |
| **Disc** ([ps2-shipping](PLANS/ps2-shipping.md) N23) | `BuildCookRun.bat -platform=PS2 -stage -pak -iso` | `<Project>/Saved/StagedBuilds/PS2/<Project>.iso`: `SYSTEM.CNF`, the ELF as `SLUS_990.01`, the pak, `AUDSRV.IRX` (ISO 9660 names, in that order on the disc) | boots in PCSX2 from `cdrom0:` (`MeasurePS2 -Iso`) |

Win64 `-stage` always needs `-pak`. PS2 may stage loose (HostFS) or with `-pak`. PCSX2 BIOS and user files stay outside the repo (`%USERPROFILE%\Documents\PCSX2\bios\`).

## Batch files

Windows batch files live in `Engine/Build/BatchFiles/` (UE layout); run them from any directory. At the root,
`Package.bat [-NoWin64] [-NoPS2] [-Only <Name[,Name]>]` (steps in `Engine/Build/BatchFiles/Package.ps1`) runs
`Setup.bat`, then `BuildCookRun.bat` for ShooterGame Win64 Shipping into `Game\ShooterGame\Packages\Win64\`, then for
PS2 builds ShooterGame under `Game\ShooterGame\Packages\PS2\`, and TestPAL / GSConformance / VU1Conformance under
`Engine\Packages\PS2\<Name>\` (Docker Desktop must be running; it checks before the Win64 build). `-Only` packages only
the artifacts it names (`ShooterGame`, `TestPAL`, `GSConformance`, `VU1Conformance`; Win64 has only ShooterGame) and
leaves the other `Packages\` folders as they are: `Package.bat -NoWin64 -Only ShooterGame` is ShooterGame for the PS2
alone. Double-clicked it pauses at the end (not when `CI` is set, for an unattended run); every `Packages\` folder is
git-ignored.

| File | Usage | What it does |
| --- | --- | --- |
| `Build.bat` | `Build.bat <Target> <Platform> <Config> [-Project=<file>] [-Mode=...] [-NoDocker] [-KeepGoing]` | Loads the MSVC environment (`GetVSEnv.bat`) unless the platform is `PS2`, then runs LeonBuildTool with all arguments |
| `Clean.bat` | `Clean.bat <Target> <Platform> <Config> [-Project=<file>]` | `Build.bat ... -Mode=Clean` |
| `Rebuild.bat` | `Rebuild.bat <Target> <Platform> <Config> [-Project=<file>]` | `Build.bat ... -Mode=Rebuild` |
| `RunTests.bat` | `RunTests.bat [-automation=<filter>]` | Builds `LeonAutomationTests Win64 Development` and runs `Engine\Binaries\Win64\LeonAutomationTests.exe` from the repo root: every automation test, or only those whose name contains `<filter>`. It then runs the LeonHeaderTool golden tests (`LeonHeaderTool -Test`), then builds `ShooterGameTests` (`-Project=Game\ShooterGame\ShooterGame.lproj`) and runs `Game\ShooterGame\Binaries\Win64\ShooterGameTests.exe` with the same arguments (ShooterGame's tests), then builds `TestPAL Win64 Development` and runs `Engine\Binaries\Win64\TestPAL.exe` (no filter); fails if any of them fails. At 0.24.0: 575 engine tests, 35 golden cases, 99 ShooterGame tests, 171 TestPAL tests; at 0.25.0: 605, 35, 125 and 174 |
| `Cook.bat` | `Cook.bat <LeonCook arguments>` | Builds `LeonCook Win64 Development` and runs `Engine\Binaries\Win64\LeonCook.exe` (`Cook.bat -run=ImportAssets -reimport -all`) |
| `CheckReimport.bat` | `CheckReimport.bat [<Project>.lproj ...]` | Gate G5: builds LeonCook, reimports the engine content (and each project's) with `-run=ImportAssets -reimport -all`, then fails when `git diff --exit-code` sees a change, or a new file appears, under `Engine/Content` or `Game/*/Content` (run it on a clean checkout of the content) |
| `FormatCode.bat` | `FormatCode.bat [--check]` | clang-format on every `.cpp/.h/.inl` under `Engine\Source`, `Engine\Platforms`, `Engine\Plugins`, `Game` (skips paths containing `ThirdParty`, `Intermediate`, `Binaries`). `--check` is a dry run that fails if a file needs formatting. The binary is `LEON_CLANG_FORMAT`, else Visual Studio's LLVM `clang-format`, else the one on `PATH`; the repository is formatted with clang-format 20 (20.1.8 is the reference: `pip install clang-format==20.1.8`), and the script warns when the version is another |
| `Lint.bat` | `Lint.bat` | `FormatCode.bat --check`, then `CheckBannedApis.ps1`, then builds `LeonAutomationTests`, `LeonCook`, `LeonPak`, `LeonGame` and `BlankProgram`, and ShooterGame's `ShooterGame` and `ShooterGameTests`, for Win64 Development |
| `SmokeTest.bat` | `SmokeTest.bat` | Gate G6: builds `ShooterGame Win64 Development`, runs `ShooterGame.exe "/Game/Maps/de_leon?team=CT" -nullrhi -ExitAfterFrames=120` (straight into a match past the main menu; the nine bots join around the player) (its log in `Game\ShooterGame\Saved\Logs\SmokeTest.log`) and fails unless the game exits with 0 and logs `ShooterGameMode: 10 pawn(s) at the end of the match, CT 5, T 5` |
| `BotMatch.bat` | `BotMatch.bat [Rounds] [Seed] [Map]` | Builds `ShooterGame Win64 Development`, plays `ShooterGame.exe [-map=<Map>] -nullrhi -benchmark -botmatch -rounds=<Rounds> -seed=<Seed>` (10, 7, de_leon; its log in `Game\ShooterGame\Saved\Logs\BotMatch[_<map>].log`) twice, and fails unless both exit with 0, log `Botmatch OK` and the two lines match (P21) |
| `BuildCookRun.bat` | `BuildCookRun.bat -project=<file> -platform=Win64\|PS2 [-configuration=Shipping\|Development] [-build] [-cook] [-stage] [-pak] [-iso] [-run] [-addcmdline="..."] [-align=<bytes>] [-pakorder=<file>] [-fullcook] [-region=NTSC\|PAL] [-discserial=<serial>]` | Builds the project's game, cooks it (incrementally; `-fullcook` cooks it all), stages it into `<Project>\Saved\StagedBuilds\<Platform>\` with its content in one `.lpak` (in a recorded open order with `-pakorder`), makes the PS2 disc image with `-iso`, and runs it (`BuildCookRun.ps1` has the steps; [below](#staging-and-shipping), [TOOLS.md](TOOLS.md#buildcookrun)) |
| `CheckBannedApis.ps1` | `powershell -File CheckBannedApis.ps1` (or `pwsh`) | Gate G4: scans `.h/.cpp/.inl` under `Engine\Source`, `Engine\Platforms`, `Engine\Plugins`, `Game` (comments ignored) and fails on glm, nlohmann, every `std::` container, string, `string_view`, stream, function and smart pointer and their headers (D2: `<string>`, `<functional>`, `<memory>`, `<sstream>`, `<vector>`, ...), `<iostream>` / `std::cout/cerr/clog`, the `printf` family (`vfprintf`, `_snprintf`, ...; allowed inside `Core/Private`), `LegacyGL` / `FLegacyTransform` / `LegacyAxes`, or `FLegacyCoordinateConversion` outside the tests (`Public/Tests`, `Private/Tests`); ThirdParty, Core's platform HAL sources (`Core/Private/Windows`, the PS2 Core), LeonHeaderTool and the test program mains are not scanned; `-Root <dir>` scans another tree; exceptions in [CODING_STANDARD.md §4](CODING_STANDARD.md#4-language) |
| `RunGates.bat` | `RunGates.bat [-PS2] [-Measure]` | Every local gate in order (the repository has no CI), each a `[ OK ]` or `[FAIL]` line with its log in `Engine\Saved\Gates\<Gate>.log`, then `RunGates OK`: `Lint.bat`, `RunTests.bat`, `CheckReimport.bat`, `SmokeTest.bat`, `BotMatch.bat 10 7` (de_leon, then `10 7 /Game/Maps/de_puerto`), `Cook.bat -run=ValidateAssets` for the engine and for ShooterGame; `-PS2` adds `Package.bat -NoWin64` (G3), `-Measure` `MeasurePS2.bat` ([TESTING.md](TESTING.md#automated)) |
| `MeasurePS2.bat` | `MeasurePS2.bat [-Project <dir>] [-Rounds 2] [-Seed 7] [-Seconds 120] [-NoBuild] [-TimeoutSeconds 900] [-Label <text>] [-Iso] [-PakOrder <file>] [-LogFileOpenOrder] [-ExtraArgs <args>]` | The PS2 frame in PCSX2, unattended (steps in `Engine\Platforms\PS2\Build\BatchFiles\MeasurePS2.ps1`): stages the game with `BuildCookRun -platform=PS2` (`-Iso`: the disc), runs a spectated bot match with `-LogFrameTimes` in PCSX2 without its window (settings: the user's `PCSX2.ini` with `Engine\Platforms\PS2\Build\PCSX2\Measure.ini` on top), reads the EE log until `ProfileSummary:`, writes `<Project>\Saved\Profiling\PS2Frame.csv` and prints a [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md) row |
| `GenerateProjectFiles.bat` | `GenerateProjectFiles.bat [-Project=<file>]` | `-Mode=GenerateProjectFiles`, then `LeonAutomationTests Win64 Development -Mode=GenerateClangDatabase` (root `compile_commands.json`) |
| `GetVSEnv.bat` | `call GetVSEnv.bat` (no arguments) | Helper for `Build.bat` and `GenerateProjectFiles.bat`: finds Visual Studio `18` then `2022` (Community, Professional, Enterprise), runs its `vcvars64.bat` quietly, prepends `C:\Program Files\CMake\bin` to `PATH` and checks that CMake and Ninja are there |

Repo root:

| File | What it does |
| --- | --- |
| `Setup.bat` | `-Mode=Setup`: downloads the pinned third-party archives |
| `GenerateProjectFiles.bat` | forwards to `Engine\Build\BatchFiles\GenerateProjectFiles.bat` |

The batch files have no shell equivalents: Win64 is the only development platform. From a Linux shell (WSL, a CI
runner) a PS2 build runs LeonBuildTool directly, `cmake -P Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake --
<Target> PS2 <Config> [-Project=<file>]`, which starts the ps2dev container as on Windows
([PS2 builds in Docker](#ps2-builds-in-docker)).

Examples:

```bat
Setup.bat
Engine\Build\BatchFiles\Build.bat LeonGame Win64 Development
Engine\Build\BatchFiles\Build.bat BlankProgram PS2 Development
Engine\Build\BatchFiles\Build.bat ShooterGame Win64 Development -Project=%CD%\Game\ShooterGame\ShooterGame.lproj
Engine\Build\BatchFiles\Build.bat ShooterGame PS2 Development -Project=%CD%\Game\ShooterGame\ShooterGame.lproj
Engine\Build\BatchFiles\Rebuild.bat LeonAutomationTests Win64 Debug -KeepGoing
Engine\Build\BatchFiles\RunTests.bat -automation=System.Engine.PhysScene
Engine\Build\BatchFiles\RunTests.bat -automation=System.Core
Engine\Build\BatchFiles\Build.bat TestPAL PS2 Development
```

### GenerateProjectFiles

`-Mode=GenerateProjectFiles` runs on a Windows host only and configures the generator for Win64 (`-A x64`) in
`Development`, with CMake's default generator (the newest Visual Studio), into
`Engine/Intermediate/ProjectFiles/` or `<Project>/Intermediate/ProjectFiles/` with `-Project=`. The solution is for
browsing and debugging; keep building with `Build.bat` (Ninja). A project whose targets are PS2-only would have no
Win64 target, so its solution would contain no executable.

## Modules — `<Module>.Build.cmake`

A module is a folder with a `<Module>.Build.cmake` file that calls `leon_module()`. Layout (UE module anatomy):

```
<Module>/
  <Module>.Build.cmake
  Public/      public headers (added to dependents' include path)
  Classes/     public headers too (UObject-style classes)
  Private/     sources (*.cpp, *.c) and private headers
  Private/Tests/   automation tests (only built into targets with COLLECT_AUTOMATION_TESTS)
```

A module with no `Public/`, `Private/` or `Classes/` folder is **flat** (UE game-module style): the whole folder is
both its sources and its public include path, and its tests live in `<Module>/Tests/`.

```cmake
leon_module(<Name>
  [TYPE Runtime|Developer|Editor|Program|External]
  [PLATFORMS <platform-or-group>...]
  [CXX_STANDARD <n>]
  [NO_MODULE_IMPLEMENTATION]
  [PUBLIC_DEPENDENCIES <Module>...] [PRIVATE_DEPENDENCIES <Module>...] [CIRCULAR_DEPENDENCIES <Module>...]
  [PUBLIC_DEFINITIONS ...] [PRIVATE_DEFINITIONS ...]
  [PUBLIC_INCLUDE_PATHS ...] [PRIVATE_INCLUDE_PATHS ...]
  [PUBLIC_SYSTEM_LIBRARIES ...]
  [EXCLUDE_SOURCES <glob relative to the module>...]
  [RUNTIME_DEPENDENCIES <file>...]
  [COMPILE_OPTIONS ...]
  [EXTERNAL_TARGETS <cmake-target>...]
  [DOWNLOAD_URL <url> DOWNLOAD_SHA256 <hash> DOWNLOAD_DIR <dir>])
```

| Keyword | Meaning |
| --- | --- |
| `TYPE` | Defaults from the folder: `Source/ThirdParty/` → `External`, `Source/Developer/` → `Developer`, `Source/Editor/` → `Editor`, `Source/Programs/` → `Program`, anything else (including project modules) → `Runtime`. An `Editor` module (UE: `Engine/Source/Editor`, edit-time code such as LeonEd) defaults to `PLATFORMS Desktop` (any other allow-list is an error) and may be linked by programs only: a `Game` target whose closure reaches one fails to configure |
| `PLATFORMS` | Allow-list of platforms or groups; empty means every platform. Depending on a module that is not allowed on the current platform is a configure error |
| `CXX_STANDARD` | Overrides the default: the **lowest** standard of the platforms the module is allowed on (see [Compile environment](#compile-environment)) |
| `NO_MODULE_IMPLEMENTATION` | Leave the module out of the generated module table (no `IMPLEMENT_MODULE` required). `Program` and `External` modules are never in the table |
| `PUBLIC_DEPENDENCIES` | Linked, and their public include paths / definitions propagate to this module's dependents |
| `PRIVATE_DEPENDENCIES` | Linked for this module only |
| `CIRCULAR_DEPENDENCIES` | Modules whose headers include each other (propagated like public ones; ignored for the dependency order) |
| `PUBLIC_DEFINITIONS` / `PRIVATE_DEFINITIONS` | Preprocessor definitions |
| `PUBLIC_INCLUDE_PATHS` / `PRIVATE_INCLUDE_PATHS` | Extra include paths, relative to the module folder (External modules: relative to the third-party folder) |
| `PUBLIC_SYSTEM_LIBRARIES` | System libraries to link (`psapi`, `dxgi`, PS2SDK `kernel`, `pad`, ...) |
| `EXCLUDE_SOURCES` | Globs relative to the module folder removed from the sources |
| `RUNTIME_DEPENDENCIES` | Files every executable linking the module gets beside it after its link (UE: `RuntimeDependencies`); a file missing at configure time is a warning and the executable is built without it. The PS2 AudioMixer's `$ENV{PS2SDK}/iop/irx/audsrv.irx` (the IOP module its audio loads), which `BuildCookRun` stages with the ELF |
| `COMPILE_OPTIONS` | Extra private compiler options |
| `EXTERNAL_TARGETS` | External modules: CMake targets created by `LeonExternal_<Name>()` |
| `DOWNLOAD_URL`, `DOWNLOAD_SHA256`, `DOWNLOAD_DIR` | External modules: pinned archive (see [ThirdParty modules](#thirdparty-modules)) |

A module name may be defined only once; unknown keywords are configure errors.

### Platform and group suffixes

Every list keyword also accepts a `_<Platform|Group>` suffix; the values apply only when the current platform matches
that name. The names come from the platform registry:

| Platform | Names that apply (platform, groups, header folder) |
| --- | --- |
| `Win64` | `Win64`, `Windows`, `Microsoft`, `Desktop` |
| `PS2` | `PS2`, `Console` |

Real examples from the engine:

```cmake
# Engine/Source/Runtime/Core/Core.Build.cmake
leon_module(Core
	PUBLIC_SYSTEM_LIBRARIES_Windows psapi ole32
)

# Engine/Source/Runtime/ApplicationCore/ApplicationCore.Build.cmake
leon_module(ApplicationCore
	PUBLIC_DEPENDENCIES Core InputCore RHI
	PRIVATE_DEPENDENCIES_Desktop GLFW STB
)

# Engine/Source/Runtime/Launch/Launch.Build.cmake
leon_module(Launch
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore RHI
	# The .lproj descriptor is loaded in PreInit (IProjectManager).
	PRIVATE_DEPENDENCIES Projects
	# Desktop games tick GEngine (UGameEngine) from FEngineLoop; Engine reaches the Renderer module only by name
	# (IRendererModule), so the launch module links it (UE: Launch's Renderer dependency), and PreInit's RHIInit needs
	# the platform RHI (OpenGLDrv; the PS2 extension links PS2RHI).
	PRIVATE_DEPENDENCIES_Desktop Engine Renderer OpenGLDrv
)
```

### Platform folder filtering

As in UBT, a source folder named after a platform, group or header folder that does **not** apply to the current
platform is skipped. On Win64, `Private/PS2/` and `Private/Console/` are not compiled; on PS2, `Private/Windows/`,
`Private/Microsoft/` and `Private/Desktop/` are not. Folders such as `GenericPlatform/` are not
platform names and always compile. The same rule applies to `Private/Tests/`.

### Platform extensions — `leon_module_extend()`

Platform-specific code for an engine module lives under `Engine/Platforms/<Platform>/Source/` at the **same relative
path** as the module (UE platform extensions). When building for that platform, the extension folder's `Public/`,
`Classes/` and `Private/` are merged into the module:

```
Engine/Source/Runtime/Core/                       Core (all platforms)
Engine/Platforms/PS2/Source/Runtime/Core/         merged into Core when building PS2
  Core_PS2.Build.cmake
  Public/PS2PlatformMemory.h ...
  Private/PS2PlatformMemory.cpp ...
```

The folder is merged even without a rules file. To add dependencies, libraries or definitions, add
`<Module>_<Platform>.Build.cmake` and call `leon_module_extend()` with any of the list keywords (plain or suffixed);
the values are appended to the module's:

```cmake
# Engine/Platforms/PS2/Source/Runtime/ApplicationCore/ApplicationCore_PS2.Build.cmake
leon_module_extend(ApplicationCore
	PRIVATE_DEPENDENCIES PS2RHI
	PUBLIC_SYSTEM_LIBRARIES pad
)
```

A platform extension can also contain whole new modules (`Engine/Platforms/PS2/Source/Runtime/PS2RHI/PS2RHI.Build.cmake`
declares `PS2RHI` with `PLATFORMS PS2`). `Engine/Platforms/<P>/Source/` is only scanned when building for `<P>`.

## Targets — `<Target>.Target.cmake`

A target is an executable. Each `*.Target.cmake` calls `leon_target()`:

```cmake
leon_target(<Name> TYPE Game|Program
  [PLATFORMS <platform-or-group>...]
  [LAUNCH_MODULE <Module>]
  [EXTRA_MODULE_NAMES <Module>...]
  [ENABLE_PLUGINS <Plugin>...] [DISABLE_PLUGINS <Plugin>...]
  [COLLECT_AUTOMATION_TESTS [AUTOMATION_TEST_MODULES <Module>...]]
  [OUTPUT_NAME <name>])
```

| Keyword | Meaning |
| --- | --- |
| `TYPE` | `Game` or `Program` (required) |
| `PLATFORMS` | Allow-list; the target is not generated on other platforms. Empty means every platform |
| `LAUNCH_MODULE` | Module compiled straight into the executable (owns `main`). Default: `Launch` for games, `<Name>` for programs |
| `EXTRA_MODULE_NAMES` | Extra root modules. Game default: the modules listed in the `.lproj` |
| `ENABLE_PLUGINS` / `DISABLE_PLUGINS` | Override plugin enablement for this target |
| `COLLECT_AUTOMATION_TESTS` | Compile every closure module's `Private/Tests/**` into the executable (`WITH_DEV_AUTOMATION_TESTS=1`) |
| `AUTOMATION_TEST_MODULES` | Leon (P17), with `COLLECT_AUTOMATION_TESTS`: compile only these modules' tests (each must be in the closure). A project's test program links the engine's runner and collects its own modules' tests, which run with the project's config; the engine's tests stay in `LeonAutomationTests` |
| `OUTPUT_NAME` | Executable base name (default: the target name) |

Where targets are found:

- Engine build (no `-Project`): `Engine/Source/*.Target.cmake`, `Engine/Source/Programs/*/*.Target.cmake`,
  `Engine/Platforms/<Platform>/Source/Programs/*/*.Target.cmake`.
- Project build: `<Project>/Source/*.Target.cmake` only.

Targets in the repository:

| Target | File | Type | Platforms | Notes |
| --- | --- | --- | --- | --- |
| `LeonGame` | `Engine/Source/LeonGame.Target.cmake` | Game | Win64 | `EXTRA_MODULE_NAMES Engine AIModule`; creates `GEngine` and opens a map (`LeonGame [<map>]`, `-map=`; UE4Game) |
| `LeonCook` | `Engine/Source/Programs/LeonCook/LeonCook.Target.cmake` | Program | Desktop | the command-line editor: `LeonCook [<Project>.lproj] -run=<Commandlet>` (UE4Editor-Cmd), links LeonEd ([TOOLS.md](TOOLS.md#leoncook)) |
| `LeonPak` | `Engine/Source/Programs/LeonPak/LeonPak.Target.cmake` | Program | Desktop | the pak tool (UnrealPak): `LeonPak <pak> -create=<list> \| -list \| -test \| -extract=<dir>` ([TOOLS.md](TOOLS.md#leonpak)) |
| `LeonAutomationTests` | `Engine/Source/Programs/LeonAutomationTests/LeonAutomationTests.Target.cmake` | Program | Desktop | `COLLECT_AUTOMATION_TESTS` |
| `TestPAL` | `Engine/Source/Programs/TestPAL/TestPAL.Target.cmake` | Program | all | `COLLECT_AUTOMATION_TESTS`; runs the Core, CoreUObject, Json, Projects and PakFile automation tests (`-filter=<text>`), prints `TestPAL: PASSED (N test(s), 0 failed)`; on PS2 run it with `RunPCSX2.ps1 -Program TestPAL` |
| `BlankProgram` | `Engine/Source/Programs/BlankProgram/BlankProgram.Target.cmake` | Program | all | starts the linked modules and prints the platform |
| `GSConformance` | `Engine/Platforms/PS2/Source/Programs/GSConformance/GSConformance.Target.cmake` | Program | PS2 | draws GSCore's GS conformance scenes with their names, for the capture compared with the reference rasterizer ([TESTING.md](TESTING.md#gs-parity-g8)) |
| `VU1Conformance` | `Engine/Platforms/PS2/Source/Programs/VU1Conformance/VU1Conformance.Target.cmake` | Program | PS2 | runs fixed vertex batches through VU1's microprograms and compares their GIF packets with the C++ emitter's (plan D2's tolerances), prints `VU1Conformance: PASSED (N batch(es), 0 failed)` ([ps2-shipping](PLANS/ps2-shipping.md) N14) |
| `ShooterGame` | `Game/ShooterGame/Source/ShooterGame.Target.cmake` | Game | Win64, PS2 | the `.lproj`'s module `ShooterGame` (`Engine`, `AIModule`; on PS2 through the GS scene renderer, [ps2-engine](PLANS/ps2-engine.md)); opens `/Game/Maps/de_leon` ([README](../Game/ShooterGame/README.md)) |
| `ShooterGameTests` | `Game/ShooterGame/Source/ShooterGameTests.Target.cmake` | Program | Win64 | `LAUNCH_MODULE LeonAutomationTests` (the engine's runner), `EXTRA_MODULE_NAMES ShooterGame Renderer LeonEd`, `COLLECT_AUTOMATION_TESTS`, `AUTOMATION_TEST_MODULES ShooterGame` |

### How a target is assembled

1. **Roots**: `Core`, the launch module, `EXTRA_MODULE_NAMES` and the modules of every enabled plugin (filtered by the
   plugin's `PlatformAllowList`).
2. **Closure**: breadth-first over public, private and circular dependencies. An unknown module, or one not allowed
   on the platform, is a configure error that names the module that required it.
3. **Libraries**: every closure module except the launch module becomes one CMake library, created once per build tree
   and shared by all its targets: `Module.<Name>` (`STATIC`, or `INTERFACE` when it has no sources) and
   `ThirdParty.<Name>` (`INTERFACE`) for External modules.
4. **Executable**: the launch module's sources, the generated module table and (with `COLLECT_AUTOMATION_TESTS`) the
   test sources, linked against all the libraries.

**Include-only dependency on the launch module.** Because the launch module is compiled into the executable rather
than into a library, a module that depends on it (to reach `GEngineLoop`, say) only receives its public include paths
and an empty `LAUNCH_API`; the symbols resolve when the executable links.

### Generated module table

For each target LeonBuildTool writes `<tree>/Generated/<Target>.ModuleInit.gen.cpp`. It lists every `Runtime`,
`Developer` and `Editor` module of the closure (dependency order, without `NO_MODULE_IMPLEMENTATION`) as
`FStaticallyLinkedModuleInfo { Name, &InitializeModule_<Name>, RegisterReflection }`, returned by
`GetStaticallyLinkedModules()`. `RegisterReflection` is `&RegisterReflection_<Name>` for a reflected module (see
[Reflection](#reflection-leonheadertool)) and `nullptr` otherwise. The table also
defines `GPrimaryGameModuleName` (the first project module in `EXTRA_MODULE_NAMES` for games, otherwise `nullptr`).
It also writes where the engine and the project are, relative to the executable's folder
(`GLeonEngineDirFromBaseDir`, `GLeonProjectDirFromBaseDir`, `GLeonProjectName`); `FPaths` builds its desktop
directories from them (the PS2 uses the staged layout under the ELF folder instead).
`FModuleManager::StartupStaticallyLinkedModules()` creates and starts them in that order
(`Engine/Source/Runtime/Core/Public/Modules/ModuleManager.h`): for each module `InitializeModule`, its
`RegisterReflection`, `OnProcessLoadedObjectsCallback` (CoreUObject constructs the recorded types), then
`StartupModule`.

Each listed module must define its entry point once, in a `Private/*.cpp`:

```cpp
IMPLEMENT_MODULE(FDefaultModuleImpl, RHI)                                  // engine module
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ShooterGame, "ShooterGame") // a project's main module
```

`IMPLEMENT_MODULE` defines `extern "C" IModuleInterface* InitializeModule_<Name>()`; a module in the table without it
fails to link, which enforces the rule. `IMPLEMENT_GAME_MODULE` and `IMPLEMENT_PRIMARY_GAME_MODULE` are aliases.

### Reflection (LeonHeaderTool)

`Configuration/ReflectionRules.cmake` treats a module as reflected when one of its `Public/`, `Classes/` or `Private/`
headers has `#include "<Name>.generated.h"`. CoreUObject runs the generated code (P9); the reflected modules are
CoreUObject (its `NoExportTypes.h`), Engine, AIModule and UMG (P12; AnimationCore was until P14), EngineSettings and
InputCore (P13), plus their test fixtures in test targets. For a reflected module:

- LeonBuildTool writes `<tree>/Inc/<Module>/<Module>.lhtmanifest`.
- A custom command runs LeonHeaderTool. It writes `<Header>.generated.h`, `<Header>.gen.cpp`,
  `<Module>.init.gen.cpp` and `<Module>.lhttypes`, and only rewrites a file whose content changed.
- The `.gen.cpp` files compile into the module, and `<tree>/Inc/<Module>` becomes a public include path.
- The module table points `RegisterReflection` at `RegisterReflection_<Module>`.
- A `LeonHeaderTool.<Module>` custom target wraps the step: a module with a circular dependency on the reflected one
  (no build-order edge) waits for it with `add_dependencies` (UMG, circular on Engine, waits for Engine's headers).

Targets with `COLLECT_AUTOMATION_TESTS` also reflect `<Module>/Private/Tests/**.h` (the `<Module>.Tests` unit,
compiled into the executable). A reflected module must be a `Runtime`, `Developer` or `Editor` module with `IMPLEMENT_MODULE`.

New headers are picked up through `CONFIGURE_DEPENDS` globs. When a header of a reflected module gains its first
`.generated.h` include, LeonHeaderTool stops once and the next build reconfigures. In a module with no reflected
header yet, touch its `.Build.cmake` after adding the first include.

The generated-code contract, the supported UE subset and the deviations are in
[Engine/Source/Programs/LeonHeaderTool/README.md](../Engine/Source/Programs/LeonHeaderTool/README.md).

## Projects and plugins

### `.lproj` (UE `.uproject`)

```json
{
	"FileVersion": 3,
	"EngineAssociation": "",
	"Description": "ShooterGame: an offline Counter-Strike-style shooter on de_leon (first-person movement, two teams, bots).",
	"Modules": [ { "Name": "ShooterGame", "Type": "Runtime", "LoadingPhase": "Default" } ],
	"Plugins": [],
	"TargetPlatforms": [ "Win64", "PS2" ]
}
```

LeonBuildTool reads `Modules[].Name` (the project's modules, default `EXTRA_MODULE_NAMES` of its game targets),
`Plugins[]` (`Name` plus `Enabled`; a missing `Enabled` counts as enabled) and `TargetPlatforms[]`. The project name is
the file name. A project folder contains `<Name>.lproj`, `Source/<Target>.Target.cmake`,
`Source/<Module>/<Module>.Build.cmake`, `Config/`, `Content/` and optionally `Plugins/`. The engine never references a
project; projects are only built with `-Project=`.

### `.lplugin` (UE `.uplugin`)

```json
{
	"FileVersion": 1,
	"Version": 1,
	"VersionName": "1.0",
	"FriendlyName": "Sample Plugin",
	"Description": "An example runtime plugin, built for Win64 only.",
	"Category": "Other",
	"EnabledByDefault": false,
	"Modules": [ { "Name": "SamplePlugin", "Type": "Runtime", "LoadingPhase": "Default", "PlatformAllowList": [ "Win64" ] } ]
}
```

Plugins are found under `Engine/Plugins/` and `<Project>/Plugins/` (any depth); their modules live in
`<Plugin>/Source/`. LeonBuildTool reads `EnabledByDefault`, `Modules[].Name` and `Modules[].PlatformAllowList`.
Enablement, applied in order: `EnabledByDefault` (always off for `Program` targets) → the project's `Plugins` list →
the target's `ENABLE_PLUGINS` / `DISABLE_PLUGINS`. The engine ships no plugin (`Engine/Plugins/` does not exist; the
JoltPhysics plugin, its only one, went in [ps2-shipping](PLANS/ps2-shipping.md) N3), so the example above is a
sketch; `System.Engine.Projects.RealDescriptors` saves one like it into the test program's `Plugins/` folder and
checks that it is discovered, disabled until a project enables it.

## Discovery

Order in the generator: plugins → `Engine/Source` → `Engine/Platforms/<Platform>/Source` (current platform only) →
each plugin's `Source` → `<Project>/Source`. Every `*.Build.cmake` under those roots is included (sorted), except
files under `Intermediate/`, `Binaries/`, `Saved/` and downloaded third-party trees (`ThirdParty/<Lib>/<lib>-<version>/`).
Then engine modules get their platform-extension folders merged, and the `*.Target.cmake` files are included.

## Platform registry

Platforms register themselves with `leon_register_platform()`. The registry loads
`Engine/Source/Programs/LeonBuildTool/Platform/*/LeonBuild*.cmake` and
`Engine/Platforms/*/Source/Programs/LeonBuildTool/LeonBuild*.cmake`, so a platform extension adds a platform without
touching the build tool.

| | Win64 | PS2 (extension) |
| --- | --- | --- |
| File | `Platform/Windows/LeonBuildWindows.cmake` | `Engine/Platforms/PS2/Source/Programs/LeonBuildTool/LeonBuildPS2.cmake` |
| Groups | `Windows Microsoft Desktop` | `PS2 Console` |
| Header folder (`LBT_COMPILED_PLATFORM`) | `Windows` | `PS2` |
| C++ standard | 17 | 17 |
| Executable suffix | `.exe` | `.elf` |
| RHI module | `OpenGLDrv` | `PS2RHI` |
| Definitions | `PLATFORM_WINDOWS=1 NOMINMAX WIN32_LEAN_AND_MEAN` | `PLATFORM_PS2=1` |
| Toolchain | host MSVC (via `GetVSEnv.bat`) | `PS2Toolchain.cmake` (ps2dev EE GCC) |
| Docker | — | `ghcr.io/ps2dev/ps2dev@sha256:79c24d37...` (pinned by digest), `SDK_ENV PS2DEV` |

Win64 (the development and editor platform, the only `Desktop` one) and PS2 (the target) are the two platforms;
`Package.bat` builds both.

## Compile environment

Every Leon module library and every executable gets these definitions (`CompileEnvironment.cmake`):

| Definition | Value |
| --- | --- |
| Platform definitions | from the registry (`PLATFORM_WINDOWS=1`, `PLATFORM_PS2=1`, ...). `HAL/Platform.h` defaults the others to 0 |
| `LBT_COMPILED_PLATFORM` | header folder name (`Windows`, `PS2`), used by `COMPILED_PLATFORM_HEADER()` |
| `PLATFORM_IS_EXTENSION` | 1 for PS2 (headers at the root of the extension's `Public/`), 0 otherwise |
| `IS_MONOLITHIC` | 1 (everything is statically linked) |
| `WITH_EDITOR` | 0 |
| `LEON_BUILD_<CONFIGURATION>` | `LEON_BUILD_DEBUG`, `LEON_BUILD_DEVELOPMENT` or `LEON_BUILD_SHIPPING` = 1; `Misc/Build.h` turns it into `UE_BUILD_*`, `DO_CHECK`, `DO_ENSURE`, `DO_GUARD_SLOW` and `NO_LOGGING` (Shipping) |
| `ENGINE_MAJOR_VERSION` / `MINOR` / `PATCH` | from `Engine/Build/Build.version` |
| `LEON_ENGINE_DIR` | absolute `Engine/` path (development fallback for `FPaths`) |
| `<MODULE>_API` | empty (`CORE_API`, `ENGINE_API`, ...), defined for the module and its dependents |
| `LEON_MODULE_NAME` | the module's name (module libraries) |

Target-level definitions reach **only the launch module** compiled into the executable, because module libraries are
shared by every target of a build tree:

| Definition | Value |
| --- | --- |
| `WITH_ENGINE` | 1 for `TYPE Game` (LeonBuildTool also links `Engine`, `Renderer` and `PakFile`), 0 for `TYPE Program` |
| `IS_PROGRAM` | 1 for `TYPE Program` |
| `WITH_DEV_AUTOMATION_TESTS` | 1 with `COLLECT_AUTOMATION_TESTS` (also seen by the collected test sources; `Misc/Build.h` defaults it to 0) |
| `LEON_TARGET_NAME`, `LEON_PROJECT_NAME`, `LEON_ROOT_DIR` | target name, project name (empty for engine targets), repo root |

Compiler settings:

| | Win64 (MSVC) | PS2 (EE GCC) |
| --- | --- | --- |
| Standard | C++17 | C++17 |
| Warnings | `/W4 /permissive- /Zc:__cplusplus /utf-8 /MP` | `-Wall -Wextra`, `-Werror=double-promotion` |
| Shadowing is an error (UE `ShadowVariableWarningLevel = Error`) | `/we4456 /we4457 /we4458 /we4459` | `-Werror=shadow` |
| No RTTI, no C++ exceptions (D17) | `/GR-`, no `/EH` flag, `_HAS_EXCEPTIONS=0`, `/wd4577` (CMake's `/EHsc` / `/GR` defaults are stripped from `CMAKE_CXX_FLAGS`; `leon_third_party_cxx_defaults(<target>)` gives them back to third-party C++ that needs them) | `-fno-rtti -fno-exceptions` (toolchain file, below) |
| Other | `/wd4324` (padding added for `alignas`, disabled as in UE); `/FS` in Debug and RelWithDebInfo | toolchain: `-D_EE -G0 -O2 -fno-exceptions -fno-rtti -fno-threadsafe-statics -ffunction-sections -fdata-sections`, linked with `$PS2SDK/ee/startup/linkfile` and `-Wl,--gc-sections` (unused functions / data are dropped; sizes in [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md)). Leon runs one EE thread, so function-local statics need no guard; together with `PS2PlatformRuntime.cpp` (global `operator new` / `delete` through `FMemory`, `__cxa_pure_virtual`) this keeps libstdc++'s unwinder and demangler out of the ELF |

**VU1 microcode** (PS2, [ps2-shipping](PLANS/ps2-shipping.md) N14). A module's `Private/VU1/*.vsm` (its own folder or
its platform extension's) is hand-written VU assembly in `dvp-as` syntax (one upper and one lower instruction a line;
the toolchain has no VCL). In a PS2 build `LeonPlatform_PS2_ModuleSources` (`LeonBuildPS2.cmake`) assembles each with
the ps2dev toolchain's `dvp-as` (`$PS2DEV/dvp/bin`) into `<tree>/VU1/<Module>/<File>.vu.o`, linked into the module:
the ELF carries the `.vutext` as data and the global labels are the EE's symbols of the code, which
`FPS2VU1::UploadPrograms` uploads with MPG. Today's programs are PS2RHI's `VU1Programs.vsm` (StaticUnlit, StaticLit)
and `Skinned.vsm` (SkinnedUnlit, SkinnedLit); `VU1Conformance` checks them.

The C++ standard of a module is the lowest standard among the platforms it is allowed on (unless the module sets
`CXX_STANDARD`); an executable uses its platform's. Every platform registers C++17, as UE 4.27 does, so all engine and
game code compiles as C++17. `CXX_EXTENSIONS` is off everywhere.

## ThirdParty modules

Third-party libraries are `External` modules under `Engine/Source/ThirdParty/<Lib>/` (or
`<Plugin>/Source/ThirdParty/<Lib>/`). They produce an `INTERFACE` target `ThirdParty.<Name>` that links the
`EXTERNAL_TARGETS`, adds `PUBLIC_INCLUDE_PATHS` as system includes, and forwards definitions, dependencies and system
libraries. Their own code is never compiled with the Leon warning flags.

If the module declares `function(LeonExternal_<Name>)`, LeonBuildTool calls it lazily — only when the module is part of
a target's closure — with `LEON_THIRDPARTY_DIR` (the downloaded folder, or the module folder when vendored) and
`LEON_MODULE_DIR` set. It usually calls `add_subdirectory(... EXCLUDE_FROM_ALL SYSTEM)` or `add_library()`.

Two ways to obtain the code:

- **Downloaded (pinned)** — `DOWNLOAD_URL` + `DOWNLOAD_SHA256` + `DOWNLOAD_DIR`. The archive is downloaded once to
  `Engine/Intermediate/ThirdPartyDownloads/<Name>-<archive>`, verified against the SHA-256 and extracted next to the
  rules file; `DOWNLOAD_DIR` must be the archive's top-level folder (`glfw-3.4`). `Setup.bat` fetches all of them
  up front (it scans `Engine/Source/ThirdParty`, `Engine/Platforms/*/Source/ThirdParty` and every plugin's
  `Source/ThirdParty`); a build also downloads a missing one while configuring. With an empty `DOWNLOAD_SHA256` the
  download is not verified and LeonBuildTool prints the hash to pin. Extracted folders are git-ignored.
- **Vendored** — the sources are committed next to the rules file (`Glad/glad/`, `CGLTF/cgltf/`, ...).

Example:

```cmake
# Engine/Source/ThirdParty/GLFW/GLFW.Build.cmake
leon_module(GLFW
	PLATFORMS Desktop
	DOWNLOAD_URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
	DOWNLOAD_SHA256 c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
	DOWNLOAD_DIR glfw-3.4
	EXTERNAL_TARGETS glfw
)

function(LeonExternal_GLFW)
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
	if(UNIX AND NOT APPLE)
		set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
	endif()
	add_subdirectory("${LEON_THIRDPARTY_DIR}" "${CMAKE_BINARY_DIR}/ThirdParty/GLFW" EXCLUDE_FROM_ALL SYSTEM)
endfunction()
```

The list of libraries, versions and licenses is in [LIBRARIES.md](LIBRARIES.md).

## PS2 builds in Docker

`PS2` registers `DOCKER_IMAGE`, `DOCKER_ENTRY` and `SDK_ENV PS2DEV`. When `PS2DEV` is not set on the host and
`-NoDocker` is not given, LeonBuildTool runs:

```
docker run --rm -v <repo root>:/leon -w /leon <image> sh /leon/Engine/Platforms/PS2/Build/BatchFiles/DockerEntry.sh \
    <Target> PS2 <Configuration> -Mode=<Mode> -NoDocker [-KeepGoing] [-Project=/leon/<relative path>]
```

- A project outside the repo root is mounted as `/project` and passed as `-Project=/project/<file>`.
- On Unix hosts the container runs as root (`DockerEntry.sh` may install CMake, Ninja and g++ with `apk`) and hands
  the build outputs back to the calling user (`LEON_HOST_UID` / `LEON_HOST_GID`, `chown` when the build ends).
- `DockerEntry.sh` exports `PS2DEV` (default `/usr/local/ps2dev`), `PS2SDK` and `PATH`. When they are missing it
  installs `cmake`, `ninja`, `make`, and `g++` / `musl-dev` (the host compiler for LeonHeaderTool, built into
  `Engine/Intermediate/Build/HostTools/LinuxMusl`) with `apk`. Then it re-runs `LeonBuildTool.cmake` inside the
  container.
- Inside, the generator uses `PS2Toolchain.cmake`, which requires `PS2DEV` and `PS2SDK` and uses the
  `mips64r5900el-ps2-elf-` compilers.

Outputs land in the mounted repo, so `Game/ShooterGame/Binaries/PS2/ShooterGame.elf` appears on the host.
`Engine/Platforms/PS2/Build/Docker/Dockerfile` is the image LeonBuildTool derives from the pinned one and tags
`leon/ps2-build:<hash of the Dockerfile and the pinned image>` (the pinned image plus CMake, Ninja, g++ and, since
[ps2-shipping](PLANS/ps2-shipping.md) N23, xorriso for `BuildCookRun -iso`, which derives it the same way); by hand:

```
docker build -t leon/ps2dev Engine/Platforms/PS2/Build/Docker
```

To use it, point `DOCKER_IMAGE` in `LeonBuildPS2.cmake` at `leon/ps2dev`. With a local ps2dev install instead, export
`PS2DEV` / `PS2SDK` and LeonBuildTool builds on the host. More on the platform: [Engine/Platforms/PS2/README.md](../Engine/Platforms/PS2/README.md).

## Staging and Shipping

**Shipping.** `Build.bat <Target> Win64 Shipping` compiles with `LEON_BUILD_SHIPPING=1` (CMake `Release`): `Misc/Build.h`
turns it into `UE_BUILD_SHIPPING`, no `check` / `ensure` (`DO_CHECK`, `DO_ENSURE` 0), no `UE_LOG` (`NO_LOGGING`: the
console shows only the `RequestEngineExit:` lines) and no editor-only data (`WITH_EDITORONLY_DATA` 0, D14), and names
the executable `<Target>-Win64-Shipping.exe`. The Shipping build tree (`Engine/Intermediate/Build/Win64/Shipping`) is
separate from Development's. A Shipping game reads only cooked content from its pak: `FEngineLoop::PreInit` always
puts the pak platform file on the chain in Shipping, it refuses loose files (except under the `Saved` folders, where
the logs, the user config layer and screenshots go), and without a `.lpak` in `<Project>/Content/Paks/` the game stops
at once (`RequestEngineExit: No pak file: ...`, exit code 1). The capture switches (`-Screenshot=`, `-ExitAfterFrames=`,
`-nullrhi`) work in Shipping.

**Staging** (`BuildCookRun.bat`, UE's `BuildCookRun`; [TOOLS.md](TOOLS.md#buildcookrun)): `-build -cook -stage -pak`
builds the game (Shipping by default), cooks the project for Win64, and makes UE's staged layout in
`<Project>/Saved/StagedBuilds/Win64/`:

```text
<Project>/Binaries/Win64/<Project>-Win64-Shipping.exe   the game (a content-only project's is LeonGame, renamed)
<Project>/Content/Paks/<Project>-Win64.lpak             every cooked file, under the mount point ../../../
```

A staged executable finds its folders itself (`FPaths::IsStaged`): when `<Project>/Content/Paks/` sits two levels
above it and the engine folder its build tree recorded (`GLeonEngineDirFromBaseDir`) has no `Source/`, the engine is
`<Stage>/Engine/` (UE's `../../../Engine/`) and the project `<Stage>/<Project>/`; without `-project=` or a project of
its own, the game takes the project's name from that folder and reads its `.lproj` from the pak. The staged folder
holds nothing else: the engine's content, config and shaders and the project's are all in the pak. `-run` starts it
(`-addcmdline="-Screenshot=<file.bmp> -ExitAfterFrames=30"` for a capture).

Checks: the staged Shipping build of the engine's content as a content-only project (`Engine/Saved/StagingTest`,
GameDefaultMap `/Engine/Maps/Template_Default`) renders the same frame as the Development build, byte for byte; two runs
of `-cook -stage -pak` give the same `.lpak` (and, since [ps2-shipping](PLANS/ps2-shipping.md) N23, a full and an
incremental cook the same cooked files); the Development staged build runs headless (`-configuration=Development
-build -cook -stage -pak -run "-addcmdline=-nullrhi -ExitAfterFrames=60"`, exit code 0), and since P21 the staged
Shipping ShooterGame plays three rounds of a bot match (`-configuration=Shipping -build -cook -stage -pak -run
"-addcmdline=-nullrhi -benchmark -botmatch -rounds=3 -seed=7"`), which passes on the game's exit code (Shipping has no
log). `Package.bat` builds and stages the Shipping ShooterGame without running it.

**Cook, pak order and disc** ([ps2-shipping](PLANS/ps2-shipping.md) N23, [TOOLS.md](TOOLS.md#the-cook)). The cook is
incremental by default: `<Project>/Intermediate/CookCache/<Platform>/` keeps each cooked package under a key of its
sources, its imports', the cooker's version and the platform's settings, and a package whose key has not changed is
copied instead of cooked (`-full` / BuildCookRun `-fullcook` cook it all); the output is the same bytes either way. The
cook's hard budgets (`[/Script/LeonEd.CookSettings]`) fail it. A run of the game with `-LogFileOpenOrder` records the
order it opens its files in (`<Project>/Saved/Logs/FileOpenOrder-<Platform>.txt`; on the PS2 the EE log's
`LogFileOpenOrder:` lines), and `-pakorder=<that file>` lays the pak out in it. `-platform=PS2 -iso` makes the bootable
disc image `<Project>/Saved/StagedBuilds/PS2/<Project>.iso` with xorriso in the PS2 build image
([TOOLS.md](TOOLS.md#ps2-disc-image)).

## Adding things

**A new engine module**: create `Engine/Source/Runtime/<Name>/<Name>.Build.cmake` with `leon_module(<Name> ...)`,
`Public/` and `Private/`, and `IMPLEMENT_MODULE(FDefaultModuleImpl, <Name>)` in a `Private/*.cpp`. Add it as a
dependency of the module that uses it (or to a target's `EXTRA_MODULE_NAMES`).

**Tests**: put test files in `<Module>/Private/Tests/` — automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`;
[CODING_STANDARD.md §10](CODING_STANDARD.md#10-tests)). They are built into `LeonAutomationTests` if the module is in
its closure (add it to `EXTRA_MODULE_NAMES` in `LeonAutomationTests.Target.cmake` otherwise); `TestPAL` links Core,
CoreUObject, Projects (→ Json) and PakFile, so it runs only their tests.

**A new game project**: create `<Dir>/<Name>.lproj`, `<Dir>/Source/<Name>.Target.cmake` with
`leon_target(<Name> TYPE Game ...)`, and a module in `<Dir>/Source/<Name>/` using `IMPLEMENT_PRIMARY_GAME_MODULE`. Build
with `Build.bat <Name> <Platform> <Config> -Project=<Dir>\<Name>.lproj`. `Game/ShooterGame` is the reference.

**A new platform**: add `Engine/Platforms/<P>/Source/Programs/LeonBuildTool/LeonBuild<P>.cmake` calling
`leon_register_platform(<P> IS_EXTENSION GROUPS ... HEADER_NAME <P> CXX_STANDARD ... EXECUTABLE_SUFFIX ... RHI_MODULE ...)`
plus a toolchain file, then module extensions under `Engine/Platforms/<P>/Source/`.

See also: [SETUP.md](SETUP.md) · [ARCHITECTURE.md](ARCHITECTURE.md) · [CODING_STANDARD.md](CODING_STANDARD.md) ·
[UnrealEngine427/](UnrealEngine427/README.md)
