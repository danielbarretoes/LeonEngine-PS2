#include <GLFW/glfw3.h>

#include <iostream>
#include "Window.h"
#include "IRHIDevice.h"
#include <stb_image.h>
#include <vector>

namespace {

int g_glfwInitCount = 0;

[[nodiscard]] GLFWwindow* AsGlfw(leon::NativeWindowHandle handle) {
    return static_cast<GLFWwindow*>(handle);
}

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
    backendOwned_ = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 0);

    GLFWwindow* glfw = glfwCreateWindow(width, height, title, nullptr, nullptr);
    handle_ = glfw;
    if (handle_ == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        Destroy();
        return false;
    }

    glfwMakeContextCurrent(glfw);
    glfwSetWindowUserPointer(glfw, this);
    glfwSetWindowSizeCallback(glfw, [](GLFWwindow* w, int ww, int hh) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w))) {
            self->ApplyWindowSize(ww, hh);
        }
    });
    glfwSetFramebufferSizeCallback(glfw, [](GLFWwindow* w, int ww, int hh) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w))) {
            self->ApplyFramebufferSize(ww, hh);
        }
    });
    glfwSetScrollCallback(glfw, [](GLFWwindow* w, double, double yOffset) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w))) {
            self->NotifyScroll(yOffset);
        }
    });
    glfwSwapInterval(1);

    syncSizesFromBackend();

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

bool Window::CreateShared(const Window& shareWith, int width, int height, const char* title) {
    if (handle_ != nullptr) {
        return true;
    }
    GLFWwindow* shareGlfw = AsGlfw(shareWith.NativeHandle());
    if (shareGlfw == nullptr) {
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

    GLFWwindow* glfw =
        glfwCreateWindow(width, height, title != nullptr ? title : "Leon Play", nullptr, shareGlfw);
    handle_ = glfw;
    if (handle_ == nullptr) {
        std::cerr << "Failed to create shared GLFW window\n";
        return false;
    }

    ++g_glfwInitCount;
    backendOwned_ = true;
    rhi_.reset();
    glfwSetWindowUserPointer(glfw, this);
    glfwSetWindowSizeCallback(glfw, [](GLFWwindow* w, int ww, int hh) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w))) {
            self->ApplyWindowSize(ww, hh);
        }
    });
    glfwSetFramebufferSizeCallback(glfw, [](GLFWwindow* w, int ww, int hh) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w))) {
            self->ApplyFramebufferSize(ww, hh);
        }
    });
    glfwSetScrollCallback(glfw, [](GLFWwindow* w, double, double yOffset) {
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w))) {
            self->NotifyScroll(yOffset);
        }
    });
    syncSizesFromBackend();
    MakeContextCurrent();
    glfwSwapInterval(1);
    glfwMakeContextCurrent(shareGlfw);
    Show();
    Focus();
    return true;
}

void Window::MakeContextCurrent() {
    if (GLFWwindow* glfw = AsGlfw(handle_)) {
        glfwMakeContextCurrent(glfw);
    }
}

void Window::Show() {
    if (GLFWwindow* glfw = AsGlfw(handle_)) {
        glfwShowWindow(glfw);
    }
}

void Window::Focus() {
    if (GLFWwindow* glfw = AsGlfw(handle_)) {
        glfwFocusWindow(glfw);
    }
}

bool Window::IsFocused() const {
    GLFWwindow* glfw = AsGlfw(handle_);
    return glfw != nullptr && glfwGetWindowAttrib(glfw, GLFW_FOCUSED) == GLFW_TRUE;
}

void Window::Destroy() {
    if (rhi::GetActiveDevice() == rhi_.get()) {
        rhi::SetActiveDevice(nullptr);
    }
    rhi_.reset();
    if (GLFWwindow* glfw = AsGlfw(handle_)) {
        glfwDestroyWindow(glfw);
        handle_ = nullptr;
    }
    if (backendOwned_) {
        backendOwned_ = false;
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
    GLFWwindow* glfw = AsGlfw(handle_);
    return glfw == nullptr || glfwWindowShouldClose(glfw) == GLFW_TRUE;
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::SwapBuffers() {
    if (GLFWwindow* glfw = AsGlfw(handle_)) {
        glfwSwapBuffers(glfw);
    }
}

void Window::syncSizesFromBackend() {
    GLFWwindow* glfw = AsGlfw(handle_);
    if (glfw == nullptr) {
        return;
    }
    glfwGetWindowSize(glfw, &windowWidth_, &windowHeight_);
    glfwGetFramebufferSize(glfw, &framebufferWidth_, &framebufferHeight_);
}

float Window::Aspect() const {
    if (framebufferWidth_ > 0 && framebufferHeight_ > 0) {
        return static_cast<float>(framebufferWidth_) / static_cast<float>(framebufferHeight_);
    }
    return windowHeight_ > 0 ? static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_)
                             : 1.0f;
}

bool Window::IsKeyPressed(EKey key) const {
    GLFWwindow* glfw = AsGlfw(handle_);
    return glfw != nullptr && glfwGetKey(glfw, ToKeyCode(key)) == GLFW_PRESS;
}

bool Window::IsMouseButtonDown(EMouseButton button) const {
    GLFWwindow* glfw = AsGlfw(handle_);
    return glfw != nullptr &&
           glfwGetMouseButton(glfw, static_cast<int>(button)) == GLFW_PRESS;
}

void Window::GetCursorPos(double& x, double& y) const {
    if (GLFWwindow* glfw = AsGlfw(handle_)) {
        glfwGetCursorPos(glfw, &x, &y);
    } else {
        x = 0.0;
        y = 0.0;
    }
}

void Window::SetCursorCaptured(bool captured) {
    cursorCaptured_ = captured;
    GLFWwindow* glfw = AsGlfw(handle_);
    if (glfw == nullptr) {
        return;
    }
    glfwSetInputMode(glfw, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(glfw, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
}

bool Window::SetIconFromFile(const char* pngPath) {
    GLFWwindow* glfw = AsGlfw(handle_);
    if (glfw == nullptr || pngPath == nullptr || pngPath[0] == '\0') {
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
    glfwSetWindowIcon(glfw, static_cast<int>(images.size()), images.data());
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
