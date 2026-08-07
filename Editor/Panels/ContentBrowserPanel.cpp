#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <glm/vec3.hpp>
#include <imgui.h>
#include <iostream>
#include <leon/core/Camera.h>
#include <leon/core/Paths.h>
#include <leon/editor/AssetTools.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EngineContent.h>
#include <leon/editor/panels/ContentBrowserPanel.h>
#include <leon/Engine.h>
#include <leon/render/LeonMaterialFormat.h>
#include <leon/render/Renderer.h>
#include <leon/render/ResourceCache.h>
#include <string>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

std::string extLower(const fs::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return e;
}

[[nodiscard]] std::string ResolvePackRootFromLevelPath(const std::string& levelPath) {
    if (levelPath.empty()) {
        return {};
    }
    std::error_code ec;
    fs::path dir = fs::path(levelPath).parent_path();
    while (!dir.empty()) {
        if (fs::is_regular_file(dir / "leon.game.json", ec)) {
            return dir.generic_string();
        }
        const fs::path parent = dir.parent_path();
        if (parent == dir) {
            break;
        }
        dir = parent;
    }
    return {};
}

[[nodiscard]] const char* KindPrefix(ContentBrowserPanel::Entry::Kind kind) {
    using Kind = ContentBrowserPanel::Entry::Kind;
    switch (kind) {
    case Kind::Folder:
        return "[Dir]";
    case Kind::Level:
        return "[Level]";
    case Kind::Material:
        return "[Mat]";
    case Kind::StaticMesh:
        return "[SM]";
    case Kind::EnginePrimitive:
        return "[Prim]";
    case Kind::FbxSource:
        return "[FBX]";
    case Kind::Character:
        return "[Char]";
    case Kind::SkelMesh:
        return "[SKM]";
    case Kind::Skeleton:
        return "[Skel]";
    case Kind::Anim:
        return "[Anim]";
    case Kind::Hdr:
        return "[HDR]";
    default:
        return "[?]";
    }
}

[[nodiscard]] bool PathsEqualNormalized(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) {
        return false;
    }
    return fs::path(a).lexically_normal() == fs::path(b).lexically_normal();
}

[[nodiscard]] bool IsAssetPathDirty(const EditorContext& ctx,
                                    const ContentBrowserPanel::Entry& entry) {
    if (entry.isDirectory) {
        return false;
    }
    if (entry.kind == ContentBrowserPanel::Entry::Kind::Level) {
        return ctx.dirty && PathsEqualNormalized(ctx.levelPath, entry.path);
    }
    if (entry.kind == ContentBrowserPanel::Entry::Kind::Material) {
        for (const std::string& p : ctx.dirtyMaterialPaths) {
            if (PathsEqualNormalized(p, entry.path)) {
                return true;
            }
        }
    }
    return false;
}

void DrawFolderIcon(ImDrawList* draw, const ImVec2& min, const ImVec2& max) {
    if (draw == nullptr) {
        return;
    }
    const float w = max.x - min.x;
    const float h = max.y - min.y;
    const float pad = std::min(w, h) * 0.14f;
    const ImVec2 bodyMin{min.x + pad, min.y + pad + h * 0.18f};
    const ImVec2 bodyMax{max.x - pad, max.y - pad};
    const float tabH = h * 0.14f;
    const float tabW = w * 0.38f;
    const ImVec2 tabMin{bodyMin.x, bodyMin.y - tabH};
    const ImVec2 tabMax{bodyMin.x + tabW, bodyMin.y + 2.0f};

    draw->AddRectFilled(min, max, IM_COL32(42, 44, 48, 255), 3.0f);
    draw->AddRectFilled(tabMin, tabMax, IM_COL32(210, 168, 72, 255), 2.0f);
    draw->AddRectFilled(bodyMin, bodyMax, IM_COL32(232, 188, 88, 255), 3.0f);
    draw->AddRectFilled(ImVec2(bodyMin.x + 2.0f, bodyMin.y + h * 0.08f),
                        ImVec2(bodyMax.x - 2.0f, bodyMax.y - 2.0f), IM_COL32(196, 152, 58, 255),
                        2.0f);
}

[[nodiscard]] ImVec4 KindTint(ContentBrowserPanel::Entry::Kind kind) {
    using Kind = ContentBrowserPanel::Entry::Kind;
    switch (kind) {
    case Kind::Folder:
        return ImVec4(0.35f, 0.35f, 0.38f, 1.0f);
    case Kind::Level:
        return ImVec4(0.20f, 0.45f, 0.75f, 1.0f);
    case Kind::Material:
        return ImVec4(0.55f, 0.28f, 0.70f, 1.0f);
    case Kind::StaticMesh:
        return ImVec4(0.25f, 0.55f, 0.40f, 1.0f);
    case Kind::Character:
    case Kind::SkelMesh:
        return ImVec4(0.70f, 0.45f, 0.20f, 1.0f);
    case Kind::Hdr:
        return ImVec4(0.15f, 0.55f, 0.70f, 1.0f);
    case Kind::Anim:
        return ImVec4(0.65f, 0.55f, 0.20f, 1.0f);
    default:
        return ImVec4(0.40f, 0.40f, 0.42f, 1.0f);
    }
}

