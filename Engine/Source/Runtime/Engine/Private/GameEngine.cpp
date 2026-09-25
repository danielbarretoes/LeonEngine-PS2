#include "Engine/GameEngine.h"

#include "DynamicRHI.h"
#include "EngineLogs.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/CString.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UGameEngine::UGameEngine()
	: GameInstance(MakeUnique<UGameInstance>())
{
	Application.Reset(FPlatformApplicationMisc::CreateApplication());
	Window = Application->MakeWindow();
	PlayerInput.AddMappingContext(UInputMappingContext::MakeDefault());
}

UGameEngine::~UGameEngine()
{
	Shutdown();
}

bool UGameEngine::Initialize(int32 Width, int32 Height, const TCHAR* Title)
{
	if (bInitialized)
	{
		return true;
	}

	if (!Window->Create(Width, Height, Title != nullptr ? Title : "Leon Engine"))
	{
		return false;
	}

	const FString ShaderDir = FPaths::ResolveLegacyContentPath("assets/Shaders");
	if (!Renderer.Initialize(ShaderDir))
	{
		Window->Destroy();
		return false;
	}
	if (!Overlay.Initialize(ShaderDir))
	{
		Renderer.Shutdown();
		Window->Destroy();
		return false;
	}

	Window->OnMouseWheel().BindLambda([this](float Delta) { PendingScrollY += Delta; });

	Camera.SetPerspective(60.0f, Window->Aspect(), DefaultCameraNearPlane, DefaultCameraFarPlane);
	Camera.SetTarget(FVector::ZeroVector);

	SetCursorCaptured(true);

	(void)AudioDevice.Initialize(/*silent=*/false);

	GameInstance->Init();

	bInitialized = true;
	bRunning = true;
	bHeadless = false;
	// Stats off until F4 -- matches editor Viewport / PIE (no Engine overlay by default).
	Overlay.SetRightText(FString());
	Overlay.SetBottomLeftText(FString());
	return true;
}

bool UGameEngine::InitializeHeadless()
{
	if (bInitialized)
	{
		return true;
	}

	Resources.SetGpuUploadEnabled(false);
	bHeadless = true;
	(void)AudioDevice.Initialize(/*silent=*/true);
	GameInstance->Init();
	bInitialized = true;
	bRunning = true;
	UE_LOG(LogEngine, Log, "Leon Engine headless (no OpenGL / window)");
	return true;
}

void UGameEngine::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	GameInstance->Shutdown();
	AudioDevice.Shutdown();
	Level.Clear();
	Resources.Clear();
	if (!bHeadless)
	{
		Overlay.Shutdown();
		Renderer.Shutdown();
		Window->Destroy();
		Window->SetCursorCaptured(false);
	}
	bRunning = false;
	bInitialized = false;
	bHeadless = false;
	bMouseLookSampleValid = false;
	bSuppressCameraDrag = false;
	bKeyboardOrbitEnabled = true;
	bOrbitMouseEnabled = true;
	PendingScrollY = 0.0f;
	Hud.Clear();
	CenterHudText.Empty();
	LastFbWidth = 0;
	LastFbHeight = 0;
	FpsAccumTime = 0.0f;
	FpsAccumFrames = 0;
	DisplayFps = 0.0f;
	DisplayMs = 0.0f;
}

float UGameEngine::ConsumeScrollY()
{
	const float Y = PendingScrollY;
	PendingScrollY = 0.0f;
	return Y;
}

void UGameEngine::SetCursorCaptured(bool bCaptured)
{
	if (bHeadless)
	{
		return;
	}
	GetPlayInputWindow().SetCursorCaptured(bCaptured);
	bMouseLookSampleValid = false; // skip one frame to avoid a jump after mode change
}

bool UGameEngine::IsCursorCaptured() const
{
	if (bHeadless)
	{
		return false;
	}
	return GetPlayInputWindow().IsCursorCaptured();
}

