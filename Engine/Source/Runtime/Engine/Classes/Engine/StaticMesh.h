#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"
#include "Serialization/BulkData.h"
#include "StaticMeshResources.h"
#include "UObject/Object.h"
#include "StaticMesh.generated.h"

class UAssetImportData;
class UBodySetup;
class UMaterialInterface;
class UStaticMeshSocket;

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

/** How a LOD is simplified from LOD 0 (UE: FMeshReductionSettings, its PercentTriangles only). */
USTRUCT()
struct ENGINE_API FMeshReductionSettings
{
	GENERATED_BODY()

	/** The share of LOD 0's triangles the LOD keeps at most (UE: PercentTriangles, 0 to 1). */
	UPROPERTY()
	float PercentTriangles = 1.0f;
};

/** A LOD of a static mesh: how it is made and down to which screen size it draws (UE: FStaticMeshSourceModel). */
USTRUCT()
struct ENGINE_API FStaticMeshSourceModel
{
	GENERATED_BODY()

	UPROPERTY()
	FMeshReductionSettings ReductionSettings;

	/**
	 * The screen size from which the LOD is drawn (UE: ScreenSize): the bounding sphere's projected diameter over the
	 * view's height; below the next LOD's, the next LOD draws. LOD 0's is 1.
	 */
	UPROPERTY()
	float ScreenSize = 1.0f;
};

/**
 * A static mesh asset (UE: UStaticMesh): its render data (LODs of LPS2 v2, the quantized strips in VU1's batches,
 * FStaticMeshLODResources), the triangles it collides with (FTriMeshCollisionData, at full precision), the bounds, a
 * material per slot and the collision description. The renderer keeps whatever it derives from the render data
 * (Engine never sees GPU objects); InitResources drops it when the geometry changes (and after a load) and
 * BeginDestroy releases it. The physics scene reads the collision triangles and the body setup for the mesh's bodies.
 *
 * In a package: the tagged properties (the slots, the body setup, an inner object), then the bounds, and the render
 * data and the collision triangles as bulk data (at the end of the file, plan decision D13). Leon has no source
 * models, LODs, nanite, UV channel data or distance fields; BuildFromMeshData takes the place of UE's build from the
 * mesh description. Its sockets (inner UStaticMeshSocket objects) are named points a component attaches to or asks for
 * (a weapon's muzzle).
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

	/** The mesh's sockets, inner objects of the mesh (UE: Sockets). */
	UPROPERTY()
	TArray<UStaticMeshSocket*> Sockets;

	/**
	 * How each LOD is made and when it is drawn (UE: SourceModels, which UE keeps in the editor and cooks into
	 * RenderData->ScreenSize; Leon keeps it at run time for the screen sizes; Docs/PLANS/ps2-shipping.md N15). Empty,
	 * or one entry: one LOD. Entry 0 is LOD 0 (the mesh as imported); each later one a meshoptimizer simplification of
	 * it (meshopt_simplify) to its PercentTriangles, built with the mesh (BuildFromMeshData) and saved with LOD 0.
	 */
	UPROPERTY()
	TArray<FStaticMeshSourceModel> SourceModels;

	/** The socket called InSocketName, or null (UE: FindSocket). */
	[[nodiscard]] UStaticMeshSocket* FindSocket(FName InSocketName) const;

#if WITH_EDITORONLY_DATA
	/** Where the mesh was imported from: made by the factory that imported it, dropped by the cook (UE:
	 * AssetImportData). */
	UPROPERTY(Instanced)
	UAssetImportData* AssetImportData = nullptr;
#endif

	/**
	 * Builds the mesh from Data (Leon; UE builds from its source models): the render data through the mesh builder
	 * (IMeshBuilderModule: MeshUtilities, linked by the editor and the tests only; one section over every index when
	 * Data has none), the collision triangles, the bounds and, when there is none, the body setup. The slots
	 * (StaticMaterials) are the caller's. False, leaving the mesh as it was, for empty data or without a builder.
	 */
	bool BuildFromMeshData(const FMeshData& Data);

	/** Makes the body setup if the mesh has none (UE: CreateBodySetup). */
	void CreateBodySetup();

	/**
	 * The geometry changed: the renderer drops its GPU copy and uploads the mesh again the next time it is drawn (UE:
	 * InitResources, which creates the render resources at once).
	 */
	void InitResources();

	/** Frees the renderer's GPU copy (UE: ReleaseResources). */
	void ReleaseResources();

	/** True when the mesh has triangles to draw (UE: HasValidRenderData). */
	[[nodiscard]] bool HasValidRenderData() const
	{
		return LODResources.Num() > 0 && !LODResources[0].IsEmpty();
	}

	/**
	 * The render data of a LOD (UE: RenderData->LODResources[LODIndex]); LOD 0 is the mesh as imported, an index past
	 * the last LOD the last one.
	 */
	[[nodiscard]] const FStaticMeshLODResources& GetLODResources(int32 LODIndex = 0) const;

	/** The LODs the render data has (UE: GetNumLODs): SourceModels' count, at least 1. */
	[[nodiscard]] int32 GetNumLODs() const
	{
		return LODResources.Num() > 0 ? LODResources.Num() : 1;
	}

	/**
	 * The screen size a LOD is drawn down to (UE: RenderData->ScreenSize[LODIndex]): the bounding sphere's projected
	 * diameter over the view's height, 1 filling it. LOD 0 has 1.
	 */
	[[nodiscard]] float GetLODScreenSize(int32 LODIndex) const;

	/** The triangles the physics scene collides with (UE: GetPhysicsTriMeshData, which fills a copy). */
	[[nodiscard]] const FTriMeshCollisionData& GetPhysicsTriMeshData() const
	{
		return PhysicsTriMeshData;
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

	/** UE: GetNumSections (of LOD 0; every LOD has the same sections). */
	[[nodiscard]] int32 GetNumSections() const
	{
		return GetLODResources().GetNumSections();
	}
	/** Triangles of the geometry (UE: GetNumTriangles of LOD 0). */
	[[nodiscard]] int32 GetNumTriangles() const
	{
		return GetLODResources().GetNumTriangles();
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

	/** The tagged properties, then the bounds, the render data and the collision triangles (bulk data). */
	void Serialize(FArchive& Ar) override;
	/** UE: a loaded mesh initializes its resources. */
	void PostLoad() override;
	/** UE: the resources go with the mesh. */
	void BeginDestroy() override;

private:
	/** One a LOD, LOD 0 first (UE: RenderData->LODResources). */
	TArray<FStaticMeshLODResources> LODResources;
	FTriMeshCollisionData PhysicsTriMeshData;
	FBox BoundingBox = FBox(FVector::ZeroVector, FVector::ZeroVector);
	/**
	 * The render data and the collision triangles in a package: filled from them while saving, read back and emptied
	 * while loading.
	 */
	FByteBulkData GeometryBulkData;
};
