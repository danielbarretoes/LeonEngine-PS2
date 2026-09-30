#include "Engine/GameViewportClient.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasTypes.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineLogs.h"
#include "EngineStats.h"
#include "GameFramework/HUD.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/DualShockPressure.h"
#include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/IInputInterface.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/UnrealMemory.h"
#include "Misc/CString.h"
#include "Misc/FileHelper.h"
#include "Misc/MemStack.h"
#include "Misc/Parse.h"
#include "RendererInterface.h"
#include "SceneInterface.h"
#include "UnrealClient.h"

DEFINE_STAT(STAT_EndOfFrameUpdates);
DEFINE_STAT(STAT_SceneRendering);
DEFINE_STAT(STAT_HUD);
DEFINE_STAT(STAT_DebugOverlay);

namespace
{
	/** How often the stats text and the `stat cycles` page refresh, in seconds. */
	constexpr float StatsRefreshSeconds = 0.25f;
	/** The `stat cycles` page: the scopes at least this long per frame, this deep, this many lines (the screen's). */
	constexpr double CycleStatsMinMilliseconds = 0.05;
	constexpr int32 CycleStatsMaxDepth = 3;
	constexpr int32 CycleStatsMaxLines = 24;
} // namespace

UGameViewportClient::UGameViewportClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGameViewportClient::~UGameViewportClient() = default;

void UGameViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance)
{
	GameInstance = OwningGameInstance;
	WorldContext.GameViewport = this;
	if (DefaultViewCamera == nullptr)
	{
		DefaultViewCamera = NewObject<UCameraComponent>(this);
	}
}

void UGameViewportClient::SetViewportWindow(FGenericWindow* InWindow)
{
	Viewport.Reset();
	if (InWindow != nullptr)
	{
		Viewport = MakeUnique<FViewport>(this, InWindow);
		// The input settings say whether the window keeps the mouse (UE: DefaultViewportMouseCaptureMode).
		SetMouseCaptureMode(GetDefault<UInputSettings>()->DefaultViewportMouseCaptureMode);
	}
}

ULocalPlayer* UGameViewportClient::SetupInitialLocalPlayer(FString& OutError)
{
	if (GameInstance == nullptr)
	{
		OutError = TEXT("The viewport client has no game instance");
		return nullptr;
	}
	return GameInstance->CreateInitialPlayer(OutError);
}

FGenericWindow* UGameViewportClient::GetWindow() const
{
	return Viewport ? Viewport->GetWindow() : nullptr;
}

UWorld* UGameViewportClient::GetWorld() const
{
	return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
}

APlayerController* UGameViewportClient::GetFirstLocalPlayerController() const
{
	return GameInstance != nullptr ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}

UCameraComponent& UGameViewportClient::GetRenderCamera(UCameraComponent& ViewCamera, float Alpha)
{
	const APlayerController* PlayerController = GetFirstLocalPlayerController();
	const APlayerCameraManager* CameraManager =
		PlayerController != nullptr ? PlayerController->PlayerCameraManager : nullptr;
	if (CameraManager == nullptr || !CameraManager->HasCameraCache() || &ViewCamera != CameraManager->GetViewCamera())
	{
		return ViewCamera;
	}
	if (RenderCamera == nullptr)
	{
		RenderCamera = NewObject<UCameraComponent>(this);
	}
	// The player camera's view between its last two updates, with the view camera's lens.
	FMinimalViewInfo View;
	CameraManager->GetInterpolatedView(Alpha, View);
	RenderCamera->SetMode(ECameraMode::FreeLook);
	RenderCamera->SetPerspective(View.FOV, ViewCamera.GetAspect(), ViewCamera.GetNearPlane(), ViewCamera.GetFarPlane());
	RenderCamera->SetEyeLocation(View.Location);
	RenderCamera->SetViewRotation(View.Rotation);
	RenderCamera->ViewModelFOV = ViewCamera.ViewModelFOV;
	return *RenderCamera;
}

UCameraComponent* UGameViewportClient::GetViewCamera() const
{
	const APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr)
	{
		return PlayerController->PlayerCameraManager->GetViewCamera();
	}
	return DefaultViewCamera;
}

