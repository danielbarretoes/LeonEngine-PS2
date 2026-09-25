#include "Misc/Paths.h"

#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/Char.h"
#include "Misc/Guid.h"

// Written by LeonBuildTool into each executable's module table: the engine and project folders relative to the
// executable's folder ("../../" for Engine/Binaries/Win64), and the target's project name ("" when it has none).
extern const char* GLeonEngineDirFromBaseDir;
extern const char* GLeonProjectDirFromBaseDir;
extern const char* GLeonProjectName;

namespace
{
	FString& GetProjectFilePathStorage()
	{
		static FString ProjectFilePath;
		return ProjectFilePath;
	}

	bool IsSlashOrBackslash(TCHAR C)
	{
		return C == '/' || C == '\\';
	}

	/** Index of the last '/' or '\', INDEX_NONE when there is none. */
	int32 FindLastSeparator(const FString& InPath)
	{
		for (int32 Index = InPath.Len() - 1; Index >= 0; --Index)
		{
			if (IsSlashOrBackslash(InPath[Index]))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Length of the root: "/", "C:/", "host:" (device names end in ':'), 0 for relative paths. */
	int32 GetRootLength(const FString& InPath)
	{
		if (InPath.IsEmpty())
		{
			return 0;
		}
		if (InPath[0] == '/')
		{
			return (InPath.Len() > 1 && InPath[1] == '/') ? 2 : 1;
		}
		for (int32 Index = 0; Index < InPath.Len(); ++Index)
		{
			const TCHAR C = InPath[Index];
			if (C == ':')
			{
				return (Index + 1 < InPath.Len() && InPath[Index + 1] == '/') ? Index + 2 : Index + 1;
			}
			if (!FChar::IsAlnum(C))
			{
				break;
			}
		}
		return 0;
	}

	FString WithTrailingSlash(FString InPath)
	{
		if (!InPath.IsEmpty() && InPath[InPath.Len() - 1] != '/')
		{
			InPath += "/";
		}
		return InPath;
	}

#if PLATFORM_DESKTOP
	/** Full path of a folder given relative to the executable's folder. */
	FString FromBaseDir(const TCHAR* Relative)
	{
		return WithTrailingSlash(FPaths::ConvertRelativePathToFull(FString(FPlatformProcess::BaseDir()) + Relative));
	}
#endif
} // namespace

FString FPaths::LaunchDir()
{
	static const FString Dir = WithTrailingSlash(FPlatformProcess::GetCurrentWorkingDirectory());
	return Dir;
}

FString FPaths::EngineDir()
{
#if PLATFORM_DESKTOP
	static const FString Dir = FromBaseDir(GLeonEngineDirFromBaseDir);
#else
	static const FString Dir = FString(FPlatformProcess::BaseDir()) + "Engine/";
#endif
	return Dir;
}

FString FPaths::RootDir()
{
#if PLATFORM_DESKTOP
	static const FString Dir = WithTrailingSlash(GetPath(GetPath(EngineDir())));
#else
	static const FString Dir = FPlatformProcess::BaseDir();
#endif
	return Dir;
}

FString FPaths::EngineContentDir()
{
	return EngineDir() + "Content/";
}

FString FPaths::EngineConfigDir()
{
	return EngineDir() + "Config/";
}

FString FPaths::EngineIntermediateDir()
{
	return EngineDir() + "Intermediate/";
}

FString FPaths::EngineSavedDir()
{
	return EngineDir() + "Saved/";
}

FString FPaths::EnginePluginsDir()
{
	return EngineDir() + "Plugins/";
}

FString FPaths::EngineSourceDir()
{
	return EngineDir() + "Source/";
}

FString FPaths::EnginePlatformExtensionsDir()
{
	return EngineDir() + "Platforms/";
}

FString FPaths::ProjectDir()
{
	if (IsProjectFilePathSet())
	{
		return WithTrailingSlash(GetPath(GetProjectFilePath()));
	}

	if (GLeonProjectDirFromBaseDir[0] != 0)
	{
#if PLATFORM_DESKTOP
		static const FString Dir = FromBaseDir(GLeonProjectDirFromBaseDir);
#else
		static const FString Dir = FString(FPlatformProcess::BaseDir()) + GLeonProjectName + "/";
#endif
		return Dir;
	}

	return EngineDir() + "Programs/" + FApp::GetName() + "/";
}

FString FPaths::ProjectContentDir()
{
	return ProjectDir() + "Content/";
}

FString FPaths::ProjectConfigDir()
{
	return ProjectDir() + "Config/";
}

FString FPaths::ProjectSavedDir()
{
	return ProjectDir() + "Saved/";
}

FString FPaths::ProjectIntermediateDir()
{
	return ProjectDir() + "Intermediate/";
}

FString FPaths::ProjectPluginsDir()
{
	return ProjectDir() + "Plugins/";
}

FString FPaths::ProjectLogDir()
{
	return ProjectSavedDir() + "Logs/";
}

FString FPaths::ProjectPlatformExtensionsDir()
{
	return ProjectDir() + "Platforms/";
}

FString FPaths::GeneratedConfigDir()
{
	return ProjectSavedDir() + "Config/";
}

const FString& FPaths::GetProjectFilePath()
{
	return GetProjectFilePathStorage();
}

bool FPaths::SetProjectFilePath(const FString& NewGameProjectFilePath)
{
	FString& Storage = GetProjectFilePathStorage();
	Storage = NewGameProjectFilePath.IsEmpty() ? FString() : ConvertRelativePathToFull(NewGameProjectFilePath);
	return true;
}

bool FPaths::IsProjectFilePathSet()
{
	return !GetProjectFilePathStorage().IsEmpty();
}

FString FPaths::GetExtension(const FString& InPath, bool bIncludeDot)
{
	const FString Filename = GetCleanFilename(InPath);
	int32 DotPos = INDEX_NONE;
	if (Filename.FindLastChar('.', DotPos))
	{
		return Filename.Mid(DotPos + (bIncludeDot ? 0 : 1));
	}
	return FString();
}

FString FPaths::GetCleanFilename(const FString& InPath)
{
	const int32 EndPos = FindLastSeparator(InPath);
	if (EndPos != INDEX_NONE)
	{
		return InPath.Mid(EndPos + 1);
	}
	// A device prefix ("host:File.ini") ends the path too.
	int32 Colon = INDEX_NONE;
	return InPath.FindLastChar(':', Colon) ? InPath.Mid(Colon + 1) : InPath;
}

FString FPaths::GetBaseFilename(const FString& InPath, bool bRemovePath)
{
	FString Wk = bRemovePath ? GetCleanFilename(InPath) : InPath;

	// Remove the extension.
	int32 ExtPos = INDEX_NONE;
	if (Wk.FindLastChar('.', ExtPos))
	{
		// Determine the position of the path/leaf separator.
		const int32 LeafPos = bRemovePath ? INDEX_NONE : FindLastSeparator(Wk);
		if (LeafPos == INDEX_NONE || ExtPos > LeafPos)
		{
			Wk = Wk.Mid(0, ExtPos);
		}
	}
	return Wk;
}

FString FPaths::GetPath(const FString& InPath)
{
	const int32 Pos = FindLastSeparator(InPath);
	return Pos != INDEX_NONE ? InPath.Mid(0, Pos) : FString();
}

FString FPaths::GetPathLeaf(const FString& InPath)
{
	if (InPath.EndsWith("/") || InPath.EndsWith("\\"))
	{
		return GetCleanFilename(InPath.Mid(0, InPath.Len() - 1));
	}
	return GetCleanFilename(InPath);
}

FString FPaths::ChangeExtension(const FString& InPath, const FString& InNewExtension)
{
	int32 Pos = INDEX_NONE;
	if (InPath.FindLastChar('.', Pos))
	{
		const int32 PathEndPos = FindLastSeparator(InPath);
		if (PathEndPos != INDEX_NONE && PathEndPos > Pos)
		{
			// The dot found was part of the path rather than the name.
			Pos = INDEX_NONE;
		}
	}

	if (Pos == INDEX_NONE)
	{
		return InPath;
	}

	FString Result = InPath.Mid(0, Pos);
	if (InNewExtension.Len() && InNewExtension[0] != '.')
	{
		Result += '.';
	}
	Result += InNewExtension;
	return Result;
}

FString FPaths::SetExtension(const FString& InPath, const FString& InNewExtension)
{
	int32 Pos = INDEX_NONE;
	if (InPath.FindLastChar('.', Pos))
	{
		const int32 PathEndPos = FindLastSeparator(InPath);
		if (PathEndPos != INDEX_NONE && PathEndPos > Pos)
		{
			// The dot found was part of the path rather than the name.
			Pos = INDEX_NONE;
		}
	}

	FString Result = Pos == INDEX_NONE ? InPath : InPath.Mid(0, Pos);
	if (InNewExtension.Len() && InNewExtension[0] != '.')
	{
		Result += '.';
	}
	Result += InNewExtension;
	return Result;
}

bool FPaths::FileExists(const FString& InPath)
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*InPath);
}

