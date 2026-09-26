#pragma once

#include "CoreMinimal.h"

struct FMeshData;
class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

/** The static mesh side of the mesh factories (UFbxFactory, UGLTFImportFactory). */
namespace StaticMeshImport
{
	/**
	 * Builds Mesh from Data: the geometry, and one slot per material slot of the source (Data.Materials and the arrays
	 * next to it, or the sections' indices), named after its source material. A slot keeps the material it had when
	 * Mesh is reimported and the slot's name is unchanged (UE); else, with bImportMaterials, a named slot gets the
	 * material asset `M_<Name>` next to the mesh (in the folder MaterialPackagePath when given), made from the source's
	 * values (and its maps imported as `T_` textures next to it) unless it exists already, which is reused as it is.
	 * The source's sockets become the mesh's UStaticMeshSocket objects (`StaticMeshSocket_<Name>`, reused on a
	 * reimport). New assets go to OutNewAssets.
	 */
	void BuildStaticMesh(UStaticMesh& Mesh, const FMeshData& Data, bool bImportMaterials,
		TArray<UObject*>& OutNewAssets, const FString& MaterialPackagePath = FString());

	/**
	 * The texture of an image file next to the asset in AssetPackageName's folder (`T_<File>`): the existing one, or
	 * a new import (added to OutNewAssets); null when the file cannot be read.
	 */
	UTexture2D* FindOrImportTexture(
		const FString& ImageFile, const FString& AssetPackageName, bool bSRGB, TArray<UObject*>& OutNewAssets);
} // namespace StaticMeshImport
