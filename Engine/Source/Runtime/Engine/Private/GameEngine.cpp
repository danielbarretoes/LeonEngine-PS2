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

UGameEngine::UGameEngine() : gameInstance_(std::make_unique<UGameInstance>()) {
    application_.reset(FPlatformApplicationMisc::CreateApplication());
    window_ = application_->MakeWindow();
    playerInput_.AddMappingContext(UInputMappingContext::MakeDefault());
}

UGameEngine::~UGameEngine() {
    Shutdown();
}

bool UGameEngine::Initialize(int width, int height, const char* title) {
    if (initialized_) {
        return true;
    }

    if (!window_->Create(width, height, title != nullptr ? title : "Leon Engine")) {
        return false;
    }

    const std::string shaderDir = FPaths::ResolveAssetPath("assets/Shaders");
    if (!renderer_.Initialize(shaderDir)) {
        window_->Destroy();
        return false;
    }
    if (!overlay_.Initialize(shaderDir)) {
        renderer_.Shutdown();
        window_->Destroy();
        return false;
    }

    window_->SetScrollCallback(
        [this](double yOffset) { pendingScrollY_ += static_cast<float>(yOffset); });

    camera_.SetPerspective(60.0f, window_->Aspect(), 0.1f, 100.0f);
    camera_.SetTarget({0.0f, 0.0f, 0.0f});

    SetCursorCaptured(true);

    (void)audioDevice_.Initialize(/*silent=*/false);

    gameInstance_->Init();

    initialized_ = true;
    running_ = true;
    headless_ = false;
    // Stats off until F4 -- matches editor Viewport / PIE (no Engine overlay by default).
    overlay_.SetRightText({});
    overlay_.SetBottomLeftText({});
    return true;
}

bool UGameEngine::InitializeHeadless() {
    if (initialized_) {
        return true;
    }

    // Redirected stdout (smoke / CI) is fully buffered; keep dedicated logs visible on kill.
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    resources_.SetGpuUploadEnabled(false);
    headless_ = true;
    (void)audioDevice_.Initialize(/*silent=*/true);
    gameInstance_->Init();
    initialized_ = true;
    running_ = true;
    std::cout << "Leon Engine headless (no OpenGL / window)\n";
    return true;
}

void UGameEngine::Shutdown() {
    if (!initialized_) {
        return;
    }

    gameInstance_->Shutdown();
    audioDevice_.Shutdown();
    level_.Clear();
    resources_.clear();
    if (!headless_) {
        overlay_.Shutdown();
        renderer_.Shutdown();
        window_->Destroy();
        window_->SetCursorCaptured(false);
    }
    running_ = false;
    initialized_ = false;
    headless_ = false;
    mouseLookSampleValid_ = false;
    suppressCameraDrag_ = false;
    keyboardOrbitEnabled_ = true;
    orbitMouseEnabled_ = true;
    pendingScrollY_ = 0.0f;
    shaderReloadHook_ = {};
    hud_.Clear();
    centerHudText_.clear();
    lastFbWidth_ = 0;
    lastFbHeight_ = 0;
    fpsAccumTime_ = 0.0f;
    fpsAccumFrames_ = 0;
    displayFps_ = 0.0f;
    displayMs_ = 0.0f;
}

float UGameEngine::ConsumeScrollY() {
    const float y = pendingScrollY_;
    pendingScrollY_ = 0.0f;
    return y;
}

void UGameEngine::SetCursorCaptured(bool captured) {
    if (headless_) {
        return;
    }
    GetPlayInputWindow().SetCursorCaptured(captured);
    mouseLookSampleValid_ = false; // skip one frame to avoid a jump after mode change
}

bool UGameEngine::IsCursorCaptured() const {
    if (headless_) {
        return false;
    }
    return GetPlayInputWindow().IsCursorCaptured();
}

void UGameEngine::SetPlayInputWindow(FGenericWindow* window) {
    FGenericWindow* previous = playInputTarget_.GetWindow();
    if (previous != nullptr && previous != window) {
        previous->SetScrollCallback(nullptr);
    }
    playInputTarget_.SetWindow(window);
    if (window != nullptr) {
        // Accumulate into the same pendingScrollY_ as the main window (PIE New Window scroll).
        window->SetScrollCallback(
            [this](double yOffset) { pendingScrollY_ += static_cast<float>(yOffset); });
    }
}

FGenericWindow& UGameEngine::GetPlayInputWindow() {
    return playInputTarget_.Resolve(*window_);
}

