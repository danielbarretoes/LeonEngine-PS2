#include <leon/editor/MaterialSpherePreview.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <leon/core/Camera.h>
#include <leon/core/Paths.h>
#include <leon/level/Light.h>
#include <leon/render/Renderer.h>
#include <leon/render/ResourceCache.h>

namespace leon::editor {
namespace {

void SetupSphereScene(Level& level, Camera& camera, const std::shared_ptr<StaticMesh>& sphere,
                      const Material& material, const std::string& materialPath) {
    level.Clear();
    level.DirectionalLights().clear();
    DirectionalLight sun{};
    sun.intensity = 1.35f;
    level.DirectionalLights().push_back(sun);

    StaticMeshComponent comp;
    comp.mesh = sphere;
    comp.editorClass = "Sphere";
    comp.materialOverride = true;
    comp.material = material;
    comp.materialPath = materialPath;
    level.AddStaticMesh(std::move(comp));

    camera.SetTarget({0.0f, 0.0f, 0.0f});
    camera.SetDistance(2.55f);
    camera.SetYawPitch(35.0f, 16.0f);
}

} // namespace

void MaterialSpherePreview::EnsureScene(ResourceCache& resources) {
    if (ready_ && sphere_ != nullptr) {
        return;
    }
    sphere_ = resources.GetSphereMesh(40, 28);
    SetupSphereScene(previewLevel_, camera_, sphere_, material_, materialPath_);
    ready_ = true;
}

void MaterialSpherePreview::SetMaterialPath(ResourceCache& resources, const std::string& materialPath) {
    if (materialPath == materialPath_ && ready_) {
        return;
    }
    materialPath_ = materialPath;
    if (!materialPath_.empty()) {
        const std::string resolved = ResolveAssetPath(materialPath_);
        material_ = resources.LoadMaterial(resolved.empty() ? materialPath_ : resolved);
    } else {
        material_ = resources.DefaultMaterial();
    }
    EnsureScene(resources);
    if (!previewLevel_.StaticMeshes().empty()) {
        auto& mesh = previewLevel_.StaticMeshes().front();
        mesh.materialOverride = true;
        mesh.material = material_;
        mesh.materialPath = materialPath_;
    }
}

void MaterialSpherePreview::Draw(Renderer& renderer, float width, float height) {
    if (!ready_ || sphere_ == nullptr) {
        ImGui::Dummy(ImVec2(width, height));
        return;
    }

    const int vw = std::max(1, static_cast<int>(width));
    const int vh = std::max(1, static_cast<int>(height));
    const ImGuiIO& io = ImGui::GetIO();
    const int fbW =
        std::max(1, static_cast<int>(std::lround(vw * std::max(io.DisplayFramebufferScale.x, 1.0f))));
    const int fbH =
        std::max(1, static_cast<int>(std::lround(vh * std::max(io.DisplayFramebufferScale.y, 1.0f))));
    if (!target_.EnsureSize(fbW, fbH)) {
        ImGui::Dummy(ImVec2(width, height));
        return;
    }

    camera_.SetPerspective(48.0f, static_cast<float>(vw) / static_cast<float>(vh), 0.05f, 100.0f);
    camera_.Orbit(ImGui::GetIO().DeltaTime * 18.0f, 0.0f);

    renderer.SetDrawFramebuffer(target_.fbo());
    renderer.BeginFrame(fbW, fbH);
    renderer.DrawScene(previewLevel_, camera_);
    renderer.SetDrawFramebuffer(0);
    target_.End();

    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(target_.colorTexture())),
                 ImVec2(static_cast<float>(vw), static_cast<float>(vh)), ImVec2(0, 1), ImVec2(1, 0));
}

void MaterialThumbnailCache::EnsureSharedScene(ResourceCache& resources) {
    if (sceneReady_ && sphere_ != nullptr) {
        return;
    }
    sphere_ = resources.GetSphereMesh(32, 24);
    SetupSphereScene(previewLevel_, camera_, sphere_, resources.DefaultMaterial(), {});
    sceneReady_ = true;
}

