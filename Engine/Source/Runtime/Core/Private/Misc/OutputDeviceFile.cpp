#include "Misc/OutputDeviceFile.h"

#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"

FOutputDeviceFile::FOutputDeviceFile(const TCHAR* InFilename, bool bInDisableBackup)
	: Filename(InFilename != nullptr ? FString(InFilename) : GetDefaultFilename())
	, bDisableBackup(bInDisableBackup)
{
}

FOutputDeviceFile::~FOutputDeviceFile()
{
	TearDown();
}

FString FOutputDeviceFile::GetDefaultFilename()
{
	const FString Name = FApp::HasProjectName() ? FString(FApp::GetProjectName()) : FString(FApp::GetName());
	return FPaths::ProjectLogDir() + Name + ".log";
}

void FOutputDeviceFile::SetFilename(const TCHAR* InFilename)
{
	TearDown();
	Filename = InFilename;
	bDead = false;
}

void FOutputDeviceFile::CreateBackupCopy(const TCHAR* InFilename)
{
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(InFilename) || FileManager.FileSize(InFilename) <= 0)
	{
		return;
	}

	const FString BackupFilename = FString::Printf("%s-backup-%s.%s",
		*FPaths::Combine(FPaths::GetPath(InFilename), FPaths::GetBaseFilename(InFilename)),
		*FileManager.GetTimeStamp(InFilename).ToString(), *FPaths::GetExtension(InFilename));
	FileManager.Move(*BackupFilename, InFilename, true, true);
}

bool FOutputDeviceFile::CreateWriter()
{
	if (!bDisableBackup)
	{
		CreateBackupCopy(*Filename);
	}
	Writer.Reset(IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_AllowRead | FILEWRITE_Silent));
	return Writer.IsValid();
}

void FOutputDeviceFile::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category)
{
	if (bDead)
	{
		return;
	}
	if (!Writer && !CreateWriter())
	{
		bDead = true;
		return;
	}

	const FString Line = FString::Printf("[%s]", *FDateTime::UtcNow().ToString("%Y.%m.%d-%H.%M.%S:%s")) +
		FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Data) + LINE_TERMINATOR;
	Writer->Serialize(const_cast<TCHAR*>(*Line), Line.Len());

	// Every line reaches the file right away, so a crashed or killed process keeps its log.
	Writer->Flush();
}

void FOutputDeviceFile::Flush()
{
	if (Writer)
	{
		Writer->Flush();
	}
}

void FOutputDeviceFile::TearDown()
{
	if (Writer)
	{
		Writer->Close();
		Writer.Reset();
	}
}