void UGameEngine::SetPlayInputWindow(FGenericWindow* InWindow)
{
	FGenericWindow* Previous = PlayInputTarget.GetWindow();
	if (Previous != nullptr && Previous != InWindow)
	{
		Previous->OnMouseWheel().Unbind();
	}
	PlayInputTarget.SetWindow(InWindow);
	if (InWindow != nullptr)
	{
		// Accumulate into the same PendingScrollY as the main window (PIE New Window scroll).
		InWindow->OnMouseWheel().BindLambda([this](float Delta) { PendingScrollY += Delta; });
	}
}

FGenericWindow& UGameEngine::GetPlayInputWindow()
{
	return PlayInputTarget.Resolve(*Window);
}

const FGenericWindow& UGameEngine::GetPlayInputWindow() const
{
	return PlayInputTarget.Resolve(*Window);
}

void UGameEngine::AddOnScreenDebugMessage(const FString& Message, float DisplaySeconds, const FLinearColor& Color)
{
	if (bHeadless)
	{
		UE_LOG(LogEngine, Log, "[headless] %s", *Message);
		(void)DisplaySeconds;
		(void)Color;
		return;
	}
	Overlay.AddOnScreenDebugMessage(Message, DisplaySeconds, Color);
}

EShaderReloadResult UGameEngine::ReloadAllShaders(bool bForce)
{
	EShaderReloadResult Result = Renderer.ReloadShaders(bForce);
	Result = MergeShaderReload(Result, Overlay.ReloadShader(bForce));
	return Result;
}

void UGameEngine::Run(
	const FUpdateCallback& OnUpdate, const FPreInputCallback& OnPreInput, const FPostRenderCallback& OnPostRender)
{
	if (!bInitialized)
	{
		UE_LOG(LogEngine, Error, "Engine is not initialized");
		return;
	}

	Start();
	double Previous = FPlatformTime::Seconds();
	float DeltaTime = 0.0f;
	do
	{
		const double Now = FPlatformTime::Seconds();
		DeltaTime = FMath::Min(static_cast<float>(Now - Previous), 0.1f);
		Previous = Now;
	} while (Tick(DeltaTime, OnUpdate, OnPreInput, OnPostRender));
}

void UGameEngine::Start()
{
	UE_LOG(LogEngine, Log, "Level static meshes: %d", Level.GetStaticMeshes().Num());
	UE_LOG(LogEngine, Log, "Controls: mouse look (cursor captured), scroll zoom orbit; close window to quit");
	UE_LOG(LogEngine, Log, "Default mode: mouse look, WASD fly along view, Q/E up/down");
	UE_LOG(LogEngine, Log, "Debug: F1 mesh AABBs + light frustum; F2 collision volumes + floor traces");
	UE_LOG(LogEngine, Log, "Debug: F3 NavMesh grid (walkable / blocked)");
	UE_LOG(LogEngine, Log, "Stats: F4 FPS / RAM / TRI overlay (off by default)");
	UE_LOG(LogEngine, Log, "Shaders: F5 force-reload (also auto-reloads when files change)");
}

bool UGameEngine::Tick(float DeltaTime, const FUpdateCallback& OnUpdate, const FPreInputCallback& OnPreInput,
	const FPostRenderCallback& OnPostRender)
{
	if (!bInitialized || !bRunning || Window->ShouldClose())
	{
		return false;
	}

	Window->PollEvents();
	if (PlayInputTarget.HasOverride())
	{
		PlayInputTarget.GetWindow()->PollEvents();
	}
	PlayerInput.Update(GetPlayInputWindow());
	(void)ReloadAllShaders(false);
	if (OnPreInput)
	{
		OnPreInput();
	}
	HandleInput(DeltaTime);
	TickPlayAudio();
	if (OnUpdate)
	{
		OnUpdate(DeltaTime);
	}
	TickPlayHud(DeltaTime);
	PendingScrollY = 0.0f; // discard unused wheel (modes that do not ConsumeScrollY)
	Render(OnPostRender);
	WritePendingScreenshot();
	Window->SwapBuffers();
	return bRunning && !Window->ShouldClose();
}

