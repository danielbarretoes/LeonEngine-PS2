#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "ImportAssetsCommandlet.generated.h"

/**
 * Imports source files as assets without an editor (UE: UImportAssetsCommandlet), run as `LeonCook [<Project>.lproj]
 * -run=ImportAssets ...`:
 * - `-source=<file> -dest=<folder>` imports one file into the folder (a long package path, `/Game/Meshes`). The asset
 *   is named after the file with its class prefix (`Cube.obj` is `SM_Cube`), or `-name=<Asset>`. `-type=` picks what
 *   it becomes (Texture, StaticMesh, SkeletalMesh, Animation, Sound, Material), else the file's extension decides.
 *   Every other `-Key=Value` switch is an import setting of the factory.
 * - `-importlist=<file.ini>` imports every section of an ImportList.ini: `Source` (relative to the ini's folder),
 *   `Dest`, optional `Name` and `Type`, and the other keys as import settings.
 * - `-reimport -all` reimports every asset under the mount points (`/Engine`, and `/Game` with a project) whose
 *   UAssetImportData names a source file that exists; `-reimport -package=<LongPackageName>[,...]` only those.
 * An existing asset is imported over in place, so references to it stay valid. Every asset the import makes or
 * changes is saved (deterministically: importing the same file again writes the same bytes, gate G5). Returns 0 when
 * everything imported, 1 otherwise.
 */
UCLASS()
class LEONED_API UImportAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UImportAssetsCommandlet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 Main(const FString& Params) override;

	/**
	 * Imports SourceFile as an asset of DestPath (a long package path), named AssetName (empty: the prefixed file
	 * name), with the factory Type names (empty: by extension) and Settings, and saves the packages it touched.
	 * Returns the asset, or null (logged).
	 */
	static UObject* ImportAsset(const FString& SourceFile, const FString& DestPath, const FString& AssetName,
		const FString& Type, const TMap<FString, FString>& Settings);

	/** Imports every section of an ImportList.ini; the number of failures. */
	static int32 ImportList(const FString& ImportListFile);

	/**
	 * Reimports the assets of the packages PackageNames (empty: every package under the mount points) that have a
	 * source file, and saves them; the number of failures. OutReimported counts the assets reimported.
	 */
	static int32 ReimportPackages(const TArray<FString>& PackageNames, int32* OutReimported = nullptr);
};
