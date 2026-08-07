#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <imgui.h>
#include <ImGuizmo.h>
#include <iostream>
#include <leon/core/Camera.h>
#include <leon/core/EKey.h>
#include <leon/core/Paths.h>
#include <leon/core/Window.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EngineContent.h>
#include <leon/editor/panels/ViewportPanel.h>
#include <leon/gameplay/Actor.h>
#include <leon/gameplay/World.h>
#include <leon/level/BasicShape.h>
#include <leon/level/Level.h>
#include <leon/physics/BodyInstance.h>
#include <leon/physics/PhysScene.h>
#include <leon/render/Frustum.h>
#include <leon/render/Renderer.h>
#include <limits>
#include <optional>
#include <string>

namespace leon::editor {
namespace {

constexpr glm::vec3 kSelectionColor{1.0f, 0.55f, 0.12f};

void AddPointAabb(Renderer& renderer, const glm::vec3& center, float halfExtent) {
    const glm::vec3 ext{halfExtent, halfExtent, halfExtent};
    renderer.AddDebugAabb(center - ext, center + ext, kSelectionColor);
}

Actor* FindActorByIndex(World* world, std::size_t index) {
    if (world == nullptr) {
        return nullptr;
    }
    Actor* target = nullptr;
    std::size_t i = 0;
    world->ForEachActor([&](Actor& actor) {
        if (i == index) {
            target = &actor;
        }
        ++i;
    });
    return target;
}

Actor* ResolveSelectedActor(EditorContext& ctx) {
    if (ctx.selection.kind != EEditorSelectionKind::Actor) {
        return nullptr;
    }
    if (ctx.selection.id != 0 && ctx.world != nullptr) {
        if (Actor* byId = ctx.world->FindActorByEditorId(ctx.selection.id)) {
            return byId;
        }
    }
    return FindActorByIndex(ctx.world, ctx.selection.index);
}

} // namespace

void ViewportPanel::EnsureEditorFreeLook(Camera& camera) {
    if (camera.Mode() == ECameraMode::FreeLook) {
        return;
    }
    const glm::vec3 eye = camera.GetCameraLocation();
    camera.SetMode(ECameraMode::FreeLook);
    camera.SetEyeLocation(eye);
}

Transform* ViewportPanel::SelectedTransform(EditorContext& ctx) {
    if (ctx.level == nullptr || !ctx.selection.IsValid()) {
        return nullptr;
    }
    switch (ctx.selection.kind) {
    case EEditorSelectionKind::StaticMesh:
        if (ctx.selection.index < ctx.level->StaticMeshes().size()) {
            return &ctx.level->StaticMeshes()[ctx.selection.index].transform;
        }
        break;
    case EEditorSelectionKind::DirectionalLight:
        if (ctx.selection.index < ctx.level->DirectionalLights().size()) {
            return &ctx.level->DirectionalLights()[ctx.selection.index].transform;
        }
        break;
    case EEditorSelectionKind::PointLight:
        if (ctx.selection.index < ctx.level->PointLights().size()) {
            return &ctx.level->PointLights()[ctx.selection.index].transform;
        }
        break;
    case EEditorSelectionKind::PlayerStart:
        if (ctx.selection.index < ctx.level->PlayerStarts().size()) {
            return &ctx.level->PlayerStarts()[ctx.selection.index].transform;
        }
        break;
    case EEditorSelectionKind::TriggerVolume:
        if (ctx.selection.index < ctx.level->TriggerVolumes().size()) {
            return &ctx.level->TriggerVolumes()[ctx.selection.index].transform;
        }
        break;
    case EEditorSelectionKind::PainCausingVolume:
        if (ctx.selection.index < ctx.level->PainCausingVolumes().size()) {
            return &ctx.level->PainCausingVolumes()[ctx.selection.index].transform;
        }
        break;
    case EEditorSelectionKind::AISpawnPoint:
        if (ctx.selection.index < ctx.level->AISpawnPoints().size()) {
            return &ctx.level->AISpawnPoints()[ctx.selection.index].transform;
        }
        break;
    case EEditorSelectionKind::Actor: {
        Actor* actor = ResolveSelectedActor(ctx);
        if (actor == nullptr) {
            return nullptr;
        }
        actorGizmoScratch_.position = actor->GetActorLocation();
        actorGizmoScratch_.rotationDegrees = {0.0f, actor->GetActorYaw(), 0.0f};
        actorGizmoScratch_.scale = {1.0f, 1.0f, 1.0f};
        return &actorGizmoScratch_;
    }
    default:
        break;
    }
    return nullptr;
}

bool ViewportPanel::SelectedFocusPoint(EditorContext& ctx, glm::vec3& outPoint) {
    if (Transform* t = SelectedTransform(ctx)) {
        outPoint = t->position;
        return true;
    }
    return false;
}

void ViewportPanel::FocusSelection(EditorContext& ctx) {
    glm::vec3 point{};
    if (SelectedFocusPoint(ctx, point) && ctx.camera != nullptr) {
        if (ctx.camera->Mode() == ECameraMode::FreeLook) {
            const glm::vec3 forward = ctx.camera->ForwardVector();
            const float dist = ctx.camera->IsOrthographic() ? 40.0f : 5.0f;
            ctx.camera->SetEyeLocation(point - forward * dist);
        } else {
            ctx.camera->SetTarget(point);
        }
    }
}

void ViewportPanel::ApplyViewMode(EditorContext& ctx, Camera& camera) {
    if (ctx.viewMode == lastAppliedViewMode_) {
        return;
    }
    lastAppliedViewMode_ = ctx.viewMode;
    if (ctx.viewMode == EEditorViewMode::Perspective) {
        return;
    }

    EnsureEditorFreeLook(camera);
    const glm::vec3 pivot = camera.EyeLocation() + camera.ForwardVector() * 8.0f;
    constexpr float kDist = 40.0f;
    switch (ctx.viewMode) {
    case EEditorViewMode::OrthoTop:
        camera.SetYawPitch(90.0f, -89.5f);
        break;
    case EEditorViewMode::OrthoFront:
        camera.SetYawPitch(-90.0f, 0.0f);
        break;
    case EEditorViewMode::OrthoSide:
        camera.SetYawPitch(0.0f, 0.0f);
        break;
    default:
        break;
    }
    camera.SetEyeLocation(pivot - camera.ForwardVector() * kDist);
    if (camera.OrthoHeight() < 1.0f) {
        camera.SetOrthoHeight(20.0f);
    }
}

void ViewportPanel::DrawEditorHelpers(EditorContext& ctx) {
    if (ctx.renderer == nullptr || ctx.level == nullptr ||
        (ctx.piePlaying && !ctx.pieNewWindow)) {
        return;
    }
    Renderer& r = *ctx.renderer;
    constexpr glm::vec3 kPlayerStartColor{0.25f, 0.85f, 1.0f};
    for (const PlayerStart& start : ctx.level->PlayerStarts()) {
        const glm::vec3& p = start.transform.position;
        const glm::vec3 ext{0.35f, 0.35f, 0.35f};
        r.AddDebugAabb(p - ext, p + ext, kPlayerStartColor);
        const float yawRad = glm::radians(start.transform.rotationDegrees.y);
        const glm::vec3 forward{std::sin(yawRad), 0.0f, std::cos(yawRad)};
        r.AddDebugArrow(p + glm::vec3{0.0f, 0.9f, 0.0f},
                        p + glm::vec3{0.0f, 0.9f, 0.0f} + forward * 0.9f, kPlayerStartColor);
    }

    constexpr glm::vec3 kTriggerColor{0.95f, 0.9f, 0.2f};
    constexpr glm::vec3 kTriggerRadiusColor{0.2f, 0.9f, 0.95f};
    for (const TriggerVolume& volume : ctx.level->TriggerVolumes()) {
        const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
        const glm::vec3& p = volume.transform.position;
        r.AddDebugAabb(p - half, p + half, kTriggerColor);
        const float radius = volume.interactRadius > 0.0f ? volume.interactRadius : 0.0f;
        if (radius > 0.05f) {
            constexpr int kSegments = 16;
            glm::vec3 prev{p.x + radius, p.y, p.z};
            for (int i = 1; i <= kSegments; ++i) {
                const float angle =
                    (static_cast<float>(i) / static_cast<float>(kSegments)) * 6.2831853f;
                const glm::vec3 next{p.x + std::cos(angle) * radius, p.y,
                                     p.z + std::sin(angle) * radius};
                r.AddDebugLine(prev, next, kTriggerRadiusColor);
                prev = next;
            }
        }
    }

    constexpr glm::vec3 kPainColor{1.0f, 0.35f, 0.15f};
    for (const PainCausingVolume& volume : ctx.level->PainCausingVolumes()) {
        const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
        const glm::vec3& p = volume.transform.position;
        r.AddDebugAabb(p - half, p + half, kPainColor);
    }

    constexpr glm::vec3 kAISpawnColor{0.35f, 0.95f, 0.4f};
    for (const AISpawnPoint& point : ctx.level->AISpawnPoints()) {
        const glm::vec3& p = point.transform.position;
        const glm::vec3 ext{0.3f, 0.3f, 0.3f};
        r.AddDebugAabb(p - ext, p + ext, kAISpawnColor);
        const float yawRad = glm::radians(point.transform.rotationDegrees.y);
        const glm::vec3 forward{std::sin(yawRad), 0.0f, std::cos(yawRad)};
        r.AddDebugArrow(p + glm::vec3{0.0f, 0.75f, 0.0f},
                        p + glm::vec3{0.0f, 0.75f, 0.0f} + forward * 0.75f, kAISpawnColor);
    }
}

void ViewportPanel::DrawSelectionOverlay(EditorContext& ctx) {
    if (ctx.renderer == nullptr || ctx.level == nullptr || !ctx.selection.IsValid()) {
        return;
    }
    Renderer& r = *ctx.renderer;

    switch (ctx.selection.kind) {
    case EEditorSelectionKind::StaticMesh: {
        if (ctx.selection.index >= ctx.level->StaticMeshes().size()) {
            break;
        }
        const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[ctx.selection.index];
        if (mesh.mesh != nullptr && mesh.mesh->Valid()) {
            const Aabb box = Aabb::fromLocalTransformed(mesh.mesh->LocalMin(), mesh.mesh->LocalMax(),
                                                        mesh.EffectiveModelMatrix());
            r.AddDebugAabb(box.min, box.max, kSelectionColor);
        } else {
            AddPointAabb(r, mesh.transform.position, 0.35f);
        }
        break;
    }
    case EEditorSelectionKind::DirectionalLight:
        if (ctx.selection.index < ctx.level->DirectionalLights().size()) {
            AddPointAabb(r, ctx.level->DirectionalLights()[ctx.selection.index].transform.position,
                         0.25f);
        }
        break;
    case EEditorSelectionKind::PointLight:
        if (ctx.selection.index < ctx.level->PointLights().size()) {
            AddPointAabb(r, ctx.level->PointLights()[ctx.selection.index].transform.position, 0.25f);
        }
        break;
    case EEditorSelectionKind::PlayerStart:
        if (ctx.selection.index < ctx.level->PlayerStarts().size()) {
            AddPointAabb(r, ctx.level->PlayerStarts()[ctx.selection.index].transform.position, 0.35f);
        }
        break;
    case EEditorSelectionKind::TriggerVolume:
        if (ctx.selection.index < ctx.level->TriggerVolumes().size()) {
            const TriggerVolume& volume = ctx.level->TriggerVolumes()[ctx.selection.index];
            const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
            r.AddDebugAabb(volume.transform.position - half, volume.transform.position + half,
                           kSelectionColor);
        }
        break;
    case EEditorSelectionKind::PainCausingVolume:
        if (ctx.selection.index < ctx.level->PainCausingVolumes().size()) {
            const PainCausingVolume& volume = ctx.level->PainCausingVolumes()[ctx.selection.index];
            const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
            r.AddDebugAabb(volume.transform.position - half, volume.transform.position + half,
                           kSelectionColor);
        }
        break;
    case EEditorSelectionKind::AISpawnPoint:
        if (ctx.selection.index < ctx.level->AISpawnPoints().size()) {
            AddPointAabb(r, ctx.level->AISpawnPoints()[ctx.selection.index].transform.position,
                         0.35f);
        }
        break;
    case EEditorSelectionKind::Actor: {
        Actor* actor = ResolveSelectedActor(ctx);
        if (actor != nullptr) {
            AddPointAabb(r, actor->GetRootComponent().GetComponentLocation(), 0.4f);
        }
        break;
    }
    default:
        break;
    }
}

void ViewportPanel::HandleCameraInput(EditorContext& ctx, float deltaTime) {
    if (ctx.piePlaying && !ctx.pieNewWindow) {
        dragMode_ = EViewportDrag::None;
        flyActive_ = false;
        return;
    }
    if (ctx.camera == nullptr || ctx.window == nullptr) {
        dragMode_ = EViewportDrag::None;
        flyActive_ = false;
        return;
    }
    // Only block while actively dragging a gizmo — IsOver() was killing RMB look near handles.
    if (ImGuizmo::IsUsing()) {
        dragMode_ = EViewportDrag::None;
        return;
    }

    const bool ortho = ctx.viewMode != EEditorViewMode::Perspective;
    Window& window = *ctx.window;
    const ImGuiIO& io = ImGui::GetIO();
    // Prefer ImGui buttons/deltas so docking / DPI / capture match the Viewport image.
    const bool alt = io.KeyAlt;
    const bool rmb = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool mmb = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    const bool lmb = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool shift = io.KeyShift;
    const bool canStartDrag = ctx.viewportHovered || flyActive_;

    if (!ortho && rmb && canStartDrag && !alt) {
        if (!flyActive_) {
            EnsureEditorFreeLook(*ctx.camera);
            flyActive_ = true;
            if (ctx.viewportHovered) {
                ImGui::SetWindowFocus();
            }
        }
    } else if (!rmb) {
        flyActive_ = false;
    }

    EViewportDrag wanted = EViewportDrag::None;
    if (!ortho && alt && lmb && ctx.viewportHovered) {
        wanted = EViewportDrag::Orbit;
    } else if (!ortho && alt && rmb && ctx.viewportHovered) {
        wanted = EViewportDrag::Dolly;
    } else if ((mmb || (ortho && rmb)) && ctx.viewportHovered) {
        wanted = EViewportDrag::Pan;
    } else if (!ortho && flyActive_) {
        wanted = EViewportDrag::Look;
    }

    if (wanted != EViewportDrag::None) {
        if (dragMode_ == wanted) {
            const float dx = io.MouseDelta.x;
            const float dy = io.MouseDelta.y;
            switch (wanted) {
            case EViewportDrag::Orbit:
                if (ctx.camera->Mode() == ECameraMode::FreeLook) {
                    ctx.camera->AddLook(dx * 0.25f, -dy * 0.25f);
                } else {
                    ctx.camera->Orbit(dx * 0.25f, -dy * 0.25f);
                }
                break;
            case EViewportDrag::Look:
                EnsureEditorFreeLook(*ctx.camera);
                ctx.camera->AddLook(dx * 0.25f, -dy * 0.25f);
                break;
            case EViewportDrag::Pan: {
                const float scale =
                    ortho ? (ctx.camera->OrthoHeight() * 0.0025f)
                          : (std::max(ctx.camera->Distance(), 1.0f) * 0.0025f);
                ctx.camera->Pan(-dx * scale, dy * scale);
                break;
            }
            case EViewportDrag::Dolly:
                if (ctx.camera->Mode() == ECameraMode::FreeLook) {
                    ctx.camera->SetEyeLocation(ctx.camera->EyeLocation() +
                                               ctx.camera->ForwardVector() * (-dy * 0.05f));
                } else {
                    ctx.camera->Zoom(dy * 0.05f);
                }
                break;
            default:
                break;
            }
        }
        dragMode_ = wanted;
    } else {
        dragMode_ = EViewportDrag::None;
    }

    if (ctx.viewportHovered && !flyActive_) {
        const float wheel = io.MouseWheel;
        if (wheel != 0.0f) {
            if (ortho) {
                ctx.camera->SetOrthoHeight(ctx.camera->OrthoHeight() * (wheel > 0.0f ? 0.9f : 1.1f));
            } else if (ctx.camera->Mode() == ECameraMode::FreeLook) {
                ctx.camera->SetEyeLocation(ctx.camera->EyeLocation() +
                                           ctx.camera->ForwardVector() * (wheel * 0.5f));
            } else {
                ctx.camera->Zoom(-wheel * 0.5f);
            }
        }
    }

    const bool wantMove =
        (flyActive_ && !ortho) ||
        (ortho && ctx.viewportHovered &&
         (window.IsKeyPressed(leon::EKey::W) || window.IsKeyPressed(leon::EKey::S) ||
          window.IsKeyPressed(leon::EKey::A) || window.IsKeyPressed(leon::EKey::D) ||
          window.IsKeyPressed(leon::EKey::Q) || window.IsKeyPressed(leon::EKey::E)));
    if (wantMove && !io.WantTextInput) {
        EnsureEditorFreeLook(*ctx.camera);
        const float speed =
            (shift ? 18.0f : 6.0f) * deltaTime * (ortho ? ctx.camera->OrthoHeight() * 0.15f : 1.0f);
        glm::vec3 eye = ctx.camera->EyeLocation();
        const glm::vec3 forward = ctx.camera->ForwardVector();
        const glm::vec3 right = ctx.camera->RightVector();
        const glm::vec3 up{0.0f, 1.0f, 0.0f};
        if (ortho) {
            const glm::vec3 camUp = glm::normalize(glm::cross(right, forward));
            if (window.IsKeyPressed(leon::EKey::W)) {
                eye += camUp * speed;
            }
            if (window.IsKeyPressed(leon::EKey::S)) {
                eye -= camUp * speed;
            }
            if (window.IsKeyPressed(leon::EKey::A)) {
                eye -= right * speed;
            }
            if (window.IsKeyPressed(leon::EKey::D)) {
                eye += right * speed;
            }
        } else {
            if (window.IsKeyPressed(leon::EKey::W)) {
                eye += forward * speed;
            }
            if (window.IsKeyPressed(leon::EKey::S)) {
                eye -= forward * speed;
            }
            if (window.IsKeyPressed(leon::EKey::A)) {
                eye -= right * speed;
            }
            if (window.IsKeyPressed(leon::EKey::D)) {
                eye += right * speed;
            }
            if (window.IsKeyPressed(leon::EKey::Q)) {
                eye -= up * speed;
            }
            if (window.IsKeyPressed(leon::EKey::E)) {
                eye += up * speed;
            }
        }
        ctx.camera->SetEyeLocation(eye);
    }
}

void ViewportPanel::HandleViewportPicking(EditorContext& ctx) {
    Camera* cam = (ctx.pieNewWindow && ctx.editorViewCamera != nullptr) ? ctx.editorViewCamera
                                                                        : ctx.camera;
    if (ctx.level == nullptr || cam == nullptr || !ctx.viewportHovered) {
        return;
    }
    // Alt+LMB is orbit — do not pick.
    if (ImGui::GetIO().KeyAlt) {
        return;
    }
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const float imageW = std::max(1.0f, itemMax.x - itemMin.x);
    const float imageH = std::max(1.0f, itemMax.y - itemMin.y);
    const float localX = mouse.x - itemMin.x;
    const float localY = mouse.y - itemMin.y;
    if (localX < 0.0f || localY < 0.0f || localX > imageW || localY > imageH) {
        return;
    }

    // Mouse → world ray through the viewport image (handles letterboxing / size mismatch).
    const float ndcX = ((localX / imageW) * 2.0f) - 1.0f;
    const float ndcY = 1.0f - ((localY / imageH) * 2.0f);
    const glm::mat4 invVP = glm::inverse(cam->ProjectionMatrix() * cam->ViewMatrix());
    glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farH = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearH.w) < 1.0e-8f || std::abs(farH.w) < 1.0e-8f) {
        return;
    }
    nearH /= nearH.w;
    farH /= farH.w;
    const glm::vec3 rayOrigin = glm::vec3(nearH);
    const glm::vec3 rayDir = glm::normalize(glm::vec3(farH) - rayOrigin);

    float bestT = std::numeric_limits<float>::max();
    EEditorSelectionKind bestKind = EEditorSelectionKind::None;
    std::size_t bestIndex = 0;

    auto considerAabb = [&](EEditorSelectionKind kind, std::size_t index, const Aabb& box) {
        float t = 0.0f;
        if (!box.intersectRay(rayOrigin, rayDir, t) || t < 0.0f || t >= bestT) {
            return;
        }
        bestT = t;
        bestKind = kind;
        bestIndex = index;
    };

    for (std::size_t i = 0; i < ctx.level->StaticMeshes().size(); ++i) {
        const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[i];
        if (mesh.hidden) {
            continue;
        }
        if (mesh.mesh != nullptr && mesh.mesh->Valid()) {
            considerAabb(EEditorSelectionKind::StaticMesh, i,
                         Aabb::fromLocalTransformed(mesh.mesh->LocalMin(), mesh.mesh->LocalMax(),
                                                    mesh.EffectiveModelMatrix()));
        } else {
            const glm::vec3& p = mesh.transform.position;
            considerAabb(EEditorSelectionKind::StaticMesh, i,
                         Aabb{p - glm::vec3(0.35f), p + glm::vec3(0.35f)});
        }
    }

    for (std::size_t i = 0; i < ctx.level->PointLights().size(); ++i) {
        const glm::vec3& p = ctx.level->PointLights()[i].transform.position;
        considerAabb(EEditorSelectionKind::PointLight, i,
                     Aabb{p - glm::vec3(0.4f), p + glm::vec3(0.4f)});
    }
    for (std::size_t i = 0; i < ctx.level->PlayerStarts().size(); ++i) {
        const glm::vec3& p = ctx.level->PlayerStarts()[i].transform.position;
        considerAabb(EEditorSelectionKind::PlayerStart, i,
                     Aabb{p - glm::vec3(0.5f), p + glm::vec3(0.5f)});
    }
    for (std::size_t i = 0; i < ctx.level->TriggerVolumes().size(); ++i) {
        const TriggerVolume& volume = ctx.level->TriggerVolumes()[i];
        const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
        considerAabb(EEditorSelectionKind::TriggerVolume, i,
                     Aabb{volume.transform.position - half, volume.transform.position + half});
    }
    for (std::size_t i = 0; i < ctx.level->PainCausingVolumes().size(); ++i) {
        const PainCausingVolume& volume = ctx.level->PainCausingVolumes()[i];
        const glm::vec3 half = glm::abs(volume.transform.scale) * 0.5f;
        considerAabb(EEditorSelectionKind::PainCausingVolume, i,
                     Aabb{volume.transform.position - half, volume.transform.position + half});
    }
    for (std::size_t i = 0; i < ctx.level->AISpawnPoints().size(); ++i) {
        const glm::vec3& p = ctx.level->AISpawnPoints()[i].transform.position;
        considerAabb(EEditorSelectionKind::AISpawnPoint, i,
                     Aabb{p - glm::vec3(0.5f), p + glm::vec3(0.5f)});
    }
    for (std::size_t i = 0; i < ctx.level->DirectionalLights().size(); ++i) {
        const glm::vec3& p = ctx.level->DirectionalLights()[i].transform.position;
        considerAabb(EEditorSelectionKind::DirectionalLight, i,
                     Aabb{p - glm::vec3(0.4f), p + glm::vec3(0.4f)});
    }

    if (bestKind != EEditorSelectionKind::None) {
        ctx.Select(bestKind, bestIndex, ImGui::GetIO().KeyCtrl);
    } else if (!ImGui::GetIO().KeyCtrl) {
        ctx.ClearSelection();
    }
}

