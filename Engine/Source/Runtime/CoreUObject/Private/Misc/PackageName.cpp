#include "Misc/PackageName.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogPackageName, Log, All);

namespace
{
	/** A root and its content folder: "/Game/" and "<Project>/Content/" (UE: FLongPackagePathsSingleton). */
	struct FMountPoint
	{
		FString RootPath;
		FString ContentPath;
	};

	/** The mount points RegisterMountPoint added, in registration order. */
	TArray<FMountPoint>& GetRegisteredMountPoints()
	{
		static TArray<FMountPoint> MountPoints;
		return MountPoints;
	}

	/** Characters a long package name may not contain (UE: INVALID_LONGPACKAGE_CHARACTERS). */
	const TCHAR* const InvalidLongPackageCharacters = TEXT("\\:*?\"<>|' ,.&!~\n\r\t@#");

	/** "/Root/" with both slashes. */
	FString NormalizeRootPath(const FString& RootPath)
	{
		FString Result = RootPath;
		if (!Result.StartsWith(TEXT("/")))
		{
			Result = TEXT("/") + Result;
		}
		if (!Result.EndsWith(TEXT("/")))
		{
			Result += TEXT("/");
		}
		return Result;
	}

	/** A full folder path with '/' and a trailing '/'. */
	FString NormalizeContentPath(const FString& ContentPath)
	{
		FString Result = FPaths::ConvertRelativePathToFull(ContentPath);
		FPaths::NormalizeFilename(Result);
		if (!Result.EndsWith(TEXT("/")))
		{
			Result += TEXT("/");
		}
		return Result;
	}

	/**
	 * Every mount point: the registered ones first (so a plugin or test root wins), then "/Engine/" and "/Game/" with
	 * the current engine and project content folders (resolved on each call: the project can change at startup).
	 */
	TArray<FMountPoint> GetMountPoints()
	{
		TArray<FMountPoint> MountPoints = GetRegisteredMountPoints();
		MountPoints.Add({FString(TEXT("/Engine/")), NormalizeContentPath(FPaths::EngineContentDir())});
		MountPoints.Add({FString(TEXT("/Game/")), NormalizeContentPath(FPaths::ProjectContentDir())});
		return MountPoints;
	}

	/** The mount point whose root starts InPackagePath (the longest one), or nullptr. */
	const FMountPoint* FindMountPointForPackage(const TArray<FMountPoint>& MountPoints, const FString& InPackagePath)
	{
		const FMountPoint* Best = nullptr;
		for (const FMountPoint& MountPoint : MountPoints)
		{
			if (InPackagePath.StartsWith(MountPoint.RootPath) &&
				(!Best || MountPoint.RootPath.Len() > Best->RootPath.Len()))
			{
				Best = &MountPoint;
			}
		}
		return Best;
	}
} // namespace

const FString& FPackageName::GetAssetPackageExtension()
{
	static const FString Extension(TEXT(".lasset"));
	return Extension;
}

const FString& FPackageName::GetMapPackageExtension()
{
	static const FString Extension(TEXT(".lmap"));
	return Extension;
}

bool FPackageName::IsPackageExtension(const TCHAR* Ext)
{
	if (!Ext)
	{
		return false;
	}
	const FString WithDot = (*Ext == '.') ? FString(Ext) : FString(TEXT(".")) + Ext;
	return WithDot.Equals(GetAssetPackageExtension(), ESearchCase::IgnoreCase) ||
		WithDot.Equals(GetMapPackageExtension(), ESearchCase::IgnoreCase);
}

void FPackageName::RegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
	const FString Root = NormalizeRootPath(RootPath);
	const FString Content = NormalizeContentPath(ContentPath);
	for (FMountPoint& MountPoint : GetRegisteredMountPoints())
	{
		if (MountPoint.RootPath.Equals(Root, ESearchCase::IgnoreCase))
		{
			MountPoint.ContentPath = Content;
			return;
		}
	}
	GetRegisteredMountPoints().Add({Root, Content});
}

void FPackageName::UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
	const FString Root = NormalizeRootPath(RootPath);
	const FString Content = NormalizeContentPath(ContentPath);
	TArray<FMountPoint>& MountPoints = GetRegisteredMountPoints();
	for (int32 Index = 0; Index < MountPoints.Num(); ++Index)
	{
		if (MountPoints[Index].RootPath.Equals(Root, ESearchCase::IgnoreCase) &&
			MountPoints[Index].ContentPath.Equals(Content, ESearchCase::IgnoreCase))
		{
			MountPoints.RemoveAt(Index);
			return;
		}
	}
}