const FGenericWindow& UGameEngine::GetPlayInputWindow() const {
    return playInputTarget_.Resolve(*window_);
}

void UGameEngine::AddOnScreenDebugMessage(std::string message, float displaySeconds,
                                     const glm::vec3& color) {
    if (headless_) {
        std::cout << "[server] " << message << '\n';
        (void)displaySeconds;
        (void)color;
        return;
    }
    overlay_.AddOnScreenDebugMessage(std::move(message), displaySeconds, color);
}

EShaderReloadResult UGameEngine::reloadAllShaders(bool force) {
    EShaderReloadResult result = renderer_.ReloadShaders(force);
    result = MergeShaderReload(result, overlay_.ReloadShader(force));
    if (shaderReloadHook_) {
        result = MergeShaderReload(result, shaderReloadHook_(force));
    }
    return result;
}

void UGameEngine::Run(const FUpdateCallback& onUpdate, const FPreInputCallback& onPreInput,
                 const FPostRenderCallback& onPostRender) {
    if (!initialized_) {
        std::cerr << "Engine is not initialized\n";
        return;
    }

    std::cout << "Level static meshes: " << level_.StaticMeshes().size() << '\n';
    std::cout << "Controls: mouse look (cursor captured), scroll zoom (orbit); close window to quit\n";
    std::cout << "Default mode: mouse look, WASD fly along view, Q/E up/down\n";
    std::cout << "Levels: keys 1-9 jump to slot; [ ] previous/next\n";
    std::cout << "Debug: F1 mesh AABBs + light frustum; F2 collision volumes + floor traces\n";
    std::cout << "Debug: F3 NavMesh grid (walkable / blocked)\n";
    std::cout << "Stats: F4 FPS / RAM / TRI overlay (off by default)\n";
    std::cout << "Shaders: F5 force-reload (also auto-reloads when files change)\n";

    auto previous = std::chrono::steady_clock::now();
    while (running_ && !window_->ShouldClose()) {
        const auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - previous).count();
        previous = now;
        deltaTime = std::min(deltaTime, 0.1f);

        window_->PollEvents();
        if (playInputTarget_.HasOverride()) {
            playInputTarget_.GetWindow()->PollEvents();
        }
        playerInput_.Update(GetPlayInputWindow());
        (void)reloadAllShaders(false);
        if (onPreInput) {
            onPreInput();
        }
        handleInput(deltaTime);
        TickPlayAudio();
        if (onUpdate) {
            onUpdate(deltaTime);
        }
        TickPlayHud(deltaTime);
        pendingScrollY_ = 0.0f; // discard unused wheel (modes that do not ConsumeScrollY)
        render(onPostRender);
        window_->SwapBuffers();
    }
}

void UGameEngine::TickPlayAudio() {
    if (!initialized_ || headless_) {
        return;
    }
    const glm::vec3 eye = camera_.GetCameraLocation();
    const glm::vec3 forward = camera_.ForwardVector();
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    audioDevice_.SetListener(eye, forward, up);
    audioDevice_.Tick();
}

void UGameEngine::TickPlayHud(float deltaTime) {
    if (!initialized_) {
        return;
    }
    hud_.Tick(deltaTime);
    overlay_.TickOnScreenMessages(deltaTime);
    if (showHudStats_) {
        updateHudStats(deltaTime);
    }
    overlay_.SetCenterText(centerHudText_);
}

void UGameEngine::PaintHudAndOverlay(int framebufferWidth, int framebufferHeight) {
    if (!initialized_ || headless_) {
        return;
    }
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }
    hud_.Paint(overlay_, framebufferWidth, framebufferHeight);
    overlay_.Draw(framebufferWidth, framebufferHeight);
}

void UGameEngine::RunHeadless(const FUpdateCallback& onUpdate, float tickHz) {
    if (!initialized_ || !headless_) {
        std::cerr << "Engine::RunHeadless requires InitializeHeadless()\n";
        return;
    }
    if (tickHz < 1.0f) {
        tickHz = 1.0f;
    }
    const float dt = 1.0f / tickHz;
    std::cout << "Headless tick " << tickHz << " Hz -- Ctrl+C to stop\n";

    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    while (running_) {
        if (onUpdate) {
            onUpdate(dt);
        }
        next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(dt));
        const auto now = clock::now();
        if (next < now) {
            // Fell behind -- resync to avoid spiral.
            next = now;
        } else {
            std::this_thread::sleep_until(next);
        }
    }
}

