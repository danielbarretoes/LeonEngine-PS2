#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <leon/content/CookedSkeletal.h>
#include <leon/core/Paths.h>
#include <leon/core/Window.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorFileDialog.h>
#include <leon/editor/panels/AssetPreviewPanel.h>
#include <leon/level/Light.h>
#include <leon/render/Renderer.h>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

std::string extensionLower(const fs::path& p) {
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e;
}

std::string joinRelative(const fs::path& baseFile, const std::string& rel) {
    if (rel.empty()) {
        return {};
    }
    fs::path r(rel);
    if (r.is_absolute()) {
        return r.generic_string();
    }
    return (baseFile.parent_path() / r).lexically_normal().generic_string();
}

} // namespace

void AssetPreviewPanel::ClearPreview() {
    kind_ = EAssetPreviewKind::None;
    loadedPath_.clear();
    materialPath_.clear();
    staticMesh_.reset();
    skeletalMesh_.reset();
    bonePose_.clear();
    previewAnim_ = {};
    animTime_ = 0.0f;
    useMaterialOverride_ = false;
    previewLevel_.Clear();
    status_.clear();
}

bool AssetPreviewPanel::LoadStatic(EditorContext& ctx, const std::string& path) {
    if (ctx.resources == nullptr) {
        return false;
    }
    staticMesh_ = ctx.resources->LoadStaticMesh(path);
    if (staticMesh_ == nullptr || !staticMesh_->Valid()) {
        status_ = "Failed to load OBJ: " + path;
        return false;
    }
    kind_ = EAssetPreviewKind::StaticMesh;
    loadedPath_ = path;
    overrideMaterial_ = ctx.resources->DefaultMaterial();
    useMaterialOverride_ = false;
    materialPath_.clear();
    materialEdit_[0] = '\0';

    previewLevel_.Clear();
    previewLevel_.DirectionalLights().clear();
    previewLevel_.DirectionalLights().push_back(DirectionalLight{});
    StaticMeshComponent comp;
    comp.mesh = staticMesh_;
    comp.editorClass = "StaticMesh";
    comp.meshPath = path;
    comp.materialOverride = false;
    previewLevel_.AddStaticMesh(std::move(comp));

    camera_.SetTarget({0.0f, 0.5f, 0.0f});
    camera_.SetDistance(3.5f);
    camera_.SetYawPitch(35.0f, 20.0f);
    status_ = "Static mesh: " + fs::path(path).filename().string();
    return true;
}

bool AssetPreviewPanel::LoadSkeletalCooked(EditorContext& ctx, const std::string& skelmeshJson) {
    SkeletalMeshData data;
    std::string materialRel;
    if (!LoadSkeletalMesh(skelmeshJson, data, nullptr, &materialRel)) {
        status_ = "Failed to load skelmesh: " + skelmeshJson;
        return false;
    }
    skeletalMesh_ = std::make_unique<SkeletalMesh>(SkeletalMesh::Upload(std::move(data)));
    if (skeletalMesh_ == nullptr || !skeletalMesh_->Valid()) {
        status_ = "GPU upload failed for skelmesh";
        return false;
    }

    materialPath_ = joinRelative(skelmeshJson, materialRel);
    if (!materialPath_.empty() && ctx.resources != nullptr) {
        skeletalMesh_->SetMaterial(ctx.resources->LoadMaterial(materialPath_));
        (void)std::snprintf(materialEdit_, sizeof(materialEdit_), "%s", materialPath_.c_str());
    }
    overrideMaterial_ = skeletalMesh_->GetMaterial();
    useMaterialOverride_ = false;

    previewAnim_ = skeletalMesh_->EmbeddedAnim();
    bonePose_.assign(static_cast<std::size_t>(skeletalMesh_->GetSkeleton().BoneCount()),
                     glm::mat4(1.0f));
    if (previewAnim_.FrameCount() > 0) {
        previewAnim_.SampleLocalPose(0.0f, bonePose_);
    } else {
        // Bind pose: identity skin (boneWorld * invBind ≈ I when boneWorld = inv(invBind)).
        const Skeleton& sk = skeletalMesh_->GetSkeleton();
        for (int i = 0; i < sk.BoneCount(); ++i) {
            bonePose_[static_cast<std::size_t>(i)] =
                glm::inverse(sk.inverseBindPose[static_cast<std::size_t>(i)]);
        }
    }

    kind_ = EAssetPreviewKind::SkeletalMesh;
    loadedPath_ = skelmeshJson;
    previewLevel_.Clear();
    previewLevel_.DirectionalLights().clear();
    previewLevel_.DirectionalLights().push_back(DirectionalLight{});

    const float fit = skeletalMesh_->FitUniformScale(1.85f);
    camera_.SetTarget({0.0f, 0.9f, 0.0f});
    camera_.SetDistance(3.5f / std::max(fit, 0.25f));
    camera_.SetYawPitch(25.0f, 12.0f);
    status_ = "Skeletal mesh: " + fs::path(skelmeshJson).filename().string();
    (void)fit;
    return true;
}