[[nodiscard]] bool ShouldSkipDirectoryName(const std::string& name) {
    return name == "build" || name == ".git" || name == "src" || name == "_leon_engine";
}

[[nodiscard]] std::string NormalizePath(const std::string& path) {
    return fs::path(path).lexically_normal().generic_string();
}

} // namespace

ContentBrowserPanel::Entry::Kind ContentBrowserPanel::ClassifyFile(const std::string& pathStr) {
    const fs::path path(pathStr);
    const std::string ext = extLower(path);
    const std::string fname = path.filename().string();

    if (ext == ".obj" || ext == ".lmesh" || ext == ".gltf" || ext == ".glb") {
        return Entry::Kind::StaticMesh;
    }
    if (ext == ".lmat") {
        return Entry::Kind::Material;
    }
    if (ext == ".lskm") {
        return Entry::Kind::SkelMesh;
    }
    if (ext == ".lskel") {
        return Entry::Kind::Skeleton;
    }
    if (ext == ".lanim") {
        return Entry::Kind::Anim;
    }
    if (ext == ".fbx") {
        return Entry::Kind::FbxSource;
    }
    if (ext == ".hdr") {
        return Entry::Kind::Hdr;
    }
    if ((fname.size() > 6 && fname.compare(fname.size() - 6, 6, ".lchar") == 0) ||
        (fname.size() > 15 && fname.compare(fname.size() - 15, 15, ".character.json") == 0)) {
        return Entry::Kind::Character;
    }
    if (ext == ".llev") {
        return Entry::Kind::Level;
    }
    return Entry::Kind::Other;
}

void ContentBrowserPanel::EnsureMaterialSwatch(Entry& entry) const {
    if (entry.hasSwatch || entry.kind != Entry::Kind::Material) {
        return;
    }
    LeonMaterialDocument doc;
    if (LoadLeonMaterialDocument(entry.path, doc)) {
        entry.swatch[0] = doc.material.albedo.x;
        entry.swatch[1] = doc.material.albedo.y;
        entry.swatch[2] = doc.material.albedo.z;
        entry.hasSwatch = true;
    }
}

void ContentBrowserPanel::SyncRoot(EditorContext& ctx) {
    const std::string syncKey = ctx.projectPath + "|" + ctx.levelPath;
    if (syncKey == lastLevelPath_ && !contentRoot_.empty()) {
        return;
    }
    lastLevelPath_ = syncKey;
    std::string projectRoot = ctx.projectPath;
    if (projectRoot.empty()) {
        projectRoot = ResolvePackRootFromLevelPath(ctx.levelPath);
    }
    if (projectRoot.empty()) {
        projectRoot = ResolveProjectsDirectory();
    }
    // Unreal-like: browse `<project>/Content` (not CMakeLists / src at project root).
    fs::path content = ProjectContentDirectory(projectRoot);
    std::error_code ec;
    if (!content.empty() && !fs::is_directory(content, ec)) {
        fs::create_directories(content, ec);
        fs::create_directories(content / "Levels", ec);
        fs::create_directories(content / "Materials", ec);
    }
    contentRoot_ = content.empty() ? projectRoot : content.generic_string();
    currentFolder_ = contentRoot_;
    scanned_ = false;
}

ContentBrowserPanel::Entry ContentBrowserPanel::BuildTree(const std::string& absoluteDir) const {
    Entry folder;
    folder.isDirectory = true;
    folder.kind = Entry::Kind::Folder;
    folder.path = absoluteDir;
    folder.name = fs::path(absoluteDir).filename().string();
    if (folder.name.empty()) {
        folder.name = absoluteDir;
    }

    std::error_code ec;
    if (!fs::is_directory(absoluteDir, ec)) {
        return folder;
    }

    std::vector<Entry> dirs;
    std::vector<Entry> files;
    for (const auto& it : fs::directory_iterator(absoluteDir, ec)) {
        Entry e;
        e.name = it.path().filename().string();
        e.path = it.path().generic_string();
        if (it.is_directory()) {
            if (ShouldSkipDirectoryName(e.name)) {
                continue;
            }
            e = BuildTree(e.path);
            dirs.push_back(std::move(e));
            continue;
        }
        if (!it.is_regular_file()) {
            continue;
        }
        e.kind = ClassifyFile(e.path);
        if (e.kind == Entry::Kind::Other) {
            continue;
        }
        if (e.kind == Entry::Kind::Material) {
            EnsureMaterialSwatch(e);
        }
        files.push_back(std::move(e));
    }

    std::sort(dirs.begin(), dirs.end(),
              [](const Entry& a, const Entry& b) { return a.name < b.name; });
    std::sort(files.begin(), files.end(),
              [](const Entry& a, const Entry& b) { return a.name < b.name; });
    folder.children.reserve(dirs.size() + files.size());
    for (Entry& d : dirs) {
        folder.children.push_back(std::move(d));
    }
    for (Entry& f : files) {
        folder.children.push_back(std::move(f));
    }
    return folder;
}

