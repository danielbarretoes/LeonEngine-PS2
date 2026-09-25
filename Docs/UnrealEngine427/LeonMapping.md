# LeonEngine ↔ Unreal Engine 4.27 mapping

How each LeonEngine piece maps onto its UE 4.27 homologue, and every place where we deliberately differ.
Update this page whenever a module or type is added, moved or renamed.

## Build system

| UE 4.27 | LeonEngine |
| --- | --- |
| UnrealBuildTool (C#) | **LeonBuildTool** — pure CMake, `Engine/Source/Programs/LeonBuildTool/` |
| `<Module>.Build.cs` (`ModuleRules`) | `<Module>.Build.cmake` → `leon_module(...)` |
| `<Target>.Target.cs` (`TargetRules`) | `<Target>.Target.cmake` → `leon_target(...)` |
| `.uproject` | `.lproj` (JSON) |
| `.uplugin` | `.lplugin` (JSON) |
| `Engine/Build/BatchFiles/Build.bat <Target> <Platform> <Config> -Project=` | same command line |
| `UE4Game.Target.cs` | `Engine/Source/LeonGame.Target.cmake` |
| Setup.bat / GitDependencies | `Setup.bat/.sh` → pinned third-party downloads |
| `Engine/Platforms/<P>/` platform extension | `Engine/Platforms/PS2/` |
| `UBT_COMPILED_PLATFORM` | `LBT_COMPILED_PLATFORM` |

## Modules

| Leon (before) | UE module (now) | Notes |
| --- | --- | --- |
| `Engine/Core` (Transform, Paths, FileIO, Camera, Input, InputMapping) | `Core` (+ Engine for camera/input mapping) | `FTransform`, `FPaths`, `FFileHelper` |
| `Engine/Platform/Host` (GLFW window, memory stats) | `ApplicationCore` (`Private/Desktop`), `Core` HAL (`Private/Windows`) | |
| `Engine/Platform/Ps2` | `Engine/Platforms/PS2/Source/Runtime/{Core,ApplicationCore,Launch}` | platform extension |
| `leon_rhi` (`IRHIDevice`) | `RHI` (`FDynamicRHI`) | |
| `Plugins/RHI/OpenGL` | `OpenGLDrv` (device) + `Renderer` (GL renderer) | debt: Renderer calls GL directly |
| `Plugins/RHI/PS2` | `Engine/Platforms/PS2/Source/Runtime/PS2RHI` | `FPS2RHI` static API |
| `Engine/Renderer` CPU side | `RenderCore` | |
| `Engine/Serialization` | `Json` | native since P4 (`FJsonObject`, `TJsonReader`, `TJsonWriter`, `FJsonSerializer`); `FJsonUtils` removed |
| `Engine/Utilities` widgets | `UMG` (`U*` widgets); `TextLayout` → `SlateCore` | |
| `Engine/Utilities` HUD | `Engine` `GameFramework/HUD.h` | debt: Engine → UMG dependency |
| `Engine/Content` (C++) | `Engine` (`Private/Content`) | `ContentValidator` removed in 0.12.0 |
| `Engine/Animation` | `AnimationCore`; FBX skeletal import in `Developer/MeshUtilities` | cooked skeletal formats removed in 0.12.0 |
| `Engine/Network` | removed in 0.12.0 (local tag `archive/net-enet-0.11`) | replication returns later as UObject replication |
| `Engine/Audio` | `AudioMixer` (`FAudioDevice`) | UE keeps `FAudioDevice` in Engine |
| `Engine/Import` | `Developer/MeshUtilities` | |
| `Engine/Scene` | `Engine` (`ULevel`, `Public/Level`) | |
| `Engine/Gameplay` | `Engine` (`Classes/GameFramework`, `Components`, `Camera`, `Engine`, `Kismet`) + `AIModule` | |
| `leon_physics_iface` | `PhysicsCore` | `FHitResult` lives here (UE: Engine) |
| `Plugins/Physics/Arcade` | `Engine` (`FPhysScene`, `Private/PhysicsEngine`) | |
| `Plugins/Physics/Jolt` | plugin `Engine/Plugins/Runtime/JoltPhysics` | Win64 only |
| `Runtime/` (GameApplication, RunLeonGame) | `Launch` (`GuardedMain`, `FEngineLoop`) | |
| `Runtime/ProjectPack` (`leon.game.json`) | removed in 0.12.0 | `Projects` reads `.lproj` / `.lplugin` since P4 (`FProjectDescriptor`, `FPluginDescriptor`) |
| `Runtime/GameHostSession`, `WorldRuntime` | removed in 0.12.0 | `FGameApplication` (`Launch`) loads one level (`-map=`) |
| `Tools/ResourceTools` | `Developer/Cooker` (`FCookRecipe`, `FCookPaths`, `UCookCommandlet`) | UE: cook commandlet in UnrealEd |
| `Tools/AssetPipeline/leon-cook` | `Programs/LeonCook` | `UE4Editor-Cmd -run=cook` equivalent |
| `Tools/Cli` (`leon-cli`) | removed | only forwarded to leon-cook |
| `Tests/` (Catch2) | `<Module>/Private/Tests/` + `Programs/LeonAutomationTests` | UE automation tests for Core (P2), Json, Projects (P4), PhysicsCore, RenderCore and AnimationCore (P5); the other modules keep Catch2 until P6 |
| — | `Programs/TestPAL` | UE `Programs/TestPAL`: runs Core's automation tests on every platform (PS2 in PCSX2) |
| `ThirdParty/`, `Build/Dependencies.cmake` | `Engine/Source/ThirdParty/<Lib>/<Lib>.Build.cmake` | |
| `Engine/Assets` | `Engine/Content` + `Engine/Shaders` | |
| `Projects/Ps2ThirdPerson` | `Game/ThirdPerson` | isolated project |
| `Editor/` | removed | future OpenGL editor |

## Types (main)

| Leon (before) | UE name (now) |
| --- | --- |
| `Actor`, `ActorComponent`, `SceneComponent` | `AActor`, `UActorComponent`, `USceneComponent` |
| `Pawn`, `Character`, `CharacterMovement` | `APawn`, `ACharacter`, `UCharacterMovementComponent` |
| `Controller`, `PlayerController`, `AIController` | `AController`, `APlayerController`, `AAIController` |
| `GameMode`, `GameState`, `PlayerState` | `AGameModeBase`, `AGameStateBase`, `APlayerState` |
| `HUD` | `AHUD` |
| `World`, `Level`, `GameInstance`, `Engine` | `UWorld`, `ULevel`, `UGameInstance`, `UGameEngine` |
| `SpringArmComponent`, `SkeletalMeshComponent` | `USpringArmComponent`, `USkeletalMeshComponent` |
| `GameplayStatics` | `UGameplayStatics` |
| `BehaviorTree`, `Blackboard` | `UBehaviorTree`, `UBlackboardComponent` |
| `AnimInstance`, `Skeleton`, `AnimSequence`, `BlendSpace1D` | `UAnimInstance`, `USkeleton`, `UAnimSequence`, `UBlendSpace1D` |
| `Texture`, `StaticMesh`, `SkeletalMesh`, `Material` | `UTexture2D`, `UStaticMesh`, `USkeletalMesh`, `FMaterial` (render parameters; no `UMaterial` asset class yet) |
| `UserWidget`, `TextBlockWidget`, `ButtonWidget`, … | `UUserWidget`, `UTextBlock`, `UButton`, … |
| `Transform` | `FLegacyTransform` (glm, desktop, until P6); UE's `FTransform` is Core math (P3) |
| `Window` | `FGenericWindow` (+ `FGLFWWindow`, `FPS2Window`) |
| `EKey`, `EPadButton` | `EKeys` |
| `IRHIDevice`, `OpenGLDevice` | `FDynamicRHI`, `FOpenGLDynamicRHI` |
| `Ps2*` RHI functions | `FPS2RHI::*` |
| `MemorySnapshot` | `FPlatformMemoryStats` |
| `AudioDevice`, `PhysScene`, `HitResult` | `FAudioDevice`, `FPhysScene`, `FHitResult` |

The full rename table lives in this file as phases land (see sections added per phase below).

### Phase 3 — HAL, InputCore, ApplicationCore, RHI, Launch

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `queryMemorySnapshot()` (RAM part) | `FPlatformMemory::GetStats()` → `FPlatformMemoryStats` | `Core/Public/HAL/PlatformMemory.h` (+ `Windows/`, `Linux/`, PS2 ext) |
| `queryMemorySnapshot()` (GPU part) | `GDynamicRHI->GetGPUMemoryStats()` → `FRHIGPUMemoryStats` | `RHI/Public/DynamicRHI.h` |
| `LeonPS2SystemTimeUs()` | `FPlatformTime::Cycles64()` / `CyclesToMicroseconds()` | `Core/Public/HAL/PlatformTime.h` |
| PS2 sin/cos LUT (`Ps2Sin256`, …) | `FPlatformMath::Sin256` / `Cos256` | `Core/Public/HAL/PlatformMath.h` |
| `DebugOverlay` state (`SetStatsHudVisible`, `SetStatsHudExtraLine`, …) | `FStatsOverlay` (`AddOnScreenDebugMessage(Key, …)`, `CycleVisibility`) | `Core/Public/Stats/StatsOverlay.h` |
| `DrawEngineDebugOverlay`, `MarkEngineFrameStart` (PS2) | `FPS2StatsOverlay::Draw` / `MarkFrameStart` via `FPlatformEngineLoopHooks` | PS2 ext `Launch/Private` |
| — | `FTicker::GetCoreTicker()`, `IsEngineExitRequested` / `RequestEngineExit` | `Core/Public/Containers/Ticker.h`, `Core/Public/CoreGlobals.h` |
| `EMouseButton` | `EMouseButtons` | `ApplicationCore/Public/GenericPlatform/GenericApplicationMessageHandler.h` |
| `InputPad` (`IsPadButtonPressed`, `GetPadLeftStick`) | `IInputInterface` (`IsGamepadKeyDown(EKeys)`, `GetGamepadAnalog(EKeys::Gamepad_LeftX)`) | `ApplicationCore/Public/GenericPlatform/IInputInterface.h` |
| `InputPad.cpp` (libpad) | `FPS2InputInterface` (UE homologue: `XInputInterface`) | PS2 ext `ApplicationCore` |
| `Window` (GLFW / PS2 `#if`) | `GenericApplication::MakeWindow()` → `FGLFWWindow` / `FPS2Window` | `ApplicationCore/Private/Desktop`, PS2 ext |
| — | `FPlatformApplicationMisc::CreateApplication()` | `ApplicationCore/Public/HAL/PlatformApplicationMisc.h` |
| `rhi::CreateDevice` | `PlatformCreateDynamicRHI()`, `GDynamicRHI` | `RHI`, `OpenGLDrv`, `PS2RHI` |
| `main()` in each exe / game | `GuardedMain` + `FEngineLoop` (`GEngineLoop`), `Launch<Platform>.cpp` | `Launch` (+ PS2 ext `LaunchPS2.cpp`) |
| `RunPs2ThirdPersonDemo(Window&)` loop | `FThirdPersonModule` (primary game module) ticking `FThirdPersonGameMode` from `FTicker` (an `FTickerDelegate` since P2) | `Game/ThirdPerson` |

Engine loop without the gameplay framework (`WITH_ENGINE=0`, PS2): `PreInit` creates the application and
the 640×448 main window, then starts the statically linked modules (the game module registers its tick);
each `Tick` runs `PollGameDeviceState` → `PollEvents` → `FTicker` → `FPlatformEngineLoopHooks::EndFrame`
(stats overlay) → `SwapBuffers` (vsync) → `PostPresent`. Desktop `WITH_ENGINE=1` delegated to the
pre-UE `GameApplication` loop until Phase 4.9 (see the Phase 4 table below).

### Phase 4 — Epic naming across every module

`namespace leon` is gone (types are global like UE; free helpers live in `Leon::` namespaces such as
`Leon::InputActions`). Types got UE prefixes group by group, then clang-tidy
`readability-identifier-naming` renamed members, methods, parameters and locals over every
translation unit (Win64 compile database; PS2-only sources through a host-clang database built from
the pinned ps2dev image headers).

| Group | Leon (before) | UE name (now) |
| --- | --- | --- |
| 4.1 Core / Json / Projects | `Transform`, `Paths` free functions, `FileIO`, `AsciiToLower` | `FTransform`, `FPaths::*` (`ExecutableDir`, `ResolveAssetPath` — P4 rewrote `FPaths` with UE's API and kept only `ResolveLegacyContentPath`), `FFileHelper` (`Misc/FileHelper.h`), `FCString::ToLower` (`Misc/CString.h`; removed in P2) |
| | `serialization::ReadVec3 / LoadJsonFile`, `ProjectPack` | `FJsonUtils` (`Serialization/JsonUtils.h`; replaced by the native Json module in P4), `FProjectDescriptor` (removed in 0.12.0 with the packs, back in P4 for `.lproj`) |
| 4.2 Render | `Renderer`, `Texture`, `StaticMesh`, `SkeletalMesh`, `Material`, `EShadingModel` | `FSceneRenderer` (`SceneRenderer.h`), `UTexture2D` (`Texture2D.h`), `UStaticMesh`, `USkeletalMesh`, `FMaterial`, `EMaterialShadingModel` |
| | `MeshData`, `SubMesh`, `Vertex`, `Aabb`, `Plane`, `Frustum` | `FMeshData`, `FMeshSection`, `FVertex`, `FBox` / `FPlane` (Core's since P3), `FFrustum` |
| | `Shader`, `ShadowMap`, `GpuPassTimer`, `LdrColorTarget`, `SsaoTarget`, … | `FShader`, `FShadowMap`, `FGPUPassTimer`, `FLDRColorTarget`, `FSSAOTarget`, … |
| | `RHITextureId`, … , `kInvalidTexture` | `FRHITextureId`, … , `InvalidTexture` |
| 4.3 Physics | `HitResult`, `CollisionQueryParams`, `BodyInstance`, `PhysScene`, `CapsuleShape` | `FHitResult`, `FCollisionQueryParams`, `FBodyInstance`, `FPhysScene`, `FCapsuleShape` |
| 4.4 Anim / Audio / Net | `Skeleton`, `AnimSequence`, `BlendSpace1D`, `AnimInstance`, `AudioDevice`, … | `USkeleton`, `UAnimSequence`, `UBlendSpace1D` (`FBlendSample`), `UAnimInstance`, `FAudioDevice`, … (the `Leon::Net` renames went away with networking in 0.12.0) |
| 4.5 UMG | `ButtonWidget`, `ImageWidget`, `ProgressBarWidget`, `TextBlockWidget`, `VerticalBoxWidget`, `WidgetPaintContext` | `UButton`, `UImage`, `UProgressBar`, `UTextBlock`, `UVerticalBox` (files renamed to match), `FPaintContext` |
| 4.6 Engine | `Engine`, `World`, `Level`, `Actor`, `Character`, `Camera`, `GameMode`, `GameState` | `UGameEngine`, `UWorld`, `ULevel`, `AActor`, `ACharacter`, `UCameraComponent` (`Camera/CameraComponent.h`), `AGameModeBase`, `AGameStateBase` |
| | `LineTraceSingleByChannel(...)`, `ApplyPointDamage(...)` (Damage.h) | `UGameplayStatics::*` (`Kismet/GameplayStatics.h`) |
| | `NavigationSystem`, `NavMesh`, `InputMappingContext`, `PlayerInput` | `UNavigationSystem`, `FNavMesh`, `UInputMappingContext`, `UPlayerInput` |
| 4.7 AI | `AIController`, `BTSequence`, `BTSelector`, `BTConditionBool`, `BTAction`, `Blackboard` | `AAIController`, `UBTComposite_Sequence`, `UBTComposite_Selector`, `UBTDecorator_Bool`, `UBTTask_Action`, `UBlackboardComponent` |
| 4.8 Tools | LeonCook `main`, `RunCookRecipeFile`, `ResolveBeside`, `CookStaticMeshFrom*` | `UCookCommandlet::Main` (`Commandlets/CookCommandlet.h`), `FCookRecipe::RunFile`, `FCookPaths::ResolveBeside`, `FStaticMeshBuilder::CookFrom*` |
| 4.9 Launch | `RunLeonGame` + `GameApplication::Run` loop | `FEngineLoop::Init/Tick/Exit` driving `FGameApplication::Init/Tick/Exit` -> `UGameEngine::Start/Tick` |

Identifier conventions applied (Epic coding standard):

- Members, methods, free functions, parameters and locals in PascalCase; `b` prefix on bools; no `k`
  or trailing-underscore forms; globals keep a `G` prefix (`GDynamicRHI`, `GEngineLoop`).
- Accessor that would collide with its member -> `GetX()` (`Id()` + `id_` -> `GetId()` + `Id`); bool
  accessors use `Is` / `Has` (`HasFailed()`).
- Parameter that would shadow a member -> `InX`; local that would shadow a member or a
  namespace-scope constant -> `LocalX` (`kFaceNormals` + `faceNormals` -> `FaceNormals` + `LocalFaceNormals`).
- Variadic templates: `template <typename... ArgsType> ... (ArgsType&&... Args)`.
- UE enumerator names with underscores are kept (`EKeys::Gamepad_FaceButton_Bottom`).
- Public top-level classes / structs carry `<MODULE>_API` (empty: static linking).
- Shadowing is a compile error on every platform (MSVC `/we4456 /we4457 /we4458 /we4459`, GCC
  `-Werror=shadow`), mirroring UE's `ShadowVariableWarningLevel = Error`.
- Sets of free functions become static classes only where UE has that homologue (`FPaths`,
  `FFileHelper`, `FCString`, `FParse`, `FJsonSerializer`, `UGameplayStatics`, `FCookRecipe`, …); other free functions stay
  free and PascalCase like UE's `DrawDebugLine`.

### P2 — Core foundations

UE 4.27 Core APIs implemented in `Engine/Source/Runtime/Core` (every platform, PS2 included). Paths are relative to
`Core/Public/`.

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| — | `UE_BUILD_*`, `DO_CHECK`, `DO_GUARD_SLOW`, `DO_ENSURE`, `NO_LOGGING`; `INDEX_NONE`, `EForceInit`, `UE_NONCOPYABLE` | `Misc/Build.h`, `Misc/CoreMiscDefines.h` (from `CoreTypes.h`) |
| `char` literals | `TCHAR`, `TEXT()`, `WIDECHAR` (Windows HAL only) | `HAL/Platform.h` |
| — | `FPlatformMisc`, `FPlatformAtomics` | `HAL/PlatformMisc.h`, `HAL/PlatformAtomics.h` (+ `Windows/`, `Linux/`, PS2 ext) |
| `malloc` / `new` in Core | `FMemory`, `GMalloc` (`FMallocAnsi`) | `HAL/UnrealMemory.h`, `HAL/MallocAnsi.h` |
| `assert` | `check`, `checkf`, `verify`, `checkNoEntry`, `checkSlow`, `ensure`, `ensureMsgf`, `ensureAlways` | `Misc/AssertionMacros.h` |
| `std::move`, `std::swap`, `std::tuple`, `std::pair` | `MoveTemp`, `Swap`, `TTuple`, `TPair` | `Templates/UnrealTemplate.h`, `Templates/Tuple.h` |
| `std::unique_ptr`, `std::shared_ptr`, `std::function`, `std::optional` | `TUniquePtr`, `TSharedPtr` / `TSharedRef` / `TWeakPtr`, `TFunction` / `TUniqueFunction` / `TFunctionRef`, `TOptional` | `Templates/`, `Misc/Optional.h` |
| `std::sort`, `std::stable_sort`, `std::lower_bound` | `Sort`, `StableSort`, `Algo::LowerBound` / `BinarySearch` | `Templates/Sorting.h`, `Algo/` |
| `std::vector`, `std::unordered_set`, `std::unordered_map`, `std::span` | `TArray`, `TSet`, `TMap` / `TMultiMap`, `TArrayView`; `TBitArray`, `TSparseArray` | `Containers/` |
| `std::string` | `FString`, `FCString`, `FChar`, `StringConv` (`TCHAR_TO_UTF8`, `FTCHARToWide`) | `Containers/UnrealString.h`, `Misc/CString.h`, `Misc/Char.h`, `Containers/StringConv.h` |
| `FCString::ToLower(std::string_view)` | removed; the UE equivalent is `FString::ToLower` | `Containers/UnrealString.h` |
| — | `FName`, `EName`, `FNameLexicalLess` / `FNameFastLess` | `UObject/NameTypes.h`, `UObject/UnrealNames.inl` |
| — | `FText` (minimal), `LOCTEXT`, `NSLOCTEXT`, `INVTEXT` | `Internationalization/Text.h` |
| — | `FCrc` | `Misc/Crc.h` |
| `printf` / `std::cout` | `UE_LOG`, `UE_CLOG`, `DECLARE_LOG_CATEGORY_EXTERN`, `DEFINE_LOG_CATEGORY(_STATIC)`, `FOutputDevice`, `GLog` | `Logging/`, `Misc/OutputDevice*.h`, `CoreGlobals.h` |
| `FTicker` with `std::function` | `FTickerDelegate` + `FDelegateHandle` (`AddTicker(Delegate, Delay)`, `RemoveTicker(Handle)`) | `Containers/Ticker.h` |
| — | `TDelegate`, `TMulticastDelegate`, `DECLARE_DELEGATE*`, `DECLARE_MULTICAST_DELEGATE*`, `DECLARE_EVENT*` | `Delegates/Delegate.h`, `Delegates/IDelegateInstance.h` |
| Core Catch2 tests (`*Tests.cpp`) | `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `FAutomationTestBase`, `FAutomationTestFramework` (`System.Core.*`) | `Misc/AutomationTest.h`, `Core/Private/Tests/*Test.cpp` |

### P3 — Core math

UE 4.27's float math in `Core/Public/Math/` (every platform), included by `CoreMinimal.h` through `Math/UnrealMath.h`.

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `glm::vec2` / `vec3` / `vec4`, `glm::ivec2` / `ivec3` | `FVector2D`, `FVector` (`\|` dot, `^` cross), `FVector4`, `FIntPoint`, `FIntVector` | `Math/Vector2D.h`, `Vector.h`, `Vector4.h`, `IntPoint.h`, `IntVector.h` |
| Euler degrees in a `glm::vec3` | `FRotator` (Pitch / Yaw / Roll in degrees) | `Math/Rotator.h` |
| `glm::quat` | `FQuat` (`A * B` applies B first) | `Math/Quat.h` |
| `glm::mat4` | `FMatrix` (row vectors, `V * M`; `A * B` applies A first) + `FRotationMatrix`, `FRotationTranslationMatrix`, `FQuatRotationTranslationMatrix`, `FScaleRotationTranslationMatrix`, `FTranslationMatrix`, `FScaleMatrix`, `FInverseRotationMatrix`, `FRotationAboutPointMatrix`, `FPerspectiveMatrix`, `FReversedZPerspectiveMatrix`, `FOrthoMatrix`, `FReversedZOrthoMatrix`, `FLookFromMatrix`, `FLookAtMatrix` | `Math/Matrix.h` and one header per derived matrix |
| RenderCore `FBox` (glm) / private frustum plane | `FBox`, `FBox2D`, `FPlane`, `FSphere`, `FBoxSphereBounds` | `Math/Box.h`, `Box2D.h`, `Plane.h`, `Sphere.h`, `BoxSphereBounds.h` |
| `FTransform` (glm TRS, Euler) | `FTransform` (quaternion, translation, 3D scale; scalar version) | `Math/Transform.h`; the old type is `FLegacyTransform` in `Migration/LegacyTransform.h` |
| `glm::vec4` colors | `FColor` (BGRA bytes), `FLinearColor` (sRGB table, HSV) | `Math/Color.h` |
| `std::mt19937` / `rand()` | `FRandomStream`, `FMath::Rand` / `FRand` / `RandRange` / `VRand` / `VRandCone` | `Math/RandomStream.h`, `Math/UnrealMathUtility.h` |
| `glm::radians`, `glm::clamp`, `glm::mix` | `FMath::DegreesToRadians`, `Clamp`, `Lerp`, `FInterpTo`, `VInterpTo`, `RInterpTo`, `QInterpTo`, `ClampAngle`, `LinePlaneIntersection`, `LineBoxIntersection`, `ClosestPointOnSegment`, … | `Math/UnrealMathUtility.h` |
| — | `ToGlm` / `FromGlm` (desktop, until P6), `LegacyAxes` (until P7) | `Migration/GlmInterop.h`, `Migration/LegacyAxes.h` |

### P4 — Files, config, command line, Json and Projects

UE 4.27's platform services in `Core` (every platform) plus the `Json` and `Projects` modules. Core paths are relative
to `Core/Public/`.

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `std::filesystem` in `FPaths` / `FFileHelper` | `IPlatformFile`, `IFileHandle`, `IPhysicalPlatformFile`, `FPlatformFileManager`; backends `FWindowsPlatformFile`, `FLinuxPlatformFile`, `FPS2PlatformFile` (read-only) | `GenericPlatform/GenericPlatformFile.h`, `HAL/PlatformFilemanager.h`, `Private/<Platform>/`, PS2 ext |
| `std::ifstream` / `std::ofstream` | `IFileManager` (`CreateFileReader` / `CreateFileWriter`, `FindFiles`, `IterateDirectory`), `FFileHelper::LoadFileToString` / `LoadFileToArray` / `SaveStringToFile` / `SaveArrayToFile` | `HAL/FileManager.h`, `HAL/FileManagerGeneric.h`, `Misc/FileHelper.h` |
| — | `FArchive`, `FMemoryArchive`, `FMemoryReader`, `FMemoryWriter`, `FBufferArchive` | `Serialization/` |
| `FPaths::ResolveAssetPath`, `ExecutableDir` | `FPaths` with UE's API (`EngineDir`, `EngineContentDir`, `ProjectDir`, `ProjectContentDir`, `ProjectSavedDir`, `ProjectLogDir`, `ProjectPluginsDir`, `Combine`, `/`, `NormalizeFilename`, `ConvertRelativePathToFull`, `MakePathRelativeTo`, `GetBaseFilename`, …); `ResolveLegacyContentPath` until P15 | `Misc/Paths.h`, `Migration/LegacyContentPath.h` (`std::string` bridge) |
| hand-written `argv` loops (`--tick`, `--show-stats`) | `FCommandLine`, `FParse` (`Param`, `Value`, `Token`, `Command`, `Bool`), `FApp`, `FPlatformProcess` (`BaseDir`, `SetArgV0`) | `Misc/CommandLine.h`, `Misc/Parse.h`, `Misc/App.h`, `HAL/PlatformProcess.h` |
| `.ini` placeholders | `FConfigCacheIni`, `FConfigFile`, `FConfigSection`, `FConfigValue`, `GConfig`, `GEngineIni` / `GGameIni` / `GInputIni` / `GEditorIni` | `Misc/ConfigCacheIni.h` |
| — | `FOutputDeviceFile` (`<Project>/Saved/Logs`), `FLogSuppressionInterface` (`[Core.Log]`, `-LogCmds`) | `Misc/OutputDeviceFile.h`, `Logging/LogSuppressionInterface.h` |
| — | `FGuid`, `FMD5` / `FMD5Hash`, `FDateTime`, `FTimespan`, `FPlatformTime::SystemTime` / `UtcTime`, `FPlatformMisc::CreateGuid` | `Misc/Guid.h`, `Misc/SecureHash.h`, `Misc/DateTime.h`, `Misc/Timespan.h` |
| `FJsonUtils` over nlohmann | `FJsonValue` (+ `FJsonValueString` / `Number` / `Boolean` / `Array` / `Object` / `Null`), `FJsonObject`, `TJsonReader` / `TJsonReaderFactory`, `TJsonWriter` / `TJsonWriterFactory` (pretty / condensed policies), `FJsonSerializer` | `Json/Public/Dom/`, `Json/Public/Serialization/`, `Json/Public/Policies/` |
| `.lproj` / `.lplugin` read only by LeonBuildTool | `FProjectDescriptor`, `FPluginDescriptor`, `FModuleDescriptor` (`EHostType`, `ELoadingPhase`), `FPluginReferenceDescriptor`, `IProjectManager`, `IPluginManager`, `IPlugin` | `Projects/Public/`, `Projects/Public/Interfaces/` |
| — | `FEngineLoop::PreInit` order: command line → project → config → log file and verbosity → project descriptor → modules | `Launch/Private/LaunchEngineLoop.cpp` |

### P5 — Lower modules on the UE types

ApplicationCore, RHI, OpenGLDrv, PS2RHI, the shared Launch code, PhysicsCore, RenderCore, AnimationCore, AudioMixer,
SlateCore and UMG use Core types (InputCore had none to replace). The world keeps its Y-up metre semantics.

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `std::unique_ptr<FGenericWindow> MakeWindow()` | `TSharedRef<FGenericWindow> MakeWindow()` | `ApplicationCore/Public/GenericPlatform/GenericApplication.h` |
| `GetCursorPos(double&, double&)`, `SetScrollCallback(std::function)` | `FVector2D GetCursorPos()`, `OnMouseWheel()` (`FOnWindowMouseWheel` delegate, float notches) | `GenericPlatform/GenericWindow.h` |
| `std::unique_ptr<FDynamicRHI> PlatformCreateDynamicRHI()` | `FDynamicRHI* PlatformCreateDynamicRHI()` (caller owns), `LogRHI` | `RHI/Public/DynamicRHI.h` |
| `printf` / `std::cerr` in the platform layers | `LogApplicationCore`, `LogRHI`, `LogInit`, `LogAudioMixer` | |
| `FCapsuleShape` (Radius, full Height) | `FCollisionShape` (`MakeCapsule(Radius, HalfHeight)`, `GetCapsuleRadius`, `GetCapsuleHalfHeight`) + UE's `ECollisionShape::Type` | `PhysicsCore/Public/CollisionShape.h` |
| `ECollisionShape` (body Box / TriangleMesh) | `EBodyCollisionShape` (Leon; frees the UE name) | `PhysicsCore/Public/BodyInstance.h` |
| glm fields of `FHitResult`, `FBodyInstance`, `FTriangleMeshCollision`; `std::vector` in `IPhysicsBackend` | `FVector` / `FVector2D` fields, `TArray`, `TUniquePtr` backends | `PhysicsCore/Public/` |
| `FVertex`, `FMeshData`, `FMaterial` on glm / std | `FVector` / `FVector2D` / `FVector4`, `TArray`, `FString`, `TSharedPtr<UTexture2D>`; `FMeshData::IsEmpty` | `RenderCore/Public/` |
| `FFrustum::ExtractFromViewProjection(glm::mat4)` | `ExtractFromViewProjection(FMatrix)` | `RenderCore/Public/Frustum.h` |
| bone data on glm (`glm::ivec4` indices, `glm::mat4` matrices, `std::string` names) | `FIntVector4` (added to Core), `FMatrix`, `FName` bone / clip names | `AnimationCore/Public/SkeletalAnimation.h`, `Core/Public/Math/IntVector.h` |
| `FAudioDevice` paths as `std::string_view`, glm listener | `const TCHAR*` paths, `FVector` listener / emitter | `AudioMixer/Public/AudioDevice.h` |
| `UTextBlock::SetText(std::string)`, glm widget colors, string ids | `SetText(FText)`, `FLinearColor` colors, `FName` ids (`TickInput` returns `NAME_None` when nothing was activated) | `UMG/Public/Components/` |
| Catch2 tests of PhysicsCore, RenderCore, AnimationCore | automation tests (`System.PhysicsCore.*`, `System.RenderCore.*`, `System.AnimationCore.*`) | `<Module>/Private/Tests/` |

## Deviations from UE 4.27 (intentional)

| Topic | UE | LeonEngine | Why |
| --- | --- | --- | --- |
| Reflection | `UCLASS`, `UObject`, UHT | none; `A`/`U` prefixes are naming only | CoreUObject is the next plan |
| Containers / strings | `TArray`, `TMap`, `FString` everywhere | Core has them (P2) and the modules up to UMG use them (P5); Engine, Renderer, AIModule, the Developer modules and Jolt still use `std::` containers / `std::string` | migration finishes in P6 |
| `TCHAR` | `wchar_t` / UTF-16 on most platforms | UTF-8 `char` on every platform; `TEXT(x)` is `x`; `WIDECHAR` only inside the Windows HAL; `TCHAR_TO_UTF8` & co. are identities | the EE has no wide-string support worth paying for; one encoding everywhere |
| `FName` pool | growing name blocks, `FNamePool` sized for desktop | 8-byte `FName`, hard-coded `EName` list; block size / count and hash buckets from `FPlatformProperties::NamePool*` (PS2: 16 KB blocks, at most 256 KB, 4096 buckets); exhausting the pool is fatal | fixed memory budget on 32 MB ([Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md)) |
| `FText` | localized text (`FTextLocalizationManager`, culture formatting) | minimal: `FromString`, `AsNumber`, `AsPercent`, `Format` (`{0}` arguments), `Join`; `LOCTEXT` / `NSLOCTEXT` keep the source text | no localization yet |
| Delegates | also dynamic (`DECLARE_DYNAMIC_*`) and `UObject` bindings | `TDelegate` / `TMulticastDelegate` with static, lambda, raw and SP bindings (+ payload) | dynamic / `UObject` delegates need CoreUObject |
| `FPlatformAtomics` on PS2 | real atomics | the generic non-atomic version | Leon runs a single EE thread |
| Automation tests | run by the session frontend / `-ExecCmds="Automation RunTests"` | `FAutomationTestFramework::RunTests(Filter)` from `LeonAutomationTests` (with Catch2) and `TestPAL` (every platform) | no editor / session frontend |
| Math | `FVector`, `FRotator`, `FMatrix` everywhere, SIMD `VectorRegister`, `double` helpers | Core has the scalar float API (P3) and the modules up to UMG use it (P5); Engine, Renderer, AIModule, the Developer modules and Jolt still use glm (Y-up metres) until P6, converting with `ToGlm` / `FromGlm`; the world stays Y-up in metres until P7 | migration step by step; the EE has no SIMD path worth matching and a single-precision FPU |
| Build tool | C# UBT | CMake scripts | no .NET dependency; PS2 toolchain is CMake-based |
| Linking | monolithic or DLLs | always static (`IS_MONOLITHIC=1`), generated module table | PS2 has no DLLs |
| Renderer | API-agnostic via RHI command lists | calls OpenGL directly | debt |
| Engine ↔ Renderer | acyclic | `CIRCULAR_DEPENDENCIES` | debt |
| PS2 gameplay | full framework on consoles | PS2 game uses `F*` types, no `AActor` | the gameplay framework is desktop-only (glm/json, C++20) |
| Config layers | `Base.ini`, `Base<T>`, `Engine/Config/<P>/`, `Engine/Platforms/<P>/Config`, project `Default<T>`, `Config/<P>/`, `Platforms/<P>/Config`, `Saved/Config` (plus `NotForLicensees` / `Restricted` folders and a binary config cache) | the same order without `NotForLicensees` / `Restricted` or the binary cache; the `Saved/Config` user layer exists only on desktop; the PS2 reads the ini files through `host:` and keeps compiled defaults when they are missing | the PS2 build has no writable storage and PCSX2's host filesystem is optional |
| Config usage | `UPROPERTY(Config)` / `LoadConfig` everywhere, input from `BaseInput.ini` | only a few keys are read (map, resolution, stats, ThirdPerson tuning) | `UPROPERTY(Config)` needs reflection (P10); config-driven input comes in P13 |
| `FString` in archives | ANSI when possible, else UTF-16 with a negative length | always UTF-8 with a positive length (including the terminator); a negative length is rejected | `TCHAR` is UTF-8 (D1) |
| `FName` in archives | an index into the package name table (the base `FArchive` does not store names) | the base `FArchive` writes it as a string | there are no packages until P11; the linker will replace this |
| `FPaths` directories | relative to the process (`../../../Engine/`) | absolute on desktop, built from the executable folder and the generated `GLeon*FromBaseDir` globals; on PS2 a staged layout under the ELF folder (`<Base>/Engine/`, `<Base>/<Project>/`) | independent of the working directory; PCSX2's `host:` is the ELF folder |
| `FDateTime` | Julian-day and `double` helpers | integer ticks only; the Julian-day helpers are left out | float-only math (D6) |
| JSON numbers | written with `%.17g` | the shortest `%.15g`–`%.17g` form that reads back to the same value | readable descriptors; still round-trips |
| Module platform lists | `WhitelistPlatforms` / `BlacklistPlatforms` (4.27) | `PlatformAllowList` / `PlatformDenyList` (UE 5 names) written; the 4.27 names are still read | the `.lplugin` files already used the UE 5 names |
| Plugin enable state | decides which plugin modules load | `IPluginManager` reports it, but LeonBuildTool alone decides what is linked (`ENABLE_PLUGINS`) | static linking, no module loading at runtime |
| Global `operator new` / `delete` | replaced through `FMemory` in every monolithic build (`REPLACEMENT_OPERATOR_NEW_AND_DELETE`) | replaced on the PS2 only (`PS2PlatformRuntime.cpp`) | keeps libstdc++'s allocation, unwinder and demangler code out of the ELF; desktop still uses the CRT |
| Capsule placement | `FCollisionShape` capsules are centered on the component | Leon's character capsule stands on the actor location (feet): it spans feet to feet + 2 × half height | the CMC-lite works from the feet until the character becomes a UCapsuleComponent (P12) |
| Body collision shape | the body setup's aggregate geometry | `FBodyInstance::CollisionShape` (`EBodyCollisionShape::Box` / `TriangleMesh`) | Leon's physics scene has AABB and triangle-mesh bodies only |
| Bone matrices | `FTransform` bone poses, `FMatrix` in UE's row-vector convention | `FMatrix` values that keep the memory of the imported glm matrices (column-vector transforms); the skin matrix is `InverseBind * BoneWorld` in FMatrix order | the renderer uploads them as they are; they become proper UE transforms with the skeletal mesh assets (P14) |
| Widget colors | `FSlateColor` / `FLinearColor` with alpha | `FLinearColor`, alpha ignored by the debug overlay | the HUD overlay draws opaque RGB |
| Game → Launch | game modules never see `FEngineLoop` | the PS2 game module reads `GEngineLoop.GetMainWindow()` (include-only dependency on the launch module) | no Slate / `GEngine` on PS2 to hand out the viewport |
| Gamepad | `FSlateApplication` routes `IInputInterface` events to the player controller | game code polls `IInputInterface` state directly | no Slate; polling matches the PS2 frame loop |
