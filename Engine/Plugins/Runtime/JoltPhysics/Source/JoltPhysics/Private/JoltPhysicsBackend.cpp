#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <limits>
#include <vector>

#include "JoltPhysicsBackend.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

JPH_SUPPRESS_WARNINGS

namespace {

void TraceImpl(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    std::cerr << buffer << '\n';
}

namespace Layers {
constexpr JPH::ObjectLayer NON_MOVING = 0;
constexpr JPH::ObjectLayer MOVING = 1;
constexpr JPH::ObjectLayer NUM_LAYERS = 2;
} // namespace Layers

namespace BroadPhaseLayers {
constexpr JPH::BroadPhaseLayer NON_MOVING(0);
constexpr JPH::BroadPhaseLayer MOVING(1);
constexpr JPH::uint NUM_LAYERS = 2;
} // namespace BroadPhaseLayers

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        switch (a) {
        case Layers::NON_MOVING:
            return b == Layers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            return false;
        }
    }
};

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        objectToBroadPhase_[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        objectToBroadPhase_[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return objectToBroadPhase_[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        switch ((JPH::BroadPhaseLayer::Type)layer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
            return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
            return "MOVING";
        default:
            return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer objectToBroadPhase_[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer,
                                     JPH::BroadPhaseLayer bp) const override {
        switch (layer) {
        case Layers::NON_MOVING:
            return bp == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            return false;
        }
    }
};

[[nodiscard]] JPH::ShapeRefC CreateBoxShape(const glm::vec3& halfExtents) {
    // Half-extents must exceed convex radius or BoxShapeSettings::Create fails.
    constexpr float kConvexRadius = 0.001f;
    constexpr float kMinHalf = 0.002f;
    const float hx = std::max(halfExtents.x, kMinHalf);
    const float hy = std::max(halfExtents.y, kMinHalf);
    const float hz = std::max(halfExtents.z, kMinHalf);
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(hx, hy, hz), kConvexRadius);
    shapeSettings.SetEmbedded();
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    if (shapeResult.HasError()) {
        return nullptr;
    }
    return shapeResult.Get();
}

/// Bake TriangleMeshCollision into a MeshShape in body-local space (origin = body.position).
[[nodiscard]] JPH::ShapeRefC CreateMeshShape(const TriangleMeshCollision& mesh,
                                             const glm::vec3& bodyPosition) {
    if (!mesh.IsValid()) {
        return nullptr;
    }
    JPH::TriangleList tris;
    tris.reserve(mesh.indices.size() / 3);
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const std::uint32_t i0 = mesh.indices[i];
        const std::uint32_t i1 = mesh.indices[i + 1];
        const std::uint32_t i2 = mesh.indices[i + 2];
        if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size() ||
            i2 >= mesh.positions.size()) {
            continue;
        }
        const glm::vec3 p0 = mesh.positions[i0] - bodyPosition;
        const glm::vec3 p1 = mesh.positions[i1] - bodyPosition;
        const glm::vec3 p2 = mesh.positions[i2] - bodyPosition;
        // Emit both windings so single-sided MeshShape collides from either side (floors/ceilings).
        tris.push_back(JPH::Triangle(JPH::Vec3(p0.x, p0.y, p0.z), JPH::Vec3(p1.x, p1.y, p1.z),
                                     JPH::Vec3(p2.x, p2.y, p2.z)));
        tris.push_back(JPH::Triangle(JPH::Vec3(p0.x, p0.y, p0.z), JPH::Vec3(p2.x, p2.y, p2.z),
                                     JPH::Vec3(p1.x, p1.y, p1.z)));
    }
    if (tris.empty()) {
        return nullptr;
    }
    // Heap settings: MeshShape retains cooked data; stack+SetEmbedded is fragile for meshes.
    JPH::Ref<JPH::MeshShapeSettings> meshSettings = new JPH::MeshShapeSettings(tris);
    JPH::ShapeSettings::ShapeResult shapeResult = meshSettings->Create();
    if (shapeResult.HasError()) {
        return nullptr;
    }
    return shapeResult.Get();
}

