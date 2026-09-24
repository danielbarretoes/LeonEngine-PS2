#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include "Debug/DebugDraw.h"
#include "Physics/PhysScene.h"
#include "TriangleCollision.h"
#include <numbers>
#include <vector>

namespace {

constexpr glm::vec3 TraceMiss{0.25f, 0.85f, 1.0f};
constexpr glm::vec3 TraceHitPath{0.2f, 1.0f, 0.35f};
constexpr glm::vec3 TraceBeyond{1.0f, 0.25f, 0.2f};
constexpr glm::vec3 TraceNormal{1.0f, 0.9f, 0.2f};
constexpr glm::vec3 TraceShape{0.95f, 0.45f, 1.0f};

[[nodiscard]] bool BodyMatchesChannel(const FBodyInstance& Body, ECollisionChannel Channel) {
    switch (Channel) {
    case ECollisionChannel::WorldStatic:
        return Body.Type == EBodyType::Static;
    case ECollisionChannel::WorldDynamic:
        return Body.Type == EBodyType::Dynamic;
    case ECollisionChannel::Pawn:
    case ECollisionChannel::Visibility:
        return true;
    }
    return true;
}

[[nodiscard]] bool SegmentAabb(const glm::vec3& Start, const glm::vec3& End, const glm::vec3& Mn,
                               const glm::vec3& Mx, float& OutT, glm::vec3& OutNormal) {
    const glm::vec3 Dir = End - Start;
    float TEnter = 0.0f;
    float TExit = 1.0f;
    glm::vec3 EnterNormal{0.0f, 1.0f, 0.0f};
    bool bHitFace = false;

    for (int Axis = 0; Axis < 3; ++Axis) {
        if (std::abs(Dir[Axis]) < 1.0e-8f) {
            if (Start[Axis] < Mn[Axis] || Start[Axis] > Mx[Axis]) {
                return false;
            }
            continue;
        }

        const float Inv = 1.0f / Dir[Axis];
        float T0 = (Mn[Axis] - Start[Axis]) * Inv;
        float T1 = (Mx[Axis] - Start[Axis]) * Inv;
        float NormalSign = -1.0f;
        if (Inv < 0.0f) {
            std::swap(T0, T1);
            NormalSign = 1.0f;
        }

        if (T0 > TEnter) {
            TEnter = T0;
            EnterNormal = {};
            EnterNormal[Axis] = NormalSign;
            bHitFace = true;
        }
        TExit = std::min(TExit, T1);
        if (TEnter > TExit) {
            return false;
        }
    }

    if (TEnter < 0.0f || TEnter > 1.0f) {
        return false;
    }

    if (!bHitFace && TEnter <= 0.0f) {
        OutT = 0.0f;
        OutNormal = {0.0f, 1.0f, 0.0f};
        return true;
    }

    OutT = TEnter;
    OutNormal = EnterNormal;
    const float Len = glm::length(OutNormal);
    if (Len > 1.0e-6f) {
        OutNormal /= Len;
    }
    return true;
}

[[nodiscard]] bool SegmentFloorY(const glm::vec3& Start, const glm::vec3& End, float FloorY,
                                 float& OutT, glm::vec3& OutNormal) {
    const float Dy = End.y - Start.y;
    if (std::abs(Dy) < 1.0e-8f) {
        return false;
    }
    const float T = (FloorY - Start.y) / Dy;
    if (T < 0.0f || T > 1.0f) {
        return false;
    }
    OutT = T;
    OutNormal = {0.0f, Dy < 0.0f ? 1.0f : -1.0f, 0.0f};
    return true;
}

[[nodiscard]] bool PointInAabb(const glm::vec3& P, const glm::vec3& Center,
                               const glm::vec3& HalfExtents, float Inflate) {
    const glm::vec3 He = HalfExtents + glm::vec3{Inflate};
    return std::abs(P.x - Center.x) <= He.x && std::abs(P.y - Center.y) <= He.y &&
           std::abs(P.z - Center.z) <= He.z;
}

/// Segment vs FSlopePlane. `inflate` expands the plane along its normal (sphere/capsule radius).
/// Hits when the offset-plane distance changes sign (approach from either side).
[[nodiscard]] bool SegmentSlopePlane(const glm::vec3& Start, const glm::vec3& End,
                                     const FSlopePlane& Plane, float Inflate, float& OutT,
                                     glm::vec3& OutNormal) {
    const float NLen = glm::length(Plane.Normal);
    if (NLen < 1.0e-6f) {
        return false;
    }
    const glm::vec3 N = Plane.Normal / NLen;
    const float D0 = glm::dot(Start - Plane.Point, N) - Inflate;
    const float D1 = glm::dot(End - Plane.Point, N) - Inflate;
    if (D0 < 0.0f && D1 < 0.0f) {
        return false;
    }
    if (D0 > 0.0f && D1 > 0.0f) {
        return false;
    }
    const float Denom = D0 - D1;
    if (std::abs(Denom) < 1.0e-8f) {
        return false;
    }
    const float T = D0 / Denom;
    if (T < 0.0f || T > 1.0f) {
        return false;
    }
    const glm::vec3 HitLoc = Start + ((End - Start) * T);
    if (!PointInAabb(HitLoc, Plane.BoundsCenter, Plane.BoundsHalfExtents, Inflate)) {
        return false;
    }
    OutT = T;
    OutNormal = (D0 >= 0.0f) ? N : -N;
    return true;
}

void WriteHit(FHitResult& Out, const glm::vec3& Start, const glm::vec3& End, float T,
              const glm::vec3& Normal, std::size_t LevelMeshIndex, bool bFloorPlane) {
    const glm::vec3 Delta = End - Start;
    const float SegLen = glm::length(Delta);
    Out.bBlockingHit = true;
    Out.Time = T;
    Out.Distance = SegLen * T;
    Out.Location = Start + (Delta * T);
    Out.ImpactPoint = Out.Location;
    Out.ImpactNormal = Normal;
    Out.TraceStart = Start;
    Out.TraceEnd = End;
    Out.LevelMeshIndex = LevelMeshIndex;
    Out.bFloorPlane = bFloorPlane;
}

void AppendSlopePlaneHits(std::vector<FHitResult>& OutHits, const glm::vec3& Start,
                          const glm::vec3& End, float Radius, float HalfHeight,
                          const std::vector<FSlopePlane>& Planes) {
    for (const FSlopePlane& Plane : Planes) {
        const float NLen = glm::length(Plane.Normal);
        const float Ny = (NLen > 1.0e-6f) ? (std::abs(Plane.Normal.y) / NLen) : 1.0f;
        const float Inflate = Radius + (HalfHeight * Ny);
        float T = 1.0f;
        glm::vec3 Normal{};
        if (!SegmentSlopePlane(Start, End, Plane, Inflate, T, Normal)) {
            continue;
        }
        FHitResult Hit{};
        WriteHit(Hit, Start, End, T, Normal, ULevel::Npos, false);
        Hit.ImpactPoint = Hit.Location - (Normal * Inflate);
        OutHits.push_back(Hit);
    }
}

void SortHitsByTime(std::vector<FHitResult>& Hits) {
    std::sort(Hits.begin(), Hits.end(),
              [](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });
}

/// Copy nearest Multi hit into `outHit` (Unreal Single returns the first blocking hit).
[[nodiscard]] bool TakeNearestHit(const std::vector<FHitResult>& Hits, FHitResult& OutHit,
                                  const glm::vec3& Start, const glm::vec3& End) {
    OutHit = {};
    OutHit.TraceStart = Start;
    OutHit.TraceEnd = End;
    OutHit.Time = 1.0f;
    if (Hits.empty()) {
        return false;
    }
    OutHit = Hits.front();
    return true;
}

void AddRingXz(FDebugDraw& Draw, const glm::vec3& Center, float Radius, const glm::vec3& Color,
               int Segments = 16) {
    const float SegCount = static_cast<float>(Segments);
    for (int I = 0; I < Segments; ++I) {
        const float A0 = (static_cast<float>(I) / SegCount) * (2.0f * std::numbers::pi_v<float>);
        const float A1 =
            (static_cast<float>(I + 1) / SegCount) * (2.0f * std::numbers::pi_v<float>);
        Draw.AddLine(Center + glm::vec3{std::cos(A0) * Radius, 0.0f, std::sin(A0) * Radius},
                     Center + glm::vec3{std::cos(A1) * Radius, 0.0f, std::sin(A1) * Radius}, Color);
    }
}

void AddImpactMarker(FDebugDraw& Draw, const FHitResult& Hit) {
    const float S = 0.08f;
    Draw.AddLine(Hit.ImpactPoint + glm::vec3{-S, 0, 0}, Hit.ImpactPoint + glm::vec3{S, 0, 0},
                 TraceNormal);
    Draw.AddLine(Hit.ImpactPoint + glm::vec3{0, -S, 0}, Hit.ImpactPoint + glm::vec3{0, S, 0},
                 TraceNormal);
    Draw.AddLine(Hit.ImpactPoint + glm::vec3{0, 0, -S}, Hit.ImpactPoint + glm::vec3{0, 0, S},
                 TraceNormal);
    Draw.AddArrow(Hit.ImpactPoint, Hit.ImpactPoint + (Hit.ImpactNormal * 0.45f), TraceNormal,
                  0.12f, 0.07f);
}

void DrawTracePath(FDebugDraw& Draw, const glm::vec3& Start, const glm::vec3& End,
                   const std::vector<FHitResult>& Hits) {
    if (Hits.empty()) {
        Draw.AddLine(Start, End, TraceMiss);
        return;
    }
    const FHitResult& First = Hits.front();
    Draw.AddLine(Start, First.ImpactPoint, TraceHitPath);
    Draw.AddLine(First.ImpactPoint, End, TraceBeyond);
    for (const FHitResult& Hit : Hits) {
        AddImpactMarker(Draw, Hit);
    }
}

[[nodiscard]] bool ShouldDraw(const FCollisionQueryParams& Params, FDebugDraw* DebugDraw) {
    return DebugDraw != nullptr && Params.DrawDebugType == EDrawDebugTrace::ForOneFrame;
}

} // namespace

