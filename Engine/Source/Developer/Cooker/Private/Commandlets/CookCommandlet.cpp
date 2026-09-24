#include "Commandlets/CookCommandlet.h"

#include <iostream>
#include "Animation/CookedSkeletal.h"
#include "StaticMeshBuilder.h"
#include "CookRecipe.h"
#include <string>

namespace {

void printUsage() {
    std::cout
        << "leon-cook — cook meshes / skeletal assets for Leon\n\n"
        << "Usage:\n"
        << "  leon-cook staticmesh --obj <mesh.obj>|--fbx <m.fbx>|--gltf <m.gltf> --out <m.lmesh>\n"
        << "    [--materials <dir>]  (glTF: write .lmat + textures)\n\n"
        << "  leon-cook character --name <Name> --mesh <idle.fbx> --run <run.fbx> --out <dir>\n"
        << "    [--jump <JumpingUp.fbx>] [--fall <FallingIdle.fbx>] [--land <Land.fbx>]\n\n"
        << "  leon-cook anim --fbx <clip.fbx> --skeleton <Bot.lskel>\n"
        << "    --name <ClipName> --out <Anims/Clip.lanim> [--noloop]\n\n"
        << "  leon-cook recipe <file.json>\n"
        << "    Runs steps from a recipe; relative paths resolve next to the JSON file.\n"
        << "    Step types: character | anim | staticmesh\n\n"
        << "Writes (staticmesh):\n"
        << "  binary .lmesh (LMSH)\n"
        << "Writes (character):\n"
        << "  <Name>.lskel / .lskm / Materials / Anims / blendspace / .lchar\n"
        << "Writes (anim):\n"
        << "  <out>.lanim\n";
}

[[nodiscard]] const char* argValue(int argc, char** argv, int& i) {
    if (i + 1 >= argc || argv[i + 1] == nullptr || argv[i + 1][0] == '\0') {
        return nullptr;
    }
    ++i;
    return argv[i];
}

[[nodiscard]] int cookStaticMesh(int argc, char** argv) {
    std::string obj;
    std::string fbx;
    std::string gltf;
    std::string out;
    std::string materials;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--obj") {
            if (const char* v = argValue(argc, argv, i)) {
                obj = v;
            } else {
                std::cerr << "--obj requires a path\n";
                return 1;
            }
        } else if (a == "--fbx") {
            if (const char* v = argValue(argc, argv, i)) {
                fbx = v;
            } else {
                std::cerr << "--fbx requires a path\n";
                return 1;
            }
        } else if (a == "--gltf") {
            if (const char* v = argValue(argc, argv, i)) {
                gltf = v;
            } else {
                std::cerr << "--gltf requires a path\n";
                return 1;
            }
        } else if (a == "--materials") {
            if (const char* v = argValue(argc, argv, i)) {
                materials = v;
            } else {
                std::cerr << "--materials requires a path\n";
                return 1;
            }
        } else if (a == "--out") {
            if (const char* v = argValue(argc, argv, i)) {
                out = v;
            } else {
                std::cerr << "--out requires a path\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown arg '" << a << "'\n";
            return 1;
        }
    }
    if (out.empty()) {
        std::cerr << "staticmesh requires --out\n";
        return 1;
    }
    const int sources = (!obj.empty() ? 1 : 0) + (!fbx.empty() ? 1 : 0) + (!gltf.empty() ? 1 : 0);
    if (sources != 1) {
        std::cerr << "staticmesh requires exactly one of --obj, --fbx, or --gltf\n";
        return 1;
    }

    std::string err;
    bool ok = false;
    if (!obj.empty()) {
        ok = FStaticMeshBuilder::CookFromObj(obj, out, err);
    } else if (!fbx.empty()) {
        ok = FStaticMeshBuilder::CookFromFbx(fbx, out, err);
    } else {
        ok = FStaticMeshBuilder::CookFromGltf(gltf, out, materials, err);
    }
    if (!ok) {
        std::cerr << (err.empty() ? "Cook static mesh failed" : err) << '\n';
        return 2;
    }
    std::cout << "Cooked static mesh -> " << out << '\n';
    return 0;
}

