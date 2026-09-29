#pragma once

#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformFile.h"

/**
 * A platform file that records the order the game opens its files in (UE: FPlatformFileOpenLog, `-FileOpenLog`;
 * Leon's switch is `-LogFileOpenOrder`, Docs/PLANS/ps2-shipping.md N23). It sits on top of the chain (above the pak
 * platform file), passes every call through, and notes each file the first time it is opened for reading, by its path
 * in the staged layout: "Engine/..." under the engine's folder, "<Project>/..." under the project's (the paths of the
 * pak's entries), anything else as it was asked for.
 *
 * Each new file is logged as `LogFileOpenOrder: "<path>" <order>` (the PS2 cannot write a file: its EE log is the
 * record there) and, at exit, WriteOrderFile writes the list to <Project>/Saved/Logs/FileOpenOrder-<Platform>.txt,
 * one `"<path>" <order>` a line. `LeonPak -order=` reads either and lays the pak's entries out in that order, so a
 * load reads the disc forward.
 */
class CORE_API FPlatformFileOpenLog : public IPlatformFile
{
public:
	/** Its name in the chain. */
	static const TCHAR* GetTypeName()
	{
		return TEXT("FileOpenLog");
	}

	/** Whether the command line asks for it (-LogFileOpenOrder). */
	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override;
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;
	virtual IPlatformFile* GetLowerLevel() override
	{
		return LowerLevel;
	}
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override
	{
		LowerLevel = NewLowerLevel;
	}
	virtual const TCHAR* GetName() const override
	{
		return GetTypeName();
	}

	virtual bool FileExists(const TCHAR* Filename) override
	{
		return LowerLevel->FileExists(Filename);
	}
	virtual int64 FileSize(const TCHAR* Filename) override
	{
		return LowerLevel->FileSize(Filename);
	}
	virtual bool DeleteFile(const TCHAR* Filename) override
	{
		return LowerLevel->DeleteFile(Filename);
	}
	virtual bool IsReadOnly(const TCHAR* Filename) override
	{
		return LowerLevel->IsReadOnly(Filename);
	}
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		return LowerLevel->MoveFile(To, From);
	}
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		return LowerLevel->SetReadOnly(Filename, bNewReadOnlyValue);
	}
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
	{
		return LowerLevel->GetTimeStamp(Filename);
	}
	/** Opens through the lower level and notes the file the first time it opens. */
	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override
	{
		return LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
	}
	/** Opens through the lower level and notes the file the first time, as OpenRead. */
	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename) override;
	virtual bool DirectoryExists(const TCHAR* Directory) override
	{
		return LowerLevel->DirectoryExists(Directory);
	}
	virtual bool CreateDirectory(const TCHAR* Directory) override
	{
		return LowerLevel->CreateDirectory(Directory);
	}
	virtual bool DeleteDirectory(const TCHAR* Directory) override
	{
		return LowerLevel->DeleteDirectory(Directory);
	}
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
	{
		return LowerLevel->GetStatData(FilenameOrDirectory);
	}
	using IPlatformFile::IterateDirectory;
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override
	{
		return LowerLevel->IterateDirectory(Directory, Visitor);
	}

	/** The staged path of Filename: "Engine/..." or "<Project>/...", or Filename with '/' separators. */
	[[nodiscard]] static FString GetOrderPath(const TCHAR* Filename);

	/** The files opened so far, in order. */
	[[nodiscard]] const TArray<FString>& GetOpenOrder() const
	{
		return OpenOrder;
	}

	/** The order file's text: `"<path>" <order>` a line, from 1. */
	[[nodiscard]] FString MakeOrderText() const;

	/** <Project>/Saved/Logs/FileOpenOrder-<Platform>.txt. */
	[[nodiscard]] static FString GetOrderFilename();

	/** Writes the order to GetOrderFilename; false when it cannot (the PS2 writes no file: its log has the order). */
	bool WriteOrderFile() const;

private:
	/** Notes Filename the first time it opens. */
	void NoteOpen(const TCHAR* Filename);

	IPlatformFile* LowerLevel = nullptr;
	TArray<FString> OpenOrder;
	/** The paths in OpenOrder, lower case. */
	TSet<FString> Seen;
};
