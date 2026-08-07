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
    std::printf("\n========== Leon Ps2Cube (3D) ==========\n");
    std::printf("  Perspective unlit box + z-buffer\n");
    std::printf("  D-Pad L/R   yaw\n");
    std::printf("  D-Pad U/D   pitch\n");
    std::printf("  L1 / R1     orbit speed -/+\n");
    std::printf("  Cross       reset rotation\n");
    std::printf("  Start       quit\n");
    std::printf("Pad=%s\n", padOk ? "ok" : "--");
    std::printf("=======================================\n\n");
}

[[nodiscard]] bool Edge(bool down, bool& prev) {
    const bool pressed = down && !prev;
    prev = down;
    return pressed;
}

} // namespace

int RunPs2CubeDemo(Window& window) {
    for (int i = 0; i < 2; ++i) {
        rhi::Ps2ClearColor(0.06f, 0.08f, 0.14f);
        window.SwapBuffers();
    }

    const bool padOk = InitializePad();
    PrintBanner(padOk);

    unsigned yaw = 20;
    unsigned pitch = 18;
    int autoSpin = 1;
    unsigned frame = 0;
    bool prevCross = false;

    for (;;) {
        window.PollEvents();
        if (IsPadButtonPressed(EPadButton::Start)) {
            break;
        }

        if (Edge(IsPadButtonPressed(EPadButton::Cross), prevCross)) {
            yaw = 20;
            pitch = 18;
            autoSpin = 1;
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
        if (IsPadButtonPressed(EPadButton::L1)) {
            autoSpin = autoSpin > 0 ? autoSpin - 1 : 0;
        }
        if (IsPadButtonPressed(EPadButton::R1)) {
            autoSpin = autoSpin < 4 ? autoSpin + 1 : 4;
        }

        if (autoSpin > 0) {
            yaw = (yaw + static_cast<unsigned>(autoSpin)) & 255u;
            if ((frame & 1u) != 0u) {
                pitch = (pitch + 1u) & 255u;
            }
        }

        rhi::Ps2ClearColor(0.05f, 0.07f, 0.12f);

        // Ground slab (flat box) under the hero cube.
        (void)rhi::Ps2DrawUnlitBox(0.0f, -8.0f, 0.0f, 18.0f, 0, 0, 0.12f, 0.22f, 0.16f);

        const Rgb rgb = HueRgb(yaw + frame / 2u);
        (void)rhi::Ps2DrawUnlitBox(0.0f, 0.0f, 0.0f, 6.0f, yaw, pitch, rgb.r, rgb.g, rgb.b);

        // Accent satellite cubes.
        const unsigned orbit = (frame * 2u) & 255u;
        const float ox = rhi::Ps2Cos256(orbit) * 16.0f;
        const float oz = rhi::Ps2Sin256(orbit) * 16.0f;
        (void)rhi::Ps2DrawUnlitBox(ox, 2.0f, oz, 2.2f, (yaw + 40u) & 255u, pitch, 0.95f, 0.55f,
                                   0.20f);
        (void)rhi::Ps2DrawUnlitBox(-ox * 0.7f, 4.0f, -oz * 0.7f, 1.6f, (255u - yaw) & 255u,
                                   (pitch + 30u) & 255u, 0.35f, 0.65f, 1.0f);

        window.SwapBuffers();
        ++frame;
    }

    std::printf("Ps2Cube: quit after %u frames\n", frame);
    return 0;
}

} // namespace leon::ps2cube
