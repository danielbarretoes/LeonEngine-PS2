#pragma once

#include "DynamicRHI.h"

#include <string>

/** OpenGL 3.3 core RHI backend (UE: FOpenGLDynamicRHI). */
class OPENGLDRV_API FOpenGLDynamicRHI final : public FDynamicRHI
{
public:
	virtual bool Init(void* (*ProcAddressLoader)(const char*)) override;
	virtual void SetViewport(int32 X, int32 Y, int32 Width, int32 Height) override;
	virtual FRHIGPUMemoryStats GetGPUMemoryStats() const override;

	virtual const char* GetName() const override
	{
		return "OpenGL";
	}

	virtual const char* GetAPIVersionString() const override
	{
		return Version.c_str();
	}

private:
	std::string Version = "unknown";
};
