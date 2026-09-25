#include "OpenGLDrv.h"

#include <glad/glad.h>

/** Windows: DXGI adapter budget/usage (more accurate than the GL extensions). */
bool GetOpenGLPlatformGPUMemoryStats(FRHIGPUMemoryStats& OutStats);

bool FOpenGLDynamicRHI::Init(void* (*ProcAddressLoader)(const char*))
{
	if (ProcAddressLoader == nullptr)
	{
		return false;
	}
	if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(ProcAddressLoader)) == 0)
	{
		UE_LOG(LogRHI, Error, "Failed to initialize GLAD (OpenGL RHI)");
		return false;
	}
	const char* VersionString = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	Version = VersionString != nullptr ? VersionString : "unknown";
	UE_LOG(LogRHI, Log, "OpenGL %s", *Version);
	return true;
}

void FOpenGLDynamicRHI::SetViewport(int32 X, int32 Y, int32 Width, int32 Height)
{
	glViewport(X, Y, Width, Height);
}

FRHIGPUMemoryStats FOpenGLDynamicRHI::GetGPUMemoryStats() const
{
	FRHIGPUMemoryStats Stats;
	if (GetOpenGLPlatformGPUMemoryStats(Stats))
	{
		return Stats;
	}
	if (GLAD_GL_NVX_gpu_memory_info != 0)
	{
		GLint TotalKb = 0;
		GLint AvailableKb = 0;
		glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &TotalKb);
		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &AvailableKb);
		if (TotalKb > 0 && AvailableKb >= 0)
		{
			Stats.bValid = true;
			Stats.bReportsUsage = true;
			Stats.BudgetBytes = static_cast<uint64>(TotalKb) * 1024u;
			const GLint UsedKb = TotalKb > AvailableKb ? (TotalKb - AvailableKb) : 0;
			Stats.UsedBytes = static_cast<uint64>(UsedKb) * 1024u;
			return Stats;
		}
	}
	if (GLAD_GL_ATI_meminfo != 0)
	{
		GLint TextureFree[4] = {};
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, TextureFree);
		if (TextureFree[0] > 0)
		{
			Stats.bValid = true;
			Stats.bReportsUsage = false;
			Stats.BudgetBytes = static_cast<uint64>(TextureFree[0]) * 1024u;
			return Stats;
		}
	}
	return Stats;
}

FDynamicRHI* PlatformCreateDynamicRHI()
{
	return new FOpenGLDynamicRHI();
}
