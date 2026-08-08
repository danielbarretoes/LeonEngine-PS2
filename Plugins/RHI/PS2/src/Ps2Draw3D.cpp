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
    /// Camera world position used to shift objects into camera-relative space.
    float CamX = 0.0f;
    float CamY = 0.0f;
    float CamZ = 0.0f;
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
    // Camera-relative rendering: bake translation into object positions, keep view at origin.
    // Avoids EE float error when character/camera drift far from world origin.
    cache.CamX = vt.LocationX;
    cache.CamY = vt.LocationY;
    cache.CamZ = vt.LocationZ;
    VECTOR cameraPosition __attribute__((aligned(16))) = {0.0f, 0.0f, 0.0f, 1.0f};
    VECTOR cameraRotation __attribute__((aligned(16))) = {vt.Pitch, vt.Yaw, 0.0f, 1.0f};
    create_world_view(cache.WorldView, cameraPosition, cameraRotation);
    // Match ps2sdk ee/draw/samples/cube: glFrustum-like perspective (aspect applied inside).
    create_view_screen(cache.ViewScreen, graph_aspect_ratio(), -3.0f, 3.0f, -3.0f, 3.0f, 1.0f,
                       2000.0f);
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

// Only emit tris fully past the near plane (no near-clip lerp). Clipped
// "wallpaper" fans were the horizontal smear band when orbiting / moving.
constexpr float kNearW = 3.5f;
constexpr float kNdcGuard = 1.15f;
constexpr float kMaxNdcEdge = 1.45f;

struct ClipVertex {
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float W = 0.0f;
    float R = 0.0f;
    float G = 0.0f;
    float B = 0.0f;
    float S = 0.0f;
    float T = 0.0f;
};

void TransformToClip(VECTOR* outClip, int count, const VECTOR* in, const MATRIX localScreen) {
    for (int i = 0; i < count; ++i) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;
        for (int j = 0; j < 4; ++j) {
            const float s = (j != 3) ? in[i][j] : 1.0f;
            x += localScreen[(4 * j) + 0] * s;
            y += localScreen[(4 * j) + 1] * s;
            z += localScreen[(4 * j) + 2] * s;
            w += localScreen[(4 * j) + 3] * s;
        }
        outClip[i][0] = x;
        outClip[i][1] = y;
        outClip[i][2] = z;
        outClip[i][3] = w;
    }
}

[[nodiscard]] float Max3(float a, float b, float c) {
    float m = a;
    if (b > m) {
        m = b;
    }
    if (c > m) {
        m = c;
    }
    return m;
}

[[nodiscard]] bool PerspectiveDivide(ClipVertex& v) {
    if (v.W < kNearW) {
        return false;
    }
    const float invW = 1.0f / v.W;
    v.X *= invW;
    v.Y *= invW;
    v.Z *= invW;
    return true;
}

[[nodiscard]] bool InNdcGuard(const ClipVertex& v) {
    return v.X >= -kNdcGuard && v.X <= kNdcGuard && v.Y >= -kNdcGuard && v.Y <= kNdcGuard;
}

[[nodiscard]] float NdcEdgeLen2(const ClipVertex& a, const ClipVertex& b) {
    const float dx = a.X - b.X;
    const float dy = a.Y - b.Y;
    return dx * dx + dy * dy;
}

[[nodiscard]] bool FanTriOk(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c) {
    if (!InNdcGuard(a) || !InNdcGuard(b) || !InNdcGuard(c)) {
        return false;
    }
    const float e2 = Max3(NdcEdgeLen2(a, b), NdcEdgeLen2(b, c), NdcEdgeLen2(c, a));
    return e2 < kMaxNdcEdge * kMaxNdcEdge;
}

[[nodiscard]] bool TrianglePastNear(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c) {
    return a.W >= kNearW && b.W >= kNearW && c.W >= kNearW;
}

void EmitConverted(const ClipVertex& v, xyz_t& outXyz, color_t& outRgb, texel_t* outSt) {
    VECTOR ndc __attribute__((aligned(16))) = {v.X, v.Y, v.Z, v.W};
    VECTOR col __attribute__((aligned(16))) = {v.R, v.G, v.B, 1.0f};
    draw_convert_xyz(&outXyz, 2048, 2048, 32, 1, reinterpret_cast<vertex_f_t*>(&ndc));
    draw_convert_rgbq(&outRgb, 1, reinterpret_cast<vertex_f_t*>(&ndc),
                      reinterpret_cast<color_f_t*>(&col), 0x80);
    if (outSt != nullptr) {
        VECTOR st __attribute__((aligned(16))) = {v.S, v.T, 0.0f, 1.0f};
        draw_convert_st(outSt, 1, reinterpret_cast<vertex_f_t*>(&ndc),
                        reinterpret_cast<texel_f_t*>(&st));
    }
}

[[nodiscard]] ClipVertex MakeClipVert(const VECTOR& clip, const VECTOR& shade, const VECTOR& uv) {
    ClipVertex v{};
    v.X = clip[0];
    v.Y = clip[1];
    v.Z = clip[2];
    v.W = clip[3];
    v.R = shade[0];
    v.G = shade[1];
    v.B = shade[2];
    v.S = uv[0];
    v.T = uv[1];
    return v;
}

#endif

} // namespace

