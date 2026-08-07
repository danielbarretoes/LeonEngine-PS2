#include <leon/editor/panels/MaterialEditorPanel.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/EditorFileDialog.h>
#include <leon/level/Level.h>

namespace leon::editor {

MaterialEditorPanel::Doc* MaterialEditorPanel::FindDoc(const std::string& path) {
    for (Doc& d : docs_) {
        if (d.path == path) {
            return &d;
        }
    }
    return nullptr;
}

const MaterialEditorPanel::Doc* MaterialEditorPanel::FindDoc(const std::string& path) const {
    for (const Doc& d : docs_) {
        if (d.path == path) {
            return &d;
        }
    }
    return nullptr;
}

bool MaterialEditorPanel::HasDirtyDocs() const {
    for (const Doc& d : docs_) {
        if (d.open && d.dirty) {
            return true;
        }
    }
    return false;
}

bool MaterialEditorPanel::IsPathDirty(const std::string& path) const {
    const Doc* doc = FindDoc(path);
    return doc != nullptr && doc->open && doc->dirty;
}

bool MaterialEditorPanel::HasFocusedDirtyDoc() const {
    if (focusedPath_.empty()) {
        return false;
    }
    return IsPathDirty(focusedPath_);
}

bool MaterialEditorPanel::SaveFocused(EditorContext& ctx) {
    if (focusedPath_.empty()) {
        return false;
    }
    return SavePath(ctx, focusedPath_);
}

bool MaterialEditorPanel::SavePath(EditorContext& ctx, const std::string& path) {
    Doc* doc = FindDoc(path);
    if (doc == nullptr || !doc->open) {
        return false;
    }
    if (!doc->dirty) {
        return true;
    }
    return SaveDoc(ctx, *doc);
}

bool MaterialEditorPanel::SaveAll(EditorContext& ctx) {
    bool ok = true;
    bool any = false;
    for (Doc& doc : docs_) {
        if (!doc.open || !doc.dirty) {
            continue;
        }
        any = true;
        ok = SaveDoc(ctx, doc) && ok;
    }
    return any ? ok : true;
}

void MaterialEditorPanel::SyncDirtyPaths(EditorContext& ctx) const {
    ctx.dirtyMaterialPaths.clear();
    for (const Doc& d : docs_) {
        if (d.open && d.dirty) {
            ctx.dirtyMaterialPaths.push_back(d.path);
        }
    }
}

void MaterialEditorPanel::RemapAssetPath(const std::string& fromAbs, const std::string& toAbs) {
    if (fromAbs.empty()) {
        return;
    }
    namespace fs = std::filesystem;
    const fs::path fromNorm = fs::path(fromAbs).lexically_normal();
    for (Doc& doc : docs_) {
        if (fs::path(doc.path).lexically_normal() != fromNorm) {
            continue;
        }
        if (toAbs.empty()) {
            doc.open = false;
            if (focusedPath_ == doc.path) {
                focusedPath_.clear();
            }
            continue;
        }
        doc.path = toAbs;
        if (focusedPath_ == fromAbs || fs::path(focusedPath_).lexically_normal() == fromNorm) {
            focusedPath_ = toAbs;
        }
    }
}

void MaterialEditorPanel::OpenMaterial(const std::string& path) {
    if (path.empty() || !IsLeonMaterialPath(path)) {
        EditorLogWarn("Material Editor: not a .lmat — " + path);
        return;
    }
    if (Doc* existing = FindDoc(path)) {
        existing->open = true;
        existing->focusOnce = true;
        return;
    }

    LeonMaterialDocument data;
    if (!LoadLeonMaterialDocument(path, data)) {
        EditorLogError("Material Editor: failed to load " + path);
        return;
    }

    Doc doc;
    doc.path = path;
    doc.data = std::move(data);
    doc.dirty = false;
    doc.open = true;
    doc.focusOnce = true;
    (void)std::snprintf(doc.nameBuf, sizeof(doc.nameBuf), "%s", doc.data.name.c_str());
    (void)std::snprintf(doc.baseMapBuf, sizeof(doc.baseMapBuf), "%s",
                        doc.data.baseColorMapPath.c_str());
    (void)std::snprintf(doc.normalMapBuf, sizeof(doc.normalMapBuf), "%s",
                        doc.data.normalMapPath.c_str());
    docs_.push_back(std::move(doc));
    EditorLogInfo("Material Editor: opened " + path);
}

bool MaterialEditorPanel::SaveDoc(EditorContext& ctx, Doc& doc) {
    doc.data.name = doc.nameBuf;
    doc.data.baseColorMapPath = doc.baseMapBuf;
    doc.data.normalMapPath = doc.normalMapBuf;
    if (!SaveLeonMaterialFile(doc.path, doc.data.name, doc.data.material, doc.data.baseColorMapPath,
                              doc.data.normalMapPath)) {
        EditorLogError("Material Editor: save failed " + doc.path);
        return false;
    }
    doc.dirty = false;
    if (ctx.resources != nullptr) {
        ctx.resources->InvalidateMaterial(doc.path);
    }
    EditorLogInfo("Material Editor: saved " + doc.path);
    return true;
}

void MaterialEditorPanel::ApplyToSelection(EditorContext& ctx, Doc& doc) {
    if (ctx.level == nullptr || ctx.resources == nullptr) {
        return;
    }
    if (ctx.selection.kind != EEditorSelectionKind::StaticMesh ||
        ctx.selection.index >= ctx.level->StaticMeshes().size()) {
        EditorLogWarn("Material Editor: select a StaticMesh to apply");
        return;
    }
    if (doc.dirty) {
        (void)SaveDoc(ctx, doc);
    }
    StaticMeshComponent& mesh = ctx.level->StaticMeshes()[ctx.selection.index];
    mesh.material = ctx.resources->LoadMaterial(doc.path);
    mesh.materialOverride = true;
    mesh.materialPath = doc.path;
    ctx.MarkDirty();
    EditorLogInfo("Applied material to selection");
}

void MaterialEditorPanel::DrawDoc(EditorContext& ctx, Doc& doc) {
    if (doc.focusOnce) {
        ImGui::SetNextWindowFocus();
        doc.focusOnce = false;
    }

    const std::string title =
        std::string("Material Editor — ") + doc.data.name + (doc.dirty ? " *" : "") +
        "###MatEdit" + doc.path;
    if (!ImGui::Begin(title.c_str(), &doc.open)) {
        ImGui::End();
        return;
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        focusedPath_ = doc.path;
    }

    ImGui::TextDisabled("%s", doc.path.c_str());
    if (doc.dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.2f, 1.0f), "*");
    }

    if (ImGui::InputText("Name", doc.nameBuf, sizeof(doc.nameBuf))) {
        doc.dirty = true;
    }

    int shading = doc.data.material.shading == EShadingModel::Unlit ? 1 : 0;
    if (ImGui::Combo("ShadingModel", &shading, "DefaultLit\0Unlit\0")) {
        doc.data.material.shading =
            shading == 1 ? EShadingModel::Unlit : EShadingModel::BlinnPhong;
        doc.dirty = true;
    }

    if (ImGui::ColorEdit3("BaseColor", &doc.data.material.albedo.x)) {
        doc.dirty = true;
    }
    if (ImGui::ColorEdit3("Specular", &doc.data.material.specular.x)) {
        doc.dirty = true;
    }
    if (ImGui::SliderFloat("Metallic", &doc.data.material.metallic, 0.0f, 1.0f)) {
        doc.dirty = true;
    }
    if (ImGui::SliderFloat("Roughness", &doc.data.material.roughness, 0.04f, 1.0f)) {
        doc.dirty = true;
    }
    if (ImGui::SliderFloat("Opacity", &doc.data.material.alpha, 0.0f, 1.0f)) {
        doc.dirty = true;
    }
    if (ImGui::DragFloat2("UVScale", &doc.data.material.uvScale.x, 0.05f, 0.01f, 64.0f)) {
        doc.dirty = true;
    }
    if (ImGui::Checkbox("CastsShadows", &doc.data.material.castsShadows)) {
        doc.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("PlanarMirror", &doc.data.material.planarMirror)) {
        doc.dirty = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Textures");
    if (ImGui::InputText("BaseColorMap", doc.baseMapBuf, sizeof(doc.baseMapBuf))) {
        doc.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("…##base")) {
        const std::string picked =
            EditorPickOpenFile("Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0All\0*.*\0", "BaseColor map");
        if (!picked.empty()) {
            (void)std::snprintf(doc.baseMapBuf, sizeof(doc.baseMapBuf), "%s", picked.c_str());
            doc.dirty = true;
        }
    }
    if (ImGui::InputText("NormalMap", doc.normalMapBuf, sizeof(doc.normalMapBuf))) {
        doc.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("…##norm")) {
        const std::string picked =
            EditorPickOpenFile("Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0All\0*.*\0", "Normal map");
        if (!picked.empty()) {
            (void)std::snprintf(doc.normalMapBuf, sizeof(doc.normalMapBuf), "%s", picked.c_str());
            doc.dirty = true;
        }
    }
    ImGui::TextDisabled("Paths: relative asset path, absolute, or checker / bump");

    ImGui::Separator();
    if (ImGui::Button("Save")) {
        (void)SaveDoc(ctx, doc);
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply to Selection")) {
        ApplyToSelection(ctx, doc);
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert") && doc.dirty) {
        LeonMaterialDocument data;
        if (LoadLeonMaterialDocument(doc.path, data)) {
            doc.data = std::move(data);
            (void)std::snprintf(doc.nameBuf, sizeof(doc.nameBuf), "%s", doc.data.name.c_str());
            (void)std::snprintf(doc.baseMapBuf, sizeof(doc.baseMapBuf), "%s",
                                doc.data.baseColorMapPath.c_str());
            (void)std::snprintf(doc.normalMapBuf, sizeof(doc.normalMapBuf), "%s",
                                doc.data.normalMapPath.c_str());
            doc.dirty = false;
        }
    }

    ImGui::End();
}

void MaterialEditorPanel::Draw(EditorContext& ctx) {
    if (!ctx.requestOpenMaterialPath.empty()) {
        OpenMaterial(ctx.requestOpenMaterialPath);
        ctx.requestOpenMaterialPath.clear();
        ctx.showMaterialEditor = true;
    }

    // Host tab when no documents (so Window → Material Editor always has a dock target).
    if (ctx.showMaterialEditor && docs_.empty()) {
        if (ImGui::Begin("Material Editor", &ctx.showMaterialEditor)) {
            ImGui::TextWrapped(
                "Double-click a .lmat in the Content Browser to open it as a tab.\n"
                "Each material opens in its own dockable window (drag tabs like Unreal).");
        }
        ImGui::End();
    }

    for (Doc& doc : docs_) {
        if (doc.open) {
            DrawDoc(ctx, doc);
        }
    }
    docs_.erase(std::remove_if(docs_.begin(), docs_.end(),
                               [](const Doc& d) { return !d.open; }),
                docs_.end());
    SyncDirtyPaths(ctx);
}

} // namespace leon::editor
