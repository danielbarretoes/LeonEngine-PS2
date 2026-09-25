# Leon Engine

A C++ game engine that follows the **Unreal Engine 4.27** source layout, module architecture and Epic naming
conventions, built with CMake through **LeonBuildTool** (our UnrealBuildTool).

- **Win64 host runtime**: the engine modules (`Core`, `Engine`, `Renderer` on OpenGL 3.3, `UMG`, `AIModule`, ...),
  the `LeonGame` game executable, the `LeonCook` command-line editor (import, reimport, cook commandlets), the `LeonPak`
  pak tool, `BuildCookRun.bat` (a Shipping build staged with its content in one `.lpak`) and the `LeonAutomationTests`
  test runner. The world uses
  UE's space: X forward, Y right, Z up, left-handed, 1 unit = 1 cm.
- **PS2 platform extension** (`Engine/Platforms/PS2`): PlayStation 2 HAL, DualShock input, engine loop hooks with a
  debug overlay, and `PS2RHI` for the Graphics Synthesizer. PS2 builds run in a pinned ps2dev Docker image.
- **One game**, `Game/ThirdPerson`: a PS2 third-person starter (orbit camera, character move / jump, primitive level),
  isolated from the engine and built with `-Project=`.

## Quick start (Windows)

Requirements: Visual Studio 2026 or 2022 with C++, CMake tools and Clang tools; CMake 3.24+; Ninja; Docker Desktop
for PS2; PCSX2 to run the PS2 build. Details: [Docs/SETUP.md](Docs/SETUP.md).

```bat
:: 1. Download the pinned third-party libraries
Setup.bat

:: 2. Build and run the automation tests (LeonAutomationTests, Win64)
Engine\Build\BatchFiles\RunTests.bat

:: 3. Build the engine game executable and the cooker -> Engine\Binaries\Win64\
Engine\Build\BatchFiles\Build.bat LeonGame Win64 Development
Engine\Build\BatchFiles\Build.bat LeonCook Win64 Development

:: 4. Build the PS2 game (in Docker) -> Game\ThirdPerson\Binaries\PS2\ThirdPerson.elf
Engine\Build\BatchFiles\Build.bat ThirdPerson PS2 Development -Project=%CD%\Game\ThirdPerson\ThirdPerson.lproj
```

Run it in PCSX2 (add `-Build` to build first):

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Project Game\ThirdPerson
```

In the game: left stick moves, right stick orbits the camera, Cross jumps, Start quits, Select cycles the debug overlay.

IDE / clangd: `GenerateProjectFiles.bat` writes a Visual Studio solution to `Engine\Intermediate\ProjectFiles\` and the
root `compile_commands.json`.

## Layout

```
Setup.bat / .sh, GenerateProjectFiles.bat / .sh
Engine/
  Build/                 Build.version, BatchFiles/ (Build, Clean, Rebuild, RunTests, Cook, BuildCookRun, Lint, ...)
  Config/                Base*.ini
  Content/, Shaders/     engine content and GLSL shaders
  Source/
    Runtime/             Core, ApplicationCore, InputCore, RHI, OpenGLDrv, RenderCore, Renderer, Engine, Launch, ...
    Developer/           MeshUtilities, TargetPlatform
    Editor/              LeonEd (factories, commandlets)
    Programs/            LeonBuildTool, LeonAutomationTests, LeonCook, LeonPak, TestPAL, BlankProgram
    ThirdParty/          GLFW, Glad, STB, ... (one External module per library)
    LeonGame.Target.cmake
  Plugins/Runtime/JoltPhysics/   Jolt rigid-body backend (Win64, disabled by default)
  Platforms/PS2/         PS2 platform extension
  Binaries/, Intermediate/       generated
Game/ThirdPerson/        ThirdPerson.lproj, Source/, Config/, Content/ (PS2 game)
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
| [Engine/Platforms/PS2/README.md](Engine/Platforms/PS2/README.md) | PS2 platform extension, frame order, debug overlay |
| [Docs/ASSET_FORMATS.md](Docs/ASSET_FORMATS.md), [Docs/LEVELS.md](Docs/LEVELS.md), [Docs/TOOLS.md](Docs/TOOLS.md) | Asset formats, maps (`.lmap`, the glTF map import), cook tools |
| [Docs/TESTING.md](Docs/TESTING.md) | Automated gates, frame captures, the axes gizmo and the manual checklist |
| [Docs/PS2OFFICIAL/](Docs/PS2OFFICIAL/README.md) | PS2 hardware manuals |

## Changelog

[CHANGELOG.md](CHANGELOG.md)
