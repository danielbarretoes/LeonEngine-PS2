#include "EmbeddedTriangleLps2.h"

#include <leon/core/InputPad.h>
#include <leon/core/Window.h>
#include <leon/rhi/Ps2RHI.h>

#include <cstdio>

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
    if (period == 0) {
        return 0.0f;
    }
    const unsigned phase = frame % period;
    const unsigned half = period / 2u;
    if (half == 0) {
        return 0.0f;
    }
    return phase < half ? static_cast<float>(phase) / static_cast<float>(half)
                        : static_cast<float>(period - phase) / static_cast<float>(half);
}

[[nodiscard]] Rgb HueRgb(unsigned hue256) {
    // Cheap 3-segment hue wheel (no libm).
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
    std::printf("Modes:\n");
    std::printf("  Cross   Showcase  (scene + orbits)\n");
    std::printf("  Circle  PadPilot  (D-Pad move, L1/R1 spin)\n");
    std::printf("  Square  StressGrid (many tris)\n");
    std::printf("  Triangle ClearOnly\n");
    std::printf("  Start   quit\n");
    std::printf("Pad=%s LPS2=%s\n", padOk ? "ok" : "--", lps2Ok ? "ok" : "--");
    std::printf("=================================\n\n");
}

void DrawSkyGround(float pulse) {
    const Rgb skyTop = LerpRgb({0.05f, 0.08f, 0.22f}, {0.10f, 0.18f, 0.40f}, pulse);
    const Rgb skyBot = LerpRgb({0.08f, 0.12f, 0.28f}, {0.14f, 0.22f, 0.38f}, pulse);
    const Rgb ground = {0.10f, 0.14f, 0.10f};

    // Centered coords: y negative is up.
    (void)leon::rhi::Ps2DrawUnlitRect(-320.0f, -224.0f, 320.0f, 40.0f, skyTop.r, skyTop.g, skyTop.b);
    (void)leon::rhi::Ps2DrawUnlitRect(-320.0f, 40.0f, 320.0f, 224.0f, skyBot.r, skyBot.g, skyBot.b);
    (void)leon::rhi::Ps2DrawUnlitRect(-320.0f, 120.0f, 320.0f, 224.0f, ground.r, ground.g, ground.b);
}

void DrawHud(EDemoMode mode, unsigned frame) {
    // Mode ticks (left) + frame pulse bar (bottom).
    const float modeW = 18.0f;
    const float gap = 6.0f;
    const float baseX = -300.0f;
    const float y0 = -210.0f;
    const float y1 = -190.0f;
    for (int i = 0; i < static_cast<int>(EDemoMode::Count); ++i) {
        const float x0 = baseX + static_cast<float>(i) * (modeW + gap);
        const float x1 = x0 + modeW;
        const bool on = static_cast<int>(mode) == i;
        const float v = on ? 0.95f : 0.25f;
        (void)leon::rhi::Ps2DrawUnlitRect(x0, y0, x1, y1, v, on ? 0.75f : 0.25f, on ? 0.20f : 0.30f);
    }

    const float barT = Wave01(frame, 90u);
    (void)leon::rhi::Ps2DrawUnlitRect(-300.0f, 200.0f, -300.0f + 600.0f * barT, 212.0f, 0.2f, 0.85f,
                                      0.55f);
}

void DrawShowcase(unsigned frame) {
    const float pulse = Wave01(frame, 120u);
    DrawSkyGround(pulse);

    const unsigned spin = (frame * 2u) & 255u;
    (void)leon::rhi::Ps2DrawUnlitTriangleEx(0.0f, -10.0f, 90.0f, spin, 1.0f, 0.82f, 0.18f);

    for (int i = 0; i < 3; ++i) {
        const unsigned orbit = (frame * 3u + static_cast<unsigned>(i) * 85u) & 255u;
        const float radius = 150.0f;
        const float ox = leon::rhi::Ps2Cos256(orbit) * radius;
        const float oy = leon::rhi::Ps2Sin256(orbit) * radius;
        const Rgb rgb = HueRgb(orbit + static_cast<unsigned>(i) * 40u);
        (void)leon::rhi::Ps2DrawUnlitTriangleEx(ox, oy, 28.0f, (spin + orbit) & 255u, rgb.r, rgb.g,
                                                rgb.b);
    }

    // Cook-path marker (LPS2 validated at boot): small cyan tri.
    (void)leon::rhi::Ps2DrawUnlitTriangleEx(-240.0f, 160.0f, 22.0f, frame & 255u, 0.2f, 0.9f, 0.95f);
}

