#include "PS2Window.h"

#include "PS2RHI.h"

#include <cstdio>

#include <graph.h>

namespace
{
	struct FPS2DisplayState
	{
		bool bInitialized = false;
	};

	FPS2DisplayState GPS2Display;
}

FPS2Window::~FPS2Window()
{
	Destroy();
}

bool FPS2Window::Create(int width, int height, const char* title)
{
	if (handle_ != nullptr)
	{
		return true;
	}
	(void)title;
	const int Width = width > 0 ? width : 640;
	const int Height = height > 0 ? height : 448;

	if (!FPS2RHI::InitDisplay(Width, Height))
	{
		std::printf("FPS2Window: FPS2RHI::InitDisplay failed\n");
		return false;
	}

	GPS2Display.bInitialized = true;
	handle_ = &GPS2Display;
	backendOwned_ = true;
	windowWidth_ = Width;
	windowHeight_ = Height;
	framebufferWidth_ = Width;
	framebufferHeight_ = Height;

	if (!InitRHI(nullptr))
	{
		Destroy();
		return false;
	}
	return true;
}

void FPS2Window::Destroy()
{
	ReleaseRHI();
	if (GPS2Display.bInitialized)
	{
		graph_shutdown();
		GPS2Display.bInitialized = false;
	}
	handle_ = nullptr;
	backendOwned_ = false;
	ResetWindowState();
}

void FPS2Window::SwapBuffers()
{
	if (handle_ != nullptr)
	{
		FPS2RHI::WaitVSync();
	}
}