bool FPackageName::MountPointExists(const FString& RootPath)
{
	const FString Root = NormalizeRootPath(RootPath);
	for (const FMountPoint& MountPoint : GetMountPoints())
	{
		if (MountPoint.RootPath.Equals(Root, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

bool FPackageName::IsScriptPackage(const FString& InPackageName)
{
	return InPackageName.StartsWith(TEXT("/Script/"));
}

bool FPackageName::IsValidLongPackageName(
	const FString& InLongPackageName, bool bIncludeReadOnlyRoots, FText* OutReason)
{
	auto Fail = [OutReason](const FString& Reason)
	{
		if (OutReason)
		{
			*OutReason = FText::FromString(Reason);
		}
		return false;
	};
	if (InLongPackageName.IsEmpty())
	{
		return Fail(TEXT("the name is empty"));
	}
	if (InLongPackageName[0] != '/')
	{
		return Fail(FString::Printf(TEXT("%s does not start with '/'"), *InLongPackageName));
	}
	if (InLongPackageName.EndsWith(TEXT("/")) || InLongPackageName.Contains(TEXT("//")))
	{
		return Fail(FString::Printf(TEXT("%s ends with '/' or contains '//'"), *InLongPackageName));
	}
	for (const TCHAR* Char = InvalidLongPackageCharacters; *Char; ++Char)
	{
		int32 Index = INDEX_NONE;
		if (InLongPackageName.FindChar(*Char, Index))
		{
			return Fail(FString::Printf(TEXT("%s contains the invalid character '%c'"), *InLongPackageName, *Char));
		}
	}
	if (IsScriptPackage(InLongPackageName))
	{
		// "/Script/<Module>": a read-only root (compiled-in packages have no file).
		return bIncludeReadOnlyRoots ? true
									 : Fail(FString::Printf(TEXT("%s is a /Script package"), *InLongPackageName));
	}
	const TArray<FMountPoint> MountPoints = GetMountPoints();
	const FMountPoint* MountPoint = FindMountPointForPackage(MountPoints, InLongPackageName);
	if (!MountPoint)
	{
		return Fail(FString::Printf(TEXT("%s is not under a mount point"), *InLongPackageName));
	}
	if (InLongPackageName.Len() == MountPoint->RootPath.Len())
	{
		return Fail(FString::Printf(TEXT("%s names no package"), *InLongPackageName));
	}
	return true;
}

bool FPackageName::TryConvertLongPackageNameToFilename(
	const FString& InLongPackageName, FString& OutFilename, const FString& InExtension)
{
	if (IsScriptPackage(InLongPackageName) || !IsValidLongPackageName(InLongPackageName))
	{
		return false;
	}
	const TArray<FMountPoint> MountPoints = GetMountPoints();
	const FMountPoint* MountPoint = FindMountPointForPackage(MountPoints, InLongPackageName);
	if (!MountPoint)
	{
		return false;
	}
	OutFilename = MountPoint->ContentPath + InLongPackageName.RightChop(MountPoint->RootPath.Len()) + InExtension;
	return true;
}

FString FPackageName::LongPackageNameToFilename(const FString& InLongPackageName, const FString& InExtension)
{
	FString Filename;
	if (!TryConvertLongPackageNameToFilename(InLongPackageName, Filename, InExtension))
	{
		UE_LOG(LogPackageName, Error, TEXT("LongPackageNameToFilename: %s is not a long package name with a file"),
			*InLongPackageName);
		return InLongPackageName;
	}
	return Filename;
}

bool FPackageName::TryConvertFilenameToLongPackageName(
	const FString& InFilename, FString& OutPackageName, FString* OutFailureReason)
{
	// Already a long package name.
	if (IsValidLongPackageName(InFilename, true))
	{
		OutPackageName = InFilename;
		return true;
	}
	FString Filename = FPaths::ConvertRelativePathToFull(InFilename);
	FPaths::NormalizeFilename(Filename);
	// No extension: "Maps/Arena.lmap" is the package "Maps/Arena".
	const FString Extension = FPaths::GetExtension(Filename, true);
	if (Extension.Len() > 0 && Filename.EndsWith(Extension))
	{
		Filename = Filename.LeftChop(Extension.Len());
	}

	const TArray<FMountPoint> MountPoints = GetMountPoints();
	const FMountPoint* Best = nullptr;
	for (const FMountPoint& MountPoint : MountPoints)
	{
		if (Filename.StartsWith(MountPoint.ContentPath) &&
			(!Best || MountPoint.ContentPath.Len() > Best->ContentPath.Len()))
		{
			Best = &MountPoint;
		}
	}
	if (!Best)
	{
		if (OutFailureReason)
		{
			*OutFailureReason =
				FString::Printf(TEXT("%s is not under the content folder of a mount point"), *InFilename);
		}
		return false;
	}
	const FString PackageName = Best->RootPath + Filename.RightChop(Best->ContentPath.Len());
	FText Reason;
	if (!IsValidLongPackageName(PackageName, false, &Reason))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason.ToString();
		}
		return false;
	}
	OutPackageName = PackageName;
	return true;
}

FString FPackageName::FilenameToLongPackageName(const FString& InFilename)
{
	FString PackageName;
	FString Reason;
	if (!TryConvertFilenameToLongPackageName(InFilename, PackageName, &Reason))
	{
		UE_LOG(LogPackageName, Error, TEXT("FilenameToLongPackageName: %s"), *Reason);
		return FString();
	}
	return PackageName;
}

bool FPackageName::DoesPackageExist(const FString& LongPackageName, const FGuid* Guid, FString* OutFilename)
{
	(void)Guid;
	FString PackageName = LongPackageName;
	if (!IsValidLongPackageName(PackageName, true) &&
		!TryConvertFilenameToLongPackageName(LongPackageName, PackageName))
	{
		return false;
	}
	if (IsScriptPackage(PackageName))
	{
		return false;
	}
	if (FLinkerLoad::FindInMemoryPackage(PackageName))
	{
		if (OutFilename)
		{
			*OutFilename = PackageName;
		}
		return true;
	}
	FString Filename;
	if (!TryConvertLongPackageNameToFilename(PackageName, Filename))
	{
		return false;
	}
	FString FoundFilename;
	if (!FindPackageFileWithoutExtension(Filename, FoundFilename))
	{
		return false;
	}
	if (OutFilename)
	{
		*OutFilename = FoundFilename;
	}
	return true;
}

bool FPackageName::FindPackageFileWithoutExtension(const FString& InPackageFilename, FString& OutFilename)
{
	IFileManager& FileManager = IFileManager::Get();
	for (const FString* Extension : {&GetAssetPackageExtension(), &GetMapPackageExtension()})
	{
		const FString Candidate = InPackageFilename + *Extension;
		if (FileManager.FileExists(*Candidate))
		{
			OutFilename = Candidate;
			return true;
		}
	}
	return false;
}

FString FPackageName::ObjectPathToPackageName(const FString& InObjectPath)
{
	// Up to the first '.' (the top-level object) or ':' (a subobject path without one).
	for (int32 Index = 0; Index < InObjectPath.Len(); ++Index)
	{
		if (InObjectPath[Index] == '.' || InObjectPath[Index] == ':')
		{
			return InObjectPath.Left(Index);
		}
	}
	return InObjectPath;
}

FString FPackageName::ObjectPathToObjectName(const FString& InObjectPath)
{
	for (int32 Index = InObjectPath.Len() - 1; Index >= 0; --Index)
	{
		if (InObjectPath[Index] == '.' || InObjectPath[Index] == ':')
		{
			return InObjectPath.RightChop(Index + 1);
		}
	}
	return InObjectPath;
}

FString FPackageName::GetShortName(const FString& LongName)
{
	int32 SlashIndex = INDEX_NONE;
	return LongName.FindLastChar('/', SlashIndex) ? LongName.RightChop(SlashIndex + 1) : LongName;
}

FString FPackageName::GetShortName(const UPackage* Package)
{
	return Package ? GetShortName(Package->GetName()) : FString();
}

FString FPackageName::GetShortName(const FName& LongName)
{
	return GetShortName(LongName.ToString());
}

FString FPackageName::GetShortName(const TCHAR* LongName)
{
	return GetShortName(FString(LongName));
}

FString FPackageName::GetLongPackagePath(const FString& InLongPackageName)
{
	int32 SlashIndex = INDEX_NONE;
	return InLongPackageName.FindLastChar('/', SlashIndex) ? InLongPackageName.Left(SlashIndex) : InLongPackageName;
}

bool FPackageName::SplitLongPackageName(const FString& InLongPackageName, FString& OutPackageRoot,
	FString& OutPackagePath, FString& OutPackageName, const bool bStripRootLeadingSlash)
{
	const TArray<FMountPoint> MountPoints = GetMountPoints();
	const FMountPoint* MountPoint = FindMountPointForPackage(MountPoints, InLongPackageName);
	FString Root;
	if (MountPoint)
	{
		Root = MountPoint->RootPath;
	}
	else if (IsScriptPackage(InLongPackageName))
	{
		Root = TEXT("/Script/");
	}
	else
	{
		return false;
	}
	const FString Rest = InLongPackageName.RightChop(Root.Len());
	int32 SlashIndex = INDEX_NONE;
	const bool bHasPath = Rest.FindLastChar('/', SlashIndex);
	OutPackageRoot = bStripRootLeadingSlash ? Root.RightChop(1) : Root;
	OutPackagePath = bHasPath ? Rest.Left(SlashIndex + 1) : FString();
	OutPackageName = bHasPath ? Rest.RightChop(SlashIndex + 1) : Rest;
	return true;
}

FName FPackageName::GetPackageMountPoint(const FString& InPackagePath, bool InWithoutSlashes)
{
	FString Root;
	FString Path;
	FString Name;
	if (!SplitLongPackageName(InPackagePath, Root, Path, Name))
	{
		return NAME_None;
	}
	return FName(*(InWithoutSlashes ? Root.Mid(1, Root.Len() - 2) : Root));
}
