#include "DynamicRHI.h"

/** No OS-level GPU memory query on Linux; FOpenGLDynamicRHI falls back to GL extensions. */
bool GetOpenGLPlatformGPUMemoryStats(FRHIGPUMemoryStats& OutStats)
{
	(void)OutStats;
	return false;
}
