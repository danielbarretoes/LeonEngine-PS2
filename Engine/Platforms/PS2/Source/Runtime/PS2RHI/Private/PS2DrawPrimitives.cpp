#include "PS2GSContext.h"

#include "PS2RHI.h"
#include "HAL/PlatformMath.h"

#include <cstdint>
#include <cstring>

#include <dma.h>
#include <draw2d.h>
#include <draw_blending.h>
#include <draw_tests.h>

namespace {

#pragma pack(push, 1)
struct Ps2MeshHeader {
    char magic[4]; // LPS2
    std::uint32_t version;
    std::uint32_t vertexCount;
    std::uint32_t indexCount;
};
#pragma pack(pop)


void FillVertex(vertex_t& out, float x, float y) {
    out.x = x;
    out.y = y;
    out.z = 0;
}

void FillColor(color_t& out, float r, float g, float b) {
    out.r = static_cast<unsigned char>(static_cast<int>(r * 255.0f) & 0xFF);
    out.g = static_cast<unsigned char>(static_cast<int>(g * 255.0f) & 0xFF);
    out.b = static_cast<unsigned char>(static_cast<int>(b * 255.0f) & 0xFF);
    out.a = 0x80;
    out.q = 1.0f;
}

[[nodiscard]] bool SubmitPacket(Leon::PS2::FPS2GSContext& gs, qword_t* end) {
    if (gs.packet == nullptr || end <= gs.packet->data) {
        return false;
    }
    dma_channel_send_normal(DMA_CHANNEL_GIF, gs.packet->data, end - gs.packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
    return true;
}

[[nodiscard]] bool IsDisplayReady(const Leon::PS2::FPS2GSContext& gs) {
    return gs.ready && gs.packet != nullptr && gs.frame.width > 0;
}

void RotatePoint(float centerX, float centerY, float lx, float ly, float cosA, float sinA,
                 float& outX, float& outY) {
    outX = centerX + lx * cosA - ly * sinA;
    outY = centerY + lx * sinA + ly * cosA;
}


} // namespace


bool FPS2RHI::DrawUnlitTriangleAt(float centerX, float centerY, float size, unsigned angle256, float r,
                            float g, float b) {
    auto& gs = Leon::PS2::GetGSContext();
    if (!IsDisplayReady(gs) || size <= 0.0f) {
        return false;
    }

    // draw_triangle_filled adds +2048; with XYOFFSET at (2048-w/2, 2048-h/2),
    // drawable space is centered on (0,0).
    const float cosA = FPlatformMath::Cos256(angle256);
    const float sinA = FPlatformMath::Sin256(angle256);

    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    RotatePoint(centerX, centerY, 0.0f, -size, cosA, sinA, x0, y0);
    RotatePoint(centerX, centerY, -size * 0.9f, size * 0.75f, cosA, sinA, x1, y1);
    RotatePoint(centerX, centerY, size * 0.9f, size * 0.75f, cosA, sinA, x2, y2);

    triangle_t tri{};
    FillColor(tri.color, r, g, b);
    FillVertex(tri.v0, x0, y0);
    FillVertex(tri.v1, x1, y1);
    FillVertex(tri.v2, x2, y2);

    qword_t* q = gs.packet->data;
    q = draw_triangle_filled(q, 0, &tri);
    q = draw_finish(q);
    return SubmitPacket(gs, q);
}

bool FPS2RHI::DrawUnlitRect(float x0, float y0, float x1, float y1, float r, float g, float b) {
    auto& gs = Leon::PS2::GetGSContext();
    if (!IsDisplayReady(gs)) {
        return false;
    }
    if (x1 < x0) {
        const float t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y1 < y0) {
        const float t = y0;
        y0 = y1;
        y1 = t;
    }

    rect_t rect{};
    FillColor(rect.color, r, g, b);
    FillVertex(rect.v0, x0, y0);
    FillVertex(rect.v1, x1, y1);

    qword_t* q = gs.packet->data;
    // Overlay: ignore z so terrain cannot cover HUD / 2D chrome.
    q = draw_disable_tests(q, 0, &gs.z);
    q = draw_rect_filled(q, 0, &rect);
    q = draw_enable_tests(q, 0, &gs.z);
    q = draw_finish(q);
    return SubmitPacket(gs, q);
}

bool FPS2RHI::DrawUnlitRectAlpha(float x0, float y0, float x1, float y1, float r, float g, float b,
                           float alpha) {
    auto& gs = Leon::PS2::GetGSContext();
    if (!IsDisplayReady(gs)) {
        return false;
    }
    if (x1 < x0) {
        const float t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y1 < y0) {
        const float t = y0;
        y0 = y1;
        y1 = t;
    }
    alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);

    rect_t rect{};
    FillColor(rect.color, r, g, b);
    // GS alpha: 0x80 = 1.0. Keep >= 1 so ATEST (A != 0) never discards the sprite.
    const int a = static_cast<int>(alpha * 128.0f + 0.5f);
    rect.color.a = static_cast<unsigned char>(a < 1 ? 1 : a);
    FillVertex(rect.v0, x0, y0);
    FillVertex(rect.v1, x1, y1);

    // (Cs - Cd) * As + Cd. libdraw bakes PRIM.ABE from a global flag inside draw_rect_filled,
    // so enable it only around this sprite (everything else stays opaque).
    blend_t blend{};
    blend.color1 = BLEND_COLOR_SOURCE;
    blend.color2 = BLEND_COLOR_DEST;
    blend.alpha = BLEND_ALPHA_SOURCE;
    blend.color3 = BLEND_COLOR_DEST;
    blend.fixed_alpha = 0x80;

    qword_t* q = gs.packet->data;
    q = draw_disable_tests(q, 0, &gs.z);
    q = draw_alpha_blending(q, 0, &blend);
    draw_enable_blending();
    q = draw_rect_filled(q, 0, &rect);
    draw_disable_blending();
    q = draw_enable_tests(q, 0, &gs.z);
    q = draw_finish(q);
    return SubmitPacket(gs, q);
}

bool FPS2RHI::DrawUnlitTriangle() {
    auto& gs = Leon::PS2::GetGSContext();
    if (!IsDisplayReady(gs)) {
        return false;
    }
    const float size = static_cast<float>(gs.frame.height) * 0.28f;
    return FPS2RHI::DrawUnlitTriangleAt(0.0f, 0.0f, size, 0, 1.0f, 0.784f, 0.125f);
}

bool FPS2RHI::DrawCookedMesh(const void* data, unsigned size) {
    if (data == nullptr || size < sizeof(Ps2MeshHeader)) {
        return false;
    }
    Ps2MeshHeader header{};
    std::memcpy(&header, data, sizeof(header));
    if (std::memcmp(header.magic, "LPS2", 4) != 0 || header.version != 1) {
        return false;
    }
    if (header.vertexCount == 0) {
        return false;
    }
    // Full LPS2 vertex upload is not wired yet; keep a visible GS result for cook smoke.
    return FPS2RHI::DrawUnlitTriangle();
}

