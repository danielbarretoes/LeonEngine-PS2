#include "HAL/IPlatformFileOpenLogWrapper.h"

#include "HAL/PlatformProperties.h"
#include "Logging/LogMacros.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogFileOpenOrder, Log, All);

bool FPlatformFileOpenLog::ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
{
	(void)Inner;
	return FParse::Param(CmdLine, TEXT("LogFileOpenOrder"));
}

bool FPlatformFileOpenLog::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	(void)CmdLine;
	LowerLevel = Inner;
	OpenOrder.Reset();
	Seen.Reset();
	UE_LOG(LogFileOpenOrder, Log, "Logging the order the files open in (-LogFileOpenOrder)");
	return LowerLevel != nullptr;
}

FString FPlatformFileOpenLog::GetOrderPath(const TCHAR* Filename)
{
	FString Full = FPaths::ConvertRelativePathToFull(FString(Filename));
	Full.ReplaceCharInline('\\', '/');
	// A device root counts as its folder ("host:Engine/X" and "host:/Engine/X" are the same file).
	const auto Under = [&Full](FString Directory, FString& OutRelative)
	{
		Directory = FPaths::ConvertRelativePathToFull(Directory);
		Directory.ReplaceCharInline('\\', '/');
		if (!Directory.EndsWith(TEXT("/")))
		{
			Directory += TEXT("/");
		}
		FString Candidate = Full;
		for (FString* Path : {&Directory, &Candidate})
		{
			const int32 Colon = Path->Find(TEXT(":"), ESearchCase::CaseSensitive);
			if (Colon > 1 && Colon + 1 < Path->Len() && (*Path)[Colon + 1] == '/')
			{
				Path->RemoveAt(Colon + 1, 1);
			}
		}
		if (Candidate.StartsWith(Directory, ESearchCase::IgnoreCase))
		{
			OutRelative = Candidate.RightChop(Directory.Len());
			return true;
		}
		return false;
	};
	FString Relative;
	if (Under(FPaths::EngineDir(), Relative))
	{
		return TEXT("Engine/") + Relative;
	}
	if (FApp::HasProjectName() && Under(FPaths::ProjectDir(), Relative))
	{
		return FString(FApp::GetProjectName()) + TEXT("/") + Relative;
	}
	return Full;
}

IFileHandle* FPlatformFileOpenLog::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	IFileHandle* Handle = LowerLevel->OpenRead(Filename, bAllowWrite);
	if (Handle != nullptr)
	{
		NoteOpen(Filename);
	}
	return Handle;
}

IAsyncReadFileHandle* FPlatformFileOpenLog::OpenAsyncRead(const TCHAR* Filename)
{
	// An asynchronous read reads the file as much as a synchronous one (the preloads of N24).
	if (LowerLevel->FileExists(Filename))
	{
		NoteOpen(Filename);
	}
	return LowerLevel->OpenAsyncRead(Filename);
}

void FPlatformFileOpenLog::NoteOpen(const TCHAR* Filename)
{
	const FString Path = GetOrderPath(Filename);
	const FString Key = Path.ToLower();
	if (!Seen.Contains(Key))
	{
		Seen.Add(Key);
		OpenOrder.Add(Path);
		UE_LOG(LogFileOpenOrder, Log, "\"%s\" %d", *Path, OpenOrder.Num());
	}
}

FString FPlatformFileOpenLog::MakeOrderText() const
{
	FString Text;
	for (int32 Index = 0; Index < OpenOrder.Num(); ++Index)
	{
		Text += FString::Printf("\"%s\" %d\n", *OpenOrder[Index], Index + 1);
	}
	return Text;
}

FString FPlatformFileOpenLog::GetOrderFilename()
{
	return FPaths::ProjectSavedDir() + TEXT("Logs/FileOpenOrder-") + FString(FPlatformProperties::PlatformName()) +
		TEXT(".txt");
}

bool FPlatformFileOpenLog::WriteOrderFile() const
{
	const FString Filename = GetOrderFilename();
	// Written through the lower level: the log's own file is not part of the order.
	LowerLevel->CreateDirectoryTree(*FPaths::GetPath(Filename));
	IFileHandle* Handle = LowerLevel->OpenWrite(*Filename);
	if (Handle == nullptr)
	{
		UE_LOG(LogFileOpenOrder, Log, "%d file(s) opened; no order file on this platform (the log has the order)",
			OpenOrder.Num());
		return false;
	}
	const FString Text = MakeOrderText();
	const bool bWritten = Handle->Write(reinterpret_cast<const uint8*>(*Text), Text.Len());
	delete Handle;
	UE_LOG(LogFileOpenOrder, Log, "%d file(s) opened, the order in %s", OpenOrder.Num(), *Filename);
	return bWritten;
}
