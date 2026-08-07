#include "Ps2LabDemo.h"

#include "EmbeddedSmTriangleLps2.h"

#include <leon/core/InputPad.h>
#include <leon/core/Window.h>
#include <leon/rhi/Ps2RHI.h>

#include <cstdio>

namespace leon::ps2lab {
namespace {

enum class EDemoMode : int {
    Showcase = 0,
    PadPilot = 1,
    StressGrid = 2,
    ClearOnly = 3,
    Count = 4,
};

struct Rgb {
    float r;
    float g;
    float b;
};

struct PilotState {
    float x = 0.0f;
    float y = 0.0f;
    unsigned angle256 = 0;
    float size = 70.0f;
};

constexpr float kHalfWidth = 320.0f;
constexpr float kHalfHeight = 224.0f;

[[nodiscard]] const char* ModeName(EDemoMode mode) {
    switch (mode) {
    case EDemoMode::Showcase:
        return "Showcase";
    case EDemoMode::PadPilot:
        return "PadPilot";
    case EDemoMode::StressGrid:
        return "StressGrid";
    case EDemoMode::ClearOnly:
        return "ClearOnly";
    default:
        return "?";
    }
}

[[nodiscard]] Rgb LerpRgb(Rgb a, Rgb b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

[[nodiscard]] float Wave01(unsigned frame, unsigned period) {
    if (period < 2u) {
        return 0.0f;
    }
    const unsigned phase = frame % period;
    const unsigned half = period / 2u;
    return phase < half ? static_cast<float>(phase) / static_cast<float>(half)
                        : static_cast<float>(period - phase) / static_cast<float>(half);
}

[[nodiscard]] Rgb HueRgb(unsigned hue256) {
    const unsigned h = hue256 % 192u;
    const unsigned seg = h / 64u;
    const float t = static_cast<float>(h % 64u) / 64.0f;
    switch (seg) {
    case 0:
        return {1.0f, t, 0.15f};
    case 1:
        return {1.0f - t, 1.0f, 0.15f};
    default:
        return {0.15f, 1.0f - t * 0.5f, 0.35f + t * 0.65f};
    }
}

void PrintBanner(bool padOk, bool lps2Ok) {
    std::printf("\n========== Leon Ps2Lab ==========\n");
    std::printf("  Cross     Showcase\n");
    std::printf("  Circle    PadPilot (D-Pad / L1-R1 / L2-R2)\n");
    std::printf("  Square    StressGrid\n");
    std::printf("  Triangle  ClearOnly\n");
    std::printf("  Start     quit\n");
    std::printf("Pad=%s LPS2=%s\n", padOk ? "ok" : "--", lps2Ok ? "ok" : "--");
    std::printf("=================================\n\n");
}

void DrawSkyGround(float pulse) {
    const Rgb skyTop = LerpRgb({0.05f, 0.08f, 0.22f}, {0.10f, 0.18f, 0.40f}, pulse);
    const Rgb skyBot = LerpRgb({0.08f, 0.12f, 0.28f}, {0.14f, 0.22f, 0.38f}, pulse);
    const Rgb ground = {0.10f, 0.14f, 0.10f};

    (void)rhi::Ps2DrawUnlitRect(-kHalfWidth, -kHalfHeight, kHalfWidth, 40.0f, skyTop.r, skyTop.g,
                                skyTop.b);
    (void)rhi::Ps2DrawUnlitRect(-kHalfWidth, 40.0f, kHalfWidth, kHalfHeight, skyBot.r, skyBot.g,
                                skyBot.b);
    (void)rhi::Ps2DrawUnlitRect(-kHalfWidth, 120.0f, kHalfWidth, kHalfHeight, ground.r, ground.g,
                                ground.b);
}

void DrawHud(EDemoMode mode, unsigned frame) {
    constexpr float kModeW = 18.0f;
    constexpr float kGap = 6.0f;
    constexpr float kBaseX = -300.0f;
    constexpr float kY0 = -210.0f;
    constexpr float kY1 = -190.0f;

    for (int i = 0; i < static_cast<int>(EDemoMode::Count); ++i) {
        const float x0 = kBaseX + static_cast<float>(i) * (kModeW + kGap);
        const bool on = static_cast<int>(mode) == i;
        const float v = on ? 0.95f : 0.25f;
        (void)rhi::Ps2DrawUnlitRect(x0, kY0, x0 + kModeW, kY1, v, on ? 0.75f : 0.25f,
                                    on ? 0.20f : 0.30f);
    }

    const float barT = Wave01(frame, 90u);
    (void)rhi::Ps2DrawUnlitRect(-300.0f, 200.0f, -300.0f + 600.0f * barT, 212.0f, 0.2f, 0.85f,
                                0.55f);
}

void DrawShowcase(unsigned frame) {
    DrawSkyGround(Wave01(frame, 120u));

    const unsigned spin = (frame * 2u) & 255u;
    (void)rhi::Ps2DrawUnlitTriangleAt(0.0f, -10.0f, 90.0f, spin, 1.0f, 0.82f, 0.18f);

    for (int i = 0; i < 3; ++i) {
        const unsigned orbit = (frame * 3u + static_cast<unsigned>(i) * 85u) & 255u;
        constexpr float kRadius = 150.0f;
        const float ox = rhi::Ps2Cos256(orbit) * kRadius;
        const float oy = rhi::Ps2Sin256(orbit) * kRadius;
        const Rgb rgb = HueRgb(orbit + static_cast<unsigned>(i) * 40u);
        (void)rhi::Ps2DrawUnlitTriangleAt(ox, oy, 28.0f, (spin + orbit) & 255u, rgb.r, rgb.g,
                                          rgb.b);
    }

    (void)rhi::Ps2DrawUnlitTriangleAt(-240.0f, 160.0f, 22.0f, frame & 255u, 0.2f, 0.9f, 0.95f);
}

void ClampPilot(PilotState& pilot) {
    if (pilot.x < -280.0f) {
        pilot.x = -280.0f;
    }
    if (pilot.x > 280.0f) {
        pilot.x = 280.0f;
    }
    if (pilot.y < -180.0f) {
        pilot.y = -180.0f;
    }
    if (pilot.y > 180.0f) {
        pilot.y = 180.0f;
    }
    if (pilot.size < 30.0f) {
        pilot.size = 30.0f;
    }
    if (pilot.size > 140.0f) {
        pilot.size = 140.0f;
    }
}

void DrawPadPilot(unsigned frame, PilotState& pilot) {
    DrawSkyGround(0.35f);

    constexpr float kSpeed = 3.5f;
    if (IsPadButtonPressed(EPadButton::DpadLeft)) {
        pilot.x -= kSpeed;
    }
    if (IsPadButtonPressed(EPadButton::DpadRight)) {
        pilot.x += kSpeed;
    }
    if (IsPadButtonPressed(EPadButton::DpadUp)) {
        pilot.y -= kSpeed;
    }
    if (IsPadButtonPressed(EPadButton::DpadDown)) {
        pilot.y += kSpeed;
    }
    if (IsPadButtonPressed(EPadButton::L1)) {
        pilot.angle256 = (pilot.angle256 + 255u) & 255u;
    }
    if (IsPadButtonPressed(EPadButton::R1)) {
        pilot.angle256 = (pilot.angle256 + 1u) & 255u;
    }
    if (IsPadButtonPressed(EPadButton::L2)) {
        pilot.size -= 1.0f;
    }
    if (IsPadButtonPressed(EPadButton::R2)) {
        pilot.size += 1.0f;
    }
    ClampPilot(pilot);

    const Rgb rgb = HueRgb((frame + pilot.angle256) & 255u);
    (void)rhi::Ps2DrawUnlitTriangleAt(pilot.x, pilot.y, pilot.size, pilot.angle256, rgb.r, rgb.g,
                                      rgb.b);

    (void)rhi::Ps2DrawUnlitRect(-8.0f, -1.0f, 8.0f, 1.0f, 0.7f, 0.7f, 0.8f);
    (void)rhi::Ps2DrawUnlitRect(-1.0f, -8.0f, 1.0f, 8.0f, 0.7f, 0.7f, 0.8f);
}

void DrawStressGrid(unsigned frame) {
    rhi::Ps2ClearColor(0.04f, 0.05f, 0.08f);
    constexpr int kCols = 6;
    constexpr int kRows = 4;
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            const float x = -220.0f + static_cast<float>(col) * 88.0f;
            const float y = -140.0f + static_cast<float>(row) * 90.0f;
            const unsigned idx = static_cast<unsigned>(row * kCols + col);
            const unsigned ang = (frame * 3u + idx * 13u) & 255u;
            const Rgb rgb = HueRgb(ang + idx * 7u);
            (void)rhi::Ps2DrawUnlitTriangleAt(x, y, 26.0f, ang, rgb.r, rgb.g, rgb.b);
        }
    }
}

[[nodiscard]] bool Edge(bool down, bool& prev) {
    const bool pressed = down && !prev;
    prev = down;
    return pressed;
}

} // namespace

