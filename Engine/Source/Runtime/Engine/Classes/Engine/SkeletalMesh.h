#pragma once

#include "CoreMinimal.h"
#include "LPS2Mesh.h"
#include "Serialization/BulkData.h"
#include "SkeletalAnimation.h"
#include "UObject/Object.h"
#include "SkeletalMesh.generated.h"

class UAssetImportData;
class UMaterialInterface;
class USkeleton;
struct FMeshData;

/** A material slot of a skeletal mesh (UE: FSkeletalMaterial). */
USTRUCT()
struct ENGINE_API FSkeletalMaterial
{
	GENERATED_BODY()

	FSkeletalMaterial() = default;
	explicit FSkeletalMaterial(UMaterialInterface* InMaterialInterface, FName InMaterialSlotName = NAME_None)
		: MaterialInterface(InMaterialInterface)
		, MaterialSlotName(InMaterialSlotName)
	{
	}

	UPROPERTY()
	UMaterialInterface* MaterialInterface = nullptr;

	UPROPERTY()
	FName MaterialSlotName;
};

/**
 * A skinned mesh asset (UE: USkeletalMesh): its skeleton, its material slots, its render data and the bounds. The
 * render data is a skinned LPS2 v2 blob (FLPS2Mesh::IsSkinned: batches of at most 48 vertices, each with a palette of
 * at most 24 bones, two bones and weights a vertex; Docs/ASSET_FORMATS.md), the same on every platform: the GS scene
 * renderer's C++ emitter skins it on the EE and the PC until VU1 does. The bones come from the skeleton asset (Leon
 * keeps no copy of the reference skeleton on the mesh).
 *
 * In a package: the tagged properties, then the bounds, and the render data and each bone's bounds radius as bulk data.
 * Leon has one LOD, no morph targets, cloth or physics asset.
 */
UCLASS()
class ENGINE_API USkeletalMesh : public UObject
{
	GENERATED_BODY()

public:
	USkeletalMesh(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The skeleton the mesh is skinned to (UE: Skeleton). */
	UPROPERTY()
	USkeleton* Skeleton = nullptr;

	/** The material of each slot (UE: Materials); a section draws with the slot its render data names. */
	UPROPERTY()
	TArray<FSkeletalMaterial> Materials;

#if WITH_EDITORONLY_DATA
	/** Where the mesh was imported from: made by the factory that imported it, dropped by the cook (UE:
	 * AssetImportData). */
	UPROPERTY(Instanced)
	UAssetImportData* AssetImportData = nullptr;
#endif

	/**
	 * Builds the render data from bind-pose geometry (its sections name the material slots) and one FSkinWeightInfo a
	 * vertex, skinned to InSkeleton, whose bones the weights index (Leon; UE builds from its import data). Needs the
	 * mesh builder (IMeshBuilderModule: the editor and the tests). Also computes the bounds and each bone's bounds
	 * radius. False, with an error, for empty data, weights past the skeleton or a build that fails.
	 */
	bool BuildFromMeshData(const FMeshData& Mesh, const TArray<FSkinWeightInfo>& SkinWeights, USkeleton* InSkeleton);

	/** True when the mesh has triangles to draw (UE: HasValidRenderData). */
	[[nodiscard]] bool HasValidRenderData() const
	{
		return RenderData.GetNumTriangles() > 0;
	}
	[[nodiscard]] int32 GetNumTriangles() const
	{
		return RenderData.GetNumTriangles();
	}

	/** The skinned LPS2 v2 render data. */
	[[nodiscard]] const FLPS2Mesh& GetRenderData() const
	{
		return RenderData;
	}

	/** The skeleton's bones, or an empty skeleton without one (UE: GetRefSkeleton). */
	[[nodiscard]] const FReferenceSkeleton& GetRefSkeleton() const;

	/** The local bounding box of the bind pose (UE: GetImportedBounds().GetBox()). */
	[[nodiscard]] const FBox& GetBoundingBox() const
	{
		return BoundingBox;
	}

	/**
	 * Each bone's bounds radius (Leon): the farthest vertex it moves, from the bone's origin in its bind space (0 for a
	 * bone that moves none). A pose's bounds are the spheres of these radii around the posed bones (GetPoseBounds).
	 */
	[[nodiscard]] const TArray<float>& GetBoneBoundsRadii() const
	{
		return BoneBoundsRadii;
	}

	/**
	 * The bounds of a pose in the mesh's space: around each posed bone that moves vertices, its radius times the bone's
	 * largest scale. Every skinned vertex is inside: it is a weighted average of points that are. The bind-pose box
	 * when the pose has another number of bones.
	 */
	[[nodiscard]] FBox GetPoseBounds(const TArray<FMatrix>& ComponentSpaceTransforms) const;

	/** Uniform scale that makes the mesh FitHeight tall (its Z extent, world units). */
	[[nodiscard]] float FitUniformScale(float FitHeight) const;

	/** The material of a slot, or null (UE: GetMaterial via Materials). */
	[[nodiscard]] UMaterialInterface* GetMaterial(int32 MaterialIndex) const;

	/** The geometry changed: the renderer's GPU copy is dropped and made again when drawn (UE: InitResources). */
	void InitResources();
	/** Frees the renderer's GPU copy (UE: ReleaseResources). */
	void ReleaseResources();

	/** The tagged properties, then the bounds and the render data (bulk data). */
	void Serialize(FArchive& Ar) override;
	void PostLoad() override;
	void BeginDestroy() override;

private:
	FLPS2Mesh RenderData;
	TArray<float> BoneBoundsRadii;
	FBox BoundingBox = FBox(FVector::ZeroVector, FVector::ZeroVector);
	/** The render data in a package: filled while saving, read back and emptied while loading. */
	FByteBulkData GeometryBulkData;
};
