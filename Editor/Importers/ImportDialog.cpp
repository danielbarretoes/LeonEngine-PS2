#include <cstdio>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/EditorFileDialog.h>
#include <leon/editor/panels/ImportDialog.h>

namespace leon::editor {
namespace fs = std::filesystem;

void ImportDialog::Open() {
    open_ = true;
    status_.clear();
    ImGui::OpenPopup("Import Asset");
}

void ImportDialog::Draw(EditorContext& ctx) {
    if (open_) {
        ImGui::OpenPopup("Import Asset");
    }
    if (!ImGui::BeginPopupModal("Import Asset", &open_, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted("Import into the project (Unreal-like Content Browser)");
    ImGui::Separator();

    ImGui::RadioButton("Static Mesh (OBJ)", &mode_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Static Mesh (FBX)", &mode_, 3);
    ImGui::SameLine();
    ImGui::RadioButton("Static Mesh (glTF)", &mode_, 4);
    ImGui::RadioButton("Character (FBX skinned)", &mode_, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Animation (FBX)", &mode_, 2);

    ImGui::InputText("Source", sourcePath_, sizeof(sourcePath_));
    ImGui::SameLine();
    if (ImGui::Button("Browse##src")) {
        const char* filter = "Autodesk FBX\0*.fbx\0All\0*.*\0";
        if (mode_ == 0) {
            filter = "Wavefront OBJ\0*.obj\0All\0*.*\0";
        } else if (mode_ == 4) {
            filter = "glTF\0*.gltf;*.glb\0All\0*.*\0";
        }
        const std::string picked = EditorPickOpenFile(filter, "Import source");
        if (!picked.empty()) {
            (void)std::snprintf(sourcePath_, sizeof(sourcePath_), "%s", picked.c_str());
            if (assetName_[0] == '\0') {
                (void)std::snprintf(assetName_, sizeof(assetName_), "%s",
                                    fs::path(picked).stem().string().c_str());
            }
        }
    }

    if (mode_ == 1) {
        ImGui::InputText("Run / Locomotion FBX (optional)", secondaryPath_, sizeof(secondaryPath_));
        ImGui::SameLine();
        if (ImGui::Button("Browse##run")) {
            const std::string picked =
                EditorPickOpenFile("Autodesk FBX\0*.fbx\0All\0*.*\0", "Run FBX");
            if (!picked.empty()) {
                (void)std::snprintf(secondaryPath_, sizeof(secondaryPath_), "%s", picked.c_str());
            }
        }
        ImGui::TextDisabled("Cooks skeleton + skelmesh + idle/run anims under assets/characters/");
    }

    if (mode_ == 2) {
        ImGui::InputText("Skeleton", skeletonPath_, sizeof(skeletonPath_));
        ImGui::SameLine();
        if (ImGui::Button("Browse##skel")) {
            const std::string picked =
                EditorPickOpenFile("Leon Skeleton\0*.lskel\0All\0*.*\0", "Skeleton");
            if (!picked.empty()) {
                (void)std::snprintf(skeletonPath_, sizeof(skeletonPath_), "%s", picked.c_str());
            }
        }
        ImGui::Checkbox("Looping", &animLooping_);
    }

    ImGui::InputText("Asset Name", assetName_, sizeof(assetName_));

    if (!status_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status_.c_str());
    }

    ImGui::Separator();
    if (ImGui::Button("Import", ImVec2(120, 0))) {
        AssetImportRequest req;
        req.mode = static_cast<EAssetImportMode>(mode_);
        req.sourcePath = sourcePath_;
        req.secondaryFbxPath = secondaryPath_;
        req.skeletonJsonPath = skeletonPath_;
        req.assetName = assetName_;
        req.animLooping = animLooping_;

        AssetImportResult result;
        if (EditorImportAsset(ctx, req, result)) {
            status_ = result.message;
            ctx.previewAssetPath = result.previewPath;
            ctx.requestContentRefresh = true;
            ctx.requestPreviewReload = true;
        } else {
            status_ = result.message.empty() ? "Import failed" : result.message;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(120, 0))) {
        open_ = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

} // namespace leon::editor
