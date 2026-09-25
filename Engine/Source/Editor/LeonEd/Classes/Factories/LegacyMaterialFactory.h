#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "LegacyMaterialFactory.generated.h"

/**
 * Makes a UMaterial of a legacy `.lmat` material (Leon, temporary: MigrateLegacyContent's importer, deleted with it).
 * The `.lmat` keys and defaults are those the runtime read before P14 part 2, so the material draws exactly as the
 * file did: `[Info]` Name / ShadingModel, the parameters (BaseColor, Specular, Metallic, Roughness, Opacity,
 * Shininess, UVScale, CastsShadows, PlanarMirror, Unlit and their aliases) and the `[Textures]` maps. A map is a
 * content key resolved to a texture package (FLegacyAssetKeys, relative to ContentRootPath), or `checker` / `bump`,
 * the engine's DefaultTexture / DefaultBumpNormalTexture. Materials have no source art: the saved M_ asset is the
 * source of truth, and it keeps no import data.
 */
UCLASS()
class LEONED_API ULegacyMaterialFactory : public UFactory
{
	GENERATED_BODY()

public:
	ULegacyMaterialFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * The package path the `.lmat`'s map keys are relative to (the migrated content folder: `/Engine`); empty: the
	 * mount point of the new material's package.
	 */
	UPROPERTY()
	FString ContentRootPath;

	UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context,
		const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled) override;
};
