#include "Commandlets/CookCommandlet.h"

#include "Commandlets/ResavePackagesCommandlet.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "LeonEdLog.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogCook, Log, All);

UCookCommandlet::UCookCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HelpDescription = TEXT("Saves packages without their editor-only data into Saved/Cooked (minimal before P16)");
	HelpUsage = TEXT("-run=Cook [-TargetPlatform=<Name>] [-package=<LongPackageName>[,...]] "
					 "[-packagefolder=<LongPackagePath>]");
	LogToConsole = 1;
}

FString UCookCommandlet::GetCookedFilename(const FString& PackageName, const FString& Platform)
{
	FString Root;
	FString Path;
	FString Name;
	if (!FPackageName::SplitLongPackageName(PackageName, Root, Path, Name))
	{
		return FString();
	}
	// "/Engine/" -> Engine, "/Game/" -> the project, any other mount point -> its name (UE's cooked layout).
	FString Folder = Root.Mid(1, Root.Len() - 2);
	if (Folder == TEXT("Game"))
	{
		Folder = FApp::HasProjectName() ? FString(FApp::GetProjectName()) : FString(TEXT("Game"));
	}
	return FPaths::ProjectSavedDir() + TEXT("Cooked/") + Platform + TEXT("/") + Folder + TEXT("/Content/") + Path +
		Name + FPackageName::GetAssetPackageExtension();
}

int32 UCookCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamsMap;
	ParseCommandLine(*Params, Tokens, Switches, ParamsMap);
	const FString* PlatformParam = ParamsMap.Find(TEXT("TargetPlatform"));
	const FString Platform = PlatformParam != nullptr && !PlatformParam->IsEmpty() ? *PlatformParam : FString("Win64");
	TArray<FString> Packages;
	UResavePackagesCommandlet::GatherPackages(ParamsMap, Packages);

	int32 Failures = 0;
	for (const FString& PackageName : Packages)
	{
		UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
		FString CookedFile = GetCookedFilename(PackageName, Platform);
		if (Package == nullptr || CookedFile.IsEmpty())
		{
			UE_LOG(LogCook, Error, "Cook: %s cannot be loaded", *PackageName);
			++Failures;
			continue;
		}
		if (Package->HasAnyPackageFlags(PKG_ContainsMap))
		{
			CookedFile = FPaths::ChangeExtension(CookedFile, FPackageName::GetMapPackageExtension());
		}
		// The import data is editor-only: its properties are filtered out, and its objects stay out of the exports
		// (UE: UAssetImportData::IsEditorOnly). The loaded package is discarded after the save.
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects, true);
		for (UObject* Object : Objects)
		{
			if (Object->IsA<UAssetImportData>())
			{
				Object->SetFlags(RF_Transient);
			}
		}
		Package->SetPackageFlags(PKG_FilterEditorOnly | PKG_Cooked);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(CookedFile), true);
		if (!UPackage::SavePackage(Package, nullptr, RF_Public | RF_Standalone, *CookedFile))
		{
			UE_LOG(LogCook, Error, "Cook: saving %s to '%s' failed", *PackageName, *CookedFile);
			++Failures;
		}
		else
		{
			UE_LOG(LogCook, Display, "Cooked %s -> %s", *PackageName, *CookedFile);
		}
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	UE_LOG(
		LogCook, Display, "Cook (%s): %d of %d packages cooked", *Platform, Packages.Num() - Failures, Packages.Num());
	return Failures == 0 ? 0 : 1;
}
