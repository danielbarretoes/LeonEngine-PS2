#include "Ps2GsContext.h"
#include "Ps2SceneState.h"

#include <leon/rhi/Ps2RHI.h>

#include <cstdint>

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

constexpr float kFaceRgb[kFaceCount][3] = {
    {0.92f, 0.22f, 0.18f}, {0.55f, 0.12f, 0.10f}, {0.28f, 0.90f, 0.32f},
    {0.14f, 0.38f, 0.16f}, {0.28f, 0.48f, 0.95f}, {0.14f, 0.22f, 0.48f},
};

VECTOR kFaceNormals[kFaceCount] __attribute__((aligned(16))) = {
    {1.0f, 0.0f, 0.0f, 1.0f},  {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f},
    {0.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f},  {0.0f, 0.0f, -1.0f, 1.0f},
};

VECTOR kFaceCorners[kCubeVertexCount] __attribute__((aligned(16))) = {
    {1.0f, -1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, 1.0f, 1.0f},   {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f, 1.0f},   {-1.0f, -1.0f, 1.0f, 1.0f},  {-1.0f, -1.0f, -1.0f, 1.0f},
    {-1.0f, 1.0f, -1.0f, 1.0f},  {-1.0f, 1.0f, 1.0f, 1.0f},   {-1.0f, 1.0f, -1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f, 1.0f},   {1.0f, 1.0f, 1.0f, 1.0f},    {-1.0f, 1.0f, 1.0f, 1.0f},
    {-1.0f, -1.0f, 1.0f, 1.0f},  {1.0f, -1.0f, 1.0f, 1.0f},   {1.0f, -1.0f, -1.0f, 1.0f},
    {-1.0f, -1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f, 1.0f},  {-1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},    {1.0f, -1.0f, 1.0f, 1.0f},   {1.0f, -1.0f, -1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f, 1.0f},   {-1.0f, 1.0f, -1.0f, 1.0f},  {-1.0f, -1.0f, -1.0f, 1.0f},
};

