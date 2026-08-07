#include <leon/editor/AssetTools.h>

#include <filesystem>
#include <iostream>
#include <leon/core/Paths.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/level/Level.h>
#include <leon/level/LeonLevelFormat.h>
#include <leon/render/LeonMaterialFormat.h>
#include <leon/render/ResourceCache.h>
#include <string>
#include <vector>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

[[nodiscard]] std::string ExtLower(const fs::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return e;
}

[[nodiscard]] std::string NormalizeKey(const EditorContext& ctx, const std::string& path) {
    if (path.empty()) {
        return {};
    }
    return fs::path(MakePackRelativeAssetPath(ctx, path)).lexically_normal().generic_string();
}

[[nodiscard]] bool IsDirectoryAsset(const std::string& absPath) {
    std::error_code ec;
    return fs::is_directory(absPath, ec) && !ec;
}

[[nodiscard]] fs::path PackContentRoot(const EditorContext& ctx) {
    if (ctx.projectPath.empty()) {
        return {};
    }
    return ProjectContentDirectory(ctx.projectPath);
}

[[nodiscard]] bool RemapPathString(const EditorContext& ctx, std::string& field,
                                   const std::string& oldKey, const std::string& newKey,
                                   bool oldIsDir) {
    if (field.empty() || oldKey.empty()) {
        return false;
    }
    const std::string key = NormalizeKey(ctx, field);
    if (key.empty()) {
        return false;
    }

    std::string mapped;
    if (oldIsDir) {
        if (key == oldKey) {
            mapped = newKey;
        } else if (key.size() > oldKey.size() && key.compare(0, oldKey.size(), oldKey) == 0 &&
                   key[oldKey.size()] == '/') {
            mapped = newKey + key.substr(oldKey.size());
        } else {
            return false;
        }
    } else if (key == oldKey) {
        mapped = newKey;
    } else {
        return false;
    }

    // Store content-relative when possible (empty clears the soft ref).
    field = mapped.empty() ? std::string{} : MakePackRelativeAssetPath(ctx, mapped);
    return true;
}

