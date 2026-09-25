#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "CookCommandlet.generated.h"

class ITargetPlatform;

/**
 * Cooks the game's content for a target platform (UE: UCookCommandlet, `-run=Cook`, cook by the book):
 *
 *   LeonCook <Project>.lproj -run=Cook -TargetPlatform=Win64|PS2 [-map=<Map>+<Map>] [-package=<Package>,...]
 *
 * 1. Seeds (GatherCookSeeds): the maps (-map=, else `+MapsToCook` of [/Script/UnrealEd.ProjectPackagingSettings],
 *    else every map under /Game/Maps, as UE cooks every map when none is listed), GameDefaultMap and
 *    ServerDefaultMap, the packages under each `+DirectoriesToAlwaysCook=(Path="/Game/...")`, and the engine's default
 *    assets its config names ([/Script/Engine.Engine] DefaultMaterialName, ...), all read from the target platform's
 *    config layers; -package= / -packagefolder= cook only those packages instead.
 * 2. The dependency closure (CollectDependencies): every package reached through the hard imports and the soft
 *    package references of the packages' tables (FLinkerLoad::CreateLinker, nothing is loaded).
 * 3. Each package, sorted, is loaded and saved into <Project>/Saved/Cooked/<Platform>/ (UE's layout:
 *    Engine/Content/..., <Project>/Content/...) with PKG_FilterEditorOnly | PKG_Cooked and the platform's name, so
 *    no editor-only data remains: the editor-only properties, and the import data of the assets and of the maps'
 *    worlds (UAssetImportData::IsEditorOnly).
 * 4. The rest of the build's files (StageNonPackageFiles): the config (the Base, Default and platform ini files, not
 *    the Editor ones), the engine's shaders and the .lproj.
 *
 * The output folder is emptied first, and the output depends only on the content: two cooks give the same bytes.
 * Returns 0 when everything cooked.
 */
UCLASS()
class LEONED_API UCookCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCookCommandlet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 Main(const FString& Params) override;

	/** The packages the cook starts from, sorted and unique (step 1 above). */
	static void GatherCookSeeds(
		const ITargetPlatform& TargetPlatform, const TMap<FString, FString>& ParamsMap, TArray<FString>& OutPackages);

	/**
	 * Seeds and every package they reach through hard imports and soft package references, sorted. False (logged)
	 * when a seed or a hard import has no file; a soft reference to a missing package is a warning.
	 */
	static bool CollectDependencies(const TArray<FString>& Seeds, TArray<FString>& OutPackages);

	/**
	 * Loads a package and saves it cooked for TargetPlatform under CookedDir (step 3 above); false (logged) when it
	 * cannot be loaded or saved. The loaded package is left for the caller's next garbage collection.
	 */
	static bool CookPackage(
		const FString& PackageName, const ITargetPlatform& TargetPlatform, const FString& CookedDir);

	/** Copies the config, the shaders and the .lproj into CookedDir (step 4 above); the file count, -1 on failure. */
	static int32 StageNonPackageFiles(const ITargetPlatform& TargetPlatform, const FString& CookedDir);

	/** <Project>/Saved/Cooked/<Platform>/. */
	static FString GetCookedDir(const FString& PlatformName);

	/**
	 * The cooked file of a package: "/Engine/X" is <CookedDir>Engine/Content/X, "/Game/X"
	 * <CookedDir><Project>/Content/X and any other mount point "/<Root>/X" <CookedDir><Root>/Content/X (UE's layout),
	 * with the extension of the package's file (.lasset unless it is a map); empty for a package under no mount point.
	 */
	static FString GetCookedFilename(
		const FString& PackageName, const FString& CookedDir, const FString& Extension = FString(TEXT(".lasset")));

	/** The folder of the project in the cooked and staged layout: its name, or "Game" without a project. */
	static FString GetProjectFolderName();
};
