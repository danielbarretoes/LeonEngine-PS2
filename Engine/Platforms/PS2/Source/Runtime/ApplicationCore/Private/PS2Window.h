#pragma once

#include "GenericPlatform/GenericWindow.h"

/** The PS2 "window": the GS display (framebuffer + z-buffer) and the PS2 RHI. */
class FPS2Window final : public FGenericWindow
{
public:
	virtual ~FPS2Window() override;

	virtual bool Create(int32 InWidth, int32 InHeight, const TCHAR* Title) override;
	virtual void Destroy() override;

	virtual bool IsFocused() const override
	{
		return Handle != nullptr;
	}

	virtual bool ShouldClose() const override
	{
		return bShouldClose || Handle == nullptr;
	}

	/** No OS message queue on the EE; controllers are polled by the application. */
	virtual void PollEvents() override
	{
	}

	/** Waits for vsync (the GS displays the single framebuffer). */
	virtual void SwapBuffers() override;
};
