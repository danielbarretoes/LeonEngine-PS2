#include "CookRecipe.h"

#include <fstream>
#include <iostream>
#include "Animation/CookedSkeletal.h"
#include "StaticMeshCook.h"
#include "CookPaths.h"
#include <nlohmann/json.hpp>

namespace leon::tools {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] int CookRecipeStaticMesh(const nlohmann::json& step, const fs::path& baseDir,
                                       int stepIndex) {
    const std::string obj = step.value("obj", "");
    const std::string fbx = step.value("fbx", "");
    const std::string gltf = step.value("gltf", "");
    const std::string out = step.value("out", "");
    const std::string materialsRel = step.value("materials", "");
    if (out.empty()) {
        std::cerr << "Recipe step " << stepIndex << ": staticmesh needs out\n";
        return 1;
    }
    const int sources = (!obj.empty() ? 1 : 0) + (!fbx.empty() ? 1 : 0) + (!gltf.empty() ? 1 : 0);
    if (sources != 1) {
        std::cerr << "Recipe step " << stepIndex
                  << ": staticmesh needs exactly one of obj/fbx/gltf\n";
        return 1;
    }
    const std::string outAbs = ResolveBeside(baseDir, out);
    const std::string materialsAbs =
        materialsRel.empty() ? std::string{} : ResolveBeside(baseDir, materialsRel);
    std::string err;
    bool ok = false;
    if (!obj.empty()) {
        const std::string src = ResolveBeside(baseDir, obj);
        std::cout << "Cook staticmesh OBJ '" << src << "' -> " << outAbs << '\n';
        ok = leon::CookStaticMeshFromObj(src, outAbs, err);
    } else if (!fbx.empty()) {
        const std::string src = ResolveBeside(baseDir, fbx);
        std::cout << "Cook staticmesh FBX '" << src << "' -> " << outAbs << '\n';
        ok = leon::CookStaticMeshFromFbx(src, outAbs, err);
    } else {
        const std::string src = ResolveBeside(baseDir, gltf);
        std::cout << "Cook staticmesh glTF '" << src << "' -> " << outAbs << '\n';
        ok = leon::CookStaticMeshFromGltf(src, outAbs, materialsAbs, err);
    }
    if (!ok) {
        std::cerr << "Cook staticmesh failed (step " << stepIndex << "): "
                  << (err.empty() ? "unknown error" : err) << '\n';
        return 2;
    }
    return 0;
}

} // namespace

int RunCookRecipeFile(const std::string& recipePath) {
    std::ifstream in(recipePath);
    if (!in) {
        std::cerr << "Cannot open recipe '" << recipePath << "'\n";
        return 1;
    }

    nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
    if (doc.is_discarded() || !doc.contains("steps") || !doc["steps"].is_array()) {
        std::cerr << "Recipe must be JSON with a \"steps\" array\n";
        return 1;
    }

    const fs::path baseDir = fs::path(recipePath).parent_path();
    int stepIndex = 0;
    for (const auto& step : doc["steps"]) {
        ++stepIndex;
        if (!step.is_object() || !step.contains("type") || !step["type"].is_string()) {
            std::cerr << "Recipe step " << stepIndex << ": missing \"type\"\n";
            return 1;
        }
        const std::string type = step["type"].get<std::string>();
        if (type == "character") {
            const std::string name = step.value("name", "");
            const std::string mesh = ResolveBeside(baseDir, step.value("mesh", ""));
            const std::string run = ResolveBeside(baseDir, step.value("run", ""));
            const std::string out = ResolveBeside(baseDir, step.value("out", "."));
            leon::CookJumpAnimPaths jump{};
            if (step.contains("jump") && step["jump"].is_string()) {
                jump.jumpStartFbx = ResolveBeside(baseDir, step["jump"].get<std::string>());
            }
            if (step.contains("fall") && step["fall"].is_string()) {
                jump.fallLoopFbx = ResolveBeside(baseDir, step["fall"].get<std::string>());
            }
            if (step.contains("land") && step["land"].is_string()) {
                jump.landFbx = ResolveBeside(baseDir, step["land"].get<std::string>());
            }
            if (name.empty() || mesh.empty() || run.empty()) {
                std::cerr << "Recipe step " << stepIndex << ": character needs name/mesh/run\n";
                return 1;
            }
            std::cout << "Cook character '" << name << "' -> " << out << '\n';
            if (!leon::CookCharacterFromFbx(name, mesh, run, out, jump)) {
                std::cerr << "Cook character failed (step " << stepIndex << ")\n";
                return 2;
            }
        } else if (type == "anim") {
            const std::string fbx = ResolveBeside(baseDir, step.value("fbx", ""));
            const std::string skeleton = ResolveBeside(baseDir, step.value("skeleton", ""));
            const std::string out = ResolveBeside(baseDir, step.value("out", ""));
            const std::string name = step.value("name", "");
            const bool looping = step.value("loop", true);
            if (fbx.empty() || skeleton.empty() || out.empty()) {
                std::cerr << "Recipe step " << stepIndex << ": anim needs fbx/skeleton/out\n";
                return 1;
            }
            std::cout << "Cook anim '" << name << "' -> " << out << '\n';
            if (!leon::CookAnimSequenceFromFbx(fbx, skeleton, out, name, looping)) {
                std::cerr << "Cook anim failed (step " << stepIndex << ")\n";
                return 2;
            }
        } else if (type == "staticmesh") {
            const int rc = CookRecipeStaticMesh(step, baseDir, stepIndex);
            if (rc != 0) {
                return rc;
            }
        } else {
            std::cerr << "Recipe step " << stepIndex << ": unknown type '" << type << "'\n";
            return 1;
        }
    }
    return 0;
}

} // namespace leon::tools
