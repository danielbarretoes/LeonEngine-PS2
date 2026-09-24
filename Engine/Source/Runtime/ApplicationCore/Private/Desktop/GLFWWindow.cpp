#include "Desktop/GLFWWindow.h"

#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <iostream>
#include <vector>

namespace
{
	int GGLFWInitCount = 0;

	GLFWwindow* AsGLFW(FNativeWindowHandle Handle)
	{
		return static_cast<GLFWwindow*>(Handle);
	}

	FGLFWWindow* FromGLFW(GLFWwindow* Window)
	{
		return static_cast<FGLFWWindow*>(glfwGetWindowUserPointer(Window));
	}

	void SetContextHints()
	{
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SAMPLES, 0);
	}
}

FGLFWWindow::~FGLFWWindow()
{
	Destroy();
}

void FGLFWWindow::InstallCallbacks()
{
	GLFWwindow* Window = AsGLFW(handle_);
	glfwSetWindowUserPointer(Window, this);
	glfwSetWindowSizeCallback(Window, [](GLFWwindow* W, int Width, int Height) {
		if (FGLFWWindow* Self = FromGLFW(W))
		{
			Self->ApplyWindowSize(Width, Height);
		}
	});
	glfwSetFramebufferSizeCallback(Window, [](GLFWwindow* W, int Width, int Height) {
		if (FGLFWWindow* Self = FromGLFW(W))
		{
			Self->ApplyFramebufferSize(Width, Height);
		}
	});
	glfwSetScrollCallback(Window, [](GLFWwindow* W, double, double YOffset) {
		if (FGLFWWindow* Self = FromGLFW(W))
		{
			Self->NotifyScroll(YOffset);
		}
	});
}

bool FGLFWWindow::Create(int width, int height, const char* title)
{
	if (handle_ != nullptr)
	{
		return true;
	}

	if (GGLFWInitCount == 0 && glfwInit() != GLFW_TRUE)
	{
		std::cerr << "Failed to initialize GLFW\n";
		return false;
	}
	++GGLFWInitCount;
	backendOwned_ = true;

	SetContextHints();
	GLFWwindow* Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	handle_ = Window;
	if (handle_ == nullptr)
	{
		std::cerr << "Failed to create GLFW window\n";
		Destroy();
		return false;
	}

	glfwMakeContextCurrent(Window);
	InstallCallbacks();
	glfwSwapInterval(1);
	SyncSizesFromBackend();

	if (!InitRHI(reinterpret_cast<void* (*)(const char*)>(glfwGetProcAddress)))
	{
		Destroy();
		return false;
	}
	return true;
}

bool FGLFWWindow::CreateShared(const FGenericWindow& shareWith, int width, int height, const char* title)
{
	if (handle_ != nullptr)
	{
		return true;
	}
	GLFWwindow* ShareWindow = AsGLFW(shareWith.NativeHandle());
	if (ShareWindow == nullptr || GGLFWInitCount == 0)
	{
		std::cerr << "FGLFWWindow::CreateShared requires an initialised share context\n";
		return false;
	}

	SetContextHints();
	glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
	GLFWwindow* Window =
		glfwCreateWindow(width, height, title != nullptr ? title : "Leon Play", nullptr, ShareWindow);
	handle_ = Window;
	if (handle_ == nullptr)
	{
		std::cerr << "Failed to create shared GLFW window\n";
		return false;
	}

	++GGLFWInitCount;
	backendOwned_ = true;
	InstallCallbacks();
	SyncSizesFromBackend();
	MakeContextCurrent();
	glfwSwapInterval(1);
	glfwMakeContextCurrent(ShareWindow);
	Show();
	Focus();
	return true;
}

void FGLFWWindow::Destroy()
{
	ReleaseRHI();
	if (GLFWwindow* Window = AsGLFW(handle_))
	{
		glfwDestroyWindow(Window);
		handle_ = nullptr;
	}
	if (backendOwned_)
	{
		backendOwned_ = false;
		if (GGLFWInitCount > 0)
		{
			--GGLFWInitCount;
		}
		if (GGLFWInitCount == 0)
		{
			glfwTerminate();
		}
	}
	ResetWindowState();
}

void FGLFWWindow::MakeContextCurrent()
{
	if (GLFWwindow* Window = AsGLFW(handle_))
	{
		glfwMakeContextCurrent(Window);
	}
}

void FGLFWWindow::Show()
{
	if (GLFWwindow* Window = AsGLFW(handle_))
	{
		glfwShowWindow(Window);
	}
}

void FGLFWWindow::Focus()
{
	if (GLFWwindow* Window = AsGLFW(handle_))
	{
		glfwFocusWindow(Window);
	}
}

