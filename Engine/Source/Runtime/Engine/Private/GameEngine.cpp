#include "Engine/GameEngine.h"

#include "Camera/CameraComponent.h"
#include "CoreGlobals.h"
#include "Engine/BlockingVolume.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineLogs.h"
#include "GameMapsSettings.h"
#include "GenericPlatform/GenericWindow.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RendererInterface.h"
#include "UObject/GarbageCollection.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"

UGameEngine::UGameEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGameEngine::BeginDestroy()
{
	if (bIsInitialized)
	{
		PreExit();
	}
	Super::BeginDestroy();
}

void UGameEngine::Init(IEngineLoop* InEngineLoop)
{
	Super::Init(InEngineLoop);

	FGenericWindow* Window = !bHeadless ? InEngineLoop->GetMainWindow() : nullptr;
	if (Window != nullptr)
	{
		// The renderer's GPU objects need the window's context (UE: the renderer comes up with the viewport).
		const FString ShaderDir = FPaths::ResolveLegacyContentPath("assets/Shaders");
		IRendererModule* RendererModule = GetRendererModulePtr();
		if (RendererModule == nullptr || !RendererModule->InitRenderer(ShaderDir))
		{
			UE_LOG(LogEngine, Error, "Failed to start the renderer");
			bIsInitialized = false;
			return;
		}
		// Stats off unless the config or -showstats asks for them.
		Overlay.SetRightText(FString());
		Overlay.SetBottomLeftText(FString());
		if (bShowStatsByDefault || FParse::Param(FCommandLine::Get(), "showstats"))
		{
			SetHudStatsVisible(true);
		}
	}
	else
	{
		UE_LOG(LogEngine, Log, "Leon Engine headless (no OpenGL / window)");
	}

	// The game instance of the project's class and its world context (UE).
	UClass* GameInstanceClass = GetDefault<UGameMapsSettings>()->GameInstanceClass.IsValid()
		? GetDefault<UGameMapsSettings>()->GameInstanceClass.TryLoadClass<UGameInstance>()
		: nullptr;
	if (GameInstanceClass == nullptr)
	{
		GameInstanceClass = UGameInstance::StaticClass();
	}
	GameInstance = NewObject<UGameInstance>(this, GameInstanceClass);
	GameInstance->InitializeStandalone();

	// The game viewport client shows the context's world in the window, and makes the first local player (UE).
	UClass* ViewportClientClass = GameViewportClientClassName.IsValid()
		? GameViewportClientClassName.TryLoadClass<UGameViewportClient>()
		: nullptr;
	if (ViewportClientClass == nullptr)
	{
		ViewportClientClass = UGameViewportClient::StaticClass();
	}
	GameViewport = NewObject<UGameViewportClient>(this, ViewportClientClass);
	GameViewport->Init(*GameInstance->GetWorldContext(), GameInstance);
	if (Window != nullptr)
	{
		GameViewport->SetViewportWindow(Window);
		if (FParse::Param(FCommandLine::Get(), "AxesGizmo"))
		{
			GameViewport->EngineShowFlags.AxesGizmo = true;
		}
	}
	FString Error;
	if (GameViewport->SetupInitialLocalPlayer(Error) == nullptr)
	{
		UE_LOG(LogEngine, Error, "Could not create the local player: %s", *Error);
	}
	GameInstance->Init();
}

void UGameEngine::Start()
{
	GameInstance->StartGameInstance();
	if (bHeadless || IsEngineExitRequested())
	{
		return;
	}
	int32 NumStaticMeshes = 0;
	if (UWorld* World = GetGameWorld(); World != nullptr && World->PersistentLevel != nullptr)
	{
		for (const AActor* Actor : World->PersistentLevel->Actors)
		{
			if (Actor != nullptr && (Actor->IsA<AStaticMeshActor>() || Actor->IsA<ABlockingVolume>()))
			{
				++NumStaticMeshes;
			}
		}
	}
	UE_LOG(LogEngine, Log, "Level static meshes: %d", NumStaticMeshes);
	UE_LOG(LogEngine, Log, "Controls: mouse look (cursor captured); close window to quit");
	UE_LOG(LogEngine, Log, "Default pawn: mouse look, WASD fly along view, Q/E down/up (BaseInput.ini)");
	UE_LOG(LogEngine, Log, "Debug: F1 show Bounds (mesh AABBs + light frustum); F2 show Collision");
	UE_LOG(LogEngine, Log, "Debug: F3 show Navigation");
	UE_LOG(LogEngine, Log, "Stats: F4 stat unit (FPS / RAM / TRI overlay, off by default)");
	UE_LOG(LogEngine, Log, "Shaders: F5 RecompileShaders (also auto-reloads when files change)");
	UE_LOG(LogEngine, Log, "Debug: F6 show AxesGizmo (X red, Y green, Z blue; world origin + view corner)");
}

