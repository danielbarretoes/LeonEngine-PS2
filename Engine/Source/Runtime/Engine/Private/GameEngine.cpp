#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include "GameFramework/Input.h"
#include "HAL/PlatformApplicationMisc.h"
#include "InputCoreTypes.h"
#include "DynamicRHI.h"
#include "HAL/PlatformMemory.h"
#include "Misc/Paths.h"
#include "Engine/GameEngine.h"
#include <string>
#include <thread>

UGameEngine::UGameEngine() : GameInstance(std::make_unique<UGameInstance>()) {
    Application.reset(FPlatformApplicationMisc::CreateApplication());
    Window = Application->MakeWindow();
    PlayerInput.AddMappingContext(UInputMappingContext::MakeDefault());
}

UGameEngine::~UGameEngine() {
    Shutdown();
}

bool UGameEngine::Initialize(int Width, int Height, const char* Title) {
    if (bInitialized) {
        return true;
    }

    if (!Window->Create(Width, Height, Title != nullptr ? Title : "Leon Engine")) {
        return false;
    }

    const std::string ShaderDir = FPaths::ResolveAssetPath("assets/Shaders");
    if (!Renderer.Initialize(ShaderDir)) {
        Window->Destroy();
        return false;
    }
    if (!Overlay.Initialize(ShaderDir)) {
        Renderer.Shutdown();
        Window->Destroy();
        return false;
    }

    Window->SetScrollCallback(
        [this](double YOffset) { PendingScrollY += static_cast<float>(YOffset); });

    Camera.SetPerspective(60.0f, Window->Aspect(), 0.1f, 100.0f);
    Camera.SetTarget({0.0f, 0.0f, 0.0f});

    SetCursorCaptured(true);

    (void)AudioDevice.Initialize(/*silent=*/false);

    GameInstance->Init();

    bInitialized = true;
    bRunning = true;
    bHeadless = false;
    // Stats off until F4 -- matches editor Viewport / PIE (no Engine overlay by default).
    Overlay.SetRightText({});
    Overlay.SetBottomLeftText({});
    return true;
}

bool UGameEngine::InitializeHeadless() {
    if (bInitialized) {
        return true;
    }

    // Redirected stdout (smoke / CI) is fully buffered; keep dedicated logs visible on kill.
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    Resources.SetGpuUploadEnabled(false);
    bHeadless = true;
    (void)AudioDevice.Initialize(/*silent=*/true);
    GameInstance->Init();
    bInitialized = true;
    bRunning = true;
    std::cout << "Leon Engine headless (no OpenGL / window)\n";
    return true;
}

