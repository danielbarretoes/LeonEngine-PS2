#include "Ps2GsContext.h"

#include <leon/rhi/Ps2RHI.h>

#if defined(LEON_PLATFORM_PS2)
#include <dma.h>
#include <draw.h>
#include <draw3d.h>
#include <graph.h>
#include <math3d.h>
#include <packet.h>

#include <cstring>
#endif

namespace leon::rhi {
namespace {

#if defined(LEON_PLATFORM_PS2)

constexpr float kTwoPi = 6.28318530718f;
constexpr int kCubeVertexCount = 8;
constexpr int kCubePointCount = 36; // 12 tris * 3

// Unit cube corners.
VECTOR kCubeCorners[kCubeVertexCount] __attribute__((aligned(16))) = {
    {-1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, -1.0f, 1.0f},
    {-1.0f, 1.0f, -1.0f, 1.0f},  {-1.0f, -1.0f, 1.0f, 1.0f},  {1.0f, -1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},    {-1.0f, 1.0f, 1.0f, 1.0f},
};

// Triangle list indices (outward faces).
const int kCubePoints[kCubePointCount] = {
    0, 1, 2, 0, 2, 3, // -Z
    4, 6, 5, 4, 7, 6, // +Z
    0, 4, 5, 0, 5, 1, // -Y
    2, 6, 7, 2, 7, 3, // +Y
    0, 3, 7, 0, 7, 4, // -X
    1, 5, 6, 1, 6, 2, // +X
};

[[nodiscard]] float TurnsToRadians(unsigned angle256) {
    return (static_cast<float>(angle256 & 255u) * kTwoPi) / 256.0f;
}

[[nodiscard]] bool Submit(ps2::GsContext& gs, qword_t* end) {
    if (gs.packet == nullptr || end <= gs.packet->data) {
        return false;
    }
    dma_channel_send_normal(DMA_CHANNEL_GIF, gs.packet->data, end - gs.packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
    return true;
}

#endif

} // namespace

bool Ps2DrawUnlitBox(float centerX, float centerY, float centerZ, float halfExtent,
                     unsigned yaw256, unsigned pitch256, float r, float g, float b) {
#if defined(LEON_PLATFORM_PS2)
    auto& gs = ps2::GetGsContext();
    if (!gs.ready || gs.packet == nullptr || halfExtent <= 0.0f) {
        return false;
    }

    VECTOR objectPosition __attribute__((aligned(16))) = {centerX, centerY, centerZ, 1.0f};
    VECTOR objectRotation __attribute__((aligned(16))) = {TurnsToRadians(pitch256),
                                                          TurnsToRadians(yaw256), 0.0f, 1.0f};
    VECTOR objectScale __attribute__((aligned(16))) = {halfExtent, halfExtent, halfExtent, 1.0f};

    // Camera sits on +Z looking toward origin (math3d world_view convention).
    VECTOR cameraPosition __attribute__((aligned(16))) = {0.0f, 12.0f, 55.0f, 1.0f};
    VECTOR cameraRotation __attribute__((aligned(16))) = {-0.22f, 0.0f, 0.0f, 1.0f};

    MATRIX localWorld;
    MATRIX worldView;
    MATRIX viewScreen;
    MATRIX localScreen;

    matrix_unit(localWorld);
    create_local_world(localWorld, objectPosition, objectRotation);
    matrix_scale(localWorld, localWorld, objectScale);

    create_world_view(worldView, cameraPosition, cameraRotation);
    create_view_screen(viewScreen, graph_aspect_ratio(), -3.0f, 3.0f, -3.0f * (3.0f / 4.0f),
                       3.0f * (3.0f / 4.0f), 1.0f, 2000.0f);
    create_local_screen(localScreen, localWorld, worldView, viewScreen);

    VECTOR tempVertices[kCubeVertexCount] __attribute__((aligned(16)));
    VECTOR colours[kCubeVertexCount] __attribute__((aligned(16)));
    xyz_t verts[kCubeVertexCount];
    color_t colors[kCubeVertexCount];

    for (int i = 0; i < kCubeVertexCount; ++i) {
        colours[i][0] = r;
        colours[i][1] = g;
        colours[i][2] = b;
        colours[i][3] = 1.0f;
    }

    calculate_vertices(tempVertices, kCubeVertexCount, kCubeCorners, localScreen);
    draw_convert_xyz(verts, 2048, 2048, 32, kCubeVertexCount,
                     reinterpret_cast<vertex_f_t*>(tempVertices));
    draw_convert_rgbq(colors, kCubeVertexCount, reinterpret_cast<vertex_f_t*>(tempVertices),
                      reinterpret_cast<color_f_t*>(colours), 0x80);

    prim_t prim{};
    prim.type = PRIM_TRIANGLE;
    prim.shading = PRIM_SHADE_FLAT;
    prim.mapping = DRAW_DISABLE;
    prim.fogging = DRAW_DISABLE;
    prim.blending = DRAW_DISABLE;
    prim.antialiasing = DRAW_ENABLE;
    prim.mapping_type = PRIM_MAP_ST;
    prim.colorfix = PRIM_UNFIXED;

    color_t baseColor{};
    baseColor.r = static_cast<unsigned char>(static_cast<int>(r * 255.0f) & 0xFF);
    baseColor.g = static_cast<unsigned char>(static_cast<int>(g * 255.0f) & 0xFF);
    baseColor.b = static_cast<unsigned char>(static_cast<int>(b * 255.0f) & 0xFF);
    baseColor.a = 0x80;
    baseColor.q = 1.0f;

    qword_t* q = gs.packet->data;
    q = draw_prim_start(q, 0, &prim, &baseColor);
    for (int i = 0; i < kCubePointCount; ++i) {
        const int idx = kCubePoints[i];
        q->dw[0] = colors[idx].rgbaq;
        q->dw[1] = verts[idx].xyz;
        ++q;
    }
    q = draw_prim_end(q, 2, DRAW_RGBAQ_REGLIST);
    q = draw_finish(q);
    return Submit(gs, q);
#else
    (void)centerX;
    (void)centerY;
    (void)centerZ;
    (void)halfExtent;
    (void)yaw256;
    (void)pitch256;
    (void)r;
    (void)g;
    (void)b;
    return false;
#endif
}

} // namespace leon::rhi
