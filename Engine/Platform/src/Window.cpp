#include <GLFW/glfw3.h>

#include <iostream>
#include <leon/core/Window.h>
#include <leon/rhi/IRHIDevice.h>
#include <stb_image.h>
#include <vector>

namespace {

/// Process-wide GLFW init refcount — only the last owner calls glfwTerminate.
int g_glfwInitCount = 0;

} // namespace

namespace leon {

Window::~Window() {
    Destroy();
}

bool Window::Create(int width, int height, const char* title) {
    if (handle_ != nullptr) {
        return true;
    }

    if (g_glfwInitCount == 0) {
        if (glfwInit() != GLFW_TRUE) {
            std::cerr << "Failed to initialize GLFW\n";
            return false;
        }
    }
    ++g_glfwInitCount;
    glfwOwned_ = true; // this window holds a GLFW init ref

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // Match editor Viewport / PIE FBO (RGB8, no MSAA) so Shipping looks the same.
    glfwWindowHint(GLFW_SAMPLES, 0);

    handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (handle_ == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        Destroy();
        return false;
    }

    glfwMakeContextCurrent(handle_);
    glfwSetWindowUserPointer(handle_, this);
    glfwSetWindowSizeCallback(handle_, windowSizeCallback);
    glfwSetFramebufferSizeCallback(handle_, framebufferSizeCallback);
    glfwSetScrollCallback(handle_, scrollCallback);
    glfwSwapInterval(1);

    syncSizesFromGlfw();

    rhi_ = rhi::CreateDefaultRHIDevice();
    if (!rhi_ ||
        !rhi_->LoadProcedures(reinterpret_cast<void* (*)(const char*)>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize RHI\n";
        Destroy();
        return false;
    }
    rhi::SetActiveDevice(rhi_.get());

    if (framebufferWidth_ > 0 && framebufferHeight_ > 0) {
        rhi_->SetViewport(0, 0, framebufferWidth_, framebufferHeight_);
    }
    return true;
}

bool Window::CreateShared(GLFWwindow* shareWith, int width, int height, const char* title) {
    if (handle_ != nullptr) {
        return true;
    }
    if (shareWith == nullptr) {
        std::cerr << "Window::CreateShared requires a share context\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    if (g_glfwInitCount == 0) {
        std::cerr << "Window::CreateShared requires GLFW already initialized\n";
        return false;
    }

    handle_ = glfwCreateWindow(width, height, title != nullptr ? title : "Leon Play", nullptr,
                               shareWith);
    if (handle_ == nullptr) {
        std::cerr << "Failed to create shared GLFW window\n";
        return false;
    }

    // Hold a GLFW init ref so destroying the primary window cannot glfwTerminate early.
    ++g_glfwInitCount;
    glfwOwned_ = true;
    rhi_.reset();
    glfwSetWindowUserPointer(handle_, this);
    glfwSetWindowSizeCallback(handle_, windowSizeCallback);
    glfwSetFramebufferSizeCallback(handle_, framebufferSizeCallback);
    glfwSetScrollCallback(handle_, scrollCallback);
    syncSizesFromGlfw();
    // Parity with primary window (CreateShared does not go through Create's SwapInterval).
    // Restore the caller context afterward — editor keeps rendering on `shareWith`.
    MakeContextCurrent();
    glfwSwapInterval(1);
    glfwMakeContextCurrent(shareWith);
    Show();
    Focus();
    return true;
}

void Window::MakeContextCurrent() {
    if (handle_ != nullptr) {
        glfwMakeContextCurrent(handle_);
    }
}

void Window::Show() {
    if (handle_ != nullptr) {
        glfwShowWindow(handle_);
    }
}

void Window::Focus() {
    if (handle_ != nullptr) {
        glfwFocusWindow(handle_);
    }
}

bool Window::IsFocused() const {
    return handle_ != nullptr && glfwGetWindowAttrib(handle_, GLFW_FOCUSED) == GLFW_TRUE;
}

void Window::Destroy() {
    if (rhi::GetActiveDevice() == rhi_.get()) {
        rhi::SetActiveDevice(nullptr);
    }
    rhi_.reset();
    if (handle_ != nullptr) {
        glfwDestroyWindow(handle_);
        handle_ = nullptr;
    }
    // Release this window's GLFW init ref (primary and shared both hold one).
    if (glfwOwned_) {
        glfwOwned_ = false;
        if (g_glfwInitCount > 0) {
            --g_glfwInitCount;
        }
        if (g_glfwInitCount == 0) {
            glfwTerminate();
        }
    }
    windowWidth_ = 0;
    windowHeight_ = 0;
    framebufferWidth_ = 0;
    framebufferHeight_ = 0;
    cursorCaptured_ = false;
    scrollCallback_ = nullptr;
}

bool Window::ShouldClose() const {
    return handle_ == nullptr || glfwWindowShouldClose(handle_) == GLFW_TRUE;
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::SwapBuffers() {
    if (handle_ != nullptr) {
        glfwSwapBuffers(handle_);
    }
}

void Window::syncSizesFromGlfw() {
    if (handle_ == nullptr) {
        return;
    }
    glfwGetWindowSize(handle_, &windowWidth_, &windowHeight_);
    glfwGetFramebufferSize(handle_, &framebufferWidth_, &framebufferHeight_);
}

float Window::Aspect() const {
    if (framebufferWidth_ > 0 && framebufferHeight_ > 0) {
        return static_cast<float>(framebufferWidth_) / static_cast<float>(framebufferHeight_);
    }
    return windowHeight_ > 0 ? static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_)
                             : 1.0f;
}

bool Window::IsKeyPressed(int key) const {
    return handle_ != nullptr && glfwGetKey(handle_, key) == GLFW_PRESS;
}

bool Window::IsMouseButtonDown(int button) const {
    return handle_ != nullptr && glfwGetMouseButton(handle_, button) == GLFW_PRESS;
}

void Window::GetCursorPos(double& x, double& y) const {
    if (handle_ != nullptr) {
        glfwGetCursorPos(handle_, &x, &y);
    } else {
        x = 0.0;
        y = 0.0;
    }
}

void Window::SetCursorCaptured(bool captured) {
    cursorCaptured_ = captured;
    if (handle_ == nullptr) {
        return;
    }
    // DISABLED hides the cursor and provides unbounded relative motion (no screen-edge clamp).
    glfwSetInputMode(handle_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(handle_, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
}

bool Window::SetIconFromFile(const char* pngPath) {
    if (handle_ == nullptr || pngPath == nullptr || pngPath[0] == '\0') {
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(pngPath, &width, &height, &channels, 4);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return false;
    }

    // Provide a few mip sizes so title-bar and taskbar icons stay sharp.
    struct IconLevel {
        int size = 0;
        std::vector<unsigned char> pixels;
    };
    const int sizes[] = {16, 32, 48, width};
    std::vector<IconLevel> levels;
    levels.reserve(4);
    for (int size : sizes) {
        if (size <= 0 || size > width || size > height) {
            continue;
        }
        bool duplicate = false;
        for (const IconLevel& existing : levels) {
            if (existing.size == size) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        IconLevel level;
        level.size = size;
        level.pixels.resize(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4u);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const int srcX = x * width / size;
                const int srcY = y * height / size;
                const std::size_t dst = (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) +
                                         static_cast<std::size_t>(x)) *
                                        4u;
                const std::size_t src = (static_cast<std::size_t>(srcY) * static_cast<std::size_t>(width) +
                                         static_cast<std::size_t>(srcX)) *
                                        4u;
                level.pixels[dst + 0] = pixels[src + 0];
                level.pixels[dst + 1] = pixels[src + 1];
                level.pixels[dst + 2] = pixels[src + 2];
                level.pixels[dst + 3] = pixels[src + 3];
            }
        }
        levels.push_back(std::move(level));
    }
    stbi_image_free(pixels);

    if (levels.empty()) {
        return false;
    }

    std::vector<GLFWimage> images(levels.size());
    for (std::size_t i = 0; i < levels.size(); ++i) {
        images[i].width = levels[i].size;
        images[i].height = levels[i].size;
        images[i].pixels = levels[i].pixels.data();
    }
    glfwSetWindowIcon(handle_, static_cast<int>(images.size()), images.data());
    return true;
}

void Window::GetWindowSize(int& width, int& height) const {
    width = windowWidth_;
    height = windowHeight_;
}

void Window::GetFramebufferSize(int& width, int& height) const {
    width = framebufferWidth_;
    height = framebufferHeight_;
}

void Window::SetScrollCallback(ScrollCallback callback) {
    scrollCallback_ = std::move(callback);
}

void Window::windowSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr) {
        return;
    }
    self->windowWidth_ = width;
    self->windowHeight_ = height;
}

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr) {
        return;
    }
    self->framebufferWidth_ = width;
    self->framebufferHeight_ = height;
    if (self->rhi_) {
        self->rhi_->SetViewport(0, 0, width, height);
    }
}

void Window::scrollCallback(GLFWwindow* window, double /*xOffset*/, double yOffset) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self != nullptr && static_cast<bool>(self->scrollCallback_)) {
        self->scrollCallback_(yOffset);
    }
}

} // namespace leon
