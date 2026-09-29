#include "PS2Window.h"

#include "GenericPlatform/GenericApplication.h"
#include "PS2RHI.h"

namespace
{
	struct FPS2DisplayState
	{
		bool bInitialized = false;
	};

	FPS2DisplayState GPS2Display;
} // namespace

FPS2Window::~FPS2Window()
{
	Destroy();
}

bool FPS2Window::Create(int32 InWidth, int32 InHeight, const TCHAR* Title)
{
	if (Handle != nullptr)
	{
		return true;
	}
	(void)Title;
	// The CRTC's modes are 640 wide at most, 448 (NTSC) or 512 (PAL) lines: a desktop size (the base Engine config's,
	// when the PS2 one was not read) would not even fit two frame buffers in the GS's 4 MB.
	const bool bPS2Mode = InWidth > 0 && InWidth <= 640 && InHeight > 0 && InHeight <= 512;
	if (!bPS2Mode)
	{
		UE_LOG(LogApplicationCore, Warning, "FPS2Window: %dx%d is not a PS2 display mode; 640x448", InWidth, InHeight);
	}
	const int32 DisplayWidth = bPS2Mode ? InWidth : 640;
	const int32 DisplayHeight = bPS2Mode ? InHeight : 448;

	if (!FPS2RHI::InitDisplay(DisplayWidth, DisplayHeight))
	{
		UE_LOG(LogApplicationCore, Error, "FPS2Window: FPS2RHI::InitDisplay failed");
		return false;
	}

	GPS2Display.bInitialized = true;
	Handle = &GPS2Display;
	bBackendOwned = true;
	WindowWidth = DisplayWidth;
	WindowHeight = DisplayHeight;
	FramebufferWidth = DisplayWidth;
	FramebufferHeight = DisplayHeight;
	return true;
}

void FPS2Window::Destroy()
{
	if (GPS2Display.bInitialized)
	{
		FPS2RHI::ShutdownDisplay();
		GPS2Display.bInitialized = false;
	}
	Handle = nullptr;
	bBackendOwned = false;
	ResetWindowState();
}

void FPS2Window::SwapBuffers()
{
	if (Handle != nullptr)
	{
		FPS2RHI::WaitVSync();
	}
}