void UGameEngine::SetHudStatsVisible(bool visible) {
    showHudStats_ = visible;
    if (!showHudStats_) {
        overlay_.SetRightText({});
        overlay_.SetBottomLeftText({});
        fpsAccumTime_ = 0.0f;
        fpsAccumFrames_ = 0;
    }
}

void UGameEngine::updateHudStats(float deltaTime) {
    fpsAccumTime_ += deltaTime;
    ++fpsAccumFrames_;
    if (fpsAccumTime_ < 0.25f) {
        return;
    }

    displayMs_ = (fpsAccumTime_ / static_cast<float>(fpsAccumFrames_)) * 1000.0f;
    displayFps_ = static_cast<float>(fpsAccumFrames_) / fpsAccumTime_;
    fpsAccumTime_ = 0.0f;
    fpsAccumFrames_ = 0;

    const FFrameStats& stats = renderer_.GetFrameStats();

    int fbWidth = 0;
    int fbHeight = 0;
    window_->GetFramebufferSize(fbWidth, fbHeight);

    const FPlatformMemoryStats memory = FPlatformMemory::GetStats();
    const auto ramMb = static_cast<double>(memory.UsedPhysical) / (1024.0 * 1024.0);
    const FRHIGPUMemoryStats gpu = GDynamicRHI != nullptr ? GDynamicRHI->GetGPUMemoryStats() : FRHIGPUMemoryStats{};

    std::array<char, 32> vram{};
    (void)std::snprintf(vram.data(), vram.size(), "VRAM n/a");
    if (gpu.bValid) {
        const auto budgetMb = static_cast<double>(gpu.BudgetBytes) / (1024.0 * 1024.0);
        if (gpu.bReportsUsage) {
            const auto usedMb = static_cast<double>(gpu.UsedBytes) / (1024.0 * 1024.0);
            (void)std::snprintf(vram.data(), vram.size(), "VRAM %.0f/%.0fM", usedMb, budgetMb);
        } else {
            (void)std::snprintf(vram.data(), vram.size(), "VRAM %.0fM", budgetMb);
        }
    }

    // Compact two-column stats (top-right).
    std::array<char, 256> text{};
    (void)std::snprintf(text.data(), text.size(),
                        "FPS %5.0f   MS %5.2f\n"
                        "RAM %4.0fM  %s\n"
                        "TRIS %5d  OBJ %d/%d\n"
                        "RES %dx%d\n"
                        "GPU Sh %.2f Pl %.2f Col %.2f\n"
                        "    AO %.2f Pst %.2f",
                        displayFps_, displayMs_, ramMb, vram.data(), stats.trianglesSubmitted,
                        stats.objectsVisible, stats.objectsTotal, fbWidth, fbHeight,
                        stats.shadowMs, stats.planarMs, stats.colorMs, stats.ssaoMs, stats.postMs);
    overlay_.SetRightText(text.data());
    overlay_.SetText({});
    overlay_.SetCenterText({});

    std::array<char, 96> hints{};
    (void)std::snprintf(hints.data(), hints.size(), "F1 AABB %s\nF2 Coll+Trace %s\nF3 NavMesh %s",
                        renderer_.IsDebugDrawEnabled() ? "ON" : "OFF",
                        collisionDebugEnabled_ ? "ON" : "OFF",
                        navMeshDebugEnabled_ ? "ON" : "OFF");
    overlay_.SetBottomLeftText(hints.data());
}