void ContentBrowserPanel::Refresh(EditorContext& ctx) {
    meshThumbs_.Clear();
    SyncRoot(ctx);
    scanned_ = true;
    materialThumbs_.Clear();
    root_ = Entry{};
    if (contentRoot_.empty()) {
        currentFolder_.clear();
        return;
    }
    root_ = BuildTree(contentRoot_);
    root_.name = fs::path(contentRoot_).filename().string();
    if (root_.name.empty() || root_.name == "." || root_.name == "..") {
        root_.name = "Content";
    }
    if (currentFolder_.empty()) {
        currentFolder_ = contentRoot_;
    } else {
        std::error_code ec;
        if (!fs::is_directory(currentFolder_, ec)) {
            currentFolder_ = contentRoot_;
        }
    }
}

const ContentBrowserPanel::Entry* ContentBrowserPanel::FindEntryByPath(const Entry& node,
                                                                      const std::string& path) const {
    const std::string needle = NormalizePath(path);
    if (NormalizePath(node.path) == needle) {
        return &node;
    }
    for (const Entry& child : node.children) {
        if (const Entry* found = FindEntryByPath(child, path)) {
            return found;
        }
    }
    return nullptr;
}

std::vector<ContentBrowserPanel::Entry> ContentBrowserPanel::CurrentFolderChildren() const {
    if (contentRoot_.empty()) {
        return {};
    }
    if (NormalizePath(currentFolder_) == NormalizePath(contentRoot_)) {
        return root_.children;
    }
    const Entry* folder = FindEntryByPath(root_, currentFolder_);
    if (folder == nullptr) {
        return root_.children;
    }
    return folder->children;
}

void ContentBrowserPanel::ActivateEntry(EditorContext& ctx, const Entry& entry, bool openFolder) {
    if (entry.isDirectory) {
        if (openFolder || viewMode_ == EViewMode::Contents) {
            currentFolder_ = entry.path;
        }
        return;
    }

    if (entry.kind == Entry::Kind::Level && ctx.engine != nullptr) {
        ctx.pendingOpenPath = entry.path;
        return;
    }
    if (entry.kind == Entry::Kind::Hdr) {
        if (ctx.requestPickHdr) {
            ctx.pendingHdrPickPath = MakePackRelativeAssetPath(ctx, entry.path);
        } else if (ctx.level != nullptr && ctx.resources != nullptr) {
            const std::string rel = MakePackRelativeAssetPath(ctx, entry.path);
            auto env = ctx.resources->LoadEnvMap(ResolveAssetPath(rel));
            if (!env) {
                env = ctx.resources->LoadEnvMap(entry.path);
            }
            if (env) {
                ctx.level->SetEnvironment(std::move(env));
                ctx.level->SetEnvironmentPath(rel);
                ctx.MarkDirty();
            }
        }
        return;
    }
    if (entry.kind == Entry::Kind::Material) {
        if (ctx.requestPickMaterial) {
            ctx.pendingMaterialPickPath = entry.path;
            ctx.requestPickMaterial = false;
        } else {
            ctx.requestOpenMaterialPath = entry.path;
            ctx.showMaterialEditor = true;
        }
        ctx.previewAssetPath = entry.path;
        ctx.requestPreviewReload = true;
        return;
    }
    if (entry.kind != Entry::Kind::Skeleton) {
        ctx.previewAssetPath = entry.path;
        ctx.requestPreviewReload = true;
    }
}

