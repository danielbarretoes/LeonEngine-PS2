#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformFile.h"

/** Owner of the platform file chain (UE: FPlatformFileManager). */
class CORE_API FPlatformFileManager
{
public:
	/** The topmost platform file; the physical one until a wrapper is pushed. */
	IPlatformFile& GetPlatformFile();

	/** Makes NewTopmostPlatformFile the one handed out; it must already wrap the previous one. */
	void SetPlatformFile(IPlatformFile& NewTopmostPlatformFile);

	/** The platform file named Name in the chain, or nullptr. */
	IPlatformFile* FindPlatformFile(const TCHAR* Name);

	static FPlatformFileManager& Get();

private:
	FPlatformFileManager() = default;

	IPlatformFile* TopmostPlatformFile = nullptr;
};
