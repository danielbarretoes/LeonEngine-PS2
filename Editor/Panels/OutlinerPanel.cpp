#include <cstddef>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/panels/OutlinerPanel.h>
#include <leon/gameplay/Actor.h>
#include <leon/gameplay/SceneComponent.h>
#include <leon/gameplay/World.h>
#include <leon/level/Level.h>
#include <typeinfo>

namespace leon::editor {
namespace {

const char* MeshLabel(const StaticMeshComponent& mesh, std::size_t index, char* buf,
                      std::size_t bufSize) {
    if (!mesh.tag.empty()) {
        (void)std::snprintf(buf, bufSize, "%s##mesh%zu", mesh.tag.c_str(), index);
        return buf;
    }
    const char* cls = mesh.editorClass.empty() ? "Mesh" : mesh.editorClass.c_str();
    (void)std::snprintf(buf, bufSize, "%s_%zu##mesh%zu", cls, index, index);
    return buf;
}

void DrawSceneComponentTree(SceneComponent& component, int depth) {
    char label[160];
    const char* typeName = typeid(component).name();
    if (std::strncmp(typeName, "class ", 6) == 0) {
        typeName += 6;
    } else if (std::strncmp(typeName, "struct ", 7) == 0) {
        typeName += 7;
    }
    if (depth == 0) {
        (void)std::snprintf(label, sizeof(label), "Root (%s)##sc%p", typeName,
                      static_cast<void*>(&component));
    } else {
        (void)std::snprintf(label, sizeof(label), "%s##sc%p", typeName, static_cast<void*>(&component));
    }

    const auto& children = component.GetAttachChildren();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (depth == 0) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    const bool open = ImGui::TreeNodeEx(label, flags);
    if (!children.empty() && open) {
        for (SceneComponent* child : children) {
            if (child != nullptr) {
                DrawSceneComponentTree(*child, depth + 1);
            }
        }
        ImGui::TreePop();
    }
}

bool SelectableLeaf(const char* label, bool selected) {
    return ImGui::Selectable(label, selected, ImGuiSelectableFlags_SpanAllColumns);
}

} // namespace

void OutlinerPanel::HandleDelete(EditorContext& ctx) {
    if (ImGui::GetIO().WantTextInput) {
        return;
    }
    if (!ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        return;
    }
    EditorCommands::DeleteSelection(ctx, ctx.history);
}

void OutlinerPanel::Draw(EditorContext& ctx) {
    if (!ctx.showOutliner) {
        return;
    }
    if (!ImGui::Begin("World Outliner", &ctx.showOutliner)) {
        ImGui::End();
        return;
    }

    HandleDelete(ctx);

    char labelBuf[160];
    constexpr ImGuiTreeNodeFlags kFolderFlags =
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_OpenOnArrow;

    if (ImGui::TreeNodeEx("Level", kFolderFlags)) {
        if (ImGui::TreeNodeEx("StaticMeshes", kFolderFlags) && ctx.level) {
            for (std::size_t i = 0; i < ctx.level->StaticMeshes().size(); ++i) {
                const bool selected = ctx.IsSelected(EEditorSelectionKind::StaticMesh, i);
                ImGui::PushID(static_cast<int>(i));
                if (SelectableLeaf(
                        MeshLabel(ctx.level->StaticMeshes()[i], i, labelBuf, sizeof(labelBuf)),
                        selected)) {
                    ctx.Select(EEditorSelectionKind::StaticMesh, i, ImGui::GetIO().KeyCtrl);
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Lights", kFolderFlags) && ctx.level) {
            for (std::size_t i = 0; i < ctx.level->DirectionalLights().size(); ++i) {
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "DirectionalLight_%zu##dir%zu", i, i);
                const bool selected = ctx.IsSelected(EEditorSelectionKind::DirectionalLight, i);
                if (SelectableLeaf(labelBuf, selected)) {
                    ctx.Select(EEditorSelectionKind::DirectionalLight, i, ImGui::GetIO().KeyCtrl);
                }
            }
            for (std::size_t i = 0; i < ctx.level->PointLights().size(); ++i) {
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "PointLight_%zu##pt%zu", i, i);
                const bool selected = ctx.IsSelected(EEditorSelectionKind::PointLight, i);
                if (SelectableLeaf(labelBuf, selected)) {
                    ctx.Select(EEditorSelectionKind::PointLight, i, ImGui::GetIO().KeyCtrl);
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("PlayerStarts", kFolderFlags) && ctx.level) {
            for (std::size_t i = 0; i < ctx.level->PlayerStarts().size(); ++i) {
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "PlayerStart_%zu##ps%zu", i, i);
                const bool selected = ctx.IsSelected(EEditorSelectionKind::PlayerStart, i);
                if (SelectableLeaf(labelBuf, selected)) {
                    ctx.Select(EEditorSelectionKind::PlayerStart, i, ImGui::GetIO().KeyCtrl);
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("TriggerVolumes", kFolderFlags) && ctx.level) {
            for (std::size_t i = 0; i < ctx.level->TriggerVolumes().size(); ++i) {
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "TriggerVolume_%zu##tv%zu", i, i);
                const bool selected = ctx.IsSelected(EEditorSelectionKind::TriggerVolume, i);
                if (SelectableLeaf(labelBuf, selected)) {
                    ctx.Select(EEditorSelectionKind::TriggerVolume, i, ImGui::GetIO().KeyCtrl);
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("PainCausingVolumes", kFolderFlags) && ctx.level) {
            for (std::size_t i = 0; i < ctx.level->PainCausingVolumes().size(); ++i) {
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "PainCausingVolume_%zu##pcv%zu", i,
                                    i);
                const bool selected = ctx.IsSelected(EEditorSelectionKind::PainCausingVolume, i);
                if (SelectableLeaf(labelBuf, selected)) {
                    ctx.Select(EEditorSelectionKind::PainCausingVolume, i, ImGui::GetIO().KeyCtrl);
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("AISpawnPoints", kFolderFlags) && ctx.level) {
            for (std::size_t i = 0; i < ctx.level->AISpawnPoints().size(); ++i) {
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "AISpawnPoint_%zu##ais%zu", i, i);
                const bool selected = ctx.IsSelected(EEditorSelectionKind::AISpawnPoint, i);
                if (SelectableLeaf(labelBuf, selected)) {
                    ctx.Select(EEditorSelectionKind::AISpawnPoint, i, ImGui::GetIO().KeyCtrl);
                }
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("World", kFolderFlags)) {
        if (ctx.world == nullptr) {
            ImGui::TextDisabled("(no World)");
        } else if (!ctx.piePlaying && ctx.world->ActorCount() == 0) {
            ImGui::TextDisabled("(start PIE to spawn gameplay Actors)");
        } else {
            std::size_t i = 0;
            ctx.world->ForEachActor([&](Actor& actor) {
                const char* pretty = typeid(actor).name();
                if (std::strncmp(pretty, "class ", 6) == 0) {
                    pretty += 6;
                } else if (std::strncmp(pretty, "struct ", 7) == 0) {
                    pretty += 7;
                }
                (void)std::snprintf(labelBuf, sizeof(labelBuf), "%s##actor%zu", pretty, i);

                const bool selected = ctx.selection.Equals(EEditorSelectionKind::Actor, i);
                ImGuiTreeNodeFlags flags =
                    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                    ImGuiTreeNodeFlags_DefaultOpen;
                if (selected) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                ImGui::PushID(static_cast<int>(i) + 10000);
                const bool open = ImGui::TreeNodeEx(labelBuf, flags);
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    ctx.Select(EEditorSelectionKind::Actor, i);
                }
                if (open) {
                    DrawSceneComponentTree(actor.GetRootComponent(), 0);
                    ImGui::TreePop();
                }
                ImGui::PopID();
                ++i;
            });
            if (i == 0) {
                ImGui::TextDisabled("(empty)");
            }
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

} // namespace leon::editor