void ContentBrowserPanel::DrawEntry(EditorContext& ctx, const Entry& entry) {
    ImGui::PushID(entry.path.c_str());
    const bool selected = PathsEqualNormalized(ctx.contentBrowserSelectedPath, entry.path);
    if (entry.isDirectory) {
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_DefaultOpen;
        if (selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        const bool open = ImGui::TreeNodeEx((std::string(KindPrefix(Entry::Kind::Folder)) + " " +
                                             entry.name)
                                                .c_str(),
                                            flags);
        // Single click = select; double click = open folder in Contents view.
        if (ImGui::IsItemClicked()) {
            ctx.contentBrowserSelectedPath = entry.path;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                currentFolder_ = entry.path;
                viewMode_ = EViewMode::Contents;
            }
        }
        AcceptFolderDropTarget(ctx, entry);
        DrawAssetItemContextMenu(ctx, entry);
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("LEON_ASSET_PATH", entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (open) {
            for (const Entry& child : entry.children) {
                DrawEntry(ctx, child);
            }
            ImGui::TreePop();
        }
    } else {
        const bool dirty = IsAssetPathDirty(ctx, entry);
        const std::string label =
            std::string(KindPrefix(entry.kind)) + " " + entry.name + (dirty ? " *" : "");
        ImGuiTreeNodeFlags leafFlags =
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selected) {
            leafFlags |= ImGuiTreeNodeFlags_Selected;
        }
        ImGui::TreeNodeEx(label.c_str(), leafFlags);
        // Single click = select (+ soft preview); double click = open asset.
        if (ImGui::IsItemClicked()) {
            ctx.contentBrowserSelectedPath = entry.path;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                ActivateEntry(ctx, entry, false);
            } else if (entry.kind != Entry::Kind::Hdr && entry.kind != Entry::Kind::Skeleton &&
                       entry.kind != Entry::Kind::Level) {
                ctx.previewAssetPath = entry.path;
                ctx.requestPreviewReload = true;
            }
        }
        DrawAssetItemContextMenu(ctx, entry);
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("LEON_ASSET_PATH", entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }
    }
    ImGui::PopID();
}

void ContentBrowserPanel::DrawEngineLibrary(EditorContext& ctx) {
    constexpr ImGuiTreeNodeFlags kFolderFlags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_DefaultOpen;
    if (!ImGui::TreeNodeEx("[Engine] Engine", kFolderFlags)) {
        return;
    }

    const auto& items = EngineContentCatalog();
    auto drawLeaf = [&](const EngineContentItem& item) {
        ImGui::PushID(item.path.c_str());
        const char* prefix = "[?]";
        switch (item.kind) {
        case EngineContentItem::Kind::Primitive:
            prefix = "[Prim]";
            break;
        case EngineContentItem::Kind::Material:
            prefix = "[Mat]";
            break;
        case EngineContentItem::Kind::Sky:
            prefix = "[HDR]";
            break;
        default:
            break;
        }
        const std::string label = std::string(prefix) + " " + item.name;
        constexpr ImGuiTreeNodeFlags kLeaf =
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        ImGui::TreeNodeEx(label.c_str(), kLeaf);
        if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (item.kind == EngineContentItem::Kind::Material) {
                const char* matRel = item.path.ends_with("M_WorldGrid")
                                         ? "Materials/M_WorldGrid.lmat"
                                         : "Materials/M_Default.lmat";
                ctx.requestOpenMaterialPath = ResolveAssetPath(matRel);
                ctx.showMaterialEditor = true;
                ctx.previewAssetPath = ResolveAssetPath(matRel);
                ctx.requestPreviewReload = true;
            } else {
                std::string err;
                glm::vec3 pos = ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0, 0};
                if (ctx.camera != nullptr && ctx.camera->Mode() == ECameraMode::FreeLook) {
                    pos = ctx.camera->EyeLocation() + ctx.camera->ForwardVector() * 5.0f;
                    pos.y = std::max(pos.y, 0.0f);
                }
                if (!ApplyEngineContent(ctx, item.path, pos, err) && !err.empty()) {
                    std::cerr << "Engine content: " << err << '\n';
                }
            }
        }
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("LEON_ASSET_PATH", item.path.c_str(), item.path.size() + 1);
            ImGui::TextUnformatted(item.name.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    };

    auto drawFolder = [&](const char* folderName, const char* pathPrefix) {
        if (!ImGui::TreeNodeEx((std::string("[Dir] ") + folderName).c_str(), kFolderFlags)) {
            return;
        }
        for (const EngineContentItem& item : items) {
            if (item.kind == EngineContentItem::Kind::Folder) {
                continue;
            }
            if (item.path.starts_with(pathPrefix) && item.path != pathPrefix) {
                const std::string rest = item.path.substr(std::string(pathPrefix).size());
                if (rest.find('/') == std::string::npos) {
                    drawLeaf(item);
                }
            }
        }
        ImGui::TreePop();
    };

    drawFolder("Basic Shapes", "leon:Engine/BasicShapes/");
    drawFolder("Materials", "leon:Engine/Materials/");
    drawFolder("HDR", "leon:Engine/Hdr/");

    ImGui::TreePop();
}

