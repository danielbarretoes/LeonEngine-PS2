#pragma once

#include "CoreMinimal.h"
#include "Serialization/BulkData.h"
#include "SkeletalAnimation.h"
#include "UObject/Object.h"
#include "SkeletalMesh.generated.h"

class UMaterialInterface;
class USkeleton;

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
 * A skinned mesh asset (UE: USkeletalMesh): its skeleton, its material slots, the skinned vertices (bone indices and
 * weights) and indices, and the bounds. The renderer keeps the GPU copy, which it makes the first time it draws the
 * mesh; the bones come from the skeleton asset (Leon keeps no copy of the reference skeleton on the mesh).
 *
 * In a package: the tagged properties, then the bounds and the vertices and indices as bulk data. Leon has one LOD
 * and one section drawn with slot 0, no morph targets, cloth or physics asset.
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

	/** The material of each slot (UE: Materials). */
	UPROPERTY()
	TArray<FSkeletalMaterial> Materials;

	/**
	 * Takes the geometry and the bounds of imported data (Leon; UE builds from its import data), skinned to
	 * InSkeleton. The skeleton's bones must be those the data's vertices index (the FBX import's RefSkeleton). False
	 * for empty data.
	 */
	bool BuildFromImportData(const FSkeletalMeshData& Data, USkeleton* InSkeleton);

	/** True when the mesh has triangles to draw (UE: HasValidRenderData). */
	[[nodiscard]] bool HasValidRenderData() const
	{
		return Indices.Num() > 0 && Vertices.Num() > 0;
	}
	[[nodiscard]] int32 GetNumTriangles() const
	{
		return Indices.Num() / 3;
	}

	/** The skeleton's bones, or an empty skeleton without one (UE: GetRefSkeleton). */
	[[nodiscard]] const FReferenceSkeleton& GetRefSkeleton() const;

	/** The local bounding box (UE: GetImportedBounds().GetBox()). */
	[[nodiscard]] const FBox& GetBoundingBox() const
	{
		return BoundingBox;
	}
	/** Uniform scale that makes the mesh FitHeight tall (its Z extent, world units). */
	[[nodiscard]] float FitUniformScale(float FitHeight) const;

	/** The material of a slot, or null (UE: GetMaterial via Materials). */
	[[nodiscard]] UMaterialInterface* GetMaterial(int32 MaterialIndex) const;

	/** The skinned vertices and the indices the renderer uploads. */
	[[nodiscard]] const TArray<FSkeletalVertex>& GetVertices() const
	{
		return Vertices;
	}
	[[nodiscard]] const TArray<uint32>& GetIndices() const
	{
		return Indices;
	}

	/** The tagged properties, then the bounds and the geometry (bulk data). */
	void Serialize(FArchive& Ar) override;

private:
	TArray<FSkeletalVertex> Vertices;
	TArray<uint32> Indices;
	FBox BoundingBox = FBox(FVector::ZeroVector, FVector::ZeroVector);
	/** The geometry in a package: filled while saving, read back and emptied while loading. */
	FByteBulkData GeometryBulkData;
};
