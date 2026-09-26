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
| `Plugins/RHI/OpenGL` | `OpenGLDrv` (device) + `Renderer` (GL renderer behind Engine's `IRendererModule` since P13) | debt: Renderer calls GL directly |
| `Plugins/RHI/PS2` | `Engine/Platforms/PS2/Source/Runtime/PS2RHI` | `FPS2RHI` static API |
| `Engine/Renderer` CPU side | `RenderCore` | |
| `Engine/Serialization` | `Json` | native since P4 (`FJsonObject`, `TJsonReader`, `TJsonWriter`, `FJsonSerializer`); `FJsonUtils` removed; served the cook recipes (P6 to P14) and `MaterialAsset` (until P14) |
| `Engine/Utilities` widgets | `UMG` (`U*` widgets); `TextLayout` → `SlateCore` | |
| `Engine/Utilities` HUD | `Engine` `GameFramework/HUD.h` | debt: Engine → UMG dependency |
| `Engine/Content` (C++) | `Engine` (`Private/Content`) | `ContentValidator` removed in 0.12.0 |
| `Engine/Animation` | `Engine` (`Classes/Animation`: the animation assets and anim instances, P14) + `AnimationCore` (their plain data); FBX skeletal import in `Developer/MeshUtilities` | cooked skeletal formats removed in 0.12.0 |
| `Engine/Network` | removed in 0.12.0 (local tag `archive/net-enet-0.11`) | replication returns later as UObject replication |
| `Engine/Audio` | `AudioMixer` (`FAudioDevice`) | UE keeps `FAudioDevice` in Engine; plays PCM16 from memory since P14 (`FSoundWavePCM`) |
| `Engine/Import` | `Developer/MeshUtilities` | |
| `Engine/Scene` | `Engine` (`ULevel`, `Public/Level`) | |
| `Engine/Gameplay` | `Engine` (`Classes/GameFramework`, `Components`, `Camera`, `Engine`, `Kismet`) + `AIModule` | |
| `leon_physics_iface` | `PhysicsCore` | `FHitResult` lives here (UE: Engine) |
| `Plugins/Physics/Arcade` | `Engine` (`FPhysScene`, `Private/PhysicsEngine`) | |
| `Plugins/Physics/Jolt` | plugin `Engine/Plugins/Runtime/JoltPhysics` | Win64 only |
| `Runtime/` (GameApplication, RunLeonGame) | `Launch` (`GuardedMain`, `FEngineLoop`) | |
| `Runtime/ProjectPack` (`leon.game.json`) | removed in 0.12.0 | `Projects` reads `.lproj` / `.lplugin` since P4 (`FProjectDescriptor`, `FPluginDescriptor`) |
| `Runtime/GameHostSession`, `WorldRuntime` | removed in 0.12.0 | `UEngine::LoadMap` opens one map (P13; `FGameApplication` in `Launch` until then) |
| `Tools/ResourceTools` | `Developer/Cooker` until P14, then `Editor/LeonEd` (`UCookCommandlet` and the other commandlets, the factories) | UE: UnrealEd; the recipes (`FCookRecipe`, `FCookPaths`) are gone |
| `Tools/AssetPipeline/leon-cook` | `Programs/LeonCook` | `UE4Editor-Cmd <Project> -run=<Commandlet>` equivalent (since P14) |
| `Editor/Importers` (the old editor's import dialog) | `Editor/LeonEd` factories (`UTextureFactory`, `UFbxFactory`, `UGLTFImportFactory`, `USoundFactory`) | UE: UnrealEd's factories |
| `Tools/Cli` (`leon-cli`) | removed | only forwarded to leon-cook |
| `Tests/` (Catch2) | `<Module>/Private/Tests/` + `Programs/LeonAutomationTests` | UE automation tests for Core (P2), Json, Projects (P4), PhysicsCore, RenderCore and AnimationCore (P5), and every other module (P6); Catch2 removed in P6 |
| — | `Programs/TestPAL` | UE `Programs/TestPAL`: runs the Core, CoreUObject, Json, Projects and PakFile automation tests on every platform (PS2 in PCSX2) |
| — | `PakFile` (P16) | UE `Runtime/PakFile`: `.lpak` files, `FPakPlatformFile` in the platform file chain; every platform |
| — | `Developer/TargetPlatform` (P16) | UE `Developer/TargetPlatform` with the `<Platform>TargetPlatform` modules folded in: Win64 and a PS2 stub |
| — | `Programs/LeonPak` (P16) | UE `Programs/UnrealPak` |
| — | `Engine/Build/BatchFiles/BuildCookRun.bat` (P16) | UE `RunUAT BuildCookRun` (AutomationTool): build, cook, stage, pak, run |
| — | `CoreUObject` (P9–P11) | UE `Runtime/CoreUObject`: `UObject`, reflection, `NewObject`, the object array, garbage collection, references, config and Exec, packages (`.lasset` / `.lmap`, linkers, `LoadObject`); every platform; since P12 Engine, AIModule and UMG are reflected on it (AnimationCore was from P12 to P14) |
| `ThirdParty/`, `Build/Dependencies.cmake` | `Engine/Source/ThirdParty/<Lib>/<Lib>.Build.cmake` | |
| `Engine/Assets` | `Engine/Content` + `Engine/Shaders` | |
| `Projects/Ps2ThirdPerson` | `Game/ThirdPerson` | isolated project |
| `Editor/` | removed; `Engine/Source/Editor` holds the editor module LeonEd since P14 | future OpenGL editor |

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
| `Texture`, `StaticMesh`, `SkeletalMesh`, `Material` | `UTexture2D`, `UStaticMesh`, `USkeletalMesh`, `UMaterial` (asset UObjects since P14; the GPU copies are the Renderer's), `FMaterial` (RenderCore's render values, `MaterialShared.h`) |
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
| | `NavigationSystem`, `NavMesh`, `InputMappingContext`, `PlayerInput` | `UNavigationSystem` (a waypoint graph since P20; `FNavMesh` removed), `UInputMappingContext`, `UPlayerInput` |
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
| `FDebugDraw` / `FDebugOverlay` with `glm::vec3` colors and `std::string` text | `FLinearColor` colors, `FString` text | `Renderer/Public/Debug/` (Engine `Public/Debug/` since P13) |
| `FResourceCache` returning `std::shared_ptr<UStaticMesh>`, `std::unordered_map` caches | `TSharedPtr<UStaticMesh>` / `TSharedPtr<UTexture2D>`, `TMap<FString, …>` caches | `Renderer/Public/ResourceCache.h` (Engine `Public/ResourceCache.h` since P13) |
| `UStaticMesh` / `USkeletalMesh` bounds as `glm::vec3` | `FVector` (`GetLocalMin`, `GetLocalMax`) | `Renderer/Public/StaticMesh.h`, `SkeletalMesh.h` (Engine `Classes/Engine/` since P13) |
| `ULevel` on `std::vector` / `std::string`, `std::size_t` mesh indices with `npos` | `TArray`, `FString`, `SIZE_T` mesh indices with `ULevel::Npos` | `Engine/Classes/Engine/Level.h` |
| `.llev` reader / writer on `std::ifstream` and `std::filesystem` | `FMemoryReader` / `FMemoryWriter` and `FFileHelper`; the same bytes, the string table stays case-sensitive | `Engine/Public/Level/LeonLevelFormat.h` |
| `.lmat` reader / writer on `std::string` streams | `FString` and `FFileHelper` with its own line parser (same rules: `#` / `;` comments, case-insensitive sections and keys) | `Renderer/Public/LeonMaterialFormat.h` (RenderCore since P13) |
| `PatchMaterialFromJson` / `HasMaterialSurfaceFields` on `nlohmann::json` | take a `const FJsonObject&` (`Json` module); missing or mistyped fields keep their value | `Renderer/Public/MaterialAsset.h` (Engine `Public/MaterialAsset.h` since P13) |
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
| — | `LeonGame -Screenshot=<file.bmp> -ExitAfterFrames=N`, `-AxesGizmo` / F6 axes gizmo (`FDebugDraw::AddAxes`, `AddViewAxes`) | `Launch/Private/Desktop/GameApplication.cpp`, `Renderer/Public/Debug/DebugDraw.h` (Engine since P13) |
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

### P11 — Packages

Saving and loading UObjects in `.lasset` / `.lmap` packages (plan decision D13), every platform (PS2 included). No
engine asset or map is a package until P14 / P15. Details:
[ASSET_FORMATS — Packages](../ASSET_FORMATS.md#packages--lasset--lmap),
[CoreUObject/README.md](../../Engine/Source/Runtime/CoreUObject/README.md).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| — | `FPackageFileSummary` (`PACKAGE_FILE_TAG` = `'LEON'`), `ELeonPackageVersion` (`VER_LEON_*`, `VER_LEON_LATEST`; UE: `EUnrealEngineObjectUE4Version`), `FEngineVersion` | `CoreUObject/Public/UObject/PackageFileSummary.h`; Core `UObject/ObjectVersion.h`, `Misc/EngineVersion.h` |
| — | `FPackageIndex`, `FObjectResource`, `FObjectImport`, `FObjectExport`, `RF_Load` | `ObjectResource.h`, `ObjectMacros.h` |
| — | `FLinker` (`ImportMap`, `ExportMap`, `NameMap`, `SoftPackageReferenceList`, `Imp` / `Exp` / `ImpExp`), `FLinkerLoad` (`CreateLinker`, `LoadAllObjects`, `CreateExport`, `CreateImport`, `Preload`, `Detach`, `FindExistingLinkerForPackage`), `FLinkerSave` (`ObjectIndicesMap`, `NameIndices`, `MapObject`, `BulkDataToAppend`), `BeginLoad` / `EndLoad` / `IsLoading`, `ResetLoaders`, `LogLinker` | `Linker.h`, `LinkerLoad.h`, `LinkerSave.h`, `Private/UObject/Linker*.cpp` |
| — | `UPackage::Save` / `SavePackage` (`ESaveFlags`, `FSavePackageResultStruct`, `ESavePackageResult`), the export / import tagging passes | `Package.h`, `SavePackage.h`, `Private/UObject/SavePackage.cpp` |
| `CreatePackage` only | `LoadPackage` (`ELoadFlags`), `StaticLoadObject`, `LoadObject<T>`, `StaticLoadClass`, `LoadClass<T>`, `FindPackage`; `UPackage::LinkerLoad`, `FileName`, `IsFullyLoaded`, `MarkAsFullyLoaded`, `ContainsMap` | `UObjectGlobals.h`, `Package.h` |
| `UObject::Serialize` stub, `PostLoad` never called | `UObject::Serialize` (tagged properties, then the class's native data), `SerializeScriptProperties`, `ConditionalPostLoad`, `IsAsset`; `GetArchetype` of a default subobject is its outer archetype's subobject | `Object.h`, `Private/UObject/Obj.cpp` |
| — | `FPropertyTag`; `UStruct::SerializeTaggedProperties` / `SerializeBin`; `UScriptStruct::SerializeItem` / `UseBinarySerialization`; `TStructOpsTypeTraits::WithSerializer` (`STRUCT_SerializeNative`); `FProperty::SerializeItem` for every property type, `ShouldSerializeValue`, `ConvertFromType` (`EConvertFromTypeResult`), `GetID`, `IsEditorOnlyProperty`, `ContainerPtrToValuePtrForDefaults` | `PropertyTag.h`, `Class.h`, `UnrealType.h`, `Private/UObject/Property*.cpp`, `Class.cpp` |
| `FArchive` names and texts as strings only | + virtual `operator<<(UObject*&)` and `GetLinker()` (no-ops in a plain archive), `UEVer()` / `LicenseeUEVer()`; `DECLARE_SERIALIZER` defines `Ar << Class*` | Core `Serialization/Archive.h`, `ObjectMacros.h` |
| in-memory soft references | `FSoftObjectPath::Serialize` / `SerializePath` (with the soft package references of a save), `TryLoad` and `LoadSynchronous` load packages | `SoftObjectPath.h`, `SoftObjectPtr.h` |
| — | `FByteBulkData` (`Lock` / `LockReadOnly` / `Unlock`, `Realloc`, `GetCopy`, `Serialize`, `BULKDATA_*`) | `CoreUObject/Public/Serialization/BulkData.h` |
| `FPaths` only | `FPackageName` (`GetAssetPackageExtension` `.lasset`, `GetMapPackageExtension` `.lmap`, `RegisterMountPoint` / `UnRegisterMountPoint`, `LongPackageNameToFilename`, `TryConvertFilenameToLongPackageName`, `IsValidLongPackageName`, `DoesPackageExist`, `ObjectPathToPackageName`, `GetShortName`, `SplitLongPackageName`, ...) | `CoreUObject/Public/Misc/PackageName.h` |
| — | 14 `System.CoreUObject.Package.*` tests with reflected fixtures (`PackageTestTypes.h`), a cross-platform golden hash | `CoreUObject/Private/Tests/PackageTest.cpp` |

### P12 — Gameplay framework as UObjects

The gameplay framework becomes `UCLASS` types owned through the world and the game instance and collected by the
garbage collector at safe points (plan decisions D11, D12, D17). Ownership, spawn and destroy:
[ARCHITECTURE.md §10](../ARCHITECTURE.md#10-gameplay-framework-engine-desktop); rules for gameplay code:
[CODING_STANDARD.md §4](../CODING_STANDARD.md#4-language).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `UStaticMeshComponent` (level POD) | `FLevelStaticMesh` (the `.llev` placed mesh, until P13's `AStaticMeshActor`) | `Engine/Classes/Engine/Level.h` |
| `ULevel` held by value in `UGameEngine` | `ULevel` (`UCLASS`; `OwningWorld`, `UPROPERTY() TArray<AActor*> Actors`; still carries the `.llev` content) | `Engine/Classes/Engine/Level.h` |
| `UWorld` a member of `AGameModeBase` | `UWorld` (`UCLASS`; `CreateWorld` in a transient `/Temp/Untitled_<N>` package, `DestroyWorld`, `PersistentLevel`, `AuthorityGameMode`, `GameState`, `OwningGameInstance`, `SetGameMode`, `BeginPlay`, `SpawnActor` / `SpawnActor<T>` / `SpawnActorDeferred`, `DestroyActor`, `EWorldType`) | `Engine/Classes/Engine/World.h`, `EngineTypes.h` |
| `TUniquePtr<T>` actors, `MakeUnique` spawn | `FActorSpawnParameters` (`Name`, `Template`, `Owner`, `Instigator`, `OverrideLevel`, `SpawnCollisionHandlingOverride`, `bDeferConstruction`, `bNoFail`, `ObjectFlags`), `ESpawnActorCollisionHandlingMethod` | `World.h` |
| `UGameInstance` (plain) | `UGameInstance` (`UCLASS`; `InitializeStandalone`, `FWorldContext` `USTRUCT` with `ThisCurrentWorld`) | `Engine/Classes/Engine/GameInstance.h` |
| `AActor` (plain, root by value) | `AActor` (`UCLASS`; `RootComponent`, `Tags`, `bHidden`, `Owner`, `Instigator`, `OwnedComponents`, `RegisterAllComponents`, `FindComponentByClass`, `PreInitializeComponents` / `InitializeComponents` / `PostInitializeComponents`, `FinishSpawning`, `DispatchBeginPlay`, `BeginPlay` / `EndPlay(EEndPlayReason)`, `Destroy` / `Destroyed`, `GetActorTransform`, …), `AInfo` | `Engine/Classes/GameFramework/Actor.h`, `Info.h` |
| `UActorComponent`, `USceneComponent` (plain) | `UCLASS`es: `bAutoRegister`, `bWantsInitializeComponent`, `ComponentTags`, `RegisterComponent` / `RegisterComponentWithWorld` / `UnregisterComponent`, `OnRegister`, `CreateRenderState_Concurrent`, `CreatePhysicsState`, `InitializeComponent`, `DestroyComponent`; `RelativeLocation` / `RelativeRotation` / `RelativeScale3D`, `SetupAttachment`, `AttachToComponent(Parent, FAttachmentTransformRules, SocketName)`, `DetachFromComponent(FDetachmentTransformRules)`, `AttachParent` / `AttachSocketName` / `AttachChildren`, `GetSocketTransform`, `GetComponentTransform` / `GetComponentToWorld`, `SetWorldLocation` … | `Components/ActorComponent.h`, `SceneComponent.h`, `Engine/EngineTypes.h` |
| — | `UPrimitiveComponent`, `UShapeComponent`, `UCapsuleComponent`, `UBoxComponent`, `USphereComponent`, `UMeshComponent`, `UStaticMeshComponent` | `Components/*.h` |
| `FSkelMeshAttachment` (a mesh glued to a bone) | a `UStaticMeshComponent` attached to the `USkeletalMeshComponent` at a bone socket | `Components/SkeletalMeshComponent.h` |
| `USkeletalMeshComponent`, `UCameraComponent`, `USpringArmComponent` (plain) | `UCLASS`es (the camera and the arm keep their Leon view logic) | `Components/SkeletalMeshComponent.h`, `Public/Camera/CameraComponent.h`, `GameFramework/SpringArmComponent.h` |
| `UCharacterMovementComponent` (a struct of tunables) | `UCharacterMovementComponent` (`UCLASS`) over `UPawnMovementComponent` over `UMovementComponent` (`UpdatedComponent`, `Velocity`) | `GameFramework/CharacterMovementComponent.h`, `PawnMovementComponent.h`, `MovementComponent.h` |
| `APawn`, `ACharacter` (plain) | `UCLASS`es: `ACharacter` default subobjects `CollisionCylinder` (root `UCapsuleComponent`), `CharMoveComp`, `CharacterMesh0`; `GetCapsuleComponent` | `GameFramework/Pawn.h`, `Character.h` |
| `AController` (not an actor) | `AController` actor (`Pawn`, `ControlRotation`, `PlayerState`, `InitPlayerState`, `OnPossess` / `OnUnPossess`), `APlayerController`, `AAIController` (AIModule) | `GameFramework/Controller.h`, `PlayerController.h`, `AIModule/Classes/AIController.h` |
| `AGameModeBase` (plain, owns the world) | `AGameModeBase` (`AInfo`; `GameStateClass`, `PlayerControllerClass`, `PlayerStateClass`, `DefaultPawnClass`, `HUDClass`, `StartPlay`); `AGameMode` with `MatchState` and the `MatchState` namespace (`EnteringMap`, `WaitingToStart`, `InProgress`, `WaitingPostMatch`, `LeavingMap`, `Aborted`) | `GameFramework/GameModeBase.h`, `GameMode.h` |
| `AGameStateBase`, `APlayerState` (plain, owned by `TUniquePtr`) | `AInfo` actors: `AGameStateBase` (`PlayerArray`), `AGameState` (`MatchState`, `PreviousMatchState`, `ElapsedTime`), `APlayerState` | `GameFramework/GameStateBase.h`, `GameState.h`, `PlayerState.h` |
| `AHUD` (plain) | `AHUD` actor; widgets are `UUserWidget` UObjects with the HUD as outer | `GameFramework/HUD.h`, UMG `Blueprint/UserWidget.h` |
| `UPlayerInput` (plain) | `UPlayerInput` (`UCLASS`) | `Public/GameFramework/InputMapping.h` |
| `UAnimInstance` (plain, `TUniquePtr`) | `UAnimInstance`, `UCharacterAnimInstance` (`UCLASS`; an inner object of the skeletal mesh component) | `AnimationCore/Public/SkeletalAnimation.h` |
| `dynamic_cast` (15 sites) | `Cast<T>` | Engine, AIModule |
| RTTI and exceptions on (MSVC defaults) | `/GR-`, no `/EH`, `_HAS_EXCEPTIONS=0` (MSVC); `-fno-rtti -fno-exceptions` (GCC / Clang); third-party keeps its flags (`leon_third_party_cxx_defaults`) | `LeonBuildTool/Configuration/CompileEnvironment.cmake` |
| no garbage collection in the engine | `CollectGarbage` after the world teardown and a level load; `UGameEngine::ConditionalCollectGarbage` (`FGarbageCollectionTimer`) after the world tick | `GameEngine.cpp`, `LeonLevelFormat.cpp` |
| `UWorld World;` in tests | `FScopedTestWorld` (create, destroy and collect around a test) | `Engine/Public/Tests/ScopedTestWorld.h` |
| — | 15 new `System.Engine.{World,Components,GameFramework}.*` tests (308 in total) | `Engine/Private/Tests/` |

### P13 — Levels as actors and the render boundary (part 1)

The `.llev` content becomes actors spawned by the level reader, physics bodies come from the components, and Engine
talks to the Renderer only through UE's interfaces (plan decision D16 for the volumes). Details:
[LEVELS.md](../LEVELS.md), [ARCHITECTURE.md §10–§12](../ARCHITECTURE.md#12-rendering-desktop). The second part
(`UEngine`, `LoadMap`, the viewport client, input and HUD by the player controller) follows.

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `FLevelStaticMesh` (level POD) | `AStaticMeshActor` (root `StaticMeshComponent0`, a `UStaticMeshComponent`) | `Engine/Classes/Engine/StaticMeshActor.h` |
| `FPlayerStart` | `APlayerStart` (`CollisionCapsule` 40 × 92, `PlayerStartTag`) | `Engine/Classes/GameFramework/PlayerStart.h` |
| `FAISpawnPoint` | `ATargetPoint` (the record tag in `Tags`) | `Engine/Classes/Engine/TargetPoint.h` |
| `FTriggerVolume`, `FPainCausingVolume`, blocking-volume meshes | `AVolume` (a `UBoxComponent` brush, `EncompassesPoint`, `GetBrushBounds`), `ATriggerVolume`, `ABlockingVolume`, `APainCausingVolume` (`bPainCausing`, `DamagePerSec`, `PainInterval`) | `Engine/Classes/GameFramework/Volume.h`, `PainCausingVolume.h`, `Engine/Classes/Engine/TriggerVolume.h`, `BlockingVolume.h` |
| `FDirectionalLight`, `FPointLight` (level PODs) | `ALight`, `ADirectionalLight`, `APointLight`; `ULightComponentBase` (`Intensity`, `LightColor`, `CastShadows`), `ULightComponent` (`GetDirection`), `ULocalLightComponent` (`AttenuationRadius`), `UDirectionalLightComponent` (`LightSourceAngle`), `UPointLightComponent` | `Engine/Classes/Engine/Light.h`, `DirectionalLight.h`, `PointLight.h`, `Engine/Classes/Components/*LightComponent*.h` |
| level name, game mode string, camera framing on `ULevel` | `AWorldSettings` (`DefaultGameMode`, `KillZ`), `ULevel::WorldSettings`; the legacy fields on its `ULegacyLevelDataComponent` | `Engine/Classes/GameFramework/WorldSettings.h`, `Engine/Public/Level/LegacyLevelDataComponent.h` |
| `ULevel::Npos`, `AActor::LevelMeshIndex`, `FBodyInstance::LevelMeshIndex`, `FHitResult` level index | `ComponentID` (the component's `GetUniqueID`), `FCollisionQueryParams::IgnoreComponentID`, `NoComponentID` | PhysicsCore `BodyInstance.h`, `CollisionQuery.h` |
| `UWorld::RegisterBodiesFromLevel`, `FPhysScene::SyncFromLevel` / `SyncToLevel` | `UPrimitiveComponent::CreatePhysicsState` / `DestroyPhysicsState`, `UActorComponent::RecreatePhysicsState`; `FPhysScene::AddComponentBody` / `RemoveComponentBody` / `GetBodyOwner` / `SyncComponentsToBodies` | `Components/PrimitiveComponent.h`, `Public/Physics/PhysScene.h` |
| collision and mobility flags on the level POD | `EComponentMobility`, `USceneComponent::SetMobility`; `ECollisionEnabled`, `UPrimitiveComponent::SetCollisionEnabled` / `SetSimulatePhysics` / `SetEnableGravity`; `FRotationConversionCache` | `Engine/EngineTypes.h`, `Components/SceneComponent.h`, `PrimitiveComponent.h` |
| `ULevel` snapshot lookups | `UGameplayStatics::GetAllActorsOfClass` / `GetAllActorsWithTag` | `Engine/Classes/Kismet/GameplayStatics.h` |
| `UWorld::Primitives`, `AddPrimitive`, `SubmitPrimitiveDraws`, `SubmitDraw` | `FSceneInterface` (`AddPrimitive`, `RemovePrimitive`, `UpdatePrimitiveTransform`, `AddLight`, `RemoveLight`, `UpdateLightTransform`), `UWorld::Scene`, `FPrimitiveSceneProxy`, `FStaticMeshSceneProxy`, `FSkeletalMeshSceneProxy`, `FLightSceneProxy`; `CreateSceneProxy`, `MarkRenderStateDirty`, `SendRenderTransform_Concurrent`, `SendRenderDynamicData_Concurrent`, `UWorld::SendAllEndOfFrameUpdates` | Engine `Public/SceneInterface.h`, `PrimitiveSceneProxy.h`, `StaticMeshSceneProxy.h`, `SkeletalMeshSceneProxy.h`, `LightSceneProxy.h` |
| `FSceneRenderer` in `UGameEngine`, `UGameEngine::GetRenderer` | `IRendererModule` (`AllocateScene`, `RemoveScene`, `BeginRenderingViewFamily`, `DrawCanvas`, Leon's `InitRenderer` / `ShutdownRenderer` / `ReloadShaders` / `GetFrameStats` / `ReadFramebufferBgr`), `GetRendererModule()`; the Renderer's `FRendererModule` and `FScene` | Engine `Public/RendererInterface.h`, Renderer `Private/RendererModule.h`, `ScenePrivate.h` |
| `FSceneRenderer::BeginFrame` / `DrawScene(Level, Camera)` | `FSceneViewFamily` (`ConstructionValues`, `EngineShowFlags`), `FSceneView`, `FSceneViewInitOptions`, `FEngineShowFlags` (`Bounds`, `AxesGizmo`) | Engine `Public/SceneView.h` |
| `FDebugOverlay` as the HUD's text backend, `AHUD::Paint(FDebugOverlay&, W, H)` | `FCanvas` (`DrawTile`, `DrawText`, `PushDepthSortKey`, `Flush_GameThread`), `AHUD::Paint(FCanvas&)`, `FPaintContext(FCanvas&)` | Engine `Public/CanvasTypes.h`, `GameFramework/HUD.h`, UMG `Blueprint/PaintContext.h` |
| `UStaticMesh`, `USkeletalMesh`, `UTexture2D`, `FResourceCache`, `MaterialAsset`, `FDebugDraw`, `FDebugOverlay` (Renderer) | the same names in Engine, CPU only (`Engine/StaticMesh.h`, `Engine/SkeletalMesh.h`, `Engine/Texture2D.h`); the GPU copies in the Renderer's `FRenderResourceCache` (`FStaticMeshRenderData`, `FSkeletalMeshRenderData`, `FTexture2DResource`) | Engine `Classes/Engine/`, `Public/`; Renderer `Private/` |
| — | `UWorld::LineBatcher` (an `FDebugDraw`) | `Engine/Classes/Engine/World.h` |
| `EShaderReloadResult`, `MakeReflectMatrix`, `FitLightSpaceMatrix`, the `.lmat` document (Renderer) | RenderCore `ShaderCore.h`, `ViewMatrices.h`, `LeonMaterialFormat.h` | `RenderCore/Public/` |
| — | `FModuleManager::GetModulePtr<T>` / `LoadModuleChecked<T>` | `Core/Public/Modules/ModuleManager.h` |
| — | `System.Engine.LevelFormat.SaveWritesTheSameBytes`, `System.Engine.Components.SceneProxiesFollowTheComponents` (310 tests) | `Engine/Private/Tests/` |

### P13 — The engine object, maps, input and the viewport client (part 2)

The game starts as a UE 4.27 game (plan decision D18 for the game mode and the engine class). Boot, `LoadMap`, input
and the console: [ARCHITECTURE.md §9–§10](../ARCHITECTURE.md#9-launch-and-the-engine-loop); the command line:
[SETUP.md](../SETUP.md#leongame).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `UGameEngine` (plain C++, an `FGCObject`), no `GEngine` | `UEngine` (`Abstract`, `Config=Engine`, `FExec`: `Init(IEngineLoop*)`, `Start`, `PreExit`, `Tick`, `DeferredCommands`, `TickDeferredCommands`, `Exec`, `Browse`, `LoadMap`, `SetClientTravel`, `TickWorldTravel`, `ConditionalCollectGarbage`, `AddOnScreenDebugMessage`, `GetWorldContextFromWorld`, `GameViewport`, `LocalPlayerClassName`, `GameViewportClientClassName`), `UGameEngine` (`GameInstance`), `GEngine`; `IEngineLoop` | `Engine/Classes/Engine/Engine.h`, `GameEngine.h`, `Engine/Public/UnrealEngine.h` |
| `FGameApplication` (`Launch/Private/Desktop`) | `FEngineLoop::PreInit` (window, `RHIInit`), `Init` (`GEngine` of `[/Script/Engine.Engine] GameEngine=`, `-ExecCmds`), `Tick`, `Exit` | `Launch/Private/LaunchEngineLoop.cpp`, `RHI/Private/RHIInit.cpp` |
| `FGenericWindow::InitRHI` / `ReleaseRHI` | `RHIInit` / `RHIExit`; the window only supplies its `GetRHIProcAddressLoader` and `BindRHIViewport` | `RHI/Public/DynamicRHI.h`, `ApplicationCore/Public/GenericPlatform/GenericWindow.h` |
| `-map=<.llev>`, `GameDefaultMap=LevelTemplates/Starter.llev` | `FURL` (`Map`, `Op`, `Portal`, `ETravelType`), `UGameInstance::StartGameInstance` (first token or `-map=`), `UEngine::Browse` / `LoadMap`, `FWorldContext::LastURL` / `TravelURL`; `GameDefaultMap=/Engine/LevelTemplates/Starter` | `Engine/Classes/Engine/EngineBaseTypes.h`, `Engine/Private/UnrealEngine.cpp`, `GameInstance.cpp` |
| `GameMapsSettings` keys read by hand | `UGameMapsSettings` (`GameDefaultMap`, `ServerDefaultMap`, `GlobalDefaultGameMode`, `GameInstanceClass`, `LocalMapOptions`, `GameModeMapPrefixes`, `GameModeClassAliases`), `UGeneralProjectSettings` | `EngineSettings/Classes/GameMapsSettings.h`, `GeneralProjectSettings.h` |
| `UWorld::SetGameMode(ADefaultGameMode::StaticClass())` | `UWorld::SetGameMode(FURL)` → `UGameInstance::CreateGameModeForURL` (`?game=`, `AWorldSettings::DefaultGameMode`, `GameModeMapPrefixes`, `GlobalDefaultGameMode`); `InitializeActorsForPlay`; `UWorld::BeginPlay` | `Engine/Classes/Engine/World.h`, `GameInstance.h` |
| `ADefaultGameMode::OnEnter` spawning the controller and the camera | `ULocalPlayer::SpawnPlayActor` → `UWorld::SpawnPlayActor` → `AGameModeBase::Login` / `InitNewPlayer` / `PostLogin` / `HandleStartingNewPlayer` / `RestartPlayer` / `FindPlayerStart` / `SpawnDefaultPawnFor` / `FinishRestartPlayer`; `UPlayer`, `ULocalPlayer`, `APlayerStartPIE` | `GameFramework/GameModeBase.h`, `Engine/Classes/Engine/Player.h`, `LocalPlayer.h`, `GameFramework/PlayerStartPIE.h` |
| `ADefaultCameraActor`, `ADefaultPlayerController` | `ADefaultPawn` + `UFloatingPawnMovement`, `APlayerController`, `APlayerCameraManager` (`FMinimalViewInfo`) | `GameFramework/DefaultPawn.h`, `FloatingPawnMovement.h`, `Camera/PlayerCameraManager.h`, `Public/Camera/CameraTypes.h` |
| `EKeys` enum, `IsKeyPressed(EKeys)` | `FKey` (`USTRUCT`, an `FName`), `FKeyDetails`, `EKeys` statics, `FInputCoreModule` | `InputCore/Classes/InputCoreTypes.h` |
| `UInputMappingContext`, `Leon::InputActions`, `FPlayInputTarget`, `RuntimeInput.h` | `UInputSettings` (`[/Script/Engine.InputSettings]`: `ActionMappings`, `AxisMappings`, `AxisConfig`, …), `UPlayerInput` (`ProcessInputStack`, `DebugExecBindings`, `GetBind`), `UInputComponent` (`BindAction`, `BindAxis`), `APlayerController::InitInputSystem` / `BuildInputStack` / `ProcessPlayerInput`, `APawn::SetupPlayerInputComponent` / `AddMovementInput` | `GameFramework/InputSettings.h`, `PlayerInput.h`, `Components/InputComponent.h`, `Engine/Config/BaseInput.ini` |
| the engine's `AHUD` and `UPlayerInput` | `APlayerController::MyHUD` (`ClientSetHUD`), `PlayerInput`, `PlayerCameraManager` | `GameFramework/PlayerController.h` |
| `UGameEngine::Render`, `HandleInput`, the F-key toggles | `UGameViewportClient` (`Init`, `ProcessInput`, `InputKey`, `InputAxis`, `Tick`, `Draw`, `Exec`, `EngineShowFlags`), `FViewport`, `FScreenshotRequest` | `Engine/Classes/Engine/GameViewportClient.h`, `Engine/Public/UnrealClient.h` |
| — | the console chain: `ULocalPlayer::Exec` → `UGameViewportClient::Exec` (`show`) → `UGameInstance` → `UEngine::Exec` (`exit`, `obj gc`, `stat`, `RecompileShaders`, `open`) → `FSelfRegisteringExec` → `UPlayer::Exec`; `APlayerController::FOV` | `Engine/Private/LocalPlayer.cpp`, `GameViewportClient.cpp`, `UnrealEngine.cpp`, `Player.cpp` |
| — | `System.Engine.URL.*`, `.EngineSettings.*`, `.LoadMap.*`, `.Input.*`, `.Console.ExecChain` (318 tests) | `Engine/Private/Tests/` |

### P14 — Asset classes (part 1)

The engine's assets become UObjects that save to `.lasset` packages, and the legacy files reach them through one
transitional loader; P14's second part (the LeonEd editor module, the factories and commandlets, the content migration)
follows. Details: [ASSET_FORMATS.md — Asset classes](../ASSET_FORMATS.md#asset-classes),
[ARCHITECTURE.md §12](../ARCHITECTURE.md#12-rendering-desktop).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `UTexture2D` (plain C++: `Create`, `CreateChecker`, `LoadFromFile`, RGBA8 `TArray`) | `UTexture` (`SRGB`, `UpdateResource`, `ReleaseResource`), `UTexture2D` (`CreateTransient`, `GetSizeX` / `GetSizeY`, `GetPixelFormat`, `GetNumMips`, `GetPlatformData`), `FTexturePlatformData`, `FTexture2DMipMap` (mip texels as `FByteBulkData`), `EPixelFormat` | `Engine/Classes/Engine/Texture.h`, `Texture2D.h`, `RenderCore/Public/PixelFormat.h` |
| `UStaticMesh` (plain C++: `CreateCpu`, `GetCpuData`, `GetSubmeshes`, `GetMaterials` of `FMaterial`s, `GetLocalMin` / `GetLocalMax`) | `UStaticMesh` (`StaticMaterials` of `FStaticMaterial`, `BodySetup`, `GetBoundingBox`, `GetBounds`, `GetNumSections`, `GetMaterial`, `HasValidRenderData`, `InitResources` / `ReleaseResources`; Leon's `BuildFromMeshData`, `GetLODResources`), `FStaticMeshLODResources` | `Engine/Classes/Engine/StaticMesh.h`, `Engine/Public/StaticMeshResources.h` |
| the physics scene reading the CPU mesh | `UBodySetup` (`AggGeom`, `CollisionTraceFlag`), `FKAggregateGeom`, `FKBoxElem`, `ECollisionTraceFlag` | `Engine/Classes/PhysicsEngine/BodySetup.h`, `AggregateGeom.h`, `BoxElem.h`, `BodySetupEnums.h` |
| `FMaterial` values (with `TSharedPtr<UTexture2D>` maps) on the meshes and components; `MaterialAsset.h`, `PatchMaterialFromJson` | `UMaterialInterface` (`GetMaterial`, `GetRenderProxy`, `GetUsedTextures`), `UMaterial` (the `.lmat` parameters as `UPROPERTY`s, `GetDefaultMaterial`), `EMaterialShadingModel`, `EMaterialDomain`; RenderCore's `FMaterial` stays the renderer's values (header `MaterialShared.h`, enum `EMaterialLightingModel`, raw `UTexture2D*` maps); the JSON material fields are gone | `Engine/Classes/Materials/MaterialInterface.h`, `Material.h`, `Engine/Classes/Engine/EngineTypes.h`, `RenderCore/Public/MaterialShared.h` |
| `USkeleton`, `UAnimSequence`, `UBlendSpace1D` (AnimationCore structs), `UAnimInstance` / `UCharacterAnimInstance` (AnimationCore), `FAnimJumpClips` | `USkeleton` (`GetReferenceSkeleton`, `Sockets`, `FindSocket`), `USkeletalMeshSocket`, `UAnimationAsset` → `UAnimSequenceBase` (`SequenceLength`, `RateScale`, `bLoop`) → `UAnimSequence` (`NumFrames`, `GetRawAnimationData`, `GetBonePose`), `UBlendSpaceBase` (`BlendParameters`, `SampleData`) → `UBlendSpace1D`; `UAnimInstance`, `UCharacterAnimInstance`, `FAnimJumpClips` (a `USTRUCT`) in Engine; AnimationCore keeps `FReferenceSkeleton`, `FRawAnimSequenceTrack`, `FRawAnimSequence`, `FSkeletalVertex`, `FSkeletalMeshData` | `Engine/Classes/Animation/*.h`, `Engine/Classes/Engine/SkeletalMeshSocket.h`, `AnimationCore/Public/SkeletalAnimation.h` |
| `USkeletalMesh` (plain C++, one `FMaterial`, the embedded clip) | `USkeletalMesh` (`Skeleton`, `Materials` of `FSkeletalMaterial`, `GetRefSkeleton`, Leon's `BuildFromImportData`) | `Engine/Classes/Engine/SkeletalMesh.h` |
| sounds played by file path | `USoundBase` (`Duration`), `USoundWave` (`NumChannels`, `SampleRate`, PCM16 bulk data) | `Engine/Classes/Sound/SoundBase.h`, `SoundWave.h` |
| — | `UDataAsset`, `UCommandlet` (`Main`, `ParseCommandLine`, `HelpDescription`, …) | `Engine/Classes/Engine/DataAsset.h`, `Engine/Classes/Commandlets/Commandlet.h` |
| `TSharedPtr<UStaticMesh>` / `TSharedPtr<USkeletalMesh>` on the components, `TArray<FMaterial>` overrides | `UStaticMeshComponent::StaticMesh`, `USkeletalMeshComponent::SkeletalMesh`, `UMeshComponent::OverrideMaterials` (`TArray<UMaterialInterface*>`), `GetMaterial` returning `UMaterialInterface*` (`UPROPERTY`s) | `Engine/Classes/Components/*.h` |
| `FResourceCache` (Engine's CPU loader and cache, `UGameEngine::GetResources`) | `FLegacyAssetLoader` (transitional: `LoadStaticMesh`, `LoadTexture`, `LoadMaterial`, `LoadSoundWave`, `LoadEngineObject`, `GetSphereMesh`); `UEngine::DefaultMaterialName` / `DefaultTextureName` / `DefaultBumpNormalTextureName` (`GlobalConfig`), `DefaultTexture`, `InitializeObjectReferences`; `LoadLevelFile(World, Path)`, `ApplyLevelDocument(World, Doc, Path)`, `MeshForBasicShape(Shape, …)` | `Engine/Public/LegacyAssetLoader.h`, `Engine/Classes/Engine/Engine.h`, `Engine/Config/BaseEngine.ini` |
| `FRenderResourceCache` entries pinning `TSharedPtr` assets | the cache keyed by asset, `IRendererModule::ReleaseAssetResources`, `ReleaseAssetRenderResources`; `FScene` is an `FGCObject` (`FPrimitiveSceneProxy::AddReferencedObjects`) | Renderer `Private/RenderResourceCache.h`, Engine `Public/RendererInterface.h`, `PrimitiveSceneProxy.h` |
| — | `System.Engine.Assets.*` (round trips, commandlet), `System.Engine.LegacyAssets.*`, `System.Engine.Animation.*` (moved from AnimationCore) (328 tests) | `Engine/Private/Tests/AssetTests.cpp`, `LegacyAssetLoaderTests.cpp`, `AnimationTests.cpp` |


### P14 — Editor module, import and content (part 2)

The LeonEd editor module imports source files into packages, LeonCook runs its commandlets, and the engine content is
`.lasset` packages; the legacy formats and the transitional loader are deleted. Details:
[ASSET_FORMATS.md — Importing assets](../ASSET_FORMATS.md#importing-assets), [TOOLS.md](../TOOLS.md#leoncook).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `Developer/Cooker` (`UCookCommandlet::Main(ArgC, ArgV)` with `staticmesh` / `recipe` modes, `FCookRecipe`, `FCookPaths`) | the editor module `LeonEd` (`TYPE Editor`, a new LeonBuildTool module type: desktop only, never in a game target); `UCookCommandlet : UCommandlet` (minimal before P16: saves with `PKG_FilterEditorOnly` / `PKG_Cooked` into `Saved/Cooked/<Platform>/`) | `Engine/Source/Editor/LeonEd/`, `Classes/Commandlets/CookCommandlet.h` |
| `LeonCook <mode> [options]` | `LeonCook [<Project>.lproj] -run=<Commandlet>`: the class `U<Name>Commandlet` found through reflection (`CommandletHelpers::FindCommandletClass`), `Main(Params)`, the error / warning summary | `Programs/LeonCook/Private/LeonCookMain.cpp`, `LeonEd/Public/CommandletHelpers.h` |
| `FStaticMeshBuilder::CookFrom*` (→ `.lmesh`), glTF `.lmat` writer | `FStaticMeshBuilder::BuildFromFile` / `BuildFromObj` / `BuildFromFbx` / `BuildFromGltf` (→ `FMeshData` with its material slots: names, values, map files) | `MeshUtilities/Public/StaticMeshBuilder.h`, `GltfImport.h` |
| `FLegacyAssetLoader::LoadTexture` / `LoadSoundWave` / `LoadStaticMesh` | `UFactory` (`SupportedClass`, `Formats`, `bCreateNew`, `bEditorImport`, `ImportPriority`, `AdditionalImportedObjects`, `CurrentFilename`, `FactoryCanImport`, `FactoryCreateNew`, `FactoryCreateBinary`, `FactoryCreateFile`, `StaticImportObject`, `CreateOrOverwriteAsset`), `UTextureFactory` (`ColorSpaceMode`, `ETextureSourceColorSpace`), `UFbxFactory` (`MeshTypeToImport` `EFBXImportType`, `Skeleton`, `bImportMaterials`; FBX and OBJ), `UGLTFImportFactory`, `USoundFactory`, `UMaterialFactoryNew` | `LeonEd/Classes/Factories/*.h` |
| `FLegacyAssetLoader::LoadMaterial`, `LeonMaterialFormat`, `LeonMeshFormat` | `ULegacyMaterialFactory` (`.lmat`), `ULegacyStaticMeshFactory` (`.lmesh` version 2) and `UMigrateLegacyContentCommandlet`, temporary | `LeonEd/Classes/Factories/Legacy*.h`, `Classes/Commandlets/MigrateLegacyContentCommandlet.h` |
| — | `FReimportHandler` (`CanReimport`, `SetReimportPaths`, `Reimport`, `GetPriority`), `FReimportManager` (`Instance`, `Reimport`), `EReimportResult` | `LeonEd/Public/EditorReimportHandler.h` |
| — | `UAssetImportData` (`SourceData` `FAssetImportInfo`, `Update`, `GetFirstFilename`, `ExtractFilenames`, `SanitizeImportFilename`, `ResolveImportFilename`), `AssetImportData` (`Instanced`, editor-only) on `UTexture`, `UStaticMesh`, `USkeletalMesh`, `UAnimSequence`, `USoundWave` | `Engine/Classes/EditorFramework/AssetImportData.h` |
| — | `UImportAssetsCommandlet` (`-source` / `-dest`, `-importlist=ImportList.ini`, `-reimport -all`), `UResavePackagesCommandlet`, `UValidateAssetsCommandlet` | `LeonEd/Classes/Commandlets/*.h` |
| `Engine/Content/Materials/*.lmat`, `Textures/T_Default_D.png`; the procedural defaults made in memory | `/Engine/EngineMaterials/M_Default`, `M_WorldGrid`, `M_SolidMetal`, `T_Default_D`, `T_Default_Bump_N`, `/Engine/EngineResources/DefaultTexture`, `/Engine/BasicShapes/Cube`, `Plane`, `Sphere` (`.lasset`); `Engine/SourceArt/` (`T_Default_D.png`, `ImportList.ini`) | `Engine/Content/`, `Engine/SourceArt/` |
| `FLegacyAssetLoader::LoadEngineObject`, `GetSphereMesh` | `LoadObject` (`UEngine::InitializeObjectReferences`, `UMaterial::GetDefaultMaterial`, `MeshForBasicShape`), `GetSphereMesh` (the package for 24 × 16, else built at run time) | `Engine/Private/UnrealEngine.cpp`, `Materials/Material.cpp`, `Level/BasicShape.cpp` |
| `ResolveLevelAssetPath` (a file path) | `ResolveLevelAssetObjectPath` (a package's object path), `FLegacyAssetKeys` (the key → package rule, until P15) | `Engine/Public/Level/LeonLevelFormat.h`, `LegacyAssetKeys.h` |
| `FAudioDevice::PlaySound2D` / `PlaySoundAtLocation` / `PlayMusic` by file path, UI `.wav` files looked up by path | `UGameplayStatics::PlaySound2D` / `PlaySoundAtLocation` (`USoundBase*`), `FAudioDevice` with `FSoundWavePCM` samples, `SetUiSound`; `UEngine::UIClickSoundName` & co. (`GlobalConfig`, empty: the procedural tones) | `Engine/Classes/Kismet/GameplayStatics.h`, `AudioMixer/Public/AudioDevice.h`, `Engine/Classes/Engine/Engine.h` |
| `FGenericWindow::SetIconFromFile(PngPath)` (stb_image) | `SetIcon(Width, Height, RGBA)` (texels, e.g. a texture's) | `ApplicationCore/Public/GenericPlatform/GenericWindow.h` |
| — | `CheckReimport.bat` (gate G5), `System.LeonEd.*`, `System.Engine.EngineContent.*`, `System.Engine.LegacyAssetKeys.*` (340 tests) | `Engine/Build/BatchFiles/`, `LeonEd/Private/Tests/`, `Engine/Private/Tests/` |

### P15 — Maps and the glTF map importer

A world saves and loads as a `.lmap` package, `UEngine::LoadMap` opens maps, LeonEd imports glTF scenes as maps, the
engine templates are maps, and the `.llev` levels, their reader and the legacy content tools are deleted. Details:
[LEVELS.md](../LEVELS.md), [ASSET_FORMATS.md — Maps](../ASSET_FORMATS.md#maps--lmap).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `.llev` levels: `LeonLevelFormat` (`FLevelDocument`, `SerializeLeonLevel`, `DeserializeLeonLevel`, `BuildLevelDocument`, `ApplyLevelDocument`), `LevelLoader` (`LoadLevelFile`) | `.lmap` map packages: `UPackage::SavePackage(Package, World, RF_Public \| RF_Standalone, "<Map>.lmap")` (`PKG_ContainsMap`), `LoadPackage` + `UWorld::FindWorldInPackage`, `UWorld::InitWorld` (`InitializationValues::InitializeScenes`), `UpdateWorldComponents`, `InitializeActorsForPlay` (UE's `RouteActorInitialize`), `ULevel::PostLoad`; `UWorld::PersistentLevel` and `ULevel::WorldSettings` saved | `Engine/Classes/Engine/World.h`, `Level.h` |
| `UEngine::LoadMap` finding a `.llev` (a long package name under a mount point, a file path, a content key) | `UEngine::LoadMap` loading a `.lmap` package by long package name or file (a file outside the mount points mounts the folder above its `Maps/` folder) | `Engine/Private/UnrealEngine.cpp` |
| `ULegacyLevelDataComponent`: spin, bob, trigger data, light orbit, game mode string, level name, camera framing | `URotatingMovementComponent` (UE's); Leon's `UBobbingMovementComponent`, `UOrbitMovementComponent`, `UInteractableComponent`; `AWorldSettings::DefaultGameMode`; the map's name; `ACameraActor` (UE's) holding the framing, and an `APlayerStart` at the view it opened with | `Engine/Classes/GameFramework/*MovementComponent.h`, `Components/InteractableComponent.h`, `Camera/CameraActor.h` |
| `APlayerStartPIE` spawned by `LoadMap` at a `.llev` framing | the map's first `APlayerStart`; `AGameModeBase::ChoosePlayerStart` takes the first start | `Engine/Private/GameFramework/GameModeBase.cpp` |
| `FLegacyAssetKeys`, `ResolveLevelAssetObjectPath` (content keys → packages) | object references saved in the map (imports of the mesh and material packages) | — |
| `Engine/Content/LevelTemplates/Blank.llev`, `Starter.llev`; `GameDefaultMap=/Engine/LevelTemplates/Starter` | `/Engine/Maps/Entry`, `/Engine/Maps/Template_Default` (UE's names), migrated with `MigrateLegacyContent -level=<file.llev> -dest=<Map>`; `GameDefaultMap=/Engine/Maps/Template_Default`, `ServerDefaultMap=/Engine/Maps/Entry` | `Engine/Content/Maps/`, `Engine/Config/BaseEngine.ini` |
| `FPaths::ResolveLegacyContentPath` (the shaders' `assets/Shaders/...` keys) | `FPaths::EngineDir()` / `Shaders` (UE: the `/Engine/Shaders` virtual folder) | `Engine/Private/GameEngine.cpp`, `Renderer/Private/*Renderer.cpp` |
| `FLegacyCoordinateConversion` in RenderCore (the level reader and saver, tests) | the same, test only: `RenderCore/Public/Tests`, `Private/Tests`, compiled with the automation tests; G4 allows it in test folders only | `RenderCore/Public/Tests/LegacyCoordinateConversion.h` |
| `UMigrateLegacyContentCommandlet`, `ULegacyMaterialFactory`, `ULegacyStaticMeshFactory` | removed once the content was migrated | — |
| — | `UGLTFMapFactory` (`-run=ImportAssets -type=Map -source=<file.glb> -dest=/Game/Maps/<Map>`), `UMapImportSettings` (`[/Script/LeonEd.MapImportSettings]`: `NodeRules` of `FMapImportNodeRule` with `EMapImportNodeKind`, `RequiredTags`), `Engine/Config/BaseEditor.ini` | `LeonEd/Classes/Factories/GLTFMapFactory.h`, `MapImportSettings.h` |
| — | `LoadGltfScene` (`FGltfScene`: nodes with world transforms and extras, meshes, KHR_lights_punctual lights) | `MeshUtilities/Public/GltfScene.h` |
| — | `ANavigationWaypoint` (`Links`, `Flags`; P20's `UNavigationSystem` builds its graph from them) | `Engine/Classes/AI/Navigation/NavigationWaypoint.h` |
| — | `UObject::Rename` (UE's, without flags or redirectors); `UWorld::AssetImportData` (editor only, an imported map's source) | `CoreUObject/Public/UObject/Object.h`, `Engine/Classes/Engine/World.h` |
| `EComponentMobility`, `ECollisionEnabled` (plain enums), the collision settings as plain members | reflected enum classes (`UENUM()`; UE's are namespaced); `Mobility`, `CollisionEnabled`, `bSimulatePhysics`, `bEnableGravity` are `UPROPERTY`s (UE keeps the last three in `FBodyInstance`) | `Engine/Classes/Engine/EngineTypes.h`, `Components/PrimitiveComponent.h` |
| — | `/Engine/Maps/AxisTest` from `Engine/SourceArt/Maps/AxisTest.glb` (`MakeAxisTest.py`); `System.Engine.MapPackage.*`, `System.Engine.AxisTestMap.NoMirroring`, `System.LeonEd.MapFactory.*`, `System.AIModule.LevelAndAISmoke.EditorStyleMapResaveHeadless` (339 tests) | `Engine/Private/Tests/`, `LeonEd/Private/Tests/`, `MeshUtilities/Private/Tests/Fixtures/MapFixture.gltf` |

### P16 — Cook, `.lpak` and staging

The cook targets a platform and follows the maps' dependencies, paks hold a staged build's files and mount in the
platform file chain, and `BuildCookRun.bat` stages a Shipping game that reads nothing but its pak. Details:
[ASSET_FORMATS.md — Paks](../ASSET_FORMATS.md#paks--lpak), [TOOLS.md — The cook](../TOOLS.md#the-cook),
[BUILD.md — Staging and Shipping](../BUILD.md#staging-and-shipping).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| — | `FPakPlatformFile` (`GetTypeName` "PakFile", `ShouldBeUsed`, `Initialize`, `Mount` / `Unmount`, `GetPakFolders`, `FindAllPakFiles`, `FindFileInPakFiles`, `IsNonPakFilenameAllowed`, `GetMountedPakFilenames`), `FPakFile` (`Find`, `Check`, `GetMountPoint`, `SetMountPoint`), `FPakEntry`, `FPakInfo` (`PakFile_Magic` = `'LPAK'`, 0x4B41504C) | `PakFile/Public/IPlatformFilePak.h` |
| — | `FPakWriter`, `FPakInputPair` (UnrealPak's `PakFileUtilities`: `CreatePakFile`, the response file) | `PakFile/Public/PakWriter.h` |
| — | `LeonPak <pak> -create=<list> [-align=] \| -list \| -test \| -extract=<dir>` (UnrealPak) | `Programs/LeonPak` |
| — | `FSHA1`, `FSHAHash` | `Core/Public/Misc/SecureHash.h` |
| — | `ITargetPlatform` (`PlatformName`, `DisplayName`, `IniPlatformName`, `HasEditorOnlyData`, `IsLittleEndian`, `RequiresCookedData`, `GetAllTextureFormats`, `GetAllWaveFormats`; Leon's `CookedPlatformName`, `GetCookNote`), `ITargetPlatformManagerModule` (`GetTargetPlatforms`, `FindTargetPlatform`, `GetActiveTargetPlatforms`, `GetRunningTargetPlatform`), `GetTargetPlatformManager` / `GetTargetPlatformManagerRef` | `Developer/TargetPlatform/Public/Interfaces/` |
| `UCookCommandlet`: every package under the mount points, a platform name for the folder | `UCookCommandlet` cooking by the book for a target platform: seeds from `MapsToCook` / every map under `/Game/Maps`, `DirectoriesToAlwaysCook` (`[/Script/UnrealEd.ProjectPackagingSettings]`), the config's paths (`GameDefaultMap`, `DefaultMaterialName`, ...); the dependency closure over the packages' tables (UE's `FPackageReader`); the config and shaders staged | `LeonEd/Classes/Commandlets/CookCommandlet.h` |
| the cook marking `UAssetImportData` objects transient | `UObject::IsEditorOnly` (UE's); `UAssetImportData::IsEditorOnly` true; `SavePackage` leaves editor-only objects out of a filtered package (UE: `IsEditorOnlyObject`) | `CoreUObject/Public/UObject/Object.h`, `Private/UObject/SavePackage.cpp` |
| `FPackageFileSummary::CookedPlatform` = the running platform | the cook's target platform (`UPackage::Save(..., CookedPlatformName)`) | `CoreUObject/Public/UObject/Package.h` |
| — | `Engine/Config/BaseGame.ini` (UE's; the packaging settings) | `Engine/Config/` |
| — | `FEngineLoop::PreInit` putting the pak platform file on the chain (UE: `LaunchCheckForFileOverride`) | `Launch/Private/LaunchEngineLoop.cpp` |
| — | `FPaths::IsStaged` (Leon), the staged layout's `../../../Engine/` | `Core/Public/Misc/Paths.h` |
| — | `UGameViewportClient::SetIgnoreInput` / `IgnoreInput` (UE's), set for an unattended run | `Engine/Classes/Engine/GameViewportClient.h` |
| — | `System.PakFile.*`, `System.Engine.PakFile.LoadsAssetFromPak`, `System.LeonEd.Cook.*`, `System.Core.Misc.SHA1`, `System.Engine.Viewport.IgnoreInput` (350 tests) | `PakFile/Private/Tests/`, `Engine/Private/Tests/`, `LeonEd/Private/Tests/`, `Core/Private/Tests/` |

### P17 — ShooterGame boot and a basic FPS

UE's collision channels and movement model in the engine, then the ShooterGame project on them: teams, team spawns,
bots that join, a first-person character with CS movement, the de_leon blockout and the G6 smoke. Details:
[ARCHITECTURE.md — Physics](../ARCHITECTURE.md#11-physics), [Character movement](../ARCHITECTURE.md#character-movement),
[ShooterGame README](../../Game/ShooterGame/README.md), [LEVELS.md — de_leon](../LEVELS.md#worked-example-de_leon).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `ECollisionChannel` with Leon's `Visibility`, `Camera`, `WorldStatic`, `WorldDynamic`, `Pawn` (a plain filter: a static body answered static traces, a dynamic body dynamic ones) | UE's `ECollisionChannel` (`ECC_WorldStatic` … `ECC_Destructible`, `ECC_EngineTraceChannel1..6`, `ECC_GameTraceChannel1..18`, `ECC_MAX`) and `ECollisionResponse` (`ECR_Ignore`, `ECR_Overlap`, `ECR_Block`), reflected | `PhysicsCore/Public/CollisionResponseContainer.h` |
| — | `FCollisionResponseContainer` (`SetResponse`, `SetAllChannels`, `GetResponse`, `GetDefaultResponseContainer`), `FCollisionResponseParams` (`DefaultResponseParam`), `FCollisionObjectQueryParams` (`AddObjectTypesToQuery`, `RemoveObjectTypesToQuery`, `IsValid`) | `PhysicsCore/Public/CollisionResponseContainer.h`, `CollisionQuery.h` |
| `FCollisionQueryParams::IgnoreComponentID` only | + `AddIgnoredActor`, `AddIgnoredComponent`, `ClearIgnoredActors` / `Components`, `bTraceComplex`, `TraceTag`, the `(TraceTag, bTraceComplex, IgnoreActor)` constructor | `PhysicsCore/Public/CollisionQuery.h` |
| `FHitResult` with a body index | + `Actor`, `Component` (`TWeakObjectPtr`), `GetActor`, `GetComponent`, `bBlockingHit` for `Multi` overlaps | `PhysicsCore/Public/CollisionQuery.h` |
| `FBodyInstance` shape and mobility | + `ObjectType`, `CollisionResponses`, `bQueryEnabled` / `bPhysicsEnabled` (UE's `ECollisionEnabled`), `GetResponseToChannel`, the upright `Capsule` shape (`OwnerID`: the owning actor) | `PhysicsCore/Public/BodyInstance.h` |
| `FPhysScene` traces with a channel filter | `LineTraceSingleByChannel` / `Multi`, `SweepSingleByChannel` / `Multi`, `Sphere` / `CapsuleTrace*ByChannel`, `LineTrace*ByObjectType` (UE's `UWorld` queries, on the scene; `UGameplayStatics` forwards) with `FCollisionResponseParams` | `Engine/Public/Physics/PhysScene.h`, `Engine/Classes/Kismet/GameplayStatics.h` |
| — | `UPrimitiveComponent::ObjectType`, `CollisionResponses`, `SetCollisionObjectType`, `SetCollisionResponseToChannel` / `ToChannels` / `ToAllChannels`, `GetCollisionResponseToChannel`, `SendPhysicsTransform`, `CanEverAffectNavigation` | `Engine/Classes/Components/PrimitiveComponent.h` |
| — | `UCollisionProfile` (`DefaultChannelResponses` of `FCustomChannelSetup`, `LoadProfileConfig`, `ReturnChannelNameFromContainerIndex`, `ReturnContainerIndexFromChannelName`), `[/Script/Engine.CollisionProfile]` | `Engine/Classes/Engine/CollisionProfile.h`, `Engine/Config/BaseEngine.ini` |
| the character's capsule: no body | a query-only `ECC_Pawn` body (UE's Pawn profile), `UCapsuleComponent::bBaseAtComponentLocation` (Leon: the capsule stands on its location) | `Engine/Classes/Components/CapsuleComponent.h`, `Private/GameFramework/Character.cpp` |
| `UCharacterMovementComponent` tunables: speed, jump, gravity, step, air control | + `MaxWalkSpeedCrouched`, `MaxAcceleration`, `GroundFriction`, `BrakingDecelerationWalking` / `Falling`, `BrakingFrictionFactor`, `BrakingFriction`, `bUseSeparateBrakingFriction`, `FallingLateralFriction`, `AirControlBoostMultiplier` / `VelocityThreshold`, `CrouchedHalfHeight`, `bWantsToCrouch`; `CalcVelocity`, `ApplyVelocityBraking`, `GetMaxSpeed` (virtual), `GetCurrentAcceleration`, `CanEverCrouch`, `IsCrouching`, `Crouch`, `UnCrouch`, `TickComponent`; Leon's `bInstantVelocity` | `Engine/Classes/GameFramework/CharacterMovementComponent.h` |
| — | `FNavAgentProperties` (`bCanCrouch`, …), `UPawnMovementComponent::NavAgentProps` | `Engine/Classes/AI/Navigation/NavigationTypes.h`, `PawnMovementComponent.h` |
| — | `ACharacter::Crouch` / `UnCrouch` / `CanCrouch` / `OnStartCrouch` / `OnEndCrouch` / `RecalculateBaseEyeHeight`, `bIsCrouched`, `CrouchedEyeHeight`; `APawn::AddMovementInput(WorldDirection, Scale, bForce)`, `GetPendingMovementInputVector`, `ConsumeMovementInputVector`, `BaseEyeHeight` | `Engine/Classes/GameFramework/Character.h`, `Pawn.h` |
| `UCameraComponent` placed by its owner | `bUsePawnControlRotation`, `GetCameraView`; `AActor::CalcCamera` asks the active camera component | `Engine/Public/Camera/CameraComponent.h` |
| — | `UPlayerInput::SetMouseSensitivity` (Exec) | `Engine/Classes/GameFramework/PlayerInput.h` |
| `AHUD::Paint` draws the widgets | `virtual DrawHUD`, `Canvas` (UE's order: `DrawHUD` before the widgets) | `Engine/Classes/GameFramework/HUD.h` |
| — | `UGameplayStatics::ParseOption`, `HasOption`, `GetIntOption` | `Engine/Classes/Kismet/GameplayStatics.h` |
| `UWorld::TickGameplayFrame`: character move → step → overlaps → actor tick | actor tick first (the movement in `UCharacterMovementComponent::TickComponent`, after the controller's input) → pawn separation → step → overlaps → sync; `UGameEngine::Tick` runs it | `Engine/Classes/Engine/World.h`, `Private/GameEngine.cpp` |
| map `RequiredTags` checked for every map | not for the engine's (`/Engine/`) maps: `UMapImportSettings::AppliesRequiredTags` | `LeonEd/Classes/Factories/MapImportSettings.h` |
| — | LeonBuildTool `AUTOMATION_TEST_MODULES` (Leon): a project's test program collects its own modules' tests | `Programs/LeonBuildTool/Configuration/TargetRules.cmake` |
| — | ShooterGame (UE's sample names): `AShooterGameMode`, `AShooterCharacter`, `UShooterCharacterMovement`, `AShooterPlayerController`, `AShooterAIController`, `AShooterPlayerState`, `AShooterHUD`, `EShooterTeam`; `COLLISION_WEAPON` is the config's `Weapon` channel (`ECC_GameTraceChannel1`) | `Game/ShooterGame/Source/ShooterGame/` |
| — | `SmokeTest.bat` (gate G6), `System.Engine.CollisionChannel.*`, `System.Engine.CharacterMovement.*` (the model), `System.LeonEd.MapFactory.EngineMapsSkipRequiredTags` (371 tests), `ShooterGame.*` (10) | `Engine/Build/BatchFiles/`, `Engine/Private/Tests/`, `Game/ShooterGame/Source/ShooterGame/Private/Tests/` |

### P18 — Weapons and damage

UE's damage API, projectile movement, spectating, life spans, sockets, a view model pass and world effects in the
engine; ShooterGame's weapons, health, armor, death and pickups on them. Details:
[ShooterGame README — Weapons](../../Game/ShooterGame/README.md#weapons).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `ACharacter::TakeDamage(float)`, `Health`, `Die`, `Revive`, `IsAlive` | `AActor::TakeDamage(Damage, FDamageEvent, EventInstigator, DamageCauser)`, `InternalTakePointDamage`, `InternalTakeRadialDamage`, `bCanBeDamaged`, `OnTakeAnyDamage` / `OnTakePointDamage` / `OnTakeRadialDamage` (native multicast delegates); the health is the game's pawn's (UE ShooterGame) | `Engine/Classes/GameFramework/Actor.h` |
| — | `FDamageEvent`, `FPointDamageEvent`, `FRadialDamageEvent`, `FRadialDamageParams` (plain structs, UE's ids), `UDamageType` | `Engine/Public/Engine/DamageEvents.h`, `Engine/Classes/GameFramework/DamageType.h` |
| `UGameplayStatics::ApplyPointDamage(ACharacter*, …)`, `ApplyRadialDamage(TArray<ACharacter*>, …)` | UE's `ApplyDamage`, `ApplyPointDamage`, `ApplyRadialDamage`, `ApplyRadialDamageWithFalloff` (line of sight on a channel), `GetWorldFromContextObject` | `Engine/Classes/Kismet/GameplayStatics.h` |
| — | `UProjectileMovementComponent` (no homing, no sliding, no interpolated component) | `Engine/Classes/GameFramework/ProjectileMovementComponent.h` |
| — | `ASpectatorPawn`, `USpectatorPawnMovement`, `AGameModeBase::SpectatorClass`, `AController::ChangeState` / `GetStateName` / `IsInState`, `NAME_Playing` / `NAME_Spectating` / `NAME_Inactive` (Engine's names), `APlayerController::BeginSpectatingState` / `SpawnSpectatorPawn` (Leon: the controller possesses its spectator pawn) | `Engine/Classes/GameFramework/SpectatorPawn.h`, `Controller.h`, `PlayerController.h` |
| — | `AActor::SetLifeSpan`, `InitialLifeSpan`, `LifeSpanExpired` (counted in the actor's tick) | `Engine/Classes/GameFramework/Actor.h` |
| — | `UStaticMeshSocket`, `UStaticMesh::Sockets` / `FindSocket`, `USceneComponent::GetSocketTransform` / `DoesSocketExist` (the static mesh component's sockets); glTF `SOCKET_<Name>` nodes (UE's FBX socket prefix) | `Engine/Classes/Engine/StaticMeshSocket.h`, `MeshUtilities/Private/GltfImport.cpp` |
| — | `bOnlyOwnerSee`, `bOwnerNoSee` (against `FSceneView::ViewActor`), Leon's `bRenderAsViewModel` and `UCameraComponent::ViewModelFOV` (UE has neither: a game draws its first-person mesh with its own FOV) | `Engine/Classes/Components/PrimitiveComponent.h`, `Renderer/Private/SceneRenderer.cpp` |
| — | Leon's world effects: `UGameplayStatics::SpawnImpactMark` (UE: `SpawnDecalAtLocation`), `SpawnTracer` (UE: a beam emitter), `SpawnPointLightAtLocation` | `Engine/Public/Effects/WorldEffects.h`, `Renderer/Private/WorldEffectsRenderer.cpp` |
| a native CDO copied its parent's config members after its constructor | UE: a native class's defaults are its constructor chain's (`bShouldInitializeProperties` false for `CLASS_Native`), then the config | `CoreUObject/Private/UObject/Class.cpp` |
| — | ShooterGame (UE's sample names): `AShooterWeapon`, `AShooterWeapon_Instant`, `AShooterWeapon_Projectile`, `AShooterProjectile`, `AShooterCharacter` health / inventory / `Die`, `AShooterGameMode::CanDealDamage` / `Killed`; CS's weapons `AShooterWeapon_Pistol`, `_Rifle`, `_Sniper`, `_Grenade` | `Game/ShooterGame/Source/ShooterGame/` |
| — | `System.Engine.Damage.*`, `ProjectileMovement.*`, `SpectatorPawn.*`, `Sockets.*`, `Actor.LifeSpan`, `System.Renderer.ViewModel.*` / `Effects.*`, `System.LeonEd.Factories.GltfSockets`, `System.CoreUObject.Config.SubclassConstructorDefaults` (383 tests), `ShooterGame.*` (19) | `*/Private/Tests/` |

### P19 — Rounds, money, buying and the bomb

Counter-Strike's defusal rules on UE's match states. Details:
[ShooterGame README — Rounds](../../Game/ShooterGame/README.md#rounds-money-and-the-bomb).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `AShooterGameMode`: teams and spawns | + UE's `ReadyToStartMatch` / `HandleMatchHasStarted` / `HandleMatchHasEnded` drive CS's rounds (`StartRound`, `EndRound`, `CheckRoundEnd`), `InitGame`'s `?seed=`, the money, `Buy`, the bomb's events, `bot_kick`, `mp_restartgame` | `Game/ShooterGame/Source/ShooterGame/` |
| `AGameState` | `AShooterGameState` (UE ShooterGame's): the round, the score, the bomb, the kill feed | same |
| `AShooterPlayerState`: the team | + the money, `ScoreKill` / `ScoreDeath` (UE ShooterGame's `NumKills`, `NumDeaths`) | same |
| — | `AShooterBomb` (CS's C4; UE ShooterGame has no bomb), `UShooterBuyMenuWidget` (a UMG `UUserWidget`), `AShooterPlayerController::Buy` / `Give` / `God` / `Kill` (UE: the cheat manager's `God`) | same |
| `ACharacter::IntegrateVertical` traced the floor from the moved feet | from where the feet began the step (UE sweeps the move) | `Engine/Private/GameFramework/Character.cpp` |
| — | `System.Engine.CharacterMovement.LongFramesKeepTheFloor` (384 tests), `ShooterGame.*` (28) | `*/Private/Tests/` |

### P20 — Bots and the waypoint navigation

The grid navmesh of the legacy engine gives way to a waypoint graph (UE3's path nodes; UE 4.27 navigates on a Recast
navmesh, which Leon does not have), and ShooterGame's bots get their brains. Details:
[ShooterGame README — Bots](../../Game/ShooterGame/README.md#bots), [LEVELS — Waypoint links](../LEVELS.md#waypoint-links).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| `UNavigationSystem` over `FNavMesh` (a grid baked from the static bodies' boxes) | `UNavigationSystem` over the level's `ANavigationWaypoint` graph: `Build`, `FindPath` (A*), `ProjectPointToNavigation`, `FindPathToLocationSynchronously` (UE's, returning a `UNavigationPath`), `CanWalkBetween` (a capsule sweep and a floor probe), `AutoLinkWaypoints`, `FWaypointLinkParams` | `Engine/Public/AI/Navigation/NavigationSystem.h`, `Engine/Classes/AI/Navigation/NavigationPath.h` |
| `FNavMesh` | removed | — |
| — | `UMapImportSettings::bAutoLinkWaypoints` (the import links the waypoints; the agent: `FWaypointLinkParams::FromConfig`, `[/Script/Engine.NavigationSystem]`) | `LeonEd/Classes/Factories/MapImportSettings.h` |
| `UBlackboardComponent`: booleans | UE's typed keys: `SetValueAsBool` / `Int` / `Float` / `Vector` / `Object` / `Name`, `GetValueAs*`, `IsValueSet`, `ClearValue` | `AIModule/Classes/BehaviorTree/BehaviorTree.h` |
| — | `UPawnSensingComponent` (UE's: `SightRadius`, `SetPeripheralVisionAngle`, `HearingThreshold`, `LOSHearingThreshold`, `OnSeePawn`, `OnHearNoise`), `AActor::MakeNoise` | `AIModule/Classes/Perception/PawnSensingComponent.h`, `Engine/Classes/GameFramework/Actor.h` |
| `AAIController`: path following | + jumping up a path's rise, repathing when stuck | `AIModule/Private/AIController.cpp` |
| `AShooterAIController`: a team, no behaviour | CS's bot: a behavior tree (buy, engage, defuse, plant, fetch the bomb, investigate, the objective), the aim model, `Difficulty` | `Game/ShooterGame/Source/ShooterGame/` |
| — | `AShooterGameMode::GetTerroristTargetSite`, `bot_stop`; `AShooterWeapon::FireNoiseLoudness` | same |
| `System.AIModule.Golden.AIControllerArrives`, `System.Engine.Golden.NavigationFindPath` (the grid's tables) | removed; `System.AIModule.Gameplay.NavigationAutoLinkStepsJumpsAndDrops`, `System.AIModule.Blackboard.TypedKeys`, `System.AIModule.PawnSensing.*`, `System.LeonEd.MapFactory.AutoLinksWaypoints` (386 tests), `ShooterGame.Bots.*` (33) | `*/Private/Tests/` |

### P21 — Bot matches, budgets and release 0.20.0

Hardening: whole matches of bots in CI. Details:
[ShooterGame README — Bot match](../../Game/ShooterGame/README.md#bot-match).

| Leon (before) | UE name (now) | Where |
| --- | --- | --- |
| headless steps always paced to the clock | `FApp::IsBenchmarking` (UE's `-benchmark`): unpaced fixed steps | `Core/Public/Misc/App.h`, `Launch/Private/LaunchEngineLoop.cpp` |
| `FPlatformMisc::RequestExitWithStatus`: the code only when forced | + a non-forced exit records its code (`GetRequestedEngineExitCode`), which `FEngineLoop::GetExitCode` returns | `Core/Public/CoreGlobals.h`, `Launch/Public/LaunchEngineLoop.h` |
| — | `AShooterGameMode`'s `-botmatch` / `-rounds=` / `-seed=`, `FShooterMatchChecker` (Leon's; UE ShooterGame has no bot match) | `Game/ShooterGame/Source/ShooterGame/` |
| — | `BotMatch.bat`; CI's bot match and staged Shipping bot match | `Engine/Build/BatchFiles/`, `.github/workflows/ci.yml` |
| — | `ShooterGame.Bots.MatchCheckerFlagsViolations` (34 ShooterGame tests) | `*/Private/Tests/` |

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

The legacy angles (of the `.llev` levels until P15, of the golden tables still) convert as: actor yaw ψ → `90 − ψ`; orbit camera (yaw Y, pitch P) → `FRotator(−P, Y + 180, 0)`;
free look → `FRotator(P, Y, 0)`; directional light → `FRotator(−P, 90 − Y, 0)`; spin rates change sign. Details and
the converters' allowed places: [ARCHITECTURE.md — Coordinates](../ARCHITECTURE.md#coordinates).

## Deviations from UE 4.27 (intentional)

| Topic | UE | LeonEngine | Why |
| --- | --- | --- | --- |
| Reflection | `UCLASS`, `UObject`, UHT | LeonHeaderTool generates UE 4.27-shaped `.generated.h` / `.gen.cpp` (no metadata, no hot-reload CRCs, explicit `RegisterReflection_<Module>` instead of static `FCompiledInDefer` objects) and CoreUObject runs it (P9); the gameplay framework, the widgets and the anim instances are reflected since P12, the level content actors, the engine, the viewport client, the players and the settings since P13, the assets since P14; `UNavigationSystem` and the behavior tree lite are still plain C++ with U names | [LeonHeaderTool/README.md](../../Engine/Source/Programs/LeonHeaderTool/README.md), [CoreUObject/README.md](../../Engine/Source/Runtime/CoreUObject/README.md) |
| World contexts | `UEngine::WorldList` holds the `FWorldContext`s | each `UGameInstance` owns its `FWorldContext` (P12); `UEngine::GetWorldContexts` collects them | one game instance per engine; no editor or PIE contexts |
| World package | `LoadMap` loads the world from its `.umap` package | the same since P15: `LoadPackage` of the `.lmap`, `UWorld::FindWorldInPackage`, `InitWorld`, then `InitializeActorsForPlay` registers and initializes the actors; a map file outside the mount points mounts the folder above its `Maps/` folder; `BeginPlay` comes after the login, as in UE | scratch content opens from the command line |
| Navigation | a Recast navmesh (`UNavigationSystemV1`, `ARecastNavMesh`) built from the level's geometry | a graph of `ANavigationWaypoint` actors (UE3's path nodes), linked by the map import (`bAutoLinkWaypoints`, P20) | a Counter-Strike map needs a few dozen nodes; no navmesh build or Recast dependency, and a graph fits the PS2 budget |
| Process exit code | `GuardedMain` returns its own error level | `FEngineLoop::GetExitCode`: the loop's failure, else the code a non-forced `FPlatformMisc::RequestExitWithStatus` recorded (P21) | a game (the bot match) fails its process after a clean shutdown |
| Startup map failure | `StartGameInstance` falls back to the default map (or asks) | the error is logged and the game exits with code 1 | a script with a wrong map must stop |
| Map on the command line | the first token | the first token (a leading `.lproj` is skipped), or `-map=<map>` | the scripts and CI already use `-map=` |
| `FURL` | parses protocol, host, port, map, options, portal; the default constructor fills the default map | map, options and portal only; `FURL()` leaves the map empty (the parsing constructor fills `GameDefaultMap`); the map keeps a file path (a `.lmap`) as given | no networking; keeps struct defaults free of config reads |
| Player start choice | `ChoosePlayerStart` prefers a Play From Here start (`APlayerStartPIE`, editor play sessions only), then picks a random unoccupied start | the first player start in level order; there is no `APlayerStartPIE` (the maps migrated from the `.llev` templates have the view their framing opened with as their first start); ShooterGame takes the first free start tagged with the team, in level order | deterministic; no editor |
| Login | `Login(UPlayer*, ENetRole, Portal, Options, FUniqueNetIdRepl, ErrorMessage)`, `PreLogin` | `Login(UPlayer*, Portal, Options, ErrorMessage)`; no `PreLogin`, net roles or unique ids | no networking |
| Game mode choice | `UWorld::SetGameMode(FURL)` asks the game instance (`?game=`, world settings, map prefixes, `GlobalDefaultGameMode`) | the same (D18); a map saves its world settings' `DefaultGameMode` | — |
| Actor root | an actor may have no root component | every actor gets a `USceneComponent` `DefaultSceneRoot` unless a subclass skips it (`DoNotCreateDefaultSubobject(AActor::DefaultSceneRootName)`, as `ACharacter` does for its capsule) | every Leon actor has a transform (`SetActorLocation` on a rootless actor would do nothing) |
| Component transform | `ComponentToWorld` cached and updated by `UpdateComponentToWorld`; `RelativeLocation` / `RelativeRotation` / `RelativeScale3D` private behind accessors (4.24+) | computed on demand by `GetComponentTransform` / `GetComponentToWorld`; the `Relative*` members are public (the 4.27 accessors exist too); a root's location and rotation are its relative ones, returned without a quaternion round trip | the movement edits the root in place and must read the exact values back; nothing to keep in sync |
| Spawn during a tick | the actor joins `ULevel::Actors` and begins play at once | it waits in `UWorld::PendingSpawnActors` and joins the level (and begins play) when the tick ends; a destroy during a tick nulls the level slot, compacted afterwards | the world ticks by walking the level array, and the previous engine behaved the same |
| Actor tick | `FActorTickFunction` / `FActorComponentTickFunction` registered with the world's tick task manager, tick groups; a pawn's tick depends on its controller's | `UWorld::Tick` walks the level: each actor ticks its registered components that enabled their tick, then `Tick` (`bCanEverTick`; `AInfo` actors do not tick, except the game mode); a possessed pawn met before its controller ticks right after it; the camera managers update after the actors | one thread, no tick groups or prerequisites yet |
| Game state clock | `AGameState` counts `ElapsedTime` with a timer; `AGameStateBase` has no match clock | `AGameStateBase` keeps Leon's match clock (`HandleMatchHasStarted` / `HandleMatchHasEnded`, seconds while in progress) ticked by the game mode, and `AGameState` counts whole seconds in its `Tick` | the default game mode and its tests use the base clock |
| Accessor shapes | `GetCharacterMovement()`, `GetMesh()`, `GetGameState<T>()` return pointers; `AController::GetPlayerState<T>()` | `ACharacter::GetCharacterMovement()` / `GetMesh()` and `AGameModeBase::GetGameState()` return references (the default subobjects and the game state always exist); `GetGameState<T>()`, `GetPlayerState<T>()`, `GetRootComponent()`, `GetCapsuleComponent()` return pointers as in UE | keeps the golden tests unchanged |
| Character movement | `UCharacterMovementComponent` runs the movement (`PerformMovement`, `PhysWalking`, …) and keeps `MovementMode`, `Velocity`, `CurrentFloor` | the component holds the tunables (`UPROPERTY`s), UE's velocity model (`CalcVelocity`, `ApplyVelocityBraking`) and crouching, and ticks; `ACharacter` still runs the kinematic movement and keeps its state | moving the code is a behaviour-neutral refactor left for later; the goldens pin the current code |
| Velocity model | acceleration, friction and braking always (`MaxAcceleration` 2048, `GroundFriction` 8, `BrakingDecelerationWalking` 2048) | `bInstantVelocity` (Leon, the default) moves at the speed along the input at once, as the legacy CMC lite did; `false` runs UE's model with UE's defaults (ShooterGame does) | the golden tables and the captures pin the instant model |
| Crouch base | a crouch keeps the capsule's centre, and `bCrouchMaintainsBaseLocation` (on by default on the ground) moves it down | the character stands on its feet, so on the ground the feet stay (the same result) and in the air the capsule shrinks around its centre (the feet come up by the difference) | Leon's capsule is placed by its feet |
| Pawn input | `AddMovementInput(WorldDirection, Scale, bForce)` on every pawn | the same on `APawn`; `ACharacter` keeps the legacy one-argument `AddMovementInput(WishDirXY)` (a wish kept until changed, the goldens use it), which hides the pawn's overload: a character's input calls `APawn::AddMovementInput` | the golden tests drive characters through the legacy call |
| Owner-only visibility | `bOwnerNoSee` / `bOnlyOwnerSee` on primitives | not implemented: ShooterGame hides the local player's body (`SetVisibility(false)`) and shows the others' | the renderer has no owner filter; one local player |
| Movement classes | `UMovementComponent` → `UNavMovementComponent` → `UPawnMovementComponent`; `USkinnedMeshComponent` between `UMeshComponent` and `USkeletalMeshComponent` | no `UNavMovementComponent` or `USkinnedMeshComponent` | nothing uses them yet |
| Render state | proxies are created on the game thread and handed to the render thread; `MarkRenderStateDirty` defers the recreation to the end of the frame, and only components that asked for it send their transforms (`bRenderTransformDirty`) | no render thread: the proxies are created, updated and read on the game thread; `MarkRenderStateDirty` recreates the proxy at once; `UWorld::SendAllEndOfFrameUpdates` sends every registered component's transform and dynamic data before each frame | one thread; the moved components did not track dirtiness before P13 |
| Scene order | `FScene` keeps primitives in an unordered array and the renderer sorts its draws | `FScene` keeps its primitives and lights ordered by owner actor (`GetUniqueID`, the spawn serial) and component index | the draw order, the shadow fit and the lights must match the level order the captures were made with |
| GPU resources | `UStaticMesh::RenderData`, `UTexture2D::Resource`: the asset owns its render resource, created and released on the render thread (`BeginInitResource`, `BeginReleaseResource`, fences before `FinishDestroy`) | the Renderer's private `FRenderResourceCache` keeps the GPU copy of each asset, keyed by the asset, and makes it the first time a proxy or a material uses it; the asset frees it when its data changes and in `BeginDestroy` (`IRendererModule::ReleaseAssetResources`); no render thread, everything on the game thread; `FScene` reports its proxies' assets to the garbage collector (pending kill or not) instead of fencing | Engine must not include Renderer headers (the GL code lives in the Renderer), and there is no render thread to fence |
| Materials | `UMaterial` is a graph of material expressions compiled to shaders (`FMaterial`, shader maps), with material instances on top | fixed shading models (`MSM_DefaultLit`: Leon's Blinn-Phong, `MSM_Unlit`) with the `.lmat` parameters and maps as `UPROPERTY`s; `GetRenderProxy` returns RenderCore's `FMaterial` values; no instances, blend modes or domains beyond `MD_Surface` | a forward renderer with fixed shaders; the PS2 draws the same fixed set |
| Asset data | textures keep source art, compression, LODs and streaming mips; static meshes keep source models and several LODs; animations keep compressed local keys; sounds keep the imported `.wav` and cooked compressed data | a texture keeps mip 0 (RGBA8, the renderer makes the mipmaps); a static mesh one LOD; an animation one model-space `FMatrix` key per bone and frame, and the reference skeleton keeps the inverse bind pose; a sound keeps PCM16; the meshes' and clips' CPU arrays go through bulk data only in packages | the renderer, the physics scene and the anim instances read those arrays; compression and streaming come with the PS2 cook |
| Legacy content | — | P14 and P15 migrated it (`MigrateLegacyContent`: `.lmat` / `.lmesh` / image / `.wav` files into packages, then the `.llev` levels into maps) and deleted the files and the tools | no legacy format is left |
| Editor module | `UnrealEd` is a large editor module; the editor executable runs commandlets (`UE4Editor-Cmd`) | `LeonEd` (`TYPE Editor`) has the factories, reimport and the commandlets only; `LeonCook` is its command-line host, without renderer | no editor UI yet |
| Factories and reimport | a `UReimport*Factory` subclass per importable class implements `FReimportHandler`; a factory's automated settings come from JSON; `NewObject` over an existing object replaces it | the import factories implement `FReimportHandler` themselves (a fresh factory reimports with the recorded settings); settings are the factory's `UPROPERTY`s set from ImportList.ini / switch text; `CreateOrOverwriteAsset` refills the existing object (Leon cannot construct over one) | fewer classes; references to an asset survive its reimport |
| Import data | `UAssetImportData` stores its source data as JSON with timestamps, and typed subclasses (`UFbxAssetImportData`, ...) hold the settings; the path is relative to the package | `SourceData` is reflected (`FAssetImportInfo`, `FAssetImportSourceFile`: relative path, MD5 hex), no timestamps, and `ImportSettings` is a sorted string map; the path is relative to the engine or project folder | reproducible reimports (gate G5): nothing that changes between runs or machines is saved |
| Cooked editor-only objects | `UAssetImportData::IsEditorOnly` keeps the objects out of cooked packages | the cook marks the import data objects transient before saving with `PKG_FilterEditorOnly` | no `IsEditorOnly` virtual on `UObject` (CoreUObject and the PS2 ELF stay unchanged) |
| Basic shapes | `/Engine/BasicShapes` are imported meshes | saved once from Leon's procedural generators; a sphere of another tessellation is built at run time (`GetSphereMesh`) | identical frames: the generator's geometry, bit for bit |
| UI sounds | Slate styles name their sounds | `[/Script/Engine.Engine] UI*SoundName` (empty: procedural tones); `FAudioDevice` keeps the cue samples | the engine ships no UI sounds (no licensed ones) |
| Renderer interface | `IRendererModule` has no renderer start-up, stats or read-back | Leon's `IRendererModule` adds `InitRenderer` / `ShutdownRenderer` (the GL objects on the window's context), `ReloadShaders`, `GetFrameStats`, `ReadFramebufferBgr` (screenshots) and `DrawCanvas` (UE draws the canvas through its render items) | no RHI command lists yet |
| Viewport | `FViewport` / `FSceneViewport` over a Slate `SViewport`; the viewport client receives input events | `FViewport` wraps the main window (size, `Draw`: canvas, `UGameViewportClient::Draw`, screenshots, present); `UGameViewportClient::ProcessInput` polls the window's keys and mouse each frame and calls `InputKey` / `InputAxis` | no Slate |
| Scene view | `FSceneViewInitOptions` takes the view origin and rotation, and the FOV is horizontal | the view takes the camera's whole view matrix, projection and vertical FOV (`FSceneView::FromCamera`); the show flags hold `Bounds` (F1), `Collision` (F2), `Navigation` (F3) and Leon's `AxesGizmo` (F6); `Collision` and `Navigation` are not drawn yet | Leon's camera builds its own matrices (orbit and free look); see Field of view |
| Canvas | `FCanvas` renders batched `FCanvasItem`s (tiles, triangles, text with `UFont`s) into a render target | `FCanvas` keeps tiles, thick lines and text in the HUD bitmap font (stb_easy_font), batched by depth sort key (higher first; inside a batch tiles, then lines, then texts), and draws them in one alpha-blended pass | the HUD and the debug text drew that way before P13; the captures match |
| Line batcher | `UWorld::LineBatcher` is a `ULineBatchComponent` | an `FDebugDraw` member drawn by the Renderer's line pass | nothing needs it as a component yet |
| Physics state | components create `FBodyInstance`s (`CreatePhysicsState`) | `CreatePhysicsState` adds a body to the world's `FPhysScene` when collision is on; the body is keyed by the component's `GetUniqueID` (`ComponentID`), and only simulated (dynamic) bodies move their components after a step; the character capsule is swept, not simulated, and since P17 is a query-only upright capsule body moved by `SendPhysicsTransform` | the bodies keep the arcade scene's shapes (AABB, triangle mesh, upright capsule) |
| Collision profiles | named profiles (`BlockAll`, `Pawn`, `OverlapAllDynamic`, …) in `DefaultEngine.ini` set a component's object type and responses | `UCollisionProfile` reads the named channels (`DefaultChannelResponses`) only; a component sets its type and responses itself, and the engine classes set UE's profile values in their constructors | enough for the game's channels; profiles come when a project needs them |
| Collision responses | a body's response to a query's channel and the query's response to the body's type, min of both | the same; a raw body added without a component (the tests' bodies) ignores the other mobility's world channel (the pre-P17 filter), a component is `ECC_WorldStatic` blocking everything; the character's capsule ignores `ECC_Pawn` besides UE's `ECC_Visibility` (pawns separate by `ResolvePawnOverlap`); a non-`Block` config default applies to the bodies made after the config loads | every golden table and capture stays as it was |
| `Multi` queries | stop at the first blocking hit and return the overlaps before it plus that block | every blocking and overlapping hit along the segment, nearest first | the navigation's and the tests' callers read every hit |
| Query order of parameters | `(OutHit, Start, End, TraceChannel, Params, ResponseParam)` | Leon's debug-draw argument stays after `Params`, and `ResponseParam` comes last | the existing callers keep their argument order |
| Map required tags | — (UE has no such check) | a project's `RequiredTags` apply to its maps only, not to the engine's `/Engine/` maps | `-reimport -all` reimports the engine content with a project's rules |
| Project tests | a project's tests are compiled into its editor / game and run by the session frontend | a Program target (`<Project>Tests`) links the engine's test runner and collects only the project's modules' tests (`AUTOMATION_TEST_MODULES`) | the engine's tests run once, in `LeonAutomationTests` |
| Game units | UE's ShooterGame values in cm | ShooterGame's movement is CS 1.6's with 1 unit = 2.54 cm (the player 72 units = 183 cm): 250 u/s = 635 cm/s | the plan's CS feel in UE's centimetres |
| Collision defaults | `UPrimitiveComponent` follows its collision profile (`BlockAllDynamic` for a static mesh, `BlockAll` for a blocking volume) | every primitive starts with `NoCollision`; the map importer turns `QueryAndPhysics` on for its static meshes, `ABlockingVolume` for its brush, and a map saves each component's settings | a character's capsule must not become a body |
| Mobility | `EComponentMobility::Static`, `Stationary`, `Movable` | `Static` and `Movable` only | no baked lighting to tell stationary lights apart |
| Volumes | `AVolume` is an `ABrush` with BSP geometry; `APainCausingVolume` derives from `APhysicsVolume` | plan decision D16: boxes (a `UBoxComponent` brush of 100 cm scaled by the actor, containment tested on the axis-aligned box), `APainCausingVolume` derives from `AVolume`; a blocking volume's body is its brush box, and a blocking volume is never drawn; the map importer sizes a volume from its node's mesh bounds | no BSP |
| Player start | `APlayerStart` derives from `ANavigationObjectBase` | from `AActor`, with the same `CollisionCapsule` (40 × 92) and `PlayerStartTag` | no navigation object base yet |
| Map import | Datasmith or the glTF importer's level import (metadata); `UCX_` / `UBX_` prefixes hard-coded in the FBX importer | a LeonEd factory with naming rules in the Editor config (a prefix → an engine class and tags, `Kind` for `UCX_` / `COL_`) and the project's `RequiredTags`; a `UCX_` piece is its bounding box; lights take glTF's intensity as it is; external images only | plan decision D15; Leon's physics has boxes and triangle meshes, no convex hulls |
| Relative rotation on save | a scene component saves `RelativeRotation` and rebuilds the quaternion | the relative quaternion is saved after the tagged properties (a native tail) | a loaded transform is bit-exact (the migrated maps draw the same pixels) |
| Light colour | `ULightComponentBase::LightColor` is an sRGB `FColor` | an `FLinearColor` (maps store linear floats); the proxy's colour is `LightColor * Intensity` | the colours round-trip exactly |
| Map data with no UE class | — | `UBobbingMovementComponent`, `UOrbitMovementComponent` and `UInteractableComponent` (the legacy levels' bob, light orbit and trigger data, P15), `ANavigationWaypoint` (the waypoint graph, P20) | the maps keep what the levels stored; UE has no counterpart |
| Actor tags | `FName` tags, compared without case | the same; the map importer adds a rule's tags and a node's suffix (plan decision D15) | UE semantics |
| Default light | a level without lights is dark | the same since P15 (the `.llev` reader spawned a default sun; the migrated maps saved it as a light) | UE semantics |
| Camera manager | every player controller spawns its `APlayerCameraManager` in `PostInitializeComponents` | only a local player's controller does, when it gets its player (`SetPlayer`) | nothing views through a remote controller |
| Default pawn input | `ADefaultPawn` registers its own `DefaultPawn_*` axis mappings (`InitializeDefaultPawnInputBindings`); `UFloatingPawnMovement` accelerates (4000 cm/s²) and brakes | `ADefaultPawn` binds `MoveForward`, `MoveRight`, `MoveUp`, `Turn`, `LookUp` (and the rates) from `BaseInput.ini`; `UFloatingPawnMovement` moves at `MaxSpeed` (800 cm/s) along the normalized input, with no acceleration | the legacy fly camera moved that way; the captures and the WASD test pin it |
| Look input | `AddYawInput` / `AddPitchInput` scale by `InputYawScale` (2.5) / `InputPitchScale` (−2.5) and accumulate until `UpdateRotation` | the scale is 1 and the input goes into the control rotation at once (pitch clamped to the camera manager's limits); the `AxisConfig` sensitivity (0.3) makes pixels degrees | the mouse keeps the legacy 0.3° per pixel |
| On-screen help text | — | the default pawn's first on-screen message still reads `DefaultCameraActor -- mouse look, WASD fly, Q/E up/down` | the frame captures include it |
| `-ExecCmds` | commands separated by `,` | `;` or `,` | the scripts used `;` |
| Capsule placement | `FCollisionShape` capsules are centered on the component | Leon's character capsule (the root `UCapsuleComponent` since P12) stands on the actor location (feet): it spans feet to feet + 2 × half height along Z | the CMC lite and the goldens work from the feet |
| Default subobjects | instanced from the archetype's subobjects (`FObjectInstancingGraph`) | every instance runs its constructor and builds its own subobjects; when an object is created from a template, a copied reference to a template subobject is redirected to the new object's subobject of the same name; a load reuses the subobject its outer's constructor built and applies the saved properties to it (delta'd against the archetype's subobject) | plan decision D12: simpler, and the loaded properties are applied afterwards (P11) |
| Object names | `MakeUniqueObjectName` counts per outer; `StaticAllocateObject` replaces an existing object with the same name | numbered per class; creating an object whose name is taken is a fatal error; a package load reuses an object of the same name and class already in memory (a default subobject) and skips an export whose name is taken by another class | no replacement semantics |
| Object storage | `FUObjectArray` allocates chunks on demand up to `MaxObjectsInGame`, items carry a cluster index; name hash and per-class / per-outer hashes | a fixed array of `FPlatformProperties::MaxObjectsInGame` slots (8192 × 12 bytes on PS2, 131072 × 16 bytes on desktop; running out is fatal); one `TMultiMap` name hash; `GetObjectsOfClass` / `GetObjectsWithOuter` walk the array | fixed memory budget on the PS2 ([Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md)) |
| Garbage collection | mark and sweep over a token stream, disregard-for-GC pool, clusters, parallel reachability, `UGCObjectReferencer` | the same mark and sweep, single-threaded, over each class's list of strong reference properties; class default objects, native objects and compiled-in packages are roots by their flags (no disregard pool); no clusters; the `FGCObject` list lives in the collector; a pending-kill pointer in a set or a map key removes the element / pair instead of leaving a null key; `GARBAGE_COLLECTION_KEEPFLAGS` is `RF_NoFlags` | one EE thread; object counts of a game; no editor |
| Script | Blueprint VM (`FFrame::Code`, `ProcessInternal`) | native thunks only: `FFrame::Code` is always null and `ProcessEvent` calls the `exec` thunk | no Blueprints |
| Registration hook | `FModuleManager::OnProcessLoadedObjectsCallback` is a multicast event | a single function pointer, bound by CoreUObject | one listener; targets without CoreUObject pay a pointer and a branch |
| NoExport structs | UHT trusts the `NoExportTypes.h` declaration | the generated code takes the offsets from the C++ type and `static_assert`s the declared size, member types and offsets against it; `FMatrix` is not reflected | a drifting declaration fails the build instead of corrupting data |
| Script containers | `FScriptArray` / `FScriptSet` / `FScriptMap` with `TFunctionRef` callbacks | the same layouts and `static_assert`s; the set and map callbacks are template callables | keeps `Set.h` / `Map.h` free of `TFunctionRef` and the call indirection |
| Containers / strings | `TArray`, `TMap`, `FString` everywhere | the same everywhere since P6, enforced by `CheckBannedApis.ps1` (G4); third-party types stay at the library seams (Jolt, tinyobjloader, ufbx, cgltf), and the SSAO kernel keeps `std::mt19937` so its samples do not change | — |
| `FString` comparison | `==` ignores case | the same; code that needs an exact match (the level string table, editor class names, volume payloads) calls `Equals(…, ESearchCase::CaseSensitive)`; actor tags are `FName`s since P13 (see Actor tags) | the pre-P6 `std::string` code compared case-sensitively |
| `TCHAR` | `wchar_t` / UTF-16 on most platforms | UTF-8 `char` on every platform; `TEXT(x)` is `x`; `WIDECHAR` only inside the Windows HAL; `TCHAR_TO_UTF8` & co. are identities | the EE has no wide-string support worth paying for; one encoding everywhere |
| `FName` pool | growing name blocks, `FNamePool` sized for desktop | 8-byte `FName`, hard-coded `EName` list; block size / count and hash buckets from `FPlatformProperties::NamePool*` (PS2: 16 KB blocks, at most 256 KB, 4096 buckets); exhausting the pool is fatal | fixed memory budget on 32 MB ([Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md)) |
| `FText` | localized text (`FTextLocalizationManager`, culture formatting) | minimal: `FromString`, `AsNumber`, `AsPercent`, `Format` (`{0}` arguments), `Join`; `LOCTEXT` / `NSLOCTEXT` keep the source text | no localization yet |
| Delegates | also dynamic (`DECLARE_DYNAMIC_*`) and `BindUFunction` | `TDelegate` / `TMulticastDelegate` with static, lambda, raw, SP and `UObject` bindings (+ payload) | no Blueprints to bind dynamic delegates or functions by name; `ProcessEvent` covers native calls |
| Console commands | `CPP_Default_` metadata fills missing trailing arguments; `IConsoleManager` console variables; `UEngine::Exec` knows hundreds of commands; no console window without `UConsole` | a missing argument keeps its zero / default value and a warning goes to the output device; no console variables (the `gc.*` settings are read from the ini directly); `UEngine::Exec` handles `exit` / `quit`, `obj gc`, `stat unit` / `stat fps`, `RecompileShaders`, `open`; the world settings answer too; commands come from `-ExecCmds` and `DebugExecBindings` (no console window yet) | LeonHeaderTool generates no metadata; no `UConsole` / UMG console yet |
| `FPlatformAtomics` on PS2 | real atomics | the generic non-atomic version | Leon runs a single EE thread |
| Automation tests | run by the session frontend / `-ExecCmds="Automation RunTests"` | `FAutomationTestFramework::RunTests(Filter)` from `LeonAutomationTests` (`-automation=<filter>`) and `TestPAL` (every platform) | no editor / session frontend |
| Math | `FVector`, `FRotator`, `FMatrix` everywhere, SIMD `VectorRegister`, `double` helpers | Core has the scalar float API (P3) and every module uses it (P5, P6), in UE's axes and centimetres since P7 | the EE has no SIMD path worth matching and a single-precision FPU |
| Field of view | `FMinimalViewInfo::FOV` is horizontal (the aspect ratio keeps X) | `UCameraComponent` keeps a vertical field of view (60°) and builds `FPerspectiveMatrix` from it | the legacy camera, the goldens and the captures use a vertical FOV; switching would change every frame |
| Depth | reversed Z (`FReversedZPerspectiveMatrix`, far at 0) | standard depth: `FPerspectiveMatrix` / `FOrthoMatrix`, z / w in [0, 1], 0 at the near plane | the GL renderer, its depth tests (`GL_LESS`) and the shadow comparisons use increasing depth |
| Clip space | the RHI hides the API's clip conventions | the renderer multiplies the UE projection by `ToGLClipSpace` (z_gl = 2z − w) once, and every pass works in GL clip space from there | the renderer calls OpenGL directly (see Renderer below) |
| Legacy data | assets are in UE space | the same since P15: no file is in the legacy Y-up metre space; the golden tests convert their tables with the test-only `FLegacyCoordinateConversion` (G4 keeps it in tests) | the tables were recorded before P7 and are never edited |
| Content facing | meshes face +X | legacy content meshes face +Y after the conversion, so a character's mesh sits at `RelativeRotation.Yaw = LegacyContentYaw` (−90), and a legacy actor yaw ψ is 90 − ψ | the content was authored facing legacy +Z; re-exporting it is content work |
| Physics backend space | PhysX / Chaos work in the world's cm, Z up | the Jolt plugin keeps Jolt in metres, Y up: positions, extents, velocities and accelerations swap Y and Z and scale by 0.01 at the backend boundary; Jolt-side constants stay in metres | Jolt's tolerances are tuned for metres |
| Audio space | the listener and emitters are world positions | miniaudio stays right-handed, Y up, in metres behind the same swap and 0.01 scale | miniaudio's distance attenuation is tuned for metres |
| Spring arm socket offset | `SocketOffset` moves only the end of the arm | the rotated `SocketOffset` is added to the arm origin, so the whole arm moves: the camera orbits and looks at the offset point, and the collision probe starts there | the legacy boom's `SocketOffsetX` / `SocketOffsetZ` offset its pivot the same way; the goldens record that behaviour |
| Spring arm lag | `FMath::VInterpTo` / `RInterpTo` toward the desired location and rotation | an exponential-smoothing alpha applied with `FMath::Lerp` (`A + t * (B - A)`) to the target, yaw / pitch and arm length; the pre-P6 code used `glm::mix` (`A * (1 - t) + B * t`), so the last bit of the lagged values can differ from earlier releases | `FMath::Lerp` is UE's formula; the smoothing itself is unchanged |
| Build tool | C# UBT | CMake scripts | no .NET dependency; PS2 toolchain is CMake-based |
| Linking | monolithic or DLLs | always static (`IS_MONOLITHIC=1`), generated module table | PS2 has no DLLs |
| Renderer | API-agnostic via RHI command lists | calls OpenGL directly | debt |
| Engine ↔ Renderer | acyclic: the Renderer depends on Engine | the same since P13; UMG depends on Engine circularly (Engine's reflected `AHUD` needs UMG's types first, so the ordered edge is Engine → UMG) | LeonHeaderTool orders the reflected modules by their non-circular edges |
| PS2 gameplay | full framework on consoles | PS2 game uses `F*` types, no `AActor` | the gameplay framework is desktop-only (Engine depends on the OpenGL Renderer, UMG and AudioMixer) |
| Header tool exceptions | UnrealHeaderTool builds with exceptions (`bEnableExceptions`) while engine modules do not | the same: `LeonHeaderTool`, a std-only host program, aborts a parse with an exception; every Leon module and program builds without RTTI or exceptions (D17) | — |
| Config layers | `Base.ini`, `Base<T>`, `Engine/Config/<P>/`, `Engine/Platforms/<P>/Config`, project `Default<T>`, `Config/<P>/`, `Platforms/<P>/Config`, `Saved/Config` (plus `NotForLicensees` / `Restricted` folders and a binary config cache) | the same order without `NotForLicensees` / `Restricted` or the binary cache; the `Saved/Config` user layer exists only on desktop; the PS2 reads the ini files through `host:` and keeps compiled defaults when they are missing | the PS2 build has no writable storage and PCSX2's host filesystem is optional |
| Config usage | `UPROPERTY(Config)` / `LoadConfig` everywhere, input from `BaseInput.ini` | the same for the engine (`UEngine`), the map settings, the input settings and the player input since P13; a few keys are still read by hand (the window size, the collection interval, ThirdPerson tuning) | the window exists before `GEngine`; the others have no settings class yet |
| Config classes | `GlobalUserConfig` / `ProjectUserConfig` classes and `UpdateDefaultConfigFile` | not supported (LeonHeaderTool rejects the specifiers); `SaveConfig` writes the desktop user layer only | D8 has no per-user global layer; `Default<T>.ini` is edited by hand (the editor module has no settings UI) |
| `FString` in archives | ANSI when possible, else UTF-16 with a negative length | always UTF-8 with a positive length (including the terminator); a negative length is rejected | `TCHAR` is UTF-8 (D1) |
| `FName` in archives | an index into the package name table (the base `FArchive` does not store names) | the package linkers write the name table index and the number, as UE; the base `FArchive` writes a string | the tests serialize names through plain archives |
| Package files | `.uasset` header, `.uexp` export data and `.ubulk` bulk data (cooked), `PACKAGE_FILE_TAG` 0x9E2A83C1, summary with custom versions, folder name, generations, thumbnails, asset registry data, preload dependencies, compression | one `.lasset` / `.lmap` file: `'LEON'` summary (versions, flags, table offsets, GUID, saving engine, cooked platform, bulk data offset), sorted name table (strings only, no name hashes), imports, exports (no `TemplateIndex`, dependencies or package GUIDs), soft package references, export data, bulk data at the end, the tag again at the end | D13; one file per package suits a CD seek and a pak |
| Package GUID and time | a new random GUID per save (`PersistentGuid` in UE 5), timestamps | the GUID is `FGuid::NewDeterministicGuid` of the long package name (MD5); no time anywhere; the name table and the import / export tables are sorted | D13: deterministic saves (a golden hash checks it on Win64 and the PS2) |
| Package loading | async loading (`FAsyncPackage`, event-driven loader), lazy export loading, linkers kept until `ResetLoaders` | synchronous `LoadPackage` only: the whole file is read at once, every export is created and serialized, `PostLoad` runs at the outermost `EndLoad` package by package in the order their loads finished (imports first), then the linker is released | the PS2 loads a level at a time; async loading is a later milestone |
| Tagged properties | `FPropertyTag` with a struct GUID and property GUIDs; sets and maps saved as a delta of the defaults; `STRUCT_SerializeFromMismatchedTag` conversions | no struct or property GUIDs (the `HasPropertyGuid` byte is always 0); sets and maps saved whole (UE's layout with 0 removed elements); a struct tag of another struct is skipped; conversions: integers, float / double, byte / enum by name, name / string / text, hard to soft references | no Blueprints; enough schema evolution for native classes |
| `UObject::Serialize` | also writes the lazy-pointer GUID flag and counts memory | tagged properties, then the class's native data | no `FLazyObjectPtr` |
| Bulk data | lazy loading, file mapping, compression, `.ubulk` / optional / memory-mapped payloads | `FByteBulkData` only: payloads at the end of the package (or inline), loaded eagerly with their owner | D13; the PS2 reads a package once |
| Soft object paths | `FArchive::operator<<(FSoftObjectPath&)` virtual, redirectors, `GRedirectCollector` | a free `operator<<` over `FSoftObjectPath::SerializePath`; the save collects soft package references through the object thread context; no redirectors | keeps Core free of CoreUObject types |
| `SavePackage` | `(InOuter, Base, TopLevelFlags, Filename, Error, Conform, bForceByteSwapping, bWarnOfLongFilename, SaveFlags, TargetPlatform, FinalTimeStamp, bSlowTask, DiffMap, SavePackageContext)` | `(InOuter, Base, TopLevelFlags, Filename, Error, SaveFlags, CookedPlatformName)` plus Leon's `SaveToMemory`; the target platform is its name (the summary's `CookedPlatform`) rather than an `ITargetPlatform*`; `PKG_ContainsMap` comes from a `.lmap` file name; exports include the inner objects of every export | no conform / diff / async save; CoreUObject is a Runtime module and cannot see the Developer interface |
| In-memory packages | none (UE 5: package resource managers) | `FLinkerLoad::RegisterInMemoryPackage` makes `LoadPackage` / `DoesPackageExist` read registered bytes | tests on every platform; the PS2 platform file is read-only |
| Editor-only data | cooked packages drop it; a runtime without editor data refuses uncooked packages | the cook (P16) drops the editor-only properties and objects (UE's `IsEditorOnly`); a build without `WITH_EDITORONLY_DATA` (PS2, Shipping) marks every package it saves `PKG_FilterEditorOnly`, and loading an uncooked package logs it once and skips the editor-only tags as unknown names (a Shipping game only reads its pak's cooked packages anyway) | D14 |
| Archive versions | `FArchive::UE4Ver()` / `LicenseeUE4Ver()` (4.27), `UEVer()` in UE 5 | `UEVer()` / `LicenseeUEVer()` (UE 5 names) over `ELeonPackageVersion`; the summary field is `FileVersionUE` | the versions are Leon's own, not UE 4's |
| `FPaths` directories | relative to the process (`../../../Engine/`) | absolute on desktop, built from the executable folder and the generated `GLeon*FromBaseDir` globals (projects live under `Game/`, so `../../../Engine/` would miss); a staged build (`FPaths::IsStaged`: paks beside the binaries, no engine sources in the build tree's engine folder) uses UE's `../../../Engine/` and the folder above `Binaries/`; on PS2 a staged layout under the ELF folder (`<Base>/Engine/`, `<Base>/<Project>/`) | independent of the working directory; PCSX2's `host:` is the ELF folder |
| Pak format | `.pak` with a per-entry header before the data, compression blocks, AES encryption, signatures (`.sig`), a path-hash index and a full directory index (4.25+), `FPakInfo` magic 0x5A6F12E1 | `.lpak`: raw entry data, one index sorted by the CRC-32 of the lowercased path (a collision compares the paths), per-entry SHA-1, a 44-byte `FPakInfo` with the magic `'LPAK'` and the index's SHA-1; `-align=` for CD sectors; no compression, encryption or signatures | small and deterministic; compression is a later milestone |
| Pak mount | the pak folders of the project, the engine and `Saved`; a relative mount point resolves against the working directory, which UE sets to the binaries folder | `<Project>/Content/Paks/` and `Engine/Content/Paks/`; a relative mount point resolves against `FPlatformProcess::BaseDir()` (the same folder); the paks mount in `FEngineLoop::PreInit` from Launch, which links PakFile (UE loads the PakFile module by name) | static linking; Leon's working directory is not the binaries folder |
| Loose files | a Shipping build reads loose files unless the paks are signed (`-signedpak`) or the loose file is not allowed by the platform | a Shipping build refuses every loose read outside the `Saved` folders and stops without a pak; Development reads loose files after the paks | the plan's rule: Shipping only reads from the pak |
| Cook | cook by the book or on the fly, `-iterate`, the asset registry, per-platform formats (DDC), `.uexp` / `.ubulk` splits, the cooked `Metadata` folder | cook by the book only, into an emptied folder; the closure from the packages' tables; seeds from the maps, `DirectoriesToAlwaysCook` and every path the Engine / Game config names; the config and shaders copied beside (UE stages those in the staging step); the PS2 target cooks the Win64 formats and logs what it cannot convert | the formats have one representation until the PS2 port |
| Target platforms | one `<Platform>TargetPlatform` module per platform, loaded by the manager | both platforms inside the TargetPlatform module; `ITargetPlatform` trimmed to what the cook asks | two platforms, static linking |
| Staging | AutomationTool (C#) `BuildCookRun`: a staging manifest, UFS / non-UFS files, a bootstrap `<Project>.exe` at the root, loose staging without `-pak`, every platform | `BuildCookRun.bat` (PowerShell): the game and one `.lpak` of the whole cooked folder; `-stage` needs `-pak`; no bootstrap executable (the game is `<Project>/Binaries/Win64/<Project>[-Win64-<Configuration>].exe`); Win64 only | the minimum a Shipping game needs; the PS2 comes with the Engine port |
| Unattended runs | `-unattended` (and automation) | `FApp::IsUnattended` is also true for Leon's captures (`-Screenshot=`, `-ExitAfterFrames=`), and an unattended game ignores the OS input (`UGameViewportClient::SetIgnoreInput`) | a capture must not depend on the mouse |
| `FDateTime` | Julian-day and `double` helpers | integer ticks only; the Julian-day helpers are left out | float-only math (D6) |
| JSON numbers | written with `%.17g` | the shortest `%.15g`–`%.17g` form that reads back to the same value | readable descriptors; still round-trips |
| Module platform lists | `WhitelistPlatforms` / `BlacklistPlatforms` (4.27) | `PlatformAllowList` / `PlatformDenyList` (UE 5 names) written; the 4.27 names are still read | the `.lplugin` files already used the UE 5 names |
| Plugin enable state | decides which plugin modules load | `IPluginManager` reports it, but LeonBuildTool alone decides what is linked (`ENABLE_PLUGINS`) | static linking, no module loading at runtime |
| Global `operator new` / `delete` | replaced through `FMemory` in every monolithic build (`REPLACEMENT_OPERATOR_NEW_AND_DELETE`) | replaced on the PS2 only (`PS2PlatformRuntime.cpp`) | keeps libstdc++'s allocation, unwinder and demangler code out of the ELF; desktop still uses the CRT |
| Body collision shape | the body setup's aggregate geometry (boxes, spheres, capsules, convex hulls) and cooked triangle meshes | `UBodySetup` has boxes only and no cooked data: a static body collides with the mesh's triangles unless `CTF_UseSimpleAsComplex`, every other body is the box of the setup's boxes, or of the mesh bounds when it has none (`FBodyInstance::CollisionShape` `Box` / `TriangleMesh`) | Leon's physics scene has AABB and triangle-mesh bodies only |
| Bone matrices | `FTransform` bone poses | `FMatrix` poses in UE's row-vector convention (the FBX import builds them with `FQuatRotationMatrix` and converts them to world space by conjugation, B⁻¹ M B); the skin matrix is `InverseBind * BoneWorld` in FMatrix order | the renderer uploads them as they are; the `UAnimSequence` tracks keep them (a later animation compression can move to `FTransform` keys) |
| Widget colors | `FSlateColor` / `FLinearColor` with alpha | `FLinearColor`, alpha ignored by the canvas tiles, lines and text | the HUD draws opaque RGB |
| Game → Launch | game modules never see `FEngineLoop` | the PS2 game module reads `GEngineLoop.GetMainWindow()` (include-only dependency on the launch module) | no Slate / `GEngine` on PS2 to hand out the viewport |
| Gamepad | `FSlateApplication` routes `IInputInterface` events to the player controller | the PS2 game polls `IInputInterface` state directly with `FKey`s; the desktop viewport client reads no gamepad | no Slate; polling matches the PS2 frame loop; the PS2 has no player controller yet |