bool Ps2DrawBox(float locationX, float locationY, float locationZ, unsigned yaw256,
                unsigned pitch256, float scaleX, float scaleY, float scaleZ) {
#if defined(LEON_PLATFORM_PS2)
    auto& gs = ps2::GetGsContext();
    if (!gs.ready || gs.packet == nullptr || scaleX <= 0.0f || scaleY <= 0.0f || scaleZ <= 0.0f) {
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

    auto& view = CachedView();
    VECTOR objectPosition __attribute__((aligned(16))) = {
        locationX - view.CamX, locationY - view.CamY, locationZ - view.CamZ, 1.0f};
    VECTOR objectRotation __attribute__((aligned(16))) = {TurnsToRadians(pitch256),
                                                          TurnsToRadians(yaw256), 0.0f, 1.0f};
    VECTOR objectScale __attribute__((aligned(16))) = {scaleX, scaleY, scaleZ, 1.0f};

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

    // Local SRT: scale → rotate → translate (same multiply order as create_local_world,
    // with scale first). Scaling AFTER create_local_world shears non-uniform boxes on yaw.
    matrix_unit(localWorld);
    matrix_scale(localWorld, localWorld, objectScale);
    matrix_rotate(localWorld, localWorld, objectRotation);
    matrix_translate(localWorld, localWorld, objectPosition);
    create_local_light(localLight, objectRotation);
    create_local_screen(localScreen, localWorld, view.WorldView, view.ViewScreen);

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

    VECTOR clipVerts[kCubeVertexCount] __attribute__((aligned(16)));
    TransformToClip(clipVerts, kCubeVertexCount, kFaceCorners, localScreen);

    prim_t prim{};
    prim.type = PRIM_TRIANGLE;
    // Gouraud so per-vertex Q interpolates (flat flattens Q → texture swim).
    prim.shading = PRIM_SHADE_GOURAUD;
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
    int emitted = 0;
    if (textured) {
        auto* dw = reinterpret_cast<std::uint64_t*>(draw_prim_start(q, 0, &prim, &baseColor));
        for (int t = 0; t < kCubePointCount; t += 3) {
            const int i0 = kCubePoints[t];
            const int i1 = kCubePoints[t + 1];
            const int i2 = kCubePoints[t + 2];
            ClipVertex v0 = MakeClipVert(clipVerts[i0], shaded[i0], kFaceUVs[i0]);
            ClipVertex v1 = MakeClipVert(clipVerts[i1], shaded[i1], kFaceUVs[i1]);
            ClipVertex v2 = MakeClipVert(clipVerts[i2], shaded[i2], kFaceUVs[i2]);
            // No near-clip lerp: any tri that crosses near is dropped (avoids wallpaper smear).
            if (!TrianglePastNear(v0, v1, v2)) {
                continue;
            }
            if (!PerspectiveDivide(v0) || !PerspectiveDivide(v1) || !PerspectiveDivide(v2)) {
                continue;
            }
            if (!FanTriOk(v0, v1, v2)) {
                continue;
            }
            xyz_t xyz[3];
            color_t rgb[3];
            texel_t st[3];
            EmitConverted(v0, xyz[0], rgb[0], &st[0]);
            EmitConverted(v1, xyz[1], rgb[1], &st[1]);
            EmitConverted(v2, xyz[2], rgb[2], &st[2]);
            for (int k = 0; k < 3; ++k) {
                *dw++ = rgb[k].rgbaq;
                *dw++ = st[k].uv;
                *dw++ = xyz[k].xyz;
            }
            ++emitted;
        }
        if ((reinterpret_cast<std::uintptr_t>(dw) % 16u) != 0u) {
            *dw++ = 0;
        }
        q = draw_prim_end(reinterpret_cast<qword_t*>(dw), 3, DRAW_STQ_REGLIST);
    } else {
        q = draw_prim_start(q, 0, &prim, &baseColor);
        VECTOR zeroUv __attribute__((aligned(16))) = {0.0f, 0.0f, 0.0f, 1.0f};
        for (int t = 0; t < kCubePointCount; t += 3) {
            const int i0 = kCubePoints[t];
            const int i1 = kCubePoints[t + 1];
            const int i2 = kCubePoints[t + 2];
            ClipVertex v0 = MakeClipVert(clipVerts[i0], shaded[i0], zeroUv);
            ClipVertex v1 = MakeClipVert(clipVerts[i1], shaded[i1], zeroUv);
            ClipVertex v2 = MakeClipVert(clipVerts[i2], shaded[i2], zeroUv);
            if (!TrianglePastNear(v0, v1, v2)) {
                continue;
            }
            if (!PerspectiveDivide(v0) || !PerspectiveDivide(v1) || !PerspectiveDivide(v2)) {
                continue;
            }
            if (!FanTriOk(v0, v1, v2)) {
                continue;
            }
            xyz_t xyz[3];
            color_t rgb[3];
            EmitConverted(v0, xyz[0], rgb[0], nullptr);
            EmitConverted(v1, xyz[1], rgb[1], nullptr);
            EmitConverted(v2, xyz[2], rgb[2], nullptr);
            for (int k = 0; k < 3; ++k) {
                q->dw[0] = rgb[k].rgbaq;
                q->dw[1] = xyz[k].xyz;
                ++q;
            }
            ++emitted;
        }
        q = draw_prim_end(q, 2, DRAW_RGBAQ_REGLIST);
    }
    if (emitted == 0) {
        return true;
    }
    q = draw_finish(q);
    return Submit(gs, q);
#else
    (void)locationX;
    (void)locationY;
    (void)locationZ;
    (void)yaw256;
    (void)pitch256;
    (void)scaleX;
    (void)scaleY;
    (void)scaleZ;
    return false;
#endif
}

} // namespace leon::rhi