void UGameEngine::Shutdown() {
    if (!bInitialized) {
        return;
    }

    GameInstance->Shutdown();
    AudioDevice.Shutdown();
    Level.Clear();
    Resources.Clear();
    if (!bHeadless) {
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
    ShaderReloadHook = {};
    Hud.Clear();
    CenterHudText.clear();
    LastFbWidth = 0;
    LastFbHeight = 0;
    FpsAccumTime = 0.0f;
    FpsAccumFrames = 0;
    DisplayFps = 0.0f;
    DisplayMs = 0.0f;
}

float UGameEngine::ConsumeScrollY() {
    const float Y = PendingScrollY;
    PendingScrollY = 0.0f;
    return Y;
}

void UGameEngine::SetCursorCaptured(bool bCaptured) {
    if (bHeadless) {
        return;
    }
    GetPlayInputWindow().SetCursorCaptured(bCaptured);
    bMouseLookSampleValid = false; // skip one frame to avoid a jump after mode change
}

bool UGameEngine::IsCursorCaptured() const {
    if (bHeadless) {
        return false;
    }
    return GetPlayInputWindow().IsCursorCaptured();
}

void UGameEngine::SetPlayInputWindow(FGenericWindow* InWindow) {
    FGenericWindow* Previous = PlayInputTarget.GetWindow();
    if (Previous != nullptr && Previous != InWindow) {
        Previous->SetScrollCallback(nullptr);
    }
    PlayInputTarget.SetWindow(InWindow);
    if (InWindow != nullptr) {
        // Accumulate into the same pendingScrollY_ as the main window (PIE New Window scroll).
        InWindow->SetScrollCallback(
            [this](double YOffset) { PendingScrollY += static_cast<float>(YOffset); });
    }
}

FGenericWindow& UGameEngine::GetPlayInputWindow() {
    return PlayInputTarget.Resolve(*Window);
}

const FGenericWindow& UGameEngine::GetPlayInputWindow() const {
    return PlayInputTarget.Resolve(*Window);
}

void UGameEngine::AddOnScreenDebugMessage(std::string Message, float DisplaySeconds,
                                     const glm::vec3& Color) {
    if (bHeadless) {
        std::cout << "[server] " << Message << '\n';
        (void)DisplaySeconds;
        (void)Color;
        return;
    }
    Overlay.AddOnScreenDebugMessage(std::move(Message), DisplaySeconds, Color);
}

EShaderReloadResult UGameEngine::ReloadAllShaders(bool bForce) {
    EShaderReloadResult Result = Renderer.ReloadShaders(bForce);
    Result = MergeShaderReload(Result, Overlay.ReloadShader(bForce));
    if (ShaderReloadHook) {
        Result = MergeShaderReload(Result, ShaderReloadHook(bForce));
    }
    return Result;
}

void UGameEngine::Run(const FUpdateCallback& OnUpdate, const FPreInputCallback& OnPreInput,
                 const FPostRenderCallback& OnPostRender) {
    if (!bInitialized) {
        std::cerr << "Engine is not initialized\n";
        return;
    }

    std::cout << "Level static meshes: " << Level.GetStaticMeshes().size() << '\n';
    std::cout << "Controls: mouse look (cursor captured), scroll zoom (orbit); close window to quit\n";
    std::cout << "Default mode: mouse look, WASD fly along view, Q/E up/down\n";
    std::cout << "Levels: keys 1-9 jump to slot; [ ] previous/next\n";
    std::cout << "Debug: F1 mesh AABBs + light frustum; F2 collision volumes + floor traces\n";
    std::cout << "Debug: F3 NavMesh grid (walkable / blocked)\n";
    std::cout << "Stats: F4 FPS / RAM / TRI overlay (off by default)\n";
    std::cout << "Shaders: F5 force-reload (also auto-reloads when files change)\n";

    auto Previous = std::chrono::steady_clock::now();
    while (bRunning && !Window->ShouldClose()) {
        const auto Now = std::chrono::steady_clock::now();
        float DeltaTime = std::chrono::duration<float>(Now - Previous).count();
        Previous = Now;
        DeltaTime = std::min(DeltaTime, 0.1f);

        Window->PollEvents();
        if (PlayInputTarget.HasOverride()) {
            PlayInputTarget.GetWindow()->PollEvents();
        }
        PlayerInput.Update(GetPlayInputWindow());
        (void)ReloadAllShaders(false);
        if (OnPreInput) {
            OnPreInput();
        }
        HandleInput(DeltaTime);
        TickPlayAudio();
        if (OnUpdate) {
            OnUpdate(DeltaTime);
        }
        TickPlayHud(DeltaTime);
        PendingScrollY = 0.0f; // discard unused wheel (modes that do not ConsumeScrollY)
        Render(OnPostRender);
        Window->SwapBuffers();
    }
}

void UGameEngine::TickPlayAudio() {
    if (!bInitialized || bHeadless) {
        return;
    }
    const glm::vec3 Eye = Camera.GetCameraLocation();
    const glm::vec3 Forward = Camera.ForwardVector();
    const glm::vec3 Up{0.0f, 1.0f, 0.0f};
    AudioDevice.SetListener(Eye, Forward, Up);
    AudioDevice.Tick();
}

void UGameEngine::TickPlayHud(float DeltaTime) {
    if (!bInitialized) {
        return;
    }
    Hud.Tick(DeltaTime);
    Overlay.TickOnScreenMessages(DeltaTime);
    if (bShowHudStats) {
        UpdateHudStats(DeltaTime);
    }
    Overlay.SetCenterText(CenterHudText);
}

void UGameEngine::PaintHudAndOverlay(int FramebufferWidth, int FramebufferHeight) {
    if (!bInitialized || bHeadless) {
        return;
    }
    if (FramebufferWidth <= 0 || FramebufferHeight <= 0) {
        return;
    }
    Hud.Paint(Overlay, FramebufferWidth, FramebufferHeight);
    Overlay.Draw(FramebufferWidth, FramebufferHeight);
}

void UGameEngine::RunHeadless(const FUpdateCallback& OnUpdate, float TickHz) {
    if (!bInitialized || !bHeadless) {
        std::cerr << "Engine::RunHeadless requires InitializeHeadless()\n";
        return;
    }
    if (TickHz < 1.0f) {
        TickHz = 1.0f;
    }
    const float Dt = 1.0f / TickHz;
    std::cout << "Headless tick " << TickHz << " Hz -- Ctrl+C to stop\n";

    using clock = std::chrono::steady_clock;
    auto Next = clock::now();
    while (bRunning) {
        if (OnUpdate) {
            OnUpdate(Dt);
        }
        Next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(Dt));
        const auto Now = clock::now();
        if (Next < Now) {
            // Fell behind -- resync to avoid spiral.
            Next = Now;
        } else {
            std::this_thread::sleep_until(Next);
        }
    }
}

