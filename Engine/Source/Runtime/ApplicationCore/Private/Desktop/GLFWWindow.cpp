#include "Desktop/GLFWWindow.h"

#include "Containers/Array.h"
#include "GenericPlatform/GenericApplication.h"

#include <GLFW/glfw3.h>
#include <stb_image.h>

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
} // namespace

FGLFWWindow::~FGLFWWindow()
{
	Destroy();
}

void FGLFWWindow::InstallCallbacks()
{
	GLFWwindow* Window = AsGLFW(Handle);
	glfwSetWindowUserPointer(Window, this);
	glfwSetWindowSizeCallback(Window,
		[](GLFWwindow* W, int Width, int Height)
		{
			if (FGLFWWindow* Self = FromGLFW(W))
			{
				Self->ApplyWindowSize(Width, Height);
			}
		});
	glfwSetFramebufferSizeCallback(Window,
		[](GLFWwindow* W, int Width, int Height)
		{
			if (FGLFWWindow* Self = FromGLFW(W))
			{
				Self->ApplyFramebufferSize(Width, Height);
			}
		});
	glfwSetScrollCallback(Window,
		[](GLFWwindow* W, double, double YOffset)
		{
			if (FGLFWWindow* Self = FromGLFW(W))
			{
				Self->NotifyMouseWheel(static_cast<float>(YOffset));
			}
		});
}