void UGameEngine::TickPlayAudio()
{
	if (!bInitialized || bHeadless)
	{
		return;
	}
	const FVector Eye = Camera.GetCameraLocation();
	const FVector Forward = Camera.ForwardVector();
	const FVector Up = FVector(0.0f, 0.0f, 1.0f);
	AudioDevice.SetListener(Eye, Forward, Up);
	AudioDevice.Tick();
}

void UGameEngine::TickPlayHud(float DeltaTime)
{
	if (!bInitialized)
	{
		return;
	}
	Hud.Tick(DeltaTime);
	Overlay.TickOnScreenMessages(DeltaTime);
	if (bShowHudStats)
	{
		UpdateHudStats(DeltaTime);
	}
	Overlay.SetCenterText(CenterHudText);
}

void UGameEngine::PaintHudAndOverlay(int32 FramebufferWidth, int32 FramebufferHeight)
{
	if (!bInitialized || bHeadless)
	{
		return;
	}
	if (FramebufferWidth <= 0 || FramebufferHeight <= 0)
	{
		return;
	}
	Hud.Paint(Overlay, FramebufferWidth, FramebufferHeight);
	Overlay.Draw(FramebufferWidth, FramebufferHeight);
}

void UGameEngine::RunHeadless(const FUpdateCallback& OnUpdate, float TickHz)
{
	if (!bInitialized || !bHeadless)
	{
		UE_LOG(LogEngine, Error, "Engine::RunHeadless requires InitializeHeadless()");
		return;
	}
	if (TickHz < 1.0f)
	{
		TickHz = 1.0f;
	}
	const float Dt = 1.0f / TickHz;
	UE_LOG(LogEngine, Log, "Headless tick %g Hz -- Ctrl+C to stop", static_cast<double>(TickHz));

	double Next = FPlatformTime::Seconds();
	while (bRunning)
	{
		if (OnUpdate)
		{
			OnUpdate(Dt);
		}
		Next += static_cast<double>(Dt);
		const double Now = FPlatformTime::Seconds();
		if (Next < Now)
		{
			// Fell behind -- resync to avoid spiral.
			Next = Now;
		}
		else
		{
			FPlatformProcess::Sleep(static_cast<float>(Next - Now));
		}
	}
}

void UGameEngine::SetHudStatsVisible(bool bVisible)
{
	bShowHudStats = bVisible;
	if (!bShowHudStats)
	{
		Overlay.SetRightText(FString());
		Overlay.SetBottomLeftText(FString());
		FpsAccumTime = 0.0f;
		FpsAccumFrames = 0;
	}
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

	const FFrameStats& Stats = Renderer.GetFrameStats();

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

	Overlay.SetBottomLeftText(
		FString::Printf("F1 AABB %s\nF2 Coll+Trace %s\nF3 NavMesh %s", Renderer.IsDebugDrawEnabled() ? "ON" : "OFF",
			bCollisionDebugEnabled ? "ON" : "OFF", bNavMeshDebugEnabled ? "ON" : "OFF"));
}

