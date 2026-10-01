# Leon Engine

A C++ game engine that follows the **Unreal Engine 4.27** source layout, module architecture and Epic naming
conventions, built with CMake through **LeonBuildTool** (our UnrealBuildTool). Version 0.26.0
([CHANGELOG.md](CHANGELOG.md)).

- **Win64, the development and editor platform**: the engine modules (`Core`, `CoreUObject`, `Engine`, `Renderer`:
  the PS2 GS's frame, emulated on OpenGL 3.3, `AnimationCore`, `AudioMixer`, `PhysicsCore`, `UMG`, `AIModule`, ...),
  the `LeonGame` game executable, the `LeonCook` command-line editor (import, reimport, cook commandlets), the
  `LeonPak` pak tool, `BuildCookRun.bat` (a build staged with its content in one `.lpak`) and the
  `LeonAutomationTests` test runner. The world uses UE's space: X forward, Y right, Z up, left-handed, 1 unit = 1 cm.
- **PS2, the target platform** (the platform extension `Engine/Platforms/PS2`,
  [README](Engine/Platforms/PS2/README.md)): the HAL, `PS2RHI` with double-buffered DMA chains to VIF1 and VU1
  microprograms for static and skinned meshes, VU0 macro-mode math and the scratchpad, the vertical blank by
  interrupt, SPU2 ADPCM audio, asynchronous disc IO, memory card saves, the DualShock 2 (two ports, pressure,
  vibration) and a bootable ISO. PS2 builds run in a pinned ps2dev Docker image and are measured in PCSX2 (an
  emulator, not the hardware).
- **One game**, isolated from the engine and built with `-Project=`: `Game/ShooterGame`, a Counter-Strike 1.6 clone
  for Win64 and the PS2 (the same code on the EE, played with the DualShock at 30 fps in PCSX2): CS movement, CS
  1.6's weapons, economy, grenades, rounds and the bomb, two teams of five with bots, the radio, animated CT and T
  characters and first-person arms, and `de_leon`, a desert town; the art is made by Blender scripts and the sounds
  by a Python script, all CC0 ([README](Game/ShooterGame/README.md)).

## Quick start (Windows)

Requirements: Visual Studio 2026 or 2022 with C++, CMake tools and Clang tools; CMake 3.24+; Ninja; Docker Desktop
for PS2; PCSX2 to run the PS2 build. Details: [Docs/SETUP.md](Docs/SETUP.md).

The shortest path: double-click **`Package.bat`** at the root. It downloads the pinned libraries, builds and packages
ShooterGame for Win64 (Shipping, with its pak) into `Game\ShooterGame\Packages\Win64\`, and the PS2 artifacts into each
owner's `Packages\PS2\` (`Game\ShooterGame\`, `Engine\Packages\PS2\TestPAL\`, `GSConformance\` and `VU1Conformance\`;
`Package.bat -NoPS2` / `-NoWin64` skip a platform). Step by step:

```bat
:: 1. Download the pinned third-party libraries
Setup.bat

:: 2. Build and run the automation tests on Win64: LeonAutomationTests (575), the LeonHeaderTool golden tests (35),
::    ShooterGameTests (99) and TestPAL (171)
Engine\Build\BatchFiles\RunTests.bat

:: 3. Build the engine game executable and the cooker -> Engine\Binaries\Win64\
Engine\Build\BatchFiles\Build.bat LeonGame Win64 Development
Engine\Build\BatchFiles\Build.bat LeonCook Win64 Development

:: 4. Build (in Docker), cook, stage and pak the PS2 game and start it in PCSX2
::    -> Game\ShooterGame\Saved\StagedBuilds\PS2\ShooterGame.elf, its pak beside it
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -pak -run

:: 5. The same as a bootable disc (SYSTEM.CNF, the ELF, the pak)
::    -> Game\ShooterGame\Saved\StagedBuilds\PS2\ShooterGame.iso
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -pak -iso

:: 6. Every local gate (lint, tests, reimport, smoke, bot match, asset validation; -PS2 adds the PS2 packages,
::    -Measure the PCSX2 frame measure)
Engine\Build\BatchFiles\RunGates.bat -PS2
```

ShooterGame (Win64): build it, play `de_leon` with both teams filled by bots, and run its smoke test (gate G6) and a
headless bot match (ten rounds, seed 7, the rules' invariants checked, played twice; it logs `Botmatch OK: 9 round(s),
CT 3 - T 6, 55 kill(s), seed 7, sides switched after round 5`):

```bat
Engine\Build\BatchFiles\Build.bat ShooterGame Win64 Development -Project=%CD%\Game\ShooterGame\ShooterGame.lproj
Game\ShooterGame\Binaries\Win64\ShooterGame.exe
Engine\Build\BatchFiles\SmokeTest.bat
Engine\Build\BatchFiles\BotMatch.bat 10 7
```

Measure the PS2 frame in PCSX2, unattended (a bot match watched through a bot's eyes; `-Iso` boots the disc):
`Engine\Build\BatchFiles\MeasurePS2.bat [-Iso]`. At 0.26.0 (as at 0.25.0) ShooterGame on de_leon runs every frame after the
first at 33.5 ms (p50 / p95 / p99; 29.62 fps with the travel from the main menu); at 0.24.0 its first frame from the
disc came 2.93 s after the engine starts
([Budgets.md](Engine/Platforms/PS2/Documentation/Budgets.md)).

Run the staged PS2 game in PCSX2 again (it reads its content from its pak, so the staged build is the one to boot):

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -StagedElf Game\ShooterGame\Saved\StagedBuilds\PS2\ShooterGame.elf
```

On the DualShock: left stick moves, right stick looks, Cross jumps, R2 fires, Start opens the buy menu, R3 toggles
`stat unit` ([controls](Game/ShooterGame/README.md#controls)).

IDE / clangd: `GenerateProjectFiles.bat` writes a Visual Studio solution to `Engine\Intermediate\ProjectFiles\` and the
root `compile_commands.json`.

## Layout

```
Setup.bat, GenerateProjectFiles.bat, Package.bat
Engine/
  Build/                 Build.version, BatchFiles/ (Build, RunTests, RunGates, Cook, BuildCookRun, BotMatch,
                         MeasurePS2, Lint, ...)
  Config/                Base*.ini
  Content/, Shaders/     engine content and GLSL shaders
  Source/
    Runtime/             Core, CoreUObject, ApplicationCore, InputCore, RHI, OpenGLDrv, GSCore, RenderCore, Renderer,
                         Engine, AnimationCore, AudioMixer, PhysicsCore, AIModule, UMG, PakFile, Launch, ...
    Developer/           MeshUtilities, TextureCompressor, AudioCompressor, GSReference, TargetPlatform
    Editor/              LeonEd (factories, commandlets, static lighting)
    Programs/            LeonBuildTool, LeonHeaderTool, LeonAutomationTests, LeonCook, LeonPak, TestPAL, BlankProgram
    ThirdParty/          CGLTF, GLFW, Glad, MeshOptimizer, MiniAudio, STB (one External module per library)
    LeonGame.Target.cmake
  Platforms/PS2/         PS2 platform extension (its README)
  Binaries/, Intermediate/       generated
Game/ShooterGame/        ShooterGame.lproj, Source/ (game + tests targets), Config/, Content/, SourceArt/ (Win64, PS2)
Docs/
```

Each module is `<Module>/<Module>.Build.cmake` + `Public/` + `Private/`; its tests live in `<Module>/Private/Tests/`
and run in `LeonAutomationTests`.

## Documentation

| Page | Content |
| --- | --- |
| [Docs/SETUP.md](Docs/SETUP.md) | Prerequisites, building, tests, PS2 and PCSX2, clangd, formatting |
| [Docs/BUILD.md](Docs/BUILD.md) | LeonBuildTool: command line, batch files, module / target rules, projects, plugins, platforms |
| [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md) | Engine architecture |
| [Docs/CODING_STANDARD.md](Docs/CODING_STANDARD.md) | Coding standard (Epic's, with Leon deviations) |
| [Docs/LIBRARIES.md](Docs/LIBRARIES.md) | Third-party libraries |
| [Docs/UnrealEngine427/](Docs/UnrealEngine427/README.md) | UE 4.27 knowledge base and the Leon ↔ UE mapping |
| [Engine/Platforms/PS2/README.md](Engine/Platforms/PS2/README.md) | PS2 platform extension: HAL, VU1 / VU0, DMA, SPU2, disc IO, memory card, pad, ISO, measuring |
| [Engine/Platforms/PS2/Documentation/Budgets.md](Engine/Platforms/PS2/Documentation/Budgets.md) | PS2 ELF size, memory and frame measures per phase |
| [Docs/PLANS/ps2-shipping.md](Docs/PLANS/ps2-shipping.md) | The plan behind 0.22.0 to 0.24.0 (Spanish): each phase's state and deviations |
| [Docs/PLANS/ps2-polish.md](Docs/PLANS/ps2-polish.md) | The plan behind 0.25.0 (Spanish); what is left is in [Docs/PENDING.md](Docs/PENDING.md) |
| [Docs/ASSET_FORMATS.md](Docs/ASSET_FORMATS.md), [Docs/LEVELS.md](Docs/LEVELS.md), [Docs/TOOLS.md](Docs/TOOLS.md) | Asset formats, maps (`.lmap`, the glTF map import), cook tools |
| [Docs/ART_PIPELINE.md](Docs/ART_PIPELINE.md) | Art: budgets, naming, the shared skeletons, the animation list, the Blender scripts and their glTF export |
| [Docs/TESTING.md](Docs/TESTING.md) | Automated gates, frame captures, the axes gizmo and the manual checklist |
| [Docs/PENDING.md](Docs/PENDING.md) | What the ps2-shipping plan left open, and the checks only a person can do |
| [Game/ShooterGame/README.md](Game/ShooterGame/README.md) | ShooterGame: build, run, controls, classes, CS movement values, weapons, rounds, bots, the bot match, de_leon |
| [Docs/PS2OFFICIAL/](Docs/PS2OFFICIAL/README.md) | PS2 hardware manuals |

## Changelog

[CHANGELOG.md](CHANGELOG.md)