void ContentBrowserPanel::DrawBreadcrumbs(EditorContext& ctx) {
    (void)ctx;
    if (contentRoot_.empty()) {
        ImGui::TextDisabled("No project root");
        return;
    }

    if (ImGui::SmallButton("Root")) {
        currentFolder_ = contentRoot_;
    }

    std::error_code ec;
    const fs::path rootPath = fs::path(contentRoot_).lexically_normal();
    const fs::path curPath = fs::path(currentFolder_).lexically_normal();
    fs::path relative = fs::relative(curPath, rootPath, ec);
    if (ec || relative.empty() || relative == ".") {
        return;
    }

    fs::path accum = rootPath;
    for (const fs::path& part : relative) {
        const std::string name = part.generic_string();
        if (name.empty() || name == ".") {
            continue;
        }
        accum /= part;
        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
        ImGui::PushID(accum.generic_string().c_str());
        if (ImGui::SmallButton(name.c_str())) {
            currentFolder_ = accum.generic_string();
        }
        ImGui::PopID();
    }
}

void ContentBrowserPanel::DrawContentsGrid(EditorContext& ctx) {
    DrawBreadcrumbs(ctx);
    ImGui::Separator();

    std::vector<Entry> children = CurrentFolderChildren();
    if (!ctx.contentFilter.empty()) {
        children.erase(std::remove_if(children.begin(), children.end(),
                                      [&](const Entry& e) {
                                          return e.name.find(ctx.contentFilter) == std::string::npos &&
                                                 e.path.find(ctx.contentFilter) == std::string::npos;
                                      }),
                       children.end());
    }

    if (children.empty()) {
        ImGui::TextDisabled("(empty folder — right-click to create)");
        return;
    }

    constexpr float kTile = 96.0f;
    constexpr float kPad = 8.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const int columns = std::max(1, static_cast<int>((avail + kPad) / (kTile + kPad)));
    int col = 0;
    // Spread 3D thumb renders across frames so opening a folder stays responsive.
    int thumbBudget = 3;

    for (Entry& entry : children) {
        if (entry.kind == Entry::Kind::Material) {
            EnsureMaterialSwatch(entry);
        }

        ImGui::PushID(entry.path.c_str());
        ImGui::BeginGroup();

        const ImVec2 tileSize(kTile, kTile);
        unsigned int thumbTex = 0;
        if (ctx.renderer != nullptr && ctx.resources != nullptr) {
            if (entry.kind == Entry::Kind::Material) {
                thumbTex = materialThumbs_.Ensure(*ctx.renderer, *ctx.resources, entry.path,
                                                  static_cast<int>(kTile), thumbBudget);
            } else if (entry.kind == Entry::Kind::StaticMesh) {
                thumbTex = meshThumbs_.Ensure(*ctx.renderer, *ctx.resources, entry.path,
                                              static_cast<int>(kTile), thumbBudget);
            }
        }

        // Single click = select (keep focus); double click = open folder/asset.
        if (ImGui::InvisibleButton("##tile", tileSize)) {
            ctx.contentBrowserSelectedPath = entry.path;
            if (!entry.isDirectory) {
                if (entry.kind == Entry::Kind::Material && ctx.requestPickMaterial) {
                    ctx.pendingMaterialPickPath = entry.path;
                    ctx.requestPickMaterial = false;
                } else if (entry.kind != Entry::Kind::Hdr && entry.kind != Entry::Kind::Skeleton &&
                           entry.kind != Entry::Kind::Level) {
                    ctx.previewAssetPath = entry.path;
                    ctx.requestPreviewReload = true;
                }
            }
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            ctx.contentBrowserSelectedPath = entry.path;
            ActivateEntry(ctx, entry, true);
        }
        DrawAssetItemContextMenu(ctx, entry);
        if (entry.isDirectory) {
            AcceptFolderDropTarget(ctx, entry);
        }
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("LEON_ASSET_PATH", entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const bool selected = PathsEqualNormalized(ctx.contentBrowserSelectedPath, entry.path);
        const ImU32 border = selected
                                 ? ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.75f, 0.25f, 1.0f))
                                 : ImGui::ColorConvertFloat4ToU32(ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        if (entry.isDirectory || entry.kind == Entry::Kind::Folder) {
            DrawFolderIcon(draw, min, max);
        } else if (thumbTex != 0) {
            draw->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(thumbTex)), min, max,
                           ImVec2(0, 1), ImVec2(1, 0));
        } else {
            ImVec4 tint = KindTint(entry.kind);
            if (entry.kind == Entry::Kind::Material && entry.hasSwatch) {
                tint = ImVec4(entry.swatch[0], entry.swatch[1], entry.swatch[2], 1.0f);
            }
            draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(tint), 3.0f);
            const char* badge = KindPrefix(entry.kind);
            const ImVec2 badgeSize = ImGui::CalcTextSize(badge);
            draw->AddText(ImVec2(min.x + (kTile - badgeSize.x) * 0.5f,
                                 min.y + (kTile - badgeSize.y) * 0.5f - 8.0f),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f)), badge);
        }
        if (selected) {
            draw->AddRectFilled(min, max, IM_COL32(255, 200, 60, 40), 3.0f);
            draw->AddRect(min, max, border, 3.0f, 0, 2.0f);
        } else {
            draw->AddRect(min, max, border, 3.0f);
        }

        const bool dirty = IsAssetPathDirty(ctx, entry);
        const std::string caption = entry.name + (dirty ? " *" : "");
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + kTile);
        ImGui::TextUnformatted(caption.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
        ImGui::PopID();

        ++col;
        if (col < columns) {
            ImGui::SameLine(0.0f, kPad);
        } else {
            col = 0;
        }
    }
}

