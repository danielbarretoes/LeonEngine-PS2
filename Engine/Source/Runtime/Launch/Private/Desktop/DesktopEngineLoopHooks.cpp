#include "PlatformEngineLoopHooks.h"

// Desktop draws its debug overlay through the engine framework (GameApplication), not here.
void FPlatformEngineLoopHooks::EndFrame(FGenericWindow& Window, GenericApplication& Application)
{
	(void)Window;
	(void)Application;
}

void FPlatformEngineLoopHooks::PostPresent()
{
}