void ViewportPanel::Draw(EditorContext& ctx) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin("Viewport")) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const ImVec2 size = ImGui::GetContentRegionAvail();
    ctx.viewportWidth = std::max(1, static_cast<int>(size.x));
    ctx.viewportHeight = std::max(1, static_cast<int>(size.y));
    ctx.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    RenderScene(ctx);

    const bool playLocksViewport = ctx.piePlaying && !ctx.pieNewWindow;

    if (target_.Valid()) {
        const ImVec2 imageSize{static_cast<float>(ctx.viewportWidth),
                               static_cast<float>(ctx.viewportHeight)};
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(target_.colorTexture())),
                     imageSize, ImVec2(0, 1), ImVec2(1, 0));

        // Hover the rendered image (not just the dock window chrome) for camera / PIE look.
        const bool imageHovered =
            ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ctx.viewportHovered = imageHovered || (flyActive_ && ImGui::IsMouseDown(ImGuiMouseButton_Right));

        Camera* viewCamForOverlay = (ctx.pieNewWindow && ctx.editorViewCamera != nullptr)
                                        ? ctx.editorViewCamera
                                        : ctx.camera;
        // During Selected Viewport PIE the play camera is ctx.camera.
        if (ctx.piePlaying && !ctx.pieNewWindow) {
            viewCamForOverlay = ctx.camera;
        }
        const ImVec2 imageMin = ImGui::GetItemRectMin();
        if (viewCamForOverlay != nullptr) {
            DrawAxisIndicator(*viewCamForOverlay, imageMin, imageSize);
        }
        DrawViewModeOverlay(ctx, imageMin);
        DrawStatsOverlay(ctx, imageMin);

        Camera* previousCtxCamera = ctx.camera;
        if (ctx.piePlaying && ctx.pieNewWindow && ctx.editorViewCamera != nullptr) {
            ctx.camera = ctx.editorViewCamera;
        }
        if (!playLocksViewport) {
            HandleCameraInput(ctx, ctx.deltaTime);
        }
        ctx.camera = previousCtxCamera;

        if (!playLocksViewport) {
            HandleAssetDrop(ctx);
        }

        const bool rmb = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        if (!playLocksViewport && ctx.viewportFocused && !ImGui::GetIO().WantTextInput && !rmb &&
            !flyActive_) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) {
                ctx.gizmoOp = EGizmoOperation::Translate;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_E)) {
                ctx.gizmoOp = EGizmoOperation::Rotate;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                ctx.gizmoOp = EGizmoOperation::Scale;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                ctx.RequestFocusSelected();
            }
        }

        Transform* transform = SelectedTransform(ctx);
        Camera* viewCam = (ctx.pieNewWindow && ctx.editorViewCamera != nullptr)
                              ? ctx.editorViewCamera
                              : ctx.camera;

        if (!playLocksViewport && transform != nullptr && viewCam != nullptr) {
            if (ctx.history != nullptr && ImGuizmo::IsUsing() && !gizmoWasUsing_) {
                ctx.history->Capture(ctx);
            }
            gizmoWasUsing_ = ImGuizmo::IsUsing();
            const bool changed =
                gizmo_.Manipulate(ctx, viewCam->ViewMatrix(), viewCam->ProjectionMatrix(),
                                  *transform);
            if (changed) {
                if (ctx.snapEnabled) {
                    EditorCommands::SnapTransform(*transform, ctx.gridSize,
                                                  ctx.rotationSnapDegrees, false);
                }
                if (ctx.selection.kind == EEditorSelectionKind::Actor) {
                    if (Actor* actor = ResolveSelectedActor(ctx)) {
                        actor->SetActorLocationAndRotation(actorGizmoScratch_.position,
                                                           actorGizmoScratch_.rotationDegrees.y);
                    }
                }
                ctx.MarkDirty();
            }
        } else {
            gizmoWasUsing_ = false;
        }

        if (!playLocksViewport && !ImGui::GetIO().WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                EditorCommands::DeleteSelection(ctx, ctx.history);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_End) && transform != nullptr) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                transform->position.y = 0.0f;
                if (ctx.snapEnabled) {
                    transform->position =
                        EditorCommands::SnapPosition(transform->position, ctx.gridSize);
                }
                ctx.MarkDirty();
            }
        }

        if (!playLocksViewport && !ImGuizmo::IsUsing()) {
            HandleViewportPicking(ctx);
        }
    } else {
        ImGui::Dummy(size);
        ctx.viewportHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (!playLocksViewport) {
            HandleCameraInput(ctx, ctx.deltaTime);
            HandleAssetDrop(ctx);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

std::optional<std::size_t> ViewportPanel::PickStaticMeshUnderMouse(EditorContext& ctx) const {
    Camera* cam = (ctx.pieNewWindow && ctx.editorViewCamera != nullptr) ? ctx.editorViewCamera
                                                                        : ctx.camera;
    if (ctx.level == nullptr || cam == nullptr) {
        return std::nullopt;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const float imageW = std::max(1.0f, itemMax.x - itemMin.x);
    const float imageH = std::max(1.0f, itemMax.y - itemMin.y);
    const float localX = mouse.x - itemMin.x;
    const float localY = mouse.y - itemMin.y;
    if (localX < 0.0f || localY < 0.0f || localX > imageW || localY > imageH) {
        return std::nullopt;
    }

    const float ndcX = ((localX / imageW) * 2.0f) - 1.0f;
    const float ndcY = 1.0f - ((localY / imageH) * 2.0f);
    const glm::mat4 invVP = glm::inverse(cam->ProjectionMatrix() * cam->ViewMatrix());
    glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farH = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearH.w) < 1.0e-8f || std::abs(farH.w) < 1.0e-8f) {
        return std::nullopt;
    }
    nearH /= nearH.w;
    farH /= farH.w;
    const glm::vec3 rayOrigin = glm::vec3(nearH);
    const glm::vec3 rayDir = glm::normalize(glm::vec3(farH) - rayOrigin);

    float bestT = std::numeric_limits<float>::max();
    std::optional<std::size_t> bestIndex;
    for (std::size_t i = 0; i < ctx.level->StaticMeshes().size(); ++i) {
        const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[i];
        if (mesh.hidden) {
            continue;
        }
        Aabb box;
        if (mesh.mesh != nullptr && mesh.mesh->Valid()) {
            box = Aabb::fromLocalTransformed(mesh.mesh->LocalMin(), mesh.mesh->LocalMax(),
                                             mesh.EffectiveModelMatrix());
        } else {
            const glm::vec3& p = mesh.transform.position;
            box = Aabb{p - glm::vec3(0.35f), p + glm::vec3(0.35f)};
        }
        float t = 0.0f;
        if (box.intersectRay(rayOrigin, rayDir, t) && t >= 0.0f && t < bestT) {
            bestT = t;
            bestIndex = i;
        }
    }
    return bestIndex;
}

void ViewportPanel::ClearMaterialDropPreview(EditorContext& ctx) {
    if (!materialPreviewActive_ || ctx.level == nullptr) {
        materialPreviewActive_ = false;
        materialPreviewPath_.clear();
        return;
    }
    if (materialPreviewMeshIndex_ < ctx.level->StaticMeshes().size()) {
        StaticMeshComponent& mesh = ctx.level->StaticMeshes()[materialPreviewMeshIndex_];
        mesh.material = materialPreviewSaved_;
        mesh.materials = materialPreviewSavedSlots_;
        mesh.materialOverride = materialPreviewSavedOverride_;
        mesh.materialPath = materialPreviewSavedPath_;
    }
    materialPreviewActive_ = false;
    materialPreviewPath_.clear();
    materialPreviewSavedSlots_.clear();
}

void ViewportPanel::ApplyMaterialDropPreview(EditorContext& ctx, std::size_t meshIndex,
                                             const std::string& materialPath,
                                             const Material& material) {
    if (ctx.level == nullptr || meshIndex >= ctx.level->StaticMeshes().size()) {
        return;
    }
    if (materialPreviewActive_ && materialPreviewMeshIndex_ == meshIndex &&
        materialPreviewPath_ == materialPath) {
        return;
    }
    if (materialPreviewActive_) {
        ClearMaterialDropPreview(ctx);
    }

    StaticMeshComponent& mesh = ctx.level->StaticMeshes()[meshIndex];
    materialPreviewSaved_ = mesh.material;
    materialPreviewSavedSlots_ = mesh.materials;
    materialPreviewSavedOverride_ = mesh.materialOverride;
    materialPreviewSavedPath_ = mesh.materialPath;
    materialPreviewMeshIndex_ = meshIndex;
    materialPreviewPath_ = materialPath;
    materialPreviewActive_ = true;

    // Clear per-slot materials so override is visible on every submesh during preview.
    mesh.materials.clear();
    mesh.material = material;
    mesh.materialOverride = true;
    mesh.materialPath = materialPath;
}

bool ViewportPanel::HandleMaterialDrag(EditorContext& ctx, const std::string& path, bool isDelivery) {
    if (ctx.resources == nullptr || ctx.level == nullptr) {
        return false;
    }

    std::string loadPath;
    std::string persistPath = path;
    if (path == "leon:Engine/Materials/M_Default") {
        persistPath = "Materials/M_Default.lmat";
        loadPath = ResolveAssetPath(persistPath);
    } else if (path == "leon:Engine/Materials/M_WorldGrid") {
        persistPath = "Materials/M_WorldGrid.lmat";
        loadPath = ResolveAssetPath(persistPath);
    } else {
        namespace fs = std::filesystem;
        std::string ext = fs::path(path).extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext != ".lmat") {
            return false;
        }
        loadPath = path;
        persistPath = path;
    }

    Material material = ctx.resources->LoadMaterial(loadPath.empty() ? persistPath : loadPath);
    if (path == "leon:Engine/Materials/M_Default" && !material.albedoMap) {
        material = ctx.resources->DefaultMaterial();
    }

    const std::optional<std::size_t> hit = PickStaticMeshUnderMouse(ctx);
    if (!hit.has_value()) {
        ClearMaterialDropPreview(ctx);
        if (isDelivery) {
            // Fall back to current selection (previous behaviour).
            if (ctx.selection.kind == EEditorSelectionKind::StaticMesh &&
                ctx.selection.index < ctx.level->StaticMeshes().size()) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                StaticMeshComponent& mesh = ctx.level->StaticMeshes()[ctx.selection.index];
                mesh.materials.clear();
                mesh.material = material;
                mesh.materialOverride = true;
                mesh.materialPath = persistPath;
                ctx.MarkDirty();
            }
        }
        return true;
    }

    if (isDelivery) {
        // Commit: keep preview material (or apply fresh), capture undo from pre-preview state.
        if (materialPreviewActive_ && materialPreviewMeshIndex_ == *hit) {
            // Restore original into history snapshot, then re-apply commit.
            StaticMeshComponent& mesh = ctx.level->StaticMeshes()[*hit];
            const Material commitMat = mesh.material;
            const std::string commitPath = persistPath;
            mesh.material = materialPreviewSaved_;
            mesh.materials = materialPreviewSavedSlots_;
            mesh.materialOverride = materialPreviewSavedOverride_;
            mesh.materialPath = materialPreviewSavedPath_;
            materialPreviewActive_ = false;
            materialPreviewPath_.clear();
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            mesh.materials.clear();
            mesh.material = commitMat;
            mesh.materialOverride = true;
            mesh.materialPath = commitPath;
            ctx.Select(EEditorSelectionKind::StaticMesh, *hit);
            ctx.MarkDirty();
            return true;
        }
        ClearMaterialDropPreview(ctx);
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        StaticMeshComponent& mesh = ctx.level->StaticMeshes()[*hit];
        mesh.materials.clear();
        mesh.material = material;
        mesh.materialOverride = true;
        mesh.materialPath = persistPath;
        ctx.Select(EEditorSelectionKind::StaticMesh, *hit);
        ctx.MarkDirty();
        return true;
    }

    ApplyMaterialDropPreview(ctx, *hit, persistPath, material);
    return true;
}

