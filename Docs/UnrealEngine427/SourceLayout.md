# UE 4.27 source layout (reference)

All paths relative to the UE checkout root. Only what matters for LeonEngine is listed.

## Root

| Path | Role |
| --- | --- |
| `Engine/` | the engine |
| `Templates/TP_ThirdPerson/`, `TP_ThirdPersonBP/` | project templates (C++ / Blueprint) |
| `Samples/` | `StarterContent`, … |
| `Setup.bat/.sh`, `GenerateProjectFiles.bat/.sh` | fetch binary deps (GitDependencies) / generate IDE projects |
| `UE4Games.uprojectdirs` | extra folders scanned for `.uproject` files |

## `Engine/`

| Path | Role |
| --- | --- |
| `Binaries/<Platform>/` | built executables (`Win64`, …) |
| `Build/` | `Build.version` (JSON), `BatchFiles/` (`Build.bat`, `Clean.bat`, `Rebuild.bat`, `RunUAT.bat`, `GenerateProjectFiles.bat`), per-platform folders |
| `Config/` | `Base*.ini` + `<Platform>/Base<Platform>*.ini` |
| `Content/` | engine assets mounted at `/Engine/` (absent until Setup) |
| `Shaders/` | `Private/*.usf/ush`, `Public/`, `Shared/` |
| `Plugins/<Category>/<Plugin>/` | engine plugins (`Runtime/`, `Editor/`, `Developer/`, …) |
| `Programs/<Program>/` | **runtime data/config** of programs (not their source) |
| `Source/` | all C++ |
| `Platforms/<Platform>/` | **platform extensions** (console/NDA platforms; not present in the public 4.27 checkout — see Platforms below) |
| `Intermediate/`, `Saved/`, `DerivedDataCache/` | generated |

## `Engine/Source/`