[[nodiscard]] int cookCharacter(int argc, char** argv) {
    std::string name;
    std::string meshFbx;
    std::string runFbx;
    std::string outDir;
    FCookJumpAnimPaths jumpAnims{};

    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--name") {
            if (const char* v = argValue(argc, argv, i)) {
                name = v;
            }
        } else if (a == "--mesh") {
            if (const char* v = argValue(argc, argv, i)) {
                meshFbx = v;
            }
        } else if (a == "--run") {
            if (const char* v = argValue(argc, argv, i)) {
                runFbx = v;
            }
        } else if (a == "--jump") {
            if (const char* v = argValue(argc, argv, i)) {
                jumpAnims.jumpStartFbx = v;
            }
        } else if (a == "--fall") {
            if (const char* v = argValue(argc, argv, i)) {
                jumpAnims.fallLoopFbx = v;
            }
        } else if (a == "--land") {
            if (const char* v = argValue(argc, argv, i)) {
                jumpAnims.landFbx = v;
            }
        } else if (a == "--out") {
            if (const char* v = argValue(argc, argv, i)) {
                outDir = v;
            }
        } else {
            std::cerr << "Unknown arg '" << a << "'\n";
            return 1;
        }
    }

    if (name.empty() || meshFbx.empty() || runFbx.empty() || outDir.empty()) {
        std::cerr << "character requires --name --mesh --run --out\n";
        printUsage();
        return 1;
    }

    if (!CookCharacterFromFbx(name, meshFbx, runFbx, outDir, jumpAnims)) {
        std::cerr << "Cook failed\n";
        return 2;
    }
    return 0;
}

[[nodiscard]] int cookAnim(int argc, char** argv) {
    std::string fbx;
    std::string skeleton;
    std::string name;
    std::string outJson;
    bool looping = true;

    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--fbx") {
            if (const char* v = argValue(argc, argv, i)) {
                fbx = v;
            }
        } else if (a == "--skeleton") {
            if (const char* v = argValue(argc, argv, i)) {
                skeleton = v;
            }
        } else if (a == "--name") {
            if (const char* v = argValue(argc, argv, i)) {
                name = v;
            }
        } else if (a == "--out") {
            if (const char* v = argValue(argc, argv, i)) {
                outJson = v;
            }
        } else if (a == "--noloop") {
            looping = false;
        } else {
            std::cerr << "Unknown arg '" << a << "'\n";
            return 1;
        }
    }

    if (fbx.empty() || skeleton.empty() || outJson.empty()) {
        std::cerr << "anim requires --fbx --skeleton --out\n";
        printUsage();
        return 1;
    }

    if (!CookAnimSequenceFromFbx(fbx, skeleton, outJson, name, looping)) {
        std::cerr << "Cook anim failed\n";
        return 2;
    }
    return 0;
}

} // namespace

int32 UCookCommandlet::Main(int32 argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    const std::string mode = argv[1];
    if (mode == "-h" || mode == "--help" || mode == "help") {
        printUsage();
        return 0;
    }

    if (mode == "staticmesh") {
        return cookStaticMesh(argc, argv);
    }
    if (mode == "character") {
        return cookCharacter(argc, argv);
    }
    if (mode == "anim") {
        return cookAnim(argc, argv);
    }
    if (mode == "recipe") {
        if (argc < 3 || argv[2] == nullptr) {
            std::cerr << "recipe requires a JSON path\n";
            printUsage();
            return 1;
        }
        return FCookRecipe::RunFile(argv[2]);
    }

    std::cerr << "Unknown mode '" << mode << "'\n";
    printUsage();
    return 1;
}