void UGameEngine::PreExit()
{
	if (!bIsInitialized)
	{
		return;
	}
	if (GameInstance != nullptr)
	{
		GameInstance->Shutdown();
	}
	AudioDevice.Shutdown();
	// The world goes first: its actors end play while the resources they use still exist.
	DestroyGameWorld();
	Resources.Clear();
	FGenericWindow* Window = GameViewport != nullptr ? GameViewport->GetWindow() : nullptr;
	if (Window != nullptr)
	{
		Overlay.Clear();
		// The GPU copies of the assets and the renderer's objects go while the context exists.
		GetRendererModule().ShutdownRenderer();
		Window->SetCursorCaptured(false);
		GameViewport->SetViewportWindow(nullptr);
	}
	Super::PreExit();
}

void UGameEngine::DestroyGameWorld()
{
	if (GameInstance != nullptr && GameInstance->GetWorld() != nullptr)
	{
		GameInstance->DestroyWorldContextWorld();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
}

UWorld* UGameEngine::GetGameWorld() const
{
	return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
}

TArray<FWorldContext*> UGameEngine::GetWorldContexts()
{
	TArray<FWorldContext*> Contexts;
	if (GameInstance != nullptr)
	{
		Contexts.Add(GameInstance->GetWorldContext());
	}
	return Contexts;
}

EShaderReloadResult UGameEngine::ReloadAllShaders(bool bForce)
{
	IRendererModule* RendererModule = GetRendererModulePtr();
	return RendererModule != nullptr ? RendererModule->ReloadShaders(bForce) : EShaderReloadResult::Unchanged;
}

void UGameEngine::Tick(float DeltaSeconds, bool /*bIdleMode*/)
{
	if (!bIsInitialized)
	{
		return;
	}
	FGenericWindow* Window = GameViewport != nullptr ? GameViewport->GetWindow() : nullptr;
	if (Window != nullptr && Window->ShouldClose())
	{
		RequestEngineExit("Main window closed");
		return;
	}
	if (Window != nullptr)
	{
		(void)ReloadAllShaders(false);
		// The window's keys and mouse reach the player's controller before the world ticks (UE: Slate's events).
		GameViewport->ProcessInput(DeltaSeconds);
		TickPlayAudio();
	}

	FWorldContext& Context = *GameInstance->GetWorldContext();
	TickWorldTravel(Context, DeltaSeconds);
	if (UWorld* World = Context.World())
	{
		// The player controllers process their input as they tick, then the world updates their cameras.
		World->Tick(DeltaSeconds);
	}
	// After the world ticked, like UE's UGameEngine::Tick (a safe point, D11).
	(void)ConditionalCollectGarbage(DeltaSeconds);

	if (Window != nullptr)
	{
		GameViewport->Tick(DeltaSeconds);
		// UE: RedrawViewports.
		GameViewport->GetGameViewport()->Draw(true);
		if (Window->ShouldClose())
		{
			RequestEngineExit("Main window closed");
		}
	}
	else
	{
		// Keep the console current when stdout is redirected (CI smoke, servers stopped with Ctrl+C).
		GLog->Flush();
	}
}

void UGameEngine::TickPlayAudio()
{
	const UCameraComponent& Camera = *GameViewport->GetViewCamera();
	const FVector Eye = Camera.GetCameraLocation();
	const FVector Forward = Camera.ForwardVector();
	const FVector Up = FVector(0.0f, 0.0f, 1.0f);
	AudioDevice.SetListener(Eye, Forward, Up);
	AudioDevice.Tick();
}
