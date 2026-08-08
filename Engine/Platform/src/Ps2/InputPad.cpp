#include <leon/core/InputPad.h>

#include <cstdio>

#if defined(LEON_PLATFORM_PS2)
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
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        return;
    }
    if (padRead(0, 0, &g_pad) == 0) {
        return;
    }
    g_padSampleValid = true;
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

} // namespace leon