// Shared per-face UV corner pattern (repeated for all 6 faces).
VECTOR kFaceUvCorners[kVertsPerFace] __attribute__((aligned(16))) = {
    {0.0f, 1.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

VECTOR kFaceUVs[kCubeVertexCount] __attribute__((aligned(16)));

const int kCubePoints[kCubePointCount] = {
    0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
    12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
};

struct ViewMatrices {
    MATRIX WorldView{};
    MATRIX ViewScreen{};
    bool Ready = false;
};

ViewMatrices& CachedView() {
    static ViewMatrices cache{};
    return cache;
}

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

void EnsureFaceUVs() {
    static bool ready = false;
    if (ready) {
        return;
    }
    for (int face = 0; face < kFaceCount; ++face) {
        for (int v = 0; v < kVertsPerFace; ++v) {
            vector_copy(kFaceUVs[face * kVertsPerFace + v], kFaceUvCorners[v]);
        }
    }
    ready = true;
}

void RefreshViewMatrices(const Ps2ViewTarget& vt) {
    auto& cache = CachedView();
    VECTOR cameraPosition __attribute__((aligned(16))) = {vt.LocationX, vt.LocationY, vt.LocationZ,
                                                          1.0f};
    VECTOR cameraRotation __attribute__((aligned(16))) = {vt.Pitch, vt.Yaw, 0.0f, 1.0f};
    create_world_view(cache.WorldView, cameraPosition, cameraRotation);
    create_view_screen(cache.ViewScreen, graph_aspect_ratio(), -3.0f, 3.0f, -3.0f * (3.0f / 4.0f),
                       3.0f * (3.0f / 4.0f), 1.0f, 2000.0f);
    cache.Ready = true;
}

// math3d: intensity = max(0, -N·L); L is ray travel direction. w must be 1.
void BuildDirectionalRay(VECTOR outDirection, unsigned yaw256, unsigned pitch256) {
    const float cp = Ps2Cos256(pitch256);
    VECTOR raw __attribute__((aligned(16))) = {Ps2Sin256(yaw256) * cp, -Ps2Sin256(pitch256),
                                               Ps2Cos256(yaw256) * cp, 1.0f};
    vector_normalize(outDirection, raw);
    outDirection[3] = 1.0f;
}

#endif

} // namespace

bool Ps2DrawBox(float locationX, float locationY, float locationZ, unsigned yaw256,
                unsigned pitch256, float scale) {
#if defined(LEON_PLATFORM_PS2)
    auto& gs = ps2::GetGsContext();
    if (!gs.ready || gs.packet == nullptr || scale <= 0.0f) {
        return false;
    }

    EnsureFaceUVs();

    auto& scene = ps2::GetSceneState();
    if (scene.ViewDirty || !CachedView().Ready) {
        RefreshViewMatrices(scene.ViewTarget);
        scene.ViewDirty = false;
    }

    const Ps2Material& mat = scene.BoundMaterial;
    const bool textured = mat.BaseColorMap != nullptr && mat.BaseColorMap->Valid();
    const bool lit = mat.ShadingModel == EShadingModel::DefaultLit;

    if (textured) {
        mat.BaseColorMap->Bind();
    }

    VECTOR objectPosition __attribute__((aligned(16))) = {locationX, locationY, locationZ, 1.0f};
    VECTOR objectRotation __attribute__((aligned(16))) = {TurnsToRadians(pitch256),
                                                          TurnsToRadians(yaw256), 0.0f, 1.0f};
    VECTOR objectScale __attribute__((aligned(16))) = {scale, scale, scale, 1.0f};

    VECTOR lightDirections[2] __attribute__((aligned(16)));
    VECTOR lightColours[2] __attribute__((aligned(16)));
    const int lightTypes[2] = {LIGHT_AMBIENT, LIGHT_DIRECTIONAL};

    lightDirections[0][0] = 0.0f;
    lightDirections[0][1] = 0.0f;
    lightDirections[0][2] = 0.0f;
    lightDirections[0][3] = 1.0f;
    lightColours[0][0] = scene.AmbientR;
    lightColours[0][1] = scene.AmbientG;
    lightColours[0][2] = scene.AmbientB;
    lightColours[0][3] = 1.0f;

    BuildDirectionalRay(lightDirections[1], scene.Sun.Yaw256, scene.Sun.Pitch256);
    lightColours[1][0] = scene.Sun.LightColorR * scene.Sun.Intensity;
    lightColours[1][1] = scene.Sun.LightColorG * scene.Sun.Intensity;
    lightColours[1][2] = scene.Sun.LightColorB * scene.Sun.Intensity;
    lightColours[1][3] = 1.0f;

    MATRIX localWorld;
    MATRIX localLight;
    MATRIX localScreen;

    matrix_unit(localWorld);
    create_local_world(localWorld, objectPosition, objectRotation);
    matrix_scale(localWorld, localWorld, objectScale);
    create_local_light(localLight, objectRotation);
    create_local_screen(localScreen, localWorld, CachedView().WorldView, CachedView().ViewScreen);

    VECTOR objectNormals[kCubeVertexCount] __attribute__((aligned(16)));
    VECTOR worldNormals[kCubeVertexCount] __attribute__((aligned(16)));
    VECTOR lights[kCubeVertexCount] __attribute__((aligned(16)));
    VECTOR albedos[kCubeVertexCount] __attribute__((aligned(16)));
    VECTOR shaded[kCubeVertexCount] __attribute__((aligned(16)));

    for (int face = 0; face < kFaceCount; ++face) {
        const float ar0 = mat.BaseColorR;
        const float ag0 = mat.BaseColorG;
        const float ab0 = mat.BaseColorB;
        const bool faceTint = mat.UseFaceAlbedo && !textured;
        for (int v = 0; v < kVertsPerFace; ++v) {
            const int idx = face * kVertsPerFace + v;
            vector_copy(objectNormals[idx], kFaceNormals[face]);
            albedos[idx][0] = faceTint ? ar0 * kFaceRgb[face][0] : ar0;
            albedos[idx][1] = faceTint ? ag0 * kFaceRgb[face][1] : ag0;
            albedos[idx][2] = faceTint ? ab0 * kFaceRgb[face][2] : ab0;
            albedos[idx][3] = 1.0f;
        }
    }

    if (lit) {
        calculate_normals(worldNormals, kCubeVertexCount, objectNormals, localLight);
        calculate_lights(lights, kCubeVertexCount, worldNormals, lightDirections, lightColours,
                         lightTypes, 2);
        calculate_colours(shaded, kCubeVertexCount, albedos, lights);
    } else {
        for (int i = 0; i < kCubeVertexCount; ++i) {
            vector_copy(shaded[i], albedos[i]);
        }
    }

    // GS MODULATE treats 0x80 as 1.0 — scale so convert maps 1.0 → ~128.
    if (textured) {
        for (int i = 0; i < kCubeVertexCount; ++i) {
            shaded[i][0] *= 0.5f;
            shaded[i][1] *= 0.5f;
            shaded[i][2] *= 0.5f;
        }
    }

    VECTOR tempVertices[kCubeVertexCount] __attribute__((aligned(16)));
    xyz_t verts[kCubeVertexCount];
    color_t colors[kCubeVertexCount];
    texel_t sts[kCubeVertexCount];

    calculate_vertices(tempVertices, kCubeVertexCount, kFaceCorners, localScreen);
    draw_convert_xyz(verts, 2048, 2048, 32, kCubeVertexCount,
                     reinterpret_cast<vertex_f_t*>(tempVertices));
    draw_convert_rgbq(colors, kCubeVertexCount, reinterpret_cast<vertex_f_t*>(tempVertices),
                      reinterpret_cast<color_f_t*>(shaded), 0x80);
    if (textured) {
        draw_convert_st(sts, kCubeVertexCount, reinterpret_cast<vertex_f_t*>(tempVertices),
                        reinterpret_cast<texel_f_t*>(kFaceUVs));
    }

    prim_t prim{};
    prim.type = PRIM_TRIANGLE;
    prim.shading = PRIM_SHADE_FLAT;
    prim.mapping = textured ? DRAW_ENABLE : DRAW_DISABLE;
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
    if (textured) {
        auto* dw = reinterpret_cast<std::uint64_t*>(draw_prim_start(q, 0, &prim, &baseColor));
        for (int i = 0; i < kCubePointCount; ++i) {
            const int idx = kCubePoints[i];
            *dw++ = colors[idx].rgbaq;
            *dw++ = sts[idx].uv;
            *dw++ = verts[idx].xyz;
        }
        if ((reinterpret_cast<std::uintptr_t>(dw) % 16u) != 0u) {
            *dw++ = 0;
        }
        q = draw_prim_end(reinterpret_cast<qword_t*>(dw), 3, DRAW_STQ_REGLIST);
    } else {
        q = draw_prim_start(q, 0, &prim, &baseColor);
        for (int i = 0; i < kCubePointCount; ++i) {
            const int idx = kCubePoints[i];
            q->dw[0] = colors[idx].rgbaq;
            q->dw[1] = verts[idx].xyz;
            ++q;
        }
        q = draw_prim_end(q, 2, DRAW_RGBAQ_REGLIST);
    }
    q = draw_finish(q);
    return Submit(gs, q);
#else
    (void)locationX;
    (void)locationY;
    (void)locationZ;
    (void)yaw256;
    (void)pitch256;
    (void)scale;
    return false;
#endif
}

} // namespace leon::rhi