class JoltPhysicsBackend final : public IPhysicsBackend {
public:
    JoltPhysicsBackend() {
        EnsureJoltTypes();
        tempAllocator_ = std::make_unique<JPH::TempAllocatorImpl>(4 * 1024 * 1024);
        jobSystem_ = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

        constexpr JPH::uint kMaxBodies = 4096;
        constexpr JPH::uint kMaxBodyPairs = 4096;
        constexpr JPH::uint kMaxContactConstraints = 4096;
        physicsSystem_.Init(kMaxBodies, 0, kMaxBodyPairs, kMaxContactConstraints,
                            broadPhaseLayerInterface_, objectVsBroadphaseLayerFilter_,
                            objectVsObjectLayerFilter_);
    }

    ~JoltPhysicsBackend() override { RigidClear(); }

    [[nodiscard]] const char* GetName() const override { return "Jolt"; }
    [[nodiscard]] bool HasRigidWorld() const override { return true; }
    [[nodiscard]] bool HasNarrowPhaseTraces() const override { return true; }

    void RigidClear() override {
        JPH::BodyInterface& bodies = physicsSystem_.GetBodyInterface();
        for (const JPH::BodyID& id : bodyIds_) {
            DestroyBody(bodies, id);
        }
        bodyIds_.clear();
        DestroyFloor(bodies);
        lastSkip_ = (std::numeric_limits<std::size_t>::max)();
        floorY_ = std::numeric_limits<float>::quiet_NaN();
    }

