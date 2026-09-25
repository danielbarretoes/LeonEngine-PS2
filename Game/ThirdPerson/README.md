# ThirdPerson (PS2)

PlayStation 2 third-person demo, modelled on Unreal's TP_ThirdPerson template: a character you move relative to the camera, jump, and an orbit camera on a boom that is pulled in when props block it. The level is a large ground plane (18 × 18 tiles over a 140 × 140 arena) with 17 box props: crates, raised platforms, a three-step stair and walls. Everything is built in code and drawn with the PS2 Graphics Synthesizer through `PS2RHI`; the demo loads no files.

Collision is box-based: the character stands on the highest prop top under its footprint (step-up and landing) and is pushed out of props it cannot step onto, both in each prop's yawed frame. The sun direction rotates slowly over time.

## Controls

| Input | Action |
| --- | --- |
| Left stick | Move (camera-relative) |
| Right stick | Orbit the camera (yaw / pitch) |
| Cross | Jump |
| Start | Quit (requests engine exit) |
| Select | Cycle the engine debug overlay: stats + pad → stats → pad → none |

The overlay belongs to the engine (`FStatsOverlay`, drawn on PS2 by Launch's `PS2StatsOverlay`). The game adds two lines to it, `BOXES <drawn>/<total>` and `TRIS <emitted> CLIP <clipped>`, and prints the Draw3D counters to the PCSX2 EE console every 30 frames.

## Build and run

From the repository root (Windows):

```bat
Engine\Build\BatchFiles\Build.bat ThirdPerson PS2 Development -Project=%CD%\Game\ThirdPerson\ThirdPerson.lproj
```

The PS2 build runs inside the pinned ps2dev Docker image (Docker must be running) unless `PS2DEV` is set on the host. Output: `Game/ThirdPerson/Binaries/PS2/ThirdPerson.elf` (Debug and Shipping add `-PS2-<Configuration>` to the name). On Linux use `Engine/Build/BatchFiles/Linux/Build.sh` with the same arguments.

Run in PCSX2 (optionally building first):

```powershell
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Project Game\ThirdPerson -Build
```

The script finds PCSX2 through `$env:LEON_PCSX2`, `pcsx2-qt.exe` on `PATH`, or the default install folders, and boots the ELF with `-fastboot -elf`. Bind a controller to PCSX2's Pad 1 to play. See [Docs/TOOLS.md](../../Docs/TOOLS.md) and [Docs/SETUP.md](../../Docs/SETUP.md).

## Layout

```text
Game/ThirdPerson/
├── ThirdPerson.lproj          project descriptor (module ThirdPerson, TargetPlatforms PS2, no plugins)
├── Config/                          DefaultEngine.ini, DefaultGame.ini (character tuning), DefaultInput.ini
├── Content/                         Levels/, Materials/, Textures/ (README only; content is built in code)
└── Source/
    ├── ThirdPerson.Target.cmake     game target
    └── ThirdPerson/                 game module
        ├── ThirdPerson.Build.cmake
        ├── ThirdPerson.h/.cpp               FThirdPersonModule
        ├── ThirdPersonGameMode.h/.cpp       FThirdPersonGameMode
        ├── ThirdPersonCharacter.h/.cpp      FThirdPersonCharacter
        ├── ThirdPersonCameraBoom.h/.cpp     FThirdPersonCameraBoom
        └── ThirdPersonLevel.h/.cpp          FThirdPersonLevel, FThirdPersonPrimitive
```

`Binaries/` and `Intermediate/` are build outputs.

## Target and launch

`Source/ThirdPerson.Target.cmake`:

```cmake
leon_target(ThirdPerson TYPE Game
	PLATFORMS PS2
	COMPILE_AGAINST_ENGINE OFF
)
```

The gameplay framework (`Engine` module) is desktop-only, so the target compiles its launch module with `WITH_ENGINE=0`. The executable's `main` comes from the engine's `Launch` module (the default launch module of a Game target): on PS2, `LaunchPS2.cpp` calls `GuardedMain`, which runs `FEngineLoop`:

1. `FEngineLoop::PreInit` creates the platform application, the main window and the RHI on it (`RHIInit`), then starts the statically linked modules: CoreUObject (the object array of 8 192 slots and the transient package, logged as `LogUObjectBase: Object system started`), InputCore (`EKeys`, the reflected `FKey`) and ThirdPerson.
2. `FEngineLoop::Tick` polls the pad, ticks `FTicker::GetCoreTicker()`, lets the platform hooks draw the stats overlay, and presents.
3. When `RequestEngineExit` is called (Start pressed), `FEngineLoop::Exit` shuts the modules down.

LeonBuildTool generates the target's module table and marks ThirdPerson as the primary game module (the first project module of the Game target).

## Module

`ThirdPerson.Build.cmake`: public dependencies `Core`, `InputCore`, `ApplicationCore`; private dependencies `Launch` (for `GEngineLoop`'s main window and application) and `PS2RHI` (GS drawing). InputCore depends on CoreUObject (`FKey` is a `USTRUCT`), so the game boots the object system although it has no UObject of its own yet; `Engine/Platforms/PS2/Documentation/Budgets.md` has its cost.

| Class | Role |
| --- | --- |
| `FThirdPersonModule` | Primary game module (`IMPLEMENT_PRIMARY_GAME_MODULE(FThirdPersonModule, ThirdPerson, "ThirdPerson")`). `StartupModule` gets the main window and input interface from `GEngineLoop`, creates the game mode, calls `StartPlay` and registers a `FTicker` delegate that calls `FThirdPersonGameMode::Tick`; `ShutdownModule` removes it |
| `FThirdPersonGameMode` | Creates the procedural textures and `FPS2Material`s, builds the level, spawns the character, places the camera and sets the lights. `Tick` reads the pad, moves the character and camera, draws the frame, and returns `false` after Start requests exit |
| `FThirdPersonCharacter` | Camera-relative movement, jump, gravity, step-up and wall push-out (reduced CharacterMovementComponent); drawn as two boxes |
| `FThirdPersonCameraBoom` | Orbit camera: spherical boom around a smoothed look-at point, shortened by a probe against the props (SpringArm + FollowCamera); publishes the PS2 view target. Angles are in 1/256 turn |
| `FThirdPersonLevel` | Ground tiles and props (`FThirdPersonPrimitive` boxes) plus the queries the character and boom need: `FindSupportY`, `ResolveWallCollisions`, `ProbeBoomLength` |

## Config

`Config/DefaultEngine.ini`, `DefaultGame.ini` and `DefaultInput.ini` follow Unreal's layout and are loaded by `GConfig` at startup, on top of `Engine/Config/Base*.ini` and `Engine/Platforms/PS2/Config/PS2Engine.ini`. `FThirdPersonCharacter::LoadConfig` reads `MoveSpeed`, `Gravity` and `JumpSpeed` from `[/Script/ThirdPerson.ThirdPersonCharacter]` in `DefaultGame.ini` and logs where they came from (`Character tuning from DefaultGame.ini` or `... compiled defaults`). `DefaultInput.ini` only documents the pad bindings that `FThirdPersonGameMode::Tick` hard-codes (input from config arrives in P13).

On the PS2 the files are read through PCSX2's `host:` device, which is the ELF's folder. `RunPCSX2.ps1` stages the ini files and `ThirdPerson.lproj` there before launching (`-NoStage` skips it). PCSX2 only opens them with **Settings > Advanced > Enable Host Filesystem** (`[EmuCore] HostFs = true` in `PCSX2.ini`); without it the game runs with the compiled defaults, which are the same values.

## Isolation

The engine never references the game: no engine module, plugin or build rule depends on or includes anything under `Game/ThirdPerson`. The game depends on engine modules through its `.Build.cmake`, and is only pulled into a build through its `.lproj` (`-Project=`). Keep game code, content and config inside `Game/ThirdPerson`.
