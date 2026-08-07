#include <leon/editor/panels/OutputLogPanel.h>

#include <imgui.h>
#include <leon/editor/EditorOutputLog.h>
#include <string>
#include <vector>

namespace leon::editor {
namespace {

void CopyLinesToClipboard(const std::vector<const EditorLogLine*>& lines) {
    std::string text;
    text.reserve(lines.size() * 64);
    for (const EditorLogLine* line : lines) {
        if (line == nullptr) {
            continue;
        }
        text += line->text;
        text.push_back('\n');
    }
    ImGui::SetClipboardText(text.c_str());
}

} // namespace

void OutputLogPanel::Draw(EditorContext& ctx) {
    if (!ctx.showOutputLog) {
        return;
    }
    if (ctx.requestFocusOutputLog) {
        ImGui::SetNextWindowFocus();
        ctx.requestFocusOutputLog = false;
    }
    if (!ImGui::Begin("Output Log", &ctx.showOutputLog)) {
        ImGui::End();
        return;
    }

    const auto all = EditorOutputLog::Instance().Snapshot();
    std::vector<const EditorLogLine*> visible;
    visible.reserve(all.size());
    for (const EditorLogLine& line : all) {
        if (filterLevel_ == 1 && line.level < EEditorLogLevel::Warning) {
            continue;
        }
        if (filterLevel_ == 2 && line.level < EEditorLogLevel::Error) {
            continue;
        }
        visible.push_back(&line);
    }

    if (ImGui::Button("Clear")) {
        EditorOutputLog::Instance().Clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        CopyLinesToClipboard(visible);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Copy filtered log to clipboard");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##log_filter", &filterLevel_, "All\0Warning+\0Error\0");

    ImGui::Separator();
    if (ImGui::BeginChild("##log_scroll", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        // Ctrl+C copies the filtered log when this pane is focused.
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
            ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) &&
            !ImGui::GetIO().WantTextInput) {
            CopyLinesToClipboard(visible);
        }

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const EditorLogLine& line = *visible[static_cast<std::size_t>(i)];
                ImVec4 color{0.85f, 0.85f, 0.85f, 1.0f};
                if (line.level == EEditorLogLevel::Warning) {
                    color = ImVec4{0.95f, 0.8f, 0.25f, 1.0f};
                } else if (line.level == EEditorLogLevel::Error) {
                    color = ImVec4{0.95f, 0.35f, 0.3f, 1.0f};
                }
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::PushID(i);
                // Selectable lets the user highlight a line; right-click copies.
                ImGui::Selectable(line.text.c_str(), false,
                                  ImGuiSelectableFlags_AllowDoubleClick |
                                      ImGuiSelectableFlags_SpanAllColumns);
                if (ImGui::BeginPopupContextItem("##log_line_ctx")) {
                    if (ImGui::MenuItem("Copy Line")) {
                        ImGui::SetClipboardText(line.text.c_str());
                    }
                    if (ImGui::MenuItem("Copy All (filtered)")) {
                        CopyLinesToClipboard(visible);
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
    ImGui::End();
}

} // namespace leon::editor