bool ViewportPanel::WorldPointUnderMouse(EditorContext& ctx, glm::vec3& outPoint) const {
    Camera* cam = (ctx.pieNewWindow && ctx.editorViewCamera != nullptr) ? ctx.editorViewCamera
                                                                        : ctx.camera;
    if (cam == nullptr) {
        return false;
    }
    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const float imageW = std::max(1.0f, itemMax.x - itemMin.x);
    const float imageH = std::max(1.0f, itemMax.y - itemMin.y);
    const float localX = mouse.x - itemMin.x;
    const float localY = mouse.y - itemMin.y;
    if (localX < 0.0f || localY < 0.0f || localX > imageW || localY > imageH) {
        return false;
    }

    const float ndcX = ((localX / imageW) * 2.0f) - 1.0f;
    const float ndcY = 1.0f - ((localY / imageH) * 2.0f);
    const glm::mat4 invVP = glm::inverse(cam->ProjectionMatrix() * cam->ViewMatrix());
    glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farH = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearH.w) < 1.0e-8f || std::abs(farH.w) < 1.0e-8f) {
        return false;
    }
    nearH /= nearH.w;
    farH /= farH.w;
    const glm::vec3 origin = glm::vec3(nearH);
    const glm::vec3 dir = glm::normalize(glm::vec3(farH) - origin);

    // Intersect ground plane y = 0; fall back to a point ahead of the camera.
    if (std::abs(dir.y) > 1.0e-5f) {
        const float t = -origin.y / dir.y;
        if (t > 0.05f) {
            outPoint = origin + dir * t;
            return true;
        }
    }
    outPoint = origin + dir * 5.0f;
    outPoint.y = std::max(outPoint.y, 0.0f);
    return true;
}

