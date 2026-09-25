#pragma once

#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "InputCoreTypes.h"
#include "Math/Vector2D.h"
#include "Templates/UniquePtr.h"

class FDynamicRHI;

/** Opaque OS / graphics window handle (GLFWwindow* on desktop, GS state on PS2). */
using FNativeWindowHandle = void*;

/** Mouse wheel over a window, in notches (UE routes it through FGenericApplicationMessageHandler::OnMouseWheel). */
DECLARE_DELEGATE_OneParam(FOnWindowMouseWheel, float /* Delta */);

/**
 * Platform window + graphics context (UE: FGenericWindow). Created by
 * GenericApplication::MakeWindow(); owns the RHI device it creates (published in GDynamicRHI).
 */
class APPLICATIONCORE_API FGenericWindow
{
public:
	FGenericWindow();
	virtual ~FGenericWindow();

	FGenericWindow(const FGenericWindow&) = delete;
	FGenericWindow& operator=(const FGenericWindow&) = delete;

	virtual bool Create(int32 InWidth, int32 InHeight, const TCHAR* Title) = 0;

	/** Secondary window sharing the graphics context of ShareWith (desktop only). */
	virtual bool CreateShared(const FGenericWindow& ShareWith, int32 InWidth, int32 InHeight, const TCHAR* Title);

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

	/** Whether a keyboard key or a mouse button is down (the platform maps the FKey to its own code). */
	virtual bool IsKeyPressed(const FKey& Key) const;
	virtual bool IsMouseButtonDown(EMouseButtons Button) const;

	/** Cursor position in window coordinates (UE: ICursor::GetPosition). */
	virtual FVector2D GetCursorPos() const;

	virtual void SetCursorCaptured(bool bCaptured);
	virtual bool SetIconFromFile(const TCHAR* PngPath);

	FNativeWindowHandle NativeHandle() const
	{
		return Handle;
	}

	FDynamicRHI* RHIDevice() const
	{
		return OwnedRHI.Get();
	}

	int32 Width() const
	{
		return WindowWidth;
	}

	int32 Height() const
	{
		return WindowHeight;
	}

	void GetWindowSize(int32& OutWidth, int32& OutHeight) const;

	int32 GetFramebufferWidth() const
	{
		return FramebufferWidth;
	}

	int32 GetFramebufferHeight() const
	{
		return FramebufferHeight;
	}

	void GetFramebufferSize(int32& OutWidth, int32& OutHeight) const;

	float Aspect() const;

	bool IsCursorCaptured() const
	{
		return bCursorCaptured;
	}

	/** Bound by the consumer of the wheel (the game engine's camera zoom). */
	FOnWindowMouseWheel& OnMouseWheel()
	{
		return MouseWheelDelegate;
	}

	/** Backend callbacks update sizes / the wheel through these. */
	void ApplyWindowSize(int32 InWidth, int32 InHeight);
	void ApplyFramebufferSize(int32 InWidth, int32 InHeight);
	void NotifyMouseWheel(float Delta);

protected:
	/** Creates the platform RHI, loads it and publishes it in GDynamicRHI. */
	bool InitRHI(void* (*ProcAddressLoader)(const char*));

	/** Releases the RHI created by InitRHI (clears GDynamicRHI if it is ours). */
	void ReleaseRHI();

	/** Resets the size / cursor / delegate state after the backend window is gone. */
	void ResetWindowState();

	FNativeWindowHandle Handle = nullptr;
	TUniquePtr<FDynamicRHI> OwnedRHI;
	int32 WindowWidth = 0;
	int32 WindowHeight = 0;
	int32 FramebufferWidth = 0;
	int32 FramebufferHeight = 0;
	bool bBackendOwned = false;
	bool bCursorCaptured = false;
	bool bShouldClose = false;
	FOnWindowMouseWheel MouseWheelDelegate;
};
