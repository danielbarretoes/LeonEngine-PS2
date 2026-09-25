#include "HAL/PlatformFilemanager.h"

#include "Misc/CString.h"

IPlatformFile& FPlatformFileManager::GetPlatformFile()
{
	if (TopmostPlatformFile == nullptr)
	{
		TopmostPlatformFile = &IPlatformFile::GetPlatformPhysical();
	}
	return *TopmostPlatformFile;
}

void FPlatformFileManager::SetPlatformFile(IPlatformFile& NewTopmostPlatformFile)
{
	TopmostPlatformFile = &NewTopmostPlatformFile;
}

IPlatformFile* FPlatformFileManager::FindPlatformFile(const TCHAR* Name)
{
	for (IPlatformFile* ChainElement = &GetPlatformFile(); ChainElement; ChainElement = ChainElement->GetLowerLevel())
	{
		if (FCString::Stricmp(ChainElement->GetName(), Name) == 0)
		{
			return ChainElement;
		}
	}
	return nullptr;
}

FPlatformFileManager& FPlatformFileManager::Get()
{
	static FPlatformFileManager Singleton;
	return Singleton;
}