void DrawDebugLineTrace(FDebugDraw& Draw, const glm::vec3& Start, const glm::vec3& End,
                        const std::vector<FHitResult>& Hits) {
    DrawTracePath(Draw, Start, End, Hits);
}

void DrawDebugSphereTrace(FDebugDraw& Draw, const glm::vec3& Start, const glm::vec3& End,
                          float Radius, const std::vector<FHitResult>& Hits) {
    const float R = std::max(Radius, 0.0f);
    DrawTracePath(Draw, Start, End, Hits);
    AddRingXz(Draw, Start, R, TraceShape);
    AddRingXz(Draw, End, R, TraceShape);
    if (!Hits.empty()) {
        AddRingXz(Draw, Hits.front().ImpactPoint + glm::vec3{0.0f, R, 0.0f}, R, TraceHitPath);
    }
}

void DrawDebugCapsuleTrace(FDebugDraw& Draw, const glm::vec3& Start, const glm::vec3& End,
                           float Radius, float HalfHeight, const std::vector<FHitResult>& Hits) {
    const float R = std::max(Radius, 0.0f);
    const float Hh = std::max(HalfHeight, 0.0f);
    DrawTracePath(Draw, Start, End, Hits);
    AddRingXz(Draw, Start + glm::vec3{0.0f, Hh, 0.0f}, R, TraceShape);
    AddRingXz(Draw, Start - glm::vec3{0.0f, Hh, 0.0f}, R, TraceShape);
    AddRingXz(Draw, End + glm::vec3{0.0f, Hh, 0.0f}, R, TraceShape);
    AddRingXz(Draw, End - glm::vec3{0.0f, Hh, 0.0f}, R, TraceShape);
    Draw.AddLine(Start + glm::vec3{R, -Hh, 0.0f}, Start + glm::vec3{R, Hh, 0.0f}, TraceShape);
    Draw.AddLine(Start + glm::vec3{-R, -Hh, 0.0f}, Start + glm::vec3{-R, Hh, 0.0f}, TraceShape);
    if (!Hits.empty()) {
        AddRingXz(Draw, Hits.front().ImpactPoint, R, TraceHitPath);
    }
}

