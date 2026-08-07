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

void LoadPadModules() {
    SifInitRpc(0);
    // rom0 modules need a working BIOS in PCSX2.
    if (SifLoadModule("rom0:SIO2MAN", 0, nullptr) < 0) {
        std::printf("InputPad: SIO2MAN load failed\n");
    }
    if (SifLoadModule("rom0:PADMAN", 0, nullptr) < 0) {
        std::printf("InputPad: PADMAN load failed\n");
    }
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
    return true;
#else
    return false;
#endif
}

bool IsPadButtonPressed(EPadButton button) {
#if defined(LEON_PLATFORM_PS2)
    if (!g_padReady) {
        return false;
    }
    const int state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        return false;
    }
    padButtonStatus buttons{};
    if (padRead(0, 0, &buttons) == 0) {
        return false;
    }
    // libpad reports pressed bits inverted (0 = pressed).
    const unsigned short btns = static_cast<unsigned short>(buttons.btns ^ 0xFFFF);
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

} // namespace leon
