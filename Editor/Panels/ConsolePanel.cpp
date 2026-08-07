#include <leon/editor/panels/ConsolePanel.h>

#include <cstring>
#include <imgui.h>
#include <leon/editor/EditorOutputLog.h>
#include <string>
#include <vector>

namespace leon::editor {
namespace {

void CopyLogSnapshotToClipboard() {
    const auto all = EditorOutputLog::Instance().Snapshot();
    std::string text;
    text.reserve(all.size() * 64);
    for (const EditorLogLine& line : all) {
        text += line.text;
        text.push_back('\n');
    }
    ImGui::SetClipboardText(text.c_str());
}

} // namespace

void ConsolePanel::Execute(EditorContext& ctx, const std::string& line) {
    if (line.empty()) {
        return;
    }
    EditorLogInfo("> " + line);

    if (line == "help" || line == "?") {
        EditorLogInfo("Commands: help, clear, focus, openmat <path.lmat>, stat");
        return;
    }
    if (line == "clear") {
        EditorOutputLog::Instance().Clear();
        return;
    }
    if (line == "focus" || line == "FocusSelected") {
        ctx.RequestFocusSelected();
        EditorLogInfo("Focus Selected requested");
        return;
    }
    if (line == "stat") {
        EditorLogInfo(std::string("dirty=") + (ctx.dirty ? "1" : "0") +
                      " level=" + (ctx.levelPath.empty() ? "(unsaved)" : ctx.levelPath));
        return;
    }
    constexpr const char* kOpenMat = "openmat ";
    if (line.rfind(kOpenMat, 0) == 0) {
        const std::string path = line.substr(std::strlen(kOpenMat));
        if (path.empty()) {
            EditorLogWarn("Usage: openmat <path.lmat>");
            return;
        }
        ctx.requestOpenMaterialPath = path;
        EditorLogInfo("Opening material " + path);
        return;
    }

    EditorLogWarn("Unknown command. Type 'help'.");
}

void ConsolePanel::Draw(EditorContext& ctx) {
    if (!ctx.showConsole) {
        return;
    }
    if (ctx.requestFocusConsole) {
        ImGui::SetNextWindowFocus();
        ctx.requestFocusConsole = false;
    }
    if (!ImGui::Begin("Console", &ctx.showConsole)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear")) {
        EditorOutputLog::Instance().Clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        CopyLogSnapshotToClipboard();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll_);
    ImGui::SameLine();
    ImGui::TextDisabled("Tip: help | clear | openmat path.lmat");

    ImGui::Separator();
    const float footer = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    if (ImGui::BeginChild("##console_scroll", ImVec2(0, -footer), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::GetIO().KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_C) && !ImGui::GetIO().WantTextInput) {
            CopyLogSnapshotToClipboard();
        }
        const auto all = EditorOutputLog::Instance().Snapshot();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(all.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const EditorLogLine& line = all[static_cast<std::size_t>(i)];
                ImVec4 color{0.85f, 0.85f, 0.85f, 1.0f};
                if (line.level == EEditorLogLevel::Warning) {
                    color = ImVec4{0.95f, 0.8f, 0.25f, 1.0f};
                } else if (line.level == EEditorLogLevel::Error) {
                    color = ImVec4{0.95f, 0.35f, 0.3f, 1.0f};
                }
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::PushID(i);
                ImGui::Selectable(line.text.c_str(), false,
                                  ImGuiSelectableFlags_AllowDoubleClick |
                                      ImGuiSelectableFlags_SpanAllColumns);
                if (ImGui::BeginPopupContextItem("##console_line_ctx")) {
                    if (ImGui::MenuItem("Copy Line")) {
                        ImGui::SetClipboardText(line.text.c_str());
                    }
                    if (ImGui::MenuItem("Copy All")) {
                        CopyLogSnapshotToClipboard();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
                ImGui::PopStyleColor();
            }
        }
        if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth(-1.0f);
    const bool submit = ImGui::InputText(
        "##console_input", input_, sizeof(input_),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll);
    if (submit) {
        Execute(ctx, input_);
        input_[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::End();
}

} // namespace leon::editor
