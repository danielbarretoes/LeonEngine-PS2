#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <leon/core/Paths.h>
#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/panels/DetailsPanel.h>
#include <leon/gameplay/Actor.h>
#include <leon/gameplay/World.h>
#include <leon/level/Light.h>
#include <leon/render/Renderer.h>
#include <leon/render/ResourceCache.h>
#include <typeinfo>

namespace leon::editor {
namespace {

bool EditTransformFields(Transform& t, EditorContext& ctx) {
    bool changed = false;
    auto drag = [&](const char* label, float* v, float speed, float vMin = 0.0f,
                    float vMax = 0.0f) {
        const bool edited =
            (vMin == 0.0f && vMax == 0.0f) ? ImGui::DragFloat3(label, v, speed)
                                          : ImGui::DragFloat3(label, v, speed, vMin, vMax);
        if (edited) {
            if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            return true;
        }
        return false;
    };
    changed |= drag("Location", &t.position.x, 0.05f);
    changed |= drag("Rotation", &t.rotationDegrees.x, 0.5f);
    changed |= drag("Scale", &t.scale.x, 0.05f, 0.01f, 100.0f);
    return changed;
}

[[nodiscard]] const char* DemangleType(const char* name) {
    if (name == nullptr) {
        return "Unknown";
    }
    if (std::strncmp(name, "class ", 6) == 0) {
        return name + 6;
    }
    if (std::strncmp(name, "struct ", 7) == 0) {
        return name + 7;
    }
    return name;
}

[[nodiscard]] std::string MaterialDisplayLabel(const std::string& path) {
    if (path.empty()) {
        return "(none)";
    }
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

} // namespace

void DetailsPanel::RefreshMaterialList(EditorContext& ctx) {
    if (ctx.requestContentRefresh) {
        materialsProjectKey_.clear();
    }
    if (!materialsProjectKey_.empty() && materialsProjectKey_ == ctx.projectPath &&
        !materials_.empty()) {
        return;
    }
    materialsProjectKey_ = ctx.projectPath;
    materials_ = CollectEditorMaterials(ctx.projectPath);
}

void DetailsPanel::DrawMaterialPicker(EditorContext& ctx, StaticMeshComponent& mesh) {
    RefreshMaterialList(ctx);

    if (ctx.resources == nullptr) {
        ImGui::TextDisabled("Materials unavailable (no ResourceCache)");
        return;
    }

    const std::string previewLabel = MaterialDisplayLabel(mesh.materialPath);
    if (ImGui::BeginCombo("Material", previewLabel.c_str())) {
        for (const EditorMaterialEntry& entry : materials_) {
            const bool selected = entry.authoringPath == mesh.materialPath ||
                                  entry.absolutePath == mesh.materialPath;
            if (ImGui::Selectable(entry.displayName.c_str(), selected)) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                const std::string loadPath =
                    entry.absolutePath.empty() ? ResolveAssetPath(entry.authoringPath)
                                               : entry.absolutePath;
                mesh.material = ctx.resources->LoadMaterial(loadPath);
                mesh.materialOverride = true;
                mesh.materialPath = entry.authoringPath;
                ctx.MarkDirty();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", entry.authoringPath.c_str());
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Pick Material…")) {
        ctx.requestPickMaterial = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Edit Material…") && !mesh.materialPath.empty()) {
        ctx.requestOpenMaterialPath =
            !ctx.projectPath.empty()
                ? ResolveContentAssetPath(ctx.projectPath, mesh.materialPath)
                : ResolveAssetPath(mesh.materialPath);
        ctx.showMaterialEditor = true;
    }
    if (!ctx.pendingMaterialPickPath.empty()) {
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        mesh.material = ctx.resources->LoadMaterial(ctx.pendingMaterialPickPath);
        mesh.materialOverride = true;
        mesh.materialPath = ctx.pendingMaterialPickPath;
        ctx.pendingMaterialPickPath.clear();
        ctx.requestPickMaterial = false;
        ctx.MarkDirty();
        materialsProjectKey_.clear();
    }

    if (ctx.renderer != nullptr && ctx.resources != nullptr) {
        ImGui::Spacing();
        ImGui::TextDisabled("Preview");
        const std::string previewKey =
            mesh.materialPath.empty() ? "Materials/M_Default.lmat" : mesh.materialPath;
        const std::string previewPath =
            !ctx.projectPath.empty() ? ResolveContentAssetPath(ctx.projectPath, previewKey)
                                     : ResolveAssetPath(previewKey);
        materialPreview_.SetMaterialPath(*ctx.resources, previewPath);
        materialPreview_.Draw(*ctx.renderer, 160.0f, 160.0f);
    }
}

void DetailsPanel::Draw(EditorContext& ctx) {
    if (!ctx.showDetails) {
        return;
    }
    if (!ImGui::Begin("Details", &ctx.showDetails)) {
        ImGui::End();
        return;
    }

    if (ctx.level == nullptr || !ctx.selection.IsValid()) {
        ImGui::TextDisabled("Nothing selected");
        ImGui::End();
        return;
    }

    switch (ctx.selection.kind) {
    case EEditorSelectionKind::StaticMesh: {
        if (ctx.selection.index >= ctx.level->StaticMeshes().size()) {
            break;
        }
        StaticMeshComponent& mesh = ctx.level->StaticMeshes()[ctx.selection.index];
        ImGui::Text("Static Mesh — %s",
                    mesh.editorClass.empty() ? "Mesh" : mesh.editorClass.c_str());

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (EditTransformFields(mesh.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
            char tagBuf[128];
            (void)std::snprintf(tagBuf, sizeof(tagBuf), "%s", mesh.tag.c_str());
            if (ImGui::InputText("Tag", tagBuf, sizeof(tagBuf))) {
                mesh.tag = tagBuf;
                ctx.MarkDirty();
            }
            if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            if (!mesh.meshPath.empty()) {
                ImGui::TextDisabled("Mesh: %s", mesh.meshPath.c_str());
            } else {
                ImGui::TextDisabled("Mesh: (procedural / unbound)");
            }
            DrawMaterialPicker(ctx, mesh);
        }
        if (ImGui::CollapsingHeader("Mobility", ImGuiTreeNodeFlags_DefaultOpen)) {
            int mobility = static_cast<int>(mesh.mobility);
            const char* mobilityItems[] = {"Static", "Movable"};
            // "##mobility": CollapsingHeader("Mobility") already owns that ID.
            if (ImGui::Combo("Type##mobility", &mobility, mobilityItems, 2)) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                mesh.mobility = static_cast<EComponentMobility>(mobility);
                if (mesh.mobility == EComponentMobility::Movable) {
                    mesh.lightmap.reset();
                }
                ctx.MarkDirty();
            }
            ImGui::TextDisabled(mesh.mobility == EComponentMobility::Static
                                    ? "Receives Build Lights lightmaps"
                                    : "Realtime lighting only");
        }
        if (ImGui::CollapsingHeader("Lightmass", ImGuiTreeNodeFlags_DefaultOpen)) {
            const bool isStatic = mesh.mobility == EComponentMobility::Static;
            if (!isStatic) {
                ImGui::BeginDisabled();
            }
            int resIndex = 2; // 128
            static constexpr int kRes[] = {32, 64, 128, 256, 512};
            for (int i = 0; i < 5; ++i) {
                if (kRes[i] == mesh.lightmapResolution) {
                    resIndex = i;
                    break;
                }
            }
            const char* resLabels[] = {"32", "64", "128", "256", "512"};
            if (ImGui::Combo("Lightmap Resolution", &resIndex, resLabels, 5)) {
                mesh.lightmapResolution = kRes[resIndex];
                ctx.MarkDirty();
            }
            if (mesh.UsesLightmap()) {
                ImGui::TextDisabled("Lightmap baked (%dx%d)", mesh.lightmapResolution,
                                    mesh.lightmapResolution);
            } else {
                ImGui::TextDisabled("No lightmap — use Toolbar Build Lights");
            }
            if (!isStatic) {
                ImGui::EndDisabled();
            }
        }
        if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Collision Enabled", &mesh.collisionEnabled)) {
                ctx.MarkDirty();
            }
            if (ImGui::Checkbox("Simulate Physics", &mesh.simulatePhysics)) {
                if (mesh.simulatePhysics) {
                    mesh.collisionEnabled = true;
                }
                ctx.MarkDirty();
            }
            if (ImGui::Checkbox("Enable Gravity", &mesh.enableGravity)) {
                ctx.MarkDirty();
            }
        }
        if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Hidden", &mesh.hidden)) {
                ctx.MarkDirty();
            }
            if (ImGui::DragFloat("Spin Yaw", &mesh.spinYaw, 0.5f)) {
                ctx.MarkDirty();
            }
        }
        break;
    }
    case EEditorSelectionKind::DirectionalLight: {
        if (ctx.selection.index >= ctx.level->DirectionalLights().size()) {
            break;
        }
        DirectionalLight& light = ctx.level->DirectionalLights()[ctx.selection.index];
        ImGui::TextUnformatted("Directional Light");
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (EditTransformFields(light.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::ColorEdit3("Light Color", &light.lightColor.x)) {
                ctx.MarkDirty();
            }
            if (ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 20.0f)) {
                ctx.MarkDirty();
            }
            if (ImGui::Checkbox("Cast Shadows", &light.castShadows)) {
                ctx.MarkDirty();
            }
            if (ImGui::DragFloat("Source Angle", &light.sourceAngle, 0.01f, 0.0f, 10.0f)) {
                ctx.MarkDirty();
            }
        }
        break;
    }
    case EEditorSelectionKind::PointLight: {
        if (ctx.selection.index >= ctx.level->PointLights().size()) {
            break;
        }
        PointLight& light = ctx.level->PointLights()[ctx.selection.index];
        ImGui::TextUnformatted("Point Light");
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (EditTransformFields(light.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::ColorEdit3("Light Color", &light.lightColor.x)) {
                ctx.MarkDirty();
            }
            if (ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 20.0f)) {
                ctx.MarkDirty();
            }
            if (ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f, 100.0f)) {
                ctx.MarkDirty();
            }
            if (ImGui::Checkbox("Cast Shadows", &light.castShadows)) {
                ctx.MarkDirty();
            }
        }
        break;
    }
    case EEditorSelectionKind::PlayerStart: {
        if (ctx.selection.index >= ctx.level->PlayerStarts().size()) {
            break;
        }
        PlayerStart& start = ctx.level->PlayerStarts()[ctx.selection.index];
        ImGui::TextUnformatted("Player Start");
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (EditTransformFields(start.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        break;
    }
    case EEditorSelectionKind::TriggerVolume: {
        if (ctx.selection.index >= ctx.level->TriggerVolumes().size()) {
            break;
        }
        TriggerVolume& volume = ctx.level->TriggerVolumes()[ctx.selection.index];
        ImGui::TextUnformatted("Trigger Volume");
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (EditTransformFields(volume.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        if (ImGui::CollapsingHeader("Trigger", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::DragInt("Interact Cost", &volume.interactCost, 1.0f, 0, 100000)) {
                if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                ctx.MarkDirty();
            }
            if (ImGui::DragFloat("Radius", &volume.interactRadius, 0.05f, 0.1f, 50.0f)) {
                if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                ctx.MarkDirty();
            }
            char payloadBuf[128];
            (void)std::snprintf(payloadBuf, sizeof(payloadBuf), "%s", volume.payload.c_str());
            if (ImGui::InputText("Payload", payloadBuf, sizeof(payloadBuf))) {
                volume.payload = payloadBuf;
                ctx.MarkDirty();
            }
            if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            if (ImGui::Checkbox("Consume On Use", &volume.bConsumeOnUse)) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                ctx.MarkDirty();
            }
            char tagBuf[128];
            (void)std::snprintf(tagBuf, sizeof(tagBuf), "%s", volume.tag.c_str());
            if (ImGui::InputText("Tag", tagBuf, sizeof(tagBuf))) {
                volume.tag = tagBuf;
                ctx.MarkDirty();
            }
            if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
        }
        break;
    }
    case EEditorSelectionKind::PainCausingVolume: {
        if (ctx.selection.index >= ctx.level->PainCausingVolumes().size()) {
            break;
        }
        PainCausingVolume& volume = ctx.level->PainCausingVolumes()[ctx.selection.index];
        ImGui::TextUnformatted("Pain Causing Volume");
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (EditTransformFields(volume.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        if (ImGui::CollapsingHeader("Damage", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::DragFloat("Damage Per Second", &volume.damagePerSecond, 0.25f, 0.0f,
                                 500.0f)) {
                if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                ctx.MarkDirty();
            }
            if (ImGui::DragFloat("Damage Interval", &volume.damageInterval, 0.01f, 0.05f, 5.0f)) {
                if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                ctx.MarkDirty();
            }
            char tagBuf[128];
            (void)std::snprintf(tagBuf, sizeof(tagBuf), "%s", volume.tag.c_str());
            if (ImGui::InputText("Tag", tagBuf, sizeof(tagBuf))) {
                volume.tag = tagBuf;
                ctx.MarkDirty();
            }
            if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
        }
        break;
    }
    case EEditorSelectionKind::AISpawnPoint: {
        if (ctx.selection.index >= ctx.level->AISpawnPoints().size()) {
            break;
        }
        AISpawnPoint& point = ctx.level->AISpawnPoints()[ctx.selection.index];
        ImGui::TextUnformatted("AI Spawn Point");
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (EditTransformFields(point.transform, ctx)) {
                ctx.MarkDirty();
            }
        }
        if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen)) {
            char tagBuf[128];
            (void)std::snprintf(tagBuf, sizeof(tagBuf), "%s", point.tag.c_str());
            if (ImGui::InputText("Tag", tagBuf, sizeof(tagBuf))) {
                point.tag = tagBuf;
                ctx.MarkDirty();
            }
            if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
        }
        break;
    }
    case EEditorSelectionKind::Actor: {
        if (ctx.world == nullptr) {
            ImGui::TextDisabled("No World");
            break;
        }
        Actor* target = (ctx.selection.id != 0) ? ctx.world->FindActorByEditorId(ctx.selection.id)
                                                : ctx.FindActorByIndex(ctx.selection.index);
        if (target == nullptr) {
            ImGui::TextDisabled("Actor gone");
            break;
        }
        ImGui::Text("Actor — %s", DemangleType(typeid(*target).name()));
        ImGui::TextDisabled("EditorId: %llu", static_cast<unsigned long long>(target->GetEditorId()));

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            glm::vec3 loc = target->GetActorLocation();
            float yaw = target->GetActorYaw();
            bool changed = false;
            changed |= ImGui::DragFloat3("Location", &loc.x, 0.05f);
            changed |= ImGui::DragFloat("Rotation (Yaw)", &yaw, 0.5f);
            if (changed) {
                target->SetActorLocationAndRotation(loc, yaw);
            }
        }
        if (ImGui::CollapsingHeader("Actor", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("Root SceneComponent children: %zu",
                                target->GetRootComponent().GetAttachChildren().size());
            ImGui::TextDisabled("Level mesh index: %zu", target->LevelMeshIndex());
        }
        break;
    }
    default:
        break;
    }

    ImGui::End();
}

} // namespace leon::editor
