#pragma once

#include <functional>
#include <leon/rhi/IRHIDevice.h>
#include <memory>

struct GLFWwindow;

namespace leon {

/// OS window + graphics context (GLFW). GL loading goes through IRHIDevice.
class Window {
public:
    using ScrollCallback = std::function<void(double yOffset)>;

    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(int width, int height, const char* title);
    /// Secondary OS window sharing the OpenGL context of `shareWith` (no second RHI).
    bool CreateShared(GLFWwindow* shareWith, int width, int height, const char* title);
    void Destroy();

    /// Make this window's GL context current (needed when rendering to a shared play window).
    void MakeContextCurrent();
    /// Show + raise + focus (needed so GLFW_CURSOR_DISABLED / keyboard work on PIE New Window).
    void Show();
    void Focus();
    [[nodiscard]] bool IsFocused() const;

    [[nodiscard]] bool ShouldClose() const;
    void PollEvents();
    void SwapBuffers();

    [[nodiscard]] GLFWwindow* Handle() const { return handle_; }
    [[nodiscard]] rhi::IRHIDevice* RHIDevice() const { return rhi_.get(); }

    [[nodiscard]] int Width() const { return windowWidth_; }
    [[nodiscard]] int Height() const { return windowHeight_; }
    void GetWindowSize(int& width, int& height) const;

    [[nodiscard]] int FramebufferWidth() const { return framebufferWidth_; }
    [[nodiscard]] int FramebufferHeight() const { return framebufferHeight_; }
    void GetFramebufferSize(int& width, int& height) const;

    [[nodiscard]] float Aspect() const;

    [[nodiscard]] bool IsKeyPressed(int key) const;
    [[nodiscard]] bool IsMouseButtonDown(int button) const;
    void GetCursorPos(double& x, double& y) const;

    void SetCursorCaptured(bool captured);
    [[nodiscard]] bool IsCursorCaptured() const { return cursorCaptured_; }

    /// Set the OS window / taskbar icon from a PNG (RGBA). No-op if the file cannot be loaded.
    bool SetIconFromFile(const char* pngPath);

    void SetScrollCallback(ScrollCallback callback);

private:
    void syncSizesFromGlfw();

    static void windowSizeCallback(GLFWwindow* window, int width, int height);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

    GLFWwindow* handle_ = nullptr;
    std::unique_ptr<rhi::IRHIDevice> rhi_;
    int windowWidth_ = 0;
    int windowHeight_ = 0;
    int framebufferWidth_ = 0;
    int framebufferHeight_ = 0;
    /// True when this window holds a process-wide GLFW init ref (primary and shared windows).
    bool glfwOwned_ = false;
    bool cursorCaptured_ = false;
    ScrollCallback scrollCallback_;
};

} // namespace leon