bool FPaths::DirectoryExists(const FString& InPath)
{
	return FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*InPath);
}

bool FPaths::IsDrive(const FString& InPath)
{
	FString ConvertedPathString = InPath;
	ConvertedPathString.ReplaceCharInline('\\', '/');
	return !ConvertedPathString.IsEmpty() && GetRootLength(ConvertedPathString) == ConvertedPathString.Len() &&
		ConvertedPathString[0] != '/';
}

bool FPaths::IsRelative(const FString& InPath)
{
	FString Normalized = InPath;
	Normalized.ReplaceCharInline('\\', '/');
	return GetRootLength(Normalized) == 0;
}

void FPaths::NormalizeFilename(FString& InPath)
{
	InPath.ReplaceCharInline('\\', '/');
}

void FPaths::NormalizeDirectoryName(FString& InPath)
{
	InPath.ReplaceCharInline('\\', '/');
	if (InPath.EndsWith("/", ESearchCase::CaseSensitive) && !InPath.EndsWith("//", ESearchCase::CaseSensitive) &&
		!InPath.EndsWith(":/", ESearchCase::CaseSensitive))
	{
		InPath.LeftChopInline(1);
	}
}

bool FPaths::CollapseRelativeDirectories(FString& InPath)
{
	if (InPath.IsEmpty())
	{
		return true;
	}

	const int32 RootLength = GetRootLength(InPath);
	const FString Root = InPath.Mid(0, RootLength);
	const bool bTrailingSlash = InPath.Len() > RootLength && IsSlashOrBackslash(InPath[InPath.Len() - 1]);

	TArray<FString> Parts;
	FString Remainder = InPath.Mid(RootLength);
	Remainder.ReplaceCharInline('\\', '/');
	Remainder.ParseIntoArray(Parts, "/", true);

	TArray<FString> Result;
	for (const FString& Part : Parts)
	{
		if (Part.Equals("."))
		{
			continue;
		}
		if (Part.Equals(".."))
		{
			// Like UE, a ".." that would leave the start of the path is an error.
			if (Result.Num() == 0)
			{
				return false;
			}
			Result.Pop();
			continue;
		}
		Result.Add(Part);
	}

	FString Collapsed = Root + FString::Join(Result, "/");
	if (bTrailingSlash && Result.Num() > 0)
	{
		Collapsed += "/";
	}
	InPath = Collapsed;
	return true;
}

