#include "Commandlets/ResavePackagesCommandlet.h"

#include "AssetImportUtils.h"
#include "Engine/World.h"
#include "LeonEdLog.h"
#include "StaticLightingSystem.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

UResavePackagesCommandlet::UResavePackagesCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HelpDescription = TEXT("Loads packages and saves them again in the current format");
	HelpUsage = TEXT("-run=ResavePackages [-package=<LongPackageName>[,...]] [-packagefolder=<LongPackagePath>] "
					 "[-buildlighting]");
	LogToConsole = 1;
}

void UResavePackagesCommandlet::GatherPackages(
	const TMap<FString, FString>& ParamsMap, TArray<FString>& OutPackageNames)
{
	if (const FString* Packages = ParamsMap.Find(TEXT("package")))
	{
		Packages->ParseIntoArray(OutPackageNames, TEXT(","), true);
		return;
	}
	if (const FString* Folder = ParamsMap.Find(TEXT("packagefolder")))
	{
		FAssetImportUtils::FindPackages(*Folder, OutPackageNames);
		return;
	}
	TArray<FString> Roots;
	FAssetImportUtils::GetContentMountPoints(Roots);
	for (const FString& Root : Roots)
	{
		FAssetImportUtils::FindPackages(Root, OutPackageNames);
	}
}

int32 UResavePackagesCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamsMap;
	ParseCommandLine(*Params, Tokens, Switches, ParamsMap);
	TArray<FString> Packages;
	GatherPackages(ParamsMap, Packages);
	// UE's -buildlighting: a map's static lighting is baked again before it is saved (FStaticLightingSystem).
	const bool bBuildLighting = Switches.Contains(TEXT("buildlighting"));

	int32 Failures = 0;
	for (const FString& PackageName : Packages)
	{
		UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
		UWorld* World = Package != nullptr && bBuildLighting ? UWorld::FindWorldInPackage(Package) : nullptr;
		if (World != nullptr)
		{
			(void)FStaticLightingSystem::Build(*World);
		}
		if (Package == nullptr || !FAssetImportUtils::SavePackage(Package))
		{
			UE_LOG(LogLeonEd, Error, "ResavePackages: %s failed", *PackageName);
			++Failures;
		}
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	UE_LOG(LogLeonEd, Display, "ResavePackages: %d of %d packages saved", Packages.Num() - Failures, Packages.Num());
	return Failures == 0 ? 0 : 1;
}
