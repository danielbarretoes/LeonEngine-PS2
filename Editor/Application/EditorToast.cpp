#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <leon/editor/EditorToast.h>
#include <mutex>
#include <vector>


namespace leon::editor {
namespace {

struct ToastEntry {
    std::string message;
    EEditorToastKind kind = EEditorToastKind::Info;
    double expireAt = 0.0;
    float duration = 3.5f;
};

std::mutex g_mutex;
std::vector<ToastEntry> g_toasts;

ImVec4 KindColor(EEditorToastKind kind) {
    switch (kind) {
    case EEditorToastKind::Success:
        return {0.35f, 0.82f, 0.45f, 1.0f};
    case EEditorToastKind::Warning:
        return {0.95f, 0.78f, 0.28f, 1.0f};
    case EEditorToastKind::Error:
        return {0.95f, 0.38f, 0.32f, 1.0f};
    case EEditorToastKind::Info:
    default:
        return {0.55f, 0.75f, 0.95f, 1.0f};
    }
}

const char* KindLabel(EEditorToastKind kind) {
    switch (kind) {
    case EEditorToastKind::Success:
        return "OK";
    case EEditorToastKind::Warning:
        return "Warn";
    case EEditorToastKind::Error:
        return "Error";
    case EEditorToastKind::Info:
    default:
        return "Build";
    }
}

} // namespace

void EditorToast(std::string message, EEditorToastKind kind, float seconds) {
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    if (message.empty()) {
        return;
    }
    if (seconds < 1.0f) {
        seconds = 1.0f;
    }
    std::lock_guard lock(g_mutex);
    if (g_toasts.size() >= 6) {
        g_toasts.erase(g_toasts.begin());
    }
    // expireAt filled on first Draw (needs ImGui time).
    g_toasts.push_back(ToastEntry{std::move(message), kind, 0.0, seconds});
}

void DrawEditorToasts() {
    const double now = ImGui::GetTime();
    std::vector<ToastEntry> local;
    {
        std::lock_guard lock(g_mutex);
        for (ToastEntry& t : g_toasts) {
            if (t.expireAt <= 0.0) {
                t.expireAt = now + static_cast<double>(t.duration);
            }
        }
        g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(),
                                      [now](const ToastEntry& t) { return now >= t.expireAt; }),
                       g_toasts.end());
        local = g_toasts;
    }
    if (local.empty()) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = 16.0f;
    float y = viewport->WorkPos.y + viewport->WorkSize.y - margin;

    for (int i = static_cast<int>(local.size()) - 1; i >= 0; --i) {
        const ToastEntry& t = local[static_cast<std::size_t>(i)];
        const float life = static_cast<float>(t.expireAt - now);
        float alpha = 1.0f;
        if (life < 0.45f) {
            alpha = life / 0.45f;
        }
        const ImVec4 accent = KindColor(t.kind);

        ImGui::SetNextWindowBgAlpha(0.92f * alpha);
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin, y),
                                ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.13f, 0.94f * alpha));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(accent.x, accent.y, accent.z, 0.85f * alpha));

        char id[32];
        std::snprintf(id, sizeof(id), "##toast_%d", i);
        if (ImGui::Begin(id, nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoDocking)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(accent.x, accent.y, accent.z, alpha));
            ImGui::TextUnformatted(KindLabel(t.kind));
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.92f, 0.92f, alpha));
            ImGui::TextUnformatted(t.message.c_str());
            ImGui::PopStyleColor();
            y -= ImGui::GetWindowSize().y + 8.0f;
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}

} // namespace leon::editor
