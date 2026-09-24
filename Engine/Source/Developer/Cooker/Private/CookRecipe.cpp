#include "CookRecipe.h"

#include <fstream>
#include <iostream>
#include "Animation/CookedSkeletal.h"
#include "StaticMeshBuilder.h"
#include "CookPaths.h"
#include <nlohmann/json.hpp>

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
    const std::string outAbs = FCookPaths::ResolveBeside(baseDir, out);
    const std::string materialsAbs =
        materialsRel.empty() ? std::string{} : FCookPaths::ResolveBeside(baseDir, materialsRel);
    std::string err;
    bool ok = false;
    if (!obj.empty()) {
        const std::string src = FCookPaths::ResolveBeside(baseDir, obj);
        std::cout << "Cook staticmesh OBJ '" << src << "' -> " << outAbs << '\n';
        ok = FStaticMeshBuilder::CookFromObj(src, outAbs, err);
    } else if (!fbx.empty()) {
        const std::string src = FCookPaths::ResolveBeside(baseDir, fbx);
        std::cout << "Cook staticmesh FBX '" << src << "' -> " << outAbs << '\n';
        ok = FStaticMeshBuilder::CookFromFbx(src, outAbs, err);
    } else {
        const std::string src = FCookPaths::ResolveBeside(baseDir, gltf);
        std::cout << "Cook staticmesh glTF '" << src << "' -> " << outAbs << '\n';
        ok = FStaticMeshBuilder::CookFromGltf(src, outAbs, materialsAbs, err);
    }
    if (!ok) {
        std::cerr << "Cook staticmesh failed (step " << stepIndex << "): "
                  << (err.empty() ? "unknown error" : err) << '\n';
        return 2;
    }
    return 0;
}

} // namespace

int FCookRecipe::RunFile(const std::string& recipePath) {
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
            const std::string mesh = FCookPaths::ResolveBeside(baseDir, step.value("mesh", ""));
            const std::string run = FCookPaths::ResolveBeside(baseDir, step.value("run", ""));
            const std::string out = FCookPaths::ResolveBeside(baseDir, step.value("out", "."));
            FCookJumpAnimPaths jump{};
            if (step.contains("jump") && step["jump"].is_string()) {
                jump.JumpStartFbx = FCookPaths::ResolveBeside(baseDir, step["jump"].get<std::string>());
            }
            if (step.contains("fall") && step["fall"].is_string()) {
                jump.FallLoopFbx = FCookPaths::ResolveBeside(baseDir, step["fall"].get<std::string>());
            }
            if (step.contains("land") && step["land"].is_string()) {
                jump.LandFbx = FCookPaths::ResolveBeside(baseDir, step["land"].get<std::string>());
            }
            if (name.empty() || mesh.empty() || run.empty()) {
                std::cerr << "Recipe step " << stepIndex << ": character needs name/mesh/run\n";
                return 1;
            }
            std::cout << "Cook character '" << name << "' -> " << out << '\n';
            if (!CookCharacterFromFbx(name, mesh, run, out, jump)) {
                std::cerr << "Cook character failed (step " << stepIndex << ")\n";
                return 2;
            }
        } else if (type == "anim") {
            const std::string fbx = FCookPaths::ResolveBeside(baseDir, step.value("fbx", ""));
            const std::string skeleton = FCookPaths::ResolveBeside(baseDir, step.value("skeleton", ""));
            const std::string out = FCookPaths::ResolveBeside(baseDir, step.value("out", ""));
            const std::string name = step.value("name", "");
            const bool looping = step.value("loop", true);
            if (fbx.empty() || skeleton.empty() || out.empty()) {
                std::cerr << "Recipe step " << stepIndex << ": anim needs fbx/skeleton/out\n";
                return 1;
            }
            std::cout << "Cook anim '" << name << "' -> " << out << '\n';
            if (!CookAnimSequenceFromFbx(fbx, skeleton, out, name, looping)) {
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

