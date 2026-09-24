#include "StaticMeshBuilder.h"

#include "FbxStaticMesh.h"
#include "GltfImport.h"
#include "ObjImport.h"
#include "LeonMeshFormat.h"
#include "MeshData.h"
#include <vector>


bool FStaticMeshBuilder::CookFromObj(const std::string& objPath, const std::string& outLmeshPath,
                           std::string& outError) {
    FMeshData data = LoadObj(objPath);
    if (data.empty()) {
        outError = "Failed to load OBJ: " + objPath;
        return false;
    }
    ComputeTangents(data);
    if (!SaveLeonMeshFile(outLmeshPath, data)) {
        outError = "Failed to write .lmesh: " + outLmeshPath;
        return false;
    }
    outError.clear();
    return true;
}

bool FStaticMeshBuilder::CookFromFbx(const std::string& fbxPath, const std::string& outLmeshPath,
                           std::string& outError) {
    FMeshData data;
    if (!LoadStaticMeshFromFbx(fbxPath, data)) {
        outError = "Failed to load FBX: " + fbxPath;
        return false;
    }
    if (!SaveLeonMeshFile(outLmeshPath, data)) {
        outError = "Failed to write .lmesh: " + outLmeshPath;
        return false;
    }
    outError.clear();
    return true;
}

bool FStaticMeshBuilder::CookFromGltf(const std::string& gltfPath, const std::string& outLmeshPath,
                            const std::string& materialsOutDir, std::string& outError) {
    FMeshData data;
    std::vector<FGltfImportedMaterial> mats;
    if (!LoadStaticMeshFromGltf(gltfPath, data, materialsOutDir, &mats, outError)) {
        return false;
    }
    if (!SaveLeonMeshFile(outLmeshPath, data)) {
        outError = "Failed to write .lmesh: " + outLmeshPath;
        return false;
    }
    outError.clear();
    return true;
}

