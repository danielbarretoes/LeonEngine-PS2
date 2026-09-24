#include "GenericPlatform/GenericWindow.h"

#include "DynamicRHI.h"

#include <cstdio>
#include <utility>

// Out of line: the unique_ptr<FDynamicRHI> member needs the complete type.
FGenericWindow::FGenericWindow() = default;

FGenericWindow::~FGenericWindow()
{
	ReleaseRHI();
}

bool FGenericWindow::CreateShared(const FGenericWindow&, int, int, const char*)
{
	return false;
}

bool FGenericWindow::IsKeyPressed(EKeys) const
{
	return false;
}

bool FGenericWindow::IsMouseButtonDown(EMouseButtons) const
{
	return false;
}

void FGenericWindow::GetCursorPos(double& X, double& Y) const
{
	X = 0.0;
	Y = 0.0;
}

void FGenericWindow::SetCursorCaptured(bool bCaptured)
{
	bCursorCaptured = bCaptured;
}

bool FGenericWindow::SetIconFromFile(const char*)
{
	return false;
}

void FGenericWindow::GetWindowSize(int& InWidth, int& InHeight) const
{
	InWidth = WindowWidth;
	InHeight = WindowHeight;
}

void FGenericWindow::GetFramebufferSize(int& InWidth, int& InHeight) const
{
	InWidth = FramebufferWidth;
	InHeight = FramebufferHeight;
}

float FGenericWindow::Aspect() const
{
	if (FramebufferWidth > 0 && FramebufferHeight > 0)
	{
		return static_cast<float>(FramebufferWidth) / static_cast<float>(FramebufferHeight);
	}
	return WindowHeight > 0 ? static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight) : 1.0f;
}

void FGenericWindow::SetScrollCallback(FScrollCallback Callback)
{
	ScrollCallback = std::move(Callback);
}

void FGenericWindow::ApplyWindowSize(int InWidth, int InHeight)
{
	WindowWidth = InWidth;
	WindowHeight = InHeight;
}

void FGenericWindow::ApplyFramebufferSize(int InWidth, int InHeight)
{
	FramebufferWidth = InWidth;
	FramebufferHeight = InHeight;
	if (OwnedRHI)
	{
		OwnedRHI->SetViewport(0, 0, InWidth, InHeight);
	}
}

void FGenericWindow::NotifyScroll(double YOffset)
{
	if (ScrollCallback)
	{
		ScrollCallback(YOffset);
	}
}

bool FGenericWindow::InitRHI(void* (*ProcAddressLoader)(const char*))
{
	OwnedRHI = PlatformCreateDynamicRHI();
	if (!OwnedRHI || !OwnedRHI->Init(ProcAddressLoader))
	{
		std::printf("FGenericWindow: failed to initialize the RHI\n");
		OwnedRHI.reset();
		return false;
	}
	GDynamicRHI = OwnedRHI.get();
	if (FramebufferWidth > 0 && FramebufferHeight > 0)
	{
		OwnedRHI->SetViewport(0, 0, FramebufferWidth, FramebufferHeight);
	}
	return true;
}

void FGenericWindow::ReleaseRHI()
{
	if (GDynamicRHI != nullptr && GDynamicRHI == OwnedRHI.get())
	{
		GDynamicRHI = nullptr;
	}
	OwnedRHI.reset();
}

void FGenericWindow::ResetWindowState()
{
	WindowWidth = 0;
	WindowHeight = 0;
	FramebufferWidth = 0;
	FramebufferHeight = 0;
	bCursorCaptured = false;
	bShouldClose = false;
	ScrollCallback = nullptr;
}
