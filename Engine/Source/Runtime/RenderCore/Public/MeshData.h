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

/** An image embedded in a mesh's source (a .glb's buffer view, a `data:` URI): its name and its encoded file bytes. */
struct RENDERCORE_API FMeshEmbeddedImage
{
	/** The source's name for it (glTF: the image's name), without an extension. */
	FString Name;
	/** The PNG / JPEG bytes as the source holds them; empty for no embedded image. */
	TArray<uint8> EncodedData;
};

/**
 * CPU-side mesh data (no OpenGL handles): what the importers (MeshUtilities) read from a source file and UStaticMesh
 * builds from. Materials and the arrays after it are parallel, one entry per material slot; the mesh factories make a
 * UMaterial asset of each named slot.
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
	/** Each slot's embedded base colour map (a .glb's image), or one without bytes. */
	TArray<FMeshEmbeddedImage> AlbedoMapImages;
	/**
	 * Each slot's physical material, as the source names it (a glTF material's extras: `{"physMaterial":
	 * "/Game/PhysicalMaterials/PM_Wood"}`, a long package name or object path), or empty.
	 */
	TArray<FString> PhysicalMaterialNames;
	/** The mesh's sockets, in the source's order (UE: the FBX importer's SOCKET_ nodes). */
	TArray<FMeshSocketData> Sockets;

	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.Num() == 0 || Indices.Num() == 0;
	}
};
