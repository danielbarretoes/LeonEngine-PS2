# PS2 platform extension

PlayStation 2 (Emotion Engine + Graphics Synthesizer) support, organised as a UE 4.27 **platform extension**
(`Engine/Platforms/<Platform>/`). All PS2 implementation code lives here (shared engine code only sees generic
macros such as `PLATFORM_PS2`, defaulted to 0 in `HAL/Platform.h`): this folder registers the platform with
LeonBuildTool, merges PS2 code into the engine modules of the same name, and adds the `PS2RHI` module. Build-system
details: [Docs/BUILD.md](../../../Docs/BUILD.md).

## Layout

```
Engine/Platforms/PS2/
  Build/
    BatchFiles/
      RunPCSX2.ps1            launch a project's ELF in PCSX2 (optionally build it first)
      DockerEntry.sh          entry point inside the ps2dev container (re-runs LeonBuildTool)
    Docker/Dockerfile         optional local image: pinned ps2dev + CMake + Ninja
  Config/PS2Engine.ini        platform config (placeholder, not loaded yet)
  Documentation/CookNotes.md
  Source/
    Programs/LeonBuildTool/
      LeonBuildPS2.cmake      leon_register_platform(PS2 ...)
      PS2Toolchain.cmake      CMake toolchain for the EE GCC (mips64r5900el-ps2-elf-)
    Runtime/
      Core/                   extension of Core             (Core_PS2.Build.cmake)
      ApplicationCore/        extension of ApplicationCore  (ApplicationCore_PS2.Build.cmake)
      Launch/                 extension of Launch           (Launch_PS2.Build.cmake)
      PS2RHI/                 PS2-only module               (PS2RHI.Build.cmake)
```

## Platform registration

`Source/Programs/LeonBuildTool/LeonBuildPS2.cmake` registers `PS2` with LeonBuildTool:

| Setting | Value |
| --- | --- |
| Groups (folder names and keyword suffixes that apply) | `PS2`, `Console` |
| Header folder / `LBT_COMPILED_PLATFORM` | `PS2` (`PLATFORM_IS_EXTENSION=1`: headers sit at the root of `Public/`, e.g. `PS2PlatformMemory.h`) |
| Definitions | `PLATFORM_PS2=1` |
| C++ standard | 17 |
| Executable suffix | `.elf` |
| RHI module | `PS2RHI` |
| Build types | Debug → `Debug`; Development and Shipping → `Release` (`-O2`, no debug info) |
| Docker image | `ghcr.io/ps2dev/ps2dev@sha256:79c24d3762f5cfeee0beabeacc94480fe580d379d3e48a0bfbac681a8895daf6` |
| SDK variable | `PS2DEV` — when it is not set on the host, the build re-runs inside the image |

`PS2Toolchain.cmake` requires `PS2DEV` and `PS2SDK`, uses `$PS2DEV/ee/bin/mips64r5900el-ps2-elf-{gcc,g++}`, compiles
with `-D_EE -G0 -O2 -Wall` (C++: `-fno-exceptions -fno-rtti`) and links with `$PS2SDK/ee/startup/linkfile`. The
engine adds `-Wall -Wextra -Werror=shadow` to every Leon module on PS2.

## Module extensions

