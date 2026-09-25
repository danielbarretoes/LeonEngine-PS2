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
| UnrealHeaderTool | **LeonHeaderTool** (P8) — `Engine/Source/Programs/LeonHeaderTool/`, run by LeonBuildTool for every module that includes a `.generated.h`; its code runs on CoreUObject (P9) |

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
| `Engine/Serialization` | `Json` | native since P4 (`FJsonObject`, `TJsonReader`, `TJsonWriter`, `FJsonSerializer`); `FJsonUtils` removed; also serves `MaterialAsset` and the cook recipes since P6 (nlohmann removed) |
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
| `Tests/` (Catch2) | `<Module>/Private/Tests/` + `Programs/LeonAutomationTests` | UE automation tests for Core (P2), Json, Projects (P4), PhysicsCore, RenderCore and AnimationCore (P5), and every other module (P6); Catch2 removed in P6 |
| — | `Programs/TestPAL` | UE `Programs/TestPAL`: runs the Core, CoreUObject, Json and Projects automation tests on every platform (PS2 in PCSX2) |
| — | `CoreUObject` (P9, P10) | UE `Runtime/CoreUObject`: `UObject`, reflection, `NewObject`, the object array, garbage collection, references, config and Exec; every platform; only its own types and test fixtures are reflected until P12 |
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
| `Transform` | `FTransform` (Core math, P3; every component, level record and light since P7). The interim `FLegacyTransform` was removed in P7 |
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
| `FTransform` (glm TRS, Euler) | `FTransform` (quaternion, translation, 3D scale; scalar version) | `Math/Transform.h`; the old type became `FLegacyTransform` (`Migration/LegacyTransform.h` in Core until P6, then `Engine/Public/Level/LegacyTransform.h`) and was removed in P7 |
| `glm::vec4` colors | `FColor` (BGRA bytes), `FLinearColor` (sRGB table, HSV) | `Math/Color.h` |
| `std::mt19937` / `rand()` | `FRandomStream`, `FMath::Rand` / `FRand` / `RandRange` / `VRand` / `VRandCone` | `Math/RandomStream.h`, `Math/UnrealMathUtility.h` |
| `glm::radians`, `glm::clamp`, `glm::mix` | `FMath::DegreesToRadians`, `Clamp`, `Lerp`, `FInterpTo`, `VInterpTo`, `RInterpTo`, `QInterpTo`, `ClampAngle`, `LinePlaneIntersection`, `LineBoxIntersection`, `ClosestPointOnSegment`, … | `Math/UnrealMathUtility.h` |
| — | `ToGlm` / `FromGlm` (desktop), `LegacyAxes`; both removed in P6 | `Migration/GlmInterop.h`, `Migration/LegacyAxes.h` (deleted) |

### P4 — Files, config, command line, Json and Projects

UE 4.27's platform services in `Core` (every platform) plus the `Json` and `Projects` modules. Core paths are relative
to `Core/Public/`.

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `std::filesystem` in `FPaths` / `FFileHelper` | `IPlatformFile`, `IFileHandle`, `IPhysicalPlatformFile`, `FPlatformFileManager`; backends `FWindowsPlatformFile`, `FLinuxPlatformFile`, `FPS2PlatformFile` (read-only) | `GenericPlatform/GenericPlatformFile.h`, `HAL/PlatformFilemanager.h`, `Private/<Platform>/`, PS2 ext |
| `std::ifstream` / `std::ofstream` | `IFileManager` (`CreateFileReader` / `CreateFileWriter`, `FindFiles`, `IterateDirectory`), `FFileHelper::LoadFileToString` / `LoadFileToArray` / `SaveStringToFile` / `SaveArrayToFile` | `HAL/FileManager.h`, `HAL/FileManagerGeneric.h`, `Misc/FileHelper.h` |
| — | `FArchive`, `FMemoryArchive`, `FMemoryReader`, `FMemoryWriter`, `FBufferArchive` | `Serialization/` |
| `FPaths::ResolveAssetPath`, `ExecutableDir` | `FPaths` with UE's API (`EngineDir`, `EngineContentDir`, `ProjectDir`, `ProjectContentDir`, `ProjectSavedDir`, `ProjectLogDir`, `ProjectPluginsDir`, `Combine`, `/`, `NormalizeFilename`, `ConvertRelativePathToFull`, `MakePathRelativeTo`, `GetBaseFilename`, …); `ResolveLegacyContentPath` until P15 | `Misc/Paths.h` (the `Migration/LegacyContentPath.h` `std::string` bridge was removed in P6) |
| hand-written `argv` loops (`--tick`, `--show-stats`) | `FCommandLine`, `FParse` (`Param`, `Value`, `Token`, `Command`, `Bool`), `FApp`, `FPlatformProcess` (`BaseDir`, `SetArgV0`) | `Misc/CommandLine.h`, `Misc/Parse.h`, `Misc/App.h`, `HAL/PlatformProcess.h` |
| `.ini` placeholders | `FConfigCacheIni`, `FConfigFile`, `FConfigSection`, `FConfigValue`, `GConfig`, `GEngineIni` / `GGameIni` / `GInputIni` / `GEditorIni` | `Misc/ConfigCacheIni.h` |
| — | `FOutputDeviceFile` (`<Project>/Saved/Logs`), `FLogSuppressionInterface` (`[Core.Log]`, `-LogCmds`) | `Misc/OutputDeviceFile.h`, `Logging/LogSuppressionInterface.h` |
| — | `FGuid`, `FMD5` / `FMD5Hash`, `FDateTime`, `FTimespan`, `FPlatformTime::SystemTime` / `UtcTime`, `FPlatformMisc::CreateGuid` | `Misc/Guid.h`, `Misc/SecureHash.h`, `Misc/DateTime.h`, `Misc/Timespan.h` |
| `FJsonUtils` over nlohmann | `FJsonValue` (+ `FJsonValueString` / `Number` / `Boolean` / `Array` / `Object` / `Null`), `FJsonObject`, `TJsonReader` / `TJsonReaderFactory`, `TJsonWriter` / `TJsonWriterFactory` (pretty / condensed policies), `FJsonSerializer` | `Json/Public/Dom/`, `Json/Public/Serialization/`, `Json/Public/Policies/` |
| `.lproj` / `.lplugin` read only by LeonBuildTool | `FProjectDescriptor`, `FPluginDescriptor`, `FModuleDescriptor` (`EHostType`, `ELoadingPhase`), `FPluginReferenceDescriptor`, `IProjectManager`, `IPluginManager`, `IPlugin` | `Projects/Public/`, `Projects/Public/Interfaces/` |
| — | `FEngineLoop::PreInit` order: command line → project → config → log file and verbosity → project descriptor → modules | `Launch/Private/LaunchEngineLoop.cpp` |

