#include "Ps2CubeDemo.h"

#include <leon/core/InputPad.h>
#include <leon/core/Window.h>
#include <leon/rhi/Ps2RHI.h>

#include <cstdio>

namespace leon::ps2cube {
namespace {

struct Rgb {
    float r;
    float g;
    float b;
};

[[nodiscard]] Rgb HueRgb(unsigned hue256) {
    const unsigned h = hue256 % 192u;
    const unsigned seg = h / 64u;
    const float t = static_cast<float>(h % 64u) / 64.0f;
    switch (seg) {
    case 0:
        return {1.0f, 0.35f + 0.45f * t, 0.15f};
    case 1:
        return {1.0f - 0.55f * t, 0.85f, 0.20f};
    default:
        return {0.25f, 0.55f + 0.25f * (1.0f - t), 0.95f};
    }
}

void PrintBanner(bool padOk) {
    std::printf("\n========== Leon Ps2Cube (3D scene) ==========\n");
    std::printf("  M_Ground / M_Cube + T_*_D | DirectionalLight\n");
    std::printf("  HUD: FPS + work ms\n");
    std::printf("  D-Pad        object yaw/pitch\n");
    std::printf("  L1/R1        orbit speed\n");
    std::printf("  L2/R2        sun yaw\n");
    std::printf("  Square/Tri   sun Intensity\n");
    std::printf("  Cross        reset | Start quit\n");
    std::printf("Pad=%s\n", padOk ? "ok" : "--");
    std::printf("============================================\n\n");
}

[[nodiscard]] bool Edge(bool down, bool& prev) {
    const bool pressed = down && !prev;
    prev = down;
    return pressed;
}

void FormatHud(char* out, unsigned outSize, int fps, float workMs) {
    int tenths = static_cast<int>(workMs * 10.0f + 0.5f);
    if (tenths < 0) {
        tenths = 0;
    }
    std::snprintf(out, outSize, "FPS %d  %d.%d ms", fps, tenths / 10, tenths % 10);
}

[[nodiscard]] float Clamp(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

} // namespace

int RunPs2CubeDemo(Window& window) {
    for (int i = 0; i < 2; ++i) {
        rhi::Ps2ClearColor(0.06f, 0.08f, 0.14f);
        window.SwapBuffers();
    }

    const bool padOk = InitializePad();
    PrintBanner(padOk);

    // Content names: T_Checker_D / T_Grid_D (procedural embed).
    const rhi::Ps2Texture texChecker = rhi::Ps2Texture::CreateChecker(64);
    const rhi::Ps2Texture texGrid = rhi::Ps2Texture::CreateGrid(64);

    rhi::Ps2Material mGround{};
    mGround.BaseColorR = 0.85f;
    mGround.BaseColorG = 0.85f;
    mGround.BaseColorB = 0.80f;
    mGround.BaseColorMap = texGrid.Valid() ? &texGrid : nullptr;
    mGround.ShadingModel = rhi::EShadingModel::DefaultLit;

    rhi::Ps2Material mCube{};
    mCube.BaseColorR = 1.0f;
    mCube.BaseColorG = 1.0f;
    mCube.BaseColorB = 1.0f;
    mCube.BaseColorMap = texChecker.Valid() ? &texChecker : nullptr;
    mCube.ShadingModel = rhi::EShadingModel::DefaultLit;
    mCube.UseFaceAlbedo = !texChecker.Valid();

    rhi::Ps2Material mAccent{};
    mAccent.ShadingModel = rhi::EShadingModel::DefaultLit;

    rhi::Ps2ViewTarget view{};
    rhi::Ps2SetViewTarget(view);

    rhi::DirectionalLight sun{};
    sun.Yaw256 = 12;
    sun.Pitch256 = 80;
    rhi::Ps2SetDirectionalLight(sun);
    rhi::Ps2SetAmbientLightColor(0.18f, 0.20f, 0.26f);

    unsigned yaw = 20;
    unsigned pitch = 18;
    int autoSpin = 1;
    unsigned frame = 0;
    bool prevCross = false;
    bool sunManual = false;

    std::uint64_t prevFrameUs = rhi::Ps2GetSystemTimeUs();
    std::uint64_t hudAccumUs = 0;
    unsigned hudFrames = 0;
    float workSumMs = 0.0f;
    char hudLine[32] = "FPS --";

    for (;;) {
        const std::uint64_t frameStartUs = rhi::Ps2GetSystemTimeUs();

        window.PollEvents();
        if (IsPadButtonPressed(EPadButton::Start)) {
            break;
        }

        if (Edge(IsPadButtonPressed(EPadButton::Cross), prevCross)) {
            yaw = 20;
            pitch = 18;
            autoSpin = 1;
            sunManual = false;
            sun.Intensity = 1.0f;
            sun.Yaw256 = 12;
            sun.Pitch256 = 80;
            std::printf("Ps2Cube: reset\n");
        }
        if (IsPadButtonPressed(EPadButton::DpadLeft)) {
            yaw = (yaw + 255u) & 255u;
            autoSpin = 0;
        }
        if (IsPadButtonPressed(EPadButton::DpadRight)) {
            yaw = (yaw + 1u) & 255u;
            autoSpin = 0;
        }
        if (IsPadButtonPressed(EPadButton::DpadUp)) {
            pitch = (pitch + 255u) & 255u;
            autoSpin = 0;
        }
        if (IsPadButtonPressed(EPadButton::DpadDown)) {
            pitch = (pitch + 1u) & 255u;
            autoSpin = 0;
        }
        if (IsPadButtonPressed(EPadButton::L1) && autoSpin > 0) {
            --autoSpin;
        }
        if (IsPadButtonPressed(EPadButton::R1) && autoSpin < 4) {
            ++autoSpin;
        }
        if (IsPadButtonPressed(EPadButton::L2)) {
            sun.Yaw256 = (sun.Yaw256 + 255u) & 255u;
            sunManual = true;
        }
        if (IsPadButtonPressed(EPadButton::R2)) {
            sun.Yaw256 = (sun.Yaw256 + 1u) & 255u;
            sunManual = true;
        }
        if (IsPadButtonPressed(EPadButton::Square)) {
            sun.Intensity = Clamp(sun.Intensity - 0.02f, 0.15f, 2.0f);
        }
        if (IsPadButtonPressed(EPadButton::Triangle)) {
            sun.Intensity = Clamp(sun.Intensity + 0.02f, 0.15f, 2.0f);
        }

        if (autoSpin > 0) {
            yaw = (yaw + static_cast<unsigned>(autoSpin)) & 255u;
            if ((frame & 1u) != 0u) {
                pitch = (pitch + 1u) & 255u;
            }
        }

        if (!sunManual) {
            sun.Yaw256 = (frame / 2u) & 255u;
        }
        rhi::Ps2SetDirectionalLight(sun);

        // clear → draw → HUD → swap
        rhi::Ps2ClearColor(0.10f, 0.11f, 0.14f);

        rhi::Ps2BindMaterial(mGround);
        (void)rhi::Ps2DrawBox(0.0f, -8.0f, 0.0f, 0, 0, 18.0f);

        rhi::Ps2BindMaterial(mCube);
        (void)rhi::Ps2DrawBox(0.0f, 0.0f, 0.0f, yaw, pitch, 6.0f);

        const unsigned orbit = (frame * 2u) & 255u;
        const float ox = rhi::Ps2Cos256(orbit) * 16.0f;
        const float oz = rhi::Ps2Sin256(orbit) * 16.0f;

        const Rgb accent = HueRgb(yaw + frame / 2u);
        mAccent.BaseColorR = accent.r;
        mAccent.BaseColorG = accent.g;
        mAccent.BaseColorB = accent.b;
        mAccent.BaseColorMap = nullptr;
        rhi::Ps2BindMaterial(mAccent);
        (void)rhi::Ps2DrawBox(ox, 2.0f, oz, (yaw + 40u) & 255u, pitch, 2.2f);

        mAccent.BaseColorR = 0.85f;
        mAccent.BaseColorG = 0.85f;
        mAccent.BaseColorB = 0.95f;
        rhi::Ps2BindMaterial(mAccent);
        (void)rhi::Ps2DrawBox(-ox * 0.7f, 4.0f, -oz * 0.7f, (255u - yaw) & 255u,
                              (pitch + 30u) & 255u, 1.6f);

        const std::uint64_t workEndUs = rhi::Ps2GetSystemTimeUs();
        const float workMs = static_cast<float>(workEndUs - frameStartUs) / 1000.0f;

        hudAccumUs += workEndUs - prevFrameUs;
        prevFrameUs = workEndUs;
        workSumMs += workMs;
        ++hudFrames;
        if (hudAccumUs >= 250000ull && hudFrames > 0) {
            const float secs = static_cast<float>(hudAccumUs) / 1000000.0f;
            const int fps = static_cast<int>(static_cast<float>(hudFrames) / secs + 0.5f);
            FormatHud(hudLine, sizeof(hudLine), fps, workSumMs / static_cast<float>(hudFrames));
            hudAccumUs = 0;
            hudFrames = 0;
            workSumMs = 0.0f;
        }

        (void)rhi::Ps2DrawUnlitRect(-310.0f, -215.0f, -70.0f, -185.0f, 0.05f, 0.06f, 0.08f);
        rhi::Ps2DrawDebugHudText(-300.0f, -210.0f, hudLine, 0.95f, 0.95f, 0.75f);

        window.SwapBuffers();
        ++frame;
    }

    std::printf("Ps2Cube: quit after %u frames\n", frame);
    return 0;
}

} // namespace leon::ps2cube
