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
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
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
		// The renderer's GPU objects need the window's context (UE: the renderer comes up with the viewport). The
		// shaders are the engine's (UE: the /Engine/Shaders virtual folder, Engine/Shaders on disk).
		const FString ShaderDir = FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders"));
		IRendererModule* RendererModule = GetRendererModulePtr();
		if (RendererModule == nullptr || !RendererModule->InitRenderer(ShaderDir))
		{
			UE_LOG(LogEngine, Error, "Failed to start the renderer");
			bIsInitialized = false;
			return;
		}
		bLogFrameTimes = FParse::Param(FCommandLine::Get(), "LogFrameTimes");
		// Stats off unless the config or -showstats asks for them.
		Overlay.SetRightText(FString());
		Overlay.SetBottomLeftText(FString());
		SetHudStatsVisible(bShowStatsByDefault || FParse::Param(FCommandLine::Get(), "showstats"));
		UE_LOG(LogEngine, Log, "HUD stats: %s", IsHudStatsVisible() ? "on" : "off");
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
	// No one is at the controls of an unattended run (a scripted capture): the OS input must not move the view.
	GameViewport->SetIgnoreInput(FApp::IsUnattended());
	if (Window != nullptr)
	{
		GameViewport->SetViewportWindow(Window);
		// The application's gamepad (the PS2's DualShock; the desktop has none), read with the window (UE: Slate's
		// controller events).
		GenericApplication* Application = InEngineLoop->GetApplication();
		GameViewport->SetInputInterface(Application != nullptr ? Application->GetInputInterface() : nullptr);
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

	const double GameStart = FPlatformTime::Seconds();
	FWorldContext& Context = *GameInstance->GetWorldContext();
	TickWorldTravel(Context, DeltaSeconds);
	if (UWorld* World = Context.World())
	{
		// The player controllers process their input as they tick, the characters move after their controllers, the
		// world updates the cameras, then the physics steps (UWorld::TickGameplayFrame).
		FWorldGameplayFrameParams Frame;
		Frame.DeltaTime = DeltaSeconds;
		// The viewport's show flags draw into the world's line batch, which the scene renderer flushes (UE); without a
		// window nothing would empty it.
		if (Window != nullptr && GameViewport != nullptr)
		{
			const FEngineShowFlags& ShowFlags = GameViewport->EngineShowFlags;
			Frame.CollisionDebugDraw = ShowFlags.Collision ? &World->LineBatcher : nullptr;
			Frame.NavigationDebugDraw = ShowFlags.Navigation ? &World->LineBatcher : nullptr;
		}
		World->TickGameplayFrame(Frame);
	}
	// After the world ticked, like UE's UGameEngine::Tick (a safe point, D11).
	(void)ConditionalCollectGarbage(DeltaSeconds);

	if (Window != nullptr)
	{
		const double DrawStart = FPlatformTime::Seconds();
		GameViewport->Tick(DeltaSeconds);
		// UE: RedrawViewports.
		GameViewport->GetGameViewport()->Draw(true);
		if (bLogFrameTimes)
		{
			const double DrawEnd = FPlatformTime::Seconds();
			AccumulateFrameTimes(DeltaSeconds, DrawStart - GameStart, DrawEnd - DrawStart);
		}
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

void UGameEngine::AccumulateFrameTimes(float DeltaSeconds, double GameSeconds, double DrawSeconds)
{
	++FrameLogFrames;
	FrameLogTime += DeltaSeconds;
	FrameLogWorst = FMath::Max(FrameLogWorst, DeltaSeconds);
	FrameLogGameSeconds += GameSeconds;
	FrameLogDrawSeconds += DrawSeconds;
	const FFrameStats& Stats = GetRendererModule().GetFrameStats();
	FrameLogTriangles += Stats.TrianglesSubmitted;
	FrameLogRegisterWrites += Stats.RegisterWrites;
	FrameLogTextureUploads += Stats.TextureUploads;
	FrameLogPeakTriangles = FMath::Max(FrameLogPeakTriangles, Stats.TrianglesSubmitted);
	if (FrameLogTime < FrameLogSeconds)
	{
		return;
	}
	const double Frames = double(FrameLogFrames);
	UE_LOG(LogEngine, Display,
		"Frame times over %d frames: %.1f ms average (%.1f fps), %.1f ms worst; world %.1f ms, "
		"draw and present %.1f ms",
		FrameLogFrames, double(FrameLogTime) * 1000.0 / Frames, Frames / double(FrameLogTime),
		double(FrameLogWorst) * 1000.0, FrameLogGameSeconds * 1000.0 / Frames, FrameLogDrawSeconds * 1000.0 / Frames);
	// The GS work a frame records, identical on the PC and the PS2 (the same scene renderer): what the PS2's frame
	// costs follows from it (Docs/PLANS/ps2-preview.md V2; the milliseconds once calibrated in PCSX2).
	UE_LOG(LogEngine, Display,
		"Frame work over %d frames: %.0f triangles average (%d peak), %.0f GS register writes, %.2f texture uploads",
		FrameLogFrames, double(FrameLogTriangles) / Frames, FrameLogPeakTriangles,
		double(FrameLogRegisterWrites) / Frames, double(FrameLogTextureUploads) / Frames);
	FrameLogTriangles = 0;
	FrameLogRegisterWrites = 0;
	FrameLogTextureUploads = 0;
	FrameLogPeakTriangles = 0;
	FrameLogFrames = 0;
	FrameLogTime = 0.0f;
	FrameLogWorst = 0.0f;
	FrameLogGameSeconds = 0.0;
	FrameLogDrawSeconds = 0.0;
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
