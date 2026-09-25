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

### Done — Core math (P3)

`Runtime/Core/Public/Math/` on every platform ([LeonMapping — P3](LeonMapping.md#p3--core-math)): float `FMath`,
vectors, `FRotator`, `FQuat`, `FMatrix` and the derived matrices, `FPlane`, `FBox`, `FSphere`, `FBoxSphereBounds`, a
scalar `FTransform`, `FColor` / `FLinearColor`, `FRandomStream`. The glm transform became `FLegacyTransform` (until
P7);
RenderCore's frustum uses Core's `FBox` / `FPlane`; `GlmInterop.h` and `LegacyAxes.h` bridged the unmigrated code
until P6. PS2 builds reject implicit float to double promotion.

### Done — Files, config, command line, Json and Projects (P4)

Every platform ([LeonMapping — P4](LeonMapping.md#p4--files-config-command-line-json-and-projects)):
`IPlatformFile` / `FPlatformFileManager` (Win32, POSIX, PS2 read-only on `host:`), `IFileManager`, `FArchive` and the
memory archives, `FPaths` with UE's API (no more `std::filesystem`), `FFileHelper`, `FCommandLine` / `FParse` /
`FApp`, `FGuid`, `FMD5`, `FDateTime`, `FConfigCacheIni` / `GConfig` with the D8 layers, the log file
(`FOutputDeviceFile`) and `[Core.Log]` / `-LogCmds` verbosity, and `FEngineLoop::PreInit` in UE's order. `Json` is now
native (no nlohmann) and `Projects` reads `.lproj` / `.lplugin`. ThirdPerson reads its character
tuning from `DefaultGame.ini` (compiled defaults when PCSX2's host filesystem is off). Released as 0.13.0.

### Done — Lower modules on the UE types (P5)

ApplicationCore, RHI, OpenGLDrv, PS2RHI, the shared Launch code, PhysicsCore (with `FPhysScene`), RenderCore,
AnimationCore, AudioMixer, SlateCore and UMG use `TArray`, `FString` / `FName` / `FText`, `TUniquePtr` /
`TSharedPtr`, delegates, `UE_LOG` and Core math ([LeonMapping — P5](LeonMapping.md#p5--lower-modules-on-the-ue-types));
their Catch2 tests became automation tests.

### Done — Upper modules on the UE types (P6)

Renderer, Engine, AIModule, MeshUtilities, Cooker, LeonCook, the JoltPhysics plugin and the desktop Launch code
(`FGameApplication`) use `FVector` / `FMatrix`, `TArray`, `TMap`, `FString` / `FName` / `FText`, `TFunction`,
`TUniquePtr` / `TSharedPtr` and `UE_LOG` ([LeonMapping — P6](LeonMapping.md#p6--upper-modules-on-the-ue-types)).
glm, nlohmann and Catch2 are gone, and so is Core's `Migration/` folder; `Json` serves the materials and the cook
recipes; every one of the 179 tests is an automation test; every platform compiles C++17; `CheckBannedApis.ps1` (G4)
guards the result. The world stayed Y-up in metres with glm's GL matrix layout until P7.

### Done — UE axes and units (P7)

The world is UE's: X forward, Y right, Z up, left-handed, 1 unit = 1 cm
([LeonMapping — P7](LeonMapping.md#p7--ue-axes-and-units), [ARCHITECTURE — Coordinates](../ARCHITECTURE.md#coordinates)).
Render matrices are composed with `FMatrix` operators; the camera builds UE view and projection matrices and the
renderer applies `ToGLClipSpace` last. Components, level data and lights hold `FTransform`; controllers carry a
`ControlRotation`. `FLegacyCoordinateConversion` converts `.llev` levels and version-1 `.lmesh` meshes in their
readers only, the importers end with `FImportCoordinateConversion` and write `.lmesh` version 2, and Jolt and
miniaudio stay Y up in metres behind a swap-and-scale boundary. `LegacyGL` and `FLegacyTransform` are gone and G4 bans
them. 22 golden tests recorded before the switch pass unchanged; 231 tests in total. Released as 0.14.0.

### Next

- The plan continues with CoreUObject (below). What P7 left: the legacy `.llev` / `.lmesh` version-1 data and
  `FLegacyCoordinateConversion` go away with the `.lasset` packages; the deviations it kept (vertical field of view,
  no reversed Z, the GL clip adapter, legacy content facing +Y, the doubled mouse look, the spring arm's socket
  offset) are listed in [LeonMapping — Deviations](LeonMapping.md#deviations-from-ue-427-intentional).

## CoreUObject

- `UObject`, `UClass`, `UStruct`, `UProperty`/`FProperty` — `Runtime/CoreUObject/Public/UObject/`.
- The reflection generator exists (P8): `Engine/Source/Programs/LeonHeaderTool`, which LeonBuildTool runs for every
  module that includes a `.generated.h`. CoreUObject provides the macros and `UE4CodeGen_Private` runtime its
  README lists, and calls each module's `RegisterReflection` before `StartupModule`.
- `NewObject`, `CreateDefaultSubobject`, garbage collection (`GarbageCollection.h`), `TWeakObjectPtr`,
  `TSubclassOf`, `ConstructorHelpers`.
- Once available: turn the naming-only `A`/`U` classes into real `UCLASS` types.
- Replication: the ENet networking was removed in 0.12.0 (local tag `archive/net-enet-0.11`); it returns as
  UObject replication (`UNetDriver`, replicated properties) — `Runtime/Engine/Classes/Engine/NetDriver.h`.

## Engine / platform

- Gameplay framework on PS2 (the modules use UE containers and Core math since P6, but Engine still depends on the
  desktop-only Renderer, UMG and AudioMixer); then `Game/ThirdPerson` can use `AThirdPersonCharacter : ACharacter`
  like TP_ThirdPerson.
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
- **Project descriptors:** `.lproj` is loaded in `PreInit` (P4), but `LeonGame` still has no project and loads one
  level with `-map=`; `UEngine::LoadMap` and the config-driven game mode replace it in P13.
- **Platform checks in shared code:** the `PLATFORM_WINDOWS` tests in `Core/Private/HAL/MallocAnsi.cpp` and
  `Core/Private/Misc/OutputDeviceRedirector.cpp` should become HAL functions or move under `Private/Windows`.
- **Linux:** registered in LeonBuildTool but not built or tested; enable `-Werror=shadow` on the Linux host
  flags when it becomes a gate.
