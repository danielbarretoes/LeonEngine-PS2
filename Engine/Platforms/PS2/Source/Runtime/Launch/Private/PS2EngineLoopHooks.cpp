#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "PS2StatsOverlay.h"
#include "PlatformEngineLoopHooks.h"

void FPlatformEngineLoopHooks::EndFrame(FGenericWindow& Window, GenericApplication& Application)
{
	FPS2StatsOverlay::Draw(
		Window.GetFramebufferWidth(), Window.GetFramebufferHeight(), Application.GetInputInterface());
}

void FPlatformEngineLoopHooks::PostPresent()
{
	FPS2StatsOverlay::MarkFrameStart();
}