bool FPhysScene::LineTraceMultiByChannel(std::vector<FHitResult>& OutHits, const glm::vec3& Start,
                                        const glm::vec3& End, ECollisionChannel Channel,
                                        const FCollisionQueryParams& Params,
                                        FDebugDraw* DebugDraw) const {
    OutHits.clear();

    if (BackendIface != nullptr && BackendIface->HasNarrowPhaseTraces()) {
        // BodyInstances may have been nudged (CMC) without a Step — sync before CastRay.
        if (BackendIface->HasRigidWorld()) {
            BackendIface->RigidPrepareStep(Bodies, Params.SkipLevelMeshIndex);
        }
        (void)BackendIface->RigidLineTrace(OutHits, Start, End, Channel,
                                            Params.SkipLevelMeshIndex);
    } else {
        for (std::size_t Bi = 0; Bi < Bodies.size(); ++Bi) {
            const FBodyInstance& Body = Bodies[Bi];
            if (Body.LevelMeshIndex == Params.SkipLevelMeshIndex) {
                continue;
            }
            if (!BodyMatchesChannel(Body, Channel)) {
                continue;
            }
            const glm::vec3 Mn = Body.Position - Body.HalfExtents;
            const glm::vec3 Mx = Body.Position + Body.HalfExtents;
            float TAabb = 1.0f;
            glm::vec3 NAabb{};
            if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb)) {
                continue;
            }

            float T = TAabb;
            glm::vec3 Normal = NAabb;
            if (Body.CollisionShape == ECollisionShape::TriangleMesh && Bi < TriangleMeshes.size() &&
                TriangleMeshes[Bi].IsValid()) {
                float TMesh = 1.0f;
                glm::vec3 NMesh{};
                if (!SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], 0.0f, TMesh, NMesh)) {
                    continue; // Broadphase only — no actual triangle hit.
                }
                T = TMesh;
                Normal = NMesh;
            }

            FHitResult Hit{};
            WriteHit(Hit, Start, End, T, Normal, Body.LevelMeshIndex, false);
            OutHits.push_back(Hit);
        }
    }

    if (Params.bTraceFloorPlane) {
        float T = 1.0f;
        glm::vec3 Normal{};
        if (SegmentFloorY(Start, End, Params.FloorY, T, Normal)) {
            FHitResult Hit{};
            WriteHit(Hit, Start, End, T, Normal, ULevel::Npos, true);
            OutHits.push_back(Hit);
        }
    }
    AppendSlopePlaneHits(OutHits, Start, End, 0.0f, 0.0f, SlopePlanes);

    SortHitsByTime(OutHits);
    if (ShouldDraw(Params, DebugDraw)) {
        DrawDebugLineTrace(*DebugDraw, Start, End, OutHits);
    }
    return !OutHits.empty();
}