### P5 — Lower modules on the UE types

ApplicationCore, RHI, OpenGLDrv, PS2RHI, the shared Launch code, PhysicsCore, RenderCore, AnimationCore, AudioMixer,
SlateCore and UMG use Core types (InputCore had none to replace). The world keeps its Y-up metre semantics (until
P7).

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

### P6 — Upper modules on the UE types

Renderer, Engine, AIModule, MeshUtilities, Cooker, LeonCook, the JoltPhysics plugin and the desktop Launch code
(`FGameApplication`) use Core types; glm, nlohmann, Catch2 and Core's `Migration/` folder are removed. The world keeps
its Y-up metre semantics until P7.

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `glm::vec2` / `vec3` / `vec4` / `mat4` in the upper modules | `FVector2D`, `FVector`, `FVector4`, `FMatrix` (render matrices keep glm's GL layout until P7) | |
| `glm::perspective`, `ortho`, `lookAt`, `translate`, `rotate`, `scale`, `mat4_cast`, `value_ptr`, `radians`, `normalize`, `A * B`, `M * v`, the normal matrix | `LegacyGL::Perspective`, `Ortho`, `LookAt`, `Translate`, `Rotate`, `Scale`, `QuatToMatrix`, `ValuePtr`, `Radians`, `Normalize`, `Mul(A, B)`, `Transform` / `TransformPoint` / `TransformDirection`, `NormalMatrix3x3` (glm's formulas term by term); removed in P7 | `RenderCore/Public/LegacyGLMath.h` (deleted in P7) |
| `FLegacyTransform` on glm (Core `Migration/LegacyTransform.h`, desktop) | `FLegacyTransform` on Core math (`ModelMatrix()` returns an `FMatrix`); replaced by `FTransform` in P7 | `Engine/Public/Level/LegacyTransform.h` (deleted in P7) |
| `ToGlm` / `FromGlm`, `LegacyAxes`, `Migration/LegacyContentPath.h` | removed; `FPaths::ResolveLegacyContentPath` stays until P15 | `Core/Public/Misc/Paths.h` |
| `std::vector`, `std::string`, `std::unordered_map`, `std::function`, `std::unique_ptr` / `std::shared_ptr` | `TArray`, `FString`, `TMap`, `TFunction`, `TUniquePtr` / `TSharedPtr` | |
| `UInputMappingContext` / `UPlayerInput` with `std::string_view` action names | `FName` action names, `TMap<FName, …>` bindings and state | `Engine/Public/GameFramework/InputMapping.h` |
| `FDebugDraw` / `FDebugOverlay` with `glm::vec3` colors and `std::string` text | `FLinearColor` colors, `FString` text | `Renderer/Public/Debug/` |
| `FResourceCache` returning `std::shared_ptr<UStaticMesh>`, `std::unordered_map` caches | `TSharedPtr<UStaticMesh>` / `TSharedPtr<UTexture2D>`, `TMap<FString, …>` caches | `Renderer/Public/ResourceCache.h` |
| `UStaticMesh` / `USkeletalMesh` bounds as `glm::vec3` | `FVector` (`GetLocalMin`, `GetLocalMax`) | `Renderer/Public/StaticMesh.h`, `SkeletalMesh.h` |
| `ULevel` on `std::vector` / `std::string`, `std::size_t` mesh indices with `npos` | `TArray`, `FString`, `SIZE_T` mesh indices with `ULevel::Npos` | `Engine/Classes/Engine/Level.h` |
| `.llev` reader / writer on `std::ifstream` and `std::filesystem` | `FMemoryReader` / `FMemoryWriter` and `FFileHelper`; the same bytes, the string table stays case-sensitive | `Engine/Public/Level/LeonLevelFormat.h` |
| `.lmat` reader / writer on `std::string` streams | `FString` and `FFileHelper` with its own line parser (same rules: `#` / `;` comments, case-insensitive sections and keys) | `Renderer/Public/LeonMaterialFormat.h` |
| `PatchMaterialFromJson` / `HasMaterialSurfaceFields` on `nlohmann::json` | take a `const FJsonObject&` (`Json` module); missing or mistyped fields keep their value | `Renderer/Public/MaterialAsset.h` |
| cook recipes on nlohmann, switches parsed into `std::string` | `FJsonSerializer` / `FJsonObject`; switches read with `FCString`; files through `IFileManager` / `FPaths` | `Cooker/Private/CookRecipe.cpp`, `Commandlets/CookCommandlet.cpp` |
| navigation A* open set in a `std::priority_queue` | `TArray` heap (`HeapPush` / `HeapPop`) | `Engine/Private/AI/Navigation/NavigationSystem.cpp` |
| `std::cout` / `std::cerr` in the upper modules, `printf` in BlankProgram | `UE_LOG` with `LogEngine`, `LogLevel`, `LogPath`, `LogPhysics` (`Engine/Public/EngineLogs.h`), `LogRenderer` (`Renderer/Private/RendererLog.h`), `LogMeshUtilities`, `LogCook`, `LogJolt`, `LogLaunch`, `LogBlankProgram`; headless `LeonGame` flushes `GLog` every tick | |
| `std::chrono`, `std::this_thread::sleep_until` | `FPlatformTime`, `FPlatformProcess::Sleep` (Windows and Linux) | `Core/Public/HAL/PlatformProcess.h` |
| `TIsDerivedFrom<Base, Derived>` | `TIsDerivedFrom<Derived, Base>` (UE's order) | `Core/Public/Templates/UnrealTypeTraits.h` |
| `#include <Windows.h>` in the Windows HAL and OpenGLDrv | `Windows/WindowsHWrapper.h` (UE's name; keeps Core's `TEXT`) | `Core/Public/Windows/WindowsHWrapper.h` |
| MSVC warning C4324 (padding added for `alignas`) | disabled (`/wd4324`), as UE does | `LeonBuildTool/Configuration/CompileEnvironment.cmake` |
| C++20 on Win64 and Linux | C++17 on every platform, as UE 4.27 | `LeonBuildTool/Platform/*/LeonBuild*.cmake` |
| Catch2 tests of Engine, Renderer, AIModule, MeshUtilities and JoltPhysics; `-noautomation` / `-automationonly` | automation tests (`System.Engine.*`, `System.Renderer.*`, `System.AIModule.*`, `System.MeshUtilities.*`, `System.JoltPhysics.*`), 179 in total; `LeonAutomationTests` keeps only `-automation=<filter>` | `<Module>/Private/Tests/` |
| `System.Core.Migration.GlmInterop.*`, `System.Core.Migration.LegacyTransform.*` | `System.RenderCore.LegacyGLMath.Builders` / `Composition` (checked against values glm 1.0.1 printed), `System.Engine.LegacyTransform.*`; both replaced in P7 | `RenderCore/Private/Tests/LegacyGLMathTests.cpp`, `Engine/Private/Tests/LegacyTransformTests.cpp` (deleted in P7) |
| — | `CheckBannedApis.ps1` (gate G4): rejects glm, nlohmann, `std::` containers / strings / functions / smart pointers, iostream and the `printf` family outside the allowed places | `Engine/Build/BatchFiles/` |

### P7 — UE axes and units

The world moves from Y up, right-handed, metres with glm's GL matrices to UE's space: X forward, Y right, Z up,
left-handed, 1 unit = 1 cm, UE view and projection matrices. Legacy data is converted in its readers; the importers
write UE space. Released as 0.14.0. Conventions: [Coordinates](#coordinates).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| world Y up, right-handed, 1 unit = 1 m | X forward, Y right, Z up, left-handed, 1 unit = 1 cm; every metre constant scaled by 100 (masses stay in kg) | every desktop module and the shaders |
| `LegacyGL::Mul(A, B)`, `Transform` / `TransformPoint` / `TransformDirection`, `ValuePtr` | `B * A` (`FMatrix` order), `TransformFVector4` / `TransformPosition` / `TransformVector`, `FShader::SetMat4(Name, const FMatrix&)` | `Core/Public/Math/Matrix.h`, `Renderer/Public/Shader.h` |
| `LegacyGL::LookAt`, `Perspective`, `Ortho` (right-handed, clip z in [-1, 1]) | `MakeViewMatrix(Origin, FRotator)` / `MakeLookAtView` (UE view space: x right, y up, z forward), `FPerspectiveMatrix` / `FOrthoMatrix` (depth [0, 1]), then `ToGLClipSpace` | `RenderCore/Public/ViewMatrices.h`, `GLClipSpace.h` |
| `LegacyGL::NormalMatrix3x3`, the overlay's ortho matrix | renderer-private helpers | `Renderer/Private/RenderMatrices.h` |
| `LegacyGL::QuatToMatrix` (FBX skeletal import) | `FQuatRotationMatrix` | `MeshUtilities/Private/FbxSkeletalImport.cpp` |
| `FLegacyTransform` (Euler XYZ degrees, `T * Rx * Ry * Rz * S`) | `FTransform` / `FRotator` on components (`RelativeLocation`, `RelativeRotation`, `RelativeScale3D`), level records, lights, volumes, player starts and attachments; children compose as `Relative * ParentWorld` | `Engine/Classes/Components/SceneComponent.h`, `Engine/Public/Level/` |
| legacy `.llev` values used as they are | `FLegacyCoordinateConversion`: basis (X, Z, Y) × 100, rotations (−X, −Z, −Y, W), tangents (X, Z, Y, −W), scale (X, Z, Y), the angle map; only in the readers, the saver and tests (G4) | `RenderCore/Public/LegacyCoordinateConversion.h` |
| `LightDirectionFromRotation` / `RotationFromLightDirection` | the light's rotation; it shines along `GetRotation().GetForwardVector()` | `Engine/Public/Level/Light.h` |
| actor yaw as a float (`AActor` float overloads) | `FRotator` (`GetActorRotation`, `SetActorRotation`, `SetActorLocationAndRotation`) | `Engine/Classes/GameFramework/Actor.h` |
| `UCameraComponent::Orbit` / `AddLook` / `SetYawPitch` / `GetYawDegrees` / `GetPitchDegrees` | `AddViewRotation` / `SetViewRotation` / `GetViewRotation` (a view `FRotator`) | `Engine/Public/Camera/CameraComponent.h` |
| controller yaw / pitch kept by the game modes | `AController::ControlRotation`, `APawn::AddControllerYawInput` / `AddControllerPitchInput` / `GetViewRotation`, `APlayerController` look input with `ViewPitchMin` / `ViewPitchMax` | `Engine/Classes/GameFramework/Controller.h`, `Pawn.h`, `PlayerController.h` |
| move input as forward / right floats | `FVector2D` (X forward, Y right) from `UPlayerInput::GetMoveInput`; `YawRelativeMove(FRotator, FVector2D)` | `Engine/Public/GameFramework/InputMapping.h`, `Input.h` |
| spring arm boom yaw / pitch angles | `TargetOffset`, `SocketOffset`, `bUsePawnControlRotation`, `GetTargetRotation` | `Engine/Classes/GameFramework/SpringArmComponent.h` |
| `QuerySupportY`, `FloorY`, `VelXz` / `VelocityY`, `ClampPositionXZ`, `SeparateAabbXZ`, `AabbOverlapY`, NavMesh `OriginZ`, `BobBaseY`, `MakeReflectMatrix(PlaneY)` | `QuerySupportZ`, `FloorZ`, `VelXY` / `VelocityZ`, `ClampPositionXY`, `SeparateAabbXY`, `AabbOverlapZ`, NavMesh `OriginY`, `BobBaseZ`, `MakeReflectMatrix(PlaneZ)` | Engine, AIModule, Renderer |
| importers producing legacy Y-up metres | `FImportCoordinateConversion` as the last step (`EImportAxes::RightHandedYUp` (X, Z, Y) × 100 for OBJ / glTF; `RightHandedZUp` (X, −Y, Z) × the file unit for FBX, UE's `FFbxDataConverter`) | `MeshUtilities/Public/ImportCoordinateConversion.h` |
| `.lmesh` version 1 (legacy space) | version 2 (world space) written; version 1 converted at load | `RenderCore/Public/LeonMeshFormat.h` |
| Jolt and miniaudio fed engine values directly | boundaries that swap Y and Z and scale by 0.01 (the libraries stay Y up in metres) | `JoltPhysics/Private/JoltPhysicsBackend.cpp`, `AudioMixer/Private/AudioDevice.cpp` |
| — | golden tests of the legacy behaviour (`System.Engine.Golden.*`, `System.AIModule.Golden.*`, `System.JoltPhysics.Golden.*`) and their adapters | `Engine/Public/Tests/LegacyGolden.h`, `*/Private/Tests/Golden*Tests.cpp` |
| — | `LeonGame -Screenshot=<file.bmp> -ExitAfterFrames=N`, `-AxesGizmo` / F6 axes gizmo (`FDebugDraw::AddAxes`, `AddViewAxes`) | `Launch/Private/Desktop/GameApplication.cpp`, `Renderer/Public/Debug/DebugDraw.h` |
| G4 without the legacy bridges | G4 bans `LegacyGL`, `FLegacyTransform`, `LegacyAxes` and fences `FLegacyCoordinateConversion` | `Engine/Build/BatchFiles/CheckBannedApis.ps1` |

### P9 — CoreUObject

The runtime side of LeonHeaderTool's code: `UObject` and its reflection, on every platform (PS2 included). No engine
class is a `UObject` yet (P12). Details: [CoreUObject/README.md](../../Engine/Source/Runtime/CoreUObject/README.md).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| — | `UObjectBase` / `UObjectBaseUtility` / `UObject`, `EObjectFlags`, `EInternalObjectFlags` | `CoreUObject/Public/UObject/UObjectBase.h`, `UObjectBaseUtility.h`, `Object.h`, `ObjectMacros.h` |
| — | `UField`, `UStruct`, `UScriptStruct` (`ICppStructOps`, `TCppStructOps`), `UClass`, `UEnum` (`_MAX`), `UFunction`, `UPackage` | `Class.h`, `Package.h` |
| — | `FField` / `FFieldClass` / `FProperty` and the property types (numeric, bool with bitfields, byte / enum, string / name / text, object / class / weak / soft, struct, array / set / map), `TFieldIterator` | `Field.h`, `UnrealType.h` |
| — | `NewObject`, `StaticConstructObject_Internal`, `FObjectInitializer`, `CreateDefaultSubobject`, `StaticFindObject` / `FindObject`, `MakeUniqueObjectName`, `CreatePackage`, `GetTransientPackage`, `GetDefault` | `UObjectGlobals.h` |
| — | `GUObjectArray` (`FUObjectArray`, `FUObjectItem`), the name hash, `TObjectIterator` / `TObjectRange` | `UObjectArray.h`, `UObjectHash.h`, `UObjectIterator.h` |
| — | `Cast` / `CastChecked` / `ExactCast`, `TSubclassOf`, `TWeakObjectPtr`, `TSoftObjectPtr` / `TSoftClassPtr`, `FSoftObjectPath` | `Templates/Casts.h`, `Templates/SubclassOf.h`, `UObject/WeakObjectPtr*.h`, `SoftObject*.h` |
| — | `FFrame`, `DECLARE_FUNCTION` / `DEFINE_FUNCTION`, `P_GET_*`, `UObject::ProcessEvent` | `Stack.h`, `Script.h`, `ScriptMacros.h`, `Private/UObject/ScriptCore.cpp` |
| — | `UE4CodeGen_Private` (params, `ConstructUClass` & co.), `RegisterCompiledInInfo`, `ProcessNewlyLoadedUObjects`, `UObjectBaseInit`, the intrinsic classes (`IMPLEMENT_CORE_INTRINSIC_CLASS`) | `UObjectGlobals.h`, `UObjectBase.h`, `GeneratedCppIncludes.h`, `Private/UObject/Class.cpp` |
| — | `NoExportTypes.h`: `USTRUCT(noexport)` `FVector`, `FVector2D`, `FVector4`, `FPlane`, `FRotator`, `FQuat`, `FTransform`, `FColor`, `FLinearColor`, `FGuid`, `FIntPoint`, `FIntVector`, `FBox` | `CoreUObject/Public/UObject/NoExportTypes.h`; LeonHeaderTool `NoExport` support |
| — | `FScriptArray`, `FScriptSparseArray`, `FScriptSet`, `FScriptMap`, `TScriptBitArray` (layout views of the Core containers, checked with `static_assert`s) | `Core/Public/Containers/ScriptArray.h`, `SparseArray.h`, `Set.h`, `Map.h`, `BitArray.h` |
| — | `TEnumAsByte`, `WITH_EDITORONLY_DATA`, `PRAGMA_DISABLE/ENABLE_DEPRECATION_WARNINGS`, `FPlatformProperties::MaxObjectsInGame` (8192 on PS2, 131072 on desktop) | `Core/Public/Containers/EnumAsByte.h`, `Misc/Build.h`, `HAL/Platform.h`, `GenericPlatformProperties.h` |
| `FModuleManager` started modules only | `RegisterReflection` then `OnProcessLoadedObjectsCallback` before each `StartupModule` | `Core/Public/Modules/ModuleManager.h` |
| — | `System.CoreUObject.*` automation tests (27) with reflected fixtures | `CoreUObject/Private/Tests/` |

### P10 — GC and references, config and Exec

Garbage collection, the reference types, config members and console commands on CoreUObject, every platform (PS2
included). Nothing in the engine calls them until P12 / P13. Details:
[CoreUObject/README.md](../../Engine/Source/Runtime/CoreUObject/README.md).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| objects lived until exit; `AddToRoot` only set a flag | `CollectGarbage(KeepFlags, bPerformFullPurge)`, `TryCollectGarbage`, `IsGarbageCollecting`, `IncrementalPurgeGarbage`, `IsIncrementalPurgePending`, `GARBAGE_COLLECTION_KEEPFLAGS`, `FReferenceCollector`; `LogGarbage` | `CoreUObject/Public/UObject/UObjectGlobals.h`, `GarbageCollection.h`, `Private/UObject/GarbageCollection.cpp` |
| — | `FGCObject` (`AddReferencedObjects`, `GetReferencerName`), `TStrongObjectPtr` | `GCObject.h`, `StrongObjectPtr.h` |
| — | `UObject::AddReferencedObjects` / `UClass::ClassAddReferencedObjects` (passed by `IMPLEMENT_CLASS`), `UClass::ReferenceTokenStream` / `AssembleReferenceTokenStream`, `CLASS_TokenStreamAssembled` | `Object.h`, `Class.h`, `ObjectMacros.h` |
| `BeginDestroy` / `FinishDestroy` stubs | `ConditionalBeginDestroy` (renames to `NAME_None`), `IsReadyForFinishDestroy`, `ConditionalFinishDestroy`, `LowLevelRename`, routing checks | `Object.h`, `UObjectBase.h`, `Private/UObject/Obj.cpp` |
| — | `MarkPendingKill` / `IsPendingKill` semantics of 4.27 (references cleared during the mark), `IsUnreachable`, `IsPendingKillOrUnreachable`, `IsValid(UObject*)`; `RF_MarkAsRootSet` / `RF_MarkAsNative` become `RootSet` / `Native` | `UObjectBaseUtility.h`, `Object.h`, `UObjectBase.cpp` |
| — | `FGarbageCollectionSettings` (`gc.TimeBetweenPurgingPendingKillObjects` from `[/Script/Engine.GarbageCollectionSettings]`), `FGarbageCollectionTimer` (UE: `UEngine::ConditionalCollectGarbage`), `FGarbageCollectionStats` | `GarbageCollection.h` |
| `FWeakObjectPtr` (`Get`, `IsValid`, `IsStale`) | + `Get(bEvenIfPendingKill)`, `IsExplicitlyNull`, `HasSameIndexAndSerialNumber`; `TWeakObjectPtr<T, TWeakObjectPtrBase>` with UE's comparisons | `WeakObjectPtr.h`, `WeakObjectPtrTemplates.h`, Core `UObject/WeakObjectPtrTemplatesFwd.h` |
| minimal `FSoftObjectPath` / `FSoftObjectPtr` | `FSoftObjectPath` (4.27 layout, `GetLongPackageName`, `GetAssetName`, `IsAsset`, `TryLoad`, `ExportTextItem` / `ImportTextItem`, `GetCurrentTag`), `FSoftClassPath`, `TPersistentObjectPtr`, `FSoftObjectPtr::LoadSynchronous`, `TSoftObjectPtr` / `TSoftClassPtr` (`LoadSynchronous`, `IsStale`) | `SoftObjectPath.h`, `PersistentObjectPtr.h`, `SoftObjectPtr.h`; `NoExportTypes.h` reflects the paths |
| — | `TStructOpsTypeTraits::WithExportTextItem` / `WithImportTextItem`, `STRUCT_ExportTextItemNative` / `STRUCT_ImportTextItemNative` | `Class.h`, `PropertyStruct.cpp` |
| — | `EPropertyObjectReferenceType`, `FProperty::ContainsObjectReference(EncounteredStructProps, Type)` | `UnrealType.h` |
| `PostConstructLink` and `PPF_ConfigOnly` unused | `UObject::LoadConfig` / `SaveConfig` / `ReloadConfig` / `PostReloadConfig` / `OverridePerObjectConfigSection` / `GetDefaultConfigFilename`, `UE4::ELoadConfigPropagationFlags`, `GetConfigFilename`, `UsesPerObjectConfig`, `UClass::GetConfigName`; class default objects load their config in `PostConstructInit` | `Object.h`, `ObjectMacros.h`, `Class.h`, `Private/UObject/Obj.cpp`, `UObjectGlobals.cpp` |
| `FUNC_Exec` unused | `UObject::CallFunctionByNameWithArguments`, `ProcessConsoleExec`; Core `FExec`, `FSelfRegisteringExec`, `FStaticSelfRegisteringExec` | `Object.h`, `Private/UObject/ScriptCore.cpp`, Core `Misc/Exec.h`, `Misc/CoreMisc.h` |
| delegates bound static / lambda / raw / SP | + `BindUObject` / `CreateUObject` / `AddUObject` (`TUObjectDelegateInstance` over `TWeakObjectPtr`); `Add` compacts dead bindings | Core `Delegates/Delegate.h`, `DelegateInstancesImpl.h` |
| `PerObjectConfig` rejected | LeonHeaderTool emits `CLASS_PerObjectConfig`; golden case `ConfigAndExec` (35 cases) | `Programs/LeonHeaderTool` |
| — | 21 new `System.CoreUObject.*` tests (GarbageCollection, Delegates, SoftObject, Config, Exec: 48 in total) | `CoreUObject/Private/Tests/` |

## Coordinates

| Topic | UE 4.27 | LeonEngine |
| --- | --- | --- |
| Axes | X forward, Y right, Z up, left-handed | the same since P7 |
| Units | 1 unit = 1 cm (`WorldToMeters` 100) | the same; masses in kg |
| Rotations | `FRotator` (positive yaw turns right, positive pitch looks up) | the same |
| Matrices | `FMatrix` row vectors, `A * B` applies A first | the same |
| View space | x right, y up, z forward (`FViewMatrices`) | the same (`MakeViewMatrix`, `MakeLookAtView`) |
| Projection | reversed-Z `FReversedZPerspectiveMatrix`, horizontal FOV | `FPerspectiveMatrix` (depth [0, 1], not reversed) with a vertical FOV, then `ToGLClipSpace` |
| Importers | `FFbxDataConverter` (X, −Y, Z); glTF `ConvertVec3` (X, Z, Y) | `FImportCoordinateConversion` with the same two bases |
| Physics | PhysX / Chaos in cm, Z up | the arcade scene in cm, Z up; Jolt in m, Y up behind a boundary |
| Audio | listener and emitters in world space | miniaudio in m, Y up behind a boundary |

Legacy `.llev` angles convert as: actor yaw ψ → `90 − ψ`; orbit camera (yaw Y, pitch P) → `FRotator(−P, Y + 180, 0)`;
free look → `FRotator(P, Y, 0)`; directional light → `FRotator(−P, 90 − Y, 0)`; spin rates change sign. Details and
the converters' allowed places: [ARCHITECTURE.md — Coordinates](../ARCHITECTURE.md#coordinates).

## Deviations from UE 4.27 (intentional)

| Topic | UE | LeonEngine | Why |
| --- | --- | --- | --- |
| Reflection | `UCLASS`, `UObject`, UHT | LeonHeaderTool generates UE 4.27-shaped `.generated.h` / `.gen.cpp` (no metadata, no hot-reload CRCs, explicit `RegisterReflection_<Module>` instead of static `FCompiledInDefer` objects) and CoreUObject runs it (P9); only CoreUObject's types and test fixtures are reflected, the engine's `A`/`U` classes are naming only until P12 | [LeonHeaderTool/README.md](../../Engine/Source/Programs/LeonHeaderTool/README.md), [CoreUObject/README.md](../../Engine/Source/Runtime/CoreUObject/README.md) |
| Default subobjects | instanced from the archetype's subobjects (`FObjectInstancingGraph`) | every instance runs its constructor and builds its own subobjects; when an object is created from a template, a copied reference to a template subobject is redirected to the new object's subobject of the same name | plan decision D12: simpler, and the loaded properties are applied afterwards (P11) |
| Object names | `MakeUniqueObjectName` counts per outer; `StaticAllocateObject` replaces an existing object with the same name | numbered per class; creating an object whose name is taken is a fatal error | no replacement semantics without packages and GC |
| Object storage | `FUObjectArray` allocates chunks on demand up to `MaxObjectsInGame`, items carry a cluster index; name hash and per-class / per-outer hashes | a fixed array of `FPlatformProperties::MaxObjectsInGame` slots (8192 × 12 bytes on PS2, 131072 × 16 bytes on desktop; running out is fatal); one `TMultiMap` name hash; `GetObjectsOfClass` / `GetObjectsWithOuter` walk the array | fixed memory budget on the PS2 ([Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md)) |
| Garbage collection | mark and sweep over a token stream, disregard-for-GC pool, clusters, parallel reachability, `UGCObjectReferencer` | the same mark and sweep, single-threaded, over each class's list of strong reference properties; class default objects, native objects and compiled-in packages are roots by their flags (no disregard pool); no clusters; the `FGCObject` list lives in the collector; a pending-kill pointer in a set or a map key removes the element / pair instead of leaving a null key; `GARBAGE_COLLECTION_KEEPFLAGS` is `RF_NoFlags` | one EE thread; object counts of a game; no editor |
| Script | Blueprint VM (`FFrame::Code`, `ProcessInternal`) | native thunks only: `FFrame::Code` is always null and `ProcessEvent` calls the `exec` thunk | no Blueprints |
| Registration hook | `FModuleManager::OnProcessLoadedObjectsCallback` is a multicast event | a single function pointer, bound by CoreUObject | one listener; targets without CoreUObject pay a pointer and a branch |
| NoExport structs | UHT trusts the `NoExportTypes.h` declaration | the generated code takes the offsets from the C++ type and `static_assert`s the declared size, member types and offsets against it; `FMatrix` is not reflected | a drifting declaration fails the build instead of corrupting data |
| Script containers | `FScriptArray` / `FScriptSet` / `FScriptMap` with `TFunctionRef` callbacks | the same layouts and `static_assert`s; the set and map callbacks are template callables | keeps `Set.h` / `Map.h` free of `TFunctionRef` and the call indirection |
| Containers / strings | `TArray`, `TMap`, `FString` everywhere | the same everywhere since P6, enforced by `CheckBannedApis.ps1` (G4); third-party types stay at the library seams (Jolt, tinyobjloader, ufbx, cgltf), and the SSAO kernel keeps `std::mt19937` so its samples do not change | — |
| `FString` comparison | `==` ignores case | the same; code that needs an exact match (the level string table, mesh tags, editor class names, volume payloads) calls `Equals(…, ESearchCase::CaseSensitive)` | the pre-P6 `std::string` code compared case-sensitively |
| `TCHAR` | `wchar_t` / UTF-16 on most platforms | UTF-8 `char` on every platform; `TEXT(x)` is `x`; `WIDECHAR` only inside the Windows HAL; `TCHAR_TO_UTF8` & co. are identities | the EE has no wide-string support worth paying for; one encoding everywhere |
| `FName` pool | growing name blocks, `FNamePool` sized for desktop | 8-byte `FName`, hard-coded `EName` list; block size / count and hash buckets from `FPlatformProperties::NamePool*` (PS2: 16 KB blocks, at most 256 KB, 4096 buckets); exhausting the pool is fatal | fixed memory budget on 32 MB ([Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md)) |
| `FText` | localized text (`FTextLocalizationManager`, culture formatting) | minimal: `FromString`, `AsNumber`, `AsPercent`, `Format` (`{0}` arguments), `Join`; `LOCTEXT` / `NSLOCTEXT` keep the source text | no localization yet |
| Delegates | also dynamic (`DECLARE_DYNAMIC_*`) and `BindUFunction` | `TDelegate` / `TMulticastDelegate` with static, lambda, raw, SP and `UObject` bindings (+ payload) | no Blueprints to bind dynamic delegates or functions by name; `ProcessEvent` covers native calls |
| Console commands | `CPP_Default_` metadata fills missing trailing arguments; `IConsoleManager` console variables | a missing argument keeps its zero / default value and a warning goes to the output device; no console variables (the `gc.*` settings are read from the ini directly) | LeonHeaderTool generates no metadata; the console arrives with P13 |
| `FPlatformAtomics` on PS2 | real atomics | the generic non-atomic version | Leon runs a single EE thread |
| Automation tests | run by the session frontend / `-ExecCmds="Automation RunTests"` | `FAutomationTestFramework::RunTests(Filter)` from `LeonAutomationTests` (`-automation=<filter>`) and `TestPAL` (every platform) | no editor / session frontend |
| Math | `FVector`, `FRotator`, `FMatrix` everywhere, SIMD `VectorRegister`, `double` helpers | Core has the scalar float API (P3) and every module uses it (P5, P6), in UE's axes and centimetres since P7 | the EE has no SIMD path worth matching and a single-precision FPU |
| Field of view | `FMinimalViewInfo::FOV` is horizontal (the aspect ratio keeps X) | `UCameraComponent` keeps a vertical field of view (60°) and builds `FPerspectiveMatrix` from it | the legacy camera, the goldens and the captures use a vertical FOV; switching would change every frame |
| Depth | reversed Z (`FReversedZPerspectiveMatrix`, far at 0) | standard depth: `FPerspectiveMatrix` / `FOrthoMatrix`, z / w in [0, 1], 0 at the near plane | the GL renderer, its depth tests (`GL_LESS`) and the shadow comparisons use increasing depth |
| Clip space | the RHI hides the API's clip conventions | the renderer multiplies the UE projection by `ToGLClipSpace` (z_gl = 2z − w) once, and every pass works in GL clip space from there | the renderer calls OpenGL directly (see Renderer below) |
| Legacy data | assets are in UE space | `.llev` files and version-1 `.lmesh` files stay Y up in metres and are converted by `FLegacyCoordinateConversion` in their readers (the level saver converts back); G4 keeps the converter out of every other file | existing content keeps loading; the formats go away with the `.lasset` packages |
| Content facing | meshes face +X | legacy content meshes face +Y after the conversion, so a character's mesh sits at `RelativeRotation.Yaw = LegacyContentYaw` (−90) and the level reader maps an actor yaw ψ to 90 − ψ | the content was authored facing legacy +Z; re-exporting it is content work |
| Physics backend space | PhysX / Chaos work in the world's cm, Z up | the Jolt plugin keeps Jolt in metres, Y up: positions, extents, velocities and accelerations swap Y and Z and scale by 0.01 at the backend boundary; Jolt-side constants stay in metres | Jolt's tolerances are tuned for metres |
| Audio space | the listener and emitters are world positions | miniaudio stays right-handed, Y up, in metres behind the same swap and 0.01 scale | miniaudio's distance attenuation is tuned for metres |
| Mouse look | the player controller scales the mouse delta once (`InputYawScale` / `InputPitchScale`) | in `LeonGame` the engine's free look and `ADefaultGameMode` both apply 0.15° per pixel, so the view turns 0.3° per pixel | kept from the legacy engine so the feel does not change |
| Spring arm socket offset | `SocketOffset` moves only the end of the arm | the rotated `SocketOffset` is added to the arm origin, so the whole arm moves: the camera orbits and looks at the offset point, and the collision probe starts there | the legacy boom's `SocketOffsetX` / `SocketOffsetZ` offset its pivot the same way; the goldens record that behaviour |
| Spring arm lag | `FMath::VInterpTo` / `RInterpTo` toward the desired location and rotation | an exponential-smoothing alpha applied with `FMath::Lerp` (`A + t * (B - A)`) to the target, yaw / pitch and arm length; the pre-P6 code used `glm::mix` (`A * (1 - t) + B * t`), so the last bit of the lagged values can differ from earlier releases | `FMath::Lerp` is UE's formula; the smoothing itself is unchanged |
| Build tool | C# UBT | CMake scripts | no .NET dependency; PS2 toolchain is CMake-based |
| Linking | monolithic or DLLs | always static (`IS_MONOLITHIC=1`), generated module table | PS2 has no DLLs |
| Renderer | API-agnostic via RHI command lists | calls OpenGL directly | debt |
| Engine ↔ Renderer | acyclic | `CIRCULAR_DEPENDENCIES` | debt |
| PS2 gameplay | full framework on consoles | PS2 game uses `F*` types, no `AActor` | the gameplay framework is desktop-only (Engine depends on the OpenGL Renderer, UMG and AudioMixer) |
| Config layers | `Base.ini`, `Base<T>`, `Engine/Config/<P>/`, `Engine/Platforms/<P>/Config`, project `Default<T>`, `Config/<P>/`, `Platforms/<P>/Config`, `Saved/Config` (plus `NotForLicensees` / `Restricted` folders and a binary config cache) | the same order without `NotForLicensees` / `Restricted` or the binary cache; the `Saved/Config` user layer exists only on desktop; the PS2 reads the ini files through `host:` and keeps compiled defaults when they are missing | the PS2 build has no writable storage and PCSX2's host filesystem is optional |
| Config usage | `UPROPERTY(Config)` / `LoadConfig` everywhere, input from `BaseInput.ini` | `UPROPERTY(Config)` / `LoadConfig` / `SaveConfig` work (P10), but the engine still reads only a few keys by hand (map, resolution, stats, ThirdPerson tuning) | the engine classes become UObjects in P12; config-driven input comes in P13 |
| Config classes | `GlobalUserConfig` / `ProjectUserConfig` classes and `UpdateDefaultConfigFile` | not supported (LeonHeaderTool rejects the specifiers); `SaveConfig` writes the desktop user layer only | D8 has no per-user global layer; `Default<T>.ini` is edited by the editor module (P14) |
| `FString` in archives | ANSI when possible, else UTF-16 with a negative length | always UTF-8 with a positive length (including the terminator); a negative length is rejected | `TCHAR` is UTF-8 (D1) |
| `FName` in archives | an index into the package name table (the base `FArchive` does not store names) | the base `FArchive` writes it as a string | there are no packages until P11; the linker will replace this |
| `FPaths` directories | relative to the process (`../../../Engine/`) | absolute on desktop, built from the executable folder and the generated `GLeon*FromBaseDir` globals; on PS2 a staged layout under the ELF folder (`<Base>/Engine/`, `<Base>/<Project>/`) | independent of the working directory; PCSX2's `host:` is the ELF folder |
| `FDateTime` | Julian-day and `double` helpers | integer ticks only; the Julian-day helpers are left out | float-only math (D6) |
| JSON numbers | written with `%.17g` | the shortest `%.15g`–`%.17g` form that reads back to the same value | readable descriptors; still round-trips |
| Module platform lists | `WhitelistPlatforms` / `BlacklistPlatforms` (4.27) | `PlatformAllowList` / `PlatformDenyList` (UE 5 names) written; the 4.27 names are still read | the `.lplugin` files already used the UE 5 names |
| Plugin enable state | decides which plugin modules load | `IPluginManager` reports it, but LeonBuildTool alone decides what is linked (`ENABLE_PLUGINS`) | static linking, no module loading at runtime |
| Global `operator new` / `delete` | replaced through `FMemory` in every monolithic build (`REPLACEMENT_OPERATOR_NEW_AND_DELETE`) | replaced on the PS2 only (`PS2PlatformRuntime.cpp`) | keeps libstdc++'s allocation, unwinder and demangler code out of the ELF; desktop still uses the CRT |
| Capsule placement | `FCollisionShape` capsules are centered on the component | Leon's character capsule stands on the actor location (feet): it spans feet to feet + 2 × half height along Z | the CMC-lite works from the feet until the character becomes a UCapsuleComponent (P12) |
| Body collision shape | the body setup's aggregate geometry | `FBodyInstance::CollisionShape` (`EBodyCollisionShape::Box` / `TriangleMesh`) | Leon's physics scene has AABB and triangle-mesh bodies only |
| Bone matrices | `FTransform` bone poses | `FMatrix` poses in UE's row-vector convention (the FBX import builds them with `FQuatRotationMatrix` and converts them to world space by conjugation, B⁻¹ M B); the skin matrix is `InverseBind * BoneWorld` in FMatrix order | the renderer uploads them as they are; they become `FTransform` poses with the skeletal mesh assets (P14) |
| Widget colors | `FSlateColor` / `FLinearColor` with alpha | `FLinearColor`, alpha ignored by the debug overlay | the HUD overlay draws opaque RGB |
| Game → Launch | game modules never see `FEngineLoop` | the PS2 game module reads `GEngineLoop.GetMainWindow()` (include-only dependency on the launch module) | no Slate / `GEngine` on PS2 to hand out the viewport |
| Gamepad | `FSlateApplication` routes `IInputInterface` events to the player controller | game code polls `IInputInterface` state directly | no Slate; polling matches the PS2 frame loop |
