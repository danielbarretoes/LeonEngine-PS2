#include "StaticMeshBuilder.h"

#include "FbxStaticMesh.h"
#include "GltfImport.h"
#include "LeonMeshFormat.h"
#include "MeshData.h"
#include "ObjImport.h"

#include <vector>

bool FStaticMeshBuilder::CookFromObj(const std::string& ObjPath, const std::string& OutLmeshPath, std::string& OutError)
{
	FMeshData Data = LoadObj(ObjPath);
	if (Data.IsEmpty())
	{
		OutError = "Failed to load OBJ: " + ObjPath;
		return false;
	}
	ComputeTangents(Data);
	if (!SaveLeonMeshFile(FString(OutLmeshPath.c_str()), Data))
	{
		OutError = "Failed to write .lmesh: " + OutLmeshPath;
		return false;
	}
	OutError.clear();
	return true;
}

bool FStaticMeshBuilder::CookFromFbx(const std::string& FbxPath, const std::string& OutLmeshPath, std::string& OutError)
{
	FMeshData Data;
	if (!LoadStaticMeshFromFbx(FbxPath, Data))
	{
		OutError = "Failed to load FBX: " + FbxPath;
		return false;
	}
	if (!SaveLeonMeshFile(FString(OutLmeshPath.c_str()), Data))
	{
		OutError = "Failed to write .lmesh: " + OutLmeshPath;
		return false;
	}
	OutError.clear();
	return true;
}

bool FStaticMeshBuilder::CookFromGltf(const std::string& GltfPath, const std::string& OutLmeshPath,
	const std::string& MaterialsOutDir, std::string& OutError)
{
	FMeshData Data;
	std::vector<FGltfImportedMaterial> Mats;
	if (!LoadStaticMeshFromGltf(GltfPath, Data, MaterialsOutDir, &Mats, OutError))
	{
		return false;
	}
	if (!SaveLeonMeshFile(FString(OutLmeshPath.c_str()), Data))
	{
		OutError = "Failed to write .lmesh: " + OutLmeshPath;
		return false;
	}
	OutError.clear();
	return true;
}