Each folder under `Source/Runtime/` with the same relative path as an engine module is merged into that module when
building for PS2 (its `Public/` and `Private/` join the module's). Its `<Module>_PS2.Build.cmake` calls
`leon_module_extend()` to add PS2-only dependencies and PS2SDK libraries.

### Core — `Source/Runtime/Core/`

Adds `kernel` (EE timer). Implements the HAL types that `HAL/Platform*.h` select through
`COMPILED_PLATFORM_HEADER()`:

| Type | Header | What it does |
| --- | --- | --- |
| `FPS2PlatformTypes` → `FPlatformTypes` | `PS2Platform.h` | EE is ILP32: 32-bit `SIZE_T`, `PTRINT`, `UPTRINT`; `PLATFORM_DESKTOP 0`, `PLATFORM_64BITS 0` |
| `FPS2PlatformProperties` → `FPlatformProperties` | `PS2PlatformProperties.h` | `PlatformName()` = `"PS2"`, `IsGameOnly()` = true |
| `FPS2PlatformMemory` → `FPlatformMemory` | `PS2PlatformMemory.h` | `GetStats()`: program image + heap (newlib break above `0x00100000`) of 32 MB EE RAM |
| `FPS2PlatformTime` → `FPlatformTime` | `PS2PlatformTime.h` | `Cycles64()` from `GetTimerSystemTime()` (BUSCLK, 147.456 MHz), `Seconds()`, `CyclesToMicroseconds()` |
| `FPS2PlatformMath` → `FPlatformMath` | `PS2PlatformMath.h` | `Sin256()` / `Cos256()`: quarter-wave table on a 1/256-turn angle, no libm (soft-float `double` is slow on the EE) |

`Core.Build.cmake` excludes `Private/Misc/FileHelper.cpp`, `Private/Misc/Paths.cpp` and `Private/Math/Transform.cpp` on
PS2 (`EXCLUDE_SOURCES_PS2`), and GLM is a Desktop-only dependency.

### ApplicationCore — `Source/Runtime/ApplicationCore/`

Adds `PS2RHI` (private) and `pad`.

| Type | File | What it does |
| --- | --- | --- |
| `FPS2PlatformApplicationMisc` → `FPlatformApplicationMisc` | `Public/PS2PlatformApplicationMisc.h` | `CreateApplication()` returns an `FPS2Application` |
| `FPS2Application` | `Private/PS2Application.cpp` | `GenericApplication`: `MakeWindow()` → `FPS2Window`; `PollGameDeviceState()` reads the pad; owns the `FPS2InputInterface` |
| `FPS2Window` | `Private/PS2Window.h/.cpp` | `FGenericWindow` over the GS display: `Create()` calls `FPS2RHI::InitDisplay()` (default 640x448) and creates the RHI; `PollEvents()` is empty (no OS message queue); `SwapBuffers()` waits for vsync |
| `FPS2InputInterface` | `Public/PS2InputInterface.h` | `IInputInterface` for the DualShock on port 0 (libpad; UE homologue: `XInputInterface`). Loads `rom0:SIO2MAN` and `rom0:PADMAN`, requests analog mode, applies a 0.18 stick dead zone. Also exposes raw state (`IsPortOpen()`, `GetRawButtonMask()`, `GetRawSticks()`) for the debug overlay. `FPS2InputInterface::Get()` returns the single instance |

### Launch — `Source/Runtime/Launch/`

Adds `PS2RHI` (private).

| File | What it does |
| --- | --- |
| `Private/LaunchPS2.cpp` | `main()` → `GuardedMain()` (UE: `Launch<Platform>.cpp`) |
| `Private/PS2EngineLoopHooks.cpp` | `FPlatformEngineLoopHooks::EndFrame()` draws the overlay; `PostPresent()` marks the frame start |
| `Private/PS2StatsOverlay.h/.cpp` | `FPS2StatsOverlay`: draws the `FStatsOverlay` state with the GS |

### PS2RHI — `Source/Runtime/PS2RHI/`

A PS2-only module (`PLATFORMS PS2`, depends on `Core` and `RHI`, links `draw math3d packet graph dma kernel`): the
GS backend (`FDynamicRHI` implementation returned by `PlatformCreateDynamicRHI()`) plus the static `FPS2RHI`
immediate-mode drawing API used by the game and the overlay. See [its README](Source/Runtime/PS2RHI/README.md).

## Frame order

The PS2 game target is built with `COMPILE_AGAINST_ENGINE OFF` (`WITH_ENGINE=0`): the desktop gameplay framework
(`Engine` module) does not run on PS2, and `FEngineLoop` (`Engine/Source/Runtime/Launch/Private/LaunchEngineLoop.cpp`)
owns the application and the main window itself.

Startup (`GuardedMain` → `FEngineLoop::PreInit`):

1. `FPlatformApplicationMisc::CreateApplication()` → `FPS2Application` (initializes the pad).
2. `MakeWindow()` + `Create(640, 448, LEON_TARGET_NAME)` → GS display, z-buffer, `FPS2DynamicRHI` in `GDynamicRHI`.
3. `FModuleManager::StartupStaticallyLinkedModules()` — the game module's `StartupModule()` can already use
   `GEngineLoop.GetMainWindow()` / `GetApplication()` and registers its tick with `FTicker::GetCoreTicker()`.

Each `FEngineLoop::Tick()`:

1. `Application->PollGameDeviceState()` — read the DualShock (`FPS2InputInterface::SendControllerEvents()`).
2. `MainWindow->PollEvents()` — no-op on PS2.
3. `FTicker::GetCoreTicker().Tick(DeltaTime)` — game work (the game mode clears, draws the scene, sets debug messages).
4. `FPlatformEngineLoopHooks::EndFrame()` — `FPS2StatsOverlay::Draw()`: handles Select and draws the overlay on top.
5. `MainWindow->SwapBuffers()` — `FPS2RHI::WaitVSync()`.
6. `FPlatformEngineLoopHooks::PostPresent()` — `FPS2StatsOverlay::MarkFrameStart()`: starts timing the next frame's
   work and resets the Draw3D counters (`FPS2RHI::BeginDraw3DStatsFrame()`).

The loop ends when `RequestEngineExit()` is called (the ThirdPerson game does it on Start) or the window reports
`ShouldClose()`; `FEngineLoop::Exit()` shuts the modules down and destroys the window.

## Debug overlay

Every PS2 game gets the engine debug overlay (UE: `stat fps` / `stat unit` + `AddOnScreenDebugMessage`, reduced).
State lives in `FStatsOverlay` (`Engine/Source/Runtime/Core/Public/Stats/StatsOverlay.h`); the PS2 Launch extension
draws it with the GS as two 50% translucent panels:

- **Stats** (top-left), refreshed every 0.25 s:
  - `FPS <n>  <ms> ms` — frames per second and the game work per frame (from `PostPresent` to `EndFrame`, before the
    overlay's own draws);
  - `RAM <used>/<total> MB` — `FPlatformMemory::GetStats()` (32 MB EE RAM);
  - `VRAM <used>/<total> MB` — `GDynamicRHI->GetGPUMemoryStats()` (GS allocations of 4 MB);
  - `RES <w>X<h>`;
  - up to `FStatsOverlay::MaxOnScreenMessages` (4) game lines set with
    `FStatsOverlay::AddOnScreenDebugMessage(Key, Text)` (ThirdPerson shows its box and triangle counters there).
- **Gamepad widget** (top-right): status LED (green = reading, orange = port open without data, red = port closed),
  buttons light while held, stick dots follow the raw axes (no dead zone).

**Select** (`EKeys::Gamepad_Special_Left`) cycles the visibility: both → stats only → gamepad only → none → both.
Both panels are visible at start. Games can also call `FStatsOverlay::SetStatsVisible()` /
`SetGamepadWidgetVisible()`.

## Targeting PS2 from a game

The reference is `Game/ThirdPerson`:

```cmake
# Game/ThirdPerson/Source/ThirdPerson.Target.cmake
leon_target(ThirdPerson TYPE Game
	PLATFORMS PS2
	COMPILE_AGAINST_ENGINE OFF
)

# Game/ThirdPerson/Source/ThirdPerson/ThirdPerson.Build.cmake
leon_module(ThirdPerson
	PUBLIC_DEPENDENCIES Core InputCore ApplicationCore
	PRIVATE_DEPENDENCIES Launch PS2RHI
)
```

- `PLATFORMS PS2` and `"TargetPlatforms": [ "PS2" ]` in the `.leonproject`.
- `COMPILE_AGAINST_ENGINE OFF`: `WITH_ENGINE=0`, the loop described above. Only modules that are allowed on PS2
  (`Core`, `InputCore`, `ApplicationCore`, `RHI`, `Launch`, `PS2RHI`) can be in the closure.
- The primary game module uses `IMPLEMENT_PRIMARY_GAME_MODULE`, gets the window and input from `GEngineLoop` (a
  dependency on `Launch` gives the include path only) and registers its frame with `FTicker::GetCoreTicker()`.
- Drawing goes through `FPS2RHI` (`PS2RHI.h`); input through `IInputInterface` (`Application->GetInputInterface()`).

Build and run:

```bat
Engine\Build\BatchFiles\Build.bat ThirdPerson PS2 Development -Project=%CD%\Game\ThirdPerson\ThirdPerson.leonproject
Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Project Game\ThirdPerson
```

`RunPCSX2.ps1 [-Project <dir|file.leonproject>] [-Configuration Debug|Development|Shipping] [-Build]` resolves
`<Project>\Binaries\PS2\<Name>.elf` (`<Name>-PS2-<Configuration>.elf` outside Development), finds PCSX2 through
`$env:LEON_PCSX2`, `PATH` or the default install folders, and starts it with `-fastboot -elf`. PCSX2 setup notes:
[Docs/SETUP.md](../../../Docs/SETUP.md#pcsx2-notes).

## Reference

- Sony manuals and PS2 architecture notes: [Docs/PS2OFFICIAL/](../../../Docs/PS2OFFICIAL/README.md)
- UE 4.27 platform extension layout: [Docs/UnrealEngine427/SourceLayout.md](../../../Docs/UnrealEngine427/SourceLayout.md)
