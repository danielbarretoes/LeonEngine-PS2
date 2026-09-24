#pragma once

#include <string>


/// DCC → cooked `.lmesh` (edit-time / `leon-cook`). Shipping loads `.lmesh` via `LeonMeshFormat`.

[[nodiscard]] bool CookStaticMeshFromObj(const std::string& objPath, const std::string& outLmeshPath,
                                         std::string& outError);

[[nodiscard]] bool CookStaticMeshFromFbx(const std::string& fbxPath, const std::string& outLmeshPath,
                                         std::string& outError);

[[nodiscard]] bool CookStaticMeshFromGltf(const std::string& gltfPath,
                                          const std::string& outLmeshPath,
                                          const std::string& materialsOutDir,
                                          std::string& outError);

