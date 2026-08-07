#include "Ps2GsInternal.h"

#include <leon/rhi/Ps2RHI.h>

#include <cstdint>
#include <cstring>

#if defined(LEON_PLATFORM_PS2)
#include <dma.h>
#include <draw2d.h>
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

// Quarter-wave sin table (0..64 => 0..90°), avoids libm on EE soft-float.
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
    unsigned idx = a & 63u;
    float s = 0.0f;
    switch (quad) {
    case 0:
        s = kSinQuarter[idx];
        break;
    case 1:
        s = kSinQuarter[64u - idx];
        break;
    case 2:
        s = -kSinQuarter[idx];
        break;
    default:
        s = -kSinQuarter[64u - idx];
        break;
    }
    return s;
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
    const int rr = static_cast<int>(r * 255.0f) & 0xFF;
    const int gg = static_cast<int>(g * 255.0f) & 0xFF;
    const int bb = static_cast<int>(b * 255.0f) & 0xFF;
    out.r = static_cast<unsigned char>(rr);
    out.g = static_cast<unsigned char>(gg);
    out.b = static_cast<unsigned char>(bb);
    out.a = 0x80;
    out.q = 1.0f;
}

[[nodiscard]] bool SubmitPacket(qword_t* end) {
    using ps2gs::g_packet;
    if (g_packet == nullptr || end <= g_packet->data) {
        return false;
    }
    dma_channel_send_normal(DMA_CHANNEL_GIF, g_packet->data, end - g_packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
    return true;
}

[[nodiscard]] bool DisplayReady() {
    using ps2gs::g_displayReady;
    using ps2gs::g_frame;
    using ps2gs::g_packet;
    return g_displayReady && g_packet != nullptr && g_frame.width > 0;
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

bool Ps2DrawUnlitTriangleEx(float centerX, float centerY, float size, unsigned angle256, float r,
                            float g, float b) {
#if defined(LEON_PLATFORM_PS2)
    if (!DisplayReady() || size <= 0.0f) {
        return false;
    }

    const float c = Cos256(angle256);
    const float s = Sin256(angle256);
    // Unit triangle in local space, then rotate + translate.
    const float lx0 = 0.0f;
    const float ly0 = -size;
    const float lx1 = -size * 0.9f;
    const float ly1 = size * 0.75f;
    const float lx2 = size * 0.9f;
    const float ly2 = size * 0.75f;

    auto rot = [&](float lx, float ly, float& ox, float& oy) {
        ox = centerX + lx * c - ly * s;
        oy = centerY + lx * s + ly * c;
    };

    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    rot(lx0, ly0, x0, y0);
    rot(lx1, ly1, x1, y1);
    rot(lx2, ly2, x2, y2);

    triangle_t tri{};
    FillColor(tri.color, r, g, b);
    FillVertex(tri.v0, x0, y0);
    FillVertex(tri.v1, x1, y1);
    FillVertex(tri.v2, x2, y2);

    qword_t* q = ps2gs::g_packet->data;
    q = draw_triangle_filled(q, 0, &tri);
    q = draw_finish(q);
    return SubmitPacket(q);
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
    if (!DisplayReady()) {
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

    qword_t* q = ps2gs::g_packet->data;
    q = draw_rect_filled(q, 0, &rect);
    q = draw_finish(q);
    return SubmitPacket(q);
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

bool Ps2DrawUnlitTriangle() {
#if defined(LEON_PLATFORM_PS2)
    using ps2gs::g_frame;
    if (!DisplayReady()) {
        return false;
    }
    const float size = static_cast<float>(g_frame.height) * 0.28f;
    return Ps2DrawUnlitTriangleEx(0.0f, 0.0f, size, 0, 1.0f, 0.784f, 0.125f);
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
    return Ps2DrawUnlitTriangle();
#else
    return header.vertexCount > 0;
#endif
}

} // namespace leon::rhi
