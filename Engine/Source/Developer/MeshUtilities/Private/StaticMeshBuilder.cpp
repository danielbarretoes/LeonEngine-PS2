#include "StaticMeshBuilder.h"

#include "FbxStaticMesh.h"
#include "GltfImport.h"
#include "LeonMeshFormat.h"
#include "MeshData.h"
#include "MeshUtilitiesLog.h"
#include "ObjImport.h"

DEFINE_LOG_CATEGORY(LogMeshUtilities);

bool FStaticMeshBuilder::CookFromObj(const FString& ObjPath, const FString& OutMeshPath, FString& OutError)
{
	FMeshData Data = LoadObj(ObjPath);
	if (Data.IsEmpty())
	{
		OutError = "Failed to load OBJ: " + ObjPath;
		return false;
	}
	ComputeTangents(Data);
	if (!SaveLeonMeshFile(OutMeshPath, Data))
	{
		OutError = "Failed to write .lmesh: " + OutMeshPath;
		return false;
	}
	OutError.Empty();
	return true;
}

bool FStaticMeshBuilder::CookFromFbx(const FString& FbxPath, const FString& OutMeshPath, FString& OutError)
{
	FMeshData Data;
	if (!LoadStaticMeshFromFbx(FbxPath, Data))
	{
		OutError = "Failed to load FBX: " + FbxPath;
		return false;
	}
	if (!SaveLeonMeshFile(OutMeshPath, Data))
	{
		OutError = "Failed to write .lmesh: " + OutMeshPath;
		return false;
	}
	OutError.Empty();
	return true;
}

bool FStaticMeshBuilder::CookFromGltf(
	const FString& GltfPath, const FString& OutMeshPath, const FString& MaterialsOutDir, FString& OutError)
{
	FMeshData Data;
	TArray<FGltfImportedMaterial> Materials;
	if (!LoadStaticMeshFromGltf(GltfPath, Data, MaterialsOutDir, &Materials, OutError))
	{
		return false;
	}
	if (!SaveLeonMeshFile(OutMeshPath, Data))
	{
		OutError = "Failed to write .lmesh: " + OutMeshPath;
		return false;
	}
	OutError.Empty();
	return true;
}
