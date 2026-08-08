#include "Ps2GsContext.h"

#include <leon/rhi/Ps2RHI.h>

#include <cctype>

#if defined(LEON_PLATFORM_PS2)
#include <dma.h>
#include <draw2d.h>
#include <draw_tests.h>
#include <timer.h>
#endif

namespace leon::rhi {
namespace {

constexpr float kCell = 2.0f;
constexpr float kGlyphAdvance = 12.0f;

[[nodiscard]] const unsigned char* GlyphRows(char ch) {
    switch (ch) {
    case ' ': {
        static const unsigned char r[7] = {0, 0, 0, 0, 0, 0, 0};
        return r;
    }
    case '.': {
        static const unsigned char r[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
        return r;
    }
    case '0': {
        static const unsigned char r[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
        return r;
    }
    case '1': {
        static const unsigned char r[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
        return r;
    }
    case '2': {
        static const unsigned char r[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
        return r;
    }
    case '3': {
        static const unsigned char r[7] = {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E};
        return r;
    }
    case '4': {
        static const unsigned char r[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
        return r;
    }
    case '5': {
        static const unsigned char r[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
        return r;
    }
    case '6': {
        static const unsigned char r[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
        return r;
    }
    case '7': {
        static const unsigned char r[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
        return r;
    }
    case '8': {
        static const unsigned char r[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
        return r;
    }
    case '9': {
        static const unsigned char r[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
        return r;
    }
    case 'F': {
        static const unsigned char r[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
        return r;
    }
    case 'P': {
        static const unsigned char r[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
        return r;
    }
    case 'S': {
        static const unsigned char r[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
        return r;
    }
    case 'M': {
        static const unsigned char r[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
        return r;
    }
    case 'm': {
        static const unsigned char r[7] = {0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15};
        return r;
    }
    case 's': {
        static const unsigned char r[7] = {0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E};
        return r;
    }
    default:
        return nullptr;
    }
}

#if defined(LEON_PLATFORM_PS2)

void FillColor(color_t& out, float r, float g, float b) {
    out.r = static_cast<unsigned char>(static_cast<int>(r * 255.0f) & 0xFF);
    out.g = static_cast<unsigned char>(static_cast<int>(g * 255.0f) & 0xFF);
    out.b = static_cast<unsigned char>(static_cast<int>(b * 255.0f) & 0xFF);
    out.a = 0x80;
    out.q = 1.0f;
}

void FlushRects(ps2::GsContext& gs, qword_t* end) {
    if (end <= gs.packet->data) {
        return;
    }
    dma_channel_send_normal(DMA_CHANNEL_GIF, gs.packet->data, end - gs.packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
}

/// Append one filled rect; flush when the shared packet is getting full.
qword_t* AppendRect(ps2::GsContext& gs, qword_t* q, float x0, float y0, float x1, float y1,
                    const color_t& color) {
    // ~8 qwords per rect + finish headroom.
    constexpr int kQwordsPerRect = 10;
    constexpr int kPacketBudget = 2000; // matches enlarged GIF packet
    if ((q - gs.packet->data) + kQwordsPerRect >= kPacketBudget) {
        q = draw_enable_tests(q, 0, &gs.z);
        q = draw_finish(q);
        FlushRects(gs, q);
        q = gs.packet->data;
        q = draw_disable_tests(q, 0, &gs.z);
    }

    rect_t rect{};
    rect.color = color;
    rect.v0.x = x0;
    rect.v0.y = y0;
    rect.v0.z = 0;
    rect.v1.x = x1;
    rect.v1.y = y1;
    rect.v1.z = 0;
    return draw_rect_filled(q, 0, &rect);
}

qword_t* AppendGlyphRuns(ps2::GsContext& gs, qword_t* q, float x, float y, char ch,
                         const color_t& color) {
    const unsigned char* rows = GlyphRows(ch);
    if (rows == nullptr) {
        return q;
    }
    for (int row = 0; row < 7; ++row) {
        const unsigned char bits = rows[row];
        int col = 0;
        while (col < 5) {
            while (col < 5 && (bits & static_cast<unsigned char>(0x10 >> col)) == 0) {
                ++col;
            }
            if (col >= 5) {
                break;
            }
            const int runStart = col;
            while (col < 5 && (bits & static_cast<unsigned char>(0x10 >> col)) != 0) {
                ++col;
            }
            const float x0 = x + static_cast<float>(runStart) * kCell;
            const float x1 = x + static_cast<float>(col) * kCell;
            const float y0 = y + static_cast<float>(row) * kCell;
            q = AppendRect(gs, q, x0, y0, x1, y0 + kCell, color);
        }
    }
    return q;
}

#endif

} // namespace

std::uint64_t Ps2GetSystemTimeUs() {
#if defined(LEON_PLATFORM_PS2)
    // BUSCLK 147.456 MHz → us = ticks * 125 / 18432
    const std::uint64_t ticks = GetTimerSystemTime();
    return (ticks * 125ULL) / 18432ULL;
#else
    return 0;
#endif
}

void Ps2DrawDebugHudText(float x, float y, const char* text, float r, float g, float b) {
#if defined(LEON_PLATFORM_PS2)
    if (text == nullptr) {
        return;
    }
    auto& gs = ps2::GetGsContext();
    if (!gs.ready || gs.packet == nullptr) {
        return;
    }

    color_t color{};
    FillColor(color, r, g, b);

    qword_t* q = gs.packet->data;
    // Overlay: disable z so 3D near-plane wallpaper cannot cover FPS text.
    q = draw_disable_tests(q, 0, &gs.z);
    float cx = x;
    for (const char* p = text; *p != '\0'; ++p) {
        char ch = *p;
        if (ch >= 'a' && ch <= 'z' && ch != 'm' && ch != 's') {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        q = AppendGlyphRuns(gs, q, cx, y, ch, color);
        cx += kGlyphAdvance;
    }
    q = draw_enable_tests(q, 0, &gs.z);
    q = draw_finish(q);
    FlushRects(gs, q);
#else
    (void)x;
    (void)y;
    (void)text;
    (void)r;
    (void)g;
    (void)b;
#endif
}

} // namespace leon::rhi