| Folder | Contents |
| --- | --- |
| `Runtime/` | modules that ship in games |
| `Developer/` | tooling modules usable by programs and the editor, never shipped (TargetPlatform, MeshUtilities, MeshBuilder, TextureCompressor, DerivedDataCache, PakFileUtilities, …) |
| `Editor/` | editor-only modules (UnrealEd, LevelEditor, …). Cooking is `Editor/UnrealEd/…/Commandlets/CookCommandlet.h` |
| `Programs/` | standalone programs: UnrealBuildTool (C#), AutomationTool, UnrealHeaderTool, UnrealPak, ShaderCompileWorker, BlankProgram, TestPAL, … |
| `ThirdParty/<Lib>/` | one folder per library with `<Lib>.Build.cs` |
| `UE4Game.Target.cs`, `UE4Client.Target.cs`, `UE4Server.Target.cs`, `UE4Editor.Target.cs` | engine-level targets (content-only projects run on `UE4Game`) |

### Runtime modules relevant to LeonEngine

- Foundation: `Core`, `CoreUObject`, `ApplicationCore`, `Launch`, `Projects`, `BuildSettings`, `TraceLog`
- Engine: `Engine`, `EngineSettings`, `DeveloperSettings`
- Input: `InputCore` (`Classes/InputCoreTypes.h` → `FKey`, `EKeys`), `InputDevice`
- Rendering: `RHI`, `RenderCore`, `Renderer`, `OpenGLDrv`, `NullDrv`, **`EmptyRHI`** (clean template for a new RHI backend), `VulkanRHI`, `D3D12RHI`, `Windows/D3D11RHI`
- UI: `SlateCore`, `Slate`, `UMG`
- Physics: `PhysicsCore`, `Experimental/Chaos*`
- Audio: `AudioMixer`, `AudioMixerCore`
- Net: `Sockets`, `Networking`, `NetCore`, `PacketHandlers`
- Data: `Json`, `JsonUtilities`, `ImageCore`, `ImageWrapper`, `PakFile`, `AssetRegistry`
- Animation: `AnimationCore`, `AnimGraphRuntime`
- Gameplay: `AIModule`, `NavigationSystem`, `GameplayTags`, `GameplayTasks`

## Module anatomy

```
<Module>/
  <Module>.Build.cs          dependencies, definitions (ModuleRules)
  Public/                    headers other modules may include
  Classes/                   legacy public folder for UObject headers (Engine uses it heavily)
  Private/                   implementation + private headers
  Private/Tests/             automation tests (Core has them)
  Resources/                 (Launch: Version.h, Windows/PCLaunch.rc)
```

No `Internal/` folder in 4.27 (UE5 only). Includes are module-relative: `#include "GameFramework/Actor.h"`.
Every header starts from `CoreMinimal.h` (Core) — it pulls `CoreTypes.h` (platform typedefs `int32`, `uint8`,
`TCHAR`, `FORCEINLINE`), containers, math, delegates, logging.

## Platform code

Two styles coexist in 4.27:

1. **In-module platform folders** (public platforms): `Core/Public/Windows/WindowsPlatformMisc.h` +
   `Core/Private/Windows/WindowsPlatformMisc.cpp`; also `Linux/`, `Unix/`, `Mac/`, `Apple/`, `IOS/`, `Android/`,
   `Microsoft/`, `MSVC/`, `Clang/`.
2. **Platform extensions** for console / NDA platforms (introduced 4.24+, mandated by the coding standard):
   `Engine/Platforms/<Platform>/Source/Runtime/<Module>/Private/<Platform>PlatformMemory.cpp`,
   `Engine/Platforms/<Platform>/Config/`, … They are merged into the module of the same name by UBT.

Generic API vs dispatch:

- `Core/Public/GenericPlatform/Generic*.h` — base implementations (`FGenericPlatformMisc`, `…Memory`, `…Time`,
  `…Math`, `…File`, `…Atomics`, `…Process`, `…TLS`, `…String`).
- `Core/Public/HAL/Platform*.h` — typedef dispatch: `HAL/PlatformMemory.h` does
  `#include COMPILED_PLATFORM_HEADER(PlatformMemory.h)` then `typedef F<Plat>PlatformMemory FPlatformMemory`.
- `Core/Public/HAL/PreprocessorHelpers.h`:
  - `PLATFORM_HEADER_NAME` = `OVERRIDE_PLATFORM_HEADER_NAME` or `UBT_COMPILED_PLATFORM`.
  - `PLATFORM_IS_EXTENSION` (default 0).
  - `COMPILED_PLATFORM_HEADER(Suffix)` → extension: `"<Plat><Suffix>"`; in-module: `"<Plat>/<Plat><Suffix>"`.
- `HAL/Platform.h` defaults every `PLATFORM_<X>` to 0, then includes the platform header which sets its own to 1
  and defines capability macros (`PLATFORM_DESKTOP`, `PLATFORM_USE_PTHREADS`, …).
- Rule: no `PLATFORM_<X>` checks outside `<X>` folders; add a static function to `FPlatformMisc` or a capability
  define instead.

UBT platform folder filtering: a folder named after a platform or platform group (`Windows`, `Microsoft`, `Linux`,
`Unix`, `Desktop`, …) only compiles for matching platforms.

## Modules and startup

- `Core/Public/Modules/ModuleInterface.h` (`IModuleInterface`: `StartupModule`, `ShutdownModule`),
  `ModuleManager.h` (`FModuleManager`, `FDefaultModuleImpl`, `FDefaultGameModuleImpl`).
- `IMPLEMENT_MODULE(Class, Name)`: monolithic builds create a static `FStaticallyLinkedModuleRegistrant<Class>`
  plus `extern "C" void IMPLEMENT_MODULE_<Name>()` (a forced linker reference proving each module implements it).
- `IMPLEMENT_GAME_MODULE` = `IMPLEMENT_MODULE`; `IMPLEMENT_PRIMARY_GAME_MODULE(Class, Name, "GameName")` also sets
  the game name globals.

## Launch

`Runtime/Launch/`:

- `Public/LaunchEngineLoop.h` — `FEngineLoop` (`PreInit`, `Init`, `Tick`, `Exit`), `GEngineLoop`.
- `Private/Launch.cpp` — `GuardedMain(CmdLine)`: `EnginePreInit` → `EngineInit` → `while (!IsEngineExitRequested())
  EngineTick()` → `EngineExit` (via a cleanup guard).
- `Private/LaunchEngineLoop.cpp` — the implementation.
- `Private/<Platform>/Launch<Platform>.cpp` — the OS entry point (`WinMain` in `Windows/LaunchWindows.cpp`).
- `Resources/Version.h` — `ENGINE_MAJOR_VERSION` etc.
- Game-less loop: programs compile with `bCompileAgainstEngine = false` (`WITH_ENGINE=0`); ticking then goes through
  `FTicker::GetCoreTicker()` (`Core/Public/Containers/Ticker.h`).

## RHI and rendering

- `RHI/Public/DynamicRHI.h` — `FDynamicRHI` (abstract backend), `GDynamicRHI`, `PlatformCreateDynamicRHI()`.
- `RHI/Public/RHI.h`, `RHIResources.h`, `RHIDefinitions.h`, `RHICommandList.h`.
- `RHI/Private/<Platform>/` — per-platform RHI selection.
- `OpenGLDrv/Public/OpenGLDrv.h` (`FOpenGLDynamicRHI`), `OpenGLResources.h`, `Private/Windows|Linux|Android`.
- `EmptyRHI/Public/EmptyRHI.h`, `Private/Empty{Commands,Texture,VertexBuffer,…}.cpp` — skeleton backend.
- `RenderCore/` — `RenderResource.h`, `RenderingThread.h`, `Shader.h`, `VertexFactory.h`.
- `Renderer/Private/` — `DeferredShadingRenderer`, `BasePassRendering`, … (API-agnostic, goes through RHI).

## Application and input

- `ApplicationCore/Public/GenericPlatform/` — `GenericApplication.h`, `GenericWindow.h` (`FGenericWindow`),
  `GenericWindowDefinition.h`, `IInputInterface.h`, `ICursor.h`, `GenericPlatformApplicationMisc.h`.
- `ApplicationCore/Public/HAL/PlatformApplicationMisc.h` — dispatch (`FPlatformApplicationMisc::CreateApplication()`).
- `ApplicationCore/Public/Windows/WindowsApplication.h`, `WindowsWindow.h`.
- Gamepad on Windows: `ApplicationCore/Private/Windows/XInputInterface.{h,cpp}`, owned by `FWindowsApplication`.
- `InputCore/Classes/InputCoreTypes.h` — `struct FKey`, `struct EKeys` (statics: `SpaceBar`,
  `Gamepad_FaceButton_Bottom`, `Gamepad_LeftX`, `Gamepad_Special_Left` (= Select/Back),
  `Gamepad_Special_Right` (= Start), `Gamepad_DPad_Up`, …).

## Plugins

`Engine/Plugins/<Category>/<Plugin>/`:

- `<Plugin>.uplugin` (JSON: `FileVersion`, `FriendlyName`, `Category`, `EnabledByDefault`, `CanContainContent`,
  `Modules[] {Name, Type Runtime|Editor|Developer, LoadingPhase, WhitelistPlatforms/BlacklistPlatforms}`).
- `Source/<Module>/<Module>.Build.cs`, `Public/`, `Private/` (module startup in `Private/<Plugin>Module.cpp`).
- Optional `Content/`, `Config/`, `Resources/`, `Source/ThirdParty/`.

## Config

- `Engine/Config/Base.ini`, `BaseEngine.ini`, `BaseGame.ini`, `BaseInput.ini`, `BaseScalability.ini`, …
- `Engine/Config/<Platform>/Base<Platform>Engine.ini`, `<Platform>Engine.ini`, `DataDrivenPlatformInfo.ini`.
- Layering: `Engine/Config/Base<Cat>.ini` → `Engine/Config/<Plat>/Base<Plat><Cat>.ini` →
  `<Project>/Config/Default<Cat>.ini` → `<Project>/Config/<Plat>/<Plat><Cat>.ini` → `Saved/Config/…`.
- Sections keyed by class path (`[/Script/Engine.InputSettings]`), `+Key=` appends to arrays.

## Project template `Templates/TP_ThirdPerson`

```
TP_ThirdPerson.uproject                  {"FileVersion":3,"Modules":[{"Name":"TP_ThirdPerson","Type":"Runtime","LoadingPhase":"Default"}]}
Config/DefaultEngine.ini DefaultGame.ini DefaultInput.ini DefaultEditor.ini
Source/TP_ThirdPerson.Target.cs          Type = TargetType.Game; ExtraModuleNames.Add("TP_ThirdPerson")
Source/TP_ThirdPersonEditor.Target.cs
Source/TP_ThirdPerson/TP_ThirdPerson.Build.cs     Core, CoreUObject, Engine, InputCore, HeadMountedDisplay
Source/TP_ThirdPerson/TP_ThirdPerson.h/.cpp       IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, TP_ThirdPerson, "TP_ThirdPerson")
Source/TP_ThirdPerson/TP_ThirdPersonCharacter.h/.cpp
Source/TP_ThirdPerson/TP_ThirdPersonGameMode.h/.cpp
```

Behaviour worth copying:

- Character: capsule 42×96, `BaseTurnRate = 45`, `bUseControllerRotation{Pitch,Yaw,Roll} = false`,
  `bOrientRotationToMovement = true`, `RotationRate = (0, 540, 0)`, `JumpZVelocity = 600`, `AirControl = 0.2`.
- `CameraBoom` (`USpringArmComponent`): `TargetArmLength = 300`, `bUsePawnControlRotation = true`;
  `FollowCamera` (`UCameraComponent`) attached to `USpringArmComponent::SocketName`.
- Movement: controller yaw → `FRotationMatrix(YawRotation).GetUnitAxis(X|Y)` → `AddMovementInput`.
- Input (`DefaultInput.ini`): Jump = SpaceBar / `Gamepad_FaceButton_Bottom`; axes MoveForward/MoveRight on
  `Gamepad_LeftY/X`, Turn/LookUp on `Gamepad_RightX/Y` (dead zone 0.25).
- GameMode: `DefaultPawnClass` = the ThirdPersonCharacter Blueprint.
