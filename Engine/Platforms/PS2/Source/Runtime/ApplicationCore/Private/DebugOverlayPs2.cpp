#include "Stats/DebugOverlay.h"
#include "InputPad.h"
#include "HAL/MemoryStats.h"
#include "Ps2RHI.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace leon {
namespace {

bool g_statsVisible = true;
bool g_padWidgetVisible = true;
bool g_prevSelect = false;

std::uint64_t g_frameStartUs = 0;
std::uint64_t g_prevDrawUs = 0;
std::uint64_t g_accumUs = 0;
unsigned g_accumFrames = 0;
float g_workSumMs = 0.0f;

constexpr unsigned kLineChars = 32;
char g_lineFps[kLineChars] = "FPS --";
char g_lineRam[kLineChars] = "RAM --";
char g_lineVram[kLineChars] = "VRAM --";
char g_lineRes[kLineChars] = "RES --";
char g_extraLines[kStatsHudExtraLines][kLineChars] = {};

// Shared layout (screen px). Pad widget scale sets the common padding.
constexpr float kMargin = 10.0f;
constexpr float kPadScale = 0.75f;
constexpr float kPadding = kPadDebugPadding * kPadScale;
constexpr float kTextScale = 0.5f;
constexpr float kGlyphW = 6.0f * kTextScale * 2.0f; // advance: 6 cells × 1 px
constexpr float kGlyphH = 7.0f * kTextScale * 2.0f; // 7 cells × 1 px
constexpr float kLineGap = 2.0f;
constexpr float kStatsChars = 16.0f;
constexpr float kPanelAlpha = 0.5f;
constexpr unsigned kStatsPeriodUs = 250000u;

/// Integer tenths: EE has no hardware double, keep printf off the float path.
void FormatMb(char* out, unsigned outSize, const char* label, std::size_t used,
              std::size_t total) {
    const unsigned usedTenths = static_cast<unsigned>((used * 10u + 512u * 1024u) / (1024u * 1024u));
    const unsigned totalTenths =
        static_cast<unsigned>((total * 10u + 512u * 1024u) / (1024u * 1024u));
    std::snprintf(out, outSize, "%s %u.%u/%u.%u MB", label, usedTenths / 10u, usedTenths % 10u,
                  totalTenths / 10u, totalTenths % 10u);
}

void RefreshStats(int screenWidth, int screenHeight, float workMs) {
    const std::uint64_t nowUs = rhi::Ps2GetSystemTimeUs();
    if (g_prevDrawUs != 0) {
        g_accumUs += nowUs - g_prevDrawUs;
        g_workSumMs += workMs;
        ++g_accumFrames;
    }
    g_prevDrawUs = nowUs;
    if (g_accumUs < kStatsPeriodUs || g_accumFrames == 0) {
        return;
    }

    const float secs = static_cast<float>(g_accumUs) / 1000000.0f;
    const int fps = static_cast<int>(static_cast<float>(g_accumFrames) / secs + 0.5f);
    int tenths = static_cast<int>(g_workSumMs / static_cast<float>(g_accumFrames) * 10.0f + 0.5f);
    if (tenths < 0) {
        tenths = 0;
    }
    std::snprintf(g_lineFps, sizeof(g_lineFps), "FPS %d  %d.%d ms", fps, tenths / 10,
                  tenths % 10);

    const MemorySnapshot mem = queryMemorySnapshot();
    FormatMb(g_lineRam, sizeof(g_lineRam), "RAM", mem.processWorkingSetBytes, mem.ramBudgetBytes);
    if (mem.gpuValid && mem.gpuReportsUsage) {
        FormatMb(g_lineVram, sizeof(g_lineVram), "VRAM", mem.gpuUsedBytes, mem.gpuBudgetBytes);
    }
    std::snprintf(g_lineRes, sizeof(g_lineRes), "RES %dX%d", screenWidth, screenHeight);

    g_accumUs = 0;
    g_accumFrames = 0;
    g_workSumMs = 0.0f;
}

struct HudLine {
    const char* Text;
    float R;
    float G;
    float B;
};

void DrawStatsHud(int screenWidth, int screenHeight) {
    HudLine lines[4 + kStatsHudExtraLines] = {
        {g_lineFps, 0.95f, 0.95f, 0.75f},
        {g_lineRam, 0.75f, 0.95f, 0.80f},
        {g_lineVram, 0.95f, 0.80f, 0.90f},
        {g_lineRes, 0.80f, 0.90f, 0.95f},
    };
    unsigned count = 4;
    for (unsigned i = 0; i < kStatsHudExtraLines; ++i) {
        if (g_extraLines[i][0] != '\0') {
            lines[count++] = {g_extraLines[i], 0.70f, 0.85f, 0.85f};
        }
    }

    std::size_t chars = static_cast<std::size_t>(kStatsChars);
    for (unsigned i = 0; i < count; ++i) {
        const std::size_t len = std::strlen(lines[i].Text);
        chars = len > chars ? len : chars;
    }

    const float left = -static_cast<float>(screenWidth) * 0.5f + kMargin;
    const float top = -static_cast<float>(screenHeight) * 0.5f + kMargin;
    const float width = kPadding * 2.0f + static_cast<float>(chars) * kGlyphW - 1.0f;
    const float height = kPadding * 2.0f + static_cast<float>(count) * kGlyphH +
                         static_cast<float>(count - 1u) * kLineGap;
    (void)rhi::Ps2DrawUnlitRectAlpha(left, top, left + width, top + height, 0.02f, 0.03f, 0.05f,
                                     kPanelAlpha);

    float y = top + kPadding;
    for (unsigned i = 0; i < count; ++i) {
        rhi::Ps2DrawDebugHudText(left + kPadding, y, lines[i].Text, lines[i].R, lines[i].G,
                                 lines[i].B, kTextScale);
        y += kGlyphH + kLineGap;
    }
}

/// Select: both → stats only → pad only → none → both.
void CycleOverlayMode() {
    if (g_statsVisible && g_padWidgetVisible) {
        g_padWidgetVisible = false;
    } else if (g_statsVisible) {
        g_statsVisible = false;
        g_padWidgetVisible = true;
    } else if (g_padWidgetVisible) {
        g_padWidgetVisible = false;
    } else {
        g_statsVisible = true;
        g_padWidgetVisible = true;
    }
    std::printf("DebugOverlay: stats %s, pad %s\n", g_statsVisible ? "on" : "off",
                g_padWidgetVisible ? "on" : "off");
}

} // namespace

