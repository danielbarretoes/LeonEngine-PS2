#include "Engine/GameEngine.h"

#include "Camera/PlayerCameraManager.h"
#include "CanvasTypes.h"
#include "CoreGlobals.h"
#include "DynamicRHI.h"
#include "Engine/BlockingVolume.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineLogs.h"
#include "GameFramework/HUD.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformMemory.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"
#include "Misc/CString.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RendererInterface.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UnrealEngine.h"

namespace
{

	[[nodiscard]] int32 GetEngineInt(const TCHAR* Section, const TCHAR* Key, int32 Default)
	{
		int32 Value = Default;
		if (GConfig != nullptr)
		{
			GConfig->GetInt(Section, Key, Value, GEngineIni);
		}
		return Value;
	}

} // namespace

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

	DefaultViewCamera = NewObject<UCameraComponent>(this);

	if (!bHeadless)
	{
		// The game window and the renderer on its context (UE: CreateGameWindow and the viewport).
		Application.Reset(FPlatformApplicationMisc::CreateApplication());
		Window = Application->MakeWindow();
		const int32 ResolutionX = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionX", 1280);
		const int32 ResolutionY = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionY", 720);
		const FString Title = FApp::HasProjectName() ? FString("Leon - ") + FApp::GetProjectName() : FString("Leon");
		if (!Window->Create(ResolutionX, ResolutionY, *Title))
		{
			UE_LOG(LogEngine, Error, "Failed to create the game window");
			bIsInitialized = false;
			return;
		}
		const FString ShaderDir = FPaths::ResolveLegacyContentPath("assets/Shaders");
		IRendererModule* RendererModule = GetRendererModulePtr();
		if (RendererModule == nullptr || !RendererModule->InitRenderer(ShaderDir))
		{
			UE_LOG(LogEngine, Error, "Failed to start the renderer");
			Window->Destroy();
			bIsInitialized = false;
			return;
		}
		// The input settings say whether the window keeps the mouse (UE: DefaultViewportMouseCaptureMode).
		const EMouseCaptureMode CaptureMode = GetDefault<UInputSettings>()->DefaultViewportMouseCaptureMode;
		SetCursorCaptured(CaptureMode == EMouseCaptureMode::CapturePermanently ||
			CaptureMode == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
		// Stats off unless the config or -showstats asks for them.
		Overlay.SetRightText(FString());
		Overlay.SetBottomLeftText(FString());
		if (bShowStatsByDefault || FParse::Param(FCommandLine::Get(), "showstats"))
		{
			SetHudStatsVisible(true);
		}
		if (FParse::Param(FCommandLine::Get(), "AxesGizmo"))
		{
			EngineShowFlags.AxesGizmo = true;
		}
	}
	else
	{
		UE_LOG(LogEngine, Log, "Leon Engine headless (no OpenGL / window)");
	}

	// The game instance of the project's class, its world context and its first player (UE).
	UClass* GameInstanceClass = GetDefault<UGameMapsSettings>()->GameInstanceClass.IsValid()
		? GetDefault<UGameMapsSettings>()->GameInstanceClass.TryLoadClass<UGameInstance>()
		: nullptr;
	if (GameInstanceClass == nullptr)
	{
		GameInstanceClass = UGameInstance::StaticClass();
	}
	GameInstance = NewObject<UGameInstance>(this, GameInstanceClass);
	GameInstance->InitializeStandalone();
	FString Error;
	if (GameInstance->CreateInitialPlayer(Error) == nullptr)
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
	UE_LOG(LogEngine, Log, "Default mode: mouse look, WASD fly along view, Q/E up/down");
	UE_LOG(LogEngine, Log, "Debug: F1 mesh AABBs + light frustum; F2 collision volumes + floor traces");
	UE_LOG(LogEngine, Log, "Debug: F3 NavMesh grid (walkable / blocked)");
	UE_LOG(LogEngine, Log, "Stats: F4 FPS / RAM / TRI overlay (off by default)");
	UE_LOG(LogEngine, Log, "Shaders: F5 force-reload (also auto-reloads when files change)");
	UE_LOG(LogEngine, Log, "Debug: F6 axes gizmo (X red, Y green, Z blue; world origin + view corner)");
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
	if (!bHeadless && Window)
	{
		Overlay.Clear();
		// The GPU copies of the assets and the renderer's objects go while the context exists.
		GetRendererModule().ShutdownRenderer();
		Window->Destroy();
		Window->SetCursorCaptured(false);
	}
	DownKeys.Empty();
	FpsAccumTime = 0.0f;
	FpsAccumFrames = 0;
	DisplayFps = 0.0f;
	DisplayMs = 0.0f;
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

APlayerController* UGameEngine::GetFirstLocalPlayerController() const
{
	return GameInstance != nullptr ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}

UCameraComponent* UGameEngine::GetViewCamera() const
{
	const APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr)
	{
		return PlayerController->PlayerCameraManager->GetViewCamera();
	}
	return DefaultViewCamera;
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

void UGameEngine::SetCursorCaptured(bool bCaptured)
{
	if (!Window)
	{
		return;
	}
	Window->SetCursorCaptured(bCaptured);
	bMouseLookSampleValid = false; // skip one frame to avoid a jump after mode change
}

bool UGameEngine::IsCursorCaptured() const
{
	return Window && Window->IsCursorCaptured();
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
	if (Window && Window->ShouldClose())
	{
		RequestEngineExit("Main window closed");
		return;
	}
	if (Window)
	{
		Window->PollEvents();
		(void)ReloadAllShaders(false);
		HandleInput(DeltaSeconds);
		ProcessInput(DeltaSeconds);
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

	if (Window)
	{
		TickPlayHud(DeltaSeconds);
		Render();
		WritePendingScreenshot();
		Window->SwapBuffers();
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
	const UCameraComponent& Camera = *GetViewCamera();
	const FVector Eye = Camera.GetCameraLocation();
	const FVector Forward = Camera.ForwardVector();
	const FVector Up = FVector(0.0f, 0.0f, 1.0f);
	AudioDevice.SetListener(Eye, Forward, Up);
	AudioDevice.Tick();
}

void UGameEngine::TickPlayHud(float DeltaTime)
{
	// The HUDs tick in the world; the engine's text counts its messages down.
	Overlay.TickOnScreenMessages(DeltaTime);
	if (bShowHudStats)
	{
		UpdateHudStats(DeltaTime);
	}
}

void UGameEngine::PaintHudAndOverlay(FCanvas& Canvas)
{
	// The player's HUD, then the engine's text (UE: the viewport client posts the HUD, then the screen messages).
	if (const APlayerController* PlayerController = GetFirstLocalPlayerController())
	{
		if (PlayerController->MyHUD != nullptr)
		{
			PlayerController->MyHUD->Paint(Canvas);
		}
	}
	Overlay.Draw(Canvas);
}

void UGameEngine::UpdateHudStats(float DeltaTime)
{
	FpsAccumTime += DeltaTime;
	++FpsAccumFrames;
	if (FpsAccumTime < 0.25f)
	{
		return;
	}

	DisplayMs = (FpsAccumTime / static_cast<float>(FpsAccumFrames)) * 1000.0f;
	DisplayFps = static_cast<float>(FpsAccumFrames) / FpsAccumTime;
	FpsAccumTime = 0.0f;
	FpsAccumFrames = 0;

	const FFrameStats& Stats = GetRendererModule().GetFrameStats();

	int32 FbWidth = 0;
	int32 FbHeight = 0;
	Window->GetFramebufferSize(FbWidth, FbHeight);

	const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
	const auto RamMb = static_cast<double>(Memory.UsedPhysical) / (1024.0 * 1024.0);
	const FRHIGPUMemoryStats Gpu = GDynamicRHI != nullptr ? GDynamicRHI->GetGPUMemoryStats() : FRHIGPUMemoryStats{};

	ANSICHAR Vram[32];
	(void)FCStringAnsi::Snprintf(Vram, UE_ARRAY_COUNT(Vram), "VRAM n/a");
	if (Gpu.bValid)
	{
		const auto BudgetMb = static_cast<double>(Gpu.BudgetBytes) / (1024.0 * 1024.0);
		if (Gpu.bReportsUsage)
		{
			const auto UsedMb = static_cast<double>(Gpu.UsedBytes) / (1024.0 * 1024.0);
			(void)FCStringAnsi::Snprintf(Vram, UE_ARRAY_COUNT(Vram), "VRAM %.0f/%.0fM", UsedMb, BudgetMb);
		}
		else
		{
			(void)FCStringAnsi::Snprintf(Vram, UE_ARRAY_COUNT(Vram), "VRAM %.0fM", BudgetMb);
		}
	}

	// Compact two-column stats (top-right).
	const FString Text = FString::Printf("FPS %5.0f   MS %5.2f\n"
										 "RAM %4.0fM  %s\n"
										 "TRIS %5d  OBJ %d/%d\n"
										 "RES %dx%d\n"
										 "GPU Sh %.2f Pl %.2f Col %.2f\n"
										 "    AO %.2f Pst %.2f",
		static_cast<double>(DisplayFps), static_cast<double>(DisplayMs), RamMb, Vram, Stats.TrianglesSubmitted,
		Stats.ObjectsVisible, Stats.ObjectsTotal, FbWidth, FbHeight, static_cast<double>(Stats.ShadowMs),
		static_cast<double>(Stats.PlanarMs), static_cast<double>(Stats.ColorMs), static_cast<double>(Stats.SsaoMs),
		static_cast<double>(Stats.PostMs));
	Overlay.SetRightText(Text);
	Overlay.SetText(FString());
	Overlay.SetCenterText(FString());

	Overlay.SetBottomLeftText(FString::Printf("F1 AABB %s\nF2 Coll+Trace %s\nF3 NavMesh %s\nF6 Axes %s",
		EngineShowFlags.Bounds ? "ON" : "OFF", bCollisionDebugEnabled ? "ON" : "OFF",
		bNavMeshDebugEnabled ? "ON" : "OFF", EngineShowFlags.AxesGizmo ? "ON" : "OFF"));
}

void UGameEngine::HandleInput(float /*DeltaTime*/)
{
	FGenericWindow& InputWindow = *Window;

	const bool bF1Down = InputWindow.IsKeyPressed(EKeys::F1);
	if (bF1Down && !bDebugKeyWasDown)
	{
		EngineShowFlags.Bounds = !EngineShowFlags.Bounds;
		UE_LOG(LogEngine, Log, "Debug draw (mesh AABB): %s", EngineShowFlags.Bounds ? "on" : "off");
	}
	bDebugKeyWasDown = bF1Down;

	const bool bF2Down = InputWindow.IsKeyPressed(EKeys::F2);
	if (bF2Down && !bCollisionDebugKeyWasDown)
	{
		bCollisionDebugEnabled = !bCollisionDebugEnabled;
		UE_LOG(LogEngine, Log, "Collision debug: %s", bCollisionDebugEnabled ? "on" : "off");
	}
	bCollisionDebugKeyWasDown = bF2Down;

	const bool bF3Down = InputWindow.IsKeyPressed(EKeys::F3);
	if (bF3Down && !bNavMeshDebugKeyWasDown)
	{
		bNavMeshDebugEnabled = !bNavMeshDebugEnabled;
		UE_LOG(LogEngine, Log, "NavMesh debug: %s", bNavMeshDebugEnabled ? "on" : "off");
	}
	bNavMeshDebugKeyWasDown = bF3Down;

	const bool bF4Down = InputWindow.IsKeyPressed(EKeys::F4);
	if (bF4Down && !bHudStatsKeyWasDown)
	{
		SetHudStatsVisible(!bShowHudStats);
		FpsAccumTime = 0.0f;
		FpsAccumFrames = 0;
		UE_LOG(LogEngine, Log, "HUD stats: %s", bShowHudStats ? "on" : "off");
	}
	bHudStatsKeyWasDown = bF4Down;

	const bool bF5Down = InputWindow.IsKeyPressed(EKeys::F5);
	if (bF5Down && !bReloadKeyWasDown)
	{
		const EShaderReloadResult Result = ReloadAllShaders(true);
		if (Result == EShaderReloadResult::Failed)
		{
			UE_LOG(LogEngine, Warning, "Shader reload failed; previous programs kept where possible");
		}
		else if (Result == EShaderReloadResult::Reloaded)
		{
			UE_LOG(LogEngine, Log, "Shaders reloaded (F5)");
		}
		else
		{
			UE_LOG(LogEngine, Log, "Shaders unchanged (F5)");
		}
	}
	bReloadKeyWasDown = bF5Down;

	const bool bF6Down = InputWindow.IsKeyPressed(EKeys::F6);
	if (bF6Down && !bAxesGizmoKeyWasDown)
	{
		EngineShowFlags.AxesGizmo = !EngineShowFlags.AxesGizmo;
		UE_LOG(LogEngine, Log, "Axes gizmo: %s", EngineShowFlags.AxesGizmo ? "on" : "off");
	}
	bAxesGizmoKeyWasDown = bF6Down;
}

void UGameEngine::ProcessInput(float DeltaTime)
{
	// UE: the viewport's key and mouse events reach the player controller's input (UGameViewportClient::InputKey /
	// InputAxis). The window is polled: a key that changed state is a pressed or released event.
	APlayerController* PlayerController = GetFirstLocalPlayerController();
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);
	for (const FKey& Key : AllKeys)
	{
		if (Key.IsGamepadKey() || Key.IsAxis1D())
		{
			continue;
		}
		const bool bDown = Window->IsKeyPressed(Key);
		const bool bWasDown = DownKeys.Contains(Key);
		if (bDown == bWasDown)
		{
			continue;
		}
		if (bDown)
		{
			DownKeys.Add(Key);
		}
		else
		{
			DownKeys.Remove(Key);
		}
		if (PlayerController != nullptr)
		{
			(void)PlayerController->InputKey(Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f, false);
		}
	}

	// The mouse moves the MouseX / MouseY axes in pixels (up is positive, as in UE) while the cursor is captured or
	// the left button is down; the first sample after a change only records the position.
	const FVector2D Cursor = Window->GetCursorPos();
	const double MouseX = Cursor.X;
	const double MouseY = Cursor.Y;
	const bool bWantLook = Window->IsCursorCaptured() || Window->IsMouseButtonDown(EMouseButtons::Left);
	if (bWantLook)
	{
		if (bMouseLookSampleValid && PlayerController != nullptr)
		{
			const float Dx = static_cast<float>(MouseX - LastMouseX);
			const float Dy = static_cast<float>(MouseY - LastMouseY);
			if (Dx != 0.0f)
			{
				(void)PlayerController->InputAxis(EKeys::MouseX, Dx, DeltaTime, 1, false);
			}
			if (Dy != 0.0f)
			{
				(void)PlayerController->InputAxis(EKeys::MouseY, -Dy, DeltaTime, 1, false);
			}
		}
		bMouseLookSampleValid = true;
	}
	else
	{
		bMouseLookSampleValid = false;
	}
	LastMouseX = MouseX;
	LastMouseY = MouseY;
}

void UGameEngine::Render()
{
	int32 FbWidth = 0;
	int32 FbHeight = 0;
	Window->GetFramebufferSize(FbWidth, FbHeight);
	if (FbWidth <= 0 || FbHeight <= 0)
	{
		return;
	}

	// The view camera's projection follows the framebuffer (a new player camera starts with its own).
	UCameraComponent& Camera = *GetViewCamera();
	Camera.SetPerspective(Camera.FieldOfView(), static_cast<float>(FbWidth) / static_cast<float>(FbHeight),
		DefaultCameraNearPlane, DefaultCameraFarPlane);

	// The world's components send their moved transforms and poses to the scene, then the view family is rendered
	// (UE: UGameViewportClient::Draw), then the HUD and the debug text go through the frame's canvas.
	UWorld* World = GetGameWorld();
	World->SendAllEndOfFrameUpdates();
	FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(FbWidth, FbHeight, World->Scene, EngineShowFlags));
	const FSceneView View(FSceneView::FromCamera(ViewFamily, Camera));
	ViewFamily.Views.Add(&View);
	FCanvas Canvas(FbWidth, FbHeight);
	GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);
	PaintHudAndOverlay(Canvas);
	Canvas.Flush_GameThread();
}

