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

bool FPS2Window::Create(int InWidth, int InHeight, const char* Title)
{
	if (Handle != nullptr)
	{
		return true;
	}
	(void)Title;
	const int DisplayWidth = InWidth > 0 ? InWidth : 640;
	const int DisplayHeight = InHeight > 0 ? InHeight : 448;

	if (!FPS2RHI::InitDisplay(DisplayWidth, DisplayHeight))
	{
		std::printf("FPS2Window: FPS2RHI::InitDisplay failed\n");
		return false;
	}

	GPS2Display.bInitialized = true;
	Handle = &GPS2Display;
	bBackendOwned = true;
	WindowWidth = DisplayWidth;
	WindowHeight = DisplayHeight;
	FramebufferWidth = DisplayWidth;
	FramebufferHeight = DisplayHeight;

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
