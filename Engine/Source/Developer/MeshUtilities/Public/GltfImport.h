#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/**
 * Loads the meshes of a .gltf / .glb (all primitives merged, one section and material slot per primitive) into
 * FMeshData, in the engine world. Each slot takes its glTF material: the name, the PBR factors and the paths of the
 * external base colour and normal images (embedded images are not read). A node named `SOCKET_<Name>` (UE's FBX
 * convention) becomes a socket <Name>, placed relative to the nearest ancestor node with a mesh (its world transform
 * without one). Edit time only (UGLTFImportFactory).
 */
[[nodiscard]] MESHUTILITIES_API bool LoadStaticMeshFromGltf(const FString& Path, FMeshData& Out, FString& OutError);
