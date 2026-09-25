#pragma once

#include "CoreTypes.h"
#include "Logging/LogVerbosity.h"

class FName;
class FString;

/** Log line formatting shared by the devices (UE: FOutputDeviceHelper). */
struct CORE_API FOutputDeviceHelper
{
	/** "Category: Verbosity: Message"; the verbosity is omitted for Log (UE). */
	static FString FormatLogLine(ELogVerbosity::Type Verbosity, const FName& Category, const TCHAR* Message = nullptr);
};