void UGameEngine::SetHudStatsVisible(bool bVisible) {
    bShowHudStats = bVisible;
    if (!bShowHudStats) {
        Overlay.SetRightText({});
        Overlay.SetBottomLeftText({});
        FpsAccumTime = 0.0f;
        FpsAccumFrames = 0;
    }
}

void UGameEngine::UpdateHudStats(float DeltaTime) {
    FpsAccumTime += DeltaTime;
    ++FpsAccumFrames;
    if (FpsAccumTime < 0.25f) {
        return;
    }

    DisplayMs = (FpsAccumTime / static_cast<float>(FpsAccumFrames)) * 1000.0f;
    DisplayFps = static_cast<float>(FpsAccumFrames) / FpsAccumTime;
    FpsAccumTime = 0.0f;
    FpsAccumFrames = 0;

    const FFrameStats& Stats = Renderer.GetFrameStats();

    int FbWidth = 0;
    int FbHeight = 0;
    Window->GetFramebufferSize(FbWidth, FbHeight);

    const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
    const auto RamMb = static_cast<double>(Memory.UsedPhysical) / (1024.0 * 1024.0);
    const FRHIGPUMemoryStats Gpu = GDynamicRHI != nullptr ? GDynamicRHI->GetGPUMemoryStats() : FRHIGPUMemoryStats{};

    std::array<char, 32> Vram{};
    (void)std::snprintf(Vram.data(), Vram.size(), "VRAM n/a");
    if (Gpu.bValid) {
        const auto BudgetMb = static_cast<double>(Gpu.BudgetBytes) / (1024.0 * 1024.0);
        if (Gpu.bReportsUsage) {
            const auto UsedMb = static_cast<double>(Gpu.UsedBytes) / (1024.0 * 1024.0);
            (void)std::snprintf(Vram.data(), Vram.size(), "VRAM %.0f/%.0fM", UsedMb, BudgetMb);
        } else {
            (void)std::snprintf(Vram.data(), Vram.size(), "VRAM %.0fM", BudgetMb);
        }
    }

    // Compact two-column stats (top-right).
    std::array<char, 256> Text{};
    (void)std::snprintf(Text.data(), Text.size(),
                        "FPS %5.0f   MS %5.2f\n"
                        "RAM %4.0fM  %s\n"
                        "TRIS %5d  OBJ %d/%d\n"
                        "RES %dx%d\n"
                        "GPU Sh %.2f Pl %.2f Col %.2f\n"
                        "    AO %.2f Pst %.2f",
                        DisplayFps, DisplayMs, RamMb, Vram.data(), Stats.TrianglesSubmitted,
                        Stats.ObjectsVisible, Stats.ObjectsTotal, FbWidth, FbHeight,
                        Stats.ShadowMs, Stats.PlanarMs, Stats.ColorMs, Stats.SsaoMs, Stats.PostMs);
    Overlay.SetRightText(Text.data());
    Overlay.SetText({});
    Overlay.SetCenterText({});

    std::array<char, 96> Hints{};
    (void)std::snprintf(Hints.data(), Hints.size(), "F1 AABB %s\nF2 Coll+Trace %s\nF3 NavMesh %s",
                        Renderer.IsDebugDrawEnabled() ? "ON" : "OFF",
                        bCollisionDebugEnabled ? "ON" : "OFF",
                        bNavMeshDebugEnabled ? "ON" : "OFF");
    Overlay.SetBottomLeftText(Hints.data());
}

