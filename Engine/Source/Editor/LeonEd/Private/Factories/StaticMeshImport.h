#pragma once

#include "CoreMinimal.h"

struct FMeshData;
struct FMeshEmbeddedImage;
class UMaterialInterface;
class UObject;
class UStaticMesh;
class UTexture2D;

/** What the mesh factories share (UGLTFImportFactory's static and skeletal meshes, UGLTFMapFactory's meshes). */
namespace StaticMeshImport
{
	/**
	 * Builds Mesh from Data: the geometry, and one slot per material slot of the source (Data.Materials and the arrays
	 * next to it, or the sections' indices), named after its source material. A slot keeps the material it had when
	 * Mesh is reimported and the slot's name is unchanged (UE); else, with bImportMaterials, a named slot gets the
	 * material asset `M_<Name>` next to the mesh (in the folder MaterialPackagePath when given), made from the source's
	 * values (and its maps imported as `T_` textures next to it) unless it exists already, which is reused as it is.
	 * The source's sockets become the mesh's UStaticMeshSocket objects (`StaticMeshSocket_<Name>`, reused on a
	 * reimport). New assets go to OutNewAssets. False when the mesh's render data cannot be built (N29: the reason is
	 * logged with the asset's name, e.g. texture coordinates over a batch's range); the mesh is left as it was.
	 */
	[[nodiscard]] bool BuildStaticMesh(UStaticMesh& Mesh, const FMeshData& Data, bool bImportMaterials,
		TArray<UObject*>& OutNewAssets, const FString& MaterialPackagePath = FString());

	/**
	 * The material asset `M_<SlotName>` of a source slot, in MaterialPackagePath (a folder) or next to the mesh
	 * (MeshPackageName's folder): the existing one, or a new one (added to OutNewAssets) made from Data's values for
	 * Slot, its base colour map an imported `T_` texture (an external file) or one made from embedded bytes.
	 */
	UMaterialInterface* FindOrCreateMaterial(const FString& SlotName, const FMeshData& Data, int32 Slot,
		const FString& MeshPackageName, const FString& MaterialPackagePath, TArray<UObject*>& OutNewAssets);

	/**
	 * The physical material the source names for Slot (Data.PhysicalMaterialNames: the glTF material's extras) goes
	 * on Material when the import made it (`M_<SlotName>`), a new one or an existing one: the source decides it, as the
	 * Blender scripts are the source of the art (plan decision D6). A material that changes is added to OutChanged (to
	 * be saved); a slot whose source names none keeps what its material has. An error when the physical material does
	 * not exist.
	 */
	void ApplyPhysicalMaterial(UMaterialInterface* Material, const FString& SlotName, const FMeshData& Data, int32 Slot,
		TArray<UObject*>& OutChanged);

	/**
	 * The albedo texture of an image file next to the asset in AssetPackageName's folder (`T_<File>`, sRGB): the
	 * existing one, or a new import (added to OutNewAssets); null when the file cannot be read.
	 */
	UTexture2D* FindOrImportTexture(
		const FString& ImageFile, const FString& AssetPackageName, TArray<UObject*>& OutNewAssets);

	/**
	 * The albedo texture of an image embedded in a mesh source (`T_<Image.Name>`, sRGB, next to the asset in
	 * AssetPackageName's folder): the existing one, or a new one decoded from its bytes (added to OutNewAssets),
	 * without import data: the mesh's import makes it. Null when the bytes are not an image.
	 */
	UTexture2D* FindOrCreateEmbeddedTexture(
		const FMeshEmbeddedImage& Image, const FString& AssetPackageName, TArray<UObject*>& OutNewAssets);
} // namespace StaticMeshImport