void DrawPadPilot(unsigned frame, PilotState& pilot) {
    DrawSkyGround(0.35f);

    const float speed = 3.5f;
    if (leon::IsPadButtonPressed(leon::EPadButton::DpadLeft)) {
        pilot.x -= speed;
    }
    if (leon::IsPadButtonPressed(leon::EPadButton::DpadRight)) {
        pilot.x += speed;
    }
    if (leon::IsPadButtonPressed(leon::EPadButton::DpadUp)) {
        pilot.y -= speed;
    }
    if (leon::IsPadButtonPressed(leon::EPadButton::DpadDown)) {
        pilot.y += speed;
    }
    if (leon::IsPadButtonPressed(leon::EPadButton::L1)) {
        pilot.angle256 = (pilot.angle256 + 255u) & 255u;
    }
    if (leon::IsPadButtonPressed(leon::EPadButton::R1)) {
        pilot.angle256 = (pilot.angle256 + 1u) & 255u;
    }
    if (leon::IsPadButtonPressed(leon::EPadButton::L2)) {
        pilot.size = pilot.size > 30.0f ? pilot.size - 1.0f : 30.0f;
    }
    if (leon::IsPadButtonPressed(leon::EPadButton::R2)) {
        pilot.size = pilot.size < 140.0f ? pilot.size + 1.0f : 140.0f;
    }

    // Soft clamp to visible area.
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

    const Rgb rgb = HueRgb((frame + pilot.angle256) & 255u);
    (void)leon::rhi::Ps2DrawUnlitTriangleEx(pilot.x, pilot.y, pilot.size, pilot.angle256, rgb.r,
                                            rgb.g, rgb.b);

    // Crosshair at origin.
    (void)leon::rhi::Ps2DrawUnlitRect(-8.0f, -1.0f, 8.0f, 1.0f, 0.7f, 0.7f, 0.8f);
    (void)leon::rhi::Ps2DrawUnlitRect(-1.0f, -8.0f, 1.0f, 8.0f, 0.7f, 0.7f, 0.8f);
}

void DrawStressGrid(unsigned frame) {
    leon::rhi::Ps2ClearColor(0.04f, 0.05f, 0.08f);
    constexpr int kCols = 6;
    constexpr int kRows = 4;
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            const float x = -220.0f + static_cast<float>(col) * 88.0f;
            const float y = -140.0f + static_cast<float>(row) * 90.0f;
            const unsigned idx = static_cast<unsigned>(row * kCols + col);
            const unsigned ang = (frame * 3u + idx * 13u) & 255u;
            const Rgb rgb = HueRgb(ang + idx * 7u);
            (void)leon::rhi::Ps2DrawUnlitTriangleEx(x, y, 26.0f, ang, rgb.r, rgb.g, rgb.b);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    leon::Window window;
    if (!window.Create(640, 448, "Leon Ps2Lab")) {
        std::printf("Ps2Lab: Window::Create failed\n");
        return 1;
    }

    for (int i = 0; i < 2; ++i) {
        leon::rhi::Ps2ClearColor(0.08f, 0.12f, 0.28f);
        (void)leon::rhi::Ps2DrawUnlitTriangle();
        window.SwapBuffers();
    }

    const bool padOk = leon::InitializePs2Pad();
    const bool lps2Ok = leon::rhi::Ps2DrawCookedMesh(leon::ps2lab::kEmbeddedTriangleLps2,
                                                     leon::ps2lab::kEmbeddedTriangleLps2Size);
    PrintBanner(padOk, lps2Ok);

    EDemoMode mode = EDemoMode::Showcase;
    PilotState pilot{};
    unsigned frame = 0;
    bool prevCross = false;
    bool prevCircle = false;
    bool prevSquare = false;
    bool prevTriangle = false;

    for (;;) {
        window.PollEvents();
        if (leon::IsPadButtonPressed(leon::EPadButton::Start)) {
            break;
        }

        const bool cross = leon::IsPadButtonPressed(leon::EPadButton::Cross);
        const bool circle = leon::IsPadButtonPressed(leon::EPadButton::Circle);
        const bool square = leon::IsPadButtonPressed(leon::EPadButton::Square);
        const bool triangle = leon::IsPadButtonPressed(leon::EPadButton::Triangle);

        if (cross && !prevCross) {
            mode = EDemoMode::Showcase;
            std::printf("mode -> %s\n", ModeName(mode));
        }
        if (circle && !prevCircle) {
            mode = EDemoMode::PadPilot;
            std::printf("mode -> %s\n", ModeName(mode));
        }
        if (square && !prevSquare) {
            mode = EDemoMode::StressGrid;
            std::printf("mode -> %s\n", ModeName(mode));
        }
        if (triangle && !prevTriangle) {
            mode = EDemoMode::ClearOnly;
            std::printf("mode -> %s\n", ModeName(mode));
        }

        switch (mode) {
        case EDemoMode::Showcase:
            leon::rhi::Ps2ClearColor(0.02f, 0.03f, 0.06f);
            DrawShowcase(frame);
            DrawHud(mode, frame);
            break;
        case EDemoMode::PadPilot:
            leon::rhi::Ps2ClearColor(0.02f, 0.03f, 0.06f);
            DrawPadPilot(frame, pilot);
            DrawHud(mode, frame);
            break;
        case EDemoMode::StressGrid:
            DrawStressGrid(frame);
            DrawHud(mode, frame);
            break;
        case EDemoMode::ClearOnly: {
            const float w = Wave01(frame, 100u);
            leon::rhi::Ps2ClearColor(0.05f + 0.25f * w, 0.08f + 0.10f * (1.0f - w),
                                     0.20f + 0.40f * w);
            DrawHud(mode, frame);
            break;
        }
        default:
            break;
        }

        window.SwapBuffers();

        prevCross = cross;
        prevCircle = circle;
        prevSquare = square;
        prevTriangle = triangle;
        ++frame;
    }

    std::printf("Ps2Lab: quit after %u frames\n", frame);
    window.Destroy();
    return 0;
}
