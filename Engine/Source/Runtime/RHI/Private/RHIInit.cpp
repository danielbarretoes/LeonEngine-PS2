#include "DynamicRHI.h"

// In its own file: only the executables that call RHIInit (the launch module) link a platform RHI module.

bool RHIInit(FRHIProcAddressLoader ProcAddressLoader)
{
	if (GDynamicRHI != nullptr)
	{
		return true;
	}
	FDynamicRHI* DynamicRHI = PlatformCreateDynamicRHI();
	if (DynamicRHI == nullptr || !DynamicRHI->Init(ProcAddressLoader))
	{
		UE_LOG(LogRHI, Error, "RHIInit: failed to initialize the RHI");
		delete DynamicRHI;
		return false;
	}
	GDynamicRHI = DynamicRHI;
	return true;
}

void RHIExit()
{
	delete GDynamicRHI;
	GDynamicRHI = nullptr;
}
