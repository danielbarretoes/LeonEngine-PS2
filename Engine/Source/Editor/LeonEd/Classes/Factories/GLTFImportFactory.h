#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "GLTFImportFactory.generated.h"

class USkeleton;
struct FStaticMeshSourceModel;

/** What a glTF file is imported as (Leon; UE's FBX importer: EFBXImportType). */
UENUM()
enum class EGLTFImportType : uint8
{
	/** A UStaticMesh (SM_) of every triangle primitive. */
	StaticMesh,
	/** A USkeletalMesh (SK_) of the skinned meshes, on Skeleton or a new SKEL_ skeleton. */
	SkeletalMesh,
	/** A UAnimSequence (A_) per glTF animation, on Skeleton. */
	Animation,
};

/**
 * Imports `.gltf` / `.glb` files (UE: UGLTFImportFactory, the glTF Importer plugin), through cgltf in MeshUtilities,
 * glTF's right-handed Y-up metres converted to the engine world. glTF is the only mesh and animation format
 * (Docs/PLANS/ps2-shipping.md D11). ImportType (the import commandlet's `-type=` / ImportList.ini `Type=`) picks what:
 *
 * - StaticMesh: every triangle primitive merged into one UStaticMesh, one section and material slot per primitive;
 *   `SOCKET_<Name>` nodes become its sockets.
 * - SkeletalMesh: the skinned meshes as a USkeletalMesh (skinned LPS2 v2, two bones a vertex) on Skeleton, the
 *   existing mesh's skeleton on a reimport, or `SKEL_<Name>` (NewSkeletonName) next to the mesh (made when it does
 *   not exist); its bones
 *   must match the file's (names, order, parents). `SOCKET_<Name>` nodes under a joint become the skeleton's sockets.
 * - Animation: one UAnimSequence `A_<AnimationName>` per glTF animation, next to the asset asked for, on Skeleton (or
 *   the existing clip's); the file's bones must match it (names, order, parents). With AnimationName set, only that
 *   animation, as the asset asked for (what a reimport does: every clip records its animation's name).
 *
 * With bImportMaterials each named glTF material becomes an `M_<Name>` UMaterial next to the mesh (base colour,
 * opacity) with its base colour image as a `T_` texture: an external file is imported (and reimported) on its own; an
 * image embedded in a .glb or a data URI becomes `T_<ImageName>` made from its bytes, without import data (the mesh's
 * import makes it). A glTF scene imported as a map is UGLTFMapFactory's (`-type=Map`).
 */
UCLASS()
class LEONED_API UGLTFImportFactory
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UGLTFImportFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** What to make. ImportList.ini: `Type=SkeletalMesh` (the commandlet sets `ImportType`). */
	UPROPERTY()
	EGLTFImportType ImportType = EGLTFImportType::StaticMesh;

	/** The skeleton of a skeletal mesh or an animation (UE: ImportUI->Skeleton). ImportList.ini: an object path. */
	UPROPERTY()
	USkeleton* Skeleton = nullptr;

	/**
	 * The name of the skeleton a skeletal mesh without Skeleton makes next to itself (Leon; UE names it
	 * `<Mesh>_Skeleton`, renamed by hand in its editor): empty for `SKEL_<Name>` of the mesh's. The first of several
	 * meshes that share a skeleton names it (`SK_Body_CT` makes `SKEL_Body`), the others give it as Skeleton.
	 */
	UPROPERTY()
	FString NewSkeletonName;

	/** The one glTF animation to import (empty: every one). A reimport of a clip names its own. */
	UPROPERTY()
	FString AnimationName;

	/** Makes the materials (Leon; UE's glTF importer always does). */
	UPROPERTY()
	bool bImportMaterials = true;

	/**
	 * A static mesh's LODs after LOD 0 (Docs/PLANS/ps2-shipping.md N15; UE: the LOD settings of its import options):
	 * `<PercentTriangles>@<ScreenSize>` each, comma separated, in order (ImportList.ini `LODs=0.5@0.3,0.25@0.1`: LOD 1
	 * keeps half the triangles and draws below 0.3 of the view's height, LOD 2 a quarter below 0.1). Empty: one LOD.
	 */
	UPROPERTY()
	FString LODs;

	/**
	 * The source models of LODs (the mesh's UStaticMesh::SourceModels, LOD 0 first); false with OutError for an entry
	 * that is not `<share>@<size>` with a share in (0, 1] and sizes going down.
	 */
	static bool ParseLODs(const FString& InLODs, TArray<FStaticMeshSourceModel>& OutSourceModels, FString& OutError);

	UClass* ResolveSupportedClass() override;
	bool DoesSupportClass(UClass* Class) override;
	UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled) override;

	// FReimportHandler
	bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	EReimportResult::Type Reimport(UObject* Obj) override;
	void GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const override;

private:
	UObject* ImportStaticMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename);
	UObject* ImportSkeletalMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename);
	UObject* ImportAnimations(UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename);
};