void UGameEngine::HandleInput(float DeltaTime)
{
	// PIE "New Window" routes capture + look here; fall back to the main window otherwise.
	FGenericWindow& InputWindow = GetPlayInputWindow();

	const bool bF1Down = InputWindow.IsKeyPressed(EKeys::F1);
	if (bF1Down && !bDebugKeyWasDown)
	{
		Renderer.ToggleDebugDraw();
		UE_LOG(LogEngine, Log, "Debug draw (mesh AABB): %s", Renderer.IsDebugDrawEnabled() ? "on" : "off");
	}
	bDebugKeyWasDown = bF1Down;

	const bool bF2Down = InputWindow.IsKeyPressed(EKeys::F2);
	if (bF2Down && !bCollisionDebugKeyWasDown)
	{
		ToggleCollisionDebug();
		UE_LOG(LogEngine, Log, "Collision debug: %s", IsCollisionDebugEnabled() ? "on" : "off");
	}
	bCollisionDebugKeyWasDown = bF2Down;

	const bool bF3Down = InputWindow.IsKeyPressed(EKeys::F3);
	if (bF3Down && !bNavMeshDebugKeyWasDown)
	{
		ToggleNavMeshDebug();
		UE_LOG(LogEngine, Log, "NavMesh debug: %s", IsNavMeshDebugEnabled() ? "on" : "off");
	}
	bNavMeshDebugKeyWasDown = bF3Down;

	const bool bF4Down = InputWindow.IsKeyPressed(EKeys::F4);
	if (bF4Down && !bHudStatsKeyWasDown)
	{
		SetHudStatsVisible(!bShowHudStats);
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

	constexpr float KeyboardOrbitSpeed = 90.0f;
	if (bKeyboardOrbitEnabled)
	{
		// Reuse Move* axes so remapping WASD also remaps keyboard orbit tumble: right turns the view right, forward
		// tilts it up (the eye goes down).
		const FVector2D MoveInput = PlayerInput.GetMoveInput();
		const float Yaw = MoveInput.Y * KeyboardOrbitSpeed;
		const float Pitch = MoveInput.X * KeyboardOrbitSpeed;
		if (Yaw != 0.0f || Pitch != 0.0f)
		{
			Camera.AddViewRotation(FRotator(Pitch * DeltaTime, Yaw * DeltaTime, 0.0f));
		}
	}

	// Orbit mouse: Engine owns scroll zoom on camera distance (look is continuous below).
	if (bOrbitMouseEnabled && Camera.GetMode() == ECameraMode::Orbit)
	{
		const float ScrollY = ConsumeScrollY();
		if (ScrollY != 0.0f)
		{
			/** cm per wheel notch */
			constexpr float ZoomPerNotch = 40.0f;
			Camera.Zoom(ScrollY * ZoomPerNotch);
		}
	}

	const FVector2D Cursor = InputWindow.GetCursorPos();
	const double MouseX = Cursor.X;
	const double MouseY = Cursor.Y;

	const bool bWantLook =
		!bSuppressCameraDrag && (InputWindow.IsCursorCaptured() || InputWindow.IsMouseButtonDown(EMouseButtons::Left));

	if (bWantLook)
	{
		if (bMouseLookSampleValid)
		{
			const float Dx = static_cast<float>(MouseX - LastMouseX);
			const float Dy = static_cast<float>(MouseY - LastMouseY);
			if (Camera.GetMode() == ECameraMode::FreeLook)
			{
				constexpr float LookDegreesPerPixel = 0.15f;
				Camera.AddViewRotation(FRotator(-Dy * LookDegreesPerPixel, Dx * LookDegreesPerPixel, 0.0f));
			}
			else if (bOrbitMouseEnabled)
			{
				// Dragging down tilts the view down (the eye rises over the target).
				constexpr float OrbitDegreesPerPixel = 0.3f;
				Camera.AddViewRotation(FRotator(-Dy * OrbitDegreesPerPixel, Dx * OrbitDegreesPerPixel, 0.0f));
			}
		}
		bMouseLookSampleValid = true;
		LastMouseX = MouseX;
		LastMouseY = MouseY;
	}
	else
	{
		bMouseLookSampleValid = false;
		LastMouseX = MouseX;
		LastMouseY = MouseY;
	}
}

void UGameEngine::Render(const FPostRenderCallback& OnPostRender)
{
	int32 FbWidth = 0;
	int32 FbHeight = 0;
	Window->GetFramebufferSize(FbWidth, FbHeight);
	if (FbWidth <= 0 || FbHeight <= 0)
	{
		return;
	}

	if (FbWidth != LastFbWidth || FbHeight != LastFbHeight)
	{
		LastFbWidth = FbWidth;
		LastFbHeight = FbHeight;
		Camera.SetPerspective(Camera.FieldOfView(), static_cast<float>(FbWidth) / static_cast<float>(FbHeight),
			DefaultCameraNearPlane, DefaultCameraFarPlane);
	}

	Renderer.BeginFrame(FbWidth, FbHeight);
	Renderer.DrawScene(Level, Camera);
	PaintHudAndOverlay(FbWidth, FbHeight);
	if (OnPostRender)
	{
		OnPostRender(FbWidth, FbHeight);
	}
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
	Renderer.ReadFramebufferBgr(Width, Height, Bgr);
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
