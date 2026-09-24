#pragma once

#include "GenericPlatform/GenericApplication.h"

/** Desktop application: GLFW windows; gamepads are not wired on desktop yet. */
class FGLFWApplication final : public GenericApplication
{
public:
	virtual std::unique_ptr<FGenericWindow> MakeWindow() override;
};