bool FGLFWWindow::IsFocused() const
{
	GLFWwindow* Window = AsGLFW(handle_);
	return Window != nullptr && glfwGetWindowAttrib(Window, GLFW_FOCUSED) == GLFW_TRUE;
}

bool FGLFWWindow::ShouldClose() const
{
	GLFWwindow* Window = AsGLFW(handle_);
	return Window == nullptr || glfwWindowShouldClose(Window) == GLFW_TRUE;
}

void FGLFWWindow::PollEvents()
{
	glfwPollEvents();
}

void FGLFWWindow::SwapBuffers()
{
	if (GLFWwindow* Window = AsGLFW(handle_))
	{
		glfwSwapBuffers(Window);
	}
}

void FGLFWWindow::SyncSizesFromBackend()
{
	GLFWwindow* Window = AsGLFW(handle_);
	if (Window == nullptr)
	{
		return;
	}
	glfwGetWindowSize(Window, &windowWidth_, &windowHeight_);
	glfwGetFramebufferSize(Window, &framebufferWidth_, &framebufferHeight_);
}

bool FGLFWWindow::IsKeyPressed(EKeys key) const
{
	GLFWwindow* Window = AsGLFW(handle_);
	return Window != nullptr && !IsGamepadKey(key) && glfwGetKey(Window, ToKeyCode(key)) == GLFW_PRESS;
}

bool FGLFWWindow::IsMouseButtonDown(EMouseButtons button) const
{
	GLFWwindow* Window = AsGLFW(handle_);
	return Window != nullptr && button != EMouseButtons::Invalid &&
		glfwGetMouseButton(Window, static_cast<int>(button)) == GLFW_PRESS;
}

void FGLFWWindow::GetCursorPos(double& x, double& y) const
{
	if (GLFWwindow* Window = AsGLFW(handle_))
	{
		glfwGetCursorPos(Window, &x, &y);
	}
	else
	{
		x = 0.0;
		y = 0.0;
	}
}

void FGLFWWindow::SetCursorCaptured(bool captured)
{
	cursorCaptured_ = captured;
	GLFWwindow* Window = AsGLFW(handle_);
	if (Window == nullptr)
	{
		return;
	}
	glfwSetInputMode(Window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported() == GLFW_TRUE)
	{
		glfwSetInputMode(Window, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
	}
}

bool FGLFWWindow::SetIconFromFile(const char* pngPath)
{
	GLFWwindow* Window = AsGLFW(handle_);
	if (Window == nullptr || pngPath == nullptr || pngPath[0] == '\0')
	{
		return false;
	}

	int Width = 0;
	int Height = 0;
	int Channels = 0;
	unsigned char* Pixels = stbi_load(pngPath, &Width, &Height, &Channels, 4);
	if (Pixels == nullptr || Width <= 0 || Height <= 0)
	{
		if (Pixels != nullptr)
		{
			stbi_image_free(Pixels);
		}
		return false;
	}

	struct FIconLevel
	{
		int Size = 0;
		std::vector<unsigned char> Pixels;
	};
	const int Sizes[] = {16, 32, 48, Width};
	std::vector<FIconLevel> Levels;
	Levels.reserve(4);
	for (int Size : Sizes)
	{
		if (Size <= 0 || Size > Width || Size > Height)
		{
			continue;
		}
		bool bDuplicate = false;
		for (const FIconLevel& Existing : Levels)
		{
			bDuplicate = bDuplicate || Existing.Size == Size;
		}
		if (bDuplicate)
		{
			continue;
		}

		FIconLevel Level;
		Level.Size = Size;
		Level.Pixels.resize(static_cast<size_t>(Size) * static_cast<size_t>(Size) * 4u);
		for (int Y = 0; Y < Size; ++Y)
		{
			for (int X = 0; X < Size; ++X)
			{
				const size_t Dst = (static_cast<size_t>(Y) * static_cast<size_t>(Size) + static_cast<size_t>(X)) * 4u;
				const size_t Src = (static_cast<size_t>(Y * Height / Size) * static_cast<size_t>(Width) +
									   static_cast<size_t>(X * Width / Size)) *
					4u;
				for (size_t Channel = 0; Channel < 4u; ++Channel)
				{
					Level.Pixels[Dst + Channel] = Pixels[Src + Channel];
				}
			}
		}
		Levels.push_back(std::move(Level));
	}
	stbi_image_free(Pixels);

	if (Levels.empty())
	{
		return false;
	}

	std::vector<GLFWimage> Images(Levels.size());
	for (size_t Index = 0; Index < Levels.size(); ++Index)
	{
		Images[Index].width = Levels[Index].Size;
		Images[Index].height = Levels[Index].Size;
		Images[Index].pixels = Levels[Index].Pixels.data();
	}
	glfwSetWindowIcon(Window, static_cast<int>(Images.size()), Images.data());
	return true;
}
