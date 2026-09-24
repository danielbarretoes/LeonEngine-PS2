#include <leon/core/InputPad.h>

#include <cstdio>

#if defined(LEON_PLATFORM_PS2)
#include <leon/rhi/Ps2RHI.h>

#include <libpad.h>
#include <loadfile.h>
#include <sifrpc.h>
#endif

namespace leon {
namespace {

#if defined(LEON_PLATFORM_PS2)
char g_padBuf[256] __attribute__((aligned(64)));
bool g_padReady = false;
bool g_analogRequested = false;
padButtonStatus g_pad{};
bool g_padSampleValid = false;
int g_lastPadState = -1;
unsigned short g_lastLoggedBtns = 0;

constexpr float kStickDeadzone = 0.18f;

void LoadPadModules() {
    SifInitRpc(0);
    if (SifLoadModule("rom0:SIO2MAN", 0, nullptr) < 0) {
        std::printf("InputPad: SIO2MAN load failed\n");
    }
    if (SifLoadModule("rom0:PADMAN", 0, nullptr) < 0) {
        std::printf("InputPad: PADMAN load failed\n");
    }
}

[[nodiscard]] float AxisFromByte(unsigned char raw) {
    // DualShock: 0..255, center ~128.
    float v = (static_cast<float>(raw) - 128.0f) / 128.0f;
    if (v < -1.0f) {
        v = -1.0f;
    }
    if (v > 1.0f) {
        v = 1.0f;
    }
    if (v > -kStickDeadzone && v < kStickDeadzone) {
        return 0.0f;
    }
    // Rescale outside deadzone to full range.
    const float sign = v < 0.0f ? -1.0f : 1.0f;
    const float mag = (v < 0.0f ? -v : v) - kStickDeadzone;
    const float span = 1.0f - kStickDeadzone;
    return sign * (mag / span);
}

void TryEnableAnalog() {
    if (g_analogRequested || !g_padReady) {
        return;
    }
    const int state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        return;
    }
    // DualShock analog sticks.
    padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
    g_analogRequested = true;
    std::printf("InputPad: DualShock analog mode requested\n");
}
#endif

} // namespace

bool InitializePad() {
#if defined(LEON_PLATFORM_PS2)
    LoadPadModules();
    padInit(0);
    if (padPortOpen(0, 0, g_padBuf) == 0) {
        g_padReady = false;
        return false;
    }
    g_padReady = true;
    g_analogRequested = false;
    g_padSampleValid = false;
    return true;
#else
    return false;
#endif
}

void PollPad() {
#if defined(LEON_PLATFORM_PS2)
    g_padSampleValid = false;
    if (!g_padReady) {
        return;
    }
    TryEnableAnalog();
    const int state = padGetState(0, 0);
    if (state != g_lastPadState) {
        std::printf("InputPad: port0 state %d\n", state);
        g_lastPadState = state;
    }
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        return;
    }
    if (padRead(0, 0, &g_pad) == 0) {
        return;
    }
    g_padSampleValid = true;
    const unsigned short btns = static_cast<unsigned short>(g_pad.btns ^ 0xFFFF);
    if (btns != g_lastLoggedBtns) {
        std::printf("InputPad: btns 0x%04X\n", btns);
        g_lastLoggedBtns = btns;
    }
#endif
}

bool IsPadButtonPressed(EPadButton button) {
#if defined(LEON_PLATFORM_PS2)
    if (!g_padSampleValid) {
        PollPad();
    }
    if (!g_padSampleValid) {
        return false;
    }
    const unsigned short btns = static_cast<unsigned short>(g_pad.btns ^ 0xFFFF);
    switch (button) {
    case EPadButton::Cross:
        return (btns & PAD_CROSS) != 0;
    case EPadButton::Circle:
        return (btns & PAD_CIRCLE) != 0;
    case EPadButton::Square:
        return (btns & PAD_SQUARE) != 0;
    case EPadButton::Triangle:
        return (btns & PAD_TRIANGLE) != 0;
    case EPadButton::Start:
        return (btns & PAD_START) != 0;
    case EPadButton::Select:
        return (btns & PAD_SELECT) != 0;
    case EPadButton::DpadUp:
        return (btns & PAD_UP) != 0;
    case EPadButton::DpadDown:
        return (btns & PAD_DOWN) != 0;
    case EPadButton::DpadLeft:
        return (btns & PAD_LEFT) != 0;
    case EPadButton::DpadRight:
        return (btns & PAD_RIGHT) != 0;
    case EPadButton::L1:
        return (btns & PAD_L1) != 0;
    case EPadButton::R1:
        return (btns & PAD_R1) != 0;
    case EPadButton::L2:
        return (btns & PAD_L2) != 0;
    case EPadButton::R2:
        return (btns & PAD_R2) != 0;
    default:
        return false;
    }
#else
    (void)button;
    return false;
#endif
}

PadStick GetPadLeftStick() {
#if defined(LEON_PLATFORM_PS2)
    if (!g_padSampleValid) {
        PollPad();
    }
    if (!g_padSampleValid) {
        return {};
    }
    // libpad: ljoy_h / ljoy_v — Y flipped so -1 = stick up.
    return {AxisFromByte(g_pad.ljoy_h), -AxisFromByte(g_pad.ljoy_v)};
#else
    return {};
#endif
}

PadStick GetPadRightStick() {
#if defined(LEON_PLATFORM_PS2)
    if (!g_padSampleValid) {
        PollPad();
    }
    if (!g_padSampleValid) {
        return {};
    }
    return {AxisFromByte(g_pad.rjoy_h), -AxisFromByte(g_pad.rjoy_v)};
#else
    return {};
#endif
}

