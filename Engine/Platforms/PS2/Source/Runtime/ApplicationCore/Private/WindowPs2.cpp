#include "Stats/DebugOverlay.h"
#include "InputPad.h"
#include "Window.h"
#include "IRHIDevice.h"
#include "Ps2RHI.h"

#include <cstdio>

#if defined(LEON_PLATFORM_PS2)
#include <graph.h>
#endif

namespace leon {

namespace {
struct Ps2WindowState {
    bool initialized = false;
};
Ps2WindowState g_ps2Window{};
} // namespace

Window::~Window() {
    Destroy();
}

bool Window::Create(int width, int height, const char* title) {
    if (handle_ != nullptr) {
        return true;
    }
    (void)title;
#if defined(LEON_PLATFORM_PS2)
    const int w = width > 0 ? width : 640;
    const int h = height > 0 ? height : 448;

    if (!rhi::Ps2InitDisplay(w, h)) {
        std::printf("Ps2 Window: Ps2InitDisplay failed\n");
        return false;
    }

    g_ps2Window.initialized = true;
    handle_ = &g_ps2Window;
    backendOwned_ = true;
    windowWidth_ = w;
    windowHeight_ = h;
    framebufferWidth_ = w;
    framebufferHeight_ = h;

    rhi_ = rhi::CreateDefaultRHIDevice();
    if (!rhi_ || !rhi_->LoadProcedures(nullptr)) {
        std::printf("Failed to initialize PS2 RHI\n");
        Destroy();
        return false;
    }
    rhi::SetActiveDevice(rhi_.get());
    rhi_->SetViewport(0, 0, framebufferWidth_, framebufferHeight_);
    return true;
#else
    (void)width;
    (void)height;
    return false;
#endif
}

bool Window::CreateShared(const Window& /*shareWith*/, int /*width*/, int /*height*/,
                          const char* /*title*/) {
    return false;
}

void Window::MakeContextCurrent() {}
void Window::Show() {}
void Window::Focus() {}
bool Window::IsFocused() const { return handle_ != nullptr; }

void Window::Destroy() {
    if (rhi::GetActiveDevice() == rhi_.get()) {
        rhi::SetActiveDevice(nullptr);
    }
    rhi_.reset();
#if defined(LEON_PLATFORM_PS2)
    if (g_ps2Window.initialized) {
        graph_shutdown();
        g_ps2Window.initialized = false;
    }
#endif
    handle_ = nullptr;
    backendOwned_ = false;
    windowWidth_ = 0;
    windowHeight_ = 0;
    framebufferWidth_ = 0;
    framebufferHeight_ = 0;
    cursorCaptured_ = false;
    scrollCallback_ = nullptr;
    shouldClose_ = false;
}

bool Window::ShouldClose() const { return shouldClose_ || handle_ == nullptr; }

void Window::PollEvents() {
#if defined(LEON_PLATFORM_PS2)
    // Fresh pad sample every frame (queries otherwise reuse the last cached read).
    PollPad();
#endif
}

void Window::SwapBuffers() {
#if defined(LEON_PLATFORM_PS2)
    if (handle_ != nullptr) {
        DrawEngineDebugOverlay(framebufferWidth_, framebufferHeight_);
        rhi::Ps2WaitVsync();
        MarkEngineFrameStart();
    }
#endif
}

void Window::syncSizesFromBackend() {}

float Window::Aspect() const {
    return windowHeight_ > 0 ? static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_)
                             : 1.0f;
}

bool Window::IsKeyPressed(EKey /*key*/) const { return false; }

bool Window::IsMouseButtonDown(EMouseButton /*button*/) const { return false; }

void Window::GetCursorPos(double& x, double& y) const {
    x = 0.0;
    y = 0.0;
}

void Window::SetCursorCaptured(bool captured) { cursorCaptured_ = captured; }

bool Window::SetIconFromFile(const char* /*pngPath*/) { return false; }

void Window::GetWindowSize(int& width, int& height) const {
    width = windowWidth_;
    height = windowHeight_;
}

void Window::GetFramebufferSize(int& width, int& height) const {
    width = framebufferWidth_;
    height = framebufferHeight_;
}

void Window::SetScrollCallback(ScrollCallback callback) { scrollCallback_ = std::move(callback); }

void Window::ApplyWindowSize(int width, int height) {
    windowWidth_ = width;
    windowHeight_ = height;
}

void Window::ApplyFramebufferSize(int width, int height) {
    framebufferWidth_ = width;
    framebufferHeight_ = height;
    if (rhi_) {
        rhi_->SetViewport(0, 0, width, height);
    }
}

void Window::NotifyScroll(double yOffset) {
    if (scrollCallback_) {
        scrollCallback_(yOffset);
    }
}

} // namespace leon