void ViewportPanel::HandleAssetDrop(EditorContext& ctx) {
    if (ctx.level == nullptr || ctx.resources == nullptr ||
        (ctx.piePlaying && !ctx.pieNewWindow)) {
        if (materialPreviewActive_) {
            ClearMaterialDropPreview(ctx);
        }
        return;
    }

    // Drag cancelled / left the viewport — restore any hover material preview.
    if (ImGui::GetDragDropPayload() == nullptr && materialPreviewActive_) {
        ClearMaterialDropPreview(ctx);
    }

    if (!ImGui::BeginDragDropTarget()) {
        if (materialPreviewActive_) {
            ClearMaterialDropPreview(ctx);
        }
        return;
    }

    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
        "LEON_ASSET_PATH",
        ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
    if (payload == nullptr || payload->Data == nullptr) {
        ImGui::EndDragDropTarget();
        if (materialPreviewActive_) {
            ClearMaterialDropPreview(ctx);
        }
        return;
    }
    const std::string path(static_cast<const char*>(payload->Data));
    const bool isDelivery = payload->IsDelivery();
    ImGui::EndDragDropTarget();
    if (path.empty()) {
        return;
    }

    // Materials: live preview on the mesh under the cursor; commit on drop.
    if (HandleMaterialDrag(ctx, path, isDelivery)) {
        return;
    }
    if (!isDelivery) {
        return;
    }

    // Non-material drops only apply on mouse release.
    ClearMaterialDropPreview(ctx);

    glm::vec3 dropPos = ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0, 0};
    (void)WorldPointUnderMouse(ctx, dropPos);

    if (IsEngineContentPath(path)) {
        std::string err;
        if (!ApplyEngineContent(ctx, path, dropPos, err) && !err.empty()) {
            std::cerr << "Viewport drop: " << err << '\n';
        }
        return;
    }

    namespace fs = std::filesystem;
    const fs::path file(path);
    std::string ext = file.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Levels: open on drop via pendingOpenPath → EditorLayout::OpenLevel.
    if (ext == ".llev") {
        if (ctx.engine != nullptr) {
            ctx.pendingOpenPath = path;
        }
        return;
    }

    // Static meshes (source OBJ or cooked .lmesh).
    if (ext == ".obj" || ext == ".lmesh") {
        const std::string meshPath = MakePackRelativeAssetPath(ctx, path);
        auto mesh = ctx.resources->LoadStaticMesh(ResolveAssetPath(meshPath));
        if (mesh == nullptr || !mesh->Valid()) {
            mesh = ctx.resources->LoadStaticMesh(path);
        }
        if (mesh == nullptr || !mesh->Valid()) {
            std::cerr << "Viewport drop: failed to load mesh " << path << '\n';
            return;
        }
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        StaticMeshComponent actor;
        actor.mesh = std::move(mesh);
        actor.transform.position = dropPos;
        actor.editorClass = "StaticMesh";
        actor.meshPath = meshPath;
        actor.material = ctx.resources->DefaultMaterial();
        ctx.level->AddStaticMesh(std::move(actor));
        ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1);
        ctx.MarkDirty();
        return;
    }

    // HDR env maps.
    if (ext == ".hdr") {
        const std::string rel = MakePackRelativeAssetPath(ctx, path);
        auto env = ctx.resources->LoadEnvMap(ResolveAssetPath(rel));
        if (env == nullptr) {
            env = ctx.resources->LoadEnvMap(path);
        }
        if (env != nullptr) {
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            ctx.level->SetEnvironment(env);
            ctx.level->SetEnvironmentPath(rel);
            ctx.MarkDirty();
        }
        return;
    }

    std::cerr << "Viewport drop: unsupported asset " << path << '\n';
}

