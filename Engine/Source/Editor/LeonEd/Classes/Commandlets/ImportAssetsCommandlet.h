#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "ImportAssetsCommandlet.generated.h"

/**
 * Imports source files as assets without an editor (UE: UImportAssetsCommandlet), run as `LeonCook [<Project>.lproj]
 * -run=ImportAssets ...`:
 * - `-source=<file> -dest=<folder>` imports one file into the folder (a long package path, `/Game/Meshes`). The asset
 *   is named after the file with its class prefix (`Cube.glb` is `SM_Cube`), or `-name=<Asset>`. `-type=` picks what
 *   it becomes (Texture, StaticMesh, SkeletalMesh, Animation, Sound, Map), else the file's extension decides. A glTF
 *   imported as Animation makes an `A_<Animation>` per glTF animation next to it (`-AnimationName=` picks one).
 *   Every other `-Key=Value` switch is an import setting of the factory.
 * - `-type=Map -source=<file.glb> -dest=/Game/Maps/<Map>` imports a glTF scene as the map package `-dest` names
 *   (UGLTFMapFactory: its meshes and materials go to `/Game/Maps/<Map>/Meshes` and `/Materials`).
 * - `-importlist=<file.ini>` imports every section of an ImportList.ini: `Source` (relative to the ini's folder),
 *   `Dest`, optional `Name` and `Type`, and the other keys as import settings.
 * - Animation assets without a source file (Docs/PLANS/ps2-shipping.md N25) are made from their description: a section
 *   (or `-type=<Type> -dest=<folder> -name=<Asset>` and the settings as switches) of Type BlendSpace, BlendSpace1D,
 *   AimOffsetBlendSpace1D or AnimMontage without a Source, named after the section, its keys the factory's settings
 *   (UBlendSpaceFactoryNew, UAnimMontageFactory); a key given several times (`+Sample=`) keeps every value, joined
 *   with `;`.
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
	 * name), with the factory Type names (empty: by extension) and Settings, and saves the packages it touched. A map
	 * (Type Map) is the package DestPath itself, named after it. Returns the asset, or null (logged).
	 */
	static UObject* ImportAsset(const FString& SourceFile, const FString& DestPath, const FString& AssetName,
		const FString& Type, const TMap<FString, FString>& Settings);

	/**
	 * Makes the animation asset AssetName of DestPath from a description (Type BlendSpace, BlendSpace1D,
	 * AimOffsetBlendSpace1D or AnimMontage; Settings are the factory's) and saves it. Returns it, or null (logged).
	 */
	static UObject* CreateAsset(
		const FString& DestPath, const FString& AssetName, const FString& Type, const TMap<FString, FString>& Settings);

	/** The factory class that makes Type's assets from a description, or null for a type that imports a file. */
	[[nodiscard]] static UClass* FindCreateFactoryClass(const FString& Type);

	/** Imports every section of an ImportList.ini; the number of failures. */
	static int32 ImportList(const FString& ImportListFile);

	/**
	 * Reimports the assets of the packages PackageNames (empty: every package under the mount points) that have a
	 * source file, and saves them; the number of failures. OutReimported counts the assets reimported.
	 */
	static int32 ReimportPackages(const TArray<FString>& PackageNames, int32* OutReimported = nullptr);
};
