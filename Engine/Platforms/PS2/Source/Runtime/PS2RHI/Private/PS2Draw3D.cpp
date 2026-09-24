#include "PS2GSContext.h"
#include "PS2SceneState.h"

#include "PS2RHI.h"
#include "HAL/PlatformMath.h"

#include <cstdint>
#include <cstdio>

#include <dma.h>
#include <draw.h>
#include <draw3d.h>
#include <draw_tests.h>
#include <graph.h>
#include <math3d.h>
#include <packet.h>

namespace {

FPS2Draw3DStats gDraw3DDebug{};


constexpr float kTwoPi = 6.28318530718f;
constexpr int kFaceCount = 6;
constexpr int kVertsPerFace = 4;
constexpr int kCubeVertexCount = kFaceCount * kVertsPerFace;

constexpr float kFaceRgb[kFaceCount][3] = {
    {0.92f, 0.22f, 0.18f}, {0.55f, 0.12f, 0.10f}, {0.28f, 0.90f, 0.32f},
    {0.14f, 0.38f, 0.16f}, {0.28f, 0.48f, 0.95f}, {0.14f, 0.22f, 0.48f},
};

VECTOR kFaceNormals[kFaceCount] __attribute__((aligned(16))) = {
    {1.0f, 0.0f, 0.0f, 1.0f},  {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f},
    {0.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f},  {0.0f, 0.0f, -1.0f, 1.0f},
};

/// Local axis each face normal points along (0 = X, 1 = Y, 2 = Z) — picks the half-extent.
constexpr int kFaceAxis[kFaceCount] = {0, 0, 1, 1, 2, 2};

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

// Per-face UV corner pattern (same for all 6 faces).
constexpr float kFaceUv[kVertsPerFace][2] = {
    {0.0f, 1.0f},
    {1.0f, 1.0f},
    {1.0f, 0.0f},
    {0.0f, 0.0f},
};

// Two triangles per face, indices into the face's 4 corners.
constexpr int kFaceTris[2][3] = {{0, 1, 2}, {0, 2, 3}};

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

[[nodiscard]] bool Submit(Leon::PS2::FPS2GSContext& gs, qword_t* end) {
    if (gs.packet == nullptr || end <= gs.packet->data) {
        return false;
    }
    dma_channel_send_normal(DMA_CHANNEL_GIF, gs.packet->data, end - gs.packet->data, 0, 0);
    dma_wait_fast();
    draw_wait_finish();
    return true;
}

void RefreshViewMatrices(const FPS2ViewTarget& vt) {
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
    const float cp = FPlatformMath::Cos256(pitch256);
    VECTOR raw __attribute__((aligned(16))) = {FPlatformMath::Sin256(yaw256) * cp, -FPlatformMath::Sin256(pitch256),
                                               FPlatformMath::Cos256(yaw256) * cp, 1.0f};
    vector_normalize(outDirection, raw);
    outDirection[3] = 1.0f;
}

// --- Homogeneous clipping -------------------------------------------------------------------
// The GS has no clipper and math3d gives W = -Z_eye. draw_convert_xyz maps NDC ±1 onto the
// whole 0..4096 GS coordinate range, while the 640×448 screen only spans ~±0.16 × ±0.11 NDC.
// So: clip against the near plane and a ±kGuard band (keeps XYZ2 in range), trivially reject
// against the visible frustum, and let the GS scissor trim the rest. No triangle is dropped
// just because one vertex is off-screen or behind the camera.
constexpr float kNearW = 1.0f; // = create_view_screen near
constexpr float kGuard = 0.95f;
constexpr float kVisibleSlack = 1.05f;
// draw_convert_xyz z-bits: max_z = 1<<(bits-1); near (NDC z = 1) → 1<<bits fits ZBUF_32.
constexpr int kDepthBits = 24;
constexpr int kMaxPolyVerts = 3 + 5; // each of the 5 clip planes adds at most one vertex

enum : unsigned {
    kOutNear = 1u << 0,
    kOutVisXPos = 1u << 1,
    kOutVisXNeg = 1u << 2,
    kOutVisYPos = 1u << 3,
    kOutVisYNeg = 1u << 4,
    kOutGuardXPos = 1u << 5,
    kOutGuardXNeg = 1u << 6,
    kOutGuardYPos = 1u << 7,
    kOutGuardYNeg = 1u << 8,
    kOutRejectMask = kOutNear | kOutVisXPos | kOutVisXNeg | kOutVisYPos | kOutVisYNeg,
    kOutClipMask = kOutNear | kOutGuardXPos | kOutGuardXNeg | kOutGuardYPos | kOutGuardYNeg,
};

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

/// Visible frustum half-extents in NDC (screen px / 2048, see draw_convert_xyz).
struct VisibleExtents {
    float X = 0.16f;
    float Y = 0.11f;
};

[[nodiscard]] unsigned Outcode(const ClipVertex& v, const VisibleExtents& vis) {
    unsigned code = 0;
    if (v.W < kNearW) {
        code |= kOutNear;
    }
    const float vx = vis.X * v.W;
    const float vy = vis.Y * v.W;
    const float g = kGuard * v.W;
    if (v.X > vx) {
        code |= kOutVisXPos;
    }
    if (v.X < -vx) {
        code |= kOutVisXNeg;
    }
    if (v.Y > vy) {
        code |= kOutVisYPos;
    }
    if (v.Y < -vy) {
        code |= kOutVisYNeg;
    }
    if (v.X > g) {
        code |= kOutGuardXPos;
    }
    if (v.X < -g) {
        code |= kOutGuardXNeg;
    }
    if (v.Y > g) {
        code |= kOutGuardYPos;
    }
    if (v.Y < -g) {
        code |= kOutGuardYNeg;
    }
    return code;
}

/// Signed distance to clip plane p (inside when >= 0). Order matches kOutClipMask bits.
[[nodiscard]] float PlaneDistance(const ClipVertex& v, int plane) {
    switch (plane) {
    case 0:
        return v.W - kNearW;
    case 1:
        return kGuard * v.W - v.X;
    case 2:
        return kGuard * v.W + v.X;
    case 3:
        return kGuard * v.W - v.Y;
    default:
        return kGuard * v.W + v.Y;
    }
}

constexpr unsigned kPlaneBit[5] = {kOutNear, kOutGuardXPos, kOutGuardXNeg, kOutGuardYPos,
                                   kOutGuardYNeg};

[[nodiscard]] ClipVertex Lerp(const ClipVertex& a, const ClipVertex& b, float t) {
    ClipVertex o{};
    o.X = a.X + (b.X - a.X) * t;
    o.Y = a.Y + (b.Y - a.Y) * t;
    o.Z = a.Z + (b.Z - a.Z) * t;
    o.W = a.W + (b.W - a.W) * t;
    o.R = a.R + (b.R - a.R) * t;
    o.G = a.G + (b.G - a.G) * t;
    o.B = a.B + (b.B - a.B) * t;
    o.S = a.S + (b.S - a.S) * t;
    o.T = a.T + (b.T - a.T) * t;
    return o;
}

/// Sutherland–Hodgman against the planes in `planes` (kOutClipMask bits). Returns vertex count.
[[nodiscard]] int ClipPolygon(ClipVertex* poly, int count, unsigned planes) {
    ClipVertex scratch[kMaxPolyVerts];
    ClipVertex* in = poly;
    ClipVertex* out = scratch;
    for (int p = 0; p < 5 && count > 0; ++p) {
        if ((planes & kPlaneBit[p]) == 0) {
            continue;
        }
        int outCount = 0;
        for (int i = 0; i < count; ++i) {
            const ClipVertex& a = in[i];
            const ClipVertex& b = in[(i + 1) % count];
            const float da = PlaneDistance(a, p);
            const float db = PlaneDistance(b, p);
            if (da >= 0.0f) {
                out[outCount++] = a;
            }
            if ((da >= 0.0f) != (db >= 0.0f) && outCount < kMaxPolyVerts) {
                out[outCount++] = Lerp(a, b, da / (da - db));
            }
        }
        count = outCount;
        ClipVertex* t = in;
        in = out;
        out = t;
    }
    if (in != poly) {
        for (int i = 0; i < count; ++i) {
            poly[i] = in[i];
        }
    }
    return count;
}

/// Writes one GIF REGLIST vertex group (RGBAQ [+ ST] + XYZ2) after the perspective divide.
struct TriangleWriter {
    std::uint64_t* Dw = nullptr;
    bool Textured = false;
    int Count = 0;