void DrawPadDebugOverlay(float x, float y, float scale) {
#if defined(LEON_PLATFORM_PS2)
    if (!g_padSampleValid) {
        PollPad();
    }
    const bool live = g_padSampleValid;
    const unsigned short btns = live ? static_cast<unsigned short>(g_pad.btns ^ 0xFFFF) : 0;

    // 50% panel so the scene stays readable underneath.
    constexpr float kPanelAlpha = 0.5f;
    constexpr float kIdleAlpha = 0.5f;
    constexpr float kLitAlpha = 0.9f;

    const auto box = [&](float x0, float y0, float x1, float y1, float r, float g, float b,
                         float a) {
        (void)rhi::Ps2DrawUnlitRectAlpha(x + x0 * scale, y + y0 * scale, x + x1 * scale,
                                         y + y1 * scale, r, g, b, a);
    };
    // Lit colour while held, dim grey otherwise.
    const auto button = [&](unsigned short mask, float x0, float y0, float x1, float y1, float r,
                            float g, float b) {
        if ((btns & mask) != 0) {
            box(x0, y0, x1, y1, r, g, b, kLitAlpha);
        } else {
            box(x0, y0, x1, y1, 0.55f, 0.58f, 0.64f, kIdleAlpha);
        }
    };
    // Raw bytes (no deadzone) so any stick movement is visible; screen y grows down like raw v.
    const auto stick = [&](unsigned char rawH, unsigned char rawV, unsigned short clickMask,
                           float cx, float cy) {
        if ((btns & clickMask) != 0) {
            box(cx - 9.0f, cy - 9.0f, cx + 9.0f, cy + 9.0f, 0.95f, 0.75f, 0.2f, kIdleAlpha);
        } else {
            box(cx - 9.0f, cy - 9.0f, cx + 9.0f, cy + 9.0f, 0.3f, 0.32f, 0.38f, kIdleAlpha);
        }
        float dx = 0.0f;
        float dy = 0.0f;
        if (live) {
            dx = (static_cast<float>(rawH) - 128.0f) / 128.0f * 7.0f;
            dy = (static_cast<float>(rawV) - 128.0f) / 128.0f * 7.0f;
        }
        const bool moved = dx * dx + dy * dy > 1.0f;
        box(cx + dx - 2.0f, cy + dy - 2.0f, cx + dx + 2.0f, cy + dy + 2.0f, moved ? 1.0f : 0.8f,
            moved ? 0.85f : 0.8f, moved ? 0.2f : 0.85f, kLitAlpha);
    };

    // Layout in widget units (150×78), kPadDebugPadding on every side, mirrored around x = 75.
    box(0.0f, 0.0f, 150.0f, 78.0f, 0.02f, 0.03f, 0.05f, kPanelAlpha);

    // Status LED (top centre).
    if (live) {
        box(71.0f, 4.0f, 79.0f, 12.0f, 0.2f, 0.95f, 0.3f, kLitAlpha);
    } else if (g_padReady) {
        box(71.0f, 4.0f, 79.0f, 12.0f, 1.0f, 0.6f, 0.1f, kLitAlpha);
    } else {
        box(71.0f, 4.0f, 79.0f, 12.0f, 0.95f, 0.15f, 0.15f, kLitAlpha);
    }

    constexpr float kWhiteR = 0.95f;
    constexpr float kWhiteG = 0.9f;
    constexpr float kWhiteB = 0.4f;
    button(PAD_L2, 4.0f, 4.0f, 30.0f, 8.0f, kWhiteR, kWhiteG, kWhiteB);
    button(PAD_L1, 4.0f, 10.0f, 30.0f, 14.0f, kWhiteR, kWhiteG, kWhiteB);
    button(PAD_R2, 120.0f, 4.0f, 146.0f, 8.0f, kWhiteR, kWhiteG, kWhiteB);
    button(PAD_R1, 120.0f, 10.0f, 146.0f, 14.0f, kWhiteR, kWhiteG, kWhiteB);

    button(PAD_UP, 13.0f, 20.0f, 21.0f, 28.0f, kWhiteR, kWhiteG, kWhiteB);
    button(PAD_DOWN, 13.0f, 38.0f, 21.0f, 46.0f, kWhiteR, kWhiteG, kWhiteB);
    button(PAD_LEFT, 4.0f, 29.0f, 12.0f, 37.0f, kWhiteR, kWhiteG, kWhiteB);
    button(PAD_RIGHT, 22.0f, 29.0f, 30.0f, 37.0f, kWhiteR, kWhiteG, kWhiteB);

    button(PAD_SELECT, 57.0f, 31.0f, 69.0f, 35.0f, kWhiteR, kWhiteG, kWhiteB);
    button(PAD_START, 81.0f, 31.0f, 93.0f, 35.0f, kWhiteR, kWhiteG, kWhiteB);

    button(PAD_TRIANGLE, 129.0f, 20.0f, 137.0f, 28.0f, 0.2f, 0.9f, 0.5f);
    button(PAD_CROSS, 129.0f, 38.0f, 137.0f, 46.0f, 0.35f, 0.55f, 1.0f);
    button(PAD_SQUARE, 120.0f, 29.0f, 128.0f, 37.0f, 0.95f, 0.45f, 0.85f);
    button(PAD_CIRCLE, 138.0f, 29.0f, 146.0f, 37.0f, 0.95f, 0.3f, 0.3f);

    stick(g_pad.ljoy_h, g_pad.ljoy_v, PAD_L3, 55.0f, 65.0f);
    stick(g_pad.rjoy_h, g_pad.rjoy_v, PAD_R3, 95.0f, 65.0f);
#else
    (void)x;
    (void)y;
    (void)scale;
#endif
}

} // namespace leon
