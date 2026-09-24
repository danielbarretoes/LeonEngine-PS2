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

void FGenericWindow::GetCursorPos(double& x, double& y) const
{
	x = 0.0;
	y = 0.0;
}

void FGenericWindow::SetCursorCaptured(bool captured)
{
	cursorCaptured_ = captured;
}

bool FGenericWindow::SetIconFromFile(const char*)
{
	return false;
}

void FGenericWindow::GetWindowSize(int& width, int& height) const
{
	width = windowWidth_;
	height = windowHeight_;
}

void FGenericWindow::GetFramebufferSize(int& width, int& height) const
{
	width = framebufferWidth_;
	height = framebufferHeight_;
}

float FGenericWindow::Aspect() const
{
	if (framebufferWidth_ > 0 && framebufferHeight_ > 0)
	{
		return static_cast<float>(framebufferWidth_) / static_cast<float>(framebufferHeight_);
	}
	return windowHeight_ > 0 ? static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_) : 1.0f;
}

void FGenericWindow::SetScrollCallback(FScrollCallback callback)
{
	scrollCallback_ = std::move(callback);
}

void FGenericWindow::ApplyWindowSize(int width, int height)
{
	windowWidth_ = width;
	windowHeight_ = height;
}

void FGenericWindow::ApplyFramebufferSize(int width, int height)
{
	framebufferWidth_ = width;
	framebufferHeight_ = height;
	if (rhi_)
	{
		rhi_->SetViewport(0, 0, width, height);
	}
}

void FGenericWindow::NotifyScroll(double yOffset)
{
	if (scrollCallback_)
	{
		scrollCallback_(yOffset);
	}
}

bool FGenericWindow::InitRHI(void* (*procAddressLoader)(const char*))
{
	rhi_ = PlatformCreateDynamicRHI();
	if (!rhi_ || !rhi_->Init(procAddressLoader))
	{
		std::printf("FGenericWindow: failed to initialize the RHI\n");
		rhi_.reset();
		return false;
	}
	GDynamicRHI = rhi_.get();
	if (framebufferWidth_ > 0 && framebufferHeight_ > 0)
	{
		rhi_->SetViewport(0, 0, framebufferWidth_, framebufferHeight_);
	}
	return true;
}

void FGenericWindow::ReleaseRHI()
{
	if (GDynamicRHI != nullptr && GDynamicRHI == rhi_.get())
	{
		GDynamicRHI = nullptr;
	}
	rhi_.reset();
}

void FGenericWindow::ResetWindowState()
{
	windowWidth_ = 0;
	windowHeight_ = 0;
	framebufferWidth_ = 0;
	framebufferHeight_ = 0;
	cursorCaptured_ = false;
	shouldClose_ = false;
	scrollCallback_ = nullptr;
}