int RemapOpenLevel(EditorContext& ctx, const std::string& oldKey, const std::string& newKey,
                   bool oldIsDir) {
    if (ctx.level == nullptr) {
        return 0;
    }
    int count = 0;
    Level& level = *ctx.level;
    for (StaticMeshComponent& mesh : level.StaticMeshes()) {
        if (RemapPathString(ctx, mesh.meshPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
        if (RemapPathString(ctx, mesh.materialPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
        if (RemapPathString(ctx, mesh.lightmapPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
    }
    std::string env = level.EnvironmentPath();
    if (RemapPathString(ctx, env, oldKey, newKey, oldIsDir)) {
        level.SetEnvironmentPath(std::move(env));
        ++count;
    }
    if (count > 0) {
        ctx.MarkDirty();
    }
    return count;
}

int RemapLevelDocument(EditorContext& ctx, LevelDocument& doc, const std::string& oldKey,
                       const std::string& newKey, bool oldIsDir) {
    int count = 0;
    if (RemapPathString(ctx, doc.environmentPath, oldKey, newKey, oldIsDir)) {
        ++count;
    }
    for (LevelActorRecord& actor : doc.actors) {
        if (RemapPathString(ctx, actor.meshPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
        if (RemapPathString(ctx, actor.materialPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
        if (RemapPathString(ctx, actor.lightmapPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
    }
    return count;
}

int RemapPackLevelsOnDisk(EditorContext& ctx, const std::string& oldKey, const std::string& newKey,
                          bool oldIsDir, const std::string& skipAbsLevelPath) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    std::error_code ec;
    if (!fs::exists(content, ec)) {
        return 0;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        if (ExtLower(entry.path()) != ".llev") {
            continue;
        }
        const std::string path = entry.path().generic_string();
        if (!skipAbsLevelPath.empty() &&
            fs::path(path).lexically_normal() == fs::path(skipAbsLevelPath).lexically_normal()) {
            // Open level is remapped in memory and saved by the user (dirty package).
            continue;
        }
        LevelDocument doc;
        if (!LoadLeonLevelFile(path, doc)) {
            continue;
        }
        const int n = RemapLevelDocument(ctx, doc, oldKey, newKey, oldIsDir);
        if (n <= 0) {
            continue;
        }
        if (!SaveLeonLevelFile(path, doc)) {
            std::cerr << "AssetTools: failed to write fixed-up level " << path << '\n';
            continue;
        }
        count += n;
    }
    return count;
}

int RemapPackMaterialsOnDisk(EditorContext& ctx, const std::string& oldKey,
                             const std::string& newKey, bool oldIsDir) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    std::error_code ec;
    if (!fs::exists(content, ec)) {
        return 0;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        if (ExtLower(entry.path()) != ".lmat") {
            continue;
        }
        const std::string path = entry.path().generic_string();
        // Skip rewriting the material file that is itself being moved/renamed.
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        LeonMaterialDocument doc;
        if (!LoadLeonMaterialDocument(path, doc)) {
            continue;
        }
        bool changed = false;
        if (RemapPathString(ctx, doc.baseColorMapPath, oldKey, newKey, oldIsDir)) {
            changed = true;
            ++count;
        }
        if (RemapPathString(ctx, doc.normalMapPath, oldKey, newKey, oldIsDir)) {
            changed = true;
            ++count;
        }
        if (!changed) {
            continue;
        }
        if (!SaveLeonMaterialFile(path, doc.name, doc.material, doc.baseColorMapPath,
                                  doc.normalMapPath)) {
            std::cerr << "AssetTools: failed to write fixed-up material " << path << '\n';
            continue;
        }
        if (ctx.resources != nullptr) {
            ctx.resources->InvalidateMaterial(path);
        }
    }
    return count;
}

int RemapSessionPaths(EditorContext& ctx, const std::string& oldKey, const std::string& newKey,
                      bool oldIsDir) {
    int count = 0;
    auto remapOne = [&](std::string& field) {
        if (RemapPathString(ctx, field, oldKey, newKey, oldIsDir)) {
            ++count;
        }
    };
    remapOne(ctx.previewAssetPath);
    remapOne(ctx.requestOpenMaterialPath);
    remapOne(ctx.requestSaveAssetPath);
    remapOne(ctx.pendingMaterialPickPath);
    remapOne(ctx.pendingMeshPickPath);
    remapOne(ctx.pendingHdrPickPath);
    remapOne(ctx.contentBrowserSelectedPath);

    for (std::string& p : ctx.dirtyMaterialPaths) {
        remapOne(p);
    }

    if (!ctx.levelPath.empty()) {
        const std::string levelKey = NormalizeKey(ctx, ctx.levelPath);
        if (oldIsDir) {
            if (levelKey == oldKey ||
                (levelKey.size() > oldKey.size() &&
                 levelKey.compare(0, oldKey.size(), oldKey) == 0 &&
                 levelKey[oldKey.size()] == '/')) {
                std::string mapped = newKey.empty()
                                         ? std::string{}
                                         : (levelKey == oldKey
                                                ? newKey
                                                : newKey + levelKey.substr(oldKey.size()));
                if (!mapped.empty()) {
                    const std::string resolved =
                        ResolveContentAssetPath(ctx.projectPath, mapped);
                    ctx.levelPath = resolved.empty() ? mapped : resolved;
                } else {
                    ctx.levelPath.clear();
                }
                ++count;
            }
        } else if (levelKey == oldKey) {
            if (newKey.empty()) {
                ctx.levelPath.clear();
            } else {
                const std::string resolved = ResolveContentAssetPath(ctx.projectPath, newKey);
                ctx.levelPath = resolved.empty() ? newKey : resolved;
            }
            ++count;
        }
    }
    return count;
}

int CountInString(const EditorContext& ctx, const std::string& field, const std::string& oldKey,
                  bool oldIsDir) {
    if (field.empty() || oldKey.empty()) {
        return 0;
    }
    const std::string key = NormalizeKey(ctx, field);
    if (key.empty()) {
        return 0;
    }
    if (oldIsDir) {
        if (key == oldKey) {
            return 1;
        }
        if (key.size() > oldKey.size() && key.compare(0, oldKey.size(), oldKey) == 0 &&
            key[oldKey.size()] == '/') {
            return 1;
        }
        return 0;
    }
    return key == oldKey ? 1 : 0;
}

void FinishOp(EditorContext& ctx, const std::string& oldAbs, const std::string& newAbs,
              bool oldWasDir, AssetToolsResult& result) {
    result.fixedUpReferences = FixUpReferences(ctx, oldAbs, newAbs, oldWasDir);

    if (ctx.resources != nullptr) {
        if (!oldWasDir && ExtLower(oldAbs) == ".lmat") {
            ctx.resources->InvalidateMaterial(oldAbs);
            if (!newAbs.empty()) {
                ctx.resources->InvalidateMaterial(newAbs);
            }
        }
    }

    ctx.requestContentRefresh = true;
    ctx.requestMaterialEditorRemapFrom = oldAbs;
    ctx.requestMaterialEditorRemapTo = newAbs;
}

[[nodiscard]] AssetToolsResult Fail(std::string error) {
    AssetToolsResult r;
    r.error = std::move(error);
    return r;
}

} // namespace

bool IsEditablePackAsset(const EditorContext& ctx, const std::string& absPath) {
    if (ctx.projectPath.empty() || absPath.empty()) {
        return false;
    }
    if (absPath.rfind("leon:", 0) == 0) {
        return false;
    }
    fs::path relative;
    return detail::IsUnderRoot(absPath, PackContentRoot(ctx), relative);
}

int CountAssetReferences(const EditorContext& ctx, const std::string& absPath) {
    if (absPath.empty()) {
        return 0;
    }
    const std::string oldKey = NormalizeKey(ctx, absPath);
    if (oldKey.empty()) {
        return 0;
    }
    const bool oldIsDir = IsDirectoryAsset(absPath);
    int count = 0;

    if (ctx.level != nullptr) {
        for (const StaticMeshComponent& mesh : ctx.level->StaticMeshes()) {
            count += CountInString(ctx, mesh.meshPath, oldKey, oldIsDir);
            count += CountInString(ctx, mesh.materialPath, oldKey, oldIsDir);
            count += CountInString(ctx, mesh.lightmapPath, oldKey, oldIsDir);
        }
        count += CountInString(ctx, ctx.level->EnvironmentPath(), oldKey, oldIsDir);
    }

    const fs::path content = PackContentRoot(ctx);
    std::error_code ec;
    if (content.empty() || !fs::exists(content, ec)) {
        return count;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        const std::string ext = ExtLower(entry.path());
        const std::string path = entry.path().generic_string();
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        if (ext == ".llev") {
            if (!ctx.levelPath.empty() &&
                fs::path(path).lexically_normal() ==
                    fs::path(ctx.levelPath).lexically_normal()) {
                continue; // already counted from open level
            }
            LevelDocument doc;
            if (!LoadLeonLevelFile(path, doc)) {
                continue;
            }
            count += CountInString(ctx, doc.environmentPath, oldKey, oldIsDir);
            for (const LevelActorRecord& actor : doc.actors) {
                count += CountInString(ctx, actor.meshPath, oldKey, oldIsDir);
                count += CountInString(ctx, actor.materialPath, oldKey, oldIsDir);
                count += CountInString(ctx, actor.lightmapPath, oldKey, oldIsDir);
            }
        } else if (ext == ".lmat") {
            LeonMaterialDocument doc;
            if (!LoadLeonMaterialDocument(path, doc)) {
                continue;
            }
            count += CountInString(ctx, doc.baseColorMapPath, oldKey, oldIsDir);
            count += CountInString(ctx, doc.normalMapPath, oldKey, oldIsDir);
        }
    }
    return count;
}

int FixUpReferences(EditorContext& ctx, const std::string& oldAbsOrRel,
                    const std::string& newAbsOrRel, bool oldIsDirectory) {
    // Flow: Fix Up References (Unreal Fix Up Redirectors equivalent, in-place)
    // 1. Normalize old/new content-relative keys
    // 2. Remap open level (mark dirty)
    // 3. Rewrite other pack .llev / .lmat on disk
    // 4. Update editor session paths
    if (oldAbsOrRel.empty()) {
        return 0;
    }
    const std::string oldKey = NormalizeKey(ctx, oldAbsOrRel);
    if (oldKey.empty()) {
        return 0;
    }
    const std::string newKey =
        newAbsOrRel.empty() ? std::string{} : NormalizeKey(ctx, newAbsOrRel);

    int total = 0;
    total += RemapOpenLevel(ctx, oldKey, newKey, oldIsDirectory);
    total += RemapPackLevelsOnDisk(ctx, oldKey, newKey, oldIsDirectory, ctx.levelPath);
    total += RemapPackMaterialsOnDisk(ctx, oldKey, newKey, oldIsDirectory);
    total += RemapSessionPaths(ctx, oldKey, newKey, oldIsDirectory);
    return total;
}

AssetToolsResult RenameAsset(EditorContext& ctx, const std::string& fromAbs,
                             const std::string& newName) {
    // Flow: Content Browser Rename
    // 1. Validate under pack Content
    // 2. filesystem::rename (same parent, new base name, keep extension)
    // 3. FixUpReferences(old → new)
    // 4. Session + Material Editor remap flags + refresh
    if (!IsEditablePackAsset(ctx, fromAbs)) {
        return Fail("Asset is not editable pack Content");
    }
    if (newName.empty()) {
        return Fail("Name cannot be empty");
    }
    for (char c : newName) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|') {
            return Fail("Name contains invalid characters");
        }
    }

    std::error_code ec;
    const fs::path from(fromAbs);
    if (!fs::exists(from, ec) || ec) {
        return Fail("Asset not found");
    }
    const bool isDir = fs::is_directory(from, ec);
    const fs::path dest =
        isDir ? (from.parent_path() / newName) : (from.parent_path() / (newName + from.extension().string()));
    if (fs::exists(dest, ec)) {
        return Fail("An asset with that name already exists");
    }
    fs::rename(from, dest, ec);
    if (ec) {
        return Fail("Rename failed: " + ec.message());
    }

    AssetToolsResult result;
    result.ok = true;
    FinishOp(ctx, fromAbs, dest.generic_string(), isDir, result);
    return result;
}

AssetToolsResult MoveAsset(EditorContext& ctx, const std::string& fromAbs,
                           const std::string& destFolderAbs) {
    // Flow: Content Browser Move
    // 1. Validate source + dest under pack Content
    // 2. filesystem::rename into dest folder
    // 3. FixUpReferences(old → new)
    // 4. Session + refresh
    if (!IsEditablePackAsset(ctx, fromAbs) || !IsEditablePackAsset(ctx, destFolderAbs)) {
        return Fail("Move must stay inside pack Content");
    }
    std::error_code ec;
    const fs::path from(fromAbs);
    const fs::path destDir(destFolderAbs);
    if (!fs::exists(from, ec) || ec) {
        return Fail("Asset not found");
    }
    if (!fs::is_directory(destDir, ec) || ec) {
        return Fail("Destination is not a folder");
    }
    const fs::path dest = destDir / from.filename();
    if (fs::weakly_canonical(from.parent_path(), ec) == fs::weakly_canonical(destDir, ec)) {
        AssetToolsResult noop;
        noop.ok = true;
        return noop;
    }
    // Reject moving a folder into itself.
    if (fs::is_directory(from, ec)) {
        const fs::path canonFrom = fs::weakly_canonical(from, ec);
        const fs::path canonDest = fs::weakly_canonical(destDir, ec);
        const fs::path rel = canonDest.lexically_relative(canonFrom);
        if (!rel.empty() && rel.generic_string().find("..") == std::string::npos) {
            return Fail("Cannot move a folder into itself");
        }
    }
    if (fs::exists(dest, ec)) {
        return Fail("An asset with that name already exists in the destination");
    }
    const bool isDir = fs::is_directory(from, ec);
    fs::rename(from, dest, ec);
    if (ec) {
        return Fail("Move failed: " + ec.message());
    }

    AssetToolsResult result;
    result.ok = true;
    FinishOp(ctx, fromAbs, dest.generic_string(), isDir, result);
    return result;
}

AssetToolsResult DeleteAssets(EditorContext& ctx, const std::string& pathAbs) {
    // Flow: Content Browser Delete Assets
    // 1. Validate under pack Content
    // 2. Count was shown in UI; Force Delete clears refs
    // 3. filesystem::remove_all
    // 4. FixUpReferences(old → empty)
    if (!IsEditablePackAsset(ctx, pathAbs)) {
        return Fail("Asset is not editable pack Content");
    }
    std::error_code ec;
    const fs::path path(pathAbs);
    if (!fs::exists(path, ec) || ec) {
        return Fail("Asset not found");
    }
    const bool isDir = fs::is_directory(path, ec);
    // Capture key before delete (path may vanish).
    const std::string oldAbs = path.generic_string();

    fs::remove_all(path, ec);
    if (ec) {
        return Fail("Delete failed: " + ec.message());
    }

    AssetToolsResult result;
    result.ok = true;
    result.fixedUpReferences = FixUpReferences(ctx, oldAbs, {}, isDir);
    if (ctx.resources != nullptr && !isDir && ExtLower(oldAbs) == ".lmat") {
        ctx.resources->InvalidateMaterial(oldAbs);
    }
    ctx.requestContentRefresh = true;
    ctx.requestMaterialEditorRemapFrom = oldAbs;
    ctx.requestMaterialEditorRemapTo.clear();
    if (ctx.contentBrowserSelectedPath == oldAbs) {
        ctx.contentBrowserSelectedPath.clear();
    }
    return result;
}

} // namespace leon::editor