    void Vertex(const ClipVertex& v) {
        const float invW = 1.0f / v.W;
        VECTOR ndc __attribute__((aligned(16))) = {v.X * invW, v.Y * invW, v.Z * invW, v.W};
        VECTOR col __attribute__((aligned(16))) = {v.R, v.G, v.B, 1.0f};
        xyz_t xyz{};
        color_t rgb{};
        draw_convert_xyz(&xyz, 2048, 2048, kDepthBits, 1, reinterpret_cast<vertex_f_t*>(&ndc));
        // Alpha 0x80 = 1.0 for GS modulate; never 0 (ATEST discards A==0). Q = 1/W.
        draw_convert_rgbq(&rgb, 1, reinterpret_cast<vertex_f_t*>(&ndc),
                          reinterpret_cast<color_f_t*>(&col), 0x80);
        *Dw++ = rgb.rgbaq;
        if (Textured) {
            VECTOR st __attribute__((aligned(16))) = {v.S, v.T, 0.0f, 1.0f};
            texel_t tex{};
            draw_convert_st(&tex, 1, reinterpret_cast<vertex_f_t*>(&ndc),
                            reinterpret_cast<texel_f_t*>(&st));
            *Dw++ = tex.uv;
        }
        *Dw++ = xyz.xyz;
    }

