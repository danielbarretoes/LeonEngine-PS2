#include "CookRecipe.h"

#include <fstream>
#include <iostream>
#include "Animation/CookedSkeletal.h"
#include "StaticMeshBuilder.h"
#include "CookPaths.h"
#include <nlohmann/json.hpp>

namespace {

namespace fs = std::filesystem;

[[nodiscard]] int CookRecipeStaticMesh(const nlohmann::json& Step, const fs::path& BaseDir,
                                       int StepIndex) {
    const std::string Obj = Step.value("obj", "");
    const std::string Fbx = Step.value("fbx", "");
    const std::string Gltf = Step.value("gltf", "");
    const std::string Out = Step.value("out", "");
    const std::string MaterialsRel = Step.value("materials", "");
    if (Out.empty()) {
        std::cerr << "Recipe step " << StepIndex << ": staticmesh needs out\n";
        return 1;
    }
    const int Sources = (!Obj.empty() ? 1 : 0) + (!Fbx.empty() ? 1 : 0) + (!Gltf.empty() ? 1 : 0);
    if (Sources != 1) {
        std::cerr << "Recipe step " << StepIndex
                  << ": staticmesh needs exactly one of obj/fbx/gltf\n";
        return 1;
    }
    const std::string OutAbs = FCookPaths::ResolveBeside(BaseDir, Out);
    const std::string MaterialsAbs =
        MaterialsRel.empty() ? std::string{} : FCookPaths::ResolveBeside(BaseDir, MaterialsRel);
    std::string Err;
    bool bOk = false;
    if (!Obj.empty()) {
        const std::string Src = FCookPaths::ResolveBeside(BaseDir, Obj);
        std::cout << "Cook staticmesh OBJ '" << Src << "' -> " << OutAbs << '\n';
        bOk = FStaticMeshBuilder::CookFromObj(Src, OutAbs, Err);
    } else if (!Fbx.empty()) {
        const std::string Src = FCookPaths::ResolveBeside(BaseDir, Fbx);
        std::cout << "Cook staticmesh FBX '" << Src << "' -> " << OutAbs << '\n';
        bOk = FStaticMeshBuilder::CookFromFbx(Src, OutAbs, Err);
    } else {
        const std::string Src = FCookPaths::ResolveBeside(BaseDir, Gltf);
        std::cout << "Cook staticmesh glTF '" << Src << "' -> " << OutAbs << '\n';
        bOk = FStaticMeshBuilder::CookFromGltf(Src, OutAbs, MaterialsAbs, Err);
    }
    if (!bOk) {
        std::cerr << "Cook staticmesh failed (step " << StepIndex << "): "
                  << (Err.empty() ? "unknown error" : Err) << '\n';
        return 2;
    }
    return 0;
}

} // namespace

int FCookRecipe::RunFile(const std::string& RecipePath) {
    std::ifstream In(RecipePath);
    if (!In) {
        std::cerr << "Cannot open recipe '" << RecipePath << "'\n";
        return 1;
    }

    nlohmann::json Doc = nlohmann::json::parse(In, nullptr, false);
    if (Doc.is_discarded() || !Doc.contains("steps") || !Doc["steps"].is_array()) {
        std::cerr << "Recipe must be JSON with a \"steps\" array\n";
        return 1;
    }

    const fs::path BaseDir = fs::path(RecipePath).parent_path();
    int StepIndex = 0;
    for (const auto& Step : Doc["steps"]) {
        ++StepIndex;
        if (!Step.is_object() || !Step.contains("type") || !Step["type"].is_string()) {
            std::cerr << "Recipe step " << StepIndex << ": missing \"type\"\n";
            return 1;
        }
        const std::string Type = Step["type"].get<std::string>();
        if (Type == "character") {
            const std::string Name = Step.value("name", "");
            const std::string Mesh = FCookPaths::ResolveBeside(BaseDir, Step.value("mesh", ""));
            const std::string Run = FCookPaths::ResolveBeside(BaseDir, Step.value("run", ""));
            const std::string Out = FCookPaths::ResolveBeside(BaseDir, Step.value("out", "."));
            FCookJumpAnimPaths Jump{};
            if (Step.contains("jump") && Step["jump"].is_string()) {
                Jump.JumpStartFbx = FCookPaths::ResolveBeside(BaseDir, Step["jump"].get<std::string>());
            }
            if (Step.contains("fall") && Step["fall"].is_string()) {
                Jump.FallLoopFbx = FCookPaths::ResolveBeside(BaseDir, Step["fall"].get<std::string>());
            }
            if (Step.contains("land") && Step["land"].is_string()) {
                Jump.LandFbx = FCookPaths::ResolveBeside(BaseDir, Step["land"].get<std::string>());
            }
            if (Name.empty() || Mesh.empty() || Run.empty()) {
                std::cerr << "Recipe step " << StepIndex << ": character needs name/mesh/run\n";
                return 1;
            }
            std::cout << "Cook character '" << Name << "' -> " << Out << '\n';
            if (!CookCharacterFromFbx(Name, Mesh, Run, Out, Jump)) {
                std::cerr << "Cook character failed (step " << StepIndex << ")\n";
                return 2;
            }
        } else if (Type == "anim") {
            const std::string Fbx = FCookPaths::ResolveBeside(BaseDir, Step.value("fbx", ""));
            const std::string Skeleton = FCookPaths::ResolveBeside(BaseDir, Step.value("skeleton", ""));
            const std::string Out = FCookPaths::ResolveBeside(BaseDir, Step.value("out", ""));
            const std::string Name = Step.value("name", "");
            const bool bLooping = Step.value("loop", true);
            if (Fbx.empty() || Skeleton.empty() || Out.empty()) {
                std::cerr << "Recipe step " << StepIndex << ": anim needs fbx/skeleton/out\n";
                return 1;
            }
            std::cout << "Cook anim '" << Name << "' -> " << Out << '\n';
            if (!CookAnimSequenceFromFbx(Fbx, Skeleton, Out, Name, bLooping)) {
                std::cerr << "Cook anim failed (step " << StepIndex << ")\n";
                return 2;
            }
        } else if (Type == "staticmesh") {
            const int Rc = CookRecipeStaticMesh(Step, BaseDir, StepIndex);
            if (Rc != 0) {
                return Rc;
            }
        } else {
            std::cerr << "Recipe step " << StepIndex << ": unknown type '" << Type << "'\n";
            return 1;
        }
    }
    return 0;
}