void ViewportPanel::RenderScene(EditorContext& ctx) {
    if (ctx.renderer == nullptr || ctx.level == nullptr) {
        return;
    }

    Camera* viewCam = ctx.camera;
    if (ctx.piePlaying && ctx.pieNewWindow && ctx.editorViewCamera != nullptr) {
        viewCam = ctx.editorViewCamera;
    }
    if (viewCam == nullptr) {
        return;
    }

    // Temporarily point context camera helpers at the view used for this panel.
    Camera* previousCtxCamera = ctx.camera;
    ctx.camera = viewCam;

    const ImGuiIO& io = ImGui::GetIO();
    const float scaleX = std::max(io.DisplayFramebufferScale.x, 1.0f);
    const float scaleY = std::max(io.DisplayFramebufferScale.y, 1.0f);
    const int fbW = std::max(1, static_cast<int>(std::lround(ctx.viewportWidth * scaleX)));
    const int fbH = std::max(1, static_cast<int>(std::lround(ctx.viewportHeight * scaleY)));

    if (!target_.EnsureSize(fbW, fbH)) {
        ctx.camera = previousCtxCamera;
        return;
    }

    ApplyViewMode(ctx, *viewCam);
    const float aspect = static_cast<float>(ctx.viewportWidth) /
                         static_cast<float>(std::max(ctx.viewportHeight, 1));
    if (ctx.viewMode != EEditorViewMode::Perspective) {
        viewCam->SetOrthographic(viewCam->OrthoHeight(), aspect, 0.1f, 500.0f);
    } else {
        viewCam->SetPerspective(viewCam->FieldOfView(), aspect, 0.1f, 100.0f);
    }

    if (ctx.requestFocusSelected) {
        FocusSelection(ctx);
        ctx.requestFocusSelected = false;
    }

    // During Selected Viewport PIE, force Lit (match Shipping).
    const bool playLocksViewport = ctx.piePlaying && !ctx.pieNewWindow;
    const bool playerCollision =
        !playLocksViewport && ctx.viewportViewMode == EEditorViewportViewMode::PlayerCollision;

    // Player Collision = collision wireframes only (Unreal VMI_CollisionPawn-like).
    if (!playerCollision) {
        DrawEditorHelpers(ctx);
        DrawSelectionOverlay(ctx);
        DrawGrid(ctx);
    } else {
        AppendPlayerCollisionOverlay(ctx);
    }

    // Flow: View Mode
    // 1. Lit → full DrawScene
    // 2. Player Collision → disable scene geometry, flush collision wireframes only
    ctx.renderer->SetSceneGeometryEnabled(!playerCollision);
    ctx.renderer->SetDrawFramebuffer(target_.fbo());
    ctx.renderer->BeginFrame(fbW, fbH);
    ctx.renderer->DrawScene(*ctx.level, *viewCam);
    if (playLocksViewport && ctx.piePaintPlayOverlay) {
        ctx.piePaintPlayOverlay(fbW, fbH);
    }
    ctx.renderer->SetDrawFramebuffer(0);
    ctx.renderer->SetSceneGeometryEnabled(true);
    target_.End();

    ctx.camera = previousCtxCamera;
}