void UGameEngine::handleInput(float deltaTime) {
    // PIE "New Window" routes capture + look here; fall back to the main window otherwise.
    FGenericWindow& inputWindow = GetPlayInputWindow();

    const bool f1Down = inputWindow.IsKeyPressed(EKeys::F1);
    if (f1Down && !debugKeyWasDown_) {
        renderer_.ToggleDebugDraw();
        std::cout << "Debug draw (mesh AABB): " << (renderer_.IsDebugDrawEnabled() ? "on" : "off")
                  << '\n';
    }
    debugKeyWasDown_ = f1Down;

    const bool f2Down = inputWindow.IsKeyPressed(EKeys::F2);
    if (f2Down && !collisionDebugKeyWasDown_) {
        ToggleCollisionDebug();
        std::cout << "Collision debug: " << (IsCollisionDebugEnabled() ? "on" : "off") << '\n';
    }
    collisionDebugKeyWasDown_ = f2Down;

    const bool f3Down = inputWindow.IsKeyPressed(EKeys::F3);
    if (f3Down && !navMeshDebugKeyWasDown_) {
        ToggleNavMeshDebug();
        std::cout << "NavMesh debug: " << (IsNavMeshDebugEnabled() ? "on" : "off") << '\n';
    }
    navMeshDebugKeyWasDown_ = f3Down;

    const bool f4Down = inputWindow.IsKeyPressed(EKeys::F4);
    if (f4Down && !hudStatsKeyWasDown_) {
        SetHudStatsVisible(!showHudStats_);
        std::cout << "HUD stats: " << (showHudStats_ ? "on" : "off") << '\n';
    }
    hudStatsKeyWasDown_ = f4Down;

    const bool f5Down = inputWindow.IsKeyPressed(EKeys::F5);
    if (f5Down && !reloadKeyWasDown_) {
        const EShaderReloadResult result = reloadAllShaders(true);
        if (result == EShaderReloadResult::Failed) {
            std::cerr << "Shader reload failed; previous programs kept where possible\n";
        } else if (result == EShaderReloadResult::Reloaded) {
            std::cout << "Shaders reloaded (F5)\n";
        } else {
            std::cout << "Shaders unchanged (F5)\n";
        }
    }
    reloadKeyWasDown_ = f5Down;

    constexpr float kKeyboardOrbitSpeed = 90.0f;
    if (keyboardOrbitEnabled_) {
        // Reuse Move* axes so remapping WASD also remaps keyboard orbit tumble.
        const FMoveAxes2D axes = playerInput_.GetMoveAxes2D();
        const float yaw = axes.x * kKeyboardOrbitSpeed;
        const float pitch = -axes.z * kKeyboardOrbitSpeed;
        if (yaw != 0.0f || pitch != 0.0f) {
            camera_.Orbit(yaw * deltaTime, pitch * deltaTime);
        }
    }

    // Orbit mouse: Engine owns scroll zoom on camera distance (look is continuous below).
    if (orbitMouseEnabled_ && camera_.Mode() == ECameraMode::Orbit) {
        const float scrollY = ConsumeScrollY();
        if (scrollY != 0.0f) {
            camera_.Zoom(scrollY * 0.4f);
        }
    }

    double mouseX = 0.0;
    double mouseY = 0.0;
    inputWindow.GetCursorPos(mouseX, mouseY);

    const bool wantLook =
        !suppressCameraDrag_ &&
        (inputWindow.IsCursorCaptured() || inputWindow.IsMouseButtonDown(EMouseButtons::Left));

    if (wantLook) {
        if (mouseLookSampleValid_) {
            const float dx = static_cast<float>(mouseX - lastMouseX_);
            const float dy = static_cast<float>(mouseY - lastMouseY_);
            if (camera_.Mode() == ECameraMode::FreeLook) {
                constexpr float kLookDegreesPerPixel = 0.15f;
                camera_.AddLook(dx * kLookDegreesPerPixel, -dy * kLookDegreesPerPixel);
            } else if (orbitMouseEnabled_) {
                constexpr float kOrbitDegreesPerPixel = 0.3f;
                camera_.Orbit(dx * kOrbitDegreesPerPixel, dy * kOrbitDegreesPerPixel);
            }
        }
        mouseLookSampleValid_ = true;
        lastMouseX_ = mouseX;
        lastMouseY_ = mouseY;
    } else {
        mouseLookSampleValid_ = false;
        lastMouseX_ = mouseX;
        lastMouseY_ = mouseY;
    }
}

void UGameEngine::render(const FPostRenderCallback& onPostRender) {
    int fbWidth = 0;
    int fbHeight = 0;
    window_->GetFramebufferSize(fbWidth, fbHeight);
    if (fbWidth <= 0 || fbHeight <= 0) {
        return;
    }

    if (fbWidth != lastFbWidth_ || fbHeight != lastFbHeight_) {
        lastFbWidth_ = fbWidth;
        lastFbHeight_ = fbHeight;
        camera_.SetPerspective(camera_.FieldOfView(),
                               static_cast<float>(fbWidth) / static_cast<float>(fbHeight), 0.1f,
                               100.0f);
    }

    renderer_.BeginFrame(fbWidth, fbHeight);
    renderer_.DrawScene(level_, camera_);
    PaintHudAndOverlay(fbWidth, fbHeight);
    if (onPostRender) {
        onPostRender(fbWidth, fbHeight);
    }
}