void UGameViewportClient::SetMouseCaptureMode(EMouseCaptureMode Mode)
{
	FGenericWindow* Window = GetWindow();
	if (Window == nullptr)
	{
		return;
	}
	Window->SetCursorCaptured(Mode == EMouseCaptureMode::CapturePermanently ||
		Mode == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
	// The next mouse sample only records the position, so a capture change does not jump the view.
	bMouseLookSampleValid = false;
}

bool UGameViewportClient::IsCursorCaptured() const
{
	const FGenericWindow* Window = GetWindow();
	return Window != nullptr && Window->IsCursorCaptured();
}

void UGameViewportClient::SetIgnoreInput(bool bIgnore)
{
	bIgnoreInput = bIgnore;
	// Keys held when the input comes back are new presses, and the next mouse sample only records the position.
	DownKeys.Reset();
	for (FViewportGamepadState& Pad : GamepadStates)
	{
		Pad = FViewportGamepadState();
	}
	bMouseLookSampleValid = false;
}

bool UGameViewportClient::InputKey(FViewport* /*InViewport*/, int32 ControllerId, FKey Key, EInputEvent EventType,
	float AmountDepressed, bool bGamepad)
{
	if (IgnoreInput())
	{
		return false;
	}
	// The player of the controller id (UE): a controller no local player uses (the PS2's second pad in a one-player
	// game) reaches no one.
	ULocalPlayer* TargetPlayer = FindLocalPlayerFromControllerId(ControllerId);
	if (TargetPlayer != nullptr && TargetPlayer->PlayerController != nullptr)
	{
		return TargetPlayer->PlayerController->InputKey(Key, EventType, AmountDepressed, bGamepad);
	}
	return false;
}

bool UGameViewportClient::InputAxis(FViewport* /*InViewport*/, int32 ControllerId, FKey Key, float Delta,
	float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (IgnoreInput())
	{
		return false;
	}
	ULocalPlayer* TargetPlayer = FindLocalPlayerFromControllerId(ControllerId);
	if (TargetPlayer != nullptr && TargetPlayer->PlayerController != nullptr)
	{
		return TargetPlayer->PlayerController->InputAxis(Key, Delta, DeltaTime, NumSamples, bGamepad);
	}
	return false;
}

void UGameViewportClient::ProcessInput(float DeltaTime)
{
	FGenericWindow* Window = GetWindow();
	if (Window == nullptr || IgnoreInput())
	{
		return;
	}
	// The window is polled: a key that changed state since the last frame is a pressed or released event.
	// Reused every frame (the game thread's): the copy keeps its capacity.
	static TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);
	for (const FKey& Key : AllKeys)
	{
		if (Key.IsGamepadKey() || Key.IsAxis1D())
		{
			continue;
		}
		const bool bDown = Window->IsKeyPressed(Key);
		if (bDown == DownKeys.Contains(Key))
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
		(void)InputKey(Viewport.Get(), 0, Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f, false);
	}

	// The mouse moves the MouseX / MouseY axes in pixels (up is positive, as in UE) while the cursor is captured or
	// the left button is down; the first sample after a change only records the position.
	const FVector2D Cursor = Window->GetCursorPos();
	const double MouseX = Cursor.X;
	const double MouseY = Cursor.Y;
	const bool bWantLook = Window->IsCursorCaptured() || Window->IsMouseButtonDown(EMouseButtons::Left);
	if (bWantLook)
	{
		if (bMouseLookSampleValid)
		{
			const float Dx = static_cast<float>(MouseX - LastMouseX);
			const float Dy = static_cast<float>(MouseY - LastMouseY);
			if (Dx != 0.0f)
			{
				(void)InputAxis(Viewport.Get(), 0, EKeys::MouseX, Dx, DeltaTime, 1, false);
			}
			if (Dy != 0.0f)
			{
				(void)InputAxis(Viewport.Get(), 0, EKeys::MouseY, -Dy, DeltaTime, 1, false);
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
	// A free cursor moves over the HUD's widgets (hover, clicks), in the frame's pixels.
	if (!Window->IsCursorCaptured())
	{
		ULocalPlayer* Player = FindLocalPlayerFromControllerId(0);
		AHUD* HUD =
			Player != nullptr && Player->PlayerController != nullptr ? Player->PlayerController->MyHUD : nullptr;
		if (HUD != nullptr)
		{
			const IRendererModule* Renderer = GetRendererModulePtr();
			const FIntPoint WindowSize = Viewport ? Viewport->GetWindowSize() : FIntPoint(0, 0);
			HUD->InputMouseMove(Renderer != nullptr ? Renderer->WindowToRenderTarget(Cursor, WindowSize) : Cursor);
		}
	}

	ProcessGamepadInput(AllKeys, DeltaTime);
}

void UGameViewportClient::ProcessGamepadInput(const TArray<FKey>& AllKeys, float DeltaTime)
{
	static_assert(UE_ARRAY_COUNT(FViewportGamepadState::Pressures) == FDualShockPressure::NumButtons,
		"a pressure a DualShock 2 button");
	static_assert(UE_ARRAY_COUNT(GamepadStates) == IInputInterface::MaxControllers, "a state a controller");
	const int32 NumControllers = InputInterface != nullptr
		? FMath::Min(InputInterface->GetNumControllers(), IInputInterface::MaxControllers)
		: 1;
	for (int32 ControllerId = 0; ControllerId < NumControllers; ++ControllerId)
	{
		const bool bConnected = InputInterface != nullptr && InputInterface->IsGamepadConnected(ControllerId);
		FViewportGamepadState& Pad = GamepadStates[ControllerId];
		for (const FKey& Key : AllKeys)
		{
			if (!Key.IsGamepadKey())
			{
				continue;
			}
			if (Key.IsAxis1D())
			{
				const float Value = bConnected ? InputInterface->GetGamepadAnalog(ControllerId, Key) : 0.0f;
				const int32 Pressure = FDualShockPressure::IndexOfAxis(Key);
				if (Pressure != INDEX_NONE)
				{
					// A pressure goes when it changes (the axis keeps its last value): most frames press nothing.
					if (Value == Pad.Pressures[Pressure])
					{
						continue;
					}
					Pad.Pressures[Pressure] = Value;
				}
				// A stick's sample every frame: an axis without samples keeps its last value (UPlayerInput).
				(void)InputAxis(Viewport.Get(), ControllerId, Key, Value, DeltaTime, 1, true);
				continue;
			}
			// A pad that goes away releases its buttons.
			const bool bDown = bConnected && InputInterface->IsGamepadKeyDown(ControllerId, Key);
			if (bDown == Pad.DownKeys.Contains(Key))
			{
				continue;
			}
			if (bDown)
			{
				Pad.DownKeys.Add(Key);
			}
			else
			{
				Pad.DownKeys.Remove(Key);
			}
			(void)InputKey(
				Viewport.Get(), ControllerId, Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f, true);
		}
	}
}

ULocalPlayer* UGameViewportClient::FindLocalPlayerFromControllerId(int32 ControllerId) const
{
	if (GameInstance == nullptr)
	{
		return nullptr;
	}
	for (ULocalPlayer* Player : GameInstance->GetLocalPlayers())
	{
		if (Player != nullptr && Player->GetControllerId() == ControllerId)
		{
			return Player;
		}
	}
	return nullptr;
}

void UGameViewportClient::Tick(float DeltaTime)
{
	if (GEngine == nullptr || GetWindow() == nullptr)
	{
		return;
	}
	GEngine->GetDebugOverlay().TickOnScreenMessages(DeltaTime);
	const bool bStatsVisible = GEngine->IsHudStatsVisible();
	if (bStatsVisible != bStatsVisibleLastTick)
	{
		FpsAccumTime = 0.0f;
		FpsAccumFrames = 0;
		bStatsVisibleLastTick = bStatsVisible;
	}
	if (bStatsVisible)
	{
		UpdateHudStats(DeltaTime);
	}
	const bool bCycleStatsVisible = GEngine->IsCycleStatsVisible();
	if (bCycleStatsVisible != bCycleStatsVisibleLastTick)
	{
		CycleStatsAccumTime = 0.0f;
		CycleStatsWindow.Restart();
		bCycleStatsVisibleLastTick = bCycleStatsVisible;
	}
	if (bCycleStatsVisible)
	{
		UpdateCycleStatsPage(DeltaTime);
	}
	const bool bMemoryStatsVisible = GEngine->IsMemoryStatsVisible();
	if (bMemoryStatsVisible != bMemoryStatsVisibleLastTick)
	{
		// The page's first refresh comes at once, with the churn counted from now.
		MemoryStatsAccumTime = StatsRefreshSeconds;
		MemoryStatsFrames = 0;
		MemoryStatsAllocations = FMemory::GetUsage().TotalAllocations;
		for (int32 Index = 0; Index <= FLowLevelMemTracker::NumTags; ++Index)
		{
			MemoryStatsTagAllocations[Index] = FLowLevelMemTracker::GetTagStats(ELLMTag(Index)).TotalAllocations;
		}
		bMemoryStatsVisibleLastTick = bMemoryStatsVisible;
	}
	if (bMemoryStatsVisible)
	{
		UpdateMemoryStatsPage(DeltaTime);
	}
}

void UGameViewportClient::UpdateMemoryStatsPage(float DeltaTime)
{
	MemoryStatsAccumTime += DeltaTime;
	++MemoryStatsFrames;
	if (MemoryStatsAccumTime < StatsRefreshSeconds)
	{
		return;
	}
	MemoryStatsAccumTime = 0.0f;
	const auto Kilobytes = [](uint64 Bytes) { return static_cast<unsigned long long>((Bytes + 1023) / 1024); };
	const double Frames = double(FMath::Max(1, MemoryStatsFrames));
	const FMallocUsage Heap = FMemory::GetUsage();
	const FLLMTagStats Total = FLowLevelMemTracker::GetTagStats(ELLMTag::Count);
	const FMemStack& Stack = FMemStack::Get();
	FString Text = FString::Printf("MEMORY heap %llu KB, peak %llu KB, budget %llu KB\n", Kilobytes(Heap.CurrentBytes),
		Kilobytes(Heap.PeakBytes), Kilobytes(Total.BudgetBytes));
	Text += FString::Printf("%llu blocks, %.1f allocs/frame; arena %llu of %llu KB (peak %llu), %llu overflows\n",
		static_cast<unsigned long long>(Heap.NumAllocations),
		double(Heap.TotalAllocations - MemoryStatsAllocations) / Frames, Kilobytes(Heap.ArenaUsedBytes),
		Kilobytes(Heap.ArenaBytes), Kilobytes(Heap.ArenaPeakBytes),
		static_cast<unsigned long long>(Heap.ArenaOverflows));
	Text += FString::Printf(
		"frame stack peak %llu KB of %llu KB\n", Kilobytes(Stack.GetPeakByteCount()), Kilobytes(Stack.GetChunkBytes()));
	if (FLowLevelMemTracker::IsEnabled())
	{
		Text += "TAG            KB   PEAK BUDGET ALLOC/F";
		for (int32 Index = 0; Index < FLowLevelMemTracker::NumTags; ++Index)
		{
			const FLLMTagStats Tag = FLowLevelMemTracker::GetTagStats(ELLMTag(Index));
			if (Tag.PeakBytes == 0)
			{
				continue;
			}
			Text += FString::Printf("\n%-11s %5llu %6llu %6llu %7.1f", FLowLevelMemTracker::GetTagName(ELLMTag(Index)),
				Kilobytes(Tag.CurrentBytes), Kilobytes(Tag.PeakBytes), Kilobytes(Tag.BudgetBytes),
				double(Tag.TotalAllocations - MemoryStatsTagAllocations[Index]) / Frames);
		}
	}
	for (int32 Index = 0; Index <= FLowLevelMemTracker::NumTags; ++Index)
	{
		MemoryStatsTagAllocations[Index] = FLowLevelMemTracker::GetTagStats(ELLMTag(Index)).TotalAllocations;
	}
	MemoryStatsAllocations = Heap.TotalAllocations;
	MemoryStatsFrames = 0;
	GEngine->GetDebugOverlay().SetText(Text);
}

void UGameViewportClient::UpdateCycleStatsPage(float DeltaTime)
{
	CycleStatsAccumTime += DeltaTime;
	if (CycleStatsAccumTime < StatsRefreshSeconds || CycleStatsWindow.GetNumFrames() <= 0)
	{
		return;
	}
	CycleStatsAccumTime = 0.0f;
	TArray<FString> Lines;
	CycleStatsWindow.GetReportLines(Lines, CycleStatsMinMilliseconds, CycleStatsMaxDepth, CycleStatsMaxLines);
	FString Text =
		FString::Printf("CYCLES over %lld frames (ms, calls)", static_cast<long long>(CycleStatsWindow.GetNumFrames()));
	for (const FString& Line : Lines)
	{
		Text += "\n";
		Text += Line;
	}
	GEngine->GetDebugOverlay().SetText(Text);
	CycleStatsWindow.Restart();
}

void UGameViewportClient::UpdateHudStats(float DeltaTime)
{
	FpsAccumTime += DeltaTime;
	++FpsAccumFrames;
	if (FpsAccumTime < StatsRefreshSeconds)
	{
		return;
	}

	DisplayMs = (FpsAccumTime / static_cast<float>(FpsAccumFrames)) * 1000.0f;
	DisplayFps = static_cast<float>(FpsAccumFrames) / FpsAccumTime;
	FpsAccumTime = 0.0f;
	FpsAccumFrames = 0;

	const FFrameStats& Stats = GetRendererModule().GetFrameStats();
	const FIntPoint Size = Viewport->GetSizeXY();

	// GMalloc's heap now / at most / its budget (the config's total, Docs/PLANS/ps2-shipping.md N17), not the process.
	const FMallocUsage Heap = FMemory::GetUsage();
	const uint64 HeapBudget = FLowLevelMemTracker::GetTagStats(ELLMTag::Count).BudgetBytes;
	const auto ToMb = [](uint64 Bytes) { return static_cast<double>(Bytes) / (1024.0 * 1024.0); };
	ANSICHAR Ram[40];
	if (HeapBudget > 0)
	{
		(void)FCStringAnsi::Snprintf(Ram, UE_ARRAY_COUNT(Ram), "RAM %.1f/%.1f/%.0fM", ToMb(Heap.CurrentBytes),
			ToMb(Heap.PeakBytes), ToMb(HeapBudget));
	}
	else
	{
		(void)FCStringAnsi::Snprintf(
			Ram, UE_ARRAY_COUNT(Ram), "RAM %.1f/%.1fM", ToMb(Heap.CurrentBytes), ToMb(Heap.PeakBytes));
	}
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

	// Compact two-column stats (top-right), out of the way of a game's HUD at the bottom.
	FDebugOverlay& Overlay = GEngine->GetDebugOverlay();
	FString Text = FString::Printf("FPS %5.0f   MS %5.2f\n"
								   "%s  %s\n"
								   "TRIS %5d  OBJ %d/%d\n"
								   "RES %dx%d\n"
								   "GS %d writes, %d tex",
		static_cast<double>(DisplayFps), static_cast<double>(DisplayMs), Ram, Vram, Stats.TrianglesSubmitted,
		Stats.ObjectsVisible, Stats.ObjectsTotal, Size.X, Size.Y, Stats.RegisterWrites, Stats.TextureUploads);
#if PLATFORM_DESKTOP
	// The keyboard's debug views; a console has no F keys.
	Text += FString::Printf("\nF1 AABB %s  F2 COLL %s\nF3 NAV %s  F6 AXES %s", EngineShowFlags.Bounds ? "ON" : "OFF",
		EngineShowFlags.Collision ? "ON" : "OFF", EngineShowFlags.Navigation ? "ON" : "OFF",
		EngineShowFlags.AxesGizmo ? "ON" : "OFF");
#endif
	Overlay.SetRightText(Text);
	Overlay.SetCenterText(FString());
	Overlay.SetBottomLeftText(FString());
}

void UGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const FIntPoint Size = InViewport->GetSizeXY();

	// The view camera's projection follows the frame as the display shows it (a new player camera starts with its own):
	// the GS frame's 640 x 448 pixels fill a 4:3 TV (Docs/PLANS/ps2-preview.md V1).
	UCameraComponent& ViewCamera = *GetViewCamera();
	const IRendererModule* Renderer = GetRendererModulePtr();
	const float Aspect = Renderer != nullptr ? Renderer->GetDisplayAspectRatio(Size)
											 : static_cast<float>(Size.X) / static_cast<float>(Size.Y);
	ViewCamera.SetPerspective(ViewCamera.FieldOfView(), Aspect, DefaultCameraNearPlane, DefaultCameraFarPlane);

	// The frame is drawn between the world's last two steps (ps2-shipping D4): the moving proxies (the world sent each
	// step's transforms at the end of the step) and the player's view, from a camera of the viewport's own, so nothing
	// of the interpolation reaches the simulation. Then the view family is rendered, then the HUD and the engine's text
	// go through the frame's canvas (UE).
	const float Alpha = GEngine != nullptr ? GEngine->GetRenderInterpolationAlpha() : 1.0f;
	if (World->Scene != nullptr)
	{
		SCOPE_CYCLE_COUNTER(STAT_EndOfFrameUpdates);
		World->Scene->InterpolateTransforms(Alpha);
	}
	UCameraComponent& Camera = GetRenderCamera(ViewCamera, Alpha);
	{
		SCOPE_CYCLE_COUNTER(STAT_SceneRendering);
		FSceneViewFamily ViewFamily(
			FSceneViewFamily::ConstructionValues(Size.X, Size.Y, World->Scene, EngineShowFlags));
		FSceneViewInitOptions ViewInitOptions = FSceneView::FromCamera(ViewFamily, Camera);
		// The player's view target owns the view (UE: ViewActor), for the owner-only and owner-hidden primitives.
		const APlayerController* ViewingController = GetFirstLocalPlayerController();
		if (ViewingController != nullptr && ViewingController->PlayerCameraManager != nullptr)
		{
			ViewInitOptions.ViewActor = ViewingController->PlayerCameraManager->GetViewTarget();
		}
		const FSceneView View(ViewInitOptions);
		ViewFamily.Views.Add(&View);
		GetRendererModule().BeginRenderingViewFamily(SceneCanvas, &ViewFamily);
	}

	// The player's HUD (UE: the HUD's PostRender), then the on-screen messages and stats (UE: the engine's).
	if (const APlayerController* PlayerController = GetFirstLocalPlayerController())
	{
		if (PlayerController->MyHUD != nullptr)
		{
			SCOPE_CYCLE_COUNTER(STAT_HUD);
			PlayerController->MyHUD->Paint(*SceneCanvas);
		}
	}
	if (GEngine != nullptr)
	{
		SCOPE_CYCLE_COUNTER(STAT_DebugOverlay);
		GEngine->GetDebugOverlay().Draw(*SceneCanvas);
	}
}

bool UGameViewportClient::ProcessScreenShots(FViewport* InViewport)
{
	if (!FScreenshotRequest::IsScreenshotRequested())
	{
		return false;
	}
	const FString Path = FScreenshotRequest::GetFilename();
	FScreenshotRequest::Reset();

	const FIntPoint Size = InViewport->GetSizeXY();
	const int32 Width = Size.X;
	const int32 Height = Size.Y;
	TArray<uint8> Bgr;
	GetRendererModule().ReadFramebufferBgr(Width, Height, Bgr);
	if (Bgr.Num() == 0)
	{
		UE_LOG(LogEngine, Warning, "Screenshot skipped: empty framebuffer");
		return false;
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
		return true;
	}
	UE_LOG(LogEngine, Error, "Screenshot could not be written: %s", *Path);
	return false;
}

bool UGameViewportClient::HandleShowCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	const FString FlagName = FParse::Token(Cmd, false);
	if (FlagName.IsEmpty())
	{
		int32 NumFlags = 0;
		const TCHAR* const* Names = FEngineShowFlags::GetFlagNames(NumFlags);
		for (int32 Index = 0; Index < NumFlags; ++Index)
		{
			bool* Flag = EngineShowFlags.FindFlag(Names[Index]);
			Ar.Logf(TEXT("%s=%d"), Names[Index], Flag != nullptr && *Flag ? 1 : 0);
		}
		return true;
	}
	bool* Flag = EngineShowFlags.FindFlag(FlagName);
	if (Flag == nullptr)
	{
		Ar.Logf(TEXT("Unknown show flag specified: %s"), *FlagName);
		return true;
	}
	*Flag = !*Flag;
	UE_LOG(LogEngine, Log, TEXT("show %s: %s"), *FlagName, *Flag ? TEXT("on") : TEXT("off"));
	return true;
}

bool UGameViewportClient::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	const TCHAR* Str = Cmd;
	if (FParse::Command(&Str, TEXT("SHOW")))
	{
		return HandleShowCommand(Str, Ar);
	}
	if (GameInstance != nullptr &&
		(GameInstance->Exec(InWorld, Cmd, Ar) || GameInstance->ProcessConsoleExec(Cmd, Ar, nullptr)))
	{
		return true;
	}
	return GEngine != nullptr && GEngine->Exec(InWorld, Cmd, Ar);
}
