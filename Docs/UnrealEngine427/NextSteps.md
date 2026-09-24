# Next steps toward UE 4.27 parity

The UE-layout refactor is done: `Engine/Source/{Runtime,Developer,Programs,ThirdParty}` layout with the PS2
platform extension and the JoltPhysics plugin, LeonBuildTool, HAL / ApplicationCore / RHI / Launch
(`GuardedMain` + `FEngineLoop` on both desktop and PS2), Epic naming in every module, the Epic
`.clang-format`, and the docs ([ARCHITECTURE](../ARCHITECTURE.md), [BUILD](../BUILD.md),
[CODING_STANDARD](../CODING_STANDARD.md), [LeonMapping](LeonMapping.md)).

This page lists what the refactor intentionally left out, plus the debt it surfaced. Each item names the
UE 4.27 location to mirror.

## Core (next plan)

- **Containers:** `TArray`, `TMap`, `TSet`, `TSparseArray` — `Runtime/Core/Public/Containers/`.
- **Strings:** `FString`, `FName`, `FText`, `TCHAR`/`TEXT()` — `Containers/UnrealString.h`, `UObject/NameTypes.h`,
  `Internationalization/Text.h`. On PS2, `TCHAR` should stay `char` (no wide strings on EE).
- **Math:** native `FVector`, `FRotator`, `FQuat`, `FMatrix`, `FTransform` (Z-up, UE units = cm) — `Math/`.
  Replaces glm in shared modules and lets the gameplay framework compile on PS2. (Today `FTransform` is a
  glm-based desktop type.)
- **Logging:** `UE_LOG`, `DECLARE_LOG_CATEGORY_EXTERN`, `FOutputDevice` — `Logging/LogMacros.h`.
- **Assertions:** `check`, `ensure`, `verify` — `Misc/AssertionMacros.h`.
- **Delegates:** `DECLARE_DELEGATE*`, `TMulticastDelegate` — `Delegates/` (`FTicker` currently stores
  `std::function`).
- **Memory:** `FMemory`, `FMalloc` (PS2: custom allocator over EE RAM) — `HAL/UnrealMemory.h`, `HAL/MallocAnsi.h`.
- **Config:** `FConfigCacheIni`, `GConfig` — `Misc/ConfigCacheIni.h`; load `Engine/Config` + `<Project>/Config`
  (the `.ini` files exist as placeholders).
- **Command line / parse:** `FCommandLine`, `FParse` — `Misc/CommandLine.h`, `Misc/Parse.h` (desktop flags are
  parsed by hand in `Launch/Private/Desktop/GameApplication.cpp`).

## CoreUObject

- `UObject`, `UClass`, `UStruct`, `UProperty`/`FProperty` — `Runtime/CoreUObject/Public/UObject/`.
- A reflection generator (UnrealHeaderTool homologue) — LeonBuildTool would run it before compiling a module.
- `NewObject`, `CreateDefaultSubobject`, garbage collection (`GarbageCollection.h`), `TWeakObjectPtr`,
  `TSubclassOf`, `ConstructorHelpers`.
- Once available: turn the naming-only `A`/`U` classes into real `UCLASS` types.

## Engine / platform

- Gameplay framework on PS2 (needs native math + containers first); then `Game/ThirdPerson` can use
  `AThirdPersonCharacter : ACharacter` like TP_ThirdPerson.
- Renderer through RHI command lists instead of direct GL calls; break the Engine ↔ Renderer cycle.
- `UNavigationSystemBase` seam so NavigationSystem can move to its own module.
- `PS2TargetPlatform` Developer module (cook formats for PS2: textures, LPS2 meshes).
- Texture mipmaps on PS2 (GS MIPTBP registers) — fixes floor moiré in ThirdPerson.
- AutomationTool homologue (`RunLAT`: build → cook → stage).

## Debt surfaced by the refactor

- **RHI ownership:** `FGenericWindow::InitRHI` creates the platform RHI, so ApplicationCore depends on
  OpenGLDrv / PS2RHI. UE initialises the RHI from Launch (`FEngineLoop::PreInit` → `RHIInit`); move it there.
- **Desktop window ownership:** with `WITH_ENGINE=1`, `UGameEngine` creates its own application and window
  instead of `FEngineLoop` (UE: `FSlateApplication` + `UGameEngine::GameViewport`).
- **Game → Launch:** the PS2 game module reads `GEngineLoop.GetMainWindow()` through an include-only
  dependency on Launch; give games an engine-side accessor instead (UE: `GEngine->GameViewport`).
- **Desktop packs:** `FProjectDescriptor` still resolves the pre-refactor layout
  (`Projects/<Name>/leon.game.json`); read `.lproj` + `<Project>/Content` like UE's `FProjectDescriptor`
  reads `.uproject`.
- **Platform checks in shared code:** `_WIN32` tests in `Core/Private/Misc/Paths.cpp` and
  `Engine/Private/Net/NetUtil.cpp` should become HAL functions or move under `Private/Windows`.
- **Unused dependencies:** Engine and UMG declare a private `GLFW` dependency that no source uses.
- **Linux:** registered in LeonBuildTool but not built or tested; enable `-Werror=shadow` on the Linux host
  flags when it becomes a gate.