bool AssetPreviewPanel::LoadCharacter(EditorContext& ctx, const std::string& characterAsset) {
    CharacterVisualDesc desc;
    if (!LoadCharacterVisual(characterAsset, desc)) {
        status_ = "Failed to load character (.lchar)";
        return false;
    }
    const std::string skelmesh = joinRelative(characterAsset, desc.skeletalMeshRel);
    if (!LoadSkeletalCooked(ctx, skelmesh)) {
        return false;
    }
    kind_ = EAssetPreviewKind::Character;
    loadedPath_ = characterAsset;

    // Prefer idle/run from blendspace first sample when present.
    if (!desc.blendSpaceRel.empty()) {
        BlendSpace1DAssetDesc bs;
        const std::string bsPath = joinRelative(characterAsset, desc.blendSpaceRel);
        if (LoadBlendSpace1DJson(bsPath, bs) && !bs.samples.empty()) {
            AnimSequence clip;
            const std::string animPath = joinRelative(bsPath, bs.samples.front().animRelPath);
            if (LoadAnimSequence(animPath, clip) && clip.FrameCount() > 0) {
                previewAnim_ = std::move(clip);
                animTime_ = 0.0f;
                previewAnim_.SampleLocalPose(0.0f, bonePose_);
            }
        }
    }
    status_ = "Character: " + fs::path(characterAsset).filename().string();
    return true;
}

bool AssetPreviewPanel::LoadMaterialOnly(EditorContext& ctx, const std::string& materialJson) {
    if (ctx.resources == nullptr) {
        return false;
    }
    overrideMaterial_ = ctx.resources->LoadMaterial(materialJson);
    materialPath_ = materialJson;
    (void)std::snprintf(materialEdit_, sizeof(materialEdit_), "%s", materialJson.c_str());
    useMaterialOverride_ = true;

    if (kind_ == EAssetPreviewKind::None || staticMesh_ == nullptr) {
        // Preview material on a sphere (Unreal Material Editor vibe).
        staticMesh_ = ctx.resources->GetSphereMesh(48, 32);
        kind_ = EAssetPreviewKind::Material;
        loadedPath_ = materialJson;
        previewLevel_.Clear();
        previewLevel_.DirectionalLights().clear();
        previewLevel_.DirectionalLights().push_back(DirectionalLight{});
        StaticMeshComponent comp;
        comp.mesh = staticMesh_;
        comp.editorClass = "Sphere";
        comp.materialOverride = true;
        comp.material = overrideMaterial_;
        comp.materialPath = materialJson;
        previewLevel_.AddStaticMesh(std::move(comp));
        camera_.SetTarget({0.0f, 0.0f, 0.0f});
        camera_.SetDistance(2.8f);
        camera_.SetYawPitch(40.0f, 18.0f);
    } else if (kind_ == EAssetPreviewKind::StaticMesh || kind_ == EAssetPreviewKind::Material) {
        if (!previewLevel_.StaticMeshes().empty()) {
            auto& m = previewLevel_.StaticMeshes().front();
            m.materialOverride = true;
            m.material = overrideMaterial_;
            m.materialPath = materialJson;
        }
    } else if (skeletalMesh_ != nullptr) {
        skeletalMesh_->SetMaterial(overrideMaterial_);
    }
    status_ = "Material: " + fs::path(materialJson).filename().string();
    return true;
}

