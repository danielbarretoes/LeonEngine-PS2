#pragma once

#include "GenericPlatform/GenericWindow.h"

/** Desktop window + OpenGL 3.3 core context through GLFW (Win64 / Linux). */
class FGLFWWindow final : public FGenericWindow
{
public:
	virtual ~FGLFWWindow() override;

	virtual bool Create(int32 InWidth, int32 InHeight, const TCHAR* Title) override;
	virtual void Destroy() override;

	virtual void MakeContextCurrent() override;
	virtual void Show() override;
	virtual void Focus() override;
	virtual bool IsFocused() const override;
	virtual bool ShouldClose() const override;
	virtual void PollEvents() override;
	virtual void SwapBuffers() override;

	virtual bool IsKeyPressed(const FKey& Key) const override;
	virtual bool IsMouseButtonDown(EMouseButtons Button) const override;
	virtual FVector2D GetCursorPos() const override;
	virtual void SetCursorCaptured(bool bCaptured) override;
	virtual bool SetIcon(int32 Width, int32 Height, const uint8* RGBA) override;
	virtual FRHIProcAddressLoader GetRHIProcAddressLoader() const override;

private:
	void InstallCallbacks();
	void SyncSizesFromBackend();
};
