#include "AssetImportUtils.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "LeonEdLog.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"

FString FAssetImportUtils::GetAssetPrefix(const UClass* Class)
{
	struct FPrefix
	{
		UClass* Class;
		const TCHAR* Prefix;
	};
	const FPrefix Prefixes[] = {
		{UStaticMesh::StaticClass(), TEXT("SM_")},
		{USkeletalMesh::StaticClass(), TEXT("SK_")},
		{USkeleton::StaticClass(), TEXT("SKEL_")},
		{UAnimSequence::StaticClass(), TEXT("A_")},
		{UBlendSpace1D::StaticClass(), TEXT("BS_")},
		{UTexture::StaticClass(), TEXT("T_")},
		{UMaterialInterface::StaticClass(), TEXT("M_")},
		{USoundWave::StaticClass(), TEXT("S_")},
	};
	for (const FPrefix& Entry : Prefixes)
	{
		if (Class != nullptr && Class->IsChildOf(Entry.Class))
		{
			return Entry.Prefix;
		}
	}
	return FString();
}

FString FAssetImportUtils::SanitizeName(const FString& Name)
{
	FString Out;
	Out.Reserve(Name.Len());
	for (int32 Index = 0; Index < Name.Len(); ++Index)
	{
		const TCHAR Char = Name[Index];
		Out.AppendChar(FChar::IsAlnum(Char) || Char == '_' ? Char : '_');
	}
	return Out.IsEmpty() ? FString(TEXT("Asset")) : Out;
}

FString FAssetImportUtils::MakeAssetName(const FString& Prefix, const FString& BaseName)
{
	const FString Name = SanitizeName(BaseName);
	return Prefix.IsEmpty() || Name.StartsWith(Prefix, ESearchCase::CaseSensitive) ? Name : Prefix + Name;
}

FString FAssetImportUtils::MakeAssetName(const UClass* Class, const FString& BaseName)
{
	return MakeAssetName(GetAssetPrefix(Class), BaseName);
}

UObject* FAssetImportUtils::FindOrLoadAsset(UClass* Class, const FString& PackageName, const FString& AssetName)
{
	const FString ObjectPath = PackageName + TEXT(".") + AssetName;
	if (UObject* Found = StaticFindObject(Class, nullptr, *ObjectPath))
	{
		return Found->IsPendingKill() ? nullptr : Found;
	}
	if (!FPackageName::DoesPackageExist(PackageName))
	{
		return nullptr;
	}
	return StaticLoadObject(Class, nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

FString FAssetImportUtils::GetPackageFilename(const FString& PackageName, bool bIsMap)
{
	FString Existing;
	if (FPackageName::DoesPackageExist(PackageName, nullptr, &Existing) && !Existing.IsEmpty())
	{
		return Existing;
	}
	FString Filename;
	if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename,
			bIsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension()))
	{
		return Filename;
	}
	return FString();
}

bool FAssetImportUtils::SavePackage(UPackage* Package, UObject* Asset)
{
	if (Package == nullptr)
	{
		return false;
	}
	const FString Filename = GetPackageFilename(Package->GetName(), UWorld::FindWorldInPackage(Package) != nullptr);
	if (Filename.IsEmpty())
	{
		UE_LOG(LogLeonEd, Error, "Cannot save %s: it is under no mount point", *Package->GetName());
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	if (!UPackage::SavePackage(Package, Asset, RF_Public | RF_Standalone, *Filename))
	{
		UE_LOG(LogLeonEd, Error, "Saving %s to '%s' failed", *Package->GetName(), *Filename);
		return false;
	}
	UE_LOG(LogLeonEd, Log, "Saved %s", *Filename);
	return true;
}

void FAssetImportUtils::FindPackages(const FString& PackagePath, TArray<FString>& OutPackageNames)
{
	FString Root = PackagePath;
	if (!Root.EndsWith(TEXT("/")))
	{
		Root += TEXT("/");
	}
	// "/Root/X" names <Content>/X: its folder is the content folder of the path.
	FString Probe;
	if (!FPackageName::TryConvertLongPackageNameToFilename(Root + TEXT("X"), Probe))
	{
		return;
	}
	const FString Directory = FPaths::GetPath(Probe);
	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		return;
	}
	TArray<FString> Files;
	for (const FString* Extension :
		{&FPackageName::GetAssetPackageExtension(), &FPackageName::GetMapPackageExtension()})
	{
		TArray<FString> Found;
		IFileManager::Get().FindFilesRecursive(Found, *Directory, *(TEXT("*") + *Extension), true, false);
		Files.Append(Found);
	}
	for (const FString& File : Files)
	{
		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(File, PackageName))
		{
			OutPackageNames.AddUnique(PackageName);
		}
	}
	OutPackageNames.Sort();
}

void FAssetImportUtils::GetContentMountPoints(TArray<FString>& OutRoots)
{
	OutRoots.Add(TEXT("/Engine/"));
	if (IFileManager::Get().DirectoryExists(*FPaths::ProjectContentDir()))
	{
		OutRoots.Add(TEXT("/Game/"));
	}
}
