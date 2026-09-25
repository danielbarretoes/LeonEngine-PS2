#pragma once

#include "CoreTypes.h"
#include "Logging/LogVerbosity.h"

class FName;
class FString;
class FText;

/** A log sink (UE: FOutputDevice). GLog forwards every message to the registered devices. */
class CORE_API FOutputDevice
{
public:
	FOutputDevice() = default;
	virtual ~FOutputDevice() = default;

	FOutputDevice(const FOutputDevice&) = default;
	FOutputDevice& operator=(const FOutputDevice&) = default;

	/** Receives one message (without a line terminator). */
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) = 0;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, const double /*Time*/)
	{
		Serialize(V, Verbosity, Category);
	}

	virtual void Flush()
	{
	}

	/** Called before the device is destroyed; stop writing. */
	virtual void TearDown()
	{
	}

	/** True when Serialize may be called from any thread. */
	virtual bool CanBeUsedOnAnyThread() const
	{
		return false;
	}

	/** True for devices that only keep messages in memory. */
	virtual bool IsMemoryOnly() const
	{
		return false;
	}

	void SetSuppressEventTag(bool bInSuppressEventTag)
	{
		bSuppressEventTag = bInSuppressEventTag;
	}
	FORCEINLINE bool GetSuppressEventTag() const
	{
		return bSuppressEventTag;
	}

	void SetAutoEmitLineTerminator(bool bInAutoEmitLineTerminator)
	{
		bAutoEmitLineTerminator = bInAutoEmitLineTerminator;
	}
	FORCEINLINE bool GetAutoEmitLineTerminator() const
	{
		return bAutoEmitLineTerminator;
	}

	void Log(const TCHAR* S);
	void Log(ELogVerbosity::Type Verbosity, const TCHAR* S);
	void Log(const FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Str);
	void Log(const FString& S);
	void Log(const FText& S);
	void Log(ELogVerbosity::Type Verbosity, const FString& S);
	void Log(const FName& Category, ELogVerbosity::Type Verbosity, const FString& S);

	void Logf(const TCHAR* Fmt, ...) LEON_PRINTF_FORMAT(2, 3);
	void Logf(ELogVerbosity::Type Verbosity, const TCHAR* Fmt, ...) LEON_PRINTF_FORMAT(3, 4);
	void CategorizedLogf(const FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Fmt, ...)
		LEON_PRINTF_FORMAT(4, 5);

protected:
	/** Omits the "Category: Verbosity:" prefix when set. */
	bool bSuppressEventTag = false;

	/** Appends a line terminator to each message. */
	bool bAutoEmitLineTerminator = true;
};
