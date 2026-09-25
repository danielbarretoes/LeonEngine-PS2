#include "StaticMeshBuilder.h"

#include "FbxStaticMesh.h"
#include "GltfImport.h"
#include "MeshData.h"
#include "MeshUtilitiesLog.h"
#include "Misc/Paths.h"
#include "ObjImport.h"

DEFINE_LOG_CATEGORY(LogMeshUtilities);

bool FStaticMeshBuilder::IsSupportedExtension(const FString& Extension)
{
	FString Ext = Extension;
	Ext.RemoveFromStart(TEXT("."));
	return Ext == TEXT("obj") || Ext == TEXT("fbx") || Ext == TEXT("gltf") || Ext == TEXT("glb");
}

bool FStaticMeshBuilder::BuildFromFile(const FString& SourcePath, FMeshData& OutData, FString& OutError)
{
	const FString Extension = FPaths::GetExtension(SourcePath);
	if (Extension == TEXT("obj"))
	{
		return BuildFromObj(SourcePath, OutData, OutError);
	}
	if (Extension == TEXT("fbx"))
	{
		return BuildFromFbx(SourcePath, OutData, OutError);
	}
	if ((Extension == TEXT("gltf")) || (Extension == TEXT("glb")))
	{
		return BuildFromGltf(SourcePath, OutData, OutError);
	}
	OutError = "Not a mesh source (obj, fbx, gltf, glb): " + SourcePath;
	return false;
}

bool FStaticMeshBuilder::BuildFromObj(const FString& ObjPath, FMeshData& OutData, FString& OutError)
{
	OutData = LoadObj(ObjPath);
	if (OutData.IsEmpty())
	{
		OutError = "Failed to load OBJ: " + ObjPath;
		return false;
	}
	ComputeTangents(OutData, EMeshDataBasis::Engine);
	OutError.Empty();
	return true;
}

bool FStaticMeshBuilder::BuildFromFbx(const FString& FbxPath, FMeshData& OutData, FString& OutError)
{
	if (!LoadStaticMeshFromFbx(FbxPath, OutData))
	{
		OutError = "Failed to load FBX: " + FbxPath;
		return false;
	}
	OutError.Empty();
	return true;
}

bool FStaticMeshBuilder::BuildFromGltf(const FString& GltfPath, FMeshData& OutData, FString& OutError)
{
	if (!LoadStaticMeshFromGltf(GltfPath, OutData, OutError))
	{
		return false;
	}
	OutError.Empty();
	return true;
}
