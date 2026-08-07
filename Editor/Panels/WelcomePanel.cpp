#include <leon/editor/panels/WelcomePanel.h>

#include <cstring>
#include <glad/glad.h>
#include <imgui.h>
#include <leon/core/Paths.h>
#include <leon/core/Version.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/EditorFileDialog.h>
#include <stb_image.h>

namespace leon::editor {
namespace {

void CopyToBuf(char* buf, std::size_t size, const std::string& value) {
    if (size == 0) {
        return;
    }
#ifdef _WIN32
    strncpy_s(buf, size, value.c_str(), _TRUNCATE);
#else
    std::strncpy(buf, value.c_str(), size - 1);
    buf[size - 1] = '\0';
#endif
}

} // namespace

WelcomePanel::~WelcomePanel() {
    DestroyBrandTexture();
}

void WelcomePanel::DestroyBrandTexture() {
    if (brandTexture_ != 0) {
        glDeleteTextures(1, &brandTexture_);
        brandTexture_ = 0;
    }
    brandWidth_ = 0;
    brandHeight_ = 0;
}

void WelcomePanel::EnsureBrandTexture() {
    if (brandLoadAttempted_) {
        return;
    }
    brandLoadAttempted_ = true;

    std::string path = ResolveAssetPath("assets/brand/LeonLogoUi.png");
    if (path.empty()) {
        path = ResolveAssetPath("assets/icons/LeonEditor.png");
    }
    if (path.empty()) {
        return;
    }

    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (pixels == nullptr || w <= 0 || h <= 0) {
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return;
    }

    glGenTextures(1, &brandTexture_);
    glBindTexture(GL_TEXTURE_2D, brandTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    brandWidth_ = w;
    brandHeight_ = h;
}

void WelcomePanel::Reset() {
    pendingOpenPath_.clear();
    statusMessage_.clear();
    statusIsError_ = false;
    openCreatePopup_ = false;
    selectedTemplate_ = 0;
    nameBuf_[0] = '\0';
    displayBuf_[0] = '\0';
    CopyToBuf(parentBuf_, sizeof(parentBuf_), EditorProjectService::DefaultProjectsRoot());
    templateIds_ = EditorProjectService::ListTemplateIds();
}

void WelcomePanel::RequestOpen(const std::string& path) {
    std::string directory;
    std::string err;
    if (!EditorProjectService::ResolveProjectDirectory(path, directory, err)) {
        statusIsError_ = true;
        statusMessage_ = err;
        return;
    }
    pendingOpenPath_ = directory;
    statusIsError_ = false;
    statusMessage_.clear();
}

void WelcomePanel::DrawCreateModal(EditorProjectService& projects) {
    if (!ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted("Project name (folder / CMake id)");
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("##proj_name", nameBuf_, sizeof(nameBuf_));

    ImGui::TextUnformatted("Display name");
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("##proj_display", displayBuf_, sizeof(displayBuf_));

    ImGui::TextUnformatted("Create under");
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputText("##proj_parent", parentBuf_, sizeof(parentBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Browse…")) {
        const std::string picked = EditorPickFolder("Choose projects folder");
        if (!picked.empty()) {
            CopyToBuf(parentBuf_, sizeof(parentBuf_), picked);
        }
    }

    templateIds_ = EditorProjectService::ListTemplateIds();
    if (templateIds_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                           "No templates found under Templates/.");
    } else {
        if (selectedTemplate_ < 0 || selectedTemplate_ >= static_cast<int>(templateIds_.size())) {
            selectedTemplate_ = 0;
        }
        ImGui::TextUnformatted("Template");
        ImGui::SetNextItemWidth(360.0f);
        if (ImGui::BeginCombo("##template",
                              templateIds_[static_cast<std::size_t>(selectedTemplate_)].c_str())) {
            for (int i = 0; i < static_cast<int>(templateIds_.size()); ++i) {
                const bool selected = (i == selectedTemplate_);
                if (ImGui::Selectable(templateIds_[static_cast<std::size_t>(i)].c_str(),
                                      selected)) {
                    selectedTemplate_ = i;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Blank = empty starter · ThirdPerson = Bot + arena level");
    }

    if (statusIsError_ && !statusMessage_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", statusMessage_.c_str());
    }

    ImGui::Spacing();
    const bool canCreate = nameBuf_[0] != '\0' && !templateIds_.empty();
    if (!canCreate) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Create", ImVec2(120.0f, 0.0f))) {
        std::string outPath;
        std::string err;
        const std::string name = nameBuf_;
        const std::string display = displayBuf_[0] != '\0' ? std::string(displayBuf_) : name;
        const std::string parent = parentBuf_;
        const std::string templateId = templateIds_[static_cast<std::size_t>(selectedTemplate_)];
        const bool ok = EditorProjectService::CreateFromTemplate(parent, name, display, templateId,
                                                                outPath, err);
        if (ok) {
            projects.Remember(outPath);
            pendingOpenPath_ = outPath;
            statusIsError_ = false;
            statusMessage_.clear();
            ImGui::CloseCurrentPopup();
            openCreatePopup_ = false;
        } else {
            statusIsError_ = true;
            statusMessage_ = err;
        }
    }
    if (!canCreate) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
        statusMessage_.clear();
        statusIsError_ = false;
        ImGui::CloseCurrentPopup();
        openCreatePopup_ = false;
    }

    ImGui::EndPopup();
}

void WelcomePanel::Draw(EditorProjectService& projects) {
    EnsureBrandTexture();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##LeonWelcome", nullptr, flags);

    if (brandTexture_ != 0) {
        constexpr float kLogo = 72.0f;
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(brandTexture_)),
                     ImVec2(kLogo, kLogo));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        ImGui::TextUnformatted("LEON");
        ImGui::TextDisabled("Editor %s — choose a project to continue", EngineVersionString());
        ImGui::EndGroup();
    } else {
        ImGui::TextUnformatted("LEON");
        ImGui::TextDisabled("Editor %s — choose a project to continue", EngineVersionString());
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float leftWidth = 280.0f;
    ImGui::BeginChild("##welcome_actions", ImVec2(leftWidth, 0.0f), true);

    if (ImGui::Button("Open Project…", ImVec2(-1.0f, 36.0f))) {
        const std::string picked = EditorPickFolder("Open Leon Project Folder");
        if (!picked.empty()) {
            RequestOpen(picked);
        }
    }
    if (ImGui::Button("New Project…", ImVec2(-1.0f, 36.0f))) {
        statusMessage_.clear();
        statusIsError_ = false;
        nameBuf_[0] = '\0';
        displayBuf_[0] = '\0';
        CopyToBuf(parentBuf_, sizeof(parentBuf_), EditorProjectService::DefaultProjectsRoot());
        templateIds_ = EditorProjectService::ListTemplateIds();
        selectedTemplate_ = 0;
        for (int i = 0; i < static_cast<int>(templateIds_.size()); ++i) {
            if (templateIds_[static_cast<std::size_t>(i)] == "ThirdPerson") {
                selectedTemplate_ = i;
                break;
            }
        }
        openCreatePopup_ = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Projects root");
    ImGui::TextWrapped("%s", EditorProjectService::DefaultProjectsRoot().c_str());

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##welcome_recents", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Recent Projects");
    ImGui::Separator();
    ImGui::Spacing();

    if (projects.Recents().empty()) {
        ImGui::TextDisabled("No recent projects yet.");
    } else {
        for (const RecentProject& recent : projects.Recents()) {
            ImGui::PushID(recent.path.c_str());
            const std::string label =
                recent.displayName.empty() ? recent.name : recent.displayName;
            ImGui::BeginGroup();
            if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(0.0f, 0.0f))) {
                RequestOpen(recent.path);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", recent.path.c_str());
            }
            if (!recent.engineVersion.empty()) {
                ImGui::TextDisabled("Last opened with Engine %s", recent.engineVersion.c_str());
            } else {
                ImGui::TextDisabled("Last opened with Engine —");
            }
            ImGui::EndGroup();
            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    if (statusIsError_ && !statusMessage_.empty() && !openCreatePopup_) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", statusMessage_.c_str());
    }

    ImGui::EndChild();

    if (openCreatePopup_) {
        ImGui::OpenPopup("New Project");
        openCreatePopup_ = false;
    }
    DrawCreateModal(projects);

    ImGui::End();
    ImGui::PopStyleVar(3);
}

} // namespace leon::editor
