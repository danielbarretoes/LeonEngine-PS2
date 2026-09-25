#include "GenericPlatform/GenericWindow.h"

#include "DynamicRHI.h"
#include "GenericPlatform/GenericApplication.h"

FGenericWindow::FGenericWindow() = default;

FGenericWindow::~FGenericWindow() = default;

bool FGenericWindow::CreateShared(const FGenericWindow&, int32, int32, const TCHAR*)
{
	return false;
}

bool FGenericWindow::IsKeyPressed(const FKey&) const
{
	return false;
}

bool FGenericWindow::IsMouseButtonDown(EMouseButtons) const
{
	return false;
}

FVector2D FGenericWindow::GetCursorPos() const
{
	return FVector2D::ZeroVector;
}

void FGenericWindow::SetCursorCaptured(bool bCaptured)
{
	bCursorCaptured = bCaptured;
}

bool FGenericWindow::SetIconFromFile(const TCHAR*)
{
	return false;
}

void FGenericWindow::GetWindowSize(int32& OutWidth, int32& OutHeight) const
{
	OutWidth = WindowWidth;
	OutHeight = WindowHeight;
}

void FGenericWindow::GetFramebufferSize(int32& OutWidth, int32& OutHeight) const
{
	OutWidth = FramebufferWidth;
	OutHeight = FramebufferHeight;
}

float FGenericWindow::Aspect() const
{
	if (FramebufferWidth > 0 && FramebufferHeight > 0)
	{
		return static_cast<float>(FramebufferWidth) / static_cast<float>(FramebufferHeight);
	}
	return WindowHeight > 0 ? static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight) : 1.0f;
}

void FGenericWindow::ApplyWindowSize(int32 InWidth, int32 InHeight)
{
	WindowWidth = InWidth;
	WindowHeight = InHeight;
}

void FGenericWindow::ApplyFramebufferSize(int32 InWidth, int32 InHeight)
{
	FramebufferWidth = InWidth;
	FramebufferHeight = InHeight;
	if (bDrivesRHIViewport && GDynamicRHI != nullptr)
	{
		GDynamicRHI->SetViewport(0, 0, InWidth, InHeight);
	}
}

void FGenericWindow::BindRHIViewport()
{
	bDrivesRHIViewport = true;
	if (GDynamicRHI != nullptr && FramebufferWidth > 0 && FramebufferHeight > 0)
	{
		GDynamicRHI->SetViewport(0, 0, FramebufferWidth, FramebufferHeight);
	}
}

void FGenericWindow::NotifyMouseWheel(float Delta)
{
	MouseWheelDelegate.ExecuteIfBound(Delta);
}

void FGenericWindow::ResetWindowState()
{
	WindowWidth = 0;
	WindowHeight = 0;
	FramebufferWidth = 0;
	FramebufferHeight = 0;
	bCursorCaptured = false;
	bShouldClose = false;
	bDrivesRHIViewport = false;
	MouseWheelDelegate.Unbind();
}
