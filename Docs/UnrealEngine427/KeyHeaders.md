# UE 4.27 key header paths

Prefix every path with `Engine/Source/` in the UE checkout. These are the headers LeonEngine mirrors by name and
location; read the real file only when you need its exact API.

## Core

| Header | Declares |
| --- | --- |
| `Runtime/Core/Public/CoreTypes.h` | lowest-level include (`HAL/Platform.h`, `Misc/CoreMiscDefines.h`) |
| `Runtime/Core/Public/CoreMinimal.h` | standard base include |
| `Runtime/Core/Public/HAL/Platform.h` | `PLATFORM_*` defaults, `int32`, `uint8`, `FORCEINLINE` via platform header |
| `Runtime/Core/Public/HAL/PreprocessorHelpers.h` | `COMPILED_PLATFORM_HEADER`, `PLATFORM_IS_EXTENSION`, `PREPROCESSOR_JOIN` |
| `Runtime/Core/Public/HAL/PlatformMemory.h` | `FPlatformMemory` dispatch (`GetStats()` → `FPlatformMemoryStats`) |
| `Runtime/Core/Public/HAL/PlatformTime.h` | `FPlatformTime` (`Seconds()`, `Cycles64()`) |
| `Runtime/Core/Public/HAL/PlatformMath.h` | `FPlatformMath` |
| `Runtime/Core/Public/HAL/PlatformProperties.h` | `FPlatformProperties` (`PlatformName()`) |
| `Runtime/Core/Public/GenericPlatform/GenericPlatformMemory.h` | `FGenericPlatformMemory`, `FGenericPlatformMemoryStats` |
| `Runtime/Core/Public/GenericPlatform/GenericPlatformTime.h` | `FGenericPlatformTime` |
| `Runtime/Core/Public/Windows/WindowsPlatform.h` | Windows platform defines |
| `Runtime/Core/Public/Modules/ModuleInterface.h` | `IModuleInterface` |
| `Runtime/Core/Public/Modules/ModuleManager.h` | `FModuleManager`, `IMPLEMENT_MODULE`, `IMPLEMENT_*GAME_MODULE` |
| `Runtime/Core/Public/Containers/Ticker.h` | `FTicker` (`GetCoreTicker()`, `AddTicker`, `Tick`) |
| `Runtime/Core/Public/CoreGlobals.h` | `GIsRequestingExit`, `IsEngineExitRequested()`, `RequestEngineExit()` |
| `Runtime/Core/Public/Misc/Paths.h` | `FPaths` (`EngineDir()`, `ProjectDir()`, `EngineContentDir()`, …) |
| `Runtime/Core/Public/Misc/FileHelper.h` | `FFileHelper` (`LoadFileToString`, `SaveStringToFile`) |
| `Runtime/Core/Public/Misc/CString.h` | `FCString`, `FCStringAnsi` |
| `Runtime/Core/Public/Math/Transform.h` | `FTransform` |

## Launch / RHI / Application / Input

| Header | Declares |
| --- | --- |
| `Runtime/Launch/Public/LaunchEngineLoop.h` | `FEngineLoop`, `GEngineLoop` |
| `Runtime/Launch/Private/Launch.cpp` | `GuardedMain` |
| `Runtime/Launch/Private/Windows/LaunchWindows.cpp` | `WinMain` |
| `Runtime/Launch/Resources/Version.h` | `ENGINE_MAJOR_VERSION` … |
| `Runtime/RHI/Public/DynamicRHI.h` | `FDynamicRHI`, `GDynamicRHI`, `PlatformCreateDynamicRHI` |
| `Runtime/RHI/Public/RHI.h` | `RHIInit`, `RHIExit`, globals |
| `Runtime/RHI/Public/RHIDefinitions.h` | enums, limits |
| `Runtime/OpenGLDrv/Public/OpenGLDrv.h` | `FOpenGLDynamicRHI` |
| `Runtime/EmptyRHI/Public/EmptyRHI.h` | `FEmptyDynamicRHI` (template for new backends) |
| `Runtime/ApplicationCore/Public/GenericPlatform/GenericApplication.h` | `GenericApplication`, `EMouseButtons` |
| `Runtime/ApplicationCore/Public/GenericPlatform/GenericWindow.h` | `FGenericWindow` |
| `Runtime/ApplicationCore/Public/GenericPlatform/IInputInterface.h` | `IInputInterface` (force feedback, light color) |
| `Runtime/ApplicationCore/Public/HAL/PlatformApplicationMisc.h` | `FPlatformApplicationMisc` dispatch |
| `Runtime/ApplicationCore/Private/Windows/XInputInterface.h` | `XInputInterface` (gamepad polling) |
| `Runtime/InputCore/Classes/InputCoreTypes.h` | `FKey`, `EKeys` |

## Engine (`Runtime/Engine/`)

