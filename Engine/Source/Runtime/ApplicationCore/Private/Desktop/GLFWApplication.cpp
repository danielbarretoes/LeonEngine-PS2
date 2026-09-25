#include "Desktop/GLFWApplication.h"

#include "Desktop/GLFWWindow.h"
#include "HAL/PlatformApplicationMisc.h"

TSharedRef<FGenericWindow> FGLFWApplication::MakeWindow()
{
	return MakeShared<FGLFWWindow>();
}

GenericApplication* FPlatformApplicationMisc::CreateApplication()
{
	return new FGLFWApplication();
}
