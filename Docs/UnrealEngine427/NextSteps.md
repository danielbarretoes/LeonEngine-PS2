# Next steps toward UE 4.27 parity

The UE-layout refactor is done: `Engine/Source/{Runtime,Developer,Programs,ThirdParty}` layout with the PS2
platform extension and the JoltPhysics plugin, LeonBuildTool, HAL / ApplicationCore / RHI / Launch
(`GuardedMain` + `FEngineLoop` on both desktop and PS2), Epic naming in every module, the Epic
`.clang-format`, and the docs ([ARCHITECTURE](../ARCHITECTURE.md), [BUILD](../BUILD.md),
[CODING_STANDARD](../CODING_STANDARD.md), [LeonMapping](LeonMapping.md)).

This page lists what the refactor intentionally left out, plus the debt it surfaced. Each item names the
UE 4.27 location to mirror.

## Core

### Done — Core foundations (P2)

Implemented in `Runtime/Core` on every platform, PS2 included (details:
[LeonMapping — P2](LeonMapping.md#p2--core-foundations), [ARCHITECTURE §6](../ARCHITECTURE.md#6-core-hal-and-foundations)):

- `Misc/Build.h` / `Misc/CoreMiscDefines.h`; `TCHAR` = UTF-8 `char` everywhere (deviation D1).
- HAL: `FPlatformMisc`, `FPlatformAtomics`, integer helpers on `FPlatformMath` / `FMath`; `FMemory` over
  `GMalloc` (`FMallocAnsi`, current / peak tracking).
- Assertions (`check`, `verify`, `ensure`), templates (`TUniquePtr`, `TSharedPtr`, `TFunction`, `TTuple`, `TOptional`,
  sorting, `Algo`).
- Containers (`TArray`, `TArrayView`, `TBitArray`, `TSparseArray`, `TSet`, `TMap`), `FString`, `FCString`, `FName`
  (platform-sized pool, D5), minimal `FText`, `FCrc`.
- Logging (`UE_LOG`, categories, `GLog`, stdout / debugger devices), delegates (`TDelegate`, `TMulticastDelegate`),
  `FTicker` on `FTickerDelegate`.
- Automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) for Core, run by `LeonAutomationTests` and by the new
  `TestPAL` program (PS2 in PCSX2); PS2 size / heap / name-pool budget in
  [Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md).

### Next

- **P3 — Math:** native `FVector`, `FRotator`, `FQuat`, `FMatrix`, `FTransform` and float `FMath` (Z-up and cm
  come later, in P7) — `Math/`. Replaces glm in shared modules and lets the gameplay framework compile on PS2.
  (Today `FTransform` is a glm-based desktop type.)
- **P4 — Files, config, command line:** `IPlatformFile` / `FPlatformFileManager` and an `FPaths` rewrite (removes
  the `std::filesystem` desktop-only code), `FArchive`, `FConfigCacheIni` / `GConfig` (`Misc/ConfigCacheIni.h`;
  load `Engine/Config` + `<Project>/Config`, today placeholders), `FCommandLine` / `FParse` (`Misc/CommandLine.h`,
  `Misc/Parse.h`; desktop flags are parsed by hand in `Launch/Private/Desktop/GameApplication.cpp`), native Json,
  `Projects` (`.lproj` / `.lplugin` readers), a log file device and config-driven log verbosity.
- **P5–P6 — Migration:** move the modules above Core to the UE types (`TArray`, `FString`, delegates, `UE_LOG`,
  automation tests instead of Catch2), then drop glm, nlohmann and `std::` containers from engine APIs.

## CoreUObject

- `UObject`, `UClass`, `UStruct`, `UProperty`/`FProperty` — `Runtime/CoreUObject/Public/UObject/`.
- A reflection generator (UnrealHeaderTool homologue) — LeonBuildTool would run it before compiling a module.
- `NewObject`, `CreateDefaultSubobject`, garbage collection (`GarbageCollection.h`), `TWeakObjectPtr`,
  `TSubclassOf`, `ConstructorHelpers`.
- Once available: turn the naming-only `A`/`U` classes into real `UCLASS` types.
- Replication: the ENet networking was removed in 0.12.0 (local tag `archive/net-enet-0.11`); it returns as
  UObject replication (`UNetDriver`, replicated properties) — `Runtime/Engine/Classes/Engine/NetDriver.h`.

## Engine / platform

- Gameplay framework on PS2 (needs native math (P3) and the module migration to UE containers (P5–P6)); then
  `Game/ThirdPerson` can use `AThirdPersonCharacter : ACharacter` like TP_ThirdPerson.
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
- **Project descriptors:** the pre-refactor packs (`Projects/<Name>/leon.game.json`) were removed in 0.12.0 and
  the `Projects` module is an empty placeholder; read `.lproj` + `<Project>/Content` like UE's
  `FProjectDescriptor` reads `.uproject` (P4). Until then `LeonGame` loads one level with `-map=`.
- **Platform checks in shared code:** the `PLATFORM_WINDOWS` tests in `Core/Private/Misc/Paths.cpp` should become
  HAL functions or move under `Private/Windows` (the P4 `IPlatformFile` / `FPaths` rewrite).
- **Linux:** registered in LeonBuildTool but not built or tested; enable `-Werror=shadow` on the Linux host
  flags when it becomes a gate.
