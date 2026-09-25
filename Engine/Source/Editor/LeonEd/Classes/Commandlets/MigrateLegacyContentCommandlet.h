#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "MigrateLegacyContentCommandlet.generated.h"

/**
 * Converts legacy content files into `.lasset` packages (Leon, temporary: P14 part 2 migrates the engine content with
 * it). `LeonCook -run=MigrateLegacyContent -source=<ContentDir>` converts every file under the folder, next to it,
 * into the package its content key names (FLegacyAssetKeys): the folder's package path is its mount point's, or a
 * mount point named after it (`/RenderTest`); leaves get UE's prefixes (`Red.lmat` is `M_Red`) and, under `/Engine`,
 * the legacy `Materials/` and `Textures/` folders are `EngineMaterials/`.
 * - Images (PNG, JPEG, TGA, BMP) become UTexture2D and `.wav` files USoundWave: imported (their import data names the
 *   file, which stays where it is as their source).
 * - `.lmesh` files (version 2) become UStaticMesh and `.lmat` files UMaterial: converted (ULegacyStaticMeshFactory,
 *   ULegacyMaterialFactory), with no import data; the files can be deleted afterwards.
 * Images and sounds go first, so the materials' maps resolve to their packages. `-engine` first saves the engine's
 * procedural assets once (/Engine/EngineResources/DefaultTexture, /Engine/EngineMaterials/T_Default_Bump_N and
 * /Engine/BasicShapes/Cube, Plane, Sphere) from the generators the runtime used before they were packaged. Returns 0
 * when everything converted.
 */
UCLASS()
class LEONED_API UMigrateLegacyContentCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMigrateLegacyContentCommandlet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 Main(const FString& Params) override;

	/** Converts the legacy files under ContentDir; the number of failures. */
	static int32 MigrateDirectory(const FString& ContentDir);

	/** Saves the engine's procedural assets (above); the number of failures. */
	static int32 SaveEngineProceduralAssets();
};
