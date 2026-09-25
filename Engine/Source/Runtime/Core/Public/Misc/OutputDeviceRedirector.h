#pragma once

#include "Containers/Array.h"
#include "CoreGlobals.h"
#include "CoreTypes.h"
#include "Misc/OutputDevice.h"

/** Forwards messages to every registered device; GLog is one (UE: FOutputDeviceRedirector). */
class CORE_API FOutputDeviceRedirector final : public FOutputDevice
{
public:
	FOutputDeviceRedirector() = default;

	/** Adds a device (ignored when already added). The caller keeps ownership. */
	void AddOutputDevice(FOutputDevice* OutputDevice);

	void RemoveOutputDevice(FOutputDevice* OutputDevice);

	bool IsRedirectingTo(FOutputDevice* OutputDevice) const;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual void Serialize(
		const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, const double Time) override;

	virtual void Flush() override;

	/** Flushes and tears the devices down; later messages are dropped. */
	virtual void TearDown() override;

	virtual bool CanBeUsedOnAnyThread() const override
	{
		return false;
	}

private:
	TArray<FOutputDevice*> OutputDevices;
	bool bIsTornDown = false;
	int32 SerializeDepth = 0;
};
