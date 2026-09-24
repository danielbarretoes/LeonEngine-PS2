#include "Ps2GsContext.h"

#include <leon/rhi/Ps2RHI.h>

#include <cstdint>
#include <cstring>

#if defined(LEON_PLATFORM_PS2)
#include <dma.h>
#include <draw2d.h>
#include <draw_blending.h>
#include <draw_tests.h>
#endif

namespace leon::rhi {
namespace {

#pragma pack(push, 1)
struct Ps2MeshHeader {
    char magic[4]; // LPS2
    std::uint32_t version;
    std::uint32_t vertexCount;
    std::uint32_t indexCount;
};
#pragma pack(pop)

#if defined(LEON_PLATFORM_PS2)

// Quarter-wave sin (0..64 => 0..90°). Avoids libm on EE soft-float.
constexpr float kSinQuarter[65] = {
    0.000000f, 0.024541f, 0.049068f, 0.073565f, 0.098017f, 0.122411f, 0.146730f, 0.170962f,
    0.195090f, 0.219101f, 0.242980f, 0.266713f, 0.290285f, 0.313682f, 0.336890f, 0.359895f,
    0.382683f, 0.405241f, 0.427555f, 0.449611f, 0.471397f, 0.492898f, 0.514103f, 0.534998f,
    0.555570f, 0.575808f, 0.595699f, 0.615232f, 0.634393f, 0.653173f, 0.671559f, 0.689541f,
    0.707107f, 0.724247f, 0.740951f, 0.757209f, 0.773010f, 0.788346f, 0.803208f, 0.817585f,
    0.831470f, 0.844854f, 0.857729f, 0.870087f, 0.881921f, 0.893224f, 0.903989f, 0.914210f,
    0.923880f, 0.932993f, 0.941544f, 0.949528f, 0.956940f, 0.963776f, 0.970031f, 0.975702f,
    0.980785f, 0.985278f, 0.989177f, 0.992480f, 0.995185f, 0.997290f, 0.998795f, 0.999699f,
    1.000000f,
};

[[nodiscard]] float Sin256(unsigned angle256) {
    const unsigned a = angle256 & 255u;
    const unsigned quad = a >> 6;
    const unsigned idx = a & 63u;
    switch (quad) {
    case 0:
        return kSinQuarter[idx];
    case 1:
        return kSinQuarter[64u - idx];
    case 2:
        return -kSinQuarter[idx];
    default:
        return -kSinQuarter[64u - idx];
    }
}

[[nodiscard]] float Cos256(unsigned angle256) {
    return Sin256(angle256 + 64u);
}

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

[[nodiscard]] bool SubmitPacket(ps2::GsContext& gs, qword_t* end) {
    if (gs.packet == nullptr || end <= gs.packet->data) {
        return false;
    }
    dma_channel_send_normal(DMA_CHANNEL_GIF, gs.packet->data, end - gs.packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
    return true;
}

[[nodiscard]] bool IsDisplayReady(const ps2::GsContext& gs) {
    return gs.ready && gs.packet != nullptr && gs.frame.width > 0;
}

void RotatePoint(float centerX, float centerY, float lx, float ly, float cosA, float sinA,
                 float& outX, float& outY) {
    outX = centerX + lx * cosA - ly * sinA;
    outY = centerY + lx * sinA + ly * cosA;
}

#endif

} // namespace

float Ps2Sin256(unsigned angle256) {
#if defined(LEON_PLATFORM_PS2)
    return Sin256(angle256);
#else
    (void)angle256;
    return 0.0f;
#endif
}

float Ps2Cos256(unsigned angle256) {
#if defined(LEON_PLATFORM_PS2)
    return Cos256(angle256);
#else
    (void)angle256;
    return 1.0f;
#endif
}

bool Ps2DrawUnlitTriangleAt(float centerX, float centerY, float size, unsigned angle256, float r,
                            float g, float b) {
#if defined(LEON_PLATFORM_PS2)
    auto& gs = ps2::GetGsContext();
    if (!IsDisplayReady(gs) || size <= 0.0f) {
        return false;
    }

    // draw_triangle_filled adds +2048; with XYOFFSET at (2048-w/2, 2048-h/2),
    // drawable space is centered on (0,0).
    const float cosA = Cos256(angle256);
    const float sinA = Sin256(angle256);

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
#else
    (void)centerX;
    (void)centerY;
    (void)size;
    (void)angle256;
    (void)r;
    (void)g;
    (void)b;
    return false;
#endif
}

bool Ps2DrawUnlitRect(float x0, float y0, float x1, float y1, float r, float g, float b) {
#if defined(LEON_PLATFORM_PS2)
    auto& gs = ps2::GetGsContext();
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
#else
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)r;
    (void)g;
    (void)b;
    return false;
#endif
}

bool Ps2DrawUnlitRectAlpha(float x0, float y0, float x1, float y1, float r, float g, float b,
                           float alpha) {
#if defined(LEON_PLATFORM_PS2)
    auto& gs = ps2::GetGsContext();
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
#else
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)r;
    (void)g;
    (void)b;
    (void)alpha;
    return false;
#endif
}

bool Ps2DrawUnlitTriangle() {
#if defined(LEON_PLATFORM_PS2)
    auto& gs = ps2::GetGsContext();
    if (!IsDisplayReady(gs)) {
        return false;
    }
    const float size = static_cast<float>(gs.frame.height) * 0.28f;
    return Ps2DrawUnlitTriangleAt(0.0f, 0.0f, size, 0, 1.0f, 0.784f, 0.125f);
#else
    return false;
#endif
}

bool Ps2DrawCookedMesh(const void* data, unsigned size) {
    if (data == nullptr || size < sizeof(Ps2MeshHeader)) {
        return false;
    }
    Ps2MeshHeader header{};
    std::memcpy(&header, data, sizeof(header));
    if (std::memcmp(header.magic, "LPS2", 4) != 0 || header.version != 1) {
        return false;
    }
#if defined(LEON_PLATFORM_PS2)
    if (header.vertexCount == 0) {
        return false;
    }
    // Full LPS2 vertex upload is not wired yet; keep a visible GS result for cook smoke.
    return Ps2DrawUnlitTriangle();
#else
    return header.vertexCount > 0;
#endif
}

} // namespace leon::rhi
