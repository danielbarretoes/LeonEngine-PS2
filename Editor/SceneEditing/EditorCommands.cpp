#include <leon/editor/EditorCommands.h>

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <leon/core/Paths.h>
#include <leon/editor/LevelSaver.h>
#include <leon/level/BasicShape.h>
#include <leon/level/Light.h>
#include <nlohmann/json.hpp>

namespace leon::editor {
namespace {

nlohmann::json Vec3(const glm::vec3& v) {
    return nlohmann::json::array({v.x, v.y, v.z});
}

void WriteTransform(nlohmann::json& j, const Transform& t) {
    j["position"] = Vec3(t.position);
    j["rotation"] = Vec3(t.rotationDegrees);
    j["scale"] = Vec3(t.scale);
}

[[nodiscard]] nlohmann::json SerializeSelection(const EditorContext& ctx,
                                                const EditorSelection& sel) {
    nlohmann::json j;
    if (ctx.level == nullptr || !sel.IsValid()) {
        return j;
    }
    switch (sel.kind) {
    case EEditorSelectionKind::StaticMesh:
        if (sel.index < ctx.level->StaticMeshes().size()) {
            const StaticMeshComponent& mesh = ctx.level->StaticMeshes()[sel.index];
            j["kind"] = "StaticMesh";
            j["class"] = mesh.editorClass.empty() ? "StaticMesh" : mesh.editorClass;
            WriteTransform(j, mesh.transform);
            j["tag"] = mesh.tag;
            j["material"] = mesh.materialPath;
            j["mesh"] = mesh.meshPath;
            j["collisionEnabled"] = mesh.collisionEnabled;
            j["simulatePhysics"] = mesh.simulatePhysics;
            j["enableGravity"] = mesh.enableGravity;
            j["hidden"] = mesh.hidden;
            j["mobility"] = mesh.mobility == EComponentMobility::Movable ? "Movable" : "Static";
            j["lightmapResolution"] = mesh.lightmapResolution;
        }
        break;
    case EEditorSelectionKind::PointLight:
        if (sel.index < ctx.level->PointLights().size()) {
            const PointLight& light = ctx.level->PointLights()[sel.index];
            j["kind"] = "PointLight";
            j["position"] = Vec3(light.transform.position);
            j["lightColor"] = Vec3(light.lightColor);
            j["intensity"] = light.intensity;
            j["range"] = light.range;
            j["castShadows"] = light.castShadows;
        }
        break;
    case EEditorSelectionKind::PlayerStart:
        if (sel.index < ctx.level->PlayerStarts().size()) {
            j["kind"] = "PlayerStart";
            WriteTransform(j, ctx.level->PlayerStarts()[sel.index].transform);
        }
        break;
    case EEditorSelectionKind::TriggerVolume:
        if (sel.index < ctx.level->TriggerVolumes().size()) {
            const TriggerVolume& volume = ctx.level->TriggerVolumes()[sel.index];
            j["kind"] = "TriggerVolume";
            WriteTransform(j, volume.transform);
            j["interactRadius"] = volume.interactRadius;
            j["interactCost"] = volume.interactCost;
            j["payload"] = volume.payload;
            j["tag"] = volume.tag;
            j["bConsumeOnUse"] = volume.bConsumeOnUse;
        }
        break;
    case EEditorSelectionKind::PainCausingVolume:
        if (sel.index < ctx.level->PainCausingVolumes().size()) {
            const PainCausingVolume& volume = ctx.level->PainCausingVolumes()[sel.index];
            j["kind"] = "PainCausingVolume";
            WriteTransform(j, volume.transform);
            j["damagePerSecond"] = volume.damagePerSecond;
            j["damageInterval"] = volume.damageInterval;
            j["tag"] = volume.tag;
        }
        break;
    case EEditorSelectionKind::AISpawnPoint:
        if (sel.index < ctx.level->AISpawnPoints().size()) {
            const AISpawnPoint& point = ctx.level->AISpawnPoints()[sel.index];
            j["kind"] = "AISpawnPoint";
            WriteTransform(j, point.transform);
            j["tag"] = point.tag;
        }
        break;
    case EEditorSelectionKind::DirectionalLight:
        if (sel.index < ctx.level->DirectionalLights().size()) {
            const DirectionalLight& light = ctx.level->DirectionalLights()[sel.index];
            j["kind"] = "DirectionalLight";
            j["rotation"] = Vec3(light.transform.rotationDegrees);
            j["lightColor"] = Vec3(light.lightColor);
            j["intensity"] = light.intensity;
            j["castShadows"] = light.castShadows;
        }
        break;
    default:
        break;
    }
    return j;
}

void OffsetTransform(Transform& t, const glm::vec3& delta) {
    t.position += delta;
}

} // namespace

std::vector<EditorClipboardItem>& EditorCommands::Clipboard() {
    static std::vector<EditorClipboardItem> clip;
    return clip;
}

glm::vec3 EditorCommands::SnapPosition(const glm::vec3& p, float gridSize) {
    if (gridSize <= 1.0e-5f) {
        return p;
    }
    auto snap1 = [gridSize](float v) {
        return std::round(v / gridSize) * gridSize;
    };
    return {snap1(p.x), snap1(p.y), snap1(p.z)};
}

float EditorCommands::SnapAngle(float degrees, float stepDegrees) {
    if (stepDegrees <= 1.0e-5f) {
        return degrees;
    }
    return std::round(degrees / stepDegrees) * stepDegrees;
}

void EditorCommands::SnapTransform(Transform& t, float gridSize, float angleStep, bool snapScale) {
    t.position = SnapPosition(t.position, gridSize);
    t.rotationDegrees.x = SnapAngle(t.rotationDegrees.x, angleStep);
    t.rotationDegrees.y = SnapAngle(t.rotationDegrees.y, angleStep);
    t.rotationDegrees.z = SnapAngle(t.rotationDegrees.z, angleStep);
    if (snapScale) {
        t.scale = SnapPosition(t.scale, gridSize * 0.25f);
        t.scale = glm::max(t.scale, glm::vec3(0.01f));
    }
}

void EditorCommands::DeleteSelection(EditorContext& ctx, EditorHistory* history) {
    if (ctx.level == nullptr || ctx.selected.empty()) {
        return;
    }
    if (history != nullptr) {
        history->Capture(ctx);
    }

    // Delete highest indices first per kind to keep indices stable.
    auto selected = ctx.selected;
    std::sort(selected.begin(), selected.end(),
              [](const EditorSelection& a, const EditorSelection& b) {
                  if (a.kind != b.kind) {
                      return static_cast<int>(a.kind) < static_cast<int>(b.kind);
                  }
                  return a.index > b.index;
              });

    for (const EditorSelection& sel : selected) {
        switch (sel.kind) {
        case EEditorSelectionKind::StaticMesh:
            if (sel.index < ctx.level->StaticMeshes().size()) {
                ctx.level->StaticMeshes().erase(ctx.level->StaticMeshes().begin() +
                                                static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::DirectionalLight:
            if (sel.index < ctx.level->DirectionalLights().size() &&
                ctx.level->DirectionalLights().size() > 1) {
                ctx.level->DirectionalLights().erase(ctx.level->DirectionalLights().begin() +
                                                     static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::PointLight:
            if (sel.index < ctx.level->PointLights().size()) {
                ctx.level->PointLights().erase(ctx.level->PointLights().begin() +
                                               static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::PlayerStart:
            if (sel.index < ctx.level->PlayerStarts().size()) {
                ctx.level->PlayerStarts().erase(ctx.level->PlayerStarts().begin() +
                                                static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::TriggerVolume:
            if (sel.index < ctx.level->TriggerVolumes().size()) {
                ctx.level->TriggerVolumes().erase(ctx.level->TriggerVolumes().begin() +
                                                  static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::PainCausingVolume:
            if (sel.index < ctx.level->PainCausingVolumes().size()) {
                ctx.level->PainCausingVolumes().erase(
                    ctx.level->PainCausingVolumes().begin() +
                    static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        case EEditorSelectionKind::AISpawnPoint:
            if (sel.index < ctx.level->AISpawnPoints().size()) {
                ctx.level->AISpawnPoints().erase(ctx.level->AISpawnPoints().begin() +
                                                 static_cast<std::ptrdiff_t>(sel.index));
            }
            break;
        default:
            break;
        }
    }
    ctx.ClearSelection();
    ctx.MarkDirty();
}

void EditorCommands::DuplicateSelection(EditorContext& ctx, EditorHistory* history) {
    CopySelection(ctx);
    PasteClipboard(ctx, history);
}

void EditorCommands::CopySelection(EditorContext& ctx) {
    Clipboard().clear();
    if (ctx.level == nullptr) {
        return;
    }
    const auto& list = ctx.selected.empty()
                           ? std::vector<EditorSelection>{ctx.selection}
                           : ctx.selected;
    for (const EditorSelection& sel : list) {
        nlohmann::json j = SerializeSelection(ctx, sel);
        if (j.is_null() || j.empty() || !j.contains("kind")) {
            continue;
        }
        EditorClipboardItem item;
        item.kind = sel.kind;
        item.json = j.dump();
        Clipboard().push_back(std::move(item));
    }
}

void EditorCommands::PasteClipboard(EditorContext& ctx, EditorHistory* history) {
    if (ctx.level == nullptr || Clipboard().empty()) {
        return;
    }
    if (history != nullptr) {
        history->Capture(ctx);
    }
    ctx.ClearSelection();
    constexpr glm::vec3 kOffset{1.0f, 0.0f, 1.0f};

    for (const EditorClipboardItem& item : Clipboard()) {
        nlohmann::json j = nlohmann::json::parse(item.json, nullptr, false);
        if (j.is_discarded()) {
            continue;
        }
        const std::string kind = j.value("kind", "");
        if (kind == "StaticMesh") {
            StaticMeshComponent mesh;
            const std::string cls = j.value("class", "Cube");
            const std::string meshPath = j.value("mesh", "");
            bool placed = false;
            if (ctx.resources != nullptr) {
                EBasicShape shape{};
                if (tryParseBasicShapeName(cls, shape)) {
                    BasicShape basic;
                    basic.type = shape;
                    mesh = basic.MakeStaticMesh(*ctx.resources);
                    mesh.editorClass = cls;
                    placed = true;
                } else if (!meshPath.empty()) {
                    mesh.mesh = ctx.resources->LoadStaticMesh(ResolveAssetPath(meshPath));
                    if (mesh.mesh != nullptr) {
                        mesh.meshPath = meshPath;
                        mesh.editorClass = cls.empty() ? "StaticMesh" : cls;
                        mesh.material = ctx.resources->DefaultMaterial();
                        placed = true;
                    }
                }
            }
            if (!placed) {
                for (const StaticMeshComponent& src : ctx.level->StaticMeshes()) {
                    if ((!meshPath.empty() && src.meshPath == meshPath) ||
                        src.editorClass == cls ||
                        (cls == "StaticMesh" && !src.meshPath.empty())) {
                        mesh = src;
                        placed = true;
                        break;
                    }
                }
            }
            if (!placed) {
                std::cerr << "Paste: no template mesh for class " << cls << '\n';
                continue;
            }
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                mesh.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                           j["position"][1].get<float>() + kOffset.y,
                                           j["position"][2].get<float>() + kOffset.z};
            } else {
                OffsetTransform(mesh.transform, kOffset);
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                mesh.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                  j["rotation"][1].get<float>(),
                                                  j["rotation"][2].get<float>()};
            }
            if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
                mesh.transform.scale = {j["scale"][0].get<float>(), j["scale"][1].get<float>(),
                                        j["scale"][2].get<float>()};
            }
            mesh.tag = j.value("tag", mesh.tag);
            mesh.editorClass = cls;
            if (!meshPath.empty()) {
                mesh.meshPath = meshPath;
            }
            const std::string matPath = j.value("material", "");
            if (!matPath.empty() && ctx.resources != nullptr) {
                mesh.material = ctx.resources->LoadMaterial(matPath);
                mesh.materialPath = matPath;
                mesh.materialOverride = true;
            }
            mesh.collisionEnabled = j.value("collisionEnabled", mesh.collisionEnabled);
            mesh.simulatePhysics = j.value("simulatePhysics", mesh.simulatePhysics);
            mesh.enableGravity = j.value("enableGravity", mesh.enableGravity);
            mesh.hidden = j.value("hidden", mesh.hidden);
            mesh.lightmap.reset();
            mesh.lightmapPath.clear();
            ctx.level->AddStaticMesh(std::move(mesh));
            ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1,
                       true);
        } else if (kind == "PointLight") {
            PointLight light{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                light.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                            j["position"][1].get<float>() + kOffset.y,
                                            j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("lightColor") && j["lightColor"].is_array() &&
                j["lightColor"].size() >= 3) {
                light.lightColor = {j["lightColor"][0].get<float>(), j["lightColor"][1].get<float>(),
                                    j["lightColor"][2].get<float>()};
            }
            light.intensity = j.value("intensity", light.intensity);
            light.range = j.value("range", light.range);
            light.castShadows = j.value("castShadows", light.castShadows);
            ctx.level->PointLights().push_back(light);
            ctx.Select(EEditorSelectionKind::PointLight, ctx.level->PointLights().size() - 1, true);
        } else if (kind == "PlayerStart") {
            PlayerStart start{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                start.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                            j["position"][1].get<float>() + kOffset.y,
                                            j["position"][2].get<float>() + kOffset.z};
            }
            ctx.level->AddPlayerStart(start);
            ctx.Select(EEditorSelectionKind::PlayerStart, ctx.level->PlayerStarts().size() - 1,
                       true);
        } else if (kind == "TriggerVolume") {
            TriggerVolume volume{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                volume.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                             j["position"][1].get<float>() + kOffset.y,
                                             j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                volume.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                    j["rotation"][1].get<float>(),
                                                    j["rotation"][2].get<float>()};
            }
            if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
                volume.transform.scale = {j["scale"][0].get<float>(), j["scale"][1].get<float>(),
                                          j["scale"][2].get<float>()};
            }
            volume.interactRadius = j.value("interactRadius", volume.interactRadius);
            volume.interactCost = j.value("interactCost", volume.interactCost);
            volume.payload = j.value("payload", volume.payload);
            volume.tag = j.value("tag", volume.tag);
            volume.bConsumeOnUse = j.value("bConsumeOnUse", volume.bConsumeOnUse);
            ctx.level->AddTriggerVolume(volume);
            ctx.Select(EEditorSelectionKind::TriggerVolume, ctx.level->TriggerVolumes().size() - 1,
                       true);
        } else if (kind == "PainCausingVolume") {
            PainCausingVolume volume{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                volume.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                             j["position"][1].get<float>() + kOffset.y,
                                             j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                volume.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                    j["rotation"][1].get<float>(),
                                                    j["rotation"][2].get<float>()};
            }
            if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
                volume.transform.scale = {j["scale"][0].get<float>(), j["scale"][1].get<float>(),
                                          j["scale"][2].get<float>()};
            }
            volume.damagePerSecond = j.value("damagePerSecond", volume.damagePerSecond);
            volume.damageInterval = j.value("damageInterval", volume.damageInterval);
            volume.tag = j.value("tag", volume.tag);
            ctx.level->AddPainCausingVolume(volume);
            ctx.Select(EEditorSelectionKind::PainCausingVolume,
                       ctx.level->PainCausingVolumes().size() - 1, true);
        } else if (kind == "AISpawnPoint") {
            AISpawnPoint point{};
            if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
                point.transform.position = {j["position"][0].get<float>() + kOffset.x,
                                            j["position"][1].get<float>() + kOffset.y,
                                            j["position"][2].get<float>() + kOffset.z};
            }
            if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
                point.transform.rotationDegrees = {j["rotation"][0].get<float>(),
                                                   j["rotation"][1].get<float>(),
                                                   j["rotation"][2].get<float>()};
            }
            point.tag = j.value("tag", point.tag);
            ctx.level->AddAISpawnPoint(point);
            ctx.Select(EEditorSelectionKind::AISpawnPoint, ctx.level->AISpawnPoints().size() - 1,
                       true);
        }
    }
    ctx.MarkDirty();
}

} // namespace leon::editor