    void RigidRebuild(const std::vector<BodyInstance>& bodies,
                      const std::vector<TriangleMeshCollision>* triangleMeshes,
                      std::size_t skipLevelMeshIndex) override {
        RigidClear();
        bodyIds_.assign(bodies.size(), JPH::BodyID());
        lastSkip_ = skipLevelMeshIndex;

        JPH::BodyInterface& iface = physicsSystem_.GetBodyInterface();
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].levelMeshIndex == skipLevelMeshIndex) {
                continue;
            }
            const TriangleMeshCollision* tri =
                (triangleMeshes != nullptr && i < triangleMeshes->size()) ? &(*triangleMeshes)[i]
                                                                          : nullptr;
            bodyIds_[i] = CreateBody(iface, bodies[i], i, tri);
        }
        physicsSystem_.OptimizeBroadPhase();
    }

    void RigidPrepareStep(const std::vector<BodyInstance>& bodies,
                          std::size_t skipLevelMeshIndex) override {
        // Structure changed outside SyncFromLevel — rebuild as boxes (meshes need SyncFromLevel).
        if (bodyIds_.size() != bodies.size()) {
            RigidRebuild(bodies, nullptr, skipLevelMeshIndex);
            return;
        }

        JPH::BodyInterface& iface = physicsSystem_.GetBodyInterface();
        lastSkip_ = skipLevelMeshIndex;

        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const BodyInstance& src = bodies[i];
            const bool skip = src.levelMeshIndex == skipLevelMeshIndex;

            if (skip) {
                if (!bodyIds_[i].IsInvalid()) {
                    DestroyBody(iface, bodyIds_[i]);
                    bodyIds_[i] = JPH::BodyID();
                }
                continue;
            }

            if (bodyIds_[i].IsInvalid()) {
                bodyIds_[i] = CreateBody(iface, src, i, nullptr);
                continue;
            }

            if (src.type != EBodyType::Dynamic) {
                continue;
            }

            // CMC / ResolveCapsuleSides may have nudged BodyInstance state — push into Jolt.
            iface.SetPosition(bodyIds_[i],
                              JPH::RVec3(src.position.x, src.position.y, src.position.z),
                              JPH::EActivation::Activate);
            iface.SetLinearVelocity(bodyIds_[i],
                                    JPH::Vec3(src.velXZ.x, src.velocityY, src.velXZ.y));
        }
    }

    void RigidStep(float deltaTime, float gravityMagnitude, float floorY) override {
        if (deltaTime <= 0.0f) {
            return;
        }
        EnsureFloor(floorY);
        physicsSystem_.SetGravity(JPH::Vec3(0.0f, -std::abs(gravityMagnitude), 0.0f));

        const int collisionSteps = std::max(1, static_cast<int>(std::ceil(deltaTime * 60.0f)));
        physicsSystem_.Update(deltaTime, collisionSteps, tempAllocator_.get(), jobSystem_.get());
    }

    void RigidReadBack(std::vector<BodyInstance>& bodies) override {
        JPH::BodyInterface& iface = physicsSystem_.GetBodyInterface();
        const std::size_t n = (std::min)(bodies.size(), bodyIds_.size());
        for (std::size_t i = 0; i < n; ++i) {
            const JPH::BodyID id = bodyIds_[i];
            if (id.IsInvalid() || bodies[i].type != EBodyType::Dynamic) {
                continue;
            }
            const JPH::RVec3 pos = iface.GetCenterOfMassPosition(id);
            const JPH::Vec3 vel = iface.GetLinearVelocity(id);
            bodies[i].position = {pos.GetX(), pos.GetY(), pos.GetZ()};
            bodies[i].velXZ = {vel.GetX(), vel.GetZ()};
            bodies[i].velocityY = vel.GetY();
        }
    }

    bool RigidLineTrace(std::vector<HitResult>& outHits, const glm::vec3& start,
                        const glm::vec3& end, ECollisionChannel channel,
                        std::size_t skipLevelMeshIndex) override {
        outHits.clear();
        const JPH::Vec3 origin(start.x, start.y, start.z);
        const JPH::Vec3 direction(end.x - start.x, end.y - start.y, end.z - start.z);
        const JPH::RRayCast ray(origin, direction);

        JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
        JPH::RayCastSettings settings;
        ChannelObjectLayerFilter layerFilter(channel);
        TraceBodyFilter bodyFilter(floorId_, skipLevelMeshIndex);
        physicsSystem_.GetNarrowPhaseQuery().CastRay(ray, settings, collector, {}, layerFilter,
                                                     bodyFilter);
        collector.Sort();

        const JPH::BodyLockInterface& locks = physicsSystem_.GetBodyLockInterface();
        for (const JPH::RayCastResult& hit : collector.mHits) {
            JPH::BodyLockRead lock(locks, hit.mBodyID);
            if (!lock.Succeeded()) {
                continue;
            }
            const JPH::Body& body = lock.GetBody();
            const JPH::RVec3 point = ray.GetPointOnRay(hit.mFraction);
            JPH::Vec3 normal =
                body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point);
            if (normal.LengthSq() < 1.0e-8f) {
                normal = JPH::Vec3(0, 1, 0);
            } else {
                normal = normal.Normalized();
            }
            // Point normal toward the trace start (Unreal ImpactNormal convention).
            const JPH::Vec3 toStart = origin - point;
            if (normal.Dot(toStart) < 0.0f) {
                normal = -normal;
            }
            HitResult out{};
            out.bBlockingHit = true;
            out.Time = hit.mFraction;
            out.Distance = direction.Length() * hit.mFraction;
            out.Location = {point.GetX(), point.GetY(), point.GetZ()};
            out.ImpactPoint = out.Location;
            out.ImpactNormal = {normal.GetX(), normal.GetY(), normal.GetZ()};
            out.TraceStart = start;
            out.TraceEnd = end;
            out.LevelMeshIndex = static_cast<std::size_t>(body.GetUserData());
            out.bFloorPlane = false;
            outHits.push_back(out);
        }
        return !outHits.empty();
    }

    bool RigidSphereTrace(std::vector<HitResult>& outHits, const glm::vec3& start,
                          const glm::vec3& end, float radius, ECollisionChannel channel,
                          std::size_t skipLevelMeshIndex) override {
        const float r = std::max(radius, 1.0e-3f);
        JPH::RefConst<JPH::SphereShape> sphere = new JPH::SphereShape(r);
        return CastShapeTrace(outHits, start, end, sphere, channel, skipLevelMeshIndex, r);
    }

    bool RigidCapsuleTrace(std::vector<HitResult>& outHits, const glm::vec3& start,
                           const glm::vec3& end, float radius, float halfHeight,
                           ECollisionChannel channel, std::size_t skipLevelMeshIndex) override {
        const float r = std::max(radius, 1.0e-3f);
        const float hh = std::max(halfHeight, 0.0f);
        JPH::RefConst<JPH::CapsuleShape> capsule = new JPH::CapsuleShape(hh, r);
        return CastShapeTrace(outHits, start, end, capsule, channel, skipLevelMeshIndex,
                              r + hh);
    }