void FPaths::RemoveDuplicateSlashes(FString& InPath)
{
	while (InPath.Contains("//", ESearchCase::CaseSensitive))
	{
		InPath = InPath.Replace("//", "/", ESearchCase::CaseSensitive);
	}
}

FString FPaths::ConvertRelativePathToFull(const FString& InPath)
{
	return ConvertRelativePathToFull(LaunchDir(), InPath);
}

FString FPaths::ConvertRelativePathToFull(const FString& BasePath, const FString& InPath)
{
	FString FullyPathed;
	if (IsRelative(InPath))
	{
		FullyPathed = BasePath;
		FullyPathed /= InPath;
	}
	else
	{
		FullyPathed = InPath;
	}

	NormalizeFilename(FullyPathed);
	CollapseRelativeDirectories(FullyPathed);

	if (FullyPathed.Len() == 0)
	{
		// Empty path is not absolute, and '/' is the best guess across all the platforms.
		FullyPathed = "/";
	}
	return FullyPathed;
}

bool FPaths::MakePathRelativeTo(FString& InPath, const TCHAR* InRelativeTo)
{
	FString Target = ConvertRelativePathToFull(InPath);
	FString Source = GetPath(ConvertRelativePathToFull(InRelativeTo));

	Source.ReplaceCharInline('\\', '/');
	Target.ReplaceCharInline('\\', '/');

	TArray<FString> TargetArray;
	Target.ParseIntoArray(TargetArray, "/", true);
	TArray<FString> SourceArray;
	Source.ParseIntoArray(SourceArray, "/", true);

	if (TargetArray.Num() && SourceArray.Num())
	{
		// Check for being on different drives.
		if (TargetArray[0].Len() > 1 && SourceArray[0].Len() > 1 && TargetArray[0][1] == ':' &&
			SourceArray[0][1] == ':')
		{
			if (FChar::ToUpper(TargetArray[0][0]) != FChar::ToUpper(SourceArray[0][0]))
			{
				return false;
			}
		}
	}

	while (TargetArray.Num() && SourceArray.Num() && TargetArray[0] == SourceArray[0])
	{
		TargetArray.RemoveAt(0);
		SourceArray.RemoveAt(0);
	}

	FString Result;
	for (int32 Index = 0; Index < SourceArray.Num(); Index++)
	{
		Result += "../";
	}
	for (int32 Index = 0; Index < TargetArray.Num(); Index++)
	{
		Result += TargetArray[Index];
		if (Index + 1 < TargetArray.Num())
		{
			Result += "/";
		}
	}

	InPath = Result;
	return true;
}

