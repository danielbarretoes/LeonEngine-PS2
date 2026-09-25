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

- The plan continues with CoreUObject (below; P8 and P9 are done). What P7 left: the legacy `.llev` / `.lmesh` version-1 data and
  `FLegacyCoordinateConversion` go away with the `.lasset` packages; the deviations it kept (vertical field of view,
  no reversed Z, the GL clip adapter, legacy content facing +Y, the doubled mouse look, the spring arm's socket
  offset) are listed in [LeonMapping — Deviations](LeonMapping.md#deviations-from-ue-427-intentional).

## CoreUObject

### Done — LeonHeaderTool (P8)

The reflection generator: `Engine/Source/Programs/LeonHeaderTool`, which LeonBuildTool runs for every module that
includes a `.generated.h` ([README](../../Engine/Source/Programs/LeonHeaderTool/README.md)).

### Done — CoreUObject (P9)

`Engine/Source/Runtime/CoreUObject` on every platform, PS2 included
([LeonMapping — P9](LeonMapping.md#p9--coreuobject), [README](../../Engine/Source/Runtime/CoreUObject/README.md)):
`UObject`, `UClass`, `UScriptStruct`, `UEnum`, `UFunction`, `UPackage`, `FProperty` and every property type, the script
containers over Core's `TArray` / `TSet` / `TMap` layouts, `NewObject`, `FObjectInitializer`, `CreateDefaultSubobject`
(rebuilt per instance, D12), `FindObject`, `GUObjectArray` (8192 objects on the PS2), `Cast`, `TSubclassOf`,
`ProcessEvent`, and the NoExport Core structs (`FVector`, `FTransform`, …, with LeonHeaderTool `NoExport` support).
`FModuleManager` calls each module's `RegisterReflection`, then CoreUObject's `ProcessNewlyLoadedUObjects`, before its
`StartupModule`. 27 `System.CoreUObject.*` tests run in `LeonAutomationTests` and in TestPAL on the PS2; the reflection
budget is in [Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md).

### Done — GC and references, config and Exec (P10)

([LeonMapping — P10](LeonMapping.md#p10--gc-and-references-config-and-exec),
[README](../../Engine/Source/Runtime/CoreUObject/README.md)):

- Garbage collection: UE4's stop-the-world mark and sweep (D11) from the root set, native objects, class default
  objects, compiled-in packages, `KeepFlags` objects and `FGCObject` holders, through outers and the strong reference
  properties of each class (`UObject*`, `TSubclassOf`, containers and structs of them) plus `AddReferencedObjects`;
  UE 4.27 pending kill (references cleared, object collected); `BeginDestroy` → `IsReadyForFinishDestroy` →
  `FinishDestroy` → destructor, slot freed (weak pointers go stale) and reused; incremental purge;
  `FGarbageCollectionTimer` over `gc.TimeBetweenPurgingPendingKillObjects`.
- References: `TWeakObjectPtr` complete, `TStrongObjectPtr`, `FSoftObjectPath` / `FSoftClassPath` (reflected noexport
  structs with UE's text form), `TPersistentObjectPtr`, `TSoftObjectPtr` / `TSoftClassPtr`.
- Config: `LoadConfig` / `SaveConfig` / `ReloadConfig` for `UCLASS(Config=…)` (and `PerObjectConfig`) with
  `UPROPERTY(Config)` / `GlobalConfig`; class default objects load at creation, instances copy.
- `UFUNCTION(Exec)` with `CallFunctionByNameWithArguments` / `ProcessConsoleExec`, Core's `FExec` /
  `FSelfRegisteringExec`; `BindUObject` / `AddUObject` delegates.
- 21 new `System.CoreUObject.*` tests (48 in total), in TestPAL on the PS2 too; the PS2 GC cost is in
  [Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md).

### Next

- **P11:** `.lasset` packages: `UObject::Serialize`, tagged properties, linkers, `LoadObject`; `FName` in archives
  through the package name table.
- **P12:** turn the naming-only `A`/`U` classes into real `UCLASS` types (`NewObject`, `CreateDefaultSubobject`,
  `Cast<>` instead of `dynamic_cast`); `UObject*` members become `UPROPERTY`s (GC safety), `AActor::Destroy` marks
  the actor pending kill, `UWorld` / `ULevel` hold their actors in `UPROPERTY` arrays.
- **P13:** call `CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS)` where UE does (`UEngine::LoadMap` after releasing the
  old world, the game mode's round restart) and tick an `FGarbageCollectionTimer` from `UEngine::Tick`
  (`ConditionalCollectGarbage`); settings classes (`UGameMapsSettings`, `UInputSettings`) use `UPROPERTY(Config)`
  with `FSoftObjectPath` / `FSoftClassPath`; `UGameViewportClient` routes console commands to
  `ProcessConsoleExec` / `FSelfRegisteringExec::StaticExec`.
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