    void Triangle(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c) {
        Vertex(a);
        Vertex(b);
        Vertex(c);
        ++Count;
    }
};

void SubmitClippedTriangle(TriangleWriter& writer, const ClipVertex& a, const ClipVertex& b,
                           const ClipVertex& c, const VisibleExtents& vis) {
    ++gDraw3DDebug.InTris;
    const unsigned oa = Outcode(a, vis);
    const unsigned ob = Outcode(b, vis);
    const unsigned oc = Outcode(c, vis);
    if ((oa & ob & oc & kOutRejectMask) != 0) {
        ++gDraw3DDebug.Drop0;
        return;
    }
    const unsigned needClip = (oa | ob | oc) & kOutClipMask;
    if (needClip == 0) {
        ++gDraw3DDebug.Keep3;
        writer.Triangle(a, b, c);
        return;
    }
    ClipVertex poly[kMaxPolyVerts] = {a, b, c};
    const int n = ClipPolygon(poly, 3, needClip);
    if (n < 3) {
        ++gDraw3DDebug.Drop0;
        return;
    }
    ++gDraw3DDebug.Clipped;
    for (int i = 1; i + 1 < n; ++i) {
        writer.Triangle(poly[0], poly[i], poly[i + 1]);
    }
}


} // namespace

bool FPS2RHI::DrawBox(float locationX, float locationY, float locationZ, unsigned yaw256,
                unsigned pitch256, float scaleX, float scaleY, float scaleZ) {
    auto& gs = Leon::PS2::GetGSContext();
    if (!gs.ready || gs.packet == nullptr || scaleX <= 0.0f || scaleY <= 0.0f || scaleZ <= 0.0f) {
        return false;
    }

    auto& scene = Leon::PS2::GetSceneState();
    if (scene.ViewDirty || !CachedView().Ready) {
        RefreshViewMatrices(scene.ViewTarget);
        scene.ViewDirty = false;
    }
    ++gDraw3DDebug.Boxes;

    auto& view = CachedView();
    VECTOR objectPosition __attribute__((aligned(16))) = {
        locationX - view.CamX, locationY - view.CamY, locationZ - view.CamZ, 1.0f};
    VECTOR objectRotation __attribute__((aligned(16))) = {TurnsToRadians(pitch256),
                                                          TurnsToRadians(yaw256), 0.0f, 1.0f};
    VECTOR objectScale __attribute__((aligned(16))) = {scaleX, scaleY, scaleZ, 1.0f};
    const float halfExtent[3] = {scaleX, scaleY, scaleZ};

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

    VisibleExtents vis{};
    vis.X = static_cast<float>(gs.frame.width) * 0.5f / 2048.0f * kVisibleSlack;
    vis.Y = static_cast<float>(gs.frame.height) * 0.5f / 2048.0f * kVisibleSlack;

    // Clip-space corners; reject the whole box when every corner is outside one plane.
    VECTOR clipVerts[kCubeVertexCount] __attribute__((aligned(16)));
    for (int i = 0; i < kCubeVertexCount; ++i) {
        vector_apply(clipVerts[i], kFaceCorners[i], localScreen);
    }
    unsigned boxOut = ~0u;
    for (int i = 0; i < kCubeVertexCount; ++i) {
        ClipVertex c{};
        c.X = clipVerts[i][0];
        c.Y = clipVerts[i][1];
        c.W = clipVerts[i][3];
        boxOut &= Outcode(c, vis);
    }
    if ((boxOut & kOutRejectMask) != 0) {
        ++gDraw3DDebug.CulledBoxes;
        return true;
    }

    // Face normals in camera-relative world space (camera at the origin).
    VECTOR faceNormals[kFaceCount] __attribute__((aligned(16)));
    calculate_normals(faceNormals, kFaceCount, kFaceNormals, localLight);

    // Backface cull: visible when the face centre → camera vector points along the normal.
    bool faceVisible[kFaceCount];
    int visibleFaces = 0;
    for (int f = 0; f < kFaceCount; ++f) {
        const float h = halfExtent[kFaceAxis[f]];
        const float cx = objectPosition[0] + faceNormals[f][0] * h;
        const float cy = objectPosition[1] + faceNormals[f][1] * h;
        const float cz = objectPosition[2] + faceNormals[f][2] * h;
        faceVisible[f] = faceNormals[f][0] * cx + faceNormals[f][1] * cy +
                             faceNormals[f][2] * cz < 0.0f;
        if (faceVisible[f]) {
            ++visibleFaces;
        } else {
            ++gDraw3DDebug.BackFaces;
        }
    }
    if (visibleFaces == 0) {
        return true;
    }

    const FPS2Material& mat = scene.BoundMaterial;
    const bool textured = mat.BaseColorMap != nullptr && mat.BaseColorMap->Valid();
    const bool lit = mat.ShadingModel == EMaterialShadingModel::DefaultLit;
    if (textured) {
        mat.BaseColorMap->Bind();
    }

    // Flat faces under ambient + directional light: one colour per face (6, not 24 verts).
    VECTOR albedos[kFaceCount] __attribute__((aligned(16)));
    VECTOR shaded[kFaceCount] __attribute__((aligned(16)));
    const bool faceTint = mat.UseFaceAlbedo && !textured;
    for (int f = 0; f < kFaceCount; ++f) {
        albedos[f][0] = faceTint ? mat.BaseColorR * kFaceRgb[f][0] : mat.BaseColorR;
        albedos[f][1] = faceTint ? mat.BaseColorG * kFaceRgb[f][1] : mat.BaseColorG;
        albedos[f][2] = faceTint ? mat.BaseColorB * kFaceRgb[f][2] : mat.BaseColorB;
        albedos[f][3] = 1.0f;
    }
    if (lit) {
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

        VECTOR lights[kFaceCount] __attribute__((aligned(16)));
        calculate_lights(lights, kFaceCount, faceNormals, lightDirections, lightColours,
                         lightTypes, 2);
        calculate_colours(shaded, kFaceCount, albedos, lights);
    } else {
        for (int f = 0; f < kFaceCount; ++f) {
            vector_copy(shaded[f], albedos[f]);
        }
    }
    // GS MODULATE treats 0x80 as 1.0 — scale so convert maps 1.0 → ~128.
    if (textured) {
        for (int f = 0; f < kFaceCount; ++f) {
            shaded[f][0] *= 0.5f;
            shaded[f][1] *= 0.5f;
            shaded[f][2] *= 0.5f;
        }
    }

    prim_t prim{};
    prim.type = PRIM_TRIANGLE;
    // Gouraud so per-vertex Q interpolates (flat flattens Q → texture swim).
    prim.shading = PRIM_SHADE_GOURAUD;
    prim.mapping = textured ? DRAW_ENABLE : DRAW_DISABLE;
    prim.fogging = DRAW_DISABLE;
    prim.blending = DRAW_DISABLE;
    // AA edges look like screen-door / flicker when many tris overlap.
    prim.antialiasing = DRAW_DISABLE;
    prim.mapping_type = PRIM_MAP_ST;
    prim.colorfix = PRIM_UNFIXED;

    color_t baseColor{};
    baseColor.r = 0x80;
    baseColor.g = 0x80;
    baseColor.b = 0x80;
    baseColor.a = 0x80;
    baseColor.q = 1.0f;

    qword_t* q = gs.packet->data;
    // HUD / clear may leave TEST in ALLPASS — restore z before 3D.
    q = draw_enable_tests(q, 0, &gs.z);

    TriangleWriter writer{};
    writer.Textured = textured;
    writer.Dw = reinterpret_cast<std::uint64_t*>(draw_prim_start(q, 0, &prim, &baseColor));

    for (int f = 0; f < kFaceCount; ++f) {
        if (!faceVisible[f]) {
            continue;
        }
        ClipVertex corners[kVertsPerFace];
        for (int v = 0; v < kVertsPerFace; ++v) {
            const int idx = f * kVertsPerFace + v;
            ClipVertex& c = corners[v];
            c.X = clipVerts[idx][0];
            c.Y = clipVerts[idx][1];
            c.Z = clipVerts[idx][2];
            c.W = clipVerts[idx][3];
            c.R = shaded[f][0];
            c.G = shaded[f][1];
            c.B = shaded[f][2];
            c.S = kFaceUv[v][0];
            c.T = kFaceUv[v][1];
        }
        for (const auto& tri : kFaceTris) {
            SubmitClippedTriangle(writer, corners[tri[0]], corners[tri[1]], corners[tri[2]], vis);
        }
    }

    gDraw3DDebug.Emitted += static_cast<unsigned>(writer.Count);
    if (writer.Count == 0) {
        return true;
    }
    if ((reinterpret_cast<std::uintptr_t>(writer.Dw) % 16u) != 0u) {
        *writer.Dw++ = 0;
    }
    q = draw_prim_end(reinterpret_cast<qword_t*>(writer.Dw), textured ? 3 : 2,
                      textured ? DRAW_STQ_REGLIST : DRAW_RGBAQ_REGLIST);
    q = draw_finish(q);
    const unsigned used = static_cast<unsigned>(q - gs.packet->data);
    if (used > gDraw3DDebug.PacketQwordsPeak) {
        gDraw3DDebug.PacketQwordsPeak = used;
    }
    return Submit(gs, q);
}

void FPS2RHI::BeginDraw3DStatsFrame() {
    gDraw3DDebug = FPS2Draw3DStats{};
}

void FPS2RHI::GetDraw3DStats(FPS2Draw3DStats& out) {
    out = gDraw3DDebug;
}

void FPS2RHI::PrintDraw3DStats(const FPS2Draw3DStats& s) {
    std::printf("[Draw3D] boxes=%u culled=%u backfaces=%u tris=%u keep=%u drop=%u clip=%u "
                "emit=%u qwPeak=%u\n",
                s.Boxes, s.CulledBoxes, s.BackFaces, s.InTris, s.Keep3, s.Drop0, s.Clipped,
                s.Emitted, s.PacketQwordsPeak);
}

