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
| `Engine/Serialization` | `Json` | `FJsonUtils` |
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
| `Runtime/ProjectPack` (`leon.game.json`) | removed in 0.12.0 | `Projects` is an empty placeholder until the `.lproj` / `.lplugin` readers (P4) |
| `Runtime/GameHostSession`, `WorldRuntime` | removed in 0.12.0 | `FGameApplication` (`Launch`) loads one level (`-map=`) |
| `Tools/ResourceTools` | `Developer/Cooker` (`FCookRecipe`, `FCookPaths`, `UCookCommandlet`) | UE: cook commandlet in UnrealEd |
| `Tools/AssetPipeline/leon-cook` | `Programs/LeonCook` | `UE4Editor-Cmd -run=cook` equivalent |
| `Tools/Cli` (`leon-cli`) | removed | only forwarded to leon-cook |
| `Tests/` (Catch2) | `<Module>/Private/Tests/` + `Programs/LeonAutomationTests` | Core's tests are UE automation tests (P2); the other modules keep Catch2 until P5–P6 |
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
| `Transform` | `FTransform` |
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
| 4.1 Core / Json / Projects | `Transform`, `Paths` free functions, `FileIO`, `AsciiToLower` | `FTransform`, `FPaths::*` (`ExecutableDir`, `ResolveAssetPath`, …), `FFileHelper` (`Misc/FileHelper.h`), `FCString::ToLower` (`Misc/CString.h`; removed in P2) |
| | `serialization::ReadVec3 / LoadJsonFile`, `ProjectPack` | `FJsonUtils` (`Serialization/JsonUtils.h`), `FProjectDescriptor` (removed in 0.12.0 with the packs) |
| 4.2 Render | `Renderer`, `Texture`, `StaticMesh`, `SkeletalMesh`, `Material`, `EShadingModel` | `FSceneRenderer` (`SceneRenderer.h`), `UTexture2D` (`Texture2D.h`), `UStaticMesh`, `USkeletalMesh`, `FMaterial`, `EMaterialShadingModel` |
| | `MeshData`, `SubMesh`, `Vertex`, `Aabb`, `Plane`, `Frustum` | `FMeshData`, `FMeshSection`, `FVertex`, `FBox`, `FPlane`, `FFrustum` |
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
  `FFileHelper`, `FCString`, `FJsonUtils`, `UGameplayStatics`, `FCookRecipe`, …); other free functions stay
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

## Deviations from UE 4.27 (intentional)

| Topic | UE | LeonEngine | Why |
| --- | --- | --- | --- |
| Reflection | `UCLASS`, `UObject`, UHT | none; `A`/`U` prefixes are naming only | CoreUObject is the next plan |
| Containers / strings | `TArray`, `TMap`, `FString` everywhere | Core has them (P2); the modules above Core still use `std::` containers / `std::string` | migration in P5–P6 |
| `TCHAR` | `wchar_t` / UTF-16 on most platforms | UTF-8 `char` on every platform; `TEXT(x)` is `x`; `WIDECHAR` only inside the Windows HAL; `TCHAR_TO_UTF8` & co. are identities | the EE has no wide-string support worth paying for; one encoding everywhere |
| `FName` pool | growing name blocks, `FNamePool` sized for desktop | 8-byte `FName`, hard-coded `EName` list; block size / count and hash buckets from `FPlatformProperties::NamePool*` (PS2: 16 KB blocks, at most 256 KB, 4096 buckets); exhausting the pool is fatal | fixed memory budget on 32 MB ([Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md)) |
| `FText` | localized text (`FTextLocalizationManager`, culture formatting) | minimal: `FromString`, `AsNumber`, `AsPercent`, `Format` (`{0}` arguments), `Join`; `LOCTEXT` / `NSLOCTEXT` keep the source text | no localization yet |
| Delegates | also dynamic (`DECLARE_DYNAMIC_*`) and `UObject` bindings | `TDelegate` / `TMulticastDelegate` with static, lambda, raw and SP bindings (+ payload) | dynamic / `UObject` delegates need CoreUObject |
| `FPlatformAtomics` on PS2 | real atomics | the generic non-atomic version | Leon runs a single EE thread |
| Automation tests | run by the session frontend / `-ExecCmds="Automation RunTests"` | `FAutomationTestFramework::RunTests(Filter)` from `LeonAutomationTests` (with Catch2) and `TestPAL` (every platform) | no editor / session frontend |
| Math | `FVector`, `FRotator`, `FMatrix` | glm on desktop, plain floats + `FPlatformMath` on PS2 | glm is Y-up and lowercase; aliasing would mislead |
| Build tool | C# UBT | CMake scripts | no .NET dependency; PS2 toolchain is CMake-based |
| Linking | monolithic or DLLs | always static (`IS_MONOLITHIC=1`), generated module table | PS2 has no DLLs |
| Renderer | API-agnostic via RHI command lists | calls OpenGL directly | debt |
| Engine ↔ Renderer | acyclic | `CIRCULAR_DEPENDENCIES` | debt |
| PS2 gameplay | full framework on consoles | PS2 game uses `F*` types, no `AActor` | the gameplay framework is desktop-only (glm/json, C++20) |
| Config | `FConfigCacheIni` loads layered ini | ini files exist as placeholders, not loaded | next plan |
| Game → Launch | game modules never see `FEngineLoop` | the PS2 game module reads `GEngineLoop.GetMainWindow()` (include-only dependency on the launch module) | no Slate / `GEngine` on PS2 to hand out the viewport |
| Gamepad | `FSlateApplication` routes `IInputInterface` events to the player controller | game code polls `IInputInterface` state directly | no Slate; polling matches the PS2 frame loop |
