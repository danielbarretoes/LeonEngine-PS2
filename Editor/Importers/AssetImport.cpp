#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <leon/content/CookedSkeletal.h>
#include <leon/core/Paths.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/EditorContext.h>
#include <leon/import/StaticMeshCook.h>
#include <string>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

std::string stemFromPath(const std::string& path) {
    return fs::path(path).stem().string();
}

std::string sanitizeAssetName(std::string name) {
    if (name.empty()) {
        name = "Imported";
    }
    for (char& c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            c = '_';
        }
    }
    return name;
}

[[nodiscard]] fs::path ImportAssetsRoot(const EditorContext& ctx) {
    if (!ctx.projectPath.empty()) {
        return ProjectContentDirectory(ctx.projectPath) / "assets";
    }
    return fs::path(ResolveAssetPath("assets"));
}

bool copyFileRequired(const fs::path& from, const fs::path& to, std::error_code& ec,
                      std::string& err) {
    if (!fs::is_regular_file(from, ec)) {
        err = "Source not found: " + from.generic_string();
        return false;
    }
    fs::create_directories(to.parent_path(), ec);
    if (ec) {
        err = "Failed to create " + to.parent_path().generic_string() + ": " + ec.message();
        return false;
    }
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        err = "Failed to copy " + from.generic_string() + ": " + ec.message();
        return false;
    }
    return true;
}

/// Best-effort sibling copy; returns false when the source exists but copy fails.
bool copyFileOptional(const fs::path& from, const fs::path& to, std::error_code& ec,
                      int& warnCount) {
    if (!fs::is_regular_file(from, ec)) {
        return true;
    }
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        ++warnCount;
        std::cerr << "EditorImport: failed to copy dependency " << from.generic_string() << ": "
                  << ec.message() << '\n';
        return false;
    }
    return true;
}

void appendWarnSuffix(std::string& message, int warnCount) {
    if (warnCount > 0) {
        message += " (" + std::to_string(warnCount) + " dependency copy warning(s))";
    }
}

} // namespace

