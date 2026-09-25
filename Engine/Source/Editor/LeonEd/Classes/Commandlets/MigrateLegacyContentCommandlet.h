#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "MigrateLegacyContentCommandlet.generated.h"

class UWorld;

/**
 * Converts legacy content files into `.lasset` / `.lmap` packages (Leon, temporary: P14 part 2 migrates the engine
 * content with it, P15 the levels). `LeonCook -run=MigrateLegacyContent -source=<ContentDir>` converts every file under
 * the folder, next to it, into the package its content key names (FLegacyAssetKeys): the folder's package path is its
 * mount point's, or a mount point named after it (`/RenderTest`); leaves get UE's prefixes (`Red.lmat` is `M_Red`)
 * and, under `/Engine`, the legacy `Materials/` and `Textures/` folders are `EngineMaterials/`.
 * - Images (PNG, JPEG, TGA, BMP) become UTexture2D and `.wav` files USoundWave: imported (their import data names the
 *   file, which stays where it is as their source).
 * - `.lmesh` files (version 2) become UStaticMesh and `.lmat` files UMaterial: converted (ULegacyStaticMeshFactory,
 *   ULegacyMaterialFactory), with no import data; the files can be deleted afterwards.
 * - `.llev` levels become maps, last (MigrateLevel): `<Root>/Levels/X.llev` is the map `<Root>/Maps/X`.
 * Images and sounds go first, so the materials' maps resolve to their packages. `-level=<file.llev>
 * -dest=<MapPackage>` converts one level into the map it names (the engine's templates: `/Engine/Maps/Entry`,
 * `/Engine/Maps/Template_Default`). `-engine` first saves the engine's procedural assets once
 * (/Engine/EngineResources/DefaultTexture, /Engine/EngineMaterials/T_Default_Bump_N and /Engine/BasicShapes/Cube,
 * Plane, Sphere) from the generators the runtime used before they were packaged. Returns 0 when everything converted.
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

	/**
	 * Converts a `.llev` level into the map package MapPackageName (a `.lmap`) through the level reader: its actors
	 * with their components, the ACameraActor of its camera framing, and an APlayerStart at the view the framing
	 * opens with, the level's first player start (AGameModeBase::ChoosePlayerStart takes it, so the default pawn starts
	 * where UEngine::LoadMap started it from the `.llev`). A mesh the reader built at run time (a sphere of another
	 * tessellation than the basic shape's) becomes an asset of the map, `<Map>/Meshes/SM_<Mesh>`. The world, or null
	 * (logged).
	 */
	static UWorld* MigrateLevel(const FString& LevelFile, const FString& MapPackageName);
};