| Header | Declares |
| --- | --- |
| `Classes/GameFramework/Actor.h` | `AActor` |
| `Classes/GameFramework/Pawn.h` | `APawn` |
| `Classes/GameFramework/Character.h` | `ACharacter` |
| `Classes/GameFramework/CharacterMovementComponent.h` | `UCharacterMovementComponent` |
| `Classes/GameFramework/Controller.h` | `AController` |
| `Classes/GameFramework/PlayerController.h` | `APlayerController` |
| `Classes/GameFramework/GameModeBase.h` / `GameMode.h` | `AGameModeBase` / `AGameMode` |
| `Classes/GameFramework/GameStateBase.h` | `AGameStateBase` |
| `Classes/GameFramework/PlayerState.h` | `APlayerState` |
| `Classes/GameFramework/HUD.h` | `AHUD` |
| `Classes/GameFramework/SpringArmComponent.h` | `USpringArmComponent` (lives in GameFramework, not Components) |
| `Classes/GameFramework/PlayerInput.h` | `UPlayerInput` |
| `Classes/GameFramework/DefaultPawn.h` | `ADefaultPawn` |
| `Classes/GameFramework/PlayerStart.h` | `APlayerStart` |
| `Classes/Camera/CameraComponent.h` | `UCameraComponent` |
| `Classes/Camera/PlayerCameraManager.h` | `APlayerCameraManager` |
| `Classes/Components/ActorComponent.h` | `UActorComponent` |
| `Classes/Components/SceneComponent.h` | `USceneComponent` |
| `Classes/Components/PrimitiveComponent.h` | `UPrimitiveComponent` |
| `Classes/Components/StaticMeshComponent.h` | `UStaticMeshComponent` |
| `Classes/Components/SkeletalMeshComponent.h` | `USkeletalMeshComponent` |
| `Classes/Components/CapsuleComponent.h` | `UCapsuleComponent` |
| `Classes/Components/DirectionalLightComponent.h` | `UDirectionalLightComponent` |
| `Classes/Components/LineBatchComponent.h` | `ULineBatchComponent` |
| `Classes/Engine/World.h` | `UWorld` |
| `Classes/Engine/Level.h` | `ULevel` |
| `Classes/Engine/GameInstance.h` | `UGameInstance` |
| `Classes/Engine/Engine.h` | `UEngine` |
| `Classes/Engine/GameEngine.h` | `UGameEngine` |
| `Classes/Engine/NetDriver.h` | `UNetDriver` |
| `Classes/Engine/Texture2D.h` | `UTexture2D` |
| `Classes/Engine/StaticMesh.h` / `SkeletalMesh.h` | `UStaticMesh` / `USkeletalMesh` |
| `Classes/Engine/Canvas.h` | `UCanvas` |
| `Classes/Materials/Material.h`, `MaterialInterface.h` | `UMaterial`, `UMaterialInterface` |
| `Classes/Animation/AnimInstance.h`, `AnimSequence.h`, `BlendSpace1D.h`, `Skeleton.h` in `Classes/Animation/` | animation assets |
| `Classes/Kismet/GameplayStatics.h` | `UGameplayStatics` |
| `Public/EngineGlobals.h` | `GEngine` |
| `Public/DrawDebugHelpers.h` | `DrawDebugLine`, … |
| `Public/CanvasTypes.h` | `FCanvas` |
| `Public/AudioDevice.h` | `FAudioDevice` |
| `Public/EngineMinimal.h` | minimal engine include |

## Other runtime modules

| Header | Declares |
| --- | --- |
| `Runtime/UMG/Public/Blueprint/UserWidget.h`, `Blueprint/WidgetTree.h` | `UUserWidget`, `UWidgetTree` |
| `Runtime/UMG/Public/Components/Widget.h`, `PanelWidget.h`, `PanelSlot.h`, `ContentWidget.h`, `SlateWrapperTypes.h` | the widget tree's base classes, `ESlateVisibility` |
| `Runtime/UMG/Public/Components/Border.h`, `VerticalBox.h`, `VerticalBoxSlot.h`, `CanvasPanel.h`, `CanvasPanelSlot.h`, `TextBlock.h`, `ProgressBar.h`, `Image.h` | widgets (Leon mirrors no `Button.h`: its widgets take no input) |
| `Runtime/SlateCore/Public/Layout/Margin.h`, `Types/SlateEnums.h` | `FMargin`, `EHorizontalAlignment` |
| `Runtime/AIModule/Classes/AIController.h` | `AAIController` |
| `Runtime/AIModule/Classes/BehaviorTree/BehaviorTree.h`, `BlackboardComponent.h`, `BTNode.h`, `Composites/BTComposite_Sequence.h` | behavior trees |
| `Runtime/NavigationSystem/Public/NavigationSystem.h` | `UNavigationSystemV1` |
| `Runtime/PhysicsCore/Public/…` | physics types (`FBodyInstanceCore`, collision shapes) |
| `Runtime/Projects/Public/ProjectDescriptor.h` | `FProjectDescriptor` (`.uproject`) |
| `Runtime/Projects/Public/PluginDescriptor.h` | `FPluginDescriptor` (`.uplugin`) |
| `Runtime/Json/Public/Serialization/JsonSerializer.h` | JSON |

## Developer / Editor / Programs

| Path | Role |
| --- | --- |
| `Developer/MeshUtilities/`, `Developer/MeshBuilder/` | mesh build / import helpers |
| `Developer/TargetPlatform/` | per-platform cook formats |
| `Editor/UnrealEd/Classes/Commandlets/CookCommandlet.h` | `UCookCommandlet` (cooking) |
| `Programs/UnrealBuildTool/` | UBT (C#): `Configuration/ModuleRules.cs`, `TargetRules.cs`, `System/`, `Platform/<Plat>/` |
| `Programs/BlankProgram/` | minimal program target |
| `Programs/UnrealPak/` | pak packaging program |
