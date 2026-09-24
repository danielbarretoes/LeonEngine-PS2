#pragma once

#include "CoreTypes.h"

#include <memory>

/** GPU memory reported by the active RHI (0 / invalid when the API cannot tell). */
struct FRHIGPUMemoryStats
{
	bool bValid = false;

	/** False when only the budget is known (e.g. ATI_meminfo reports free memory only). */
	bool bReportsUsage = false;

	uint64 BudgetBytes = 0;
	uint64 UsedBytes = 0;
};

/**
 * Graphics API backend (UE: FDynamicRHI). One instance per process, created by the platform RHI
 * module through PlatformCreateDynamicRHI() and published in GDynamicRHI.
 */
class RHI_API FDynamicRHI
{
public:
	virtual ~FDynamicRHI() = default;

	/** Loads API entry points once a window / context exists (OpenGL: glfwGetProcAddress). */
	virtual bool Init(void* (*ProcAddressLoader)(const char*)) = 0;

	virtual void SetViewport(int32 X, int32 Y, int32 Width, int32 Height) = 0;

	virtual FRHIGPUMemoryStats GetGPUMemoryStats() const = 0;

	virtual const char* GetName() const = 0;
	virtual const char* GetAPIVersionString() const = 0;
};

/** The active RHI (UE: GDynamicRHI); set by the window that owns the graphics context. */
extern RHI_API FDynamicRHI* GDynamicRHI;

/** Implemented by the platform's RHI module (Win64/Linux: OpenGLDrv, PS2: PS2RHI). */
std::unique_ptr<FDynamicRHI> PlatformCreateDynamicRHI();