void ViewportPanel::AppendPlayerCollisionOverlay(EditorContext& ctx) {
    if (ctx.renderer == nullptr || ctx.level == nullptr) {
        return;
    }

    // Always rebuild from the level. Edit-time `ctx.world` is usually empty (bodies are
    // registered only by GameMode / RegisterBodiesFromLevel), and SyncFromLevel alone does
    // not create missing BodyInstances — so Player Collision showed nothing for primitives.
    collisionPreviewScene_.Clear();
    const auto& meshes = ctx.level->StaticMeshes();
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        const StaticMeshComponent& component = meshes[i];
        if (!component.HasPhysicsBody()) {
            continue;
        }
        BodyInstanceDesc desc{};
        desc.levelMeshIndex = i;
        desc.type = component.simulatePhysics ? EBodyType::Dynamic : EBodyType::Static;
        desc.enableGravity = component.enableGravity;
        collisionPreviewScene_.AddBody(desc);
    }
    collisionPreviewScene_.SyncFromLevel(*ctx.level);
    collisionPreviewScene_.AppendBodiesCollisionDebug(ctx.renderer->GetDebugOverlay());
}

void ViewportPanel::DrawViewModeOverlay(EditorContext& ctx, const ImVec2& imageMin) {
    if (ctx.viewportViewMode != EEditorViewportViewMode::PlayerCollision) {
        return;
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (draw == nullptr) {
        return;
    }
    const char* label = "Player Collision";
    const ImVec2 pos{imageMin.x + 10.0f, imageMin.y + 10.0f};
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 180), label);
    draw->AddText(pos, IM_COL32(120, 200, 255, 255), label);
}

