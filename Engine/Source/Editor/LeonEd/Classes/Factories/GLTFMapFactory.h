#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "GLTFMapFactory.generated.h"

/**
 * Imports a `.gltf` / `.glb` scene as a map (Leon, plan phase P15; UE imports scenes with Datasmith or the glTF
 * importer's level import): `LeonCook [<Project>.lproj] -run=ImportAssets -type=Map -source=<file>
 * -dest=/Game/Maps/<Map>` makes the world of the `.lmap` package `/Game/Maps/<Map>` (AWorldSettings first, then one
 * actor per node, in file order, named after the node), with the meshes and materials it shows next to it:
 *
 * - each glTF mesh a mesh node shows becomes one `SM_<Mesh>` in `/Game/Maps/<Map>/Meshes` (shared by every node that
 *   shows it), without import data of its own (the map's reimport rebuilds it); its PBR materials become `M_<Material>`
 *   in `/Game/Maps/<Map>/Materials`, with their external images as `T_` textures;
 * - the naming rules of UMapImportSettings pick each node's actor: a mesh node an AStaticMeshActor (static, colliding),
 *   `UCX_` convex collision, `COL_` invisible collision, `Clip_` an ABlockingVolume, `PlayerStart` an APlayerStart
 *   (the suffix is its PlayerStartTag), `NavWaypoint` an ANavigationWaypoint (its node's extras `links`: the names of
 *   the waypoints it links to, and `flags`: names; each a JSON array of strings or one comma-separated string), and a
 *   project's rules give trigger volumes their tags (plan decision D15);
 * - KHR_lights_punctual lights become ADirectionalLight / APointLight (a spot light a point light): colour, the glTF
 *   intensity as the light's Intensity, range as AttenuationRadius; the directional light casts shadows;
 * - the scene's axes and units are converted as every glTF import does (FImportCoordinateConversion: right-handed Y up
 *   in metres to the engine's X forward, Y right, Z up, cm).
 *
 * The rules are the config's (GetDefault<UMapImportSettings>). The map's UWorld keeps the source (its AssetImportData):
 * importing over the map, or reimporting it, rebuilds its level from the file in place, so the same file saves the same
 * bytes (gate G5). The import fails when the project's RequiredTags are not met.
 */
UCLASS()
class LEONED_API UGLTFMapFactory
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UGLTFMapFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled) override;

	// FReimportHandler
	bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	EReimportResult::Type Reimport(UObject* Obj) override;
	void GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const override;
};