void AssetPreviewPanel::ReloadIfNeeded(EditorContext& ctx) {
    if (!ctx.requestPreviewReload && ctx.previewAssetPath == loadedPath_ && !loadedPath_.empty()) {
        return;
    }
    ctx.requestPreviewReload = false;
    if (ctx.previewAssetPath.empty()) {
        return;
    }

    ClearPreview();
    const fs::path p(ctx.previewAssetPath);
    const std::string path = p.generic_string();
    const std::string ext = extensionLower(p);
    const std::string fname = p.filename().string();

    if (ext == ".obj" || ext == ".lmesh") {
        (void)LoadStatic(ctx, path);
    } else if (ext == ".lchar" || fname.find(".character.json") != std::string::npos) {
        (void)LoadCharacter(ctx, path);
    } else if (ext == ".lskm") {
        (void)LoadSkeletalCooked(ctx, path);
    } else if (ext == ".lmat") {
        (void)LoadMaterialOnly(ctx, path);
    } else if (ext == ".lanim") {
        status_ = "Anim (.lanim) — open a Character to preview with pose";
        loadedPath_ = path;
        kind_ = EAssetPreviewKind::None;
    } else if (ext == ".fbx") {
        status_ = "FBX source — use File → Import to cook, then preview the cooked asset";
        loadedPath_ = path;
    } else {
        status_ = "Unsupported preview: " + fname;
    }
}

void AssetPreviewPanel::HandleOrbit(EditorContext& ctx) {
    if (ctx.window == nullptr || !ImGui::IsWindowHovered()) {
        orbitDragging_ = false;
        return;
    }
    Window& window = *ctx.window;
    const bool rmb = window.IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);
    double mx = 0.0;
    double my = 0.0;
    window.GetCursorPos(mx, my);
    if (rmb) {
        if (orbitDragging_) {
            camera_.Orbit(static_cast<float>(mx - lastMouseX_) * 0.25f,
                          -static_cast<float>(my - lastMouseY_) * 0.25f);
        }
        orbitDragging_ = true;
        lastMouseX_ = mx;
        lastMouseY_ = my;
    } else {
        orbitDragging_ = false;
        lastMouseX_ = mx;
        lastMouseY_ = my;
    }
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f && ImGui::IsWindowHovered()) {
        camera_.Zoom(-wheel * 0.35f);
    }
}

void AssetPreviewPanel::PlaceStaticInLevel(EditorContext& ctx) {
    if (ctx.level == nullptr || ctx.resources == nullptr || staticMesh_ == nullptr) {
        return;
    }
    if (kind_ != EAssetPreviewKind::StaticMesh && kind_ != EAssetPreviewKind::Material) {
        return;
    }
    StaticMeshComponent comp;
    comp.mesh = staticMesh_;
    comp.editorClass = "StaticMesh";
    comp.meshPath = MakePackRelativeAssetPath(ctx, loadedPath_);
    comp.transform.position = ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0.5f, 0};
    if (useMaterialOverride_ || !materialPath_.empty()) {
        comp.materialOverride = true;
        comp.material = useMaterialOverride_ ? overrideMaterial_ : comp.material;
        comp.materialPath = MakePackRelativeAssetPath(ctx, materialPath_);
        if (!materialPath_.empty()) {
            comp.material = ctx.resources->LoadMaterial(materialPath_);
        }
    }
    ctx.level->AddStaticMesh(std::move(comp));
    ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1);
    ctx.MarkDirty();
    status_ = "Placed StaticMesh in Level";
}

