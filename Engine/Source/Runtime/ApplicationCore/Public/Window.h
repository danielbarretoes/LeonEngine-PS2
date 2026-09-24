#pragma once

#include <functional>
#include "EKey.h"
#include "IRHIDevice.h"
#include <memory>

namespace leon {

/// Opaque OS / graphics window handle (GLFW on Host, GS context on PS2).
using NativeWindowHandle = void*;

/// OS window + graphics context. GPU entry points go through IRHIDevice.
class Window {
public:
    using ScrollCallback = std::function<void(double yOffset)>;

    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(int width, int height, const char* title);
    /// Secondary Host window sharing the GL context of `shareWith` (Editor PIE). No-op on PS2.
    bool CreateShared(const Window& shareWith, int width, int height, const char* title);
    void Destroy();

    void MakeContextCurrent();
    void Show();
    void Focus();
    [[nodiscard]] bool IsFocused() const;

    [[nodiscard]] bool ShouldClose() const;
    void PollEvents();
    void SwapBuffers();

    [[nodiscard]] NativeWindowHandle NativeHandle() const { return handle_; }
    [[nodiscard]] rhi::IRHIDevice* RHIDevice() const { return rhi_.get(); }

    [[nodiscard]] int Width() const { return windowWidth_; }
    [[nodiscard]] int Height() const { return windowHeight_; }
    void GetWindowSize(int& width, int& height) const;

    [[nodiscard]] int FramebufferWidth() const { return framebufferWidth_; }
    [[nodiscard]] int FramebufferHeight() const { return framebufferHeight_; }
    void GetFramebufferSize(int& width, int& height) const;

    [[nodiscard]] float Aspect() const;

    [[nodiscard]] bool IsKeyPressed(EKey key) const;
    [[nodiscard]] bool IsMouseButtonDown(EMouseButton button) const;
    void GetCursorPos(double& x, double& y) const;

    void SetCursorCaptured(bool captured);
    [[nodiscard]] bool IsCursorCaptured() const { return cursorCaptured_; }

    bool SetIconFromFile(const char* pngPath);

    void SetScrollCallback(ScrollCallback callback);

    /// Backend callbacks (Host GLFW / PS2 pad) update sizes through these.
    void ApplyWindowSize(int width, int height);
    void ApplyFramebufferSize(int width, int height);
    void NotifyScroll(double yOffset);

private:
    void syncSizesFromBackend();

    NativeWindowHandle handle_ = nullptr;
    std::unique_ptr<rhi::IRHIDevice> rhi_;
    int windowWidth_ = 0;
    int windowHeight_ = 0;
    int framebufferWidth_ = 0;
    int framebufferHeight_ = 0;
    bool backendOwned_ = false;
    bool cursorCaptured_ = false;
    bool shouldClose_ = false;
    ScrollCallback scrollCallback_;
};

} // namespace leon
