#include "Desktop/GLFWApplication.h"

#include "Desktop/GLFWWindow.h"
#include "HAL/PlatformApplicationMisc.h"

std::unique_ptr<FGenericWindow> FGLFWApplication::MakeWindow()
{
	return std::make_unique<FGLFWWindow>();
}

GenericApplication* FPlatformApplicationMisc::CreateApplication()
{
	return new FGLFWApplication();
}