void UGameEngine::HandleInput(float DeltaTime) {
    // PIE "New Window" routes capture + look here; fall back to the main window otherwise.
    FGenericWindow& InputWindow = GetPlayInputWindow();

    const bool bF1Down = InputWindow.IsKeyPressed(EKeys::F1);
    if (bF1Down && !bDebugKeyWasDown) {
        Renderer.ToggleDebugDraw();
        std::cout << "Debug draw (mesh AABB): " << (Renderer.IsDebugDrawEnabled() ? "on" : "off")
                  << '\n';
    }
    bDebugKeyWasDown = bF1Down;

    const bool bF2Down = InputWindow.IsKeyPressed(EKeys::F2);
    if (bF2Down && !bCollisionDebugKeyWasDown) {
        ToggleCollisionDebug();
        std::cout << "Collision debug: " << (IsCollisionDebugEnabled() ? "on" : "off") << '\n';
    }
    bCollisionDebugKeyWasDown = bF2Down;

    const bool bF3Down = InputWindow.IsKeyPressed(EKeys::F3);
    if (bF3Down && !bNavMeshDebugKeyWasDown) {
        ToggleNavMeshDebug();
        std::cout << "NavMesh debug: " << (IsNavMeshDebugEnabled() ? "on" : "off") << '\n';
    }
    bNavMeshDebugKeyWasDown = bF3Down;

    const bool bF4Down = InputWindow.IsKeyPressed(EKeys::F4);
    if (bF4Down && !bHudStatsKeyWasDown) {
        SetHudStatsVisible(!bShowHudStats);
        std::cout << "HUD stats: " << (bShowHudStats ? "on" : "off") << '\n';
    }
    bHudStatsKeyWasDown = bF4Down;

    const bool bF5Down = InputWindow.IsKeyPressed(EKeys::F5);
    if (bF5Down && !bReloadKeyWasDown) {
        const EShaderReloadResult Result = ReloadAllShaders(true);
        if (Result == EShaderReloadResult::Failed) {
            std::cerr << "Shader reload failed; previous programs kept where possible\n";
        } else if (Result == EShaderReloadResult::Reloaded) {
            std::cout << "Shaders reloaded (F5)\n";
        } else {
            std::cout << "Shaders unchanged (F5)\n";
        }
    }
    bReloadKeyWasDown = bF5Down;

    constexpr float KeyboardOrbitSpeed = 90.0f;
    if (bKeyboardOrbitEnabled) {
        // Reuse Move* axes so remapping WASD also remaps keyboard orbit tumble.
        const FMoveAxes2D Axes = PlayerInput.GetMoveAxes2D();
        const float Yaw = Axes.X * KeyboardOrbitSpeed;
        const float Pitch = -Axes.Z * KeyboardOrbitSpeed;
        if (Yaw != 0.0f || Pitch != 0.0f) {
            Camera.Orbit(Yaw * DeltaTime, Pitch * DeltaTime);
        }
    }

    // Orbit mouse: Engine owns scroll zoom on camera distance (look is continuous below).
    if (bOrbitMouseEnabled && Camera.GetMode() == ECameraMode::Orbit) {
        const float ScrollY = ConsumeScrollY();
        if (ScrollY != 0.0f) {
            Camera.Zoom(ScrollY * 0.4f);
        }
    }

    double MouseX = 0.0;
    double MouseY = 0.0;
    InputWindow.GetCursorPos(MouseX, MouseY);

    const bool bWantLook =
        !bSuppressCameraDrag &&
        (InputWindow.IsCursorCaptured() || InputWindow.IsMouseButtonDown(EMouseButtons::Left));

    if (bWantLook) {
        if (bMouseLookSampleValid) {
            const float Dx = static_cast<float>(MouseX - LastMouseX);
            const float Dy = static_cast<float>(MouseY - LastMouseY);
            if (Camera.GetMode() == ECameraMode::FreeLook) {
                constexpr float LookDegreesPerPixel = 0.15f;
                Camera.AddLook(Dx * LookDegreesPerPixel, -Dy * LookDegreesPerPixel);
            } else if (bOrbitMouseEnabled) {
                constexpr float OrbitDegreesPerPixel = 0.3f;
                Camera.Orbit(Dx * OrbitDegreesPerPixel, Dy * OrbitDegreesPerPixel);
            }
        }
        bMouseLookSampleValid = true;
        LastMouseX = MouseX;
        LastMouseY = MouseY;
    } else {
        bMouseLookSampleValid = false;
        LastMouseX = MouseX;
        LastMouseY = MouseY;
    }
}

void UGameEngine::Render(const FPostRenderCallback& OnPostRender) {
    int FbWidth = 0;
    int FbHeight = 0;
    Window->GetFramebufferSize(FbWidth, FbHeight);
    if (FbWidth <= 0 || FbHeight <= 0) {
        return;
    }

    if (FbWidth != LastFbWidth || FbHeight != LastFbHeight) {
        LastFbWidth = FbWidth;
        LastFbHeight = FbHeight;
        Camera.SetPerspective(Camera.FieldOfView(),
                               static_cast<float>(FbWidth) / static_cast<float>(FbHeight), 0.1f,
                               100.0f);
    }

    Renderer.BeginFrame(FbWidth, FbHeight);
    Renderer.DrawScene(Level, Camera);
    PaintHudAndOverlay(FbWidth, FbHeight);
    if (OnPostRender) {
        OnPostRender(FbWidth, FbHeight);
    }
}

