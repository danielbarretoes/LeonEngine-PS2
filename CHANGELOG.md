# Changelog

All notable changes to **Leon Engine** (formerly Geon) are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project aims to follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

The fourteenth step of the Core / CoreUObject plan (P14): the engine's assets become UObjects saved in `.lasset`
packages, an editor module (LeonEd) imports source files into them through LeonCook's commandlets, and the engine
content is migrated to `/Engine` packages; the legacy `.lmesh` / `.lmat` formats and run-time image and WAV loading
are gone. Behaviour, the golden tests, the `.llev` bytes and the Win64 frames are unchanged; the PS2 ThirdPerson ELF
grows by 8 bytes of alignment, BlankProgram and TestPAL are unchanged.

### Added

- **Asset classes** (P14, Engine, UE 4.27 names and headers; [ASSET_FORMATS.md](Docs/ASSET_FORMATS.md#asset-classes)).
  - `UTexture` (`SRGB`, `UpdateResource`, `ReleaseResource`) and `UTexture2D` (`CreateTransient`, `FTexturePlatformData`
    with mip 0's texels as `FByteBulkData`, `EPixelFormat`).
  - `UStaticMesh`: one LOD of geometry (`FStaticMeshLODResources`, saved as bulk data), the bounds, `StaticMaterials`
    (`FStaticMaterial`) and a `UBodySetup` inner object (`FKAggregateGeom` boxes, `ECollisionTraceFlag`), which the
    physics scene follows.
  - `UMaterialInterface` / `UMaterial`: the material parameters and maps as `UPROPERTY`s with fixed shading models
    (`EMaterialShadingModel`: `MSM_DefaultLit`, `MSM_Unlit`), `GetRenderProxy`, `UMaterial::GetDefaultMaterial`.
  - `USkeleton` (`FReferenceSkeleton`, `USkeletalMeshSocket` sockets), `USkeletalMesh`, `UAnimationAsset` /
    `UAnimSequenceBase` / `UAnimSequence` (per-bone tracks as bulk data), `UBlendSpaceBase` / `UBlendSpace1D`.
  - `USoundBase` / `USoundWave` (PCM16 bulk data, channels, rate, duration), `UDataAsset` and the `UCommandlet` base
    (`Main`, `ParseCommandLine`).
  - `UAssetImportData` (`FAssetImportInfo`: the source file relative to the engine or project, its MD5, the import
    settings; no timestamps), an editor-only instanced `AssetImportData` on textures, meshes, clips and sounds.
- **The editor module `LeonEd`** (`Engine/Source/Editor`; LeonBuildTool's new `Editor` module type, desktop only and
  rejected in game targets; [TOOLS.md](Docs/TOOLS.md#leoncook)).
  - Factories: `UFactory`, `UTextureFactory` (PNG, JPEG, TGA, BMP), `UFbxFactory` (FBX and OBJ static meshes, FBX
    skeletal meshes and animations), `UGLTFImportFactory`, `USoundFactory` (PCM16 `.wav`), `UMaterialFactoryNew`, and
    the temporary `.lmat` / `.lmesh` factories of the migration. A mesh import makes `M_` materials and `T_` textures
    for the named slots of its source; importing over an asset reimports it in place.
  - Reimport: `FReimportHandler`, `FReimportManager`.
  - Commandlets: `ImportAssets` (`-source` / `-dest`, `-importlist=ImportList.ini`, `-reimport -all`),
    `ResavePackages`, `ValidateAssets`, `MigrateLegacyContent` (temporary) and a minimal `Cook` (saves without the
    editor-only data into `Saved/Cooked/<Platform>/`; the dependency walk, target platforms and paks come in P16).
- **LeonCook `-run=`**: `LeonCook [<Project>.lproj] -run=<Commandlet> [arguments]` (UE: `UE4Editor-Cmd`) finds the
  `U<Name>Commandlet` class through reflection; engine-only without a project; an error / warning summary at the end.
- **Engine content as packages**: `/Engine/EngineMaterials/M_Default`, `M_WorldGrid`, `M_SolidMetal`, `T_Default_D`,
  `T_Default_Bump_N`, `/Engine/EngineResources/DefaultTexture` and `/Engine/BasicShapes/Cube`, `Plane`, `Sphere`;
  `Engine/SourceArt/` holds `T_Default_D.png` and `ImportList.ini`.
- **Engine defaults from the config**: `[/Script/Engine.Engine] DefaultMaterialName`, `DefaultTextureName`,
  `DefaultBumpNormalTextureName` and the UI cue sounds `UIClickSoundName`, `UIConfirmSoundName`, `UIBackSoundName`,
  `UIErrorSoundName` (empty: the procedural tones), loaded by `UEngine::InitializeObjectReferences`.
- `UGameplayStatics::PlaySound2D` / `PlaySoundAtLocation` for sound waves; `FAudioDevice` plays PCM16 samples from
  memory (`FSoundWavePCM`, `SetUiSound`).
- `FLegacyAssetKeys` (until P15): the `.llev` material and mesh keys resolve to the migrated packages
  (`ResolveLevelAssetObjectPath`), with a mount point named after a level's content folder outside the mount points.
- **Gate G5**: `Engine/Build/BatchFiles/CheckReimport.bat` reimports the content and fails when git sees a change;
  CI runs it.
- `IRendererModule::ReleaseAssetResources`: the assets free the renderer's GPU copy when their data changes and in
  `BeginDestroy`.
- 23 new tests (340 in all): every asset class saved to a package and loaded back, the factories, the commandlets
  (import lists, reproducible reimport, resave, validation, cook, migration), the engine content, the content keys and
  the scene keeping its proxies' assets.

### Changed

- The components hold their assets through `UPROPERTY`s: `UStaticMeshComponent::StaticMesh`,
  `USkeletalMeshComponent::SkeletalMesh`, `UMeshComponent::OverrideMaterials` (`UMaterialInterface*`; `GetMaterial`
  returns one). A slot without a material draws with the default material.
- The runtime loads the engine's assets from their packages (`LoadObject`); a `.llev` sphere of another tessellation
  is built at run time (`GetSphereMesh`).
- MeshUtilities returns mesh data with its material slots (`FStaticMeshBuilder::BuildFromFile`) instead of writing
  `.lmesh` and `.lmat` files.
- `FGenericWindow::SetIconFromFile(PngPath)` is `SetIcon(Width, Height, RGBA)`: texels, not a file.
- The Renderer's `FRenderResourceCache` is keyed by asset instead of pinning shared pointers, and `FScene` is an
  `FGCObject` that keeps its proxies' assets alive.
- `UAnimInstance` and `UCharacterAnimInstance` moved from AnimationCore to Engine (`Classes/Animation`); AnimationCore
  keeps the plain skeletal data (`FReferenceSkeleton`, `FRawAnimSequence`, `FSkeletalMeshData`) and is no longer
  reflected; the animation tests are `System.Engine.Animation.*`.
- RenderCore's `Material.h` is `MaterialShared.h` and its lighting enum `EMaterialLightingModel` (UE's
  `EMaterialShadingModel` is Engine's); its texture maps are `UTexture2D*`.
- `LoadLevelFile`, `ApplyLevelDocument`, `FBasicShape` and `MeshForBasicShape` lose their resource cache parameter.
- Headless runs (`-nullrhi`, tests) load textures too.
- `.gitattributes` marks `.lpak` files binary.

### Removed

- The `Developer/Cooker` module (the cook commandlet moved to LeonEd), the cook recipes (`FCookRecipe`, `FCookPaths`)
  and LeonCook's `staticmesh` / `recipe` modes.
- `LeonMeshFormat` (`.lmesh`) and `LeonMaterialFormat` (`.lmat`) in RenderCore, the engine's `.lmat` files, and
  run-time image and WAV loading: `stb_image` only decodes in LeonEd's texture factory, and ApplicationCore no longer
  depends on STB.
- `FResourceCache` (and `UEngine::GetResources`), and the transitional `FLegacyAssetLoader` that replaced it during
  P14; `MaterialAsset.h` with the JSON material fields (`PatchMaterialFromJson`, `HasMaterialSurfaceFields`) and its
  test; Engine's dependency on Json.
- `FAudioDevice`'s file-path playback (`PlaySound2D` / `PlaySoundAtLocation` / `PlayMusic` by path, the UI `.wav`
  lookup).

## [0.16.0] - 2026-09-25

Twelfth and thirteenth steps of the Core / CoreUObject plan (P12, P13): the gameplay framework becomes UObjects, owned
through the world and the game instance and freed by the garbage collector at safe points; Leon code builds without
RTTI or C++ exceptions. Levels become actors (static meshes, player starts, volumes, lights, target points and the
world settings), physics bodies come from the components, and the Engine talks to the Renderer only through UE's
render boundary: `FSceneInterface` and scene proxies, `IRendererModule`, `FSceneView` and `FCanvas`. The game then
starts as a UE 4.27 game does: `FEngineLoop` creates `GEngine` (a `UGameEngine` UObject), the game instance opens the
startup map with `UEngine::Browse` / `LoadMap` and picks its game mode with UE's precedence, the local player logs in
through the game mode, input comes from `BaseInput.ini` into the player controller (`FKey`, `UInputSettings`,
`UPlayerInput`, `UInputComponent`), and `UGameViewportClient` routes the input and the console commands and draws the
frame. The PS2 game boots the object system (InputCore's `FKey` is reflected). Behaviour, the golden tests, the
`.llev` bytes and the Win64 frames are unchanged.

### Added

- **The engine object and maps** (P13; plan decision D18).
  - `UEngine` / `UGameEngine` are UObjects (`Config=Engine`) and `GEngine` is the engine: `Init(IEngineLoop*)`,
    `Start`, `Tick`, `PreExit`, `DeferredCommands` / `TickDeferredCommands`, `Exec`, `Browse`, `LoadMap`,
    `SetClientTravel` / `TickWorldTravel`, `ConditionalCollectGarbage`, `AddOnScreenDebugMessage`, `GetWorldContexts`,
    `GameViewport`, `LocalPlayerClassName`, `GameViewportClientClassName`; `IEngineLoop` (`UnrealEngine.h`).
    `FEngineLoop::Init` creates `GEngine` from `[/Script/Engine.Engine] GameEngine=`.
  - `FURL` (`Map?Option=Value#Portal`) and `EngineBaseTypes.h` (`ETravelType`, `EBrowseReturnVal`, `EInputEvent`,
    `EMouseCaptureMode`).
  - `UEngine::LoadMap`: the players leave their controllers, the old world's actors end play (`LevelTransition`), the
    world is destroyed and the garbage collected; the new world (named after the map) loads the `.llev`, gets its game
    mode (`UWorld::SetGameMode(FURL)`), initializes its actors for play (`InitGame`), logs every local player in
    (`ULocalPlayer::SpawnPlayActor` → `UWorld::SpawnPlayActor`) and begins play (`UWorld::BeginPlay`).
  - The game mode is chosen as in UE (`UGameInstance::CreateGameModeForURL`): `?game=` (or an alias of
    `GameModeClassAliases`), the level's `AWorldSettings::DefaultGameMode`, `GameModeMapPrefixes`,
    `GlobalDefaultGameMode`, else `AGameModeBase`.
  - UE's login and restart flow on `AGameModeBase`: `Login`, `InitNewPlayer`, `UpdatePlayerStartSpot`, `PostLogin`,
    `GenericPlayerInitialization`, `InitializeHUDForPlayer`, `HandleStartingNewPlayer`, `RestartPlayer`,
    `RestartPlayerAtPlayerStart`, `FindPlayerStart` / `ChoosePlayerStart`, `SpawnDefaultPawnFor`,
    `FinishRestartPlayer`; `APlayerStartPIE` (the level's camera framing is the Play From Here start); `UPlayer`,
    `ULocalPlayer`; `UGameInstance::StartGameInstance`, `CreateInitialPlayer`, `AddLocalPlayer` / `RemoveLocalPlayer`.
  - The `EngineSettings` module: `UGameMapsSettings` (`GameDefaultMap`, `ServerDefaultMap`, `GlobalDefaultGameMode`,
    `GameInstanceClass`, `LocalMapOptions`, `GameModeMapPrefixes`, `GameModeClassAliases`) and
    `UGeneralProjectSettings`, both `UPROPERTY(Config)` classes.
- **Input by config** (P13, InputCore and Engine).
  - `FKey` is a `USTRUCT` named by an `FName` (config text `Key=SpaceBar`) with `FKeyDetails`; `EKeys` holds the keys
    and is initialized by `FInputCoreModule`. InputCore depends on CoreUObject.
  - `UInputSettings` reads `[/Script/Engine.InputSettings]` of `BaseInput.ini` (`ActionMappings`, `AxisMappings`,
    `AxisConfig`, `DefaultViewportMouseCaptureMode`, `DefaultPlayerInputClass`, `DefaultInputComponentClass`);
    `UPlayerInput` keeps the key state and runs the input stack (`ProcessInputStack`, the `AxisConfig` sensitivity,
    `DebugExecBindings`); `UInputComponent` has UE's `BindAction` and `BindAxis`.
  - `APlayerController` creates its `UPlayerInput` (`InitInputSystem`), its input component, its
    `APlayerCameraManager` (`FMinimalViewInfo`) and its `AHUD` (`ClientSetHUD`, `MyHUD`); `APawn` has
    `SetupPlayerInputComponent`, `AddMovementInput` and `FaceRotation`, and a possessed pawn ticks after its controller.
  - `ADefaultPawn` with `UFloatingPawnMovement` (800 cm/s) is the default pawn: mouse look, WASD and the arrows fly
    along the view, E / Q up and down.
- **The viewport client and the console** (P13).
  - `UGameViewportClient` (`GameViewportClientClassName`) and the main window's `FViewport` (`UnrealClient.h`): the
    keys and the mouse go to the first local player's controller; `Draw` renders the view family, the HUD and the
    on-screen text; screenshots are `FScreenshotRequest`s.
  - Console commands take UE's chain: `ULocalPlayer::Exec` → the viewport client (`show <Flag>`) → the game instance
    → `UEngine::Exec` (`exit`, `obj gc`, `stat unit` / `stat fps`, `RecompileShaders`, `open <map>`) →
    `FSelfRegisteringExec` → the player input, the controller's `Exec` functions (`FOV`), the pawn, the game mode, the
    game state and the world settings. `-ExecCmds="Cmd;Cmd"` queues commands for the first frame; F1–F6 are
    `DebugExecBindings` of `BaseInput.ini`. `FEngineShowFlags` gains `Collision` and `Navigation`.
  - `RHIInit` / `RHIExit` (RHI): `FEngineLoop::PreInit` starts the RHI on the main window's context.
  - `System.Engine.URL.*`, `.EngineSettings.*`, `.LoadMap.*`, `.Input.*` and `.Console.ExecChain`: 318 tests.
- **Levels as actors** (P13, Engine; plan decision D16 for the volumes).
  - `AStaticMeshActor` (root `StaticMeshComponent0`), `APlayerStart` (`CollisionCapsule` 40 / 92, `PlayerStartTag`),
    `ATargetPoint`, `AVolume` (a `UBoxComponent` brush, `EncompassesPoint`, `GetBrushBounds`), `ATriggerVolume`,
    `ABlockingVolume` (blocks by default), `APainCausingVolume` (`bPainCausing`, `DamagePerSec`, `PainInterval`),
    `ALight` / `ADirectionalLight` / `APointLight` with `ULightComponentBase`, `ULightComponent`,
    `ULocalLightComponent`, `UDirectionalLightComponent` and `UPointLightComponent`, and `AWorldSettings`
    (`DefaultGameMode`, `KillZ`; `ULevel::WorldSettings`).
  - The `.llev` reader spawns them (`AWorldSettings` first, then one actor per record in file order, then the lights:
    at most 2 directional and 4 point lights, a default sun when there is none) and the saver writes the same bytes
    back from the actors (`System.Engine.LevelFormat.SaveWritesTheSameBytes`). `ULegacyLevelDataComponent` keeps the
    record fields that have no UE counterpart yet (mesh and material paths, spin, bob, interaction data, the level
    name, the game mode name and the camera framing).
  - `EComponentMobility` and `USceneComponent::SetMobility`; `ECollisionEnabled` and
    `UPrimitiveComponent::SetCollisionEnabled` / `SetSimulatePhysics` / `SetEnableGravity`;
    `FRotationConversionCache` (a component's rotation keeps the exact quaternion it was given).
  - `UGameplayStatics::GetAllActorsOfClass` / `GetAllActorsWithTag`.
- **Physics state from the components** (P13). `UPrimitiveComponent::CreatePhysicsState` adds a body to the world's
  physics scene when collision is on and `DestroyPhysicsState` removes it; `UActorComponent::RecreatePhysicsState`;
  `FPhysScene::AddComponentBody` / `RemoveComponentBody` / `GetBodyOwner` / `SyncComponentsToBodies` /
  `RebuildRigidWorld`.
- **The render boundary** (P13, Engine and Renderer).
  - Engine headers: `SceneInterface.h` (`FSceneInterface`), `PrimitiveSceneProxy.h`, `StaticMeshSceneProxy.h`,
    `SkeletalMeshSceneProxy.h`, `LightSceneProxy.h`, `RendererInterface.h` (`IRendererModule`, `GetRendererModule`,
    `FFrameStats`), `SceneView.h` (`FEngineShowFlags`, `FSceneViewFamily`, `FSceneView`, `FSceneViewInitOptions`)
    and `CanvasTypes.h` (`FCanvas`: tiles, lines and text batched by depth sort key, `Flush_GameThread`).
  - Components create their scene proxies (`CreateSceneProxy`, `CreateRenderState_Concurrent`,
    `MarkRenderStateDirty`, `SendRenderTransform_Concurrent`, `SendRenderDynamicData_Concurrent`) in `UWorld::Scene`,
    which `UWorld::InitWorld` allocates through `IRendererModule::AllocateScene` (null in headless runs);
    `UWorld::SendAllEndOfFrameUpdates` before each frame; `UWorld::LineBatcher`.
  - The Renderer implements `FRendererModule` and `FScene` (primitives and lights in level order) and owns the GPU
    copies of the CPU assets (`FRenderResourceCache`: static and skeletal mesh buffers, textures) and the canvas pass.
  - `FModuleManager::GetModulePtr` / `LoadModuleChecked`; `System.Engine.Components.SceneProxiesFollowTheComponents`.
- **Gameplay framework as UObjects** (Engine, AIModule, UMG, AnimationCore; plan decisions D11, D12).
  - `UCLASS` types with `UPROPERTY` members: `AActor`, `AInfo`, `UActorComponent`, `USceneComponent`, `APawn`,
    `ACharacter`, `AController`, `APlayerController`, `AAIController`, `AGameModeBase`, `AGameStateBase`,
    `APlayerState`, `AHUD`, `UGameInstance`, `UWorld`, `ULevel` (`UPROPERTY() TArray<AActor*> Actors`),
    `UPlayerInput`, `USkeletalMeshComponent`,
    `UCameraComponent` (now a scene component), `USpringArmComponent`, `UUserWidget` and the UMG widgets,
    `UAnimInstance` / `UCharacterAnimInstance`.
  - New components: `UPrimitiveComponent` (render state in the world's primitive list, `GetCollisionShape`),
    `UShapeComponent`, `UCapsuleComponent`, `UBoxComponent`, `USphereComponent`, `UMeshComponent`,
    `UStaticMeshComponent` (a mesh and its materials; attached to a bone socket it replaces
    `FSkelMeshAttachment`), `UMovementComponent`, `UPawnMovementComponent`; `UCharacterMovementComponent` is a
    component holding the tunables.
  - `ACharacter`'s default subobjects: the root `UCapsuleComponent` (`CollisionCylinder`), the movement
    (`CharMoveComp`) and the mesh (`CharacterMesh0`); `GetCapsuleComponent`.
  - `AGameMode` with UE's `MatchState` (`MatchState::EnteringMap` … `Aborted`, `StartPlay`, `StartMatch`,
    `EndMatch`, `AbortMatch`, `OnMatchStateSet` and the `Handle*` hooks) and `AGameState` (`MatchState`,
    `PreviousMatchState`, `ElapsedTime`); `AGameModeBase` gains `GameStateClass`, `PlayerControllerClass`,
    `PlayerStateClass`, `DefaultPawnClass`, `HUDClass` and spawns its game state; controllers spawn their player
    state (`InitPlayerState`).
  - `UWorld::CreateWorld` / `DestroyWorld` (transient `/Temp/Untitled_<N>` package, root set), `SpawnActor(Class,
    Location, Rotation, FActorSpawnParameters)` and the `SpawnActor<T>` / `SpawnActorDeferred` templates with UE's
    spawn sequence (components registered, `PreInitializeComponents`, `InitializeComponents`,
    `PostInitializeComponents`, `BeginPlay`), `DestroyActor` (`Destroyed`, `EndPlay(EEndPlayReason)`, components
    unregistered, removed from the level, pending kill), `SetGameMode`, `GetGameState`, `EWorldType`,
    `ESpawnActorCollisionHandlingMethod`; `FWorldContext` owned by `UGameInstance` (`InitializeStandalone`).
  - Scene components: `SetupAttachment`, `AttachToComponent(Parent, FAttachmentTransformRules, SocketName)`,
    `DetachFromComponent(FDetachmentTransformRules)`, `GetSocketTransform` / `DoesSocketExist` (a skeletal mesh's
    bones are sockets), `GetComponentToWorld`, world-space setters, visibility (`Engine/EngineTypes.h`: attachment
    rules, `EEndPlayReason`, `EWorldType`).
  - Garbage collection at safe points (D11): after the world teardown (`UEngine::LoadMap`, `UGameEngine::PreExit`),
    after a level load, and `UEngine::ConditionalCollectGarbage` after the world tick through
    `FGarbageCollectionTimer` (`gc.TimeBetweenPurgingPendingKillObjects`); `LogSpawn`, `LogWorld`.
  - `FScopedTestWorld` for tests; reflected test fixtures in `Engine/Private/Tests/EngineTestTypes.h` and
    `AIModule/Private/Tests/GameplayTestTypes.h`; 15 new tests (`System.Engine.World.*`, `.Components.*`,
    `.GameFramework.*`).

### Changed

- The level POD `UStaticMeshComponent` is renamed `FLevelStaticMesh`; `ULevel::GetName` / `SetName` became
  `GetLevelName` / `SetLevelName` (the object name is `PersistentLevel`).
- Ownership: the game world belongs to the game instance's world context instead of `AGameModeBase`; the world spawns
  the game mode (`UWorld::SetGameMode`). Since P13 `GEngine` (in the root set) owns the game instance and the viewport
  client; the player controller owns its `UPlayerInput`, `AHUD` and camera manager.
- Actors: the actor transform is the root component's; components are default subobjects or `NewObject` +
  `RegisterComponent` (the actor-level `CreateDefaultSubobject<T>(Args...)` and `RegisterComponent(Component)` are
  gone); `Destroy` goes through `UWorld::DestroyActor` and `Destroyed()` replaces the virtual `Destroy()`;
  `EndPlay` takes an `EEndPlayReason`; `GetRootComponent()` returns a pointer.
- `Cast<>` replaces every `dynamic_cast`; `AHUD::AddWidget<T>()` and `USkeletalMeshComponent::SetAnimInstance<T>()`
  create UObjects (no constructor arguments).
- **No RTTI and no C++ exceptions** in Leon code on every platform (D17): MSVC `/GR-`, no `/EH`, `_HAS_EXCEPTIONS=0`;
  GCC / Clang `-fno-rtti -fno-exceptions`. CMake's MSVC defaults are stripped from `CMAKE_CXX_FLAGS`; third-party C++
  that needs them gets them back with `leon_third_party_cxx_defaults` (tinyobjloader); Jolt keeps its own flags.
- LeonBuildTool: every reflected module also has a `LeonHeaderTool.<Module>` target, which modules with a circular
  dependency on it wait for (UMG waits for Engine's reflected headers).
- Levels (P13): `ULevel` holds only its actors and `AWorldSettings`; `GetAllActorsOfClass<APlayerStart>` replaces the
  player start snapshots, the volume helpers and the navigation read the volume and static mesh actors, and
  `AGameModeBase::FindPlayerStart` returns an `APlayerStart`. Physics bodies and hits carry a `ComponentID` (the
  component's `GetUniqueID`) instead of a level mesh index (`IgnoreComponentID`, `NoComponentID`); the world keeps
  dynamic bodies' components in step (`SyncComponentsToBodies`) instead of copying the level to and from the physics
  scene.
- Assets (P13): `UStaticMesh`, `USkeletalMesh` and `UTexture2D` (now `Engine/StaticMesh.h`, `Engine/SkeletalMesh.h`,
  `Engine/Texture2D.h`), `FResourceCache`, the material assets and `FDebugDraw` / `FDebugOverlay` move from the
  Renderer to Engine as CPU data; their GPU buffers and textures live in the Renderer's cache.
  `FResourceCache::SetGpuUploadEnabled` is `SetTextureLoadingEnabled`. Their messages log as `LogEngine`, and the
  `.lmat` reader's as `LogLeonMaterial` (both were `LogRenderer`).
- Drawing (P13): `UGameViewportClient::Draw` renders a view family through `IRendererModule::BeginRenderingViewFamily`
  and draws the HUD and the debug text through a frame `FCanvas`; UMG's `FPaintContext`, `AHUD::Paint` and
  `FDebugOverlay::Draw` take an `FCanvas`. The show flags (`show Bounds` on F1, `show AxesGizmo` on F6) are the
  viewport client's `FEngineShowFlags`. `EShaderReloadResult` / `MergeShaderReload` move to RenderCore's
  `ShaderCore.h`, `MakeReflectMatrix` / `FitLightSpaceMatrix` to `ViewMatrices.h`, and the `.lmat` document and
  writer to RenderCore's `LeonMaterialFormat.h`.
- Module graph (P13): Engine no longer depends on the Renderer or includes its headers; the Renderer depends on
  Engine; Launch links both. Engine depends on UMG publicly and UMG on Engine circularly (it was the other way round).
  Engine depends on EngineSettings, InputCore on CoreUObject, and ApplicationCore no longer on the desktop RHI
  (Launch links OpenGLDrv).
- Launch (P13): `FEngineLoop` does the whole boot on desktop too: `PreInit` creates the application, the main window
  and the RHI (`RHIInit`; the window no longer creates it); `Init` creates `GEngine` and queues `-ExecCmds=`; `Tick`
  pumps the events, runs the deferred commands and `GEngine->Tick` (windowed frames at most 0.1 s, headless fixed
  steps at `-tick=` Hz); `Exit` calls `PreExit`, collects the garbage and releases the RHI. A headless run honours
  `-ExitAfterFrames=` too.
- `LeonGame` (P13): the map is the first argument (`LeonGame /Engine/LevelTemplates/Blank`) or `-map=`, a long package
  name (`GameDefaultMap=/Engine/LevelTemplates/Starter`), a `.llev` path or a content key, with URL options
  (`?game=<class>`); a map that cannot be opened exits with code 1. The level's game mode string selects the game mode
  (`Default` is the project's default).
- Input (P13): the mouse look is applied once at 0.3° per pixel (`AxisConfig` sensitivity) instead of twice at 0.15°,
  so it turns as fast as before; the look and move axes are summed without clamping (UE). The keys are `FKey`s in the
  window, the PS2 input interface, the PS2 stats overlay and ThirdPerson.
- The PS2 `ThirdPerson` links CoreUObject through InputCore and starts the object system (8 192 slots, 96 KB): its
  text grows by 223 624 bytes ([Budgets.md](Engine/Platforms/PS2/Documentation/Budgets.md)).

### Removed

- `FSkelMeshAttachment` and `USkeletalMeshComponent::AddAttachment` / `GetAttachments` / `GetAttachmentWorldMatrix`
  (unused; attach a `UStaticMeshComponent` to a bone socket instead); `ACharacter::SetCharacterMovement` (the
  movement is a component: edit `GetCharacterMovement()`); `UWorld::SubmitSkeletalDraws` (`SubmitPrimitiveDraws`).
- P13: `FLevelStaticMesh` and the level's snapshots (meshes, player starts, trigger, pain and blocking volumes,
  lights, the camera; `ULevel::Npos`), `AActor::LevelMeshIndex` / `SyncTransformToLevel`,
  `UWorld::RegisterBodiesFromLevel`, `FPhysScene::SyncFromLevel` / `SyncToLevel`, `FWorldGameplayFrameParams::Level`,
  `UWorld::Primitives` / `AddPrimitive` / `SubmitPrimitiveDraws` and every `SubmitDraw`, `UGameEngine::GetRenderer`
  and the Renderer's public `SceneRenderer.h`.
- P13: `ADefaultGameMode`, `ADefaultPlayerController` and `ADefaultCameraActor` (replaced by `AGameModeBase` with
  `ADefaultPawn`), the game mode's `OnEnter` / `Tick(Engine)` / `OnExit` hooks, `UInputMappingContext`,
  `Leon::InputActions`, `FPlayInputTarget`, `RuntimeInput.h` and the input mapping tests, the desktop
  `FGameApplication` (`Launch/Private/Desktop/GameApplication.*`), `FGenericWindow::InitRHI` / `ReleaseRHI`, the
  engine-owned HUD, camera and player input, and `UGameEngine::Initialize` / `InitializeHeadless` / `Shutdown` /
  `Render` / `HandleInput` and its debug toggles (`show` and the Exec chain replace them).

## [0.15.0] - 2026-09-25

Eighth to eleventh steps of the Core / CoreUObject plan (P8–P11): LeonHeaderTool, the UnrealHeaderTool counterpart,
with its LeonBuildTool step; CoreUObject, the `UObject` runtime its generated code runs on; garbage collection,
references, `UPROPERTY(Config)` and `UFUNCTION(Exec)` on it; and packages: UObjects saved to and loaded from
`.lasset` / `.lmap` files. No engine module is reflected yet: the gameplay classes become UObjects in P12, and the
engine's assets and maps become packages in P14 / P15.

### Added

- **Packages** (P11, CoreUObject; plan decision D13; layout in `Docs/ASSET_FORMATS.md`).
  - `.lasset` / `.lmap` files: an `FPackageFileSummary` starting with `'LEON'` (format version `ELeonPackageVersion`
    from Core's `UObject/ObjectVersion.h`, package flags, table offsets, GUID, saving engine version, cooked platform,
    bulk data offset), a sorted name table, the import and export tables (`FPackageIndex`, `FObjectImport`,
    `FObjectExport`), the soft package references, each export's data and the bulk data at the end. One file per
    package: no `.uexp` / `.ubulk`.
  - Saving: `UPackage::SavePackage` / `Save` / `SaveToMemory` through `FLinkerSave`. The exports are `Base`, the
    objects with the top-level flags and, recursively, their outers, inner objects (default subobjects) and the
    package objects they reference; other packages' objects become imports (classes are `/Script/<Module>` imports),
    transient objects are left out and references to them saved as null. Deterministic: the same objects give the
    same bytes on every run and platform (sorted tables, GUID derived from the package name, no timestamps); a golden
    hash checks it on Win64 and the PS2.
  - Loading: `LoadPackage`, `LoadObject<T>`, `StaticLoadObject`, `LoadClass<T>`, `StaticLoadClass`, `FindPackage`,
    `ResetLoaders`, through `FLinkerLoad`. Synchronous: exports are created with `StaticConstructObject` (default
    subobjects are the ones the outer's constructor built, D12) and serialized over their class defaults; an import
    of another package loads it first (circular references work); a missing import is a warning and a null
    reference; a missing or damaged package is an error. `PostLoad` runs at the outermost `EndLoad`, once every
    object of the load is serialized (imported packages first); `RF_NeedLoad`, `RF_NeedPostLoad`, `RF_WasLoaded` and
    `RF_LoadCompleted` are maintained. `FSoftObjectPath::TryLoad` and `TSoftObjectPtr::LoadSynchronous` load
    packages.
  - Tagged properties: `FPropertyTag`, `UStruct::SerializeTaggedProperties` / `SerializeBin`,
    `UScriptStruct::SerializeItem`, and `FProperty::SerializeItem` for every property type (containers, nested
    structs, bitfield bools in the tag, enums by name, object references as `FPackageIndex`, soft paths). Properties
    are saved as a delta against the archetype. Schema evolution: unknown tags are skipped by size, and
    `FProperty::ConvertFromType` converts integers, `float` / `double`, bytes to enums (by value or enumerator name),
    names / strings / text and hard to soft references; anything else is skipped with a warning.
  - `UObject::Serialize` (tagged properties, then the class's native data), `SerializeScriptProperties`,
    `ConditionalPostLoad`, `IsAsset`; `TStructOpsTypeTraits::WithSerializer`; `DECLARE_SERIALIZER`; `RF_Load`,
    `ELoadFlags`, `ESaveFlags`.
  - `FByteBulkData`: payloads stored after the exports (or inline), loaded eagerly.
  - `FPackageName`: long package names, `/Engine/` and `/Game/` mount points plus `RegisterMountPoint`, `/Script/`
    packages, `.lasset` / `.lmap`, file conversions, `DoesPackageExist`. `FLinkerLoad::RegisterInMemoryPackage` lets
    `LoadPackage` read packages from memory (the tests; TestPAL on the PS2).
  - Editor-only data (D14): `PKG_FilterEditorOnly` packages drop `#if WITH_EDITORONLY_DATA` properties; builds
    without editor-only data mark every package they save so, and skip the editor-only tags of an uncooked package.
  - 14 `System.CoreUObject.Package.*` tests: `LeonAutomationTests` runs 293 tests, TestPAL 112 on Win64 and 106 on
    the PS2, which also logs the cost of a 50-object round trip (`Budgets.md`).
- **Core archive hooks** (P11). `FArchive` gains virtual `operator<<(UObject*&)` and `GetLinker()` (no-ops in plain
  archives, as in UE) and `UEVer()` / `LicenseeUEVer()`; `FEngineVersion` in `Misc/EngineVersion.h`.
- **Garbage collection** (P10, CoreUObject `UObject/GarbageCollection.h`, `GCObject.h`; plan decision D11).
  - `CollectGarbage(KeepFlags, bPerformFullPurge)`, `TryCollectGarbage`, `IsGarbageCollecting`,
    `IncrementalPurgeGarbage`: UE4's stop-the-world mark and sweep. Roots are the root set (`AddToRoot`), native
    objects, class default objects and their subobjects, the compiled-in packages, `KeepFlags` objects and `FGCObject`
    holders; the mark follows outers, each class's strong reference properties (`UObject*`, `TSubclassOf`, and
    arrays / sets / maps / structs of them) and `AddReferencedObjects`.
  - UE 4.27 pending kill: `MarkPendingKill` objects are collected even while referenced, and the references to them
    are cleared.
  - Destruction: `BeginDestroy` (out of the name hash) → `IsReadyForFinishDestroy` → `FinishDestroy` → destructor;
    the freed `GUObjectArray` slot is reused and old weak pointers stay stale.
  - `FGCObject`, `FReferenceCollector`, `TStrongObjectPtr`, `UObject::AddReferencedObjects` (per class, through
    `IMPLEMENT_CLASS`), `LogGarbage` statistics, and `FGarbageCollectionTimer` with
    `[/Script/Engine.GarbageCollectionSettings] gc.TimeBetweenPurgingPendingKillObjects`. The engine calls it from
    P12 / P13 (`LoadMap`, round restart, timer).
- **References** (P10). `TWeakObjectPtr` is complete (`Get(bEvenIfPendingKill)`, `IsStale`, `IsExplicitlyNull`,
  comparisons, hashing). `FSoftObjectPath` has UE 4.27's layout and API (`GetLongPackageName`, `GetAssetName`,
  `ResolveObject`, `TryLoad`) and, with `FSoftClassPath`, is a reflected noexport struct whose text form is the path.
  `TPersistentObjectPtr`, `FSoftObjectPtr::LoadSynchronous`, `TSoftObjectPtr` / `TSoftClassPtr` (they load packages
  since P11).
- **Config members** (P10). `UObject::LoadConfig` / `SaveConfig` / `ReloadConfig` for `UCLASS(Config=…)` with
  `UPROPERTY(Config)` and `UPROPERTY(GlobalConfig)`, in the section `/Script/<Module>.<Class>`. Arrays follow the
  layers' `+ - . !` edits (or `Key[N]=`), C arrays read `Key[N]=`, and structs, enums, names, objects, classes and soft
  paths are parsed as text. A class default object loads its config when it is created, reading its parents' sections
  first, and instances copy it. `PerObjectConfig` classes read one section per object. `SaveConfig` writes the
  desktop user layer (`Saved/Config`); on the PS2 it only logs.
- **Console commands** (P10). `UObject::CallFunctionByNameWithArguments` calls `UFUNCTION(Exec)` functions with
  arguments parsed by `ImportText`; `UObject::ProcessConsoleExec`; Core's `FExec`, `FSelfRegisteringExec` and
  `FStaticSelfRegisteringExec` (`Misc/Exec.h`, `Misc/CoreMisc.h`).
- **UObject delegates** (P10). `BindUObject` / `CreateUObject` / `AddUObject` hold a `TWeakObjectPtr`: the binding is
  inert once the object is gone, and multicast `Add` drops dead bindings. Core names the pointer through
  `UObject/WeakObjectPtrTemplatesFwd.h`.
- **Struct text operations.** `TStructOpsTypeTraits::WithExportTextItem` / `WithImportTextItem` let a struct use its
  own text form (`STRUCT_ExportTextItemNative` / `STRUCT_ImportTextItemNative`).
- **LeonHeaderTool `PerObjectConfig`** (`CLASS_PerObjectConfig`) and the `ConfigAndExec` golden case (35 cases).
- **P10 tests.** 21 new `System.CoreUObject.*` tests (GarbageCollection, Delegates, SoftObject, Config, Exec):
  `LeonAutomationTests` runs 279 tests, TestPAL 98 on Win64 and 93 on the PS2. TestPAL logs the GC cost and a final
  collection; the PS2 numbers are in `Budgets.md`.

- **CoreUObject** (`Engine/Source/Runtime/CoreUObject`, every platform, PS2 included; depends on Core only). See its
  `README.md`.
  - Object model: `UObjectBase`, `UObjectBaseUtility` and `UObject`, with UE's object flags, names, outer chains and
    path names.
  - Types: `UField`, `UStruct`, `UScriptStruct`, `UClass`, `UEnum` (with `_MAX`), `UFunction` and `UPackage`. They are
    UE's intrinsic classes, written by hand.
  - Properties: `FField` / `FProperty` and every property type: numeric, bool (including bitfields), byte / enum,
    string / name / text, object / class / weak / soft references, struct, array / set / map. Properties export and
    import text.
  - Objects: `NewObject`, `FObjectInitializer`, `CreateDefaultSubobject` (every instance builds its own subobjects,
    D12), class default objects, `StaticFindObject` / `FindObject`, `MakeUniqueObjectName`, `CreatePackage` and the
    transient package.
  - Storage: `GUObjectArray` with a fixed capacity of `FPlatformProperties::MaxObjectsInGame` (8192 on the PS2,
    131072 on desktop), the name hash and `TObjectIterator`.
  - Casts and references: `Cast`, `CastChecked`, `ExactCast`, `TSubclassOf`, `TWeakObjectPtr`, and minimal
    `TSoftObjectPtr` / `FSoftObjectPath`.
  - Calls: `FFrame`, the `P_GET_*` macros and `UObject::ProcessEvent` over the generated exec thunks.
  - Registration: `RegisterCompiledInInfo` records a module's types, and `ProcessNewlyLoadedUObjects` constructs the
    packages, enums, structs, classes (supers first) and class default objects.
  - NoExport Core structs in `NoExportTypes.h`: `FVector`, `FVector2D`, `FVector4`, `FPlane`, `FRotator`, `FQuat`,
    `FTransform`, `FColor`, `FLinearColor`, `FGuid`, `FIntPoint`, `FIntVector` and `FBox`.
  - 27 `System.CoreUObject.*` automation tests with reflected fixtures in `Private/Tests`. They run in
    `LeonAutomationTests` (258 tests) and in TestPAL on every platform (73 on the PS2).
- **Script containers in Core.** `FScriptArray`, `FScriptSparseArray`, `FScriptSet`, `FScriptMap` and
  `TScriptBitArray` are type-erased views with the exact layout of the Core containers, checked with `static_assert`s.
  Core also gains `TEnumAsByte`, `WITH_EDITORONLY_DATA` (1 on desktop outside Shipping) and
  `PRAGMA_DISABLE/ENABLE_DEPRECATION_WARNINGS`.
- **LeonHeaderTool `USTRUCT(NoExport)`.**
  - NoExport structs are declared inside `#if !CPP` and have no `GENERATED_BODY`.
  - The generated code takes the offsets from the real C++ type and `static_assert`s the declared size, member types
    and offsets against it.
  - There are 4 new golden cases, so `LeonHeaderTool -Test` now runs 34 cases.
- **Module registration hook.** `FModuleManager::StartupStaticallyLinkedModules` calls each module's
  `RegisterReflection`, then `OnProcessLoadedObjectsCallback` (bound by CoreUObject), before `StartupModule`.
- **TestPAL budget lines.** TestPAL links CoreUObject and logs the reflected types, the heap used to construct them,
  the object array and the live object count. The PS2 figures are in `Budgets.md`: reflection is about 220 KB of its
  400 KB budget, and the object array takes 96 KB.

- **LeonHeaderTool** (`Engine/Source/Programs/LeonHeaderTool`) is a std-only C++17 host program with its own
  `CMakeLists.txt` (tokenizer, header parser, type model, code generator, manifest).
  - It reads UCLASS / USTRUCT / UENUM (+UMETA) / UPROPERTY / UFUNCTION, GENERATED_BODY (and the legacy
    GENERATED_UCLASS_BODY / GENERATED_USTRUCT_BODY) and `#if WITH_EDITORONLY_DATA` property blocks.
  - It writes UE 4.27-shaped `<Header>.generated.h` / `<Header>.gen.cpp` and a `<Module>.init.gen.cpp`: the package,
    plus an explicit `RegisterReflection_<Module>()` in place of static `FCompiledInDefer` objects.
  - Errors are printed as `file(line): error: message`. Outputs are only rewritten when they change.
  - `LeonHeaderTool -Test` runs 30 golden cases, 18 of them error cases.
  - The generated-code contract for P9 is in its `README.md`.
- **Host tools tree.** LeonBuildTool builds LeonHeaderTool into `Engine/Intermediate/Build/HostTools/<Host>/` before
  configuring any target, with MSVC on Win64 and g++ inside the ps2dev Docker image. It passes
  `-DLEON_HEADER_TOOL=<path>` to the target configure.
- **Reflection rules.** `Configuration/ReflectionRules.cmake` reflects any module with a header that includes its
  `.generated.h` (and `Private/Tests/**.h` for test targets):
  - it writes a `<Module>.lhtmanifest`;
  - a stamped custom command runs the tool;
  - the `.gen.cpp` files compile into the module;
  - `<tree>/Inc/<Module>` becomes a public include path.

  Only CoreUObject (and its test fixtures, in test targets) is reflected so far.
- **Module table.** `FStaticallyLinkedModuleInfo` gains `RegisterReflection` (`nullptr` for modules without reflected
  types), filled by the generated module table.
- **Runs.** `RunTests.bat`, and therefore the CI win64 job, runs the LeonHeaderTool golden tests after the automation
  tests.

### Changed

- **Default subobject archetypes** (P11). `UObject::GetArchetype` of a default subobject is the subobject of the same
  name in its outer's archetype, as in UE; packages save a subobject's properties against it.
- **PS2 ELF sizes** (P11). ThirdPerson and BlankProgram still do not link CoreUObject, but the two new `FArchive`
  virtuals add their slots to every archive vtable they link: ThirdPerson's text grows by 56 bytes.
- **Object flags at registration** (P10). `RF_MarkAsRootSet` and `RF_MarkAsNative` become the `RootSet` and `Native`
  internal flags when an object enters `GUObjectArray`, as in UE.
- **Reference properties** (P10). `FProperty::ContainsObjectReference` takes an `EPropertyObjectReferenceType`
  (strong by default); `RefLink` lists weak and soft references too, and the collector only follows strong ones.
- **Config strings** (P10). With `PPF_ConfigOnly`, a top-level `FString` / `FText` exports without quotes.
- **Host g++.** The PS2 Docker entry point, the optional Dockerfile and the CI ps2 job install `g++ musl-dev`, the
  host compiler for LeonHeaderTool.
- **Include order.** `.clang-format` keeps a reflected header's `"<Name>.generated.h"` after its other engine includes,
  as LeonHeaderTool requires.
- **PS2 ELF sizes.** ThirdPerson and BlankProgram do not link CoreUObject. The registration hook adds 40 bytes of text
  to `FModuleManager`; ThirdPerson's stripped ELF is unchanged.

## [0.14.0] - 2026-09-25

Fifth to seventh steps of the Core / CoreUObject plan (P5–P7): every module uses Unreal Engine 4.27's Core types, and
the world uses UE's axes and units (X forward, Y right, Z up, left-handed, 1 unit = 1 cm) with UE's view and
projection matrices.

Seventh step (P7): the world moves from Y-up metres with glm's GL matrices to UE's space. Legacy data (`.llev`
levels, version-1 `.lmesh` meshes) is converted where it is read; importers write UE space. Golden tests recorded in
the legacy world before the switch check every step, and `LeonGame` frames match the legacy captures up to float
rounding at texel and shadow edges.

### Changed

- **Axes and units**: the world is X forward, Y right, Z up, left-handed, 1 unit = 1 cm. Every metre constant in
  Engine, AIModule, PhysicsCore, RenderCore, Renderer and the shaders is now in centimetres: movement tuning, capsule,
  step and skin, camera near / far (10 / 10 000) / distance / zoom, spring arm, physics probes and epsilons, navigation
  cells and bands, lights, shadow fit, AO radius / bias, debug draw sizes. Primitives are 100 cm; code that reads a
  scale as a size uses 50 × |Scale| half extents; masses stay in kg.
- Movement, queries, navigation, the camera boom and reflections work on Z up, with UE-style names: `QuerySupportZ`,
  `FloorZ`, `VelXY` / `VelocityZ`, `ClampPositionXY`, `SeparateAabbXY`, NavMesh `OriginY`, `BobBaseZ`,
  `FSceneRenderer::MakeReflectMatrix(PlaneZ)`.
- **Transforms**: components, level data, lights, volumes, player starts and skeletal attachments hold UE's
  `FTransform`; `AActor` stores an `FRotator`; `USceneComponent` has `RelativeLocation`, `RelativeRotation`
  (`FRotator`) and `RelativeScale3D`, and children compose as `Relative * ParentWorld` (a non-uniform parent scale no
  longer shears). A light shines along its rotation's forward axis. Legacy content meshes face +Y, so a character's
  mesh sits at `RelativeRotation.Yaw = LegacyContentYaw` (-90).
- **Legacy data**: `FLegacyCoordinateConversion` (RenderCore) converts `.llev` data and version-1 `.lmesh` meshes at
  load, and the level saver converts back, so `.llev` files stay Y up in metres. Basis UE = 100 × (X, Z, Y) (the
  same swap as UE's glTF importer and ufbx's `left_handed_z_up`); rotations (-X, -Z, -Y, W), tangents (X, Z, Y, -W),
  scale (X, Z, Y). Angles: actor yaw ψ → 90 − ψ; orbit camera (yaw Y, pitch P) → `FRotator(-P, Y + 180, 0)`; free
  look → `FRotator(P, Y, 0)`; light → `FRotator(-P, 90 - Y, 0)`. The Euler extraction is now the true inverse of
  `Rx * Ry * Rz` (the old one inverted `Rz * Rx * Ry`; pure yaw or roll give the same result).
- **Renderer**: render matrices are composed with `FMatrix` operators (UE's row-vector order) and uploaded as they are
  (`FShader::SetMat4(Name, const FMatrix&)`). Views are in UE view space (x right, y up, z forward, left-handed;
  `RenderCore/ViewMatrices.h`: `MakeViewMatrix`, `MakeLookAtView`). `UCameraComponent::ProjectionMatrix()` is Core's
  `FPerspectiveMatrix` (vertical field of view kept) or `FOrthoMatrix` with depth in [0, 1]; the renderer converts it
  to GL clip space last with `ToGLClipSpace` (`RenderCore/GLClipSpace.h`, z_gl = 2z − w), so frustum culling, the
  shadow lookup, SSAO depth and the debug light frustum keep GL clip space. The shadow fit works in a left-handed
  light view, `ssao.frag` follows the mirrored view space, the planar mirror reflects about z = PlaneZ and
  `glFrontFace(GL_CCW)` is explicit (winding is unchanged).
- **Camera and control**: `UCameraComponent` holds a view `FRotator` (`SetViewRotation` / `GetViewRotation` /
  `AddViewRotation`: a positive yaw turns right, a positive pitch looks up, clamped to ±89°). `AController` has
  `ControlRotation`; `APawn` has `AddControllerYawInput` / `AddControllerPitchInput` / `GetViewRotation`;
  `APlayerController` applies look input and clamps the pitch (`ViewPitchMin` / `ViewPitchMax`). Move input is an
  `FVector2D` (X forward, Y right) from `UPlayerInput::GetMoveInput`. `AActor` takes `FRotator`s instead of float
  yaws. `USpringArmComponent` has `TargetOffset` and `SocketOffset` vectors and follows the pawn's control rotation
  with `bUsePawnControlRotation`.
- **MeshUtilities**: `FImportCoordinateConversion` is every importer's last step: OBJ and glTF are right-handed Y up
  ((X, Z, Y) × 100, bit for bit `FLegacyCoordinateConversion`), FBX is right-handed Z up after ufbx resolves the file
  axes ((X, −Y, Z) × the file unit, UE's `FFbxDataConverter`); ufbx no longer scales. The skeletal import conjugates
  the inverse bind pose and the sampled clips (B⁻¹ M B) and builds bone rotations with `FQuatRotationMatrix`; tangents
  are computed after the conversion.
- **`.lmesh` version 2**: writers store UE space; the reader loads version 2 as stored and converts version 1. The
  `Cube.obj` cook identity is now `EFF1459AE46710C6F1B44C0B1ECB2D739CB590F2492B9DF3EC11A03ECA7757C9`, the same data
  as the version-1 cook converted at load.
- **JoltPhysics and AudioMixer** keep their own spaces (Y up, metres) behind a boundary that swaps Y and Z and scales
  by 0.01; Jolt's own constants stay in metres.
- **Gate G4** (`CheckBannedApis.ps1`) bans `LegacyGL`, `FLegacyTransform` and `LegacyAxes` everywhere, tests included,
  and allows `FLegacyCoordinateConversion` only in its own files, the `.llev` and `.lmesh` readers, `Private/Tests`
  and `LegacyGolden.h`. Messages read `<file>:<line>: G4 <rule>: <code> -> <replacement>`; `-Root` scans another tree.
- Tests: 231 (179 after P6). The golden tables are unchanged since they were recorded; the LegacyGLMath and
  LegacyTransform tests became ViewMatrices, RenderMatrices and LegacyCoordinateConversion tests.

### Added

- Golden tests of the legacy behaviour (`System.Engine.Golden.*`: character movement, traces, navigation, spring arm,
  yaw-relative input, orbit and free-look camera NDC, shadow light space, planar reflection, frustum culling, the
  Starter level; `System.AIModule.Golden.AIControllerArrives`, `System.JoltPhysics.Golden.BoxDrop`) with their
  adapters in `Engine/Public/Tests/LegacyGolden.h`; `-GoldenRecord` prints the tables instead of checking them.
- `LeonGame -Screenshot=<file.bmp> [-ExitAfterFrames=N]` saves frame N (default 60) as a 24-bit BMP and exits;
  `FSceneRenderer::ReadFramebufferBgr`.
- Axes gizmo: **F6** in `LeonGame` (or `-AxesGizmo`) draws 1 m world axes at the origin and a view-orientation gizmo
  in the bottom-left corner, X red, Y green, Z blue; `FDebugDraw::AddAxes(FVector or FTransform, Length = 100)`,
  `AddViewAxes(View)`, `FSceneRenderer::SetAxesGizmoEnabled`. Off by default.
- `Docs/TESTING.md` with the manual checklist for the axes and units.
- `RenderCore/Public/LegacyCoordinateConversion.h`, `ViewMatrices.h`, `GLClipSpace.h`,
  `MeshUtilities/Public/ImportCoordinateConversion.h`, `Renderer/Private/RenderMatrices.h` (normal matrix, 2D overlay
  projection).

### Removed

- `RenderCore/Public/LegacyGLMath.h` (`LegacyGL`) and `Engine/Public/Level/LegacyTransform.h` (`FLegacyTransform`).
- `LightDirectionFromRotation` / `RotationFromLightDirection`, the unused model-matrix override of level meshes,
  `UCameraComponent::Orbit` / `AddLook` / `SetYawPitch` / `GetYawDegrees` / `GetPitchDegrees`, the spring arm's boom
  angles and `AActor`'s float yaw overloads.

### Fixed

- `FShadowMap::FitLightSpaceMatrix` put the near plane beyond the corner nearest the light, so the casters closest to
  the sun cast no shadow; the near plane now sits in front of it.
- Win64 builds under a non-UTF-8 console code page recorded no header dependencies (cl.exe's localized
  `/showIncludes` prefix never matched), so header edits rebuilt nothing; LeonBuildTool now configures and builds in
  code page 65001 and restores the caller's.

Sixth step of the Core / CoreUObject plan (P6): Renderer, Engine, AIModule, MeshUtilities, Cooker, LeonCook, the
JoltPhysics plugin and the desktop Launch code use Unreal Engine 4.27's Core types; glm, nlohmann/json and Catch2 are
gone. The world is still Y-up in metres (the Z-up centimetre switch is P7).

### Changed

- **Engine, Renderer, AIModule, desktop Launch**: glm is replaced by `FVector` / `FMatrix`; `std::vector` / `string` /
  `map` / `function` / smart pointers by `TArray`, `FString`, `TMap`, `TFunction`, `TUniquePtr` / `TSharedPtr`.
  Input mapping takes `FName` action names, the debug draw / overlay take `FLinearColor` colors and `FString` text,
  `FResourceCache` returns `TSharedPtr<UStaticMesh>` / `TSharedPtr<UTexture2D>`, mesh bounds are `FVector`, `ULevel`
  uses `TArray` / `FString` and `SIZE_T` mesh indices (`ULevel::Npos`), the navigation A* open set is a `TArray` heap.
- Render matrices keep glm's GL memory layout until P7: the new `LegacyGLMath.h` (RenderCore, namespace `LegacyGL`)
  composes them with `Mul(A, B)` (glm's `A * B`) and builds them with glm's formulas term by term (`Perspective`,
  `Ortho`, `LookAt`, `Translate`, `Rotate`, `Scale`, `QuatToMatrix`, `NormalMatrix3x3`). The Starter level renders
  pixel-identical to the P1 and P5 captures (outside the stats text).
- `FLegacyTransform` moves from Core (`Migration/LegacyTransform.h`) to Engine (`Level/LegacyTransform.h`) on Core
  math, until P7.
- The `.llev` reader / writer uses `FMemoryReader` / `FMemoryWriter` and `FFileHelper` and writes the same bytes; the
  string table stays case-sensitive. The level loader no longer uses try / catch.
- **Renderer**: the `.lmat` reader / writer works on `FString` through `FFileHelper` with its own line parser (same
  rules); `PatchMaterialFromJson` / `HasMaterialSurfaceFields` take an `FJsonObject` from the native `Json` module, and
  missing or mistyped fields keep their value instead of throwing. Renderer depends privately on `Json`.
- **MeshUtilities / Cooker / LeonCook**: the OBJ, FBX and glTF importers take `FString` paths and build `FVector` /
  `FMatrix` data (the skeletal import through `LegacyGL::QuatToMatrix`); vertex dedup uses `TMap`, and cooked `.lmesh`
  files are byte-identical. Files go through `IFileManager` / `FPaths`, cook recipes are read with the `Json` module,
  and the commandlet parses switches with `FCString`.
- **JoltPhysics**: bodies in a `TArray`, allocator and job system in `TUniquePtr`, Jolt's trace hook routed to
  `UE_LOG`; the floor plane tracks "no floor yet" with a flag instead of a NaN.
- iostream, `printf` and `std::chrono` become `UE_LOG` and `FPlatformTime`: new categories `LogEngine`, `LogLevel`,
  `LogPath`, `LogPhysics` (`EngineLogs.h`), `LogRenderer`, `LogMeshUtilities`, `LogCook`, `LogJolt`, `LogLaunch` and
  `LogBlankProgram`. Headless `LeonGame` flushes `GLog` every tick so redirected output stays current.
- Every test is a UE automation test (179: the 90 Engine, Renderer and AIModule cases, the two OBJ import cases and
  the seven Jolt cases migrated). `LeonAutomationTests` runs only `FAutomationTestFramework` and keeps
  `-automation=<filter>`; `RunTests.bat [-automation=<filter>]`. The glm comparison tests became
  `System.RenderCore.LegacyGLMath.Builders` / `Composition` (values glm 1.0.1 printed), and
  `System.Core.Migration.LegacyTransform.*` became `System.Engine.LegacyTransform.*`. Two level-format tests declare
  their expected errors with `AddExpectedError`.
- C++17 on every platform (Win64 and Linux were C++20), like UE 4.27.
- **Core**: `TIsDerivedFrom` takes UE's `<Derived, Base>` order; `FPlatformProcess::Sleep` on Windows and Linux; the
  Windows HAL and OpenGLDrv include `<Windows.h>` through `Windows/WindowsHWrapper.h` (UE's name), which keeps Core's
  `TEXT`. MSVC no longer warns about `alignas` padding (C4324), as in UE.
- `FString ==` ignores case (UE), so exact-case comparisons use `Equals(…, ESearchCase::CaseSensitive)`; the spring
  arm lag uses `FMath::Lerp`, whose last bit can differ from `glm::mix`.
- ThirdPerson keeps its game mode in a `TUniquePtr`; BlankProgram prints through `UE_LOG`.

### Removed

- ThirdParty modules `GLM`, `NlohmannJson` and `Catch2`.
- Core's `Migration/` folder: `GlmInterop.h` (`ToGlm` / `FromGlm`), `LegacyAxes.h`, `LegacyContentPath.h` and
  `LegacyTransform`. `FPaths::ResolveLegacyContentPath` stays until P15.
- The `-noautomation` / `-automationonly` switches and the Catch2 arguments of `LeonAutomationTests` / `RunTests.bat`.

### Added

- `Engine\Build\BatchFiles\CheckBannedApis.ps1` (gate G4): rejects glm, nlohmann, `std::vector` / `string` / `map` /
  `unordered_map` / `function` / `shared_ptr` / `unique_ptr`, iostream and the `printf` family outside ThirdParty, the
  platform HAL sources, Core's `printf` wrappers, LeonHeaderTool and the test program mains. `Lint.bat` runs it after
  the format check; the CI win64 job runs it with `pwsh`.
- `RenderCore/Public/LegacyGLMath.h`, `Engine/Public/Level/LegacyTransform.h`, `Engine/Public/EngineLogs.h`,
  `Core/Public/Windows/WindowsHWrapper.h`.

Fifth step of the Core / CoreUObject plan (P5): the modules below Engine use Unreal Engine 4.27's Core types.

### Changed

- **ApplicationCore**: `GenericApplication::MakeWindow` returns `TSharedRef<FGenericWindow>`; windows own their RHI
  in a `TUniquePtr`, report the cursor as an `FVector2D` and the mouse wheel through the `FOnWindowMouseWheel`
  delegate (`OnMouseWheel`); the GLFW and PS2 backends log through `LogApplicationCore`.
- **RHI / OpenGLDrv / PS2RHI**: `PlatformCreateDynamicRHI` returns an owned pointer like UE's; `LogRHI` replaces
  `printf` / iostream (the PS2 `[Draw3D]` stats line keeps its text); handle ids are `uint32`.
- **Launch**: the engine loop owns the application and the desktop session in `TUniquePtr`; the PS2 stats overlay
  formats with `FCString`.
- **PhysicsCore** (and `FPhysScene` in Engine): `FVector` / `FVector2D` / `TArray` API, `FCapsuleShape` became UE's
  `FCollisionShape`, the body shape enum is `EBodyCollisionShape`, backends are `TUniquePtr` and take `TArray` /
  `FVector`; hits are sorted with a stable sort.
- **RenderCore**: `FVertex`, `FMeshData`, `FMaterial`, `FFrustum` and `TransformLocalBox` use Core math and
  containers (`TSharedPtr` textures); the `.lmesh` reader / writer uses `IFileManager` archives and produces the same
  bytes.
- **AnimationCore**: skeletons, clips, blend spaces and anim instances use `TArray<FMatrix>`, `FName` names and the
  new `FIntVector4` (Core) for bone indices; bone matrices keep the glm memory layout.
- **AudioMixer**: `TCHAR` sound paths, `FVector` listener / emitter, `LogAudioMixer`.
- **SlateCore / UMG**: `FLinearColor` colors, `FText` display text (`UTextBlock::SetText(FText)`), `FName` ids.
- The PhysicsCore, RenderCore and AnimationCore tests are automation tests now (80 automation + 99 Catch2 test
  cases, 179 as before); the tests that need Engine or MeshUtilities moved into those modules.
- Engine, Renderer, AIModule, MeshUtilities and JoltPhysics convert with `ToGlm` / `FromGlm` where they call the
  migrated modules (until P6).

### Added

- `FIntVector4` (UE 4.27's) in `Core/Public/Math/IntVector.h`.

## [0.13.0] - 2026-09-25

Second to fourth steps of the Core / CoreUObject plan (P2–P4): Unreal Engine 4.27's Core foundations, float math and
platform services (files, archives, paths, config, command line) in `Engine/Source/Runtime/Core`, plus native `Json`
and `Projects` modules, on every platform including the PS2.

### Added

- **Build / defines**: `Misc/Build.h` (`UE_BUILD_DEBUG/DEVELOPMENT/SHIPPING` from `LEON_BUILD_<CONFIG>`, `DO_CHECK`,
  `DO_GUARD_SLOW`, `DO_ENSURE`, `NO_LOGGING` in Shipping, `WITH_DEV_AUTOMATION_TESTS`), `Misc/CoreMiscDefines.h`
  (`INDEX_NONE`, `EForceInit`, `ENoInit`, `EInPlace`, `UE_NONCOPYABLE`); `CoreTypes.h` includes both.
- **Characters**: `TCHAR` is UTF-8 on every platform (`TEXT(x)` is `x`; `WIDECHAR` only in the Windows HAL);
  `HAL/Platform.h` adds `LIKELY` / `UNLIKELY`, `PLATFORM_BREAK`, `LEON_PRINTF_FORMAT`.
- **HAL**: `FPlatformMisc` (`LowLevelOutputDebugString`, `LocalPrint`, `IsDebuggerPresent`, `RequestExit`; a forced
  exit halts the EE on PS2), `FPlatformAtomics` (Windows intrinsics, Linux `__atomic`, PS2 generic: one EE thread),
  integer / bit helpers on `FPlatformMath`, `FMath` integer helpers (`Math/UnrealMathUtility.h`), `FMemory` over
  `GMalloc` = `FMallocAnsi` with current / peak byte tracking, `FPlatformProperties::NamePool*` limits.
- **Assertions**: `check`, `checkf`, `verify`, `verifyf`, `checkNoEntry`, `checkNoReentry`, `unimplemented`,
  `checkSlow`, `ensure` / `ensureMsgf` / `ensureAlways` (reported once per call site through `GLog`), `FDebug`.
- **Templates / Algo**: `UnrealTemplate`, `UnrealTypeTraits`, `TypeHash`, `MemoryOps`, `AlignmentTemplates`,
  `TTuple` / `TPair`, `TUniquePtr`, `TSharedPtr` / `TSharedRef` / `TWeakPtr` (`ESPMode::NotThreadSafe` default),
  `TFunction` / `TUniqueFunction` / `TFunctionRef`, `Sort` / `StableSort`, `TOptional`, `ENUM_CLASS_FLAGS`,
  `Algo::IntroSort` / `BinarySearch` / `LowerBound` / `UpperBound` / binary heap.
- **Containers**: allocator policies (heap, inline, fixed, set, sparse array), `TArray` (UE API with heap functions;
  ranged-for catches a resize), `TArrayView`, `TBitArray`, `TSparseArray`, `TSet`, `TMap` / `TMultiMap`, `FString`
  (case-insensitive `==` / `<` / `GetTypeHash` like UE, `Printf`, `ParseIntoArray`, path `/`, `LexToString`),
  `StringConv` (`TCHAR_TO_UTF8` & co. are identities), `FCString` / `FChar`, `FCrc`.
- **Names and text**: `FName` (8 bytes, case-insensitive, numeric suffix, global pool sized per platform; PS2:
  16 KB blocks, 256 KB max, 4096 buckets, exhaustion is fatal); minimal `FText` (`Format` with `{0}` arguments,
  `AsNumber`, `AsPercent`, `Join`; `LOCTEXT` / `NSLOCTEXT` / `INVTEXT` keep the source text).
- **Logging**: `UE_LOG`, `UE_CLOG`, log categories (`DECLARE_LOG_CATEGORY_EXTERN`, `DEFINE_LOG_CATEGORY(_STATIC)`,
  run-time verbosity), `FOutputDevice`, `GLog` (`FOutputDeviceRedirector`), stdout device (EE console / PCSX2 log
  on PS2) and a Windows debugger device. Lines read `Category: Verbosity: Message`.
- **Delegates**: `TDelegate`, `TMulticastDelegate` (static / lambda / raw / SP bindings with payload, safe removal
  during `Broadcast`), `DECLARE_DELEGATE*` / `DECLARE_MULTICAST_DELEGATE*` / `DECLARE_EVENT*`, `FDelegateHandle`.
- **Automation tests** (`Misc/AutomationTest.h`): `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `FAutomationTestBase`
  (`TestEqual`, `TestTrue`, `AddExpectedError`, …), `FAutomationTestFramework::RunTests`; an unexpected error logged
  during a test fails it.
- **TestPAL** program (all platforms): runs the automation tests without Catch2 and prints
  `TestPAL: PASSED (N test(s), 0 failed)` plus memory and name-pool numbers; `RunPCSX2.ps1 -Program <Name>` runs an
  engine program's PS2 ELF. `Engine/Platforms/PS2/Documentation/Budgets.md` records ELF / heap / name-pool numbers.
- **Math** (`Math/UnrealMath.h`, P3): float `FMath` (constants, `Clamp`, `Lerp`, `FInterpTo` / `VInterpTo` /
  `RInterpTo` / `QInterpTo`, `ClampAngle`, `VRand` / `VRandCone`, `LinePlaneIntersection`, `LineBoxIntersection`,
  `ClosestPointOnSegment`, …), `FVector`, `FVector2D`, `FVector4`, `FIntPoint`, `FIntVector`, `FRotator`, `FQuat`,
  `FMatrix` (row vectors, `V * M`) with `FRotationMatrix` (`MakeFromX` & co.), `FRotationTranslationMatrix`,
  `FQuatRotationTranslationMatrix`, `FScaleRotationTranslationMatrix`, `FTranslationMatrix`, `FScaleMatrix`,
  `FInverseRotationMatrix`, `FRotationAboutPointMatrix`, perspective / ortho (normal and reversed Z) and
  `FLookFromMatrix` / `FLookAtMatrix`; `FPlane`, `FBox`, `FBox2D`, `FSphere`, `FBoxSphereBounds`, a scalar
  `FTransform`, `FColor` / `FLinearColor` (sRGB table, HSV, hex) and `FRandomStream`. Automation tests with
  reference values pass on Win64 and on PS2 (TestPAL); desktop tests also compare against glm.
- **Migration bridges** (`Core/Public/Migration/`): `GlmInterop.h` (`ToGlm` / `FromGlm`, desktop, until P6) and
  `LegacyAxes.h` (the Y-up metre world directions, until P7).
- **Platform file layer** (P4): `IPlatformFile`, `IFileHandle`, `IPhysicalPlatformFile`, `FPlatformFileManager`, with
  Win32, POSIX (Linux) and PS2 backends (read-only newlib POSIX on `host:`); `IFileManager` with buffered file
  archives, `FindFiles`, directory iteration, copy / move / delete; `FFileHelper` loads and saves strings and arrays
  (saves write a temporary file, then move it).
- **Archives**: `FArchive` with `<<` for scalars, `FString` (UTF-8), `FName`, `FText`, `TArray`, `TSet`, `TMap` and
  the math types; `FMemoryArchive`, `FMemoryReader`, `FMemoryWriter`, `FBufferArchive`.
- **Command line and app**: `FCommandLine` (every `main` builds it from `argv`), `FParse` (`Param`, `Value`, `Token`,
  `Command`, `Bool`, `Line`, …), `FApp`, `FPlatformProcess` (`BaseDir`, `SetArgV0`, current directory).
- **Misc types**: `FGuid` (`NewGuid`, `NewDeterministicGuid`), `FMD5` / `FMD5Hash`, `FDateTime` / `FTimespan`
  (integer ticks), `FPlatformTime::SystemTime` / `UtcTime`, `FPlatformMisc::CreateGuid`, `BytesToHex` / `HexToBytes`.
- **Config**: `FConfigCacheIni` / `GConfig` with `GEngineIni`, `GGameIni`, `GInputIni`, `GEditorIni`, loading UE's
  layers (engine base, engine platform, project default, project platform, and the desktop-only
  `Saved/Config/<Platform>` user layer that `Flush` writes); `+ - . !` operators, quoted values and
  `-ini:<Name>:[Section]:Key=Value` overrides.
- **Log file and verbosity**: `FOutputDeviceFile` writes `<Project>/Saved/Logs/<Project>.log` on desktop (flushed per
  line, previous run kept as a backup); `FLogSuppressionInterface` applies `[Core.Log]` and `-LogCmds=`.
- **Json module** (all platforms, no third-party code): `FJsonValue` and its subclasses, `FJsonObject`, streaming
  `TJsonReader`, `TJsonWriter` with pretty and condensed print policies, `FJsonSerializer`.
- **Projects module** (all platforms): `FProjectDescriptor` (`.lproj`), `FPluginDescriptor` (`.lplugin`),
  `FModuleDescriptor` (`EHostType`, `ELoadingPhase`, platform allow / deny lists), `FPluginReferenceDescriptor`,
  `IProjectManager`, `IPluginManager` (discovers engine and project plugins).
- `RunPCSX2.ps1` stages the ini files and the `.lproj` beside the ELF (`-NoStage` skips it) and documents that
  PCSX2's host filesystem must be enabled for the PS2 build to read them.

### Changed

- `CoreMinimal.h` includes the new Core set, math included.
- The glm-based `FTransform` is now `FLegacyTransform` (`Migration/LegacyTransform.h`, desktop only);
  `FTransform` is UE's. RenderCore's `FBox` and frustum plane are Core's `FBox` / `FPlane`:
  `FBox::FromLocalTransformed` became `TransformLocalBox`, and the ray test became `FMath::LineBoxIntersection`.
- PS2 modules compile with `-Werror=double-promotion`; `FTicker` converts to its `double` clock explicitly.
- Core's tests are automation tests (`System.Core.*`: 51 on Win64, 43 on PS2), and Json and Projects add their own;
  the other modules keep Catch2 (124 test cases). `LeonAutomationTests` runs the automation tests first, then Catch2, and fails if either fails;
  new arguments `-automation=<filter>`, `-noautomation`, `-automationonly`.
- `FTicker` uses UE's `FTickerDelegate` (a `TDelegate`) and `FDelegateHandle`, with an optional delay; the
  `std::function` API is gone. ThirdPerson logs through `UE_LOG(LogThirdPerson, …)` and ticks through
  `FTickerDelegate::CreateLambda`.
- PS2 toolchain compiles with `-ffunction-sections -fdata-sections` and links with `-Wl,--gc-sections`
  (ThirdPerson text 430 KB → 362 KB).
- `FEngineLoop::PreInit` follows UE's order on every platform: command line, project (`-project=`, a first `.lproj`
  argument or the target's project), config, log file and verbosity, `.lproj` descriptor, then modules. `Exit`
  flushes `GConfig`.
- `FPaths` is rewritten over `FString` with UE's API (`EngineDir`, `ProjectDir`, `ProjectContentDir`,
  `ProjectSavedDir`, `Combine`, …) and no `std::filesystem`; desktop directories come from new generated globals in
  `<Target>.ModuleInit.gen.cpp`, the PS2 uses a staged layout under the ELF folder. `FFileHelper` and `FPaths` build
  on the PS2 too.
- `LeonGame` reads its flags with `FParse`: `-tick=<Hz>` and `-showstats` replace `--tick <Hz>` and `--show-stats`;
  the default map, the window size and the stats default come from the engine config (`GameDefaultMap`,
  `DefaultResolutionX/Y`, `bShowStatsByDefault`).
- ThirdPerson reads `MoveSpeed`, `Gravity` and `JumpSpeed` from `DefaultGame.ini` (compiled defaults when the file
  cannot be read) and logs where they came from.
- Math `InitFromString` uses `FParse`.
- LeonBuildTool re-runs the configure step when `Engine/Build/Build.version` changes, so a version bump reaches
  existing build trees.
- `Launch` depends privately on `Projects`; `TestPAL` depends on Core and Projects.
- PS2: `-fno-threadsafe-statics`, and `PS2PlatformRuntime.cpp` routes the global `operator new` / `delete` through
  `FMemory` and defines the pure-virtual handlers, which keeps libstdc++'s unwinder and demangler out of the ELF
  (ThirdPerson text 561 KB → 442 KB).

### Removed

- `FCString::ToLower(std::string_view)`.
- `FJsonUtils` and the `Json` module's nlohmann dependency (Engine, Renderer and Cooker still use nlohmann until P6).
- `FPaths::ResolveAssetPath` (the legacy loaders use `FPaths::ResolveLegacyContentPath` until P15) and the
  `std::filesystem` code in Core.
- `Math/MathStringParsing.h`.

## [0.12.0] - 2026-09-25

First step of the Core / CoreUObject plan: UE-style descriptor extensions and the removal of the features the
new Core replaces or postpones (environment maps, lightmaps, networking, the pack / session layer and the cooked
skeletal formats).

### Changed

- Project and plugin descriptors use the UE-style extensions `.lproj` (`.uproject`) and `.lplugin`
  (`.uplugin`); LeonBuildTool rejects `.leonproject` with a rename hint.

### Removed

- HDR environment maps (skybox pass, cubemap IBL, `Engine/Content/Hdr/`); `blinn_phong.frag` keeps the
  procedural sky. `.llev` still reads and writes the environment field for compatibility but ignores it.
- `.lm` lightmaps (`LightmapIO`, `uLightmap`); they return as `<Map>_BuiltData.lasset` with static lighting.
- Networking: `NetCore`, vendored ENet, `UNetDriver`, root replication and the listen / dedicated / join flow
  (`--listen`, `--host`, `--join`, `--port`, `--dedicated`). The last networked state is tagged
  `archive/net-enet-0.11`.
- The pack / session layer: `FGameHostSession`, `FWorldRuntime`, `FLevelDirector` (level browser), `FLevelCatalog`,
  `FGameplayRouter`, `FLevelAnimation`, `FProjectDescriptor` packs (`Projects/<Name>/leon.game.json`, `--pack`),
  `FPaths` pack content roots, travel on `UGameInstance` / `AGameModeBase`, `ContentValidator`, `FArenaCamera`
  and the editor ids.
- Cooked skeletal formats (`.lskel`, `.lskm`, `.lanim`, `.lchar`, blend space JSON), their loaders on
  `USkeletalMeshComponent` and the `character` / `anim` modes and recipe steps of LeonCook. The FBX skeletal
  import moved to `Developer/MeshUtilities` (`FbxSkeletalImport.h`); AnimationCore no longer links ufbx.

### Changed (runtime)

- `LeonGame [-map=<.llev>] [-nullrhi] [--tick <Hz>] [--show-stats]` loads one level (default
  `Engine/Content/LevelTemplates/Starter.llev`) and runs `ADefaultGameMode`; `-nullrhi` runs headless.
- `AActor` keeps a spawn serial as `GetUniqueID()` (was the editor id).

## [0.11.0] - 2026-09-24

Restructure to the **Unreal Engine 4.27** layout, architecture and coding standard, built with CMake
through **LeonBuildTool**. Rename tables: `Docs/UnrealEngine427/LeonMapping.md`.

### Changed (breaking)

- **Layout**: modules under `Engine/Source/{Runtime,Developer,Programs,ThirdParty}` with
  `Public/Private/Classes`; the PS2 code is a platform extension (`Engine/Platforms/PS2`, including the
  `PS2RHI` module); Jolt is a plugin (`Engine/Plugins/Runtime/JoltPhysics`, Win64); content in
  `Engine/Content`, GLSL in `Engine/Shaders`; tests in `<Module>/Private/Tests`.
- **Build**: LeonBuildTool (UnrealBuildTool homologue, pure CMake) replaces the root CMake build and
  `Scripts/`: `<Module>.Build.cmake` / `<Target>.Target.cmake` / `.leonproject` / `.leonplugin`,
  `Engine/Build/BatchFiles/{Build,Clean,Rebuild,RunTests,Cook,FormatCode,Lint,GenerateProjectFiles}.bat`,
  `Setup.bat` (pinned third-party downloads), PS2 builds in the pinned ps2dev Docker image, generated
  statically linked module table (`IMPLEMENT_MODULE` / `FModuleManager`).
- **Game**: the only game is `Game/ThirdPerson` (from `Projects/Ps2ThirdPerson`), isolated from the engine
  and built separately (`Build.bat ThirdPerson PS2 Development -Project=...`); primary game module
  `FThirdPersonModule` ticking `FThirdPersonGameMode` through `FTicker`.
- **HAL / ApplicationCore / RHI / Launch**: `FPlatformMemory/Time/Math`, `FStatsOverlay`, `FTicker`,
  `EKeys` (UE names), `GenericApplication` / `FGenericWindow` / `IInputInterface` (GLFW desktop, PS2
  application + window + DualShock input), `FDynamicRHI` / `GDynamicRHI` (`FOpenGLDynamicRHI`,
  `FPS2RHI` static API), `GuardedMain` + `FEngineLoop` driving both the PS2 game and the desktop
  `UGameEngine` session one frame at a time.
- **Naming**: no `namespace leon`; UE type prefixes (`UGameEngine`, `UWorld`, `AActor`, `ACharacter`,
  `FSceneRenderer`, `UTexture2D`, `FPaths`, `UGameplayStatics`, `UCookCommandlet`, …); members,
  functions, parameters and locals in PascalCase with `b` bools (clang-tidy over every translation
  unit); `<MODULE>_API` on public classes; shadowing is a compile error (MSVC and GCC).
- **Formatting**: Epic `.clang-format` (tabs, Allman braces, 120 columns) applied to all engine and game
  C++; `.editorconfig`; formatting commit listed in `.git-blame-ignore-revs`.
- **Tooling / CI**: clangd and VS Code read the root `compile_commands.json`; CI builds ThirdPerson for
  PS2 (ELF artifact) and runs the Win64 build + automation tests.

### Added

- `Docs/UnrealEngine427/` knowledge base (UE 4.27 source layout, key headers, Leon mapping, next steps),
  `Docs/BUILD.md`, `Docs/CODING_STANDARD.md` (replaces `NAMING.md`).
- Config placeholders with UE names (`Engine/Config/Base*.ini`, `Engine/Platforms/PS2/Config/PS2Engine.ini`,
  `Game/ThirdPerson/Config/Default*.ini`), not loaded yet.

### Removed

- Editor, Templates (including the bot content and its tests), `Projects/Ps2Cube`, `Projects/Ps2Lab`,
  `Samples/`, `leon-cli`, the legacy root CMake build and `Scripts/`.

### Fixed

- LeonGame was built without the engine framework (target default `COMPILE_AGAINST_ENGINE` ignored).
- `FHelloMsg` protocol version initialised from itself after the rename (now `CurrentProtocolVersion`).

### Earlier in this cycle (before the restructure; paths and names as they were then)

#### Added

- **Projects/Ps2ThirdPerson** — PS2 third-person gameplay (`leon-Ps2ThirdPerson.elf`): orbit SpringArm camera, Character move/jump, primitive sandbox level; DualShock sticks (left move / right camera) + Cross jump; `Scripts/build-ps2-docker.ps1 tp`
- PS2 `InputPad`: `PollPad`, `GetPadLeftStick` / `GetPadRightStick` (DualShock analog mode)
- **Projects/Ps2Cube** — PS2 3D scene (`leon-Ps2Cube.elf`): ViewTarget, DirectionalLight, `M_*` / `T_*_D` materials+textures, FPS/ms HUD; `Scripts/build-ps2-docker.ps1 cube`
- PS2 RHI 3D: `Ps2DrawBox`, `Ps2Texture`, `Ps2Material` (`BaseColor` / `BaseColorMap` / `EShadingModel`), `Ps2SetViewTarget` / `Ps2SetDirectionalLight`, `Ps2DrawDebugHudText`; GS z-buffer in `Ps2InitDisplay`
- Editor viewport **View Mode**: Lit / Player Collision (Alt+5/6); camera-following Show Grid (View menu); `Renderer::SetSceneGeometryEnabled`
- Editor Play: Number of Players + Net Mode; multiplayer via Shipping `--listen`/`--join` + `--map <LevelKey>`; AssetTools rename/move/delete; Content Browser click UX
- Pack `GameplayLib.cmake` + `RegisterModes` / `GameHostSession`; templates copy fixed type names (no `{{NAME}}` rename)
- Unreal-lite match framework: `GameMode::PrepareMatchWorld` / `RebuildNavigation` / `SnapCharacterToFloor` / `EstimateFloorY`; `Character` health (`TakeDamage`/`Die`/`Revive`); `ApplyPointDamage` / `ApplyRadialDamage`; `VolumeHelpers`; `GameplayStatics` traces; `ArenaCamera`; `InteractionPromptWidget`; `PlayerController` button latches; `PlayerState` Lives
- `.llev` **v2**: `TriggerVolume` / `PainCausingVolume` / `AISpawnPoint` (reader still accepts v1); Editor Place/Details/Outliner/Viewport + **Build Paths** (NavMesh bake); PIE ticks pain volumes
- Net protocol **v4**: `InputCmdMsg::buttons` → `uint16` (16 bits); `InputButtons::Scoreboard` / `Sprint` aliases
- **Projects/ThirdPerson** — fixed Unreal-style sample pack (`ThirdPersonCharacter`, no rename)
- **Templates** — New Project copies the template folder as-is (fixed `ThirdPerson*` / `Blank*` types); stamps `name` / `displayName` / `templateId` only
- Net protocol **v3**: `ENetMsg::Rpc` + `RpcHeader` / `EncodeRpc` / `DecodeRpc` (`ERpcId::Notify`); pack AI `AIChaseBehavior` + snapshot AI relevancy cull; RHI opaque ids on PostProcess/EnvMap/meshes/UBO
- Travel E2E + editor-style level save/load headless tests; `Samples/README.md` points at ThirdPerson pack
- **Projects/Furytoon** — party fighter (War of the Whiskers lite): Menu→Lobby→Kitchen; 3D move + double jump; LMB/RMB light/heavy combos; AI bots fill to 4 fighters; stocks/KO; shared top-down arena camera (midpoint + auto zoom); Cube/Sphere mesh fighters; `leon-Furytoon-server`
- `kMaxPlayers` raised to **4**; listen host accepts `kMaxPlayers-1` remotes; `CharacterMovement.MaxJumpCount` (double jump)
- Net protocol **v2**: `InputCmdMsg` locomotion + `buttons` (`InputButtons::*`); `SnapshotHeader` + optional `SnapshotMatchMeta` ext; `PawnSnap` user payload; `AcceptInboundPacket` / `SanitizeInputCmd`; `SendTravelToPeers`; host `PeerPacketWindow` rate-limit
- `ActorComponent` base (`SceneComponent` derives); `RegisterComponent` / `CreateDefaultSubobject`; World ticks components
- **Projects/Zombies** — co-op FPS COD Town loop: Menu→Lobby→Town; points / doors / wall guns / perks / PaP (lava risk); typed Trigger/Pain/AISpawn volumes; round waves; hold-LMB fire + R reload + F interact; COD-style HUD; tracers + F2 traces; `leon-Zombies-server`
- NavigationSystem + grid NavMesh (Unreal-lite, no Recast): bake from static PhysScene AABBs; `FindPath` / `ProjectPointToNavigation`; `AIController` follows waypoints when nav is set; CoopTp builds nav on match prepare
- F3 toggles NavMesh debug draw (walkable green / blocked red); HUD stats moved to F4
- CoopTp arena ramps: rotated Cube prisms with TriangleMesh collision (no separate SlopePlane — avoids yaw/sign mismatch); F2 draws TriangleMesh wire tris (cyan) instead of the fat world AABB; skip fat AABB side-resolve for TriangleMesh
- Planar mirror reflection pass draws skeletal characters; mirror plane uses mesh AABB top
- CoopTp AI spawn plate: orange `AISpawnPlate` cube; stand on it once to spawn 2 chase bots; AI pawns use snapshot slots `kMaxPlayers+` (`kMaxAiPawns` / `kMaxSnapshotPawns`) so clients see them
- `AIController::MoveToActor` chase helper; CoopTp authority AI wave via plate (`SpawnAIWave` / `AIController::Possess`)
- CoopTp: Main Menu **Join Dedicated**; shipping `leon-CoopTp-server` (headless `LEON_DEDICATED_DEFAULT`); `build-project.bat/.sh --with-server`; Project Settings `buildDedicatedServer`
- HUD widgets: `ProgressBarWidget` (UProgressBar lite) and `ImageWidget` (UImage lite / solid tint); CoopTp Main Menu uses Image backdrop + join ProgressBar
- Jolt narrow-phase traces: when `HasNarrowPhaseTraces()`, PhysScene Line/Sphere/Capsule Multi delegate to CastRay/CastShape; Arcade still appends floor plane + slopes; tests `[physics][jolt][trace]`
- Content audio cues: `assets/Audio/UI/*.wav` preferred by `PlayUiSound` (procedural fallback); `PlayMusic`/`StopMusic` looping bed; CoopTp menu/lobby `MenuBed.wav`
- GPU RHI accessors PascalCase (`Shader`/`Texture`/`StaticMesh`/`EnvMap`/targets: `Create`/`Destroy`/`Valid`/`Bind`/`Set*`)
- `World` / `GameMode::SetPhysicsBackend`: opt-in Jolt for match dynamics (CoopTp match uses Jolt when linked; default World stays Arcade); incremental Jolt prepare + MeshShape statics
- Jolt Physics backend (`LEON_WITH_JOLT`, FetchContent v5.3.0): `Plugins/Physics/Jolt`, `PhysScene(EPhysicsBackend::Jolt)` drives rigid-body Step; incremental `RigidPrepareStep` (no rebuild each frame); static `TriangleMesh` → Jolt `MeshShape` on `SyncFromLevel`; CMC stays Arcade; tests in `JoltPhysicsTests`
- Arcade triangle-mesh collision (Unreal ComplexAsSimple lite): `ECollisionShape::TriangleMesh`, `TriangleMeshCollision` / `SegmentTriangle*`; static CPU meshes bake on `SyncFromLevel`; Line/Sphere/Capsule traces + `QuerySupportY` refine vs tris; tests in `TriangleMeshCollisionTests`
- Unreal-like `AudioDevice` (miniaudio): `PlaySound2D` / `PlaySoundAtLocation` / `PlayUiSound`; CoopTp menus play click/confirm/back/error cues
- Unreal-like HUD widgets: `ButtonWidget` (UButton lite) and `VerticalBoxWidget` (UVerticalBox lite); CoopTp Main Menu / Lobby / Pause use VerticalBox + Buttons

#### Fixed

- Character capsules collide with each other (players / AI) via pairwise XZ depenetration in `World::TickGameplayFrame`
- NavMesh bake: ignore wide floor slabs / `Plane` (not plates); force-block `AISpawnPlate`; `SlopeRamp` stays walkable so AI can path/climb (CMC); TriangleMesh blockers use XZ tri footprint instead of fat world AABB
- AIController: tight waypoint arrive (ignore large goal `arriveRadius` for path corners) so chase no longer shortcuts through plates/ramps; no straight-line fallback when a NavMesh is set; CoopTp agent radius 0.45
- CMC walk shove: apply Dynamic push from capsule sweep hits (SafeMove stops at skin so ResolveCapsuleSides never saw contact; jump-side overlap was the only accidental path)
- Dedicated / headless console logs: replace Unicode dashes/arrows with ASCII (`--`, `->`) so Windows cmd does not mojibake UTF-8
- Dedicated server exe opened a window: `LEON_DEDICATED_DEFAULT` on `*-server` never reached `GameApplication` in `leon_runtime`; pass `dedicatedByDefault` from pack `main` (also accept `--server`)
- CoopTp spawn: SyncFromLevel before RestartPlayer; floorY/walkBounds from PlayerStarts + mesh extents; QuerySupportY snap so Rooftops no longer drops pawns to world Y=0
- CoopTp MakeCoopLevels: synthesize MainMenu/Lobby (plane + PlayerStart); snap arena PlayerStarts to floor plane
- CoopTp Join: suppress ghost menu activate after travel (Connecting… until Welcome; 350ms lockout); GameplayRouter re-syncs GameMode same frame after ClientTravel
- `Scripts\smoke-coop-dedicated.bat` — dedicated headless smoke (Courtyard); headless `std::cout` uses unitbuf so redirected logs survive process kill

#### Changed

- Renderer / DebugOverlay / DebugDraw public frame API → PascalCase (`Initialize`, `BeginFrame`, `DrawScene`, `GetDebugOverlay`, …); PIE uses project `defaultGameMode` when the level has no GameMode override
- Dropped obsolete path fallbacks: `games/` projects root, lowercase `engine/` asset roots, and pre-Content `Templates/ThirdPerson/assets/`
- Esc no longer quits the runtime (close window / Quit Game); CoopTp **Esc**/**Backspace**: pause/resume in match, back (or Quit on Main Menu) in menus
- CoopTp Main Menu: **Host Game** / **Join Game** (default `127.0.0.1`; `--join <ip>` for LAN); dedicated via `--dedicated` only
- Default post-process quality is **Low** (light SSAO, no FXAA, 1024 shadows) instead of Medium
- `RunLeonGame` / `GameApplication::Run` register callback is now `(Engine&, GameplayRouter&)` so packs can `SetGameInstance<T>()` before modes run
- CoopTp: removed `CoopSession` singleton (session lives on `CoopGameInstance`); soft map re-enter renamed `OnTravelFinished`
- Coop polish (UE-like): single session source on `CoopGameInstance`; match gate via `GameState::HasMatchStarted`; `FinishClientJoin` unifies Welcome/Travel; shared `CoopNet::SendTravelToPeers`; `GameState::Reset` no longer clears `PlayerArray`; guarded `LoginPlayer` Logout

#### Added

- Character CMC lite (phase 0–5): `FindFloor` / `WalkableFloorZ`; capsule sweep + slide; `tryStepUp`; `EMovementMode` + `AirControl`; `AddSlopeRamp` / steep reject; UE-like tunables (`MaxWalkSpeed`, `JumpZVelocity`, `MaxStepHeight`); tests in `CharacterMovementTests`
- Unreal-like coop framework: `GameState::PlayerArray` (`Add`/`Remove`/`GetNumPlayers`), `GameMode::HandleStartingNewPlayer`, `CoopGameInstance` (session survives travel), `CoopTpPlayerState`, `LoginPlayer` → PostLogin → RestartPlayer; Lobby `ServerTravelToMatchMap`; Tab scoreboard reads PlayerArray
- CoopTp: hold **Tab** for in-match player list (`TextBlockWidget` scoreboard) on listen host, client, and standalone
- Unreal-lite HUD API: `HUD::AddWidget` / `RemoveWidget` / `Tick`, `UserWidget` Construct/Tick/Paint, `WidgetPaintContext::DrawText`, `MenuListWidget`, `TextBlockWidget`; CoopTp menus use `GetHUD()` widgets
- Editor **Project Settings** panel (Edit / Window): Game Default Map, Default GameMode, display name, description — pack-wide `leon.game.json`; **World Settings** stays per-level only
- Unreal-aligned gameplay API (no legacy aliases): `GameState` (`HasMatchStarted`, `GetServerWorldTimeSeconds`, `GetNumPlayers`, `GetMapName`, `HandleMatchHasStarted/Ended`), `GameMode` (`StartMatch`/`EndMatch`, `ServerTravel`/`ClientTravel`, `FindPlayerStart`, `GetGameState<T>`), `GameInstance::ServerTravel`/`ClientTravel`; `LevelCatalog`/`LevelDirector` PascalCase; CoopTp uses `CoopTpGameState` + seamless travel helpers
- `Projects/CoopTp` LAN 2P: **MainMenu** → **Lobby** (Courtyard/Rooftops) → match; Listen as Host / Join / Dedicated (or `--dedicated`); Esc pause → Continue / Exit to Main Menu; Welcome/Travel sync maps. Pack GameModes are not linked into the Editor (PIE uses Default/`third-person` only; `leon.game.json` `gameModes` lists authoring ids)
- Welcome: Open Project picks a **folder**; recent list shows last Engine version; stamps `engineVersion` in `leon.game.json`
- Forward post stack: HDR `SceneColorTarget`, half-res SSAO + bilateral blur, ACES tonemap/exposure, FXAA (`SetPostProcessQuality` Off/Low/Medium/High; optional Early-Z; `GpuPassTimer` Ssao/Post)
- `SpringArmComponent` collision probe (`bDoCollisionTest`, sphere sweep vs PhysScene) so third-person camera pulls in instead of clipping meshes
- `SetActiveContentRoot` / `ActiveContentRoot` and `WriteFileAtomic` / `WriteTextFileAtomic` (`leon/core`)
- Editor toolbar **Stats** toggle (FPS / ms overlay on Viewport); View → Show Stats
- Editor layout persistence (`editor_layout.ini` beside exe): Window → Save Layout / Reset Layout
- Editor applies built-in dock layout when no usable saved layout exists (waits for viewport size; ignores incomplete ini)
- Content Browser Contents: folder tile icons, full-mesh thumbnails; drag material onto Viewport mesh shows live preview and applies on drop
- Content Browser defaults to Contents view (gallery) instead of Hierarchy
- Editor **Build → Build Project** (also File + Ctrl+B): cmake-builds the open game pack via `Scripts/build-project.bat`; log in Output Log
- Build Project resolves repo root by walking parents (fixes `build/Release` depth); Output Log / Console Copy + Ctrl+C + right-click
- Portable packaging: `Scripts/package-editor.bat` → `Dist/LeonEditor/` (editor + SDK); **Build Game** exports `<project>/Shipping/`
- Build menu is **Build Game** only (`<project>/Shipping/`); editor packaging stays a Scripts/`package-editor.bat` step
- Editor branding under `Editor/Resources/Brand/` + `Icons/` (`LeonLogo.png`, `LeonLogoUi.png`, `LeonEditor.png` / `.ico`); Welcome + window / taskbar
- Regenerated `LeonEditor.ico` as BMP/DIB (not PNG-in-ICO) so `rc.exe` embeds the lion; `Scripts/make-editor-icon.py`
- Editor UI: Inter font (`assets/fonts/`), World Settings GameMode dropdown, Details material combo + sphere preview, Content Browser Hierarchy/Contents view with path breadcrumbs and tile previews
- Binary Leon Level format `.llev` (`LLEV`, little-endian, version 1) — `LeonLevelFormat.h` / `.cpp` with `LevelDocument`, `BuildLevelDocument`, `SerializeLeonLevel` / `DeserializeLeonLevel`, `SaveLeonLevelFile` / `LoadLeonLevelFile`, `ApplyLevelDocument`
- Editor New Level templates under `Engine/Assets/LevelTemplates/` (`Blank`, `Starter`)
- Project templates under `Templates/` (`Blank`, `ThirdPerson`); distinct from level templates
- Engine BasicShapes place with textured materials (`M_Default` / `M_WorldGrid` → `Textures/T_Default_D.png`)
- `LightmapIO` (Engine): runtime `.lm` load; `LoadLevelFile` hydrates lightmaps after commit
- Stable `lightmapId` → bake files `Lightmaps/LM_<id>.lm` (reorder-safe)
- Level camera `eye` / `mode` (`Orbit` \| `FreeLook`)
- Level authoring docs: [`Docs/LEVELS.md`](Docs/LEVELS.md)
- Fast editor builds: `Scripts/build-fast.bat`, `Editor/pch.h`, `Build/LeonCompileOptions.cmake` (`/MP`), `Build/SyncDirectory.cmake`
- Session-stable editor selection (`editorId`, `ResolveSelectionIndices`)
- Skinned shadow pass (`skinned_shadow_depth.vert`)
- [`Docs/LIBRARIES.md`](Docs/LIBRARIES.md)
- [`Docs/NAMING.md`](Docs/NAMING.md): folder / file / type / include / CMake conventions (Engine → Runtime → Editor → Templates → Tools)
- [`Docs/TOOLS.md`](Docs/TOOLS.md): offline cook / CLI / ResourceTools (lean `leon_engine_cook`, recipe schema)

#### Fixed

- `ResolveAssetPath` no longer picks the newest file across all `Projects/*` (cross-pack leaks); uses `SetActiveContentRoot` (Editor open project / runtime pack) then Engine/staging
- `--dedicated` uses `initializeHeadless` + `runHeadless` (no OpenGL window); WorldRuntime skips overlay chrome when headless
- `.llev` / `.lskm` / `.lanim` deserializers reject oversized counts before `reserve`/`resize`
- Level / material / lightmap saves write via temp + rename (`WriteFileAtomic`)
- Editor import targets open project `Content/assets/`; dependency copy failures surface as warnings
- Editor undo/redo applies snapshots before mutating history stacks
- Details material picker guards null `ResourceCache`
- `.lmat` parser logs bad/unknown keys instead of silent defaults
- Lightmap bake reports mkdir / `.lm` write failures (no false “saved under lightmaps/”)
- `package-editor.bat`: unique TEMP keep-dir; required vs optional robocopy sources
- Docs: `LeonEngine.exe` path; ThirdPerson cook example under `Content/assets/`
- `.gitignore`: `**/_deps/`, `Projects/*/build-fast/`, `Tools/build-fast/`, `Dist/`
- **Build Game**: resolve `LEON_REPO_ROOT` via walk-up / `-DLEON_REPO_ROOT` (projects under `Editor/build/*/Projects/` no longer point at a fake root); prefer real repo `Projects/` over staged beside-exe copy; report failure via `LEON_BUILD_EXIT` (was false “succeeded” when cmake failed)
- **Build Game** POST_BUILD: force-refresh `LEON_ENGINE_ASSETS` from repo root (stale CACHE pointed at `Editor/build/Release/Engine/Assets` and broke Shipping sync)
- Shipping / runtime: `ResolveAssetPath` finds staged `<exe>/assets/` for paths like `Hdr/…` (skybox HDR was packaged but not resolved)
- Shipping parity with Viewport / PIE: HUD stats off by default (F4 / `--show-stats`); no MSAA on game window; hide level-switcher chrome when pack has one level
- Build Game: show + focus Output Log; toast on start / success / failure
- Project layout: Unreal-like `<project>/Content/` (`levels`, `materials`, `assets`); Content Browser roots there (legacy `<project>/levels` still loads)
- Editor product exe renamed to `LeonEngine.exe` (CMake target still `leon-editor`); portable package: `Dist/LeonEditor/LeonEngine.exe`
- Dist/editor: resolve `Materials/…` under `Projects/<name>/Content/` (Details preview no longer looks beside the exe)

#### Changed

- `PlayInputTarget` (`leon/core/PlayInputTarget.h`): groups PIE / multi-window play input (window override + mouse-look gate); `Engine` convenience API unchanged
- Scripts: shared `Scripts/_vsenv.bat` (vcvars/vsdev); Editor trees documented (`build-fast` vs `build-ninja` vs `build`); `format`/`lint` include `Templates/`; PIE/`editorId` contract in ARCHITECTURE
- `leon_import` (`Engine/Import`, `leon/import/`): OBJ/FBX/glTF load + staticmesh cook; linked by Editor + `leon_engine_cook` only — not shipping `leon_engine`. `ResourceCache` accepts `.lmesh` only
- Lightmap bake is Editor-only: `leon::editor::BakeLevelLightmaps` (`Editor/SceneEditing/LightmapBaker.cpp`); Engine keeps `LightmapIO` load for shipping (Unreal-like Build Lights vs runtime)
- Runtime is opt-in: Engine no longer `add_subdirectory(Runtime)`; Projects/Templates (and Editor Blank fallback) add it after Engine so Editor/Tools do not build `leon_runtime`
- Docs: ARCHITECTURE dependency diagram — Editor does not depend on Runtime (PIE is Editor-owned)
- Editor Selected Viewport PIE hides and locks the cursor (`GLFW_CURSOR_DISABLED`) so mouse look is not clamped by screen edges; Esc / Pause restores it
- Editor New Window PIE ticks after ImGui so skinned draws are not cleared by panel `drawScene`; Play controls only in Toolbar (removed menu Play strip)
- Selected Viewport PIE blocks ImGui mouse (`NoMouse` + scrubbed mouse state) so hidden cursor cannot hover/click editor chrome
- New Window PIE renders on the editor GL context into a shared texture, then blits to the play window (fixes black screen from non-shared FBOs/VAOs)
- Default editor docking: large Viewport, right column Outliner|World Settings + Details, bottom Content Browser|Output Log
- World Settings: GameMode dropdown is `Default` / `third-person` only (removed confusing `Pie` id); Skybox dropdown lists Engine + project HDRs
- Content Browser: material sphere thumbs in Contents gallery; Create Material/Folder via right-click (Unreal-like), not toolbar
- Docs: [`Docs/ARCHITECTURE.md`](Docs/ARCHITECTURE.md) covers templates, BasicShapes, PIE, build helpers; naming points to NAMING.md
- Docs: [`Docs/SETUP.md`](Docs/SETUP.md) documents full `Scripts/` table, Tools/`leon-cli cook`, format skip rules, lightmap shipping tip, **clangd / compile_commands** (MSVC path escapes + Tools DB)
- Docs: [`Docs/ASSET_FORMATS.md`](Docs/ASSET_FORMATS.md) documents `.lm` lightmaps; cross-links [LEVELS](Docs/LEVELS.md) / [TOOLS](Docs/TOOLS.md)
- Docs: [`Docs/NAMING.md`](Docs/NAMING.md) § Tools — ResourceTools live (`leon/tools/`); cook links `leon_engine_cook`
- Docs: [`Docs/TARGET_ARCHITECTURE.md`](Docs/TARGET_ARCHITECTURE.md) is a stub pointing at ARCHITECTURE
- Scripts: `configure-ninja.bat` also exports `Tools/build-ninja` compile DB; `fix-compile-commands.ps1` for clangd
- `.clangd` / `.vscode`: QueryDriver for `cl.exe`; Tools PathMatch; quieter Catch2 clang-tidy noise
- Runtime headers at `Runtime/include/leon/runtime/`; entry `leon::runtime::RunLeonGame` only
- Engine free functions PascalCase only (camelCase aliases removed)
- Enums: `EShadingModel`, `EShaderReloadResult`, `GpuPassTimer::EPass`, `EValidationSeverity`
- Editor types in `namespace leon::editor`; `EditorFileDialog.h`; `LightmapIO` directly
- Tools folder `Cli/` (was `CLI/`)
- Levels are binary `.llev` everywhere: `LoadLevelFile` rejects any other extension, `LevelCatalog` scans `*.llev`, Editor save / undo-redo / drag-drop / templates and the project scaffold all use the binary format; all source levels migrated
- `ValidateLevelDocument` takes a decoded `LevelDocument` (asset references + value ranges); magic / version / class enums are enforced by the reader
- `LevelSaver`: `LevelToJson` / `loadLevelFromJson` replaced by `SerializeLevelSnapshot` / `LoadLevelSnapshot`
- Scripts: correct VS edition discovery; format/lint skip build trees; `cook.bat` Ninja→VS fallback; `leon-cli cook` runs sibling `leon-cook`
- Tools: `leon-cook staticmesh --fbx`/`--gltf` + recipe `staticmesh`; `leon-cli` no longer links Engine; cook POST_BUILD uses `SyncDirectory`
- `leon_engine_cook` INTERFACE (lean cook deps); `leon_resource_tools` (`RunCookRecipeFile` / `ResolveBeside`)
- Level class-name parsers moved to `Content/LevelClassNames.cpp` so cook/validator need not link Scene gameplay stack
- `PieGameMode` under `Editor/Gameplay/` (not Engine / not shipping)
- Unreal-first PascalCase on Core, `GameplayRouter`, `LevelCatalog` / `LevelDirector` (aliases kept)
- `ResolveProjectsDirectory`, `IsPendingKill`; deque anim storage; deferred `World` spawn
- PhysScene mesh AABB sync; Jolt → Arcade fallback; NetDriver ENet refcount + Hello magic
- GLFW init refcount; PIE scroll window; dirty gate Close/Exit; FreeLook camera JSON
- `GpuPassTimer` non-blocking resolve; Renderer honors `shaderDirectory`

#### Fixed

- BlendSpace pointer invalidation; Prop/Pawn empty-mesh load; skeletal shadow uniforms
- clangd false errors on Windows (MSVC `\Users` path escapes in `compile_commands.json`)
- Dead `writeBytes` helper; duplicated material shininess sync; empty validator branches

#### Removed

- JSON level format entirely (sources, loader, saver, validator, catalog) — levels are `.llev` only
- Inline material fields on level actors and the `Prop` / `Pawn` placeholder actor classes
- Migration body of [`Docs/TARGET_ARCHITECTURE.md`](Docs/TARGET_ARCHITECTURE.md) (stub → ARCHITECTURE); misspelled `LIBRERIES.md` → `LIBRARIES.md`
- Editor `LightmapPersist.h` (use `<leon/level/LightmapIO.h>`)
- CamelCase free-function aliases; `leon::games` / `leon_game_host` / `Leon::GameHost`

## [0.10.0] - 2026-08-08

### Added

- Dual-target **PS2 Emotion Engine** path: `Build/toolchains/ps2-ee.cmake`, `LEON_PLATFORM=PS2`, lean `Engine/CMakeLists.Ps2.txt`
- `Plugins/RHI/PS2` (`leon_rhi_ps2`): GS clear/VBlank, unlit triangle/rect primitives, `LPS2` header check
- Platform PS2: `WindowPs2`, `InputPad` (`InitializePad` / `EPadButton`), `Ps2CoreAnchor`
- Canonical pack **`Projects/Ps2Lab`** (`leon-Ps2Lab.elf`): Showcase / PadPilot / StressGrid / ClearOnly demos
- Toolchain smoke **`Samples/Ps2Hello`**; Docker builds via `Scripts/build-ps2-docker.ps1` / `.sh` (`ghcr.io/ps2dev/ps2dev`)
- Host scancode enums `EKey` / `EPadButton` / `EPlatform`; Window no longer exposes `GLFWwindow*`
- Docs: SETUP PS2 section, ASSET_FORMATS `LPS2`, NAMING notes for `leon_rhi_ps2` / Ps2Lab

### Changed

- README and build scripts are PS2-first; Editor no longer links removed host pack gameplay libs
- PS2 RHI layout polish: `Ps2GsContext`, `Ps2DrawPrimitives`, `Ps2DrawUnlitTriangleAt`

### Removed

- In-repo host sample packs: CoopTp, Zombies, Furytoon, ThirdPerson, Smoke (use `Templates/` for New Project)
- Dedicated pack smoke scripts (`smoke-coop-dedicated`, `smoke-packs`)

### Fixed

- PS2 display: real GIF `draw_clear` (bgcolor-only left a black framebuffer in PCSX2)
- Triangle screen-space coords (draw2d +2048 / XYOFFSET); pad init no longer blocks first painted frames

## [0.9.0] - 2026-07-31

### Added

- Physical Engine modules (`Core/`, `Platform/`, `Renderer/`, `Content/`, `Animation/`, `Network/`, `Scene/`, `Gameplay/`, `Utilities/`, `Serialization/`, `RHI/`, `Physics/`)
- Runtime `Application/` (`GameApplication`), `Project/`, `WorldRuntime/`, `Input/`
- Editor layout §6.3 (`Application/`, `Panels/`, `Gizmos/`, `SceneEditing/`, `Importers/`, `Preview/`, `Debug/`)
- Tools `CLI/` (`leon-cli`) + `ResourceTools/` reserved
- `leon_serialization` (`JsonUtil`); expanded `IRHIDevice` (`SetViewport`, `QueryGpuMemory`, active device)

### Changed

- Engine modules no longer include or link glad; DebugDraw/DebugOverlay GPU impl lives in `Plugins/RHI/OpenGL`
- `RunLeonGame` is a thin facade over `GameApplication`
- Docs: as-built layout matched; Phase 3/4 backends deferred (see also [Unreleased] for post-0.9 editor/build hardening)

## [0.8.0] - 2026-07-31

### Added

- Target top-level layout: `Engine/`, `Runtime/`, `Editor/`, `Tools/`, `Plugins/`, `Projects/`, `ThirdParty/`, `Build/`, `Scripts/`, `Tests/`
- Static plugins: `Plugins/RHI/OpenGL` (`IRHIDevice` + Renderer GPU impl), `Plugins/Physics/Arcade` (`IPhysicsBackend` + PhysScene)
- Thin Runtime library `leon_runtime` (alias `leon_game_host`)
- Smoke project `Projects/Smoke` (shipping-style exe, zero Editor)
- Canonical docs under `Docs/` (`ARCHITECTURE.md`)

### Changed

- Independent CMake entries: `cmake -S Editor|Tools|Projects/Smoke` (root still not a mega-project)
- Assets live in `Engine/Assets/`; project packs under `Projects/`
- Facade `leon_engine` links core + default plugins

### Removed

- Pre-migration `engine/` monolith tree and `games/` content root

## [0.7.1] - 2026-07-31

### Removed

- Level JSON aliases: `objects`, `gameplay`, `primitive`, `planeSize`, `solid`, `pushable`, `color`/`direction` on lights, `directionalLights`/`pointLights`, `mesh` as shape name
- LevelCatalog fallbacks to `lvl/` and `scenes/` (only `Levels/`)

### Changed

- Level schema is current-only; ContentValidator errors on removed keys
- Game POST_BUILD stages pack runtime only (`CopyGamePack.cmake`); no nested `build/` / `src/` / CMake under `games/<pack>/`
- `games/build-all.bat` builds every standalone game; per-pack `build.bat` always reconfigures CMake

## [0.7.0] - 2026-07-31

### Added

- `SceneComponent` attach tree (`AttachToComponent` / world transform); Actor root component
- `SkeletalMeshComponent` and `SpringArmComponent` inherit SceneComponent; Character mesh + pack spring arms attach to root
- Editor **Play In Editor** via `PieGameMode` (toolbar Play / Esc Stop); Outliner Actors + Details root transform
- Native Save As dialog; Place Actors **Prop** / **Pawn** placeholders
- `EPhysicsBackend` seam on `PhysScene`; `net::CaptureActorRoot` / `ApplyActorRoot`
- Architecture docs: Actor → SceneComponent → Renderer; pack checklist

### Changed

- Skeletal draws use SceneComponent world matrix (`submitSkeletalDraw` mat4 path)
- Actor↔Level `SyncTransformToLevel` documented as PhysScene mesh sync (not a second visual path)
- ContentValidator accepts Prop/Pawn editor classes
- Architecture docs in English

## [0.6.5] - 2026-07-31

### Changed

- Industry-standard monorepo split: `engine/` and `games/` only own product code
- **Independent builds:** CMake entry is `engine/` (`engine/build`, `engine/build-ninja`); each `games/<name>/` has its own `build/` + `build.bat`
- Root `CMakeLists.txt` refuses configure (not an entry point)
- `leon_game_host` (`engine/host/RunLeonGame`) shared by standalone games
- `third_party/`, `tests/`, scripts, and docs live under `engine/`
- Content paths `game/...` → `games/...`; cook via `engine/scripts/cook.bat` + recipe JSON

### Removed

- Root aggregating all games into one build; `games/CMakeLists.txt` umbrella
- Root `build/` / `build-ninja/` as the engine output location
- Root `third_party/`, `tests/`, `scripts/`, root `.bat` wrappers, `cook-bot*.bat`
- Root `ARCHITECTURE.md` / `SETUP.md` / `LIBRERIES.md` (now `engine/docs/`)

## [0.6.4] - 2026-07-30

### Added

- Unreal-like level editor `leon-editor` (`leon_editor` lib): Dear ImGui docking, Viewport FBO, Outliner, Details, Toolbar TRS (ImGuizmo), Content Browser, Place Actors, File Open/Save
- Asset Import (OBJ / FBX character cook / FBX anim cook) + Asset Preview panel with material override
- `LevelSaver` + loader provenance (`editorClass`, mesh/material paths, `bob`, point-light `orbit`) for JSON round-trip
- `Renderer::setDrawFramebuffer` so shadow/planar passes restore the editor viewport FBO

### Changed

- `ARCHITECTURE.md` / `README.md` / `SETUP.md` document the editor host and controls

## [0.6.3] - 2026-07-30

### Added

- Linux headless build: `scripts/build-linux.sh`, CMake `LEON_BUILD_CLIENT`, UNIX link of Threads/OpenGL
- SETUP.md Linux section (apt packages + configure flags)
- Vendored ENet under `third_party/enet` (stable IDE includes; no FetchContent for ENet)

### Changed

- `MemoryStats` reports process RSS via `/proc/self/statm` on Linux
- GLFW Wayland build disabled by default on Linux (fewer link deps for VPS)

## [0.6.2] - 2026-07-30

### Added

- Headless dedicated server `leon-server.exe` (no OpenGL/window): CPU meshes, fixed tick, Ctrl+C quit; `--port` / `--tick`
- `Engine::initializeHeadless` / `runHeadless`; `ResourceCache` GPU-off path; `StaticMesh`/`SkeletalMesh::CreateCpu`
- Pack `coop-tp` LAN 2P (listen `H`, dedicated `F9` / `--dedicated`, join `C`/`V`) on ENet `NetDriver`
- `game::RegisterGamePacks` — packs own GameMode registration; `app/main.cpp` no longer includes pack headers
- `World::RegisterBodiesFromLevel`, `GameMode::ResolvePlayerStart`, `LevelCatalog::FindIndexByGameModeOrPack`
- `WorldGameplayFrameParams::overridePhysicsStep` for multi-pawn physics; virtual `Character::PerformMovement`

### Changed

- `--dedicated` accepts optional `--gameMode=<id>` (defaults to `coop-tp`)
- Neutral `SpringArmComponent` defaults; packs set boom feel in Character ctors

### Fixed

- Coop dedicated: do not bind server start to **D** (MoveRight); use **F9**; ignore net role keys after connect

## [0.5.44] - 2026-07-30

### Fixed

- TPS M4 aim: blend hand-to-hand barrel with look direction + small yaw bias so the muzzle tracks the crosshair (was biased left from left-handguard offset)

## [0.5.43] - 2026-07-30

### Fixed

- TPS M4 orientation: barrel follows `LeftHand - RightHand` (rifle pose), grip locked to RightHand — no more Mixamo-axis guesswork leaving the gun vertical

## [0.5.42] - 2026-07-30

### Fixed

- TPS M4 socket rotation: map barrel +Z to Mixamo RightHand +X (was +Z→+Z, gun stood vertical in the palm)

## [0.5.41] - 2026-07-30

### Fixed

- TPS M4 fully parented to `mixamorig:RightHand` (bone TRS × mesh socket), no longer reoriented by look aim (was drifting / inverse yaw around the hand)

## [0.5.40] - 2026-07-30

### Fixed

- TPS M4 draws as a Level `StaticMeshComponent` (same opaque path as props) with per-frame model-matrix override from RightHand + aim — fixes ghost/AABB from the old attachment queue path

## [0.5.39] - 2026-07-30

### Fixed

- TPS weapon draw: unlit-only attachments, opaque alpha, face cull (two-sided z-fight looked like a grey ghost); removed debug AABB; pull grip forward into palm

## [0.5.38] - 2026-07-30

### Fixed

- TPS M4: socket uses RightHand **position** + look **aim** (barrel +Z), so the gun is not buried in Mixamo hand axes; attachment draw sets `uModel`/`uUseClipPlane`, polygon offset, bright unlit albedo

## [0.5.37] - 2026-07-30

### Fixed

- TPS weapon visibility: bake M4 to meters with grip at origin; queued static draws use unlit + no cull; attachment world AABB debug

## [0.5.36] - 2026-07-30

### Fixed

- TPS weapon attach: bone space is meters (not cm); palm offset was ~5m and sent gun/muzzle into the sky

## [0.5.35] - 2026-07-30

### Fixed

- TPS M4 scale (was microscopic after character fit); attach offset in bone space

### Added

- TPS hold-LMB auto-fire (~12 rps)

## [0.5.34] - 2026-07-30

### Changed

- TPS polish: continuous look, ADS FOV/boom/lag/speed, crosshair spread, fire muzzle→crosshair aim point
- `Camera::SetFieldOfView` / persistent FOV across resize

## [0.5.33] - 2026-07-30

### Added

- Pack `tps`: rifle Idle/Run cooked clips (`BotRifle.character.json`), `TpsAnimInstance`, M4A1 bone attachment on `mixamorig:RightHand`, LineTrace from muzzle tip
- `leon-cook anim`; `SkelMeshAttachment` + `Renderer::submitStaticDraw`; `AnimInstance::GetBoneWorldMatrices`

## [0.5.32] - 2026-07-30

### Added

- `UserWidget` / `HUD` / `WidgetPaintContext` + screen-space lines/rects on `DebugOverlay`
- Pack `tps`: `CrosshairWidget` at true screen center; character always faces boom yaw (`bOrientRotationToMovement` + `FaceRotation`); LineTrace from eye height

### Removed

- `Engine::SetHudCenterText` (replaced by HUD widgets)

## [0.5.31] - 2026-07-30

### Added

- Pack `tps`: over-right-shoulder bot, RMB ADS zoom, LMB `LineTraceSingleByChannel` with 3s debug draw + HUD crosshair
- `SpringArmComponent::SocketOffsetX`; `Engine::SetHudCenterText`

## [0.5.30] - 2026-07-29

### Changed

- `World` owns `PhysScene`; `TickGameplayFrame` runs Character move → Step → overlaps → Actor Tick → sync → skeletal draw
- TP/Showcase GameMode ticks no longer orchestrate PhysScene/draw/camera; `PlayerController::UpdateCamera`
- `AnimInstance` is locomotion-only; jump SM lives in framework `CharacterAnimInstance`; pack `ThirdPersonAnimInstance` wires clips + rates
- `SkeletalMeshComponent` stores named `AnimSequence` map; pack `NativeInitializeAnimation` binds BlendSpace/jump clips
- Engine public API PascalCase (`GetLevel`, `GetCamera`, `SetOrbitMouseEnabled`, …); camelCase aliases kept

## [0.5.29] - 2026-07-29

### Changed

- Pack-owned AnimClass: `SkeletalMeshComponent` holds polymorphic `AnimInstance`; Mixamo jump/land rates + Idle↔Run ease live in `ThirdPersonAnimInstance` (game pack), not engine defaults
- Engine `AnimInstance` defaults are neutral (play rate 1, snap blend, short crossfade); `NativeInitializeAnimation` hook for subclasses

## [0.5.28] - 2026-07-29

### Improved

- Jump/Land play rates (~2.6× / ~3×) so Mixamo clips match Character jump timing
- Longer Land → Locomotion crossfade; idle↔run BlendSpace input eases instead of snapping

## [0.5.27] - 2026-07-29

### Added

- AnimInstance jump state machine: `JumpStart` → `FallLoop` → `Land` → `Locomotion` with smoothstep crossfade between clips
- `AnimSequence::bLooping` (one-shot Jump / Land clamp; Fall loops)
- Character `GetVelocityZ()` / `ConsumeJustLanded()`; `NotifyJumped` on leave-ground
- Cook optional `--jump` / `--fall` / `--land` FBX → `Anims/JumpingUp|FallingIdle|FallingToLanding` + `character.json` keys
- Bot Mixamo jump clips wired via `cook-bot.bat`

### Changed

- `SkeletalMeshComponent::LoadFromCooked` loads jump clips and `SetupJumpStateMachine`
- Version `0.5.27`

## [0.5.26] - 2026-07-29

### Added

- Unreal-like collision traces on `PhysScene`: `LineTraceSingle/MultiByChannel`, `SphereTraceSingle/MultiByChannel`, `CapsuleTraceSingle/MultiByChannel` (`HitResult`, `ECollisionChannel`, `CollisionQueryParams`)
- `EDrawDebugTrace::ForOneFrame` + `DrawDebugLine/Sphere/CapsuleTrace` (F2 shows Character floor SphereTrace in third-person)
- `Character::IsFalling()` (pair of `IsMovingOnGround`); floor snap uses `SphereTraceSingleByChannel`
- Third-person template drives BlendSpace idle while `IsFalling()`

### Improved

- Trace `HitResult`: keep Unreal-like `Location` (sweep center) vs `ImpactPoint` (surface); Single helpers share `takeNearestHit`
- F2 / `EDrawDebugTrace::ForOneFrame` docs in README + ARCHITECTURE; dedicated `TraceDebugDrawTests`
- `LoadSkeletalMeshCooked` returns optional material path (no second JSON parse in `LoadFromCooked`)
- `format.bat` / `lint.bat` cover `tools/` and `tests/`; `test.bat` finds VS 18 (2026) VsDevCmd
- Docs: `animation/` + `content/` domains, cook-bot tooling, AnimSequence pose comments

## [0.5.25] - 2026-07-29

### Added

- Unreal-like skeletal cook pipeline: `leon-cook character` → Skeleton / SkelMesh / Material / Anims / BlendSpace1D / `*.character.json`
- Runtime `SkeletalMeshComponent::LoadFromCooked` (TP pack uses `Bot.character.json`)

### Changed

- Moved Iron Man sample to `assets/characters/iron-man/` (kebab-case); removed empty `assets/samples/`

## [0.5.24] - 2026-07-29

### Added

- FBX skeletal meshes via vendored ufbx: `SkeletalMesh`, `AnimSequence`, `BlendSpace1D`, `AnimInstance`
- Unreal-like `SkeletalMeshComponent` on `Character::GetMesh()` (TickComponent → NativeUpdateAnimation)
- GPU skinning shader (`skinned_lit.vert`) + `Renderer::submitSkeletalDraw` / `Character::SubmitMeshDraw`
- Third-person template loads Mixamo bot on GetMesh() (`Breathing Idle.fbx` + `Running.fbx`)
- Catch2 unit tests (`leon_tests` / `test.bat`): BlendSpace1D, AnimInstance, FBX bot load
- Fix FBX triangulation (`ufbx_triangulate_face` returns triangle count, not index count)
- Fix skeletal idle T-pose: bake `node_to_world` skins; prefer longest Mixamo anim stack (`mixamo.com`)

## [0.5.23] - 2026-07-29

### Improved

- Physics pass: AABB separation on min-penetration axis (X/Y/Z) so crates stack instead of sliding apart
- Dynamic bodies can rest on other dynamics; landing snap + resting friction; mass-weighted capsule push
- Character vertical land window mirrors prop snap (no side-mount teleport)

## [0.5.22] - 2026-07-29

### Fixed

- Dynamic props no longer teleport onto tall static AABBs when pushed into their sides (floor snap only when landing from above; XZ resolve before snap)

## [0.5.21] - 2026-07-29

### Changed

- Layout: `Transform` → `core/`, `Frustum`/`Aabb` → `render/`; `LevelAnimation` own header
- `LevelEntry.gameMode` read once at catalog scan (GameplayRouter no longer reopens JSON)
- F2 collision debug is an Engine tool flag (not Renderer)
- `PhysScene::Step` runs from GameMode tick; `Character::PerformMovement` is movement-only (Unreal-like)

## [0.5.20] - 2026-07-29

### Changed

- TP template: camera comes from pawn SpringArm + PlayerStart (no level `camera` JSON); `camera` remains optional for Showcase/Default framing

## [0.5.19] - 2026-07-29

### Changed

- Lights match Unreal Details: `rotation`/`position`/`scale`, `lightColor`, `intensity`, `castShadows`, `sourceAngle` (no `direction` field; legacy `direction`/`color` still load)

## [0.5.18] - 2026-07-29

### Changed

- HUD: F1/F2 AABB/Collision status bottom-left; active level browser bottom-right (stats stay top-right)

## [0.5.17] - 2026-07-29

### Added

- Unreal-like `PlayerStart` level actor (transform only) — GameMode spawns the possessed pawn there
- `Level::PlayerStarts()` / `FindPlayerStart()`; TP template spawns default Character mesh from GameMode

### Removed

- Level `"tag": "player"` mesh as the pawn — use `PlayerStart` + GameMode spawn instead

## [0.5.16] - 2026-07-29

### Removed

- `plantOnGround` level JSON flag — use `fitHeight` (scales and ground-aligns) or explicit `position` / `scale`

## [0.5.15] - 2026-07-29

### Changed

- Removed deprecated `solid` / `pushable` API fields — use Unreal-like `collisionEnabled` + `simulatePhysics`
- Loader still accepts legacy JSON keys `solid`→`collisionEnabled` and `pushable`→`simulatePhysics` for old levels

## [0.5.14] - 2026-07-29

### Added

- Unreal-like `enableGravity` on level actors / PhysScene Dynamic bodies (`bEnableGravity`)
- Dynamic bodies with gravity fall onto static supports / `floorY`

## [0.5.13] - 2026-07-29

### Changed

- Level JSON / `StaticMeshComponent`: Unreal-like `simulatePhysics` (dynamic PhysScene body)
- Legacy `"pushable": true` still accepted as an alias of `simulatePhysics`
- `solid` = static collision; `simulatePhysics` wins (Dynamic) when both are set

## [0.5.12] - 2026-07-29

### Added

- `"class": "BlockingVolume"` — Unreal-like invisible solid box (`solid` + `hidden` by default)
- `StaticMeshComponent::hidden` — skipped by the renderer; still collides in PhysScene
- Third-person level: four BlockingVolumes around the floor edge

### Notes

- Character jump uses arcade gravity: `velocityY += jumpSpeed`, then `velocityY -= gravity * dt` (not a full rigid-body solver)

## [0.5.11] - 2026-07-29

### Changed

- Showcase orbit (Demo / Iron Man): `ShowcaseOrbitActor` drives the view via `SpringArmComponent` (camera lag, rotation lag, smoothed zoom) — same feel as third-person

## [0.5.10] - 2026-07-29

### Changed

- `SpringArmComponent`: Unreal-like camera lag, rotation lag, and smoothed arm length (TP feel)
- Third-person move uses desired boom yaw (responsive) while the view lags slightly

## [0.5.9] - 2026-07-29

### Changed

- HUD: compact top-right two-column stats (`FPS`/`MS`, `RAM`/`VRAM`, `TRIS`/`OBJ`, `RES`)
- On-screen debug console moved to top-left (smaller font); default color red; newest stays put, older lines shift down
- Level browser chrome offset below the stats block

## [0.5.8] - 2026-07-29

### Changed

- Showcase (Demo / Iron Man): orbit + scroll only — no WASD pivot move
- LevelDirector: digit keys `1`–`9` (and keypad) jump to catalog levels

## [0.5.7] - 2026-07-29

### Changed

- Cursor captured + hidden by default (`GLFW_CURSOR_DISABLED`) for all levels — continuous mouse look/orbit without holding LMB
- Level browser chrome mouse clicks disabled while cursor is captured (use `[` `]` to switch levels)

## [0.5.6] - 2026-07-29

### Fixed

- Showcase: disable keyboard orbit so WASD only moves the pivot (no yaw/pitch tumble conflict)
- Showcase `OnExit` restores orbit mouse/keyboard flags like other modes
- Third-person scroll zoom: `Engine::ConsumeScrollY` + `SpringArmComponent::AddArmLengthInput` (boom length)

### Changed

- `SpringArmComponent`: arm length min/max, `ClampPitch` when seeding from level camera
- Docs: showcase custom GameMode; 0.5.4 wording aligned with later rename

## [0.5.5] - 2026-07-29

### Changed

- Renamed pack `walk` → `third-person-template` (kebab-case folder)
- Classes: `ThirdPersonTemplateGameMode` / `Character` / `PlayerController`; GameMode id `"third-person-template"`

## [0.5.4] - 2026-07-29

### Added

- `SpringArmComponent` (Unreal-like camera boom) for third-person follow
- Showcase pack GameMode: `ShowcaseGameMode` + `ShowcaseOrbitActor` + `ShowcasePlayerController` (orbit viewer)
- Third-person pack (then named `walk`): boom look (LMB), camera-relative move, Jump

### Changed

- Showcase levels `01`/`02` use `"gameMode": "showcase"`; `03_default_camera` stays `"Default"` (freelook)
- Third-person level display name set to "Third Person"

## [0.5.3] - 2026-07-29

### Changed

- `DefaultCameraActor`: Unreal-like free-look fly (LMB mouse look, WASD along view, Q/E up/down) — no orbit pivot
- `Camera`: `ECameraMode::Orbit` | `FreeLook` with `ForwardVector` / `RightVector` / `SetEyeLocation`
- Engine: `setOrbitMouseEnabled`; FreeLook uses LMB for look and disables scroll zoom

## [0.5.2] - 2026-07-29

### Added

- Input mapping system (Unreal-like): `InputMappingContext`, `PlayerInput`, `InputActions::*`
  - Default context: WASD+arrows → `MoveForward`/`MoveRight`, Q/E → `MoveUp`, Space → `Jump`
  - `Engine::input()`; sampled once per frame via `PlayerInput::Update`
  - Remap by `ClearContexts` / `AddMappingContext` (no hardcoded WASD in gameplay controllers)

### Changed

- Walk / DefaultPlayerController / keyboard orbit tumble read mapped axes/actions instead of raw GLFW keys

## [0.5.1] - 2026-07-29

### Added

- `DefaultCameraActor` + `DefaultPlayerController`: default pawn for `DefaultGameMode` (free-look fly)
- `Engine::AddOnScreenDebugMessage` / DebugOverlay timed Print-String messages (bottom-left, fade on expiry)
- Showcase level `game/showcase/Levels/03_default_camera.json` (`"gameMode": "Default"`)

### Changed

- `DefaultGameMode` spawns/possesses `DefaultCameraActor` and disables keyboard orbit while active

## [0.5.0] - 2026-07-29

### Changed

- **Breaking:** Unreal-style public naming (no `U`/`A`/`F` prefixes)
  - `GameWorld` → `World`; `getGameWorld` → `GetWorld`
  - Gameplay methods PascalCase where UE uses them (`BeginPlay`/`EndPlay`/`Tick`, `Possess`/`UnPossess`, `SpawnActor`, `GetActorLocation`, …)
  - Level visual `StaticMeshActor` → `StaticMeshComponent`; `actors()` → `StaticMeshes()` / `AddStaticMesh` / …
  - `PhysScene` / `Level` kept (≈ `FPhysScene` / `ULevel`); APIs PascalCase (`SyncFromLevel`, `Step`, …)
  - `levelActorIndex` → `levelMeshIndex` / `LevelMeshIndex`
  - `CharacterMotor` → `CharacterMovement`; `AddMovementInput` / `Jump` / `IsMovingOnGround` / `PerformMovement`
  - GPU `Mesh` → `StaticMesh`; `LoadStaticMesh` / `GetCubeMesh` / …
  - `Renderer::drawScene` takes `const Level& level` (not `scene`)
  - Removed unused `resolveScenesDirectory` (use `resolveGamesDirectory`)
- Docs: `World` (gameplay Actors) vs `Level` (map content) vs `PhysScene`; UE name mapping without prefixes

## [0.4.44] - 2026-07-29

### Added

- `ContentValidator`: validates level + material JSON on load (structure, types, referenced assets)
- Errors reject the level/material; warnings (missing optional textures/HDR) still allow load

## [0.4.43] - 2026-07-29

### Fixed

- LevelCatalog: use `Levels/` exclusively when present (legacy `lvl/` / `scenes/` only as fallback) — stops 3× duplicate levels from stale POST_BUILD copies

## [0.4.42] - 2026-07-29

### Added

- Engine `BasicLight` (`DirectionalLight` / `PointLight`); level JSON `"lights": [{ "class": "..." }]`
- Legacy `directionalLights` / `pointLights` still work when `lights` is absent
- `leon::AsciiToLower` helper for case-insensitive class / primitive name parsing
- `scripts/configure-ninja.bat` — Ninja build + `compile_commands.json` for clang-tidy / clangd
- `SETUP.md` — new-machine dependencies, first build, troubleshooting

### Changed

- Showcase + Walk levels use unified `lights[]` with class
- `build.bat` / `format.bat` / `lint.bat` detect VS 2022/2026 Community (or Professional) paths

## [0.4.41] - 2026-07-29

### Changed

- UV tiling (`uvScale` / `tiling`) lives on `Material` (shader `uUvScale`), not on Plane/BasicShape — Unreal-like

## [0.4.40] - 2026-07-29

### Added

- Engine `BasicShape` (`Cube` / `Sphere` / `Plane`): unit meshes + transform + material (Unreal Basic Shapes style)
- Level JSON `"class": "Cube"|"Sphere"|"Plane"` (alias `"primitive"`; legacy `"mesh": "cube"` still works)

### Changed

- Showcase + Walk levels place primitives via `"class"`; Plane size uses `scale` (unit mesh)

## [0.4.39] - 2026-07-29

### Added

- Material assets: `assets/Materials/` + `game/<pack>/Materials/*.json`; actor `"material": "path"`
- Engine default `M_Default` grayscale checker (Unreal-like) when procedural meshes omit material/MTL
- `ResourceCache::loadMaterial` / `defaultMaterial`; `MaterialAsset` helpers

### Changed

- Showcase + Walk levels reference material assets instead of inline albedo/specular/…

## [0.4.38] - 2026-07-29

### Changed

- Walk pack: `WalkCharacter` + `WalkPlayerController` (motor/capsule/input in C++; level JSON only places actors)
- `PlayerController::tickInput` is virtual (default no-op; Walk overrides)

## [0.4.37] - 2026-07-29

### Changed

- Pack level folders: `game/<pack>/scenes/` → `game/<pack>/Levels/` (catalog falls back to legacy `lvl/` / `scenes/` only if `Levels/` is empty)
- Level GameMode override: JSON `"gameMode"` (legacy `"gameplay"`); `DefaultGameMode` when unset
- `GameplayRouter::setDefaultMode`; Walk registered as override; showcase uses Default

## [0.4.36] - 2026-07-29

### Changed

- Level JSON: `actors[]` (legacy `objects[]`); per-actor `tag` / `solid` / `pushable` on `StaticMeshActor`
- Walk: PhysScene bodies from Level metadata; player via `tag: "player"`/`"pawn"`; motor config under `"walk": {}`
- `Level::findActorIndexByTag`; LevelAnimation `ActorSpin`/`ActorBob`

## [0.4.35] - 2026-07-29

### Added

- `GameInstance` (session, owned by `Engine`) and `PlayerState` (per-player, owned by `PlayerController`)
- `GameState` remains on `GameMode`; Walk resets/ticks GameState + PlayerState and notifies GameInstance on enter

### Changed

- `GameMode::onEnter` path arg renamed `levelJsonPath`
- `Engine::setGameInstance` calls shutdown/init when replacing a live instance
- `PlayerController.h` forward-declares `Engine` (include only in `.cpp`)

## [0.4.34] - 2026-07-29

### Changed

- Unreal-style rename: `Scene` → `Level` (`StaticMeshActor`, `LevelCatalog`/`Loader`/`Director`; folder `scene/` → `level/`)
- Physics: `CapsuleShape`, `EBodyType`, `BodyInstance`, `PhysScene`; sync `syncFromLevel`/`syncToLevel`; `levelActorIndex`
- `Engine::level()`; Actor `levelActorIndex` / `syncToLevel`; disk path `game/*/scenes/*.json` unchanged

## [0.4.33] - 2026-07-29

### Fixed

- `Controller` destructor defined out-of-line; idempotent `Pawn`/`Actor` destroy
- `AIController` arrive radius clamped; steer uses explicit length (no zero normalize)

### Changed

- Docs: Scene vs GameWorld vs PhysicsWorld table; README `modelYawOffset` default note

## [0.4.32] - 2026-07-29

### Changed

- Renamed gameplay `World` → `GameWorld` (`getGameWorld`) to distinguish from `Scene` and `PhysicsWorld`

## [0.4.31] - 2026-07-29

### Added

- `GameState` (match timer / in-progress) owned by `GameMode`; `setGameState<T>()` for subclasses
- `AIController` — wish direction or move-to-target steering for a possessed Character

### Changed

- Destroying / clearing a possessed `Pawn` auto-`unPossess`es (`destroy` + `endPlay`)
- Walk uses `player_.getCharacter()` and ticks `GameState`

## [0.4.30] - 2026-07-29

### Added

- Essential Unreal-style layer: `World` (spawn/tick/destroy), `Pawn`, `Controller`
- `GameMode` owns a `World`; `Character` is a `Pawn`; `PlayerController` extends `Controller`

### Changed

- Walk: spawns `Character` via `world_.spawnActor`, then `player_.possess`

## [0.4.29] - 2026-07-29

### Changed

- Unreal-style gameplay hierarchy: `Actor` → `Character`, `PlayerController`, `GameMode` (replaces `SceneMode` / `CharacterController`)
- Walk pack: `WalkController` → `WalkGameMode` (owns `PhysicsWorld` + `Character` + `PlayerController` possession)

## [0.4.28] - 2026-07-29

### Changed

- Project renamed **Geon → Leon Engine** (`leon` namespace, `include/leon/`, targets `leon_engine` / `leon_game` / `leon-engine`)

## [0.4.27] - 2026-07-29

### Changed

- Top-center HUD always shows `F1 AABB ON/OFF` and `F2 Collision ON/OFF`

## [0.4.26] - 2026-07-29

### Fixed

- Capsule vs dynamic: split depenetration + cancel velocity into contact (no post-step overlap stick)
- Body–body: remove velocity into MTV instead of blunt damp
- JSON: `solid` wins over `pushable` when both are set

### Changed

- `skipSceneIndex` renames the old ignore index; capsule F2 debug shows end-cap rings

## [0.4.25] - 2026-07-29

### Changed

- Physics bodies own position + AABB; explicit `syncFromScene` / `syncToScene`
- `PhysicsWorld::step()` unifies dynamics + body collisions; character uses `move()`
- Collision debug draws into `DebugDraw` (no Renderer dependency in physics)

## [0.4.24] - 2026-07-29

### Changed

- Physics/collision moved to engine domain `leon/physics/` (`Collider`, `PhysicsBody`, `PhysicsWorld`)
- `CharacterController` is a motor that consumes `PhysicsWorld`; Walk registers static/dynamic bodies from JSON

## [0.4.23] - 2026-07-29

### Fixed

- Capsule vs solid side contacts no longer vibrate (MTV separation; player always depenetrates)
- Pushable props collide with solids and each other (AABB XZ resolve by inverse mass)

## [0.4.22] - 2026-07-29

### Added

- Top-center HUD label `Collision Debug ON` while F2 collision overlay is active

## [0.4.21] - 2026-07-29

### Added

- **F2** toggles collision debug: character capsule + solid/pushable prop AABBs (orange = pushable, blue = solid)

## [0.4.20] - 2026-07-29

### Added

- `ARCHITECTURE.md` — layers, domain folders, runtime/render flow, pack rules

### Changed

- README links architecture; documents mirrored `src/` + tooling `cd /d "%~dp0"`
- `format.bat` / `lint.bat` / `build.bat` always run from the repo root

## [0.4.19] - 2026-07-29

### Changed

- `src/` mirrors `include/leon/` domains: `core/`, `scene/`, `render/`, `debug/`, `gameplay/` (`Engine.cpp` stays at `src/` root)

### Removed

- Unused `scripts/` (one-shot demo texture generator)

## [0.4.18] - 2026-07-29

### Changed

- Public headers organized by domain under `include/leon/`:
  - `core/` — Window, Paths, Camera, Input, MemoryStats
  - `scene/` — Scene, catalog/loader/director, Light, Transform, Frustum
  - `render/` — Renderer, meshes, shaders, textures, shadows, IBL, GL helpers
  - `debug/` — DebugDraw, DebugOverlay
  - `gameplay/` — SceneMode, GameplayRouter, CharacterController (unchanged)
- Root keepers: `Engine.h`, `Gameplay.h` (facades)

## [0.4.17] - 2026-07-29

### Changed

- Gameplay framework (`SceneMode`, `GameplayRouter`, `CharacterController`) moved into engine core: `include/leon/gameplay/` + `src/`
- `game/` holds only pack business logic; each pack uses `scenes/` + `src/` (headers beside sources)
- Walk: `game/walk/scenes/` + `game/walk/src/{WalkController.h,cpp}`

## [0.4.16] - 2026-07-28

### Fixed

- Capsule no longer snaps onto cube tops when approaching from the side (support only if feet are near the top)
- Pushable props get a visible shove + short slide velocity; immovable `solid` platforms block the capsule

### Changed

- Walk arena: large `solid` climb platforms + small `pushable` crates/spheres

## [0.4.15] - 2026-07-28

### Added

- `game::CharacterController`: reusable kinematic capsule (movement / jump / solids); visual mesh is cosmetics only via `syncVisual`
- Walk mode is a thin SceneMode around CharacterController (Unity-style CharacterController pattern)

## [0.4.14] - 2026-07-28

### Changed

- Walk uses a logical capsule for movement; Iron Man is visuals-only (`syncVisual`)
- Capsule can land on `"pushable"` / `"solid"` cube tops after a jump; green debug wire shows the collider

### Added

- Walk JSON: `capsuleRadius`, `capsuleHeight`, `floorY`, `stepUp`, `visualIndex`

## [0.4.13] - 2026-07-28

### Added

- Walk pack layout: `game/walk/scenes/` + `game/walk/src/` (headers beside the pack)
- Pushable cubes (`"pushable": true`) shoved by the walk player on XZ contact
- Walk Space jump + exponential yaw turn (already wired; facing offset fixed to 180°)

### Changed

- Default / scene `modelYawOffset` set to 180 so Iron Man faces the move direction

## [0.4.12] - 2026-07-28

### Changed

- Scenes live under each game pack: `game/<pack>/scenes/*.json` (no top-level `scenes/`)
- Walk facing: default `modelYawOffset` 0; smooth exponential yaw turn; Space to jump

### Added

- `SceneCatalog::scanGamePacks` + `resolveGamesDirectory()`

## [0.4.11] - 2026-07-28

### Added

- Layered build: `leon_engine` (core) + `leon_game` (scene modes) + `app/main` (host)
- `game::SceneMode` + `GameplayRouter` for pluggable per-scene logic
- Walk mode (`game/walk`, scene `03_walk.json` with `"gameplay": "walk"`)
- Engine `Input` helpers: `readWasdAxes`, `cameraRelativeMoveXZ`

### Changed

- SceneLoader ignores gameplay-only JSON keys; modes read their own config
- Project layout documents `app` → `game` → `leon_engine` dependency rule

## [0.4.8] - 2026-07-28

### Added

- HUD memory stats: process RAM working set + private (`ram` / `priv`), GPU VRAM via DXGI (fallback NVX/ATI)

## [0.4.7] - 2026-07-28

### Added

- GPU pass timers in the HUD (`sh` / `mir` / `col` ms, previous-frame, non-blocking)

### Changed

- Planar mirror at half resolution (`Renderer::kPlanarReflectionScale = 0.5`)
- Reflection pass: frustum cull, no shadows, flat normals (cheaper mirror)
- Lit pass binds environment + shadow map once per pass (not per object)

## [0.4.6] - 2026-07-28

### Added

- Planar mirror reflections for ground planes (`"planarMirror": true`): scene objects (spheres, Iron Man, …) appear in the floor, not only HDR/env lighting

### Changed

- Env cubemap anti-flicker uses lock-risk from `fwidth(R)` so large floors stay sharp instead of max-LOD blur

## [0.4.5] - 2026-07-28

### Changed

- Restored the pre-IBL lighting look (ambient 0.10, per-sample env tonemap, no final film curve / sRGB / irradiance ambient)
- Demo + Iron Man scenes restored to the UBO/roughness-era balance
- Kept UBO camera/lights, cubemap roughness LOD, and frustum culling
- Skybox exposure ×1.45 vs material env samples (brighter backdrop without washing spheres)
- Diffuse irradiance IBL: replaces flat `0.10` ambient (`k=0.20`, same per-sample tonemap; no final film curve)
- Flat-face env flicker: roughness floor 0.18 + `fwidth(R)` LOD AA (specular cubemap only)

## [0.4.4] - 2026-07-28

### Changed

- Demo/Iron Man lighting and materials dialed back (exposure, lights, albedo/specular/shininess)
- Procedural bump softer; floor no longer uses bump
- Lit pass: uniform albedo/specular converted sRGB→linear; weaker specular env strength

### Fixed

- Blinn specular no longer blows to white crescents (`(N+8)/8` energy scale removed)
- Specular fireflies from bump: stronger lobe AA + softened tangent normals
- Dark HDR sky: higher scene exposure + skybox exposure boost

## [0.4.3] - 2026-07-28

### Fixed

- Hot-reload: compile/link/read failures advance timestamps; F5 reports Failed vs Reloaded correctly
- Shader paths resolve per-file (newest wins) so repo edits beat a stale POST_BUILD copy
- Scene load aborts on any mesh failure (no partial commit); `"version": 1.0` accepted
- Multi-MTL JSON patches slots instead of collapsing; light overflow warns + truncates
- OBJ index bounds-check; near-zero scale sanitized; shadow pass disables cull (one-sided casters)
- Albedo textures as sRGB; cutout discard before lighting; HUD/chrome included in F5 reload

## [0.4.2] - 2026-07-28

### Fixed

- Hot-reload: advance file timestamps when UBO accept fails (no per-frame relink spam)
- Shadow pass draws only caster submeshes (skips transparent / unlit / `castsShadows: false`)
- Unified lit tonemap with skybox; cutout discard for opaque textured alpha
- Shadow/IBL balance (less crushed umbra); finer light AABB snap (0.5 m); milder bias/offset
- Debug lines: drop invalid `GL_POLYGON_OFFSET_LINE` (use `GL_LEQUAL`)
- `bob` defaults `baseY` to planted Y; missing scene `camera` resets to defaults

## [0.4.1] - 2026-07-28

### Fixed

- Draw order: opaque → skybox → transparent (glass no longer erased by the sky)
- Ground z-fighting: plant epsilon + slight demo lifts; softer procedural bump / less floor tiling
- Shadow bias uses geometric normal (not bump); fill lights dampen inside the primary shadow
- Specular Blinn screen-space AA; safer `normalize` when L≈−V
- Equirect→cubemap U wrap (horizon seam)
- Tangent `w` handedness for mirrored UVs (Iron Man / OBJ)
- Transparent sort by world AABB center; lit hot-reload reverts if UBO bind fails
- IBL ambient scaled when directional lights are present (less double-counting)

## [0.4.0] - 2026-07-28

### Added

- Diffuse irradiance IBL: CPU Lambertian convolution → low-res cubemap; lit ambient uses `uIrradianceMap`
- Shader hot-reload: timestamp watch + **F5** force-reload (failed compiles keep the previous program)

### Changed

- Env HDR load builds specular mips and an irradiance cubemap
- Ambient term no longer a fixed `0.10 * diffuseColor` when IBL is present

### Fixed

- Flat shiny faces no longer flicker bright/dark at distance (env LOD floor + screen-space reflection AA)
- Shadow acne/flicker: polygon offset, stronger bias, quantized caster AABB for stable light frustum

## [0.3.0] - 2026-07-28

### Added

- Scene JSON `"version": 1` (required; loader rejects missing/unsupported versions)
- Camera frustum culling by world AABB (`Frustum` / `Aabb`) with measurable `FrameStats`
- HUD: `visible/total` objects, culled count, submitted tris/draws
- std140 UBOs for camera and lights (`UniformBuffer`, bindings 0/1) on the lit pass
- Material `roughness` (JSON or derived from `shininess`) → cubemap `textureLod` blur

### Changed

- Shadow depth texture filter is `GL_NEAREST` (correct for manual PCF)
- Env cubemap reports mip count / max LOD for roughness sampling
- Demo metals expose explicit roughness steps (mirror → brushed)

## [0.2.0] - 2026-07-28

### Added

- JSON scene system under `scenes/` (`SceneCatalog`, `SceneLoader`, `SceneDirector`)
- Top-right scene browser UI (`< name >`) with mouse arrows and `[` / `]` keys
- Multi-material OBJ/MTL loading (submeshes, `Kd` / `Ks` / `Ns` / maps)
- Material `specular` and `metallic` with Blinn-Phong highlights and HDR cubemap reflections
- HDR environment maps (`EnvMap`: equirect `.hdr` → RGB16F cubemap + mips), skybox shaders,
  scene JSON `environment` / `environmentExposure`
- Debug draw layer (`DebugDraw`): object world AABBs + directional light 0 ortho frustum (**F1**)
- HUD stats: FPS, ms, tris, draws, materials, lights, resolution
- Sample scenes: `01_demo.json`, `02_ironman.json`
- Iron Man sample under `assets/samples/IronMan/`
- Sample HDR sky: `assets/Hdr/AutumnFieldPuresky1k.hdr`
- Tooling: `build.bat`, `format.bat`, `lint.bat`, `.clang-tidy`, `.clang-format`
- English README (build, format/lint, controls, JSON schema)
- nlohmann/json dependency for scene files

### Changed

- Demo content moved out of C++ into JSON (removed hard-coded `DemoScene`)
- Lit shader uses per-material specular/metallic instead of a fixed specular tint
- Procedural fake sky kept as fallback when no environment map is set
- clang-tidy config tuned for OpenGL / GLFW / GLM noise
- `Vertex` vs `MeshData`/`SubMesh` headers split for clearer naming

### Fixed

- Sphere triangle winding (outward normals / back-face culling)
- Scene switch only advances catalog index after a successful load
- JSON material fields set `materialOverride` so they win over MTL
- Scene load is atomic (staging `Scene` + deferred camera on success)
- `fitHeight` / `plantOnGround` keep JSON `position` as a post-fit offset
- HiDPI scene-browser hit-test: separate window vs framebuffer sizes

## [0.1.0] - 2026-07-28

### Added

- Initial forward renderer (OpenGL 3.3 / C++20): window, camera, meshes, textures
- Blinn-Phong and unlit shading, directional + point lights
- Shadow map (directional light 0) with PCF
- Normal mapping (TBN / tangents)
- Resource cache, procedural primitives (cube, plane, sphere)
- Transparent object queue (back-to-front)
- CMake + Visual Studio build (`build.bat`)

[0.10.0]: https://github.com/danielbarretoes/geon/compare/v0.9.0...v0.10.0
[0.9.0]: https://github.com/danielbarretoes/geon/compare/v0.8.0...v0.9.0
[0.4.5]: https://github.com/danielbarretoes/geon/compare/v0.4.4...v0.4.5
[0.4.4]: https://github.com/danielbarretoes/geon/compare/v0.4.3...v0.4.4
[0.4.3]: https://github.com/danielbarretoes/geon/compare/v0.4.2...v0.4.3
[0.4.2]: https://github.com/danielbarretoes/geon/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/danielbarretoes/geon/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/danielbarretoes/geon/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/danielbarretoes/geon/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/danielbarretoes/geon/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/danielbarretoes/geon/releases/tag/v0.1.0