bool EditorImportAsset(EditorContext& ctx, const AssetImportRequest& request,
                       AssetImportResult& out) {
    out = {};
    if (request.sourcePath.empty()) {
        out.message = "Source path is empty";
        return false;
    }
    if (!fs::is_regular_file(request.sourcePath)) {
        out.message = "Source file not found: " + request.sourcePath;
        return false;
    }

    const std::string name = sanitizeAssetName(
        request.assetName.empty() ? stemFromPath(request.sourcePath) : request.assetName);
    const fs::path assetsRoot = ImportAssetsRoot(ctx);

    if (request.mode == EAssetImportMode::StaticObj) {
        const fs::path destDir = assetsRoot / "imported" / name;
        std::error_code ec;
        fs::create_directories(destDir, ec);
        if (ec) {
            out.message = "Failed to create " + destDir.generic_string();
            return false;
        }

        const fs::path src = request.sourcePath;
        const fs::path destObj = destDir / (name + ".obj");
        std::string copyErr;
        if (!copyFileRequired(src, destObj, ec, copyErr)) {
            out.message = copyErr;
            return false;
        }

        int warnCount = 0;
        const fs::path mtlSrc = src.parent_path() / (src.stem().string() + ".mtl");
        const fs::path mtlDst = destDir / (name + ".mtl");
        (void)copyFileOptional(mtlSrc, mtlDst, ec, warnCount);

        for (const auto& entry : fs::directory_iterator(src.parent_path(), ec)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto ext = entry.path().extension().string();
            std::string lower = ext;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower == ".png" || lower == ".jpg" || lower == ".jpeg" || lower == ".tga" ||
                lower == ".bmp") {
                (void)copyFileOptional(entry.path(), destDir / entry.path().filename(), ec,
                                       warnCount);
            }
        }

        const fs::path destLmesh = destDir / (name + ".lmesh");
        std::string cookErr;
        if (!CookStaticMeshFromObj(destObj.generic_string(), destLmesh.generic_string(), cookErr)) {
            out.message = "OBJ copied but .lmesh cook failed: " + cookErr;
            appendWarnSuffix(out.message, warnCount);
            std::cerr << "EditorImport: " << out.message << '\n';
            out.ok = false;
            return false;
        }

        if (ctx.resources != nullptr) {
            (void)ctx.resources->LoadStaticMesh(destLmesh.generic_string());
        }

        out.ok = true;
        out.previewPath = destLmesh.generic_string();
        out.message = "Imported OBJ → cooked " + out.previewPath;
        appendWarnSuffix(out.message, warnCount);
        std::cout << "EditorImport: " << out.message << '\n';
        return true;
    }

    if (request.mode == EAssetImportMode::StaticFbx) {
        const fs::path destDir = assetsRoot / "imported" / name;
        std::error_code ec;
        fs::create_directories(destDir, ec);
        if (ec) {
            out.message = "Failed to create " + destDir.generic_string();
            return false;
        }

        const fs::path src = request.sourcePath;
        const fs::path destFbx = destDir / (name + ".fbx");
        std::string copyErr;
        if (!copyFileRequired(src, destFbx, ec, copyErr)) {
            out.message = copyErr;
            return false;
        }

        const fs::path destLmesh = destDir / (name + ".lmesh");
        std::string cookErr;
        if (!CookStaticMeshFromFbx(destFbx.generic_string(), destLmesh.generic_string(), cookErr)) {
            out.message = "FBX copied but .lmesh cook failed: " + cookErr;
            std::cerr << "EditorImport: " << out.message << '\n';
            return false;
        }

        if (ctx.resources != nullptr) {
            (void)ctx.resources->LoadStaticMesh(destLmesh.generic_string());
        }

        out.ok = true;
        out.previewPath = destLmesh.generic_string();
        out.message = "Imported FBX → cooked " + out.previewPath;
        std::cout << "EditorImport: " << out.message << '\n';
        return true;
    }

    if (request.mode == EAssetImportMode::StaticGltf) {
        const fs::path destDir = assetsRoot / "imported" / name;
        std::error_code ec;
        fs::create_directories(destDir, ec);
        if (ec) {
            out.message = "Failed to create " + destDir.generic_string();
            return false;
        }

        const fs::path src = request.sourcePath;
        const std::string ext = src.extension().string();
        const fs::path destGltf = destDir / (name + ext);
        std::string copyErr;
        if (!copyFileRequired(src, destGltf, ec, copyErr)) {
            out.message = copyErr;
            return false;
        }
        int warnCount = 0;
        for (const auto& entry : fs::directory_iterator(src.parent_path(), ec)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            auto e = entry.path().extension().string();
            for (char& c : e) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (e == ".bin" || e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".ktx" ||
                e == ".webp") {
                (void)copyFileOptional(entry.path(), destDir / entry.path().filename(), ec,
                                       warnCount);
            }
        }

        const fs::path destLmesh = destDir / (name + ".lmesh");
        const fs::path matsDir = destDir / "Materials";
        std::string cookErr;
        if (!CookStaticMeshFromGltf(destGltf.generic_string(), destLmesh.generic_string(),
                                    matsDir.generic_string(), cookErr)) {
            out.message = "glTF cook failed: " + cookErr;
            appendWarnSuffix(out.message, warnCount);
            std::cerr << "EditorImport: " << out.message << '\n';
            return false;
        }

        if (ctx.resources != nullptr) {
            (void)ctx.resources->LoadStaticMesh(destLmesh.generic_string());
        }

        out.ok = true;
        out.previewPath = destLmesh.generic_string();
        out.message = "Imported glTF → cooked " + out.previewPath;
        appendWarnSuffix(out.message, warnCount);
        std::cout << "EditorImport: " << out.message << '\n';
        return true;
    }

    if (request.mode == EAssetImportMode::CharacterFbx) {
        const fs::path outDir = assetsRoot / "characters" / name;
        std::error_code ec;
        fs::create_directories(outDir, ec);

        const std::string meshFbx = request.sourcePath;
        const std::string runFbx =
            request.secondaryFbxPath.empty() ? meshFbx : request.secondaryFbxPath;

        if (!CookCharacterFromFbx(name, meshFbx, runFbx, outDir.generic_string())) {
            out.message = "CookCharacterFromFbx failed (need skinned FBX)";
            return false;
        }

        const fs::path characterAsset = outDir / (name + ".lchar");
        out.ok = true;
        out.previewPath = characterAsset.generic_string();
        out.message = "Cooked character → " + out.previewPath;
        std::cout << "EditorImport: " << out.message << '\n';
        return true;
    }

    if (request.mode == EAssetImportMode::AnimFbx) {
        if (request.skeletonJsonPath.empty() || !fs::is_regular_file(request.skeletonJsonPath)) {
            out.message = "Anim import requires a valid *.lskel";
            return false;
        }
        const fs::path skelPath = request.skeletonJsonPath;
        const fs::path animDir = skelPath.parent_path() / "Anims";
        std::error_code ec;
        fs::create_directories(animDir, ec);
        const fs::path outAnim = animDir / (name + ".lanim");

        if (!CookAnimSequenceFromFbx(request.sourcePath, request.skeletonJsonPath,
                                     outAnim.generic_string(), name, request.animLooping)) {
            out.message = "CookAnimSequenceFromFbx failed";
            return false;
        }

        bool updatedBlendspace = false;
        const fs::path characterDir = skelPath.parent_path();
        fs::path blendspacePath;
        std::error_code findEc;
        for (const auto& entry : fs::directory_iterator(characterDir, findEc)) {
            if (findEc || !entry.is_regular_file()) {
                continue;
            }
            const std::string fname = entry.path().filename().string();
            const std::string fext = entry.path().extension().string();
            if (fext == ".lchar") {
                CharacterVisualDesc character;
                if (LoadCharacterVisualLchar(entry.path().generic_string(), character) &&
                    !character.blendSpaceRel.empty()) {
                    blendspacePath = characterDir / character.blendSpaceRel;
                    break;
                }
            }
        }
        if (blendspacePath.empty()) {
            for (const auto& entry : fs::directory_iterator(characterDir, findEc)) {
                if (findEc || !entry.is_regular_file()) {
                    continue;
                }
                const std::string fname = entry.path().filename().string();
                if (fname.find("Locomotion") != std::string::npos &&
                    fname.find(".blendspace1d.json") != std::string::npos) {
                    blendspacePath = entry.path();
                    break;
                }
            }
        }

        if (!blendspacePath.empty() && fs::is_regular_file(blendspacePath, findEc) && !findEc) {
            BlendSpace1DAssetDesc bs;
            if (LoadBlendSpace1DJson(blendspacePath.generic_string(), bs)) {
                std::error_code relEc;
                const fs::path animRel =
                    fs::relative(outAnim, blendspacePath.parent_path(), relEc);
                const std::string animRelStr =
                    (!relEc && !animRel.empty()) ? animRel.generic_string()
                                                 : (fs::path("Anims") / (name + ".lanim")).generic_string();
                bool alreadyPresent = false;
                for (const auto& sample : bs.samples) {
                    if (sample.animRelPath == animRelStr) {
                        alreadyPresent = true;
                        break;
                    }
                }
                if (!alreadyPresent) {
                    float position = 0.5f;
                    if (!bs.samples.empty()) {
                        position = bs.samples.back().position + 0.1f;
                    }
                    bs.samples.push_back({animRelStr, position});
                    if (SaveBlendSpace1DJson(blendspacePath.generic_string(), bs)) {
                        updatedBlendspace = true;
                    }
                }
            }
        }

        out.ok = true;
        out.previewPath = outAnim.generic_string();
        out.message = "Cooked anim → " + out.previewPath;
        if (updatedBlendspace) {
            out.message += " (updated blendspace)";
        }
        std::cout << "EditorImport: " << out.message << '\n';
        return true;
    }

    out.message = "Unknown import mode";
    return false;
}

} // namespace leon::editor
