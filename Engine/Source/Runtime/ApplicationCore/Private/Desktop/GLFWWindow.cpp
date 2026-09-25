#include "Desktop/GLFWWindow.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
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

	/** A key's GLFW code: a keyboard key, or a mouse button (UE: the platform key map, FPlatformInput::GetKeyMap). */
	struct FGLFWKeyCode
	{
		int32 Code = GLFW_KEY_UNKNOWN;
		bool bMouseButton = false;
	};

	const TMap<FName, FGLFWKeyCode>& GetGLFWKeyMap()
	{
		static const TMap<FName, FGLFWKeyCode> KeyMap = []()
		{
			TMap<FName, FGLFWKeyCode> Map;
			const auto Key = [&Map](const FKey& InKey, int32 Code)
			{ Map.Add(InKey.GetFName(), FGLFWKeyCode{Code, false}); };
			const auto Button = [&Map](const FKey& InKey, int32 Code)
			{ Map.Add(InKey.GetFName(), FGLFWKeyCode{Code, true}); };
			Button(EKeys::LeftMouseButton, GLFW_MOUSE_BUTTON_LEFT);
			Button(EKeys::RightMouseButton, GLFW_MOUSE_BUTTON_RIGHT);
			Button(EKeys::MiddleMouseButton, GLFW_MOUSE_BUTTON_MIDDLE);
			Button(EKeys::ThumbMouseButton, GLFW_MOUSE_BUTTON_4);
			Button(EKeys::ThumbMouseButton2, GLFW_MOUSE_BUTTON_5);
			Key(EKeys::SpaceBar, GLFW_KEY_SPACE);
			Key(EKeys::Apostrophe, GLFW_KEY_APOSTROPHE);
			Key(EKeys::Comma, GLFW_KEY_COMMA);
			Key(EKeys::Hyphen, GLFW_KEY_MINUS);
			Key(EKeys::Period, GLFW_KEY_PERIOD);
			Key(EKeys::Slash, GLFW_KEY_SLASH);
			const FKey* Digits[] = {&EKeys::Zero, &EKeys::One, &EKeys::Two, &EKeys::Three, &EKeys::Four, &EKeys::Five,
				&EKeys::Six, &EKeys::Seven, &EKeys::Eight, &EKeys::Nine};
			for (int32 Index = 0; Index < 10; ++Index)
			{
				Key(*Digits[Index], GLFW_KEY_0 + Index);
			}
			Key(EKeys::Semicolon, GLFW_KEY_SEMICOLON);
			Key(EKeys::Equals, GLFW_KEY_EQUAL);
			const FKey* Letters[] = {&EKeys::A, &EKeys::B, &EKeys::C, &EKeys::D, &EKeys::E, &EKeys::F, &EKeys::G,
				&EKeys::H, &EKeys::I, &EKeys::J, &EKeys::K, &EKeys::L, &EKeys::M, &EKeys::N, &EKeys::O, &EKeys::P,
				&EKeys::Q, &EKeys::R, &EKeys::S, &EKeys::T, &EKeys::U, &EKeys::V, &EKeys::W, &EKeys::X, &EKeys::Y,
				&EKeys::Z};
			for (int32 Index = 0; Index < 26; ++Index)
			{
				Key(*Letters[Index], GLFW_KEY_A + Index);
			}
			Key(EKeys::LeftBracket, GLFW_KEY_LEFT_BRACKET);
			Key(EKeys::Backslash, GLFW_KEY_BACKSLASH);
			Key(EKeys::RightBracket, GLFW_KEY_RIGHT_BRACKET);
			Key(EKeys::Tilde, GLFW_KEY_GRAVE_ACCENT);
			Key(EKeys::Escape, GLFW_KEY_ESCAPE);
			Key(EKeys::Enter, GLFW_KEY_ENTER);
			Key(EKeys::Tab, GLFW_KEY_TAB);
			Key(EKeys::BackSpace, GLFW_KEY_BACKSPACE);
			Key(EKeys::Insert, GLFW_KEY_INSERT);
			Key(EKeys::Delete, GLFW_KEY_DELETE);
			Key(EKeys::Right, GLFW_KEY_RIGHT);
			Key(EKeys::Left, GLFW_KEY_LEFT);
			Key(EKeys::Down, GLFW_KEY_DOWN);
			Key(EKeys::Up, GLFW_KEY_UP);
			Key(EKeys::PageUp, GLFW_KEY_PAGE_UP);
			Key(EKeys::PageDown, GLFW_KEY_PAGE_DOWN);
			Key(EKeys::Home, GLFW_KEY_HOME);
			Key(EKeys::End, GLFW_KEY_END);
			Key(EKeys::CapsLock, GLFW_KEY_CAPS_LOCK);
			Key(EKeys::ScrollLock, GLFW_KEY_SCROLL_LOCK);
			Key(EKeys::NumLock, GLFW_KEY_NUM_LOCK);
			Key(EKeys::PrintScreen, GLFW_KEY_PRINT_SCREEN);
			Key(EKeys::Pause, GLFW_KEY_PAUSE);
			const FKey* Functions[] = {&EKeys::F1, &EKeys::F2, &EKeys::F3, &EKeys::F4, &EKeys::F5, &EKeys::F6,
				&EKeys::F7, &EKeys::F8, &EKeys::F9, &EKeys::F10, &EKeys::F11, &EKeys::F12};
			for (int32 Index = 0; Index < 12; ++Index)
			{
				Key(*Functions[Index], GLFW_KEY_F1 + Index);
			}
			const FKey* NumPad[] = {&EKeys::NumPadZero, &EKeys::NumPadOne, &EKeys::NumPadTwo, &EKeys::NumPadThree,
				&EKeys::NumPadFour, &EKeys::NumPadFive, &EKeys::NumPadSix, &EKeys::NumPadSeven, &EKeys::NumPadEight,
				&EKeys::NumPadNine};
			for (int32 Index = 0; Index < 10; ++Index)
			{
				Key(*NumPad[Index], GLFW_KEY_KP_0 + Index);
			}
			Key(EKeys::Decimal, GLFW_KEY_KP_DECIMAL);
			Key(EKeys::Divide, GLFW_KEY_KP_DIVIDE);
			Key(EKeys::Multiply, GLFW_KEY_KP_MULTIPLY);
			Key(EKeys::Subtract, GLFW_KEY_KP_SUBTRACT);
			Key(EKeys::Add, GLFW_KEY_KP_ADD);
			Key(EKeys::NumPadEnter, GLFW_KEY_KP_ENTER);
			Key(EKeys::NumPadEquals, GLFW_KEY_KP_EQUAL);
			Key(EKeys::LeftShift, GLFW_KEY_LEFT_SHIFT);
			Key(EKeys::LeftControl, GLFW_KEY_LEFT_CONTROL);
			Key(EKeys::LeftAlt, GLFW_KEY_LEFT_ALT);
			Key(EKeys::LeftCommand, GLFW_KEY_LEFT_SUPER);
			Key(EKeys::RightShift, GLFW_KEY_RIGHT_SHIFT);
			Key(EKeys::RightControl, GLFW_KEY_RIGHT_CONTROL);
			Key(EKeys::RightAlt, GLFW_KEY_RIGHT_ALT);
			Key(EKeys::RightCommand, GLFW_KEY_RIGHT_SUPER);
			Key(EKeys::Menu, GLFW_KEY_MENU);
			return Map;
		}();
		return KeyMap;
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
	return true;
}

FRHIProcAddressLoader FGLFWWindow::GetRHIProcAddressLoader() const
{
	return reinterpret_cast<FRHIProcAddressLoader>(glfwGetProcAddress);
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

bool FGLFWWindow::IsKeyPressed(const FKey& Key) const
{
	GLFWwindow* Window = AsGLFW(Handle);
	const FGLFWKeyCode* Code = GetGLFWKeyMap().Find(Key.GetFName());
	if (Window == nullptr || Code == nullptr)
	{
		return false;
	}
	return Code->bMouseButton ? glfwGetMouseButton(Window, Code->Code) == GLFW_PRESS
							  : glfwGetKey(Window, Code->Code) == GLFW_PRESS;
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