bool FPhysScene::LineTraceSingleByChannel(FHitResult& OutHit, const glm::vec3& Start,
                                         const glm::vec3& End, ECollisionChannel Channel,
                                         const FCollisionQueryParams& Params,
                                         FDebugDraw* DebugDraw) const {
    std::vector<FHitResult> Hits;
    (void)LineTraceMultiByChannel(Hits, Start, End, Channel, Params, DebugDraw);
    return TakeNearestHit(Hits, OutHit, Start, End);
}

bool FPhysScene::SphereTraceMultiByChannel(std::vector<FHitResult>& OutHits, const glm::vec3& Start,
                                          const glm::vec3& End, float Radius,
                                          ECollisionChannel Channel,
                                          const FCollisionQueryParams& Params,
                                          FDebugDraw* DebugDraw) const {
    const float R = std::max(Radius, 0.0f);
    OutHits.clear();

    if (BackendIface != nullptr && BackendIface->HasNarrowPhaseTraces()) {
        // Push FBodyInstance → Jolt before CastShape (same as Step prepare).
        if (BackendIface->HasRigidWorld()) {
            BackendIface->RigidPrepareStep(Bodies, Params.SkipLevelMeshIndex);
        }
        (void)BackendIface->RigidSphereTrace(OutHits, Start, End, R, Channel,
                                              Params.SkipLevelMeshIndex);
    } else {
        for (std::size_t Bi = 0; Bi < Bodies.size(); ++Bi) {
            const FBodyInstance& Body = Bodies[Bi];
            if (Body.LevelMeshIndex == Params.SkipLevelMeshIndex) {
                continue;
            }
            if (!BodyMatchesChannel(Body, Channel)) {
                continue;
            }
            const glm::vec3 Expand{R, R, R};
            const glm::vec3 Mn = Body.Position - Body.HalfExtents - Expand;
            const glm::vec3 Mx = Body.Position + Body.HalfExtents + Expand;
            float TAabb = 1.0f;
            glm::vec3 NAabb{};
            if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb)) {
                continue;
            }

            float T = TAabb;
            glm::vec3 Normal = NAabb;
            if (Body.CollisionShape == ECollisionShape::TriangleMesh && Bi < TriangleMeshes.size() &&
                TriangleMeshes[Bi].IsValid()) {
                float TMesh = 1.0f;
                glm::vec3 NMesh{};
                if (!SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], R, TMesh, NMesh)) {
                    continue;
                }
                T = TMesh;
                Normal = NMesh;
            }

            FHitResult Hit{};
            WriteHit(Hit, Start, End, T, Normal, Body.LevelMeshIndex, false);
            // Unreal FHitResult: Location = sweep shape center; ImpactPoint = surface contact.
            Hit.ImpactPoint = Hit.Location - (Normal * R);
            OutHits.push_back(Hit);
        }
    }

    if (Params.bTraceFloorPlane) {
        const float PlaneY = Params.FloorY + R;
        float T = 1.0f;
        glm::vec3 Normal{};
        if (SegmentFloorY(Start, End, PlaneY, T, Normal)) {
            FHitResult Hit{};
            WriteHit(Hit, Start, End, T, Normal, ULevel::Npos, true);
            Hit.ImpactPoint = Hit.Location - (Normal * R);
            OutHits.push_back(Hit);
        }
    }
    AppendSlopePlaneHits(OutHits, Start, End, R, 0.0f, SlopePlanes);

    SortHitsByTime(OutHits);
    if (ShouldDraw(Params, DebugDraw)) {
        DrawDebugSphereTrace(*DebugDraw, Start, End, R, OutHits);
    }
    return !OutHits.empty();
}