int RunPs2LabDemo(Window& window) {
    for (int i = 0; i < 2; ++i) {
        rhi::Ps2ClearColor(0.08f, 0.12f, 0.28f);
        (void)rhi::Ps2DrawUnlitTriangle();
        window.SwapBuffers();
    }

    const bool padOk = InitializePad();
    const bool lps2Ok =
        rhi::Ps2DrawCookedMesh(kEmbeddedSmTriangleLps2, kEmbeddedSmTriangleLps2Size);
    PrintBanner(padOk, lps2Ok);

    EDemoMode mode = EDemoMode::Showcase;
    PilotState pilot{};
    unsigned frame = 0;
    bool prevCross = false;
    bool prevCircle = false;
    bool prevSquare = false;
    bool prevTriangleBtn = false;

    for (;;) {
        window.PollEvents();
        if (IsPadButtonPressed(EPadButton::Start)) {
            break;
        }

        if (Edge(IsPadButtonPressed(EPadButton::Cross), prevCross)) {
            mode = EDemoMode::Showcase;
            std::printf("mode -> %s\n", ModeName(mode));
        }
        if (Edge(IsPadButtonPressed(EPadButton::Circle), prevCircle)) {
            mode = EDemoMode::PadPilot;
            std::printf("mode -> %s\n", ModeName(mode));
        }
        if (Edge(IsPadButtonPressed(EPadButton::Square), prevSquare)) {
            mode = EDemoMode::StressGrid;
            std::printf("mode -> %s\n", ModeName(mode));
        }
        if (Edge(IsPadButtonPressed(EPadButton::Triangle), prevTriangleBtn)) {
            mode = EDemoMode::ClearOnly;
            std::printf("mode -> %s\n", ModeName(mode));
        }

        switch (mode) {
        case EDemoMode::Showcase:
            rhi::Ps2ClearColor(0.02f, 0.03f, 0.06f);
            DrawShowcase(frame);
            DrawHud(mode, frame);
            break;
        case EDemoMode::PadPilot:
            rhi::Ps2ClearColor(0.02f, 0.03f, 0.06f);
            DrawPadPilot(frame, pilot);
            DrawHud(mode, frame);
            break;
        case EDemoMode::StressGrid:
            DrawStressGrid(frame);
            DrawHud(mode, frame);
            break;
        case EDemoMode::ClearOnly: {
            const float w = Wave01(frame, 100u);
            rhi::Ps2ClearColor(0.05f + 0.25f * w, 0.08f + 0.10f * (1.0f - w), 0.20f + 0.40f * w);
            DrawHud(mode, frame);
            break;
        }
        default:
            break;
        }

        window.SwapBuffers();
        ++frame;
    }

    std::printf("Ps2Lab: quit after %u frames\n", frame);
    return 0;
}

} // namespace leon::ps2lab
