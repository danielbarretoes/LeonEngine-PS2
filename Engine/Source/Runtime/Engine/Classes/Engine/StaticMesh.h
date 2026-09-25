#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"
#include "Serialization/BulkData.h"
#include "StaticMeshResources.h"
#include "UObject/Object.h"
#include "StaticMesh.generated.h"

class UBodySetup;
class UMaterialInterface;

/** A material slot of a static mesh (UE: FStaticMaterial): the material its sections draw with, and the slot's name. */
USTRUCT()
struct ENGINE_API FStaticMaterial
{
	GENERATED_BODY()

	FStaticMaterial() = default;
	explicit FStaticMaterial(UMaterialInterface* InMaterialInterface, FName InMaterialSlotName = NAME_None)
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
 * A static mesh asset (UE: UStaticMesh): its geometry (one LOD: vertices, indices and sections), the bounds, a
 * material per slot and the collision description. The renderer keeps the GPU copy of the geometry, which it makes the
 * first time it draws the mesh (Engine never sees GPU objects), and the physics scene reads the triangles and the body
 * setup for the mesh's bodies.
 *
 * In a package: the tagged properties (the slots, the body setup, an inner object), then the bounds and the geometry
 * as bulk data (at the end of the file, plan decision D13). Leon has no source models, LODs, nanite, sockets, UV
 * channel data or distance fields; BuildFromMeshData takes the place of UE's build from the mesh description.
 */
UCLASS()
class ENGINE_API UStaticMesh : public UObject
{
	GENERATED_BODY()

public:
	UStaticMesh(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The material of each slot, which a section names by index (UE: StaticMaterials). */
	UPROPERTY()
	TArray<FStaticMaterial> StaticMaterials;

	/** The collision (UE: BodySetup): made by BuildFromMeshData, an inner object of the mesh. */
	UPROPERTY()
	UBodySetup* BodySetup = nullptr;

	/**
	 * Builds the mesh from Data (Leon; UE builds from its source models): the vertices, indices and sections (one
	 * section over every index when Data has none), the bounds and, when there is none, the body setup. The slots
	 * (StaticMaterials) are the caller's. False, leaving the mesh as it was, for empty data.
	 */
	bool BuildFromMeshData(const FMeshData& Data);

	/** Makes the body setup if the mesh has none (UE: CreateBodySetup). */
	void CreateBodySetup();

	/** True when the mesh has triangles to draw (UE: HasValidRenderData). */
	[[nodiscard]] bool HasValidRenderData() const
	{
		return LODResources.Indices.Num() > 0 && LODResources.Vertices.Num() > 0;
	}

	/** The geometry (Leon: UE reads RenderData->LODResources[0]). */
	[[nodiscard]] const FStaticMeshLODResources& GetLODResources() const
	{
		return LODResources;
	}

	/** The local bounding box of the vertices (UE: GetBoundingBox). */
	[[nodiscard]] const FBox& GetBoundingBox() const
	{
		return BoundingBox;
	}
	/** The local bounds as box and sphere (UE: GetBounds). */
	[[nodiscard]] FBoxSphereBounds GetBounds() const
	{
		return FBoxSphereBounds(BoundingBox);
	}

	/** UE: GetNumSections (of the only LOD). */
	[[nodiscard]] int32 GetNumSections() const
	{
		return LODResources.Sections.Num();
	}
	/** Triangles of the geometry (UE: GetNumTriangles of LOD 0). */
	[[nodiscard]] int32 GetNumTriangles() const
	{
		return LODResources.GetNumTriangles();
	}

	/** The material of a slot, or null (UE: GetMaterial). */
	[[nodiscard]] UMaterialInterface* GetMaterial(int32 MaterialIndex) const;
	/** UE: GetStaticMaterials. */
	[[nodiscard]] const TArray<FStaticMaterial>& GetStaticMaterials() const
	{
		return StaticMaterials;
	}
	/** UE: GetBodySetup. */
	[[nodiscard]] UBodySetup* GetBodySetup() const
	{
		return BodySetup;
	}

	/** The tagged properties, then the bounds and the geometry (bulk data). */
	void Serialize(FArchive& Ar) override;

private:
	FStaticMeshLODResources LODResources;
	FBox BoundingBox = FBox(FVector::ZeroVector, FVector::ZeroVector);
	/** The geometry in a package: filled from LODResources while saving, read back and emptied while loading. */
	FByteBulkData GeometryBulkData;
};
