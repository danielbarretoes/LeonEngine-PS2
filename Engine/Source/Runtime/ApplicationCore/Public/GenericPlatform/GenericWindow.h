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

	virtual bool Create(int InWidth, int InHeight, const char* Title) = 0;

	/** Secondary window sharing the graphics context of `shareWith` (desktop only). */
	virtual bool CreateShared(const FGenericWindow& ShareWith, int InWidth, int InHeight, const char* Title);

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

	virtual bool IsKeyPressed(EKeys Key) const;
	virtual bool IsMouseButtonDown(EMouseButtons Button) const;
	virtual void GetCursorPos(double& X, double& Y) const;
	virtual void SetCursorCaptured(bool bCaptured);
	virtual bool SetIconFromFile(const char* PngPath);

	FNativeWindowHandle NativeHandle() const
	{
		return Handle;
	}

	FDynamicRHI* RHIDevice() const
	{
		return OwnedRHI.get();
	}

	int Width() const
	{
		return WindowWidth;
	}

	int Height() const
	{
		return WindowHeight;
	}

	void GetWindowSize(int& InWidth, int& InHeight) const;

	int GetFramebufferWidth() const
	{
		return FramebufferWidth;
	}

	int GetFramebufferHeight() const
	{
		return FramebufferHeight;
	}

	void GetFramebufferSize(int& InWidth, int& InHeight) const;

	float Aspect() const;

	bool IsCursorCaptured() const
	{
		return bCursorCaptured;
	}

	void SetScrollCallback(FScrollCallback Callback);

	/** Backend callbacks update sizes / scroll through these. */
	void ApplyWindowSize(int InWidth, int InHeight);
	void ApplyFramebufferSize(int InWidth, int InHeight);
	void NotifyScroll(double YOffset);

protected:
	/** Creates the platform RHI, loads it and publishes it in GDynamicRHI. */
	bool InitRHI(void* (*ProcAddressLoader)(const char*));

	/** Releases the RHI created by InitRHI (clears GDynamicRHI if it is ours). */
	void ReleaseRHI();

	/** Resets the size / cursor / callback state after the backend window is gone. */
	void ResetWindowState();

	FNativeWindowHandle Handle = nullptr;
	std::unique_ptr<FDynamicRHI> OwnedRHI;
	int WindowWidth = 0;
	int WindowHeight = 0;
	int FramebufferWidth = 0;
	int FramebufferHeight = 0;
	bool bBackendOwned = false;
	bool bCursorCaptured = false;
	bool bShouldClose = false;
	FScrollCallback ScrollCallback;
};