void ViewportPanel::DrawAxisIndicator(const Camera& camera, const ImVec2& imageMin,
                                      const ImVec2& imageSize) const {
    if (imageSize.x < 80.0f || imageSize.y < 80.0f) {
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (draw == nullptr) {
        return;
    }

    constexpr float kLen = 40.0f;
    constexpr float kMargin = 16.0f;
    constexpr float kTipRadius = 3.0f;
    const ImVec2 origin{imageMin.x + kMargin + kLen, imageMin.y + imageSize.y - kMargin - kLen};

    // View rotation only: world axis → view space (OpenGL: +X right, +Y up, −Z forward).
    const glm::mat3 viewRot{camera.ViewMatrix()};

    struct AxisSeg {
        ImVec2 tip{};
        float depth = 0.0f;
        ImU32 color = 0;
        const char* label = "";
    };

    std::array<AxisSeg, 3> axes{{
        {{}, 0.0f, IM_COL32(232, 72, 72, 255), "X"},
        {{}, 0.0f, IM_COL32(96, 200, 96, 255), "Y"},
        {{}, 0.0f, IM_COL32(80, 140, 245, 255), "Z"},
    }};
    const glm::vec3 world[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

    for (int i = 0; i < 3; ++i) {
        const glm::vec3 v = viewRot * world[i];
        axes[i].tip = ImVec2(origin.x + v.x * kLen, origin.y - v.y * kLen);
        axes[i].depth = v.z; // larger z = farther (behind); draw those first
    }

    std::array<int, 3> order{0, 1, 2};
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return axes[a].depth > axes[b].depth; });

    // Soft disc behind the triad for contrast.
    draw->AddCircleFilled(origin, kLen * 0.55f, IM_COL32(12, 12, 14, 140), 32);

    for (int idx : order) {
        const AxisSeg& axis = axes[idx];
        draw->AddLine(origin, axis.tip, axis.color, 2.25f);
        draw->AddCircleFilled(axis.tip, kTipRadius, axis.color, 12);
        const ImVec2 labelPos{axis.tip.x + 4.0f, axis.tip.y - 10.0f};
        draw->AddText(labelPos, axis.color, axis.label);
    }
}