bool FPaths::IsSamePath(const FString& PathA, const FString& PathB)
{
	FString TmpA = ConvertRelativePathToFull(PathA);
	FString TmpB = ConvertRelativePathToFull(PathB);
	NormalizeDirectoryName(TmpA);
	NormalizeDirectoryName(TmpB);
	return TmpA.Equals(TmpB, ESearchCase::IgnoreCase);
}

bool FPaths::IsUnderDirectory(const FString& InPath, const FString& InDirectory)
{
	const FString Path = ConvertRelativePathToFull(InPath);

	FString Directory = ConvertRelativePathToFull(InDirectory);
	if (Directory.EndsWith("/"))
	{
		Directory.LeftChopInline(1);
	}

	return Path.StartsWith(Directory, ESearchCase::IgnoreCase) &&
		(Path.Len() == Directory.Len() || Path[Directory.Len()] == '/');
}

void FPaths::Split(const FString& InPath, FString& PathPart, FString& FilenamePart, FString& ExtensionPart)
{
	PathPart = GetPath(InPath);
	FilenamePart = GetBaseFilename(InPath);
	ExtensionPart = GetExtension(InPath);
}

FString FPaths::CreateTempFilename(const TCHAR* Path, const TCHAR* Prefix, const TCHAR* Extension)
{
	for (;;)
	{
		const FString UniqueFilename =
			Combine(Path, FString::Printf("%s%s%s", Prefix, *FGuid::NewGuid().ToString(), Extension));
		if (!FileExists(UniqueFilename))
		{
			return UniqueFilename;
		}
	}
}

void FPaths::CombineInternal(FString& OutPath, const TCHAR** Pathes, int32 NumPathes)
{
	if (NumPathes == 0)
	{
		return;
	}

	OutPath = Pathes[0];
	for (int32 Index = 1; Index < NumPathes; ++Index)
	{
		if (Pathes[Index] != nullptr && *Pathes[Index] != 0)
		{
			OutPath /= Pathes[Index];
		}
	}
}
