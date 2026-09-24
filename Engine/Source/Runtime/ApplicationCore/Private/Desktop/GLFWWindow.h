#pragma once

#include "GenericPlatform/GenericWindow.h"

/** Desktop window + OpenGL 3.3 core context through GLFW (Win64 / Linux). */
class FGLFWWindow final : public FGenericWindow
{
public:
	virtual ~FGLFWWindow() override;

	virtual bool Create(int width, int height, const char* title) override;
	virtual bool CreateShared(const FGenericWindow& shareWith, int width, int height, const char* title) override;
	virtual void Destroy() override;

	virtual void MakeContextCurrent() override;
	virtual void Show() override;
	virtual void Focus() override;
	virtual bool IsFocused() const override;
	virtual bool ShouldClose() const override;
	virtual void PollEvents() override;
	virtual void SwapBuffers() override;

	virtual bool IsKeyPressed(EKeys key) const override;
	virtual bool IsMouseButtonDown(EMouseButtons button) const override;
	virtual void GetCursorPos(double& x, double& y) const override;
	virtual void SetCursorCaptured(bool captured) override;
	virtual bool SetIconFromFile(const char* pngPath) override;

private:
	void InstallCallbacks();
	void SyncSizesFromBackend();
};