void ViewportPanel::DrawGrid(EditorContext& ctx) {
    if (!ctx.showGrid || ctx.renderer == nullptr || ctx.camera == nullptr ||
        (ctx.piePlaying && !ctx.pieNewWindow)) {
        return;
    }

    // Infinite-style editor grid: world-aligned steps, window follows the camera.
    const float step = std::max(ctx.gridSize, 0.05f);
    const auto snapDown = [step](float v) { return std::floor(v / step) * step; };
    const auto lineColor = [](int worldIndex, const glm::vec3& axisColor) {
        constexpr glm::vec3 kMajor{0.35f, 0.35f, 0.38f};
        constexpr glm::vec3 kMinor{0.22f, 0.22f, 0.24f};
        if (worldIndex == 0) {
            return axisColor;
        }
        if (worldIndex % 5 == 0) {
            return kMajor;
        }
        return kMinor;
    };

    const glm::vec3 eye = ctx.camera->GetCameraLocation();
    constexpr glm::vec3 kAxisX{0.55f, 0.2f, 0.2f};
    constexpr glm::vec3 kAxisY{0.2f, 0.55f, 0.25f};
    constexpr glm::vec3 kAxisZ{0.2f, 0.35f, 0.55f};

    int half = 40;
    if (ctx.viewMode != EEditorViewMode::Perspective) {
        half = std::clamp(static_cast<int>(std::ceil(ctx.camera->OrthoHeight() / step)) + 6, 12,
                          80);
    } else {
        const float reach = std::max({std::abs(eye.y) * 2.5f, 20.0f * step, 16.0f});
        half = std::clamp(static_cast<int>(std::ceil(reach / step)) + 4, 16, 80);
    }
    const float extent = static_cast<float>(half) * step;

    Renderer& r = *ctx.renderer;
    switch (ctx.viewMode) {
    case EEditorViewMode::OrthoFront: {
        // XY plane at camera Z.
        const float z = snapDown(eye.z);
        const float ox = snapDown(eye.x);
        const float oy = snapDown(eye.y);
        for (int i = -half; i <= half; ++i) {
            const float y = oy + static_cast<float>(i) * step;
            const int yi = static_cast<int>(std::lround(y / step));
            r.AddDebugLine({ox - extent, y, z}, {ox + extent, y, z}, lineColor(yi, kAxisX));
            const float x = ox + static_cast<float>(i) * step;
            const int xi = static_cast<int>(std::lround(x / step));
            r.AddDebugLine({x, oy - extent, z}, {x, oy + extent, z}, lineColor(xi, kAxisY));
        }
        break;
    }
    case EEditorViewMode::OrthoSide: {
        // YZ plane at camera X.
        const float x = snapDown(eye.x);
        const float oy = snapDown(eye.y);
        const float oz = snapDown(eye.z);
        for (int i = -half; i <= half; ++i) {
            const float y = oy + static_cast<float>(i) * step;
            const int yi = static_cast<int>(std::lround(y / step));
            r.AddDebugLine({x, y, oz - extent}, {x, y, oz + extent}, lineColor(yi, kAxisZ));
            const float z = oz + static_cast<float>(i) * step;
            const int zi = static_cast<int>(std::lround(z / step));
            r.AddDebugLine({x, oy - extent, z}, {x, oy + extent, z}, lineColor(zi, kAxisY));
        }
        break;
    }
    case EEditorViewMode::OrthoTop:
    case EEditorViewMode::Perspective:
    default: {
        // XZ floor plane (Y = 0), centered under the camera.
        const float ox = snapDown(eye.x);
        const float oz = snapDown(eye.z);
        constexpr float kY = 0.0f;
        for (int i = -half; i <= half; ++i) {
            const float z = oz + static_cast<float>(i) * step;
            const int zi = static_cast<int>(std::lround(z / step));
            r.AddDebugLine({ox - extent, kY, z}, {ox + extent, kY, z}, lineColor(zi, kAxisZ));
            const float x = ox + static_cast<float>(i) * step;
            const int xi = static_cast<int>(std::lround(x / step));
            r.AddDebugLine({x, kY, oz - extent}, {x, kY, oz + extent}, lineColor(xi, kAxisX));
        }
        break;
    }
    }
}

void ViewportPanel::DrawStatsOverlay(EditorContext& ctx, const ImVec2& imageMin) {
    if (!ctx.showStats) {
        statsAccumTime_ = 0.0f;
        statsAccumFrames_ = 0;
        return;
    }

    statsAccumTime_ += ctx.deltaTime;
    ++statsAccumFrames_;
    if (statsAccumTime_ >= 0.25f && statsAccumFrames_ > 0) {
        displayMs_ = (statsAccumTime_ / static_cast<float>(statsAccumFrames_)) * 1000.0f;
        displayFps_ = static_cast<float>(statsAccumFrames_) / statsAccumTime_;
        statsAccumTime_ = 0.0f;
        statsAccumFrames_ = 0;
    }

    char line[160];
    (void)std::snprintf(line, sizeof(line),
                        "FPS %5.0f   MS %5.2f\n"
                        "GPU Sh %.2f Pl %.2f Col %.2f\n"
                        "    AO %.2f Pst %.2f",
                        displayFps_, displayMs_,
                        ctx.renderer != nullptr ? ctx.renderer->GetFrameStats().shadowMs : 0.0f,
                        ctx.renderer != nullptr ? ctx.renderer->GetFrameStats().planarMs : 0.0f,
                        ctx.renderer != nullptr ? ctx.renderer->GetFrameStats().colorMs : 0.0f,
                        ctx.renderer != nullptr ? ctx.renderer->GetFrameStats().ssaoMs : 0.0f,
                        ctx.renderer != nullptr ? ctx.renderer->GetFrameStats().postMs : 0.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    // Offset below View Mode label when Player Collision is active.
    const float yOff =
        ctx.viewportViewMode == EEditorViewportViewMode::PlayerCollision ? 28.0f : 0.0f;
    const ImVec2 pos{imageMin.x + 10.0f, imageMin.y + 10.0f + yOff};
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 180), line);
    draw->AddText(pos, IM_COL32(240, 240, 240, 255), line);
}

} // namespace leon::editor
