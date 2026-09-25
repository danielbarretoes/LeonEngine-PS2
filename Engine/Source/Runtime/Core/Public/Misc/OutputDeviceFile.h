#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Misc/OutputDevice.h"
#include "Templates/UniquePtr.h"

class FArchive;

/**
 * Writes log lines to a file (UE: FOutputDeviceFile). The default file is <Project>/Saved/Logs/<Project or
 * program>.log; an existing log is renamed to "<Name>-backup-<date>.log" first. Lines carry a UTC time stamp:
 * "[2026.09.25-14.03.07:042]LogInit: Display: ...".
 */
class CORE_API FOutputDeviceFile : public FOutputDevice
{
public:
	explicit FOutputDeviceFile(const TCHAR* InFilename = nullptr, bool bInDisableBackup = false);
	virtual ~FOutputDeviceFile() override;

	/** Switches to another file; the next line opens it. */
	void SetFilename(const TCHAR* InFilename);

	virtual void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual void Flush() override;
	virtual void TearDown() override;

	bool IsOpened() const
	{
		return Writer.IsValid();
	}

	const FString& GetFilename() const
	{
		return Filename;
	}

	/** Renames an existing log to its backup name (UE: CreateBackupCopy). */
	static void CreateBackupCopy(const TCHAR* Filename);

	/** The log file this program writes by default. */
	static FString GetDefaultFilename();

private:
	bool CreateWriter();

	FString Filename;
	TUniquePtr<FArchive> Writer;
	bool bDisableBackup;

	/** Opening failed: stop trying. */
	bool bDead = false;
};