void SetStatsHudVisible(bool visible) { g_statsVisible = visible; }
bool IsStatsHudVisible() { return g_statsVisible; }

void SetPadDebugOverlayVisible(bool visible) { g_padWidgetVisible = visible; }
bool IsPadDebugOverlayVisible() { return g_padWidgetVisible; }

void SetStatsHudExtraLine(unsigned slot, const char* text) {
    if (slot >= kStatsHudExtraLines) {
        return;
    }
    if (text == nullptr) {
        g_extraLines[slot][0] = '\0';
        return;
    }
    std::strncpy(g_extraLines[slot], text, kLineChars - 1u);
    g_extraLines[slot][kLineChars - 1u] = '\0';
}

void DrawEngineDebugOverlay(int screenWidth, int screenHeight) {
    // Game work this frame, measured before the overlay adds its own draws.
    const std::uint64_t nowUs = rhi::Ps2GetSystemTimeUs();
    const float workMs =
        g_frameStartUs != 0 ? static_cast<float>(nowUs - g_frameStartUs) / 1000.0f : 0.0f;

    const bool select = IsPadButtonPressed(EPadButton::Select);
    if (select && !g_prevSelect) {
        CycleOverlayMode();
    }
    g_prevSelect = select;

    RefreshStats(screenWidth, screenHeight, workMs);
    if (g_statsVisible) {
        DrawStatsHud(screenWidth, screenHeight);
    }
    if (g_padWidgetVisible) {
        const float x =
            static_cast<float>(screenWidth) * 0.5f - kMargin - kPadDebugWidth * kPadScale;
        const float y = -static_cast<float>(screenHeight) * 0.5f + kMargin;
        DrawPadDebugOverlay(x, y, kPadScale);
    }
}

void MarkEngineFrameStart() {
    g_frameStartUs = rhi::Ps2GetSystemTimeUs();
    rhi::Ps2Draw3DDebugBeginFrame();
}

} // namespace leon
