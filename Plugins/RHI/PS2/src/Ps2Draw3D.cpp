#include "Ps2GsContext.h"

#include <leon/rhi/Ps2RHI.h>

#if defined(LEON_PLATFORM_PS2)
#include <dma.h>
#include <draw.h>
#include <draw3d.h>
#include <graph.h>
#include <math3d.h>
#include <packet.h>
#endif

namespace leon::rhi {
namespace {

#if defined(LEON_PLATFORM_PS2)

constexpr float kTwoPi = 6.28318530718f;
constexpr int kFaceCount = 6;
constexpr int kVertsPerFace = 4;
constexpr int kCubeVertexCount = kFaceCount * kVertsPerFace;
constexpr int kCubePointCount = kFaceCount * 6;

// Face albedo (RGB cube). Tint multiplies these.
constexpr float kFaceRgb[kFaceCount][3] = {
    {0.92f, 0.22f, 0.18f}, // +X
    {0.55f, 0.12f, 0.10f}, // -X
    {0.28f, 0.90f, 0.32f}, // +Y (receives top light most)
    {0.14f, 0.38f, 0.16f}, // -Y (in shadow)
    {0.28f, 0.48f, 0.95f}, // +Z
    {0.14f, 0.22f, 0.48f}, // -Z
};

// Object-space outward normals. w must be 1 for math3d::vector_innerproduct.
VECTOR kFaceNormals[kFaceCount] __attribute__((aligned(16))) = {
    {1.0f, 0.0f, 0.0f, 1.0f},  {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f},
    {0.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f},  {0.0f, 0.0f, -1.0f, 1.0f},
};

VECTOR kFaceCorners[kCubeVertexCount] __attribute__((aligned(16))) = {
    // +X
    {1.0f, -1.0f, -1.0f, 1.0f},
    {1.0f, -1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f, 1.0f},
    // -X
    {-1.0f, -1.0f, 1.0f, 1.0f},
    {-1.0f, -1.0f, -1.0f, 1.0f},
    {-1.0f, 1.0f, -1.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f, 1.0f},
    // +Y
    {-1.0f, 1.0f, -1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f, 1.0f},
    // -Y
    {-1.0f, -1.0f, 1.0f, 1.0f},
    {1.0f, -1.0f, 1.0f, 1.0f},
    {1.0f, -1.0f, -1.0f, 1.0f},
    {-1.0f, -1.0f, -1.0f, 1.0f},
    // +Z
    {-1.0f, -1.0f, 1.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, -1.0f, 1.0f, 1.0f},
    // -Z
    {1.0f, -1.0f, -1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f, 1.0f},
    {-1.0f, 1.0f, -1.0f, 1.0f},
    {-1.0f, -1.0f, -1.0f, 1.0f},
};

const int kCubePoints[kCubePointCount] = {
    0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
    12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
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

// math3d directional light: intensity = max(0, -N·L) where L is ray travel direction.
void BuildTopDirectionalLight(VECTOR outDirection, VECTOR outColour) {
    // Rays travel mostly downward (sun overhead), slight bias toward camera (+Z front).
    VECTOR raw __attribute__((aligned(16))) = {0.18f, -1.0f, -0.35f, 1.0f};
    vector_normalize(outDirection, raw);
    outDirection[3] = 1.0f;
    outColour[0] = 1.00f;
    outColour[1] = 0.97f;
    outColour[2] = 0.90f;
    outColour[3] = 1.00f;
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

    VECTOR cameraPosition __attribute__((aligned(16))) = {0.0f, 14.0f, 58.0f, 1.0f};
    VECTOR cameraRotation __attribute__((aligned(16))) = {-0.24f, 0.0f, 0.0f, 1.0f};

    // Lights: soft ambient fill + strong directional from above.
    VECTOR lightDirections[2] __attribute__((aligned(16)));
    VECTOR lightColours[2] __attribute__((aligned(16)));
    const int lightTypes[2] = {LIGHT_AMBIENT, LIGHT_DIRECTIONAL};

    lightDirections[0][0] = 0.0f;
    lightDirections[0][1] = 0.0f;
    lightDirections[0][2] = 0.0f;
    lightDirections[0][3] = 1.0f;
    lightColours[0][0] = 0.20f;
    lightColours[0][1] = 0.22f;
    lightColours[0][2] = 0.28f;
    lightColours[0][3] = 1.0f;

    BuildTopDirectionalLight(lightDirections[1], lightColours[1]);

    MATRIX localWorld;
    MATRIX localLight;
    MATRIX worldView;
    MATRIX viewScreen;
    MATRIX localScreen;

    matrix_unit(localWorld);
    create_local_world(localWorld, objectPosition, objectRotation);
    matrix_scale(localWorld, localWorld, objectScale);
    create_local_light(localLight, objectRotation);

    create_world_view(worldView, cameraPosition, cameraRotation);
    create_view_screen(viewScreen, graph_aspect_ratio(), -3.0f, 3.0f, -3.0f * (3.0f / 4.0f),
                       3.0f * (3.0f / 4.0f), 1.0f, 2000.0f);
    create_local_screen(localScreen, localWorld, worldView, viewScreen);

    VECTOR objectNormals[kCubeVertexCount] __attribute__((aligned(16)));
    VECTOR worldNormals[kCubeVertexCount] __attribute__((aligned(16)));
    VECTOR lights[kCubeVertexCount] __attribute__((aligned(16)));
    VECTOR albedos[kCubeVertexCount] __attribute__((aligned(16)));
    VECTOR shaded[kCubeVertexCount] __attribute__((aligned(16)));

    for (int face = 0; face < kFaceCount; ++face) {
        for (int v = 0; v < kVertsPerFace; ++v) {
            const int idx = face * kVertsPerFace + v;
            vector_copy(objectNormals[idx], kFaceNormals[face]);
            albedos[idx][0] = kFaceRgb[face][0] * r;
            albedos[idx][1] = kFaceRgb[face][1] * g;
            albedos[idx][2] = kFaceRgb[face][2] * b;
            albedos[idx][3] = 1.0f;
        }
    }

    calculate_normals(worldNormals, kCubeVertexCount, objectNormals, localLight);
    calculate_lights(lights, kCubeVertexCount, worldNormals, lightDirections, lightColours,
                     lightTypes, 2);
    calculate_colours(shaded, kCubeVertexCount, albedos, lights);

    VECTOR tempVertices[kCubeVertexCount] __attribute__((aligned(16)));
    xyz_t verts[kCubeVertexCount];
    color_t colors[kCubeVertexCount];

    calculate_vertices(tempVertices, kCubeVertexCount, kFaceCorners, localScreen);
    draw_convert_xyz(verts, 2048, 2048, 32, kCubeVertexCount,
                     reinterpret_cast<vertex_f_t*>(tempVertices));
    draw_convert_rgbq(colors, kCubeVertexCount, reinterpret_cast<vertex_f_t*>(tempVertices),
                      reinterpret_cast<color_f_t*>(shaded), 0x80);

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
    baseColor.r = 0x80;
    baseColor.g = 0x80;
    baseColor.b = 0x80;
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