void ContentBrowserPanel::BeginRenameAsset(EditorContext& ctx, const std::string& path) {
    if (!IsEditablePackAsset(ctx, path)) {
        return;
    }
    modalAssetPath_ = path;
    const fs::path p(path);
    const std::string stem = p.stem().string();
    (void)std::snprintf(renameNameBuf_, sizeof(renameNameBuf_), "%s", stem.c_str());
    openRenameModal_ = true;
}

void ContentBrowserPanel::BeginDeleteAsset(EditorContext& ctx, const std::string& path) {
    if (!IsEditablePackAsset(ctx, path)) {
        return;
    }
    modalAssetPath_ = path;
    deleteRefCount_ = CountAssetReferences(ctx, path);
    openDeleteModal_ = true;
}

void ContentBrowserPanel::AcceptFolderDropTarget(EditorContext& ctx, const Entry& folderEntry) {
    if (!folderEntry.isDirectory || !IsEditablePackAsset(ctx, folderEntry.path)) {
        return;
    }
    if (!ImGui::BeginDragDropTarget()) {
        return;
    }
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("LEON_ASSET_PATH")) {
        const char* from = static_cast<const char*>(payload->Data);
        if (from != nullptr && from[0] != '\0' && IsEditablePackAsset(ctx, from)) {
            ctx.requestMoveAssetPath = from;
            ctx.requestMoveAssetDestFolder = folderEntry.path;
        }
    }
    ImGui::EndDragDropTarget();
}

void ContentBrowserPanel::DrawAssetItemContextMenu(EditorContext& ctx, const Entry& entry) {
    if (!ImGui::BeginPopupContextItem("CBAssetItemMenu")) {
        return;
    }
    ctx.contentBrowserSelectedPath = entry.path;

    const bool dirty = IsAssetPathDirty(ctx, entry);
    const bool canSaveAsset =
        dirty && (entry.kind == Entry::Kind::Level || entry.kind == Entry::Kind::Material);
    if (ImGui::MenuItem("Save Asset", "Ctrl+S", false, canSaveAsset)) {
        ctx.requestSaveAssetPath = entry.path;
    }
    if (ImGui::MenuItem("Save All", "Ctrl+Shift+S", false,
                        ctx.dirty || !ctx.dirtyMaterialPaths.empty())) {
        ctx.requestSaveAll = true;
    }
    ImGui::Separator();
    const bool editable = IsEditablePackAsset(ctx, entry.path);
    if (ImGui::MenuItem("Rename", "F2", false, editable)) {
        BeginRenameAsset(ctx, entry.path);
    }
    if (ImGui::MenuItem("Delete", "Del", false, editable)) {
        BeginDeleteAsset(ctx, entry.path);
    }
    ImGui::EndPopup();
}