private:
    class ChannelObjectLayerFilter final : public JPH::ObjectLayerFilter {
    public:
        explicit ChannelObjectLayerFilter(ECollisionChannel channel) : channel_(channel) {}
        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer) const override {
            switch (channel_) {
            case ECollisionChannel::WorldStatic:
                return layer == Layers::NON_MOVING;
            case ECollisionChannel::WorldDynamic:
                return layer == Layers::MOVING;
            case ECollisionChannel::Pawn:
            case ECollisionChannel::Visibility:
                return true;
            }
            return true;
        }

    private:
        ECollisionChannel channel_;
    };

    class TraceBodyFilter final : public JPH::BodyFilter {
    public:
        TraceBodyFilter(JPH::BodyID floorId, std::size_t skipMesh)
            : floorId_(floorId), skipMesh_(skipMesh) {}

        [[nodiscard]] bool ShouldCollide(const JPH::BodyID& id) const override {
            return id != floorId_;
        }

        [[nodiscard]] bool ShouldCollideLocked(const JPH::Body& body) const override {
            if (body.GetID() == floorId_) {
                return false;
            }
            if (skipMesh_ == (std::numeric_limits<std::size_t>::max)()) {
                return true;
            }
            return static_cast<std::size_t>(body.GetUserData()) != skipMesh_;
        }

    private:
        JPH::BodyID floorId_;
        std::size_t skipMesh_;
    };

    bool CastShapeTrace(std::vector<HitResult>& outHits, const glm::vec3& start,
                        const glm::vec3& end, const JPH::Shape* shape, ECollisionChannel channel,
                        std::size_t skipLevelMeshIndex, float inflateHint) {
        outHits.clear();
        if (shape == nullptr) {
            return false;
        }
        const JPH::Vec3 direction(end.x - start.x, end.y - start.y, end.z - start.z);
        const JPH::RShapeCast shapeCast = JPH::RShapeCast::sFromWorldTransform(
            shape, JPH::Vec3::sOne(),
            JPH::RMat44::sTranslation(JPH::RVec3(start.x, start.y, start.z)), direction);

        JPH::AllHitCollisionCollector<JPH::CastShapeCollector> collector;
        JPH::ShapeCastSettings settings;
        ChannelObjectLayerFilter layerFilter(channel);
        TraceBodyFilter bodyFilter(floorId_, skipLevelMeshIndex);
        physicsSystem_.GetNarrowPhaseQuery().CastShape(shapeCast, settings, JPH::RVec3::sZero(),
                                                       collector, {}, layerFilter, bodyFilter);
        collector.Sort();

        const JPH::BodyLockInterface& locks = physicsSystem_.GetBodyLockInterface();
        for (const JPH::ShapeCastResult& hit : collector.mHits) {
            JPH::BodyLockRead lock(locks, hit.mBodyID2);
            if (!lock.Succeeded()) {
                continue;
            }
            const JPH::Body& body = lock.GetBody();
            const JPH::RVec3 point = shapeCast.mCenterOfMassStart.GetTranslation() +
                                    (direction * hit.mFraction);
            // Contact on world body (base offset Zero → world space); Location = sweep COM.
            const JPH::Vec3 contact = hit.mContactPointOn2;
            JPH::Vec3 normal = -hit.mPenetrationAxis;
            if (normal.LengthSq() > 1.0e-8f) {
                normal = normal.Normalized();
            } else {
                normal = JPH::Vec3(0, 1, 0);
            }
            const JPH::Vec3 toStart =
                JPH::Vec3(start.x, start.y, start.z) - JPH::Vec3(point.GetX(), point.GetY(), point.GetZ());
            if (normal.Dot(toStart) < 0.0f) {
                normal = -normal;
            }
            HitResult out{};
            out.bBlockingHit = true;
            out.Time = hit.mFraction;
            out.Distance = direction.Length() * hit.mFraction;
            out.Location = {point.GetX(), point.GetY(), point.GetZ()};
            out.ImpactPoint = {contact.GetX(), contact.GetY(), contact.GetZ()};
            out.ImpactNormal = {normal.GetX(), normal.GetY(), normal.GetZ()};
            out.TraceStart = start;
            out.TraceEnd = end;
            out.LevelMeshIndex = static_cast<std::size_t>(body.GetUserData());
            out.bFloorPlane = false;
            (void)inflateHint;
            outHits.push_back(out);
        }
        return !outHits.empty();
    }

    static void EnsureJoltTypes() {
        static bool s_ready = false;
        if (s_ready) {
            return;
        }
        s_ready = true;
        JPH::RegisterDefaultAllocator();
        JPH::Trace = TraceImpl;
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }

    static void DestroyBody(JPH::BodyInterface& iface, JPH::BodyID id) {
        if (id.IsInvalid()) {
            return;
        }
        iface.RemoveBody(id);
        iface.DestroyBody(id);
    }

    [[nodiscard]] JPH::BodyID CreateBody(JPH::BodyInterface& iface, const BodyInstance& src,
                                         std::size_t /*index*/,
                                         const TriangleMeshCollision* triMesh) const {
        JPH::ShapeRefC shape;
        JPH::RVec3 bodyPos(src.position.x, src.position.y, src.position.z);

        if (src.type == EBodyType::Static && src.collisionShape == ECollisionShape::TriangleMesh &&
            triMesh != nullptr && triMesh->IsValid()) {
            // Local-space cook (origin = Leon body/AABB center) keeps COM at the body position.
            shape = CreateMeshShape(*triMesh, src.position);
        }
        if (shape == nullptr) {
            // Near-flat AABBs (zero Y from a plane mesh) need thickness > convex radius.
            glm::vec3 he = src.halfExtents;
            he.x = std::max(he.x, 0.05f);
            he.y = std::max(he.y, 0.05f);
            he.z = std::max(he.z, 0.05f);
            shape = CreateBoxShape(he);
            bodyPos = JPH::RVec3(src.position.x, src.position.y, src.position.z);
        }
        if (shape == nullptr) {
            return JPH::BodyID();
        }

        const bool dynamic = src.type == EBodyType::Dynamic;
        JPH::BodyCreationSettings settings(
            shape, bodyPos, JPH::Quat::sIdentity(),
            dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
            dynamic ? Layers::MOVING : Layers::NON_MOVING);
        settings.mUserData = static_cast<JPH::uint64>(src.levelMeshIndex);
        if (dynamic) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = std::max(src.mass, 0.5f);
            if (!src.enableGravity) {
                settings.mGravityFactor = 0.0f;
            }
        }

        const JPH::BodyID id = iface.CreateAndAddBody(
            settings, dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
        if (dynamic && !id.IsInvalid()) {
            iface.SetLinearVelocity(id, JPH::Vec3(src.velXZ.x, src.velocityY, src.velXZ.y));
        }
        return id;
    }

    void DestroyFloor(JPH::BodyInterface& iface) {
        if (!floorId_.IsInvalid()) {
            DestroyBody(iface, floorId_);
            floorId_ = JPH::BodyID();
        }
    }

    void EnsureFloor(float floorY) {
        if (!floorId_.IsInvalid() && std::isfinite(floorY_) &&
            std::abs(floorY - floorY_) < 1.0e-4f) {
            return;
        }
        JPH::BodyInterface& iface = physicsSystem_.GetBodyInterface();
        DestroyFloor(iface);
        // Thin static slab under the world floor (Leon floorY is the support plane).
        constexpr float kHalfThickness = 0.5f;
        JPH::ShapeRefC shape = CreateBoxShape({500.0f, kHalfThickness, 500.0f});
        if (shape == nullptr) {
            return;
        }
        JPH::BodyCreationSettings settings(
            shape, JPH::RVec3(0.0f, floorY - kHalfThickness, 0.0f), JPH::Quat::sIdentity(),
            JPH::EMotionType::Static, Layers::NON_MOVING);
        floorId_ = iface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        floorY_ = floorY;
    }

    BPLayerInterfaceImpl broadPhaseLayerInterface_;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphaseLayerFilter_;
    ObjectLayerPairFilterImpl objectVsObjectLayerFilter_;
    JPH::PhysicsSystem physicsSystem_;
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator_;
    std::unique_ptr<JPH::JobSystemSingleThreaded> jobSystem_;
    std::vector<JPH::BodyID> bodyIds_;
    JPH::BodyID floorId_{};
    std::size_t lastSkip_ = (std::numeric_limits<std::size_t>::max)();
    float floorY_ = std::numeric_limits<float>::quiet_NaN();
};

} // namespace

std::unique_ptr<IPhysicsBackend> CreateJoltPhysicsBackend() {
    return std::make_unique<JoltPhysicsBackend>();
}

