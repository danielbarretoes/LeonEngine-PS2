#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "Vertex.h"

/** Contiguous index range drawn with one material slot. */
struct RENDERCORE_API FMeshSection
{
	int32 IndexOffset = 0; // in indices (not bytes)
	int32 IndexCount = 0;
	int32 MaterialIndex = 0;
};

/** A named point of a mesh from its source (a glTF `SOCKET_<Name>` node): UStaticMesh's UStaticMeshSocket. */
struct RENDERCORE_API FMeshSocketData
{
	/** The socket's name, without the source's prefix. */
	FString Name;
	/** The socket in the mesh's space, in the engine's axes and centimetres. */
	FTransform Transform;
};

/**
 * CPU-side mesh data (no OpenGL handles): what the importers (MeshUtilities) read from a source file and UStaticMesh
 * builds from. Materials and the arrays after it are parallel, one entry per material slot; the static mesh factories
 * make a UMaterial asset of each named slot.
 */
struct RENDERCORE_API FMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FMeshSection> Submeshes;
	/** Each slot's material values from the source (the OBJ `.mtl`, the glTF material). */
	TArray<FMaterial> Materials;
	/** Each slot's material name in the source; empty for a slot the source gives no material. */
	TArray<FString> MaterialSlotNames;
	/** Each slot's base colour map: the image file the source names (absolute), or empty. */
	TArray<FString> AlbedoMapPaths;
	/** Each slot's normal map: the image file the source names (absolute), or empty. */
	TArray<FString> NormalMapPaths;
	/** The mesh's sockets, in the source's order (UE: the FBX importer's SOCKET_ nodes). */
	TArray<FMeshSocketData> Sockets;

	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.Num() == 0 || Indices.Num() == 0;
	}
};

/**
 * Orthonormalizes tangents from triangle UVs (needed for normal mapping). A vertex without a UV gradient gets a tangent
 * across the world up (UE: Z up, left-handed): the tangents the legacy Y-up meshes had, converted (the golden tests
 * compare with FLegacyCoordinateConversion::ComputeLegacyTangents).
 */
void ComputeTangents(FMeshData& Data);
