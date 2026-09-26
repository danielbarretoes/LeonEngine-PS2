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
#include "GameFramework/HUD.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CString.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "RendererInterface.h"
#include "UnrealClient.h"

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
	bMouseLookSampleValid = false;
}

bool UGameViewportClient::InputKey(FViewport* /*InViewport*/, int32 ControllerId, FKey Key, EInputEvent EventType,
	float AmountDepressed, bool bGamepad)
{
	if (IgnoreInput())
	{
		return false;
	}
	// The player of the controller id (Leon has one local player: controller 0).
	(void)ControllerId;
	ULocalPlayer* TargetPlayer = GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
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
	(void)ControllerId;
	ULocalPlayer* TargetPlayer = GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
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
	TArray<FKey> AllKeys;
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
}

void UGameViewportClient::UpdateHudStats(float DeltaTime)
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
	const FIntPoint Size = Viewport->GetSizeXY();

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
	FDebugOverlay& Overlay = GEngine->GetDebugOverlay();
	const FString Text = FString::Printf("FPS %5.0f   MS %5.2f\n"
										 "RAM %4.0fM  %s\n"
										 "TRIS %5d  OBJ %d/%d\n"
										 "RES %dx%d\n"
										 "GPU Sh %.2f Pl %.2f Col %.2f",
		static_cast<double>(DisplayFps), static_cast<double>(DisplayMs), RamMb, Vram, Stats.TrianglesSubmitted,
		Stats.ObjectsVisible, Stats.ObjectsTotal, Size.X, Size.Y, static_cast<double>(Stats.ShadowMs),
		static_cast<double>(Stats.PlanarMs), static_cast<double>(Stats.ColorMs));
	Overlay.SetRightText(Text);
	Overlay.SetText(FString());
	Overlay.SetCenterText(FString());

	Overlay.SetBottomLeftText(FString::Printf("F1 AABB %s\nF2 Coll+Trace %s\nF3 Navigation %s\nF6 Axes %s",
		EngineShowFlags.Bounds ? "ON" : "OFF", EngineShowFlags.Collision ? "ON" : "OFF",
		EngineShowFlags.Navigation ? "ON" : "OFF", EngineShowFlags.AxesGizmo ? "ON" : "OFF"));
}

void UGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const FIntPoint Size = InViewport->GetSizeXY();

	// The view camera's projection follows the framebuffer (a new player camera starts with its own).
	UCameraComponent& Camera = *GetViewCamera();
	Camera.SetPerspective(Camera.FieldOfView(), static_cast<float>(Size.X) / static_cast<float>(Size.Y),
		DefaultCameraNearPlane, DefaultCameraFarPlane);

	// The world's components send their moved transforms and poses to the scene, then the view family is rendered,
	// then the HUD and the engine's text go through the frame's canvas (UE).
	World->SendAllEndOfFrameUpdates();
	FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(Size.X, Size.Y, World->Scene, EngineShowFlags));
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

	// The player's HUD (UE: the HUD's PostRender), then the on-screen messages and stats (UE: the engine's).
	if (const APlayerController* PlayerController = GetFirstLocalPlayerController())
	{
		if (PlayerController->MyHUD != nullptr)
		{
			PlayerController->MyHUD->Paint(*SceneCanvas);
		}
	}
	if (GEngine != nullptr)
	{
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
