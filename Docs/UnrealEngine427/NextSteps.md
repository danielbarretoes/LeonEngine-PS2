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
`ControlRotation`. `FLegacyCoordinateConversion` converted `.llev` levels and version-1 `.lmesh` meshes in their
readers only (the `.lmesh` reader went in P14), the importers end with `FImportCoordinateConversion`, and Jolt and
miniaudio stay Y up in metres behind a swap-and-scale boundary. `LegacyGL` and `FLegacyTransform` are gone and G4 bans
them. 22 golden tests recorded before the switch pass unchanged; 231 tests in total. Released as 0.14.0.

### Next

- The plan continues with CoreUObject (below; P8 and P9 are done). What P7 left: the legacy `.llev` data went with
  the `.lmap` maps (P15; the `.lmesh` files in P14), and `FLegacyCoordinateConversion` lives in the tests only (the
  golden tables); the deviations it kept (vertical field of view,
  no reversed Z, the GL clip adapter, legacy content facing +Y, the doubled mouse look (applied once since P13),
  the spring arm's socket offset) are listed in [LeonMapping — Deviations](LeonMapping.md#deviations-from-ue-427-intentional).

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

### Done — Packages (P11)

([LeonMapping — P11](LeonMapping.md#p11--packages), [ASSET_FORMATS — Packages](../ASSET_FORMATS.md#packages--lasset--lmap),
[README](../../Engine/Source/Runtime/CoreUObject/README.md)):

- `.lasset` / `.lmap` packages (D13): one file with a `'LEON'` summary (`FPackageFileSummary`, `ELeonPackageVersion`),
  sorted name table, import and export tables (`FPackageIndex`, `FObjectImport`, `FObjectExport`), soft package
  references, the export data and the bulk data at the end. Saving is deterministic (sorted tables, GUID derived from
  the package name, no timestamps); a golden hash checks it on Win64 and the PS2.
- `UPackage::SavePackage` / `Save` / `SaveToMemory` (`FLinkerSave`), `LoadPackage` / `LoadObject` / `LoadClass` /
  `StaticLoadObject` / `FindPackage` (`FLinkerLoad`, synchronous: imports load their packages, `PostLoad` once
  everything is serialized); `FSoftObjectPath::TryLoad` and `TSoftObjectPtr::LoadSynchronous` load packages.
- Tagged properties (`FPropertyTag`, `UStruct::SerializeTaggedProperties`, `FProperty::SerializeItem` /
  `ConvertFromType`): deltas against the archetype (D12 subobjects included), nested structs, containers, enums by
  name, schema evolution (unknown tags skipped, numbers and enums converted). `UObject::Serialize` with a native tail;
  `FByteBulkData`; `FPackageName` with `/Engine/`, `/Game/`, `/Script/` and registered mount points; editor-only
  filtering (D14).
- 14 new `System.CoreUObject.Package.*` tests (62 CoreUObject tests), 13 of them in TestPAL on the PS2 (106 tests); the
  PS2 round-trip cost is in [Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md). Released as 0.15.0.

### Done — Gameplay framework as UObjects (P12)

([LeonMapping — P12](LeonMapping.md#p12--gameplay-framework-as-uobjects),
[ARCHITECTURE §10](../ARCHITECTURE.md#10-gameplay-framework-engine-desktop)):

- Engine, AIModule, UMG and AnimationCore are reflected (AnimationCore's anim instances moved to Engine in P14). `AActor`, `UActorComponent`, `USceneComponent`, the new
  `UPrimitiveComponent` / `UShapeComponent` / `UCapsuleComponent` / `UBoxComponent` / `USphereComponent` /
  `UMeshComponent` / `UStaticMeshComponent`, `USkeletalMeshComponent`, `UCameraComponent`, `USpringArmComponent`,
  `UMovementComponent` / `UPawnMovementComponent` / `UCharacterMovementComponent`, `APawn`, `ACharacter`, `AController`,
  `APlayerController`, `AAIController`, `AInfo`, `AGameModeBase`, `AGameMode` (`MatchState`), `AGameStateBase`,
  `AGameState`, `APlayerState`, `AHUD`, `UGameInstance` (`FWorldContext`), `UWorld`, `ULevel`, `UPlayerInput`,
  `UUserWidget` and the widgets, `UAnimInstance` are `UCLASS` types with `UPROPERTY` members; the level POD is
  `FLevelStaticMesh`.
- Ownership: `UGameEngine` (an `FGCObject`) → `UGameInstance` → `FWorldContext` → `UWorld` (transient package, root set)
  → `ULevel` → actors → components. `UWorld::SpawnActor` / `SpawnActor<T>` with `FActorSpawnParameters`, UE's spawn
  sequence and `DestroyActor`; the world spawns the game mode (`SetGameMode`), the game mode its game state, player
  controllers their player states.
- `Cast<>` replaces every `dynamic_cast`; Leon code builds without RTTI or C++ exceptions on every platform (D17).
- Garbage collection at safe points (D11): world teardown, level load, `UGameEngine::ConditionalCollectGarbage`
  after the world tick. Tests create worlds with `FScopedTestWorld`. 308 tests; the golden tests only changed how
  they construct objects; Win64 frames and PS2 ELFs are unchanged.

### Done — Levels as actors and the render boundary (P13, part 1)

([LeonMapping — P13](LeonMapping.md#p13--levels-as-actors-and-the-render-boundary-part-1),
[LEVELS.md](../LEVELS.md), [ARCHITECTURE §12](../ARCHITECTURE.md#12-rendering-desktop)):

- The `.llev` reader spawns `AWorldSettings`, `AStaticMeshActor`, `APlayerStart`, `ATargetPoint`, `ATriggerVolume`,
  `ABlockingVolume`, `APainCausingVolume` (box volumes, D16), `ADirectionalLight` and `APointLight`; the saver writes
  the same bytes back from the actors. `FLevelStaticMesh` and the level snapshots are gone; `ULegacyLevelDataComponent`
  keeps the fields with no UE home until P15.
- Physics bodies come from `UPrimitiveComponent::CreatePhysicsState` and are keyed by component
  (`RegisterBodiesFromLevel`, `SyncFromLevel` / `SyncToLevel` are gone).
- The render boundary: `FSceneInterface` and the scene proxies, `IRendererModule`, `FSceneViewFamily` / `FSceneView`
  and `FCanvas` are Engine headers; the Renderer implements `FRendererModule` and `FScene` and owns the GPU copies of
  the assets, which are CPU data in Engine. `UWorld::Scene` is allocated through the module (none with `-nullrhi`),
  the HUD, UMG and the debug text draw through `FCanvas`, and Engine includes no Renderer header (the Renderer depends
  on Engine; the cycle is gone).
- 310 tests; the golden tables, the `.llev` bytes and the Win64 frames are unchanged.

### Done — The engine object, maps, input and the viewport client (P13, part 2)

([LeonMapping — P13 part 2](LeonMapping.md#p13--the-engine-object-maps-input-and-the-viewport-client-part-2),
[ARCHITECTURE §9–§10](../ARCHITECTURE.md#9-launch-and-the-engine-loop)):

- `UEngine` / `UGameEngine` are UObjects and `GEngine` is the engine, created by `FEngineLoop::Init` from
  `[/Script/Engine.Engine] GameEngine=`; `FEngineLoop` does the boot on desktop (the window and `RHIInit` in `PreInit`,
  `-ExecCmds`) and `FGameApplication` is gone.
- `UGameInstance::StartGameInstance` → `UEngine::Browse` → `LoadMap`: the old world goes (end play, destroy,
  collect), the new one loads the map's `.llev`, the game mode comes from D18's precedence (`?game=`, the world
  settings, map prefixes, `GlobalDefaultGameMode`), the local players log in through UE's `Login` / `PostLogin` /
  `RestartPlayer` flow and the world begins play. `EngineSettings` (`UGameMapsSettings`) and `FURL` arrived.
- Input by config: InputCore's `FKey` is reflected (so the PS2 game boots the object system), `UInputSettings` reads
  `BaseInput.ini`, the player controller owns its `UPlayerInput`, input component, `AHUD` and camera manager, and
  `ADefaultPawn` replaces `ADefaultGameMode`, its controller and its camera actor.
- `UGameViewportClient` routes the input and the console commands (`show`, `stat`, `obj gc`, `open`, the Exec
  functions of the player's objects, `DebugExecBindings` on F1–F6) and draws the frame.
- 318 tests; the golden tables, the `.llev` bytes and the Win64 frames are unchanged; ThirdPerson runs at 60 FPS on
  PCSX2 with the same Draw3D numbers. Released as 0.16.0.

### Done — Asset classes (P14, part 1)

([LeonMapping — P14](LeonMapping.md#p14--asset-classes-part-1),
[ASSET_FORMATS — Asset classes](../ASSET_FORMATS.md#asset-classes)):

- The assets are UObjects in Engine with UE's names and headers: `UTexture` / `UTexture2D`, `UStaticMesh` (with a
  `UBodySetup`), `UMaterialInterface` / `UMaterial` (fixed shading models, no node graph), `USkeleton` (with
  `USkeletalMeshSocket`s), `USkeletalMesh`, `UAnimSequence`, `UBlendSpace1D`, `USoundWave`, `UDataAsset` and the
  `UCommandlet` base. Their payloads (texels, geometry, tracks, samples) are `FByteBulkData` at the end of a package;
  every class saves and loads back in the tests. The anim instances moved to Engine with them; AnimationCore keeps
  the plain data the FBX import produces.
- `FResourceCache` is gone: the components hold their meshes and materials through `UPROPERTY`s, the legacy files
  become transient assets through `FLegacyAssetLoader` (the only reader of `.lmesh`, `.lmat`, images and `.wav` at run
  time), and the engine's defaults come from `[/Script/Engine.Engine]` (`DefaultMaterialName`, `DefaultTextureName`,
  `DefaultBumpNormalTextureName`), made at their `/Engine/...` paths until the packages exist.
- The Renderer keeps its GPU copies keyed by asset; an asset frees its copy when its data changes and in
  `BeginDestroy`, and the scene keeps its proxies' assets alive through the garbage collector.
- 328 tests; the golden tables, the `.llev` bytes and the Win64 frames are unchanged; the PS2 ELFs are unchanged.

### Done — Editor module, import and content (P14, part 2)

([LeonMapping — P14 part 2](LeonMapping.md#p14--editor-module-import-and-content-part-2),
[ASSET_FORMATS — Importing assets](../ASSET_FORMATS.md#importing-assets), [TOOLS — LeonCook](../TOOLS.md#leoncook)):

- The editor module `LeonEd` (`Engine/Source/Editor`, a new `Editor` module type: desktop only, never in a game
  target) brings `UFactory` and UE's factories (`UTextureFactory`, `UFbxFactory` for FBX and OBJ static and skeletal
  meshes and animations, `UGLTFImportFactory`, `USoundFactory`, `UMaterialFactoryNew`), reimport through
  `FReimportHandler` / `FReimportManager`, and the commandlets `ImportAssets` (`-source` / `-dest`, `-importlist`,
  `-reimport -all`), `ResavePackages`, `ValidateAssets`, `MigrateLegacyContent` (temporary) and a minimal `Cook`
  (`Developer/Cooker` and its recipes are gone). LeonCook is `LeonCook [<Project>.lproj] -run=<Commandlet>`.
- Imported assets keep an editor-only, instanced `UAssetImportData`: the source relative to the engine or project,
  its MD5 and the import settings, never a timestamp. Gate G5 starts: `CheckReimport.bat` (then run by the CI, since
  removed) reimports the content and fails when git sees a change.
- The engine content is `/Engine` packages: `T_Default_D` imported from `Engine/SourceArt` (`ImportList.ini`), the
  three materials migrated from their `.lmat` files (deleted), `DefaultTexture`, `T_Default_Bump_N` and the
  BasicShapes saved once from their generators. The runtime loads them with `LoadObject`; the `.llev` keys resolve to
  packages (`FLegacyAssetKeys`). The UI sounds are config keys (empty: no licensed WAVs), and sounds play PCM16 from
  memory.
- `LeonMeshFormat`, `LeonMaterialFormat`, `FLegacyAssetLoader` and run-time image / WAV loading are gone; stb_image
  lives only in the texture factory.
- 340 tests; the golden tables, the `.llev` bytes and the Win64 frames (Starter and the render test level, migrated
  with `MigrateLegacyContent`) are unchanged; the PS2 ThirdPerson ELF grows by 8 bytes of alignment (the window icon
  API), BlankProgram and TestPAL are unchanged.

### Done — Maps and the glTF map importer (P15)

([LeonMapping — P15](LeonMapping.md#p15--maps-and-the-gltf-map-importer), [LEVELS.md](../LEVELS.md),
[ASSET_FORMATS — Maps](../ASSET_FORMATS.md#maps--lmap)):

- A world saves as a `.lmap` package (`PKG_ContainsMap`): the `UWorld`, its persistent level, `AWorldSettings` and
  the actors with their components; `UEngine::LoadMap` loads `/Game/Maps/X` and `/Engine/Maps/X` (or a `.lmap` file,
  mounting its content folder), then `InitWorld` and `InitializeActorsForPlay` register and initialize the actors. The
  collision and mobility are properties, and a scene component saves its relative quaternion, so transforms load
  bit-exact.
- The legacy level data has homes: `URotatingMovementComponent` (spin), `DefaultGameMode`, and an `ACameraActor` for
  the camera framing plus an `APlayerStart` at the view it opened with. (Leon's `UBobbingMovementComponent`,
  `UOrbitMovementComponent` and `UInteractableComponent` held the bob, light orbit and trigger data until they were
  removed in 0.20.1: no UE class, and no map used them.)
- `UGLTFMapFactory` (`-run=ImportAssets -type=Map`) imports glTF scenes: one actor per node by the naming rules of
  `[/Script/LeonEd.MapImportSettings]` (`UCX_`, `COL_`, `Clip_`, `PlayerStart`, `NavWaypoint` in `BaseEditor.ini`; a
  project's own, and its `RequiredTags` check), the shared meshes and PBR materials next to the map,
  KHR_lights_punctual lights, `ANavigationWaypoint` with the extras' links and flags. A reimport rebuilds the map in
  place and saves the same bytes (G5 covers maps). `/Engine/Maps/AxisTest` (from a `.glb` a stdlib Python script
  writes) checks the axes.
- `Blank.llev` and `Starter.llev` became `/Engine/Maps/Entry` and `/Engine/Maps/Template_Default` (the default map);
  then the `.llev` reader and saver, `FLegacyAssetKeys`, `ULegacyLevelDataComponent`, `APlayerStartPIE`,
  `FPaths::ResolveLegacyContentPath`, `Engine/Content/LevelTemplates`, the legacy factories and `MigrateLegacyContent`
  were deleted, and `FLegacyCoordinateConversion` moved to the tests.
- 339 tests; the golden tables and the Win64 frames (Starter by default and by name, the render test map migrated to
  a `.lmap`) are unchanged; the PS2 ELFs are unchanged.

### Done — Cook, `.lpak` and staging (P16, 0.17.0)

([LeonMapping — P16](LeonMapping.md#p16--cook-lpak-and-staging), [ASSET_FORMATS — Paks](../ASSET_FORMATS.md#paks--lpak),
[TOOLS — The cook](../TOOLS.md#the-cook), [BUILD — Staging and Shipping](../BUILD.md#staging-and-shipping)):

- **PakFile** (Runtime, every platform): `.lpak` files (raw entries, an index sorted by the CRC-32 of the lowercased
  path with a SHA-1 per entry, a 44-byte `FPakInfo` footer with the magic `'LPAK'`), `FPakFile`, and
  `FPakPlatformFile` on top of the physical platform file: the desktop `FEngineLoop::PreInit` mounts
  `<Project>/Content/Paks/*.lpak` (and the engine's) before the config loads; Shipping reads only from its paks.
  `FPakWriter` and `Programs/LeonPak` (UnrealPak: `-create`, `-list`, `-test`, `-extract`, `-align=2048`) write them
  deterministically. Core has `FSHA1`.
- **TargetPlatform** (Developer): `ITargetPlatform` / `ITargetPlatformManagerModule` with Win64 (identity) and a PS2
  stub.
- **The cook** (`-run=Cook -TargetPlatform=Win64|PS2`): seeds from the maps (`MapsToCook`, else every map under
  `/Game/Maps`), `DirectoriesToAlwaysCook` (`BaseGame.ini` cooks `/Engine/BasicShapes`) and every path the config
  names; the closure over the packages' tables (hard imports and soft references); cooked packages without the
  editor-only data (`UObject::IsEditorOnly`: the assets' and the worlds' import data stay out) and with the target's
  name; the config (no Editor ini), the shaders and the `.lproj` staged beside them; two cooks give the same bytes.
- **Staging**: `BuildCookRun.bat -project= -platform=Win64 -build -cook -stage -pak [-run]` builds the game (Shipping by
  default), cooks, paks the cooked folder into `<Project>/Content/Paks/<Project>-Win64.lpak` under the mount point
  `../../../` and stages it in `<Project>/Saved/StagedBuilds/Win64/` (UE's layout); the staged game finds its folders
  (`FPaths::IsStaged`) and reads everything from the pak. The staged Shipping build of the engine's content renders the
  Development frame byte for byte; the CI of the time (since removed) ran a Development staged build
  headless.
- Captures are unattended: `-Screenshot` / `-ExitAfterFrames` runs ignore the mouse and the keyboard
  (`UGameViewportClient::SetIgnoreInput`), so a capture no longer depends on the mouse.
- 350 tests; the golden tables and the Win64 frames are unchanged; the engine content was resaved for 0.17.0 (the
  package summary records the engine version).

### Done — ShooterGame boot and a basic FPS (P17)

([LeonMapping — P17](LeonMapping.md#p17--shootergame-boot-and-a-basic-fps), [ShooterGame README](../../Game/ShooterGame/README.md),
[LEVELS — de_leon](../LEVELS.md#worked-example-de_leon)):

- **Collision channels** (PhysicsCore, Engine): `ECollisionChannel`, `ECollisionResponse`, `FCollisionResponseContainer`,
  `FCollisionResponseParams`, `FCollisionObjectQueryParams`, ignored actors in `FCollisionQueryParams`, traces and
  sweeps by channel and by object type (`Single` / `Multi`), `FHitResult::GetActor` / `GetComponent`,
  `UPrimitiveComponent`'s object type and responses, `UCollisionProfile` (`DefaultChannelResponses`: a game's named
  channels); the character's capsule is a query-only Pawn body that traces hit. The defaults keep every golden table.
- **UE's movement model** in `UCharacterMovementComponent` (`bInstantVelocity = false`): `MaxAcceleration`,
  `GroundFriction`, `BrakingDecelerationWalking`, `BrakingFrictionFactor`, `bUseSeparateBrakingFriction`, `AirControl`
  and its boost (`CalcVelocity`, `ApplyVelocityBraking`), crouching with the room check (`Crouch`, `UnCrouch`,
  `CrouchedHalfHeight`), the virtual `GetMaxSpeed`; the component ticks the movement. First person:
  `UCameraComponent::bUsePawnControlRotation`, `UPlayerInput::SetMouseSensitivity`.
- **ShooterGame** (`Game/ShooterGame`, Win64): `AShooterGameMode` (teams, team starts, `bot_add_ct` / `bot_add_t` /
  `bot_add` / `bot_fill`), `AShooterCharacter` (first-person camera, CS movement in `UShooterCharacterMovement`, the
  walk key, crouch), `AShooterPlayerController`, `AShooterAIController`, `AShooterPlayerState` (team), `AShooterHUD`
  (CS crosshair); its tests program `ShooterGameTests` (LeonBuildTool's `AUTOMATION_TEST_MODULES`).
- **de_leon**: a Blender-scripted blockout (two sites, three lanes, mid doors, crates, a clip, buy zones, five starts a
  team, 18 linked waypoints) imported with the project's rules and required tags; the engine's maps skip a project's
  required tags. Team bodies as placeholders.
- **G6**: `SmokeTest.bat` (ShooterGame headless, `bot_fill`, ten pawns, exit 0), run then by the CI (since
  removed), with the staged ShooterGame.
- 371 engine tests and 10 ShooterGame tests; the golden tables and the Win64 frames are unchanged.

### Done — Weapons and damage (P18)

([LeonMapping — P18](LeonMapping.md#p18--weapons-and-damage), [ShooterGame README — Weapons](../../Game/ShooterGame/README.md#weapons)):

- **Damage** (Engine): UE's `TakeDamage` with `FDamageEvent` / `FPointDamageEvent` / `FRadialDamageEvent` and
  `UDamageType`, the `Apply*Damage` helpers with a line of sight, the damage delegates; `ACharacter` has no health of
  its own any more.
- `UProjectileMovementComponent`, `ASpectatorPawn` and the controller states, actor life spans, static mesh sockets
  from glTF, the view model pass and owner visibility, impact marks, tracers and short-lived lights.
- **ShooterGame's weapons**: a pistol, a rifle, an AWP (zoom, scope, bolt) and an HE grenade; CS's spread, recoil and
  range falloff from a seeded stream (deterministic); reloads; headshots, CS's armor ratio, friendly fire; death
  drops the best weapon, which the next pawn without one picks up; a dead player spectates. Meshes and sounds made
  by scripts here (CC0).
- **Fix**: a native CDO keeps its constructor's values for inherited config members.
- 383 engine tests and 19 ShooterGame tests.

### Done — Rounds, money, buying and the bomb (P19)

([LeonMapping — P19](LeonMapping.md#p19--rounds-money-buying-and-the-bomb), [ShooterGame README — Rounds](../../Game/ShooterGame/README.md#rounds-money-and-the-bomb)):

- The match on UE's match states: a warmup, then CS's rounds (freeze, round, result), the win conditions (the
  eliminations, the time, the bomb), the match's end and `mp_restartgame`; bots fill the teams; a seeded round stream.
- CS 1.6's money, buying in the buy zones (a UMG buy menu), the bomb (plant, beeps, defuse with or without a kit,
  explosion), the HUD's clock, score, money, kill feed, messages and scoreboard.
- **Fix**: characters stay on the floor through long frames.
- 384 engine tests and 28 ShooterGame tests.

### Done — Bots and the waypoint navigation (P20)

([LeonMapping — P20](LeonMapping.md#p20--bots-and-the-waypoint-navigation), [ShooterGame README — Bots](../../Game/ShooterGame/README.md#bots)):

- `UNavigationSystem` on the level's waypoint graph (A*, a capsule sweep and a floor probe for reachability, UE's
  `FindPathToLocationSynchronously`); the grid `FNavMesh` is gone. The map import links the waypoints an agent can
  walk (`bAutoLinkWaypoints`: steps, jumps, drops); de_leon gets 26 links more.
- AIModule: a typed blackboard (UE's `SetValueAs*` / `GetValueAs*`), `UPawnSensingComponent` (sight, hearing of
  `AActor::MakeNoise`), path following that jumps and repaths when stuck.
- ShooterGame's bots: buying, engaging with a reaction time and a settling aim error, planting, defusing, fetching the
  bomb, investigating shots, holding the sites; `Difficulty`; seeded, so a match with `?seed=N` replays.
- 386 engine tests and 33 ShooterGame tests.

### Done — Bot matches, budgets and release 0.20.0 (P21)

([LeonMapping — P21](LeonMapping.md#p21--bot-matches-budgets-and-release-0200), [ShooterGame README — Bot match](../../Game/ShooterGame/README.md#bot-match)):

- `ShooterGame -nullrhi -benchmark -botmatch -rounds=N -seed=N`: a headless bot match at unpaced fixed steps, the
  rules' invariants checked every frame (`FShooterMatchChecker`), exit code 1 when one breaks. The CI of the time (since
  removed) played ten rounds twice and required the same result (a seed replays the match), and a staged Shipping
  build three.
- **Fix**: `AAIController` read a freed path after a stuck repath; it made seeded matches diverge.
- ShooterGame's numbers as the PS2 port's targets in Budgets.md (reflection, UObjects, names, heap).
- 386 engine tests and 34 ShooterGame tests; release 0.20.0 (the content resaved with the new version).

### Done — The audit fixes (0.20.1)

([LeonMapping — 0.20.1](LeonMapping.md#0201--the-audit-fixes)):

- Engine and ShooterGame bugs the audit found (actors destroyed during a visit, zero-length steps, the chase repath,
  the path's goal node, the bomb carrier after a round, plants outside a live round, grenade kill credit, the AWP's
  reward); Core, CoreUObject, PakFile and LeonPak hardening (config arrays and removals saved in the user layer,
  corrupt package counts, pak handles after unmount, `-extract` outside the destination).
- The code without a UE counterpart is gone (the bobbing, orbit and interactable components, `VolumeHelpers`, the
  interaction prompt and menu list widgets, `FAIChaseBehavior`, the basic shape and light helpers, the logic state and
  wish direction of `AAIController`, now UE's `GetMoveStatus`); `APainCausingVolume` hurts the pawns inside it.
- The post-process stack is gone (SSAO, FXAA, the tonemap, the HDR and LDR targets, early-Z, the quality presets):
  the PS2 GS has no programmable pixel stage. `r.ShadowMapResolution` / `r.PlanarReflectionScale` are read from the
  config; F2 and F3 draw the collision and the waypoint graph.
- The bots follow the waypoints' `Jump` and `Crouch` flags; terrorists escort the bomb carrier, a team that outnumbers
  the other hunts it, and a counter-terrorist rotates between the sites. UMG is UE's widget tree (no input), and the
  buy menu is built on it.
- CI ran on every push (since removed; the gates are now local batch files): the format check (G1, clang-format
  20.1.8), TestPAL (run on Win64, built for PS2) and the PS2 ELF sizes (G3).
- 386 engine tests and 42 ShooterGame tests; release 0.20.1 (the content resaved with the new
  version).

### Next

- Measure the PS2 ELFs and TestPAL in PCSX2 again (not measured since P16) and record them in Budgets.md.
- The bots' balance: in eight full matches on de_leon (seeds 1 to 8) the counter-terrorists won seven, and no round
  ended with the bomb exploding (the terrorists die or the bomb is defused first); measured before the escort, the
  hunt and the CT rotation. Grenades are the next step.
- Later: move the character movement code from `ACharacter` into `UCharacterMovementComponent` (UE's
  `PerformMovement`, `MovementMode`, `Velocity`, `CurrentFloor`); a cached `ComponentToWorld`; tick functions.
- Cook follow-ups: `-iterate` (cook only what changed), an asset registry, compressed paks, the PS2 target's formats
  (PSMT8 / PSMT4 textures, `LPS2` v2 meshes, ADPCM) with a pak aligned to 2048 on `cdrom0:` mounted by the PS2
  launch.
- Replication: the ENet networking was removed in 0.12.0 (local tag `archive/net-enet-0.11`); it returns as
  UObject replication (`UNetDriver`, replicated properties) — `Runtime/Engine/Classes/Engine/NetDriver.h`.

## Engine / platform

- Gameplay framework on PS2 (the modules use UE containers and Core math since P6, but Engine still depends on the
  desktop-only Renderer, UMG and AudioMixer); then `Game/ThirdPerson` can use `AThirdPersonCharacter : ACharacter`
  like TP_ThirdPerson.
- Renderer through RHI command lists instead of direct GL calls (the Engine ↔ Renderer cycle is gone since P13); a
  render thread (the scene proxies are the seam).
- `UNavigationSystemBase` seam so NavigationSystem can move to its own module; a navmesh (Recast) if a map ever
  needs more than a waypoint graph (the graph ignores dynamic obstacles; a bot blocked by one repaths).
- The PS2 target platform's formats (the TargetPlatform module's PS2 stub: textures, LPS2 meshes, ADPCM) and a pak on
  `cdrom0:` mounted by the PS2 launch.
- Texture mipmaps on PS2 (GS MIPTBP registers) — fixes floor moiré in ThirdPerson.
- AutomationTool homologue (`RunLAT`: build → cook → stage).

## Debt surfaced by the refactor

- **Game → Launch:** the PS2 game module reads `GEngineLoop.GetMainWindow()` through an include-only
  dependency on Launch; give games an engine-side accessor instead (UE: `GEngine->GameViewport`) once the gameplay
  framework runs on the PS2.
- **Map import (P15):** a `UCX_` piece is its bounding box (no convex hulls in the physics scene) and several merge
  into one box; the importer reads external images only (not those embedded in a `.glb`); light intensities are
  glTF's values as they are; spot lights become point lights.
- **Editor settings:** the factories take their options as properties set from text (ImportList.ini, switches) and the
  import data keeps them as a string map; UE's typed import data classes (`UFbxAssetImportData`, ...) and an import UI
  come with an editor.
- **Render resources:** the GPU copies live in the Renderer's cache keyed by asset, not on the asset (UE's `Resource`
  / `RenderData`), and there is no render thread; a render thread would need UE's resource fences.
- **Console:** commands only come from `-ExecCmds` and `DebugExecBindings`; there is no `UConsole` window or console
  variables (`IConsoleManager`).
- **Viewport:** no Slate; `UGameViewportClient` polls the window's keys and mouse each frame, and the desktop has no
  gamepad mappings.
- **Platform checks in shared code:** the `PLATFORM_WINDOWS` tests in `Core/Private/HAL/MallocAnsi.cpp` and
  `Core/Private/Misc/OutputDeviceRedirector.cpp` should become HAL functions or move under `Private/Windows`.
- **Linux:** not an official platform (Win64 is the development and editor platform, PS2 the target); LeonBuildTool
  registers it, but nothing builds or tests it.