void ContentBrowserPanel::DrawRenameAssetModal(EditorContext& ctx) {
    if (openRenameModal_) {
        ImGui::OpenPopup("Rename Asset");
        openRenameModal_ = false;
    }
    if (!ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    const fs::path path(modalAssetPath_);
    ImGui::TextUnformatted(path.filename().generic_string().c_str());
    ImGui::InputText("Name", renameNameBuf_, sizeof(renameNameBuf_));
    if (!path.extension().empty()) {
        ImGui::TextDisabled("Extension: %s", path.extension().generic_string().c_str());
    }
    if (ImGui::Button("Rename", ImVec2(120, 0))) {
        ctx.requestRenameAssetPath = modalAssetPath_;
        ctx.pendingRenameAssetName = renameNameBuf_;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ContentBrowserPanel::DrawDeleteAssetsModal(EditorContext& ctx) {
    if (openDeleteModal_) {
        ImGui::OpenPopup("Delete Assets");
        openDeleteModal_ = false;
    }
    if (!ImGui::BeginPopupModal("Delete Assets", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextWrapped("%s", modalAssetPath_.c_str());
    if (deleteRefCount_ > 0) {
        ImGui::TextWrapped("Referenced by %d locations.", deleteRefCount_);
        ImGui::TextDisabled("Force Delete clears soft references (Fix Up References).");
    } else {
        ImGui::TextUnformatted("This asset is not referenced by other packages.");
    }
    const char* confirmLabel = deleteRefCount_ > 0 ? "Force Delete" : "Delete";
    if (ImGui::Button(confirmLabel, ImVec2(140, 0))) {
        ctx.requestDeleteAssetPath = modalAssetPath_;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ContentBrowserPanel::DrawCreateContextMenu(EditorContext& ctx) {
    if (!ImGui::BeginPopupContextWindow("ContentBrowserCreateMenu",
                                        ImGuiPopupFlags_MouseButtonRight |
                                            ImGuiPopupFlags_NoOpenOverExistingPopup)) {
        return;
    }
    const bool canSaveAll = ctx.dirty || !ctx.dirtyMaterialPaths.empty();
    if (ImGui::MenuItem("Save All", "Ctrl+Shift+S", false, canSaveAll)) {
        ctx.requestSaveAll = true;
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Create")) {
        if (ImGui::MenuItem("Material…", nullptr, false, !contentRoot_.empty())) {
            openNewMaterialModal_ = true;
        }
        if (ImGui::MenuItem("Folder…", nullptr, false, !contentRoot_.empty())) {
            openNewFolderModal_ = true;
        }
        ImGui::EndMenu();
    }
    ImGui::EndPopup();
}

void ContentBrowserPanel::DrawNewMaterialModal(EditorContext& ctx) {
    if (openNewMaterialModal_) {
        ImGui::OpenPopup("New Leon Material");
        openNewMaterialModal_ = false;
    }
    if (!ImGui::BeginPopupModal("New Leon Material", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    static char matName[128] = "M_New";
    static float baseColor[3] = {0.7f, 0.7f, 0.72f};
    static float metallic = 0.0f;
    static float roughness = 0.6f;
    ImGui::InputText("Name", matName, sizeof(matName));
    ImGui::ColorEdit3("BaseColor", baseColor);
    ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &roughness, 0.04f, 1.0f);
    ImGui::TextDisabled("Writes .lmat into the current folder (or Materials/)");
    if (ImGui::Button("Create", ImVec2(120, 0))) {
        std::string name = matName;
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (!name.empty()) {
            if (name.size() < 2 || name[0] != 'M' || name[1] != '_') {
                name = "M_" + name;
            }
            fs::path dir;
            if (!currentFolder_.empty()) {
                dir = fs::path(currentFolder_);
            } else if (!contentRoot_.empty()) {
                dir = fs::path(contentRoot_) / "Materials";
            } else {
                dir = fs::path(ResolveAssetPath("assets/Materials"));
            }
            std::error_code ec;
            fs::create_directories(dir, ec);
            const fs::path outPath = dir / (name + ".lmat");
            Material mat{};
            mat.albedo = {baseColor[0], baseColor[1], baseColor[2]};
            mat.metallic = metallic;
            mat.roughness = roughness;
            if (SaveLeonMaterialFile(outPath.generic_string(), name, mat)) {
                scanned_ = false;
                Refresh(ctx);
                ctx.requestContentRefresh = true;
                std::cout << "Content Browser: created " << outPath.generic_string() << '\n';
                ImGui::CloseCurrentPopup();
            } else {
                std::cerr << "Content Browser: failed to write " << outPath << '\n';
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ContentBrowserPanel::DrawNewFolderModal(EditorContext& ctx) {
    if (openNewFolderModal_) {
        ImGui::OpenPopup("New Content Folder");
        openNewFolderModal_ = false;
    }
    if (!ImGui::BeginPopupModal("New Content Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    static char folderName[128] = "NewFolder";
    ImGui::InputText("Name", folderName, sizeof(folderName));
    if (ImGui::Button("Create", ImVec2(120, 0))) {
        std::string name = folderName;
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        const std::string parent = !currentFolder_.empty() ? currentFolder_ : contentRoot_;
        if (!name.empty() && name.find_first_of("\\/:*?\"<>|") == std::string::npos &&
            !parent.empty()) {
            std::error_code ec;
            const fs::path dir = fs::path(parent) / name;
            if (fs::create_directories(dir, ec) || fs::is_directory(dir, ec)) {
                scanned_ = false;
                Refresh(ctx);
                ImGui::CloseCurrentPopup();
            } else {
                std::cerr << "Content Browser: failed to create folder " << dir << '\n';
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ContentBrowserPanel::Draw(EditorContext& ctx) {
    if (!ctx.showContentBrowser) {
        return;
    }
    if (!ImGui::Begin("Content Browser", &ctx.showContentBrowser)) {
        ImGui::End();
        return;
    }

    SyncRoot(ctx);
    if (ctx.requestContentRefresh || !scanned_) {
        Refresh(ctx);
        ctx.requestContentRefresh = false;
    }

    if (ImGui::Button("Import…")) {
        ctx.requestOpenImportDialog = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        scanned_ = false;
        Refresh(ctx);
    }
    ImGui::SameLine();
    {
        const bool canSave = ctx.dirty || !ctx.dirtyMaterialPaths.empty();
        if (!canSave) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Save")) {
            ctx.requestSaveCurrent = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Save Asset: focused dirty material, else dirty level (Ctrl+S)");
        }
        ImGui::SameLine();
        if (ImGui::Button("Save All")) {
            ctx.requestSaveAll = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Save all dirty materials and the current level (Ctrl+Shift+S)");
        }
        if (!canSave) {
            ImGui::EndDisabled();
        }
    }
    ImGui::SameLine();
    {
        const char* modeLabel =
            viewMode_ == EViewMode::Hierarchy ? "View: Hierarchy" : "View: Contents";
        if (ImGui::Button(modeLabel)) {
            viewMode_ =
                viewMode_ == EViewMode::Hierarchy ? EViewMode::Contents : EViewMode::Hierarchy;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggle folder tree vs flat contents with path navigation");
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", contentRoot_.empty() ? "(project)" : root_.name.c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Right-click: Save All / Create. Asset RMB: Save Asset / Rename / Delete");
    }

    char filterBuf[128];
    (void)std::snprintf(filterBuf, sizeof(filterBuf), "%s", ctx.contentFilter.c_str());
    if (ImGui::InputTextWithHint("##content_filter", "Filter…", filterBuf, sizeof(filterBuf))) {
        ctx.contentFilter = filterBuf;
    }

    // Unreal-like Content Browser shortcuts when this window is focused.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::IsAnyItemActive()) {
        if (ImGui::IsKeyPressed(ImGuiKey_F2) && !ctx.contentBrowserSelectedPath.empty()) {
            BeginRenameAsset(ctx, ctx.contentBrowserSelectedPath);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !ctx.contentBrowserSelectedPath.empty()) {
            BeginDeleteAsset(ctx, ctx.contentBrowserSelectedPath);
        }
    }

    ImGui::Separator();
    if (ImGui::BeginChild("ContentBody", ImVec2(0, 0), false,
                          ImGuiWindowFlags_NoMove)) {
        DrawCreateContextMenu(ctx);
        // Drop onto current folder (Contents view background).
        if (viewMode_ == EViewMode::Contents && !currentFolder_.empty() &&
            IsEditablePackAsset(ctx, currentFolder_)) {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("LEON_ASSET_PATH")) {
                    const char* from = static_cast<const char*>(payload->Data);
                    if (from != nullptr && from[0] != '\0' && IsEditablePackAsset(ctx, from)) {
                        ctx.requestMoveAssetPath = from;
                        ctx.requestMoveAssetDestFolder = currentFolder_;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
        if (viewMode_ == EViewMode::Contents) {
            DrawContentsGrid(ctx);
        } else {
            DrawEngineLibrary(ctx);
            ImGui::Separator();
            ImGui::TextDisabled("Project");
            if (contentRoot_.empty()) {
                ImGui::TextDisabled("Open a project to browse pack content");
            } else {
                for (const Entry& child : root_.children) {
                    if (!ctx.contentFilter.empty()) {
                        const std::string needle = ctx.contentFilter;
                        auto matches = [&](auto&& self, const Entry& e) -> bool {
                            if (e.name.find(needle) != std::string::npos ||
                                e.path.find(needle) != std::string::npos) {
                                return true;
                            }
                            for (const Entry& c : e.children) {
                                if (self(self, c)) {
                                    return true;
                                }
                            }
                            return false;
                        };
                        if (!matches(matches, child)) {
                            continue;
                        }
                    }
                    DrawEntry(ctx, child);
                }
                if (root_.children.empty()) {
                    ImGui::TextDisabled("(empty pack — right-click to create)");
                }
            }
        }
    }
    ImGui::EndChild();

    DrawNewMaterialModal(ctx);
    DrawNewFolderModal(ctx);
    DrawRenameAssetModal(ctx);
    DrawDeleteAssetsModal(ctx);

    ImGui::End();
}

} // namespace leon::editor

