#include "Misc/App.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Build.h"
#include "Misc/CString.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

double FApp::DeltaTime = 1.0 / 30.0;
double FApp::CurrentTime = 0.0;

namespace
{
	TCHAR GProjectName[64] = "";
} // namespace

const TCHAR* LexToString(EBuildConfiguration Configuration)
{
	switch (Configuration)
	{
		case EBuildConfiguration::Debug:
			return "Debug";
		case EBuildConfiguration::DebugGame:
			return "DebugGame";
		case EBuildConfiguration::Development:
			return "Development";
		case EBuildConfiguration::Shipping:
			return "Shipping";
		case EBuildConfiguration::Test:
			return "Test";
		default:
			return "Unknown";
	}
}

const TCHAR* FApp::GetProjectName()
{
	return GProjectName;
}

void FApp::SetProjectName(const TCHAR* InProjectName)
{
	FCString::Strncpy(GProjectName, InProjectName != nullptr ? InProjectName : "", sizeof(GProjectName));
}

bool FApp::HasProjectName()
{
	return GProjectName[0] != 0;
}

const TCHAR* FApp::GetName()
{
	return FPlatformProcess::ExecutableName();
}

EBuildConfiguration FApp::GetBuildConfiguration()
{
#if UE_BUILD_DEBUG
	return EBuildConfiguration::Debug;
#elif UE_BUILD_SHIPPING
	return EBuildConfiguration::Shipping;
#elif UE_BUILD_DEVELOPMENT
	return EBuildConfiguration::Development;
#else
	return EBuildConfiguration::Unknown;
#endif
}

FGuid FApp::GetSessionId()
{
	static const FGuid SessionId = FGuid::NewGuid();
	return SessionId;
}

bool FApp::IsUnattended()
{
	static const bool bIsUnattended = FParse::Param(FCommandLine::Get(), "unattended");
	return bIsUnattended;
}

bool FApp::CanEverRender()
{
	return !FParse::Param(FCommandLine::Get(), "nullrhi");
}
