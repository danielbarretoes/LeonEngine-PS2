#include "PS2GSContext.h"

#include "PS2RHI.h"

#include <cctype>

#include <dma.h>
#include <draw2d.h>
#include <draw_tests.h>
#include <timer.h>

namespace {

constexpr float Cell = 2.0f;
constexpr float GlyphAdvanceCells = 6.0f; // 5 columns + 1 spacing

[[nodiscard]] const unsigned char* GlyphRows(char Ch) {
    switch (Ch) {
    case ' ': {
        static const unsigned char R[7] = {0, 0, 0, 0, 0, 0, 0};
        return R;
    }
    case '.': {
        static const unsigned char R[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
        return R;
    }
    case '0': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
        return R;
    }
    case '1': {
        static const unsigned char R[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
        return R;
    }
    case '2': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
        return R;
    }
    case '3': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E};
        return R;
    }
    case '4': {
        static const unsigned char R[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
        return R;
    }
    case '5': {
        static const unsigned char R[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
        return R;
    }
    case '6': {
        static const unsigned char R[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
        return R;
    }
    case '7': {
        static const unsigned char R[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
        return R;
    }
    case '8': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
        return R;
    }
    case '9': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
        return R;
    }
    case 'F': {
        static const unsigned char R[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
        return R;
    }
    case 'P': {
        static const unsigned char R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
        return R;
    }
    case 'S': {
        static const unsigned char R[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
        return R;
    }
    case 'M': {
        static const unsigned char R[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
        return R;
    }
    case 'm': {
        static const unsigned char R[7] = {0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15};
        return R;
    }
    case 's': {
        static const unsigned char R[7] = {0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E};
        return R;
    }
    // Clip-debug HUD letters (Draw3D counters).
    case 'C': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
        return R;
    }
    case 'D': {
        static const unsigned char R[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
        return R;
    }
    case 'E': {
        static const unsigned char R[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
        return R;
    }
    case 'W': {
        static const unsigned char R[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
        return R;
    }
    case 'N': {
        static const unsigned char R[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
        return R;
    }
    case 'X': {
        static const unsigned char R[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
        return R;
    }
    case 'Y': {
        static const unsigned char R[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
        return R;
    }
    case 'I': {
        static const unsigned char R[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
        return R;
    }
    case 'T': {
        static const unsigned char R[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
        return R;
    }
    // Engine stats HUD letters (RAM / VRAM / RES / MB).
    case 'R': {
        static const unsigned char R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
        return R;
    }
    case 'A': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        return R;
    }
    case 'B': {
        static const unsigned char R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
        return R;
    }
    case 'K': {
        static const unsigned char R[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
        return R;
    }
    case 'V': {
        static const unsigned char R[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
        return R;
    }
    // Remaining uppercase + ':' so any HUD label renders.
    case 'G': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
        return R;
    }
    case 'H': {
        static const unsigned char R[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        return R;
    }
    case 'J': {
        static const unsigned char R[7] = {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C};
        return R;
    }
    case 'L': {
        static const unsigned char R[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
        return R;
    }
    case 'O': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        return R;
    }
    case 'Q': {
        static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
        return R;
    }
    case 'U': {
        static const unsigned char R[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        return R;
    }
    case 'Z': {
        static const unsigned char R[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
        return R;
    }
    case ':': {
        static const unsigned char R[7] = {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
        return R;
    }
    case '-': {
        static const unsigned char R[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
        return R;
    }
    case '/': {
        static const unsigned char R[7] = {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
        return R;
    }
    default:
        return nullptr;
    }
}


void FillColor(color_t& Out, float InR, float G, float B) {
    Out.r = static_cast<unsigned char>(static_cast<int>(InR * 255.0f) & 0xFF);
    Out.g = static_cast<unsigned char>(static_cast<int>(G * 255.0f) & 0xFF);
    Out.b = static_cast<unsigned char>(static_cast<int>(B * 255.0f) & 0xFF);
    Out.a = 0x80;
    Out.q = 1.0f;
}

void FlushRects(Leon::PS2::FPS2GSContext& Gs, qword_t* End) {
    if (End <= Gs.Packet->data) {
        return;
    }
    dma_channel_send_normal(DMA_CHANNEL_GIF, Gs.Packet->data, End - Gs.Packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
}

/// Append one filled rect; flush when the shared packet is getting full.
qword_t* AppendRect(Leon::PS2::FPS2GSContext& Gs, qword_t* Q, float X0, float Y0, float X1, float Y1,
                    const color_t& Color) {
    // ~8 qwords per rect + finish headroom.
    constexpr int QwordsPerRect = 10;
    constexpr int PacketBudget = 2000; // matches enlarged GIF packet
    if ((Q - Gs.Packet->data) + QwordsPerRect >= PacketBudget) {
        Q = draw_enable_tests(Q, 0, &Gs.Z);
        Q = draw_finish(Q);
        FlushRects(Gs, Q);
        Q = Gs.Packet->data;
        Q = draw_disable_tests(Q, 0, &Gs.Z);
    }

    rect_t Rect{};
    Rect.color = Color;
    Rect.v0.x = X0;
    Rect.v0.y = Y0;
    Rect.v0.z = 0;
    Rect.v1.x = X1;
    Rect.v1.y = Y1;
    Rect.v1.z = 0;
    return draw_rect_filled(Q, 0, &Rect);
}

qword_t* AppendGlyphRuns(Leon::PS2::FPS2GSContext& Gs, qword_t* Q, float X, float Y, char Ch, float InCell,
                         const color_t& Color) {
    const unsigned char* Rows = GlyphRows(Ch);
    if (Rows == nullptr) {
        return Q;
    }
    for (int Row = 0; Row < 7; ++Row) {
        const unsigned char Bits = Rows[Row];
        int Col = 0;
        while (Col < 5) {
            while (Col < 5 && (Bits & static_cast<unsigned char>(0x10 >> Col)) == 0) {
                ++Col;
            }
            if (Col >= 5) {
                break;
            }
            const int RunStart = Col;
            while (Col < 5 && (Bits & static_cast<unsigned char>(0x10 >> Col)) != 0) {
                ++Col;
            }
            const float X0 = X + static_cast<float>(RunStart) * InCell;
            const float X1 = X + static_cast<float>(Col) * InCell;
            const float Y0 = Y + static_cast<float>(Row) * InCell;
            Q = AppendRect(Gs, Q, X0, Y0, X1, Y0 + InCell, Color);
        }
    }
    return Q;
}


} // namespace


void FPS2RHI::DrawDebugText(float X, float Y, const char* Text, float InR, float G, float B,
                         float Scale) {
    if (Text == nullptr) {
        return;
    }
    auto& Gs = Leon::PS2::GetGSContext();
    if (!Gs.bReady || Gs.Packet == nullptr) {
        return;
    }

    color_t Color{};
    FillColor(Color, InR, G, B);
    const float LocalCell = Cell * (Scale > 0.0f ? Scale : 1.0f);

    qword_t* Q = Gs.Packet->data;
    // Overlay: disable z so 3D near-plane wallpaper cannot cover FPS text.
    Q = draw_disable_tests(Q, 0, &Gs.Z);
    float Cx = X;
    for (const char* P = Text; *P != '\0'; ++P) {
        char Ch = *P;
        if (Ch >= 'a' && Ch <= 'z' && Ch != 'm' && Ch != 's') {
            Ch = static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
        }
        Q = AppendGlyphRuns(Gs, Q, Cx, Y, Ch, LocalCell, Color);
        Cx += GlyphAdvanceCells * LocalCell;
    }
    Q = draw_enable_tests(Q, 0, &Gs.Z);
    Q = draw_finish(Q);
    FlushRects(Gs, Q);
}