void UGameEngine::WritePendingScreenshot()
{
	if (PendingScreenshotPath.IsEmpty())
	{
		return;
	}
	const FString Path = MoveTemp(PendingScreenshotPath);
	PendingScreenshotPath.Empty();

	int32 Width = 0;
	int32 Height = 0;
	Window->GetFramebufferSize(Width, Height);
	TArray<uint8> Bgr;
	GetRendererModule().ReadFramebufferBgr(Width, Height, Bgr);
	if (Bgr.Num() == 0)
	{
		UE_LOG(LogEngine, Warning, "Screenshot skipped: empty framebuffer");
		return;
	}

	// BITMAPFILEHEADER + BITMAPINFOHEADER, rows bottom-up (as glReadPixels returns them), padded to 4 bytes.
	const int32 RowBytes = Width * 3;
	const int32 Stride = (RowBytes + 3) & ~3;
	const uint32 PixelBytes = static_cast<uint32>(Stride * Height);
	TArray<uint8> File;
	File.Reserve(static_cast<int32>(54 + PixelBytes));
	auto Put16 = [&File](uint32 Value)
	{
		File.Add(static_cast<uint8>(Value & 0xFFu));
		File.Add(static_cast<uint8>((Value >> 8) & 0xFFu));
	};
	auto Put32 = [&Put16](uint32 Value)
	{
		Put16(Value & 0xFFFFu);
		Put16(Value >> 16);
	};
	Put16(0x4D42u); // "BM"
	Put32(54u + PixelBytes);
	Put32(0u);
	Put32(54u);
	Put32(40u);
	Put32(static_cast<uint32>(Width));
	Put32(static_cast<uint32>(Height));
	Put16(1u);
	Put16(24u);
	Put32(0u);
	Put32(PixelBytes);
	Put32(2835u);
	Put32(2835u);
	Put32(0u);
	Put32(0u);
	for (int32 Row = 0; Row < Height; ++Row)
	{
		File.Append(Bgr.GetData() + Row * RowBytes, RowBytes);
		for (int32 Pad = RowBytes; Pad < Stride; ++Pad)
		{
			File.Add(0);
		}
	}
	if (FFileHelper::SaveArrayToFile(File, *Path))
	{
		UE_LOG(LogEngine, Log, "Screenshot saved: %s (%dx%d)", *Path, Width, Height);
	}
	else
	{
		UE_LOG(LogEngine, Error, "Screenshot could not be written: %s", *Path);
	}
}