void AssetPreviewPanel::RenderPreview(EditorContext& ctx) {
    if (ctx.renderer == nullptr || kind_ == EAssetPreviewKind::None) {
        return;
    }
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const int vw = std::max(1, static_cast<int>(size.x));
    const int vh = std::max(1, static_cast<int>(size.y - 8.0f));
    const ImGuiIO& io = ImGui::GetIO();
    const int fbW = std::max(
        1, static_cast<int>(std::lround(vw * std::max(io.DisplayFramebufferScale.x, 1.0f))));
    const int fbH = std::max(
        1, static_cast<int>(std::lround(vh * std::max(io.DisplayFramebufferScale.y, 1.0f))));
    if (!target_.EnsureSize(fbW, fbH)) {
        return;
    }

    camera_.SetPerspective(50.0f, static_cast<float>(vw) / static_cast<float>(vh), 0.05f, 200.0f);

    if (playAnim_ && skeletalMesh_ != nullptr && previewAnim_.FrameCount() > 0) {
        animTime_ += ctx.deltaTime;
        previewAnim_.SampleLocalPose(animTime_, bonePose_);
    }

    // Apply material override to preview level mesh when static.
    if ((kind_ == EAssetPreviewKind::StaticMesh || kind_ == EAssetPreviewKind::Material) &&
        !previewLevel_.StaticMeshes().empty() && useMaterialOverride_) {
        previewLevel_.StaticMeshes().front().materialOverride = true;
        previewLevel_.StaticMeshes().front().material = overrideMaterial_;
    }

    ctx.renderer->SetDrawFramebuffer(target_.fbo());
    ctx.renderer->BeginFrame(fbW, fbH);

    if (skeletalMesh_ != nullptr &&
        (kind_ == EAssetPreviewKind::SkeletalMesh || kind_ == EAssetPreviewKind::Character)) {
        Transform t{};
        const float fit = skeletalMesh_->FitUniformScale(1.85f);
        t.scale = {fit, fit, fit};
        if (useMaterialOverride_) {
            skeletalMesh_->SetMaterial(overrideMaterial_);
        }
        ctx.renderer->SubmitSkeletalDraw(*skeletalMesh_, t, bonePose_);
    }

    ctx.renderer->DrawScene(previewLevel_, camera_);
    ctx.renderer->SetDrawFramebuffer(0);
    target_.End();

    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(target_.colorTexture())),
                 ImVec2(static_cast<float>(vw), static_cast<float>(vh)), ImVec2(0, 1),
                 ImVec2(1, 0));
}

void AssetPreviewPanel::Draw(EditorContext& ctx) {
    if (!ctx.showAssetPreview) {
        return;
    }
    if (!ImGui::Begin("Asset Preview", &ctx.showAssetPreview)) {
        ImGui::End();
        return;
    }

    ReloadIfNeeded(ctx);

    if (ImGui::Button("Import…")) {
        ctx.requestOpenImportDialog = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        ClearPreview();
        ctx.previewAssetPath.clear();
    }
    ImGui::SameLine();
    if ((kind_ == EAssetPreviewKind::StaticMesh || kind_ == EAssetPreviewKind::Material) &&
        ImGui::Button("Place in Level")) {
        PlaceStaticInLevel(ctx);
    }

    if (skeletalMesh_ != nullptr && previewAnim_.FrameCount() > 0) {
        ImGui::SameLine();
        ImGui::Checkbox("Play Anim", &playAnim_);
    }

    ImGui::Separator();
    ImGui::InputText("Material (.lmat)", materialEdit_, sizeof(materialEdit_));
    ImGui::SameLine();
    if (ImGui::Button("Apply##mat")) {
        if (materialEdit_[0] != '\0' && ctx.resources != nullptr) {
            const std::string resolved = ResolveAssetPath(materialEdit_);
            const std::string path = fs::exists(resolved) ? resolved : std::string(materialEdit_);
            (void)LoadMaterialOnly(ctx, path);
            useMaterialOverride_ = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##mat")) {
        const std::string picked =
            EditorPickOpenFile("Leon Material\0*.lmat\0All\0*.*\0", "Material");
        if (!picked.empty()) {
            (void)std::snprintf(materialEdit_, sizeof(materialEdit_), "%s", picked.c_str());
            (void)LoadMaterialOnly(ctx, picked);
            useMaterialOverride_ = true;
        }
    }
    ImGui::Checkbox("Use material override", &useMaterialOverride_);

    if (!status_.empty()) {
        ImGui::TextWrapped("%s", status_.c_str());
    }

    HandleOrbit(ctx);
    RenderPreview(ctx);

    ImGui::End();
}

} // namespace leon::editor
