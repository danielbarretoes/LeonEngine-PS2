#pragma once

#include "GenericPlatform/GenericWindow.h"

/** Desktop window + OpenGL 3.3 core context through GLFW (Win64 / Linux). */
class FGLFWWindow final : public FGenericWindow
{
public:
	virtual ~FGLFWWindow() override;

	virtual bool Create(int Width, int Height, const char* Title) override;
	virtual bool CreateShared(const FGenericWindow& ShareWith, int Width, int Height, const char* Title) override;
	virtual void Destroy() override;

	virtual void MakeContextCurrent() override;
	virtual void Show() override;
	virtual void Focus() override;
	virtual bool IsFocused() const override;
	virtual bool ShouldClose() const override;
	virtual void PollEvents() override;
	virtual void SwapBuffers() override;

	virtual bool IsKeyPressed(EKeys Key) const override;
	virtual bool IsMouseButtonDown(EMouseButtons Button) const override;
	virtual void GetCursorPos(double& X, double& Y) const override;
	virtual void SetCursorCaptured(bool bCaptured) override;
	virtual bool SetIconFromFile(const char* PngPath) override;

private:
	void InstallCallbacks();
	void SyncSizesFromBackend();
};