bool FGLFWWindow::Create(int32 InWidth, int32 InHeight, const TCHAR* Title)
{
	if (Handle != nullptr)
	{
		return true;
	}

	if (GGLFWInitCount == 0 && glfwInit() != GLFW_TRUE)
	{
		UE_LOG(LogApplicationCore, Error, "Failed to initialize GLFW");
		return false;
	}
	++GGLFWInitCount;
	bBackendOwned = true;

	SetContextHints();
	GLFWwindow* Window = glfwCreateWindow(InWidth, InHeight, Title, nullptr, nullptr);
	Handle = Window;
	if (Handle == nullptr)
	{
		UE_LOG(LogApplicationCore, Error, "Failed to create GLFW window");
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

bool FGLFWWindow::CreateShared(const FGenericWindow& ShareWith, int32 InWidth, int32 InHeight, const TCHAR* Title)
{
	if (Handle != nullptr)
	{
		return true;
	}
	GLFWwindow* ShareWindow = AsGLFW(ShareWith.NativeHandle());
	if (ShareWindow == nullptr || GGLFWInitCount == 0)
	{
		UE_LOG(LogApplicationCore, Error, "FGLFWWindow::CreateShared requires an initialised share context");
		return false;
	}

	SetContextHints();
	glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
	GLFWwindow* Window =
		glfwCreateWindow(InWidth, InHeight, Title != nullptr ? Title : "Leon Play", nullptr, ShareWindow);
	Handle = Window;
	if (Handle == nullptr)
	{
		UE_LOG(LogApplicationCore, Error, "Failed to create shared GLFW window");
		return false;
	}

	++GGLFWInitCount;
	bBackendOwned = true;
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
	if (GLFWwindow* Window = AsGLFW(Handle))
	{
		glfwDestroyWindow(Window);
		Handle = nullptr;
	}
	if (bBackendOwned)
	{
		bBackendOwned = false;
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
	if (GLFWwindow* Window = AsGLFW(Handle))
	{
		glfwMakeContextCurrent(Window);
	}
}

void FGLFWWindow::Show()
{
	if (GLFWwindow* Window = AsGLFW(Handle))
	{
		glfwShowWindow(Window);
	}
}

void FGLFWWindow::Focus()
{
	if (GLFWwindow* Window = AsGLFW(Handle))
	{
		glfwFocusWindow(Window);
	}
}

bool FGLFWWindow::IsFocused() const
{
	GLFWwindow* Window = AsGLFW(Handle);
	return Window != nullptr && glfwGetWindowAttrib(Window, GLFW_FOCUSED) == GLFW_TRUE;
}

bool FGLFWWindow::ShouldClose() const
{
	GLFWwindow* Window = AsGLFW(Handle);
	return Window == nullptr || glfwWindowShouldClose(Window) == GLFW_TRUE;
}

void FGLFWWindow::PollEvents()
{
	glfwPollEvents();
}

void FGLFWWindow::SwapBuffers()
{
	if (GLFWwindow* Window = AsGLFW(Handle))
	{
		glfwSwapBuffers(Window);
	}
}

void FGLFWWindow::SyncSizesFromBackend()
{
	GLFWwindow* Window = AsGLFW(Handle);
	if (Window == nullptr)
	{
		return;
	}
	glfwGetWindowSize(Window, &WindowWidth, &WindowHeight);
	glfwGetFramebufferSize(Window, &FramebufferWidth, &FramebufferHeight);
}

bool FGLFWWindow::IsKeyPressed(EKeys Key) const
{
	GLFWwindow* Window = AsGLFW(Handle);
	return Window != nullptr && !IsGamepadKey(Key) && glfwGetKey(Window, ToKeyCode(Key)) == GLFW_PRESS;
}

bool FGLFWWindow::IsMouseButtonDown(EMouseButtons Button) const
{
	GLFWwindow* Window = AsGLFW(Handle);
	return Window != nullptr && Button != EMouseButtons::Invalid &&
		glfwGetMouseButton(Window, static_cast<int32>(Button)) == GLFW_PRESS;
}

FVector2D FGLFWWindow::GetCursorPos() const
{
	double X = 0.0;
	double Y = 0.0;
	if (GLFWwindow* Window = AsGLFW(Handle))
	{
		glfwGetCursorPos(Window, &X, &Y);
	}
	return FVector2D(static_cast<float>(X), static_cast<float>(Y));
}

void FGLFWWindow::SetCursorCaptured(bool bCaptured)
{
	bCursorCaptured = bCaptured;
	GLFWwindow* Window = AsGLFW(Handle);
	if (Window == nullptr)
	{
		return;
	}
	glfwSetInputMode(Window, GLFW_CURSOR, bCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported() == GLFW_TRUE)
	{
		glfwSetInputMode(Window, GLFW_RAW_MOUSE_MOTION, bCaptured ? GLFW_TRUE : GLFW_FALSE);
	}
}

bool FGLFWWindow::SetIconFromFile(const TCHAR* PngPath)
{
	GLFWwindow* Window = AsGLFW(Handle);
	if (Window == nullptr || PngPath == nullptr || PngPath[0] == '\0')
	{
		return false;
	}

	int32 Width = 0;
	int32 Height = 0;
	int32 Channels = 0;
	uint8* Pixels = stbi_load(PngPath, &Width, &Height, &Channels, 4);
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
		int32 Size = 0;
		TArray<uint8> Pixels;
	};
	const int32 Sizes[] = {16, 32, 48, Width};
	TArray<FIconLevel> Levels;
	Levels.Reserve(4);
	for (int32 Size : Sizes)
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

		FIconLevel IconLevel;
		IconLevel.Size = Size;
		IconLevel.Pixels.SetNumUninitialized(Size * Size * 4);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const int32 Dst = (Y * Size + X) * 4;
				const int32 Src = ((Y * Height / Size) * Width + X * Width / Size) * 4;
				for (int32 Channel = 0; Channel < 4; ++Channel)
				{
					IconLevel.Pixels[Dst + Channel] = Pixels[Src + Channel];
				}
			}
		}
		Levels.Add(MoveTemp(IconLevel));
	}
	stbi_image_free(Pixels);

	if (Levels.Num() == 0)
	{
		return false;
	}

	TArray<GLFWimage> Images;
	Images.SetNumZeroed(Levels.Num());
	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		Images[Index].width = Levels[Index].Size;
		Images[Index].height = Levels[Index].Size;
		Images[Index].pixels = Levels[Index].Pixels.GetData();
	}
	glfwSetWindowIcon(Window, Images.Num(), Images.GetData());
	return true;
}
