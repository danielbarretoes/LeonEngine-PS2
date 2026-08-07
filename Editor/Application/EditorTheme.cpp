#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <leon/core/Paths.h>
#include <leon/editor/EditorTheme.h>
#include <string>
#include <system_error>


namespace leon::editor {
namespace {

ImVec4 Rgb(int r, int g, int b, float a = 1.0f) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

[[nodiscard]] std::string ResolveEditorFontPath(const char* fileName) {
    const std::filesystem::path exeDir = ExecutableDirectory();
    const std::string candidates[] = {
        (exeDir / "assets" / "fonts" / fileName).generic_string(),
        (exeDir / ".." / ".." / "Editor" / "Resources" / "Fonts" / fileName).generic_string(),
        (std::filesystem::path("Editor") / "Resources" / "Fonts" / fileName).generic_string(),
        (std::filesystem::path("assets") / "fonts" / fileName).generic_string(),
    };
    for (const std::string& path : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec) && !ec) {
            return path;
        }
    }
    return ResolveAssetPath(std::string("assets/fonts/") + fileName);
}

} // namespace

void ApplyEditorTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Compact, low-rounding slate chrome (UE5 Slate) with slightly larger hit targets.
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.CellPadding = ImVec2(5.0f, 3.0f);
    style.ItemSpacing = ImVec2(8.0f, 5.0f);
    style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextPadding = ImVec2(12.0f, 2.0f);
    style.TabBarOverlineSize = 2.0f;
    style.DockingSeparatorSize = 2.0f;
    style.HoverStationaryDelay = 0.12f;
    style.HoverDelayShort = 0.12f;
    style.HoverDelayNormal = 0.35f;

    // UE5-ish dark slate + selection blue (#0070E0); higher text contrast for readability.
    const ImVec4 bg = Rgb(24, 24, 24);
    const ImVec4 bgPanel = Rgb(36, 36, 36);
    const ImVec4 bgDeep = Rgb(18, 18, 18);
    const ImVec4 bgHover = Rgb(56, 56, 56);
    const ImVec4 frame = Rgb(48, 48, 48);
    const ImVec4 frameHover = Rgb(64, 64, 64);
    const ImVec4 frameActive = Rgb(80, 80, 80);
    const ImVec4 border = Rgb(10, 10, 10);
    const ImVec4 text = Rgb(230, 230, 230);
    const ImVec4 textDisabled = Rgb(140, 140, 140);
    const ImVec4 accent = Rgb(0, 112, 224);
    const ImVec4 accentHover = Rgb(26, 140, 255);
    const ImVec4 accentDim = Rgb(0, 90, 180);
    const ImVec4 tabIdle = Rgb(30, 30, 30);
    const ImVec4 tabActive = Rgb(48, 48, 48);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = textDisabled;
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bgPanel;
    c[ImGuiCol_PopupBg] = Rgb(28, 28, 28, 0.98f);
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = frame;
    c[ImGuiCol_FrameBgHovered] = frameHover;
    c[ImGuiCol_FrameBgActive] = frameActive;
    c[ImGuiCol_TitleBg] = bgDeep;
    c[ImGuiCol_TitleBgActive] = bgDeep;
    c[ImGuiCol_TitleBgCollapsed] = bgDeep;
    c[ImGuiCol_MenuBarBg] = Rgb(32, 32, 32);
    c[ImGuiCol_ScrollbarBg] = bgDeep;
    c[ImGuiCol_ScrollbarGrab] = Rgb(72, 72, 72);
    c[ImGuiCol_ScrollbarGrabHovered] = Rgb(96, 96, 96);
    c[ImGuiCol_ScrollbarGrabActive] = Rgb(120, 120, 120);
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = Rgb(140, 140, 140);
    c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Button] = frame;
    c[ImGuiCol_ButtonHovered] = bgHover;
    c[ImGuiCol_ButtonActive] = accentDim;
    c[ImGuiCol_Header] = Rgb(48, 48, 48);
    c[ImGuiCol_HeaderHovered] = bgHover;
    c[ImGuiCol_HeaderActive] = accentDim;
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = accentHover;
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_ResizeGrip] = Rgb(60, 60, 60, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = accentHover;
    c[ImGuiCol_ResizeGripActive] = accent;
    c[ImGuiCol_Tab] = tabIdle;
    c[ImGuiCol_TabHovered] = bgHover;
    c[ImGuiCol_TabSelected] = tabActive;
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = bgDeep;
    c[ImGuiCol_TabDimmedSelected] = Rgb(40, 40, 40);
    c[ImGuiCol_TabDimmedSelectedOverline] = accentDim;
    c[ImGuiCol_DockingPreview] = Rgb(0, 112, 224, 0.45f);
    c[ImGuiCol_DockingEmptyBg] = bgDeep;
    c[ImGuiCol_PlotLines] = Rgb(180, 180, 180);
    c[ImGuiCol_PlotLinesHovered] = accentHover;
    c[ImGuiCol_PlotHistogram] = accentDim;
    c[ImGuiCol_PlotHistogramHovered] = accent;
    c[ImGuiCol_TableHeaderBg] = Rgb(40, 40, 40);
    c[ImGuiCol_TableBorderStrong] = border;
    c[ImGuiCol_TableBorderLight] = Rgb(40, 40, 40);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = Rgb(255, 255, 255, 0.02f);
    c[ImGuiCol_TextSelectedBg] = Rgb(0, 112, 224, 0.35f);
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavHighlight] = accent;
    c[ImGuiCol_NavWindowingHighlight] = Rgb(255, 255, 255, 0.5f);
    c[ImGuiCol_NavWindowingDimBg] = Rgb(0, 0, 0, 0.55f);
    c[ImGuiCol_ModalWindowDimBg] = Rgb(0, 0, 0, 0.55f);
}

bool LoadEditorFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const std::string regularPath = ResolveEditorFontPath("Inter-Regular.ttf");
    const std::string mediumPath = ResolveEditorFontPath("Inter-Medium.ttf");
    const float sizePx = 15.0f;

    // Default Latin + punctuation used in UI labels ("…", "—", en-dash, arrows).
    // Without these ranges ImGui draws '?' for missing glyphs.
    static const ImWchar kGlyphRanges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
        0x2010, 0x2027, // General Punctuation (– — …)
        0x2190, 0x2193, // ← ↑ → ↓
        0,
    };

    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    config.PixelSnapH = true;
    config.GlyphRanges = kGlyphRanges;

    ImFont* regular = nullptr;
    if (!regularPath.empty()) {
        regular = io.Fonts->AddFontFromFileTTF(regularPath.c_str(), sizePx, &config);
    }
    if (regular == nullptr) {
        regular = io.Fonts->AddFontDefault();
        std::cerr << "Editor: Inter-Regular.ttf not found; using ImGui default font\n";
        return false;
    }

    if (!mediumPath.empty()) {
        ImFontConfig mediumCfg = config;
        mediumCfg.MergeMode = false;
        (void)io.Fonts->AddFontFromFileTTF(mediumPath.c_str(), sizePx, &mediumCfg);
    }

    io.FontDefault = regular;
    return true;
}

} // namespace leon::editor
