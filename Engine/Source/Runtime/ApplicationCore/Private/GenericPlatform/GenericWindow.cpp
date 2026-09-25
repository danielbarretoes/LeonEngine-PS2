#include "GenericPlatform/GenericWindow.h"

#include "DynamicRHI.h"
#include "GenericPlatform/GenericApplication.h"

// Out of line: the TUniquePtr<FDynamicRHI> member needs the complete type.
FGenericWindow::FGenericWindow() = default;

FGenericWindow::~FGenericWindow()
{
	ReleaseRHI();
}

bool FGenericWindow::CreateShared(const FGenericWindow&, int32, int32, const TCHAR*)
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
	if (OwnedRHI)
	{
		OwnedRHI->SetViewport(0, 0, InWidth, InHeight);
	}
}

void FGenericWindow::NotifyMouseWheel(float Delta)
{
	MouseWheelDelegate.ExecuteIfBound(Delta);
}

bool FGenericWindow::InitRHI(void* (*ProcAddressLoader)(const char*))
{
	OwnedRHI.Reset(PlatformCreateDynamicRHI());
	if (!OwnedRHI || !OwnedRHI->Init(ProcAddressLoader))
	{
		UE_LOG(LogApplicationCore, Error, "FGenericWindow: failed to initialize the RHI");
		OwnedRHI.Reset();
		return false;
	}
	GDynamicRHI = OwnedRHI.Get();
	if (FramebufferWidth > 0 && FramebufferHeight > 0)
	{
		OwnedRHI->SetViewport(0, 0, FramebufferWidth, FramebufferHeight);
	}
	return true;
}

void FGenericWindow::ReleaseRHI()
{
	if (GDynamicRHI != nullptr && GDynamicRHI == OwnedRHI.Get())
	{
		GDynamicRHI = nullptr;
	}
	OwnedRHI.Reset();
}

void FGenericWindow::ResetWindowState()
{
	WindowWidth = 0;
	WindowHeight = 0;
	FramebufferWidth = 0;
	FramebufferHeight = 0;
	bCursorCaptured = false;
	bShouldClose = false;
	MouseWheelDelegate.Unbind();
}
