#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "InputCoreTypes.h"

#include <functional>
#include <memory>

class FDynamicRHI;

/** Opaque OS / graphics window handle (GLFWwindow* on desktop, GS state on PS2). */
using FNativeWindowHandle = void*;

/**
 * Platform window + graphics context (UE: FGenericWindow). Created by
 * GenericApplication::MakeWindow(); owns the RHI device it creates (published in GDynamicRHI).
 */
class APPLICATIONCORE_API FGenericWindow
{
public:
	using FScrollCallback = std::function<void(double YOffset)>;

	FGenericWindow();
	virtual ~FGenericWindow();

	FGenericWindow(const FGenericWindow&) = delete;
	FGenericWindow& operator=(const FGenericWindow&) = delete;

	virtual bool Create(int width, int height, const char* title) = 0;

	/** Secondary window sharing the graphics context of `shareWith` (desktop only). */
	virtual bool CreateShared(const FGenericWindow& shareWith, int width, int height, const char* title);

	virtual void Destroy() = 0;

	virtual void MakeContextCurrent()
	{
	}

	virtual void Show()
	{
	}

	virtual void Focus()
	{
	}

	virtual bool IsFocused() const = 0;
	virtual bool ShouldClose() const = 0;

	/** Pumps OS messages for this window. */
	virtual void PollEvents() = 0;

	/** Presents the frame (desktop: swap chain; PS2: vsync). */
	virtual void SwapBuffers() = 0;

	virtual bool IsKeyPressed(EKeys key) const;
	virtual bool IsMouseButtonDown(EMouseButtons button) const;
	virtual void GetCursorPos(double& x, double& y) const;
	virtual void SetCursorCaptured(bool captured);
	virtual bool SetIconFromFile(const char* pngPath);

	FNativeWindowHandle NativeHandle() const
	{
		return handle_;
	}

	FDynamicRHI* RHIDevice() const
	{
		return rhi_.get();
	}

	int Width() const
	{
		return windowWidth_;
	}

	int Height() const
	{
		return windowHeight_;
	}

	void GetWindowSize(int& width, int& height) const;

	int FramebufferWidth() const
	{
		return framebufferWidth_;
	}

	int FramebufferHeight() const
	{
		return framebufferHeight_;
	}

	void GetFramebufferSize(int& width, int& height) const;

	float Aspect() const;

	bool IsCursorCaptured() const
	{
		return cursorCaptured_;
	}

	void SetScrollCallback(FScrollCallback callback);

	/** Backend callbacks update sizes / scroll through these. */
	void ApplyWindowSize(int width, int height);
	void ApplyFramebufferSize(int width, int height);
	void NotifyScroll(double yOffset);

protected:
	/** Creates the platform RHI, loads it and publishes it in GDynamicRHI. */
	bool InitRHI(void* (*procAddressLoader)(const char*));

	/** Releases the RHI created by InitRHI (clears GDynamicRHI if it is ours). */
	void ReleaseRHI();

	/** Resets the size / cursor / callback state after the backend window is gone. */
	void ResetWindowState();

	FNativeWindowHandle handle_ = nullptr;
	std::unique_ptr<FDynamicRHI> rhi_;
	int windowWidth_ = 0;
	int windowHeight_ = 0;
	int framebufferWidth_ = 0;
	int framebufferHeight_ = 0;
	bool backendOwned_ = false;
	bool cursorCaptured_ = false;
	bool shouldClose_ = false;
	FScrollCallback scrollCallback_;
};
