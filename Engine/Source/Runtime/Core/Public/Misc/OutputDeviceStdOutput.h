#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDevice.h"

/** Writes formatted log lines to stdout (UE: FOutputDeviceStdOutput). On the PS2 stdout is the EE console. */
class CORE_API FOutputDeviceStdOutput : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual void Flush() override;

	virtual bool CanBeUsedOnAnyThread() const override
	{
		return true;
	}
};

/** Sends formatted log lines to the platform debug channel (UE: FOutputDeviceDebug). */
class CORE_API FOutputDeviceDebug : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;

	virtual bool CanBeUsedOnAnyThread() const override
	{
		return true;
	}
};