bool MaterialThumbnailCache::RenderThumb(Renderer& renderer, ResourceCache& resources, Thumb& thumb,
                                         const std::string& materialPath, int pixelSize) {
    EnsureSharedScene(resources);
    if (sphere_ == nullptr) {
        return false;
    }

    const int px = std::max(32, pixelSize);
    if (!thumb.target.EnsureSize(px, px)) {
        return false;
    }

    const std::string resolved = ResolveAssetPath(materialPath);
    Material mat = resources.LoadMaterial(resolved.empty() ? materialPath : resolved);
    if (!previewLevel_.StaticMeshes().empty()) {
        auto& mesh = previewLevel_.StaticMeshes().front();
        mesh.materialOverride = true;
        mesh.material = mat;
        mesh.materialPath = materialPath;
    }

    camera_.SetPerspective(48.0f, 1.0f, 0.05f, 100.0f);
    camera_.SetYawPitch(35.0f, 16.0f);

    renderer.SetDrawFramebuffer(thumb.target.fbo());
    renderer.BeginFrame(px, px);
    renderer.DrawScene(previewLevel_, camera_);
    renderer.SetDrawFramebuffer(0);
    thumb.target.End();
    thumb.ready = true;
    return true;
}

unsigned int MaterialThumbnailCache::Ensure(Renderer& renderer, ResourceCache& resources,
                                            const std::string& materialPath, int pixelSize,
                                            int& budget) {
    if (materialPath.empty()) {
        return 0;
    }
    Thumb& thumb = thumbs_[materialPath];
    if (thumb.ready && thumb.target.Valid()) {
        return thumb.target.colorTexture();
    }
    if (budget <= 0) {
        return 0;
    }
    if (RenderThumb(renderer, resources, thumb, materialPath, pixelSize)) {
        --budget;
        return thumb.target.colorTexture();
    }
    return 0;
}

void MaterialThumbnailCache::Clear() {
    thumbs_.clear();
}

bool MeshThumbnailCache::RenderThumb(Renderer& renderer, ResourceCache& resources, Thumb& thumb,
                                     const std::string& meshPath, int pixelSize) {
    auto mesh = resources.LoadStaticMesh(meshPath);
    if (mesh == nullptr || !mesh->Valid()) {
        return false;
    }

    const int px = std::max(32, pixelSize);
    if (!thumb.target.EnsureSize(px, px)) {
        return false;
    }

    previewLevel_.Clear();
    previewLevel_.DirectionalLights().clear();
    DirectionalLight sun{};
    sun.intensity = 1.35f;
    previewLevel_.DirectionalLights().push_back(sun);

    StaticMeshComponent comp;
    comp.mesh = mesh;
    comp.editorClass = "StaticMesh";
    comp.meshPath = meshPath;
    comp.materialOverride = false;
    comp.material = resources.DefaultMaterial();
    previewLevel_.AddStaticMesh(std::move(comp));

    const glm::vec3 mn = mesh->LocalMin();
    const glm::vec3 mx = mesh->LocalMax();
    const glm::vec3 center = (mn + mx) * 0.5f;
    const float radius = std::max(0.08f, glm::length(mx - mn) * 0.5f);

    camera_.SetMode(ECameraMode::Orbit);
    camera_.SetTarget(center);
    camera_.SetYawPitch(35.0f, 18.0f);
    // Fit the full AABB in a ~48° FOV square view with a little padding.
    const float fitDistance = radius / std::tan(glm::radians(24.0f)) * 1.15f;
    camera_.SetDistance(std::max(0.35f, fitDistance));
    camera_.SetPerspective(48.0f, 1.0f, std::max(0.01f, radius * 0.01f),
                           std::max(50.0f, radius * 40.0f));

    renderer.SetDrawFramebuffer(thumb.target.fbo());
    renderer.BeginFrame(px, px);
    renderer.DrawScene(previewLevel_, camera_);
    renderer.SetDrawFramebuffer(0);
    thumb.target.End();
    thumb.ready = true;
    return true;
}

unsigned int MeshThumbnailCache::Ensure(Renderer& renderer, ResourceCache& resources,
                                        const std::string& meshPath, int pixelSize, int& budget) {
    if (meshPath.empty()) {
        return 0;
    }
    Thumb& thumb = thumbs_[meshPath];
    if (thumb.ready && thumb.target.Valid()) {
        return thumb.target.colorTexture();
    }
    if (budget <= 0) {
        return 0;
    }
    if (RenderThumb(renderer, resources, thumb, meshPath, pixelSize)) {
        --budget;
        return thumb.target.colorTexture();
    }
    return 0;
}

void MeshThumbnailCache::Clear() {
    thumbs_.clear();
}

} // namespace leon::editor