bool FPhysScene::SphereTraceSingleByChannel(FHitResult& OutHit, const glm::vec3& Start,
                                           const glm::vec3& End, float Radius,
                                           ECollisionChannel Channel,
                                           const FCollisionQueryParams& Params,
                                           FDebugDraw* DebugDraw) const {
    std::vector<FHitResult> Hits;
    (void)SphereTraceMultiByChannel(Hits, Start, End, Radius, Channel, Params, DebugDraw);
    return TakeNearestHit(Hits, OutHit, Start, End);
}

bool FPhysScene::CapsuleTraceMultiByChannel(std::vector<FHitResult>& OutHits, const glm::vec3& Start,
                                           const glm::vec3& End, float Radius, float HalfHeight,
                                           ECollisionChannel Channel,
                                           const FCollisionQueryParams& Params,
                                           FDebugDraw* DebugDraw) const {
    const float R = std::max(Radius, 0.0f);
    const float Hh = std::max(HalfHeight, 0.0f);
    OutHits.clear();
    const glm::vec3 Expand{R, Hh + R, R};

    if (BackendIface != nullptr && BackendIface->HasNarrowPhaseTraces()) {
        if (BackendIface->HasRigidWorld()) {
            BackendIface->RigidPrepareStep(Bodies, Params.SkipLevelMeshIndex);
        }
        (void)BackendIface->RigidCapsuleTrace(OutHits, Start, End, R, Hh, Channel,
                                               Params.SkipLevelMeshIndex);
    } else {
        for (std::size_t Bi = 0; Bi < Bodies.size(); ++Bi) {
            const FBodyInstance& Body = Bodies[Bi];
            if (Body.LevelMeshIndex == Params.SkipLevelMeshIndex) {
                continue;
            }
            if (!BodyMatchesChannel(Body, Channel)) {
                continue;
            }
            const glm::vec3 Mn = Body.Position - Body.HalfExtents - Expand;
            const glm::vec3 Mx = Body.Position + Body.HalfExtents + Expand;
            float TAabb = 1.0f;
            glm::vec3 NAabb{};
            if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb)) {
                continue;
            }

            float T = TAabb;
            glm::vec3 Normal = NAabb;
            if (Body.CollisionShape == ECollisionShape::TriangleMesh && Bi < TriangleMeshes.size() &&
                TriangleMeshes[Bi].IsValid()) {
                // Inflate like slope planes: radius + |ny|*halfHeight approximated via radius+hh.
                float TMesh = 1.0f;
                glm::vec3 NMesh{};
                if (!SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], R + Hh, TMesh, NMesh)) {
                    continue;
                }
                T = TMesh;
                Normal = NMesh;
            }

            FHitResult Hit{};
            WriteHit(Hit, Start, End, T, Normal, Body.LevelMeshIndex, false);
            const float Pull = (std::abs(Normal.y) > 0.5f) ? (Hh + R) : R;
            Hit.ImpactPoint = Hit.Location - (Normal * Pull);
            OutHits.push_back(Hit);
        }
    }

    if (Params.bTraceFloorPlane) {
        const float PlaneY = Params.FloorY + Hh + R;
        float T = 1.0f;
        glm::vec3 Normal{};
        if (SegmentFloorY(Start, End, PlaneY, T, Normal)) {
            FHitResult Hit{};
            WriteHit(Hit, Start, End, T, Normal, ULevel::Npos, true);
            Hit.ImpactPoint = Hit.Location - (Normal * (Hh + R));
            OutHits.push_back(Hit);
        }
    }
    AppendSlopePlaneHits(OutHits, Start, End, R, Hh, SlopePlanes);

    SortHitsByTime(OutHits);
    if (ShouldDraw(Params, DebugDraw)) {
        DrawDebugCapsuleTrace(*DebugDraw, Start, End, R, Hh, OutHits);
    }
    return !OutHits.empty();
}

bool FPhysScene::CapsuleTraceSingleByChannel(FHitResult& OutHit, const glm::vec3& Start,
                                            const glm::vec3& End, float Radius, float HalfHeight,
                                            ECollisionChannel Channel,
                                            const FCollisionQueryParams& Params,
                                            FDebugDraw* DebugDraw) const {
    std::vector<FHitResult> Hits;
    (void)CapsuleTraceMultiByChannel(Hits, Start, End, Radius, HalfHeight, Channel, Params,
                                     DebugDraw);
    return TakeNearestHit(Hits, OutHit, Start, End);
}

