#include "StaticMeshBuilder.h"

#include "GltfImport.h"
#include "MeshData.h"
#include "MeshUtilitiesLog.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogMeshUtilities);

bool FStaticMeshBuilder::IsSupportedExtension(const FString& Extension)
{
	FString Ext = Extension;
	Ext.RemoveFromStart(TEXT("."));
	return Ext == TEXT("gltf") || Ext == TEXT("glb");
}

bool FStaticMeshBuilder::BuildFromFile(const FString& SourcePath, FMeshData& OutData, FString& OutError)
{
	const FString Extension = FPaths::GetExtension(SourcePath);
	if ((Extension == TEXT("gltf")) || (Extension == TEXT("glb")))
	{
		return BuildFromGltf(SourcePath, OutData, OutError);
	}
	OutError = "Not a mesh source (gltf, glb): " + SourcePath;
	return false;
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
