#include "EditorFramework/AssetImportData.h"

#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "UObject/Package.h"

UAssetImportData::UAssetImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITORONLY_DATA

namespace
{

	/** A full path with '/' separators and no "..". */
	FString NormalizeFullPath(const FString& Path)
	{
		FString Full = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Full);
		FPaths::CollapseRelativeDirectories(Full);
		return Full;
	}

	/** NormalizeFullPath of a directory, ending with '/'. */
	FString NormalizeDirectory(const FString& Path)
	{
		FString Dir = NormalizeFullPath(Path);
		if (!Dir.EndsWith(TEXT("/")))
		{
			Dir += TEXT("/");
		}
		return Dir;
	}

} // namespace

void UAssetImportData::Update(const FString& AbsoluteFilename, const FString& FileHash)
{
	SourceData.SourceFiles.Reset();
	SourceData.SourceFiles.Add(FAssetImportSourceFile(
		SanitizeImportFilename(AbsoluteFilename), FileHash.IsEmpty() ? HashFile(AbsoluteFilename) : FileHash));
}

FString UAssetImportData::GetFirstFilename() const
{
	return SourceData.SourceFiles.Num() > 0 ? ResolveImportFilename(SourceData.SourceFiles[0].RelativeFilename)
											: FString();
}

void UAssetImportData::ExtractFilenames(TArray<FString>& AbsoluteFilenames) const
{
	for (const FAssetImportSourceFile& File : SourceData.SourceFiles)
	{
		AbsoluteFilenames.Add(ResolveImportFilename(File.RelativeFilename));
	}
}

FString UAssetImportData::GetFirstFileHash() const
{
	return SourceData.SourceFiles.Num() > 0 ? SourceData.SourceFiles[0].FileHash : FString();
}

FString UAssetImportData::SanitizeImportFilename(const FString& AbsolutePath) const
{
	const FString Full = NormalizeFullPath(AbsolutePath);
	FString Relative = Full;
	// MakePathRelativeTo drops the last element of the base: the root ends with '/'.
	if (!FPaths::MakePathRelativeTo(Relative, *GetSourceRootDir()))
	{
		return Full;
	}
	return Relative;
}

FString UAssetImportData::ResolveImportFilename(const FString& RelativePath) const
{
	if (RelativePath.IsEmpty())
	{
		return FString();
	}
	if (!FPaths::IsRelative(RelativePath))
	{
		return NormalizeFullPath(RelativePath);
	}
	return NormalizeFullPath(GetSourceRootDir() + RelativePath);
}

FString UAssetImportData::GetSourceRootDir() const
{
	return GetSourceRootDir(GetOutermost()->GetName());
}

void UAssetImportData::SetImportSettings(const TMap<FString, FString>& InSettings)
{
	ImportSettings = InSettings;
	ImportSettings.KeySort(TLess<FString>());
}

FString UAssetImportData::GetSourceRootDir(const FString& LongPackageName)
{
	const FName MountPoint = FPackageName::GetPackageMountPoint(LongPackageName);
	if (MountPoint == FName(TEXT("Engine")))
	{
		return NormalizeDirectory(FPaths::EngineDir());
	}
	if (MountPoint == FName(TEXT("Game")))
	{
		return NormalizeDirectory(FPaths::ProjectDir());
	}
	// Any other mount point: the directory above its content ("/Root/X" names <Content>/X).
	FString Filename;
	if (!MountPoint.IsNone() &&
		FPackageName::TryConvertLongPackageNameToFilename(
			FString(TEXT("/")) + MountPoint.ToString() + TEXT("/X"), Filename))
	{
		return NormalizeDirectory(FPaths::GetPath(FPaths::GetPath(Filename)));
	}
	return NormalizeDirectory(FPaths::ProjectDir());
}

FString UAssetImportData::HashFile(const FString& Filename)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
	{
		return FString();
	}
	return FMD5::HashBytes(Bytes.GetData(), static_cast<uint64>(Bytes.Num()));
}

#endif // WITH_EDITORONLY_DATA
