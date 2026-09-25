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
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <limits>
#include <vector>

JPH_SUPPRESS_WARNINGS

namespace
{

	void TraceImpl(const char* Fmt, ...)
	{
		char Buffer[1024];
		va_list Args;
		va_start(Args, Fmt);
		std::vsnprintf(Buffer, sizeof(Buffer), Fmt, Args);
		va_end(Args);
		std::cerr << Buffer << '\n';
	}

	namespace Layers
	{
		constexpr JPH::ObjectLayer NonMoving = 0;
		constexpr JPH::ObjectLayer MOVING = 1;
		constexpr JPH::ObjectLayer NumLayers = 2;
	} // namespace Layers

	namespace BroadPhaseLayers
	{
		constexpr JPH::BroadPhaseLayer NonMoving(0);
		constexpr JPH::BroadPhaseLayer MOVING(1);
		constexpr JPH::uint NumLayers = 2;
	} // namespace BroadPhaseLayers

	class FObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
	{
	public:
		[[nodiscard]] bool ShouldCollide(JPH::ObjectLayer A, JPH::ObjectLayer B) const override
		{
			switch (A)
			{
				case Layers::NonMoving:
					return B == Layers::MOVING;
				case Layers::MOVING:
					return true;
				default:
					return false;
			}
		}
	};

	class FBPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
	{
	public:
		FBPLayerInterfaceImpl()
		{
			ObjectToBroadPhase[Layers::NonMoving] = BroadPhaseLayers::NonMoving;
			ObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}

		[[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NumLayers;
		}

		[[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer Layer) const override
		{
			return ObjectToBroadPhase[Layer];
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		[[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
		{
			switch ((JPH::BroadPhaseLayer::Type)layer)
			{
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
		JPH::BroadPhaseLayer ObjectToBroadPhase[Layers::NumLayers];
	};

	class FObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		[[nodiscard]] bool ShouldCollide(JPH::ObjectLayer Layer, JPH::BroadPhaseLayer Bp) const override
		{
			switch (Layer)
			{
				case Layers::NonMoving:
					return Bp == BroadPhaseLayers::MOVING;
				case Layers::MOVING:
					return true;
				default:
					return false;
			}
		}
	};

	[[nodiscard]] JPH::ShapeRefC CreateBoxShape(const FVector& HalfExtents)
	{
		// Half-extents must exceed convex radius or BoxShapeSettings::Create fails.
		constexpr float ConvexRadius = 0.001f;
		constexpr float MinHalf = 0.002f;
		const float Hx = std::max(HalfExtents.X, MinHalf);
		const float Hy = std::max(HalfExtents.Y, MinHalf);
		const float Hz = std::max(HalfExtents.Z, MinHalf);
		JPH::BoxShapeSettings ShapeSettings(JPH::Vec3(Hx, Hy, Hz), ConvexRadius);
		ShapeSettings.SetEmbedded();
		JPH::ShapeSettings::ShapeResult ShapeResult = ShapeSettings.Create();
		if (ShapeResult.HasError())
		{
			return nullptr;
		}
		return ShapeResult.Get();
	}

	/// Bake FTriangleMeshCollision into a MeshShape in body-local space (origin = body.position).
	[[nodiscard]] JPH::ShapeRefC CreateMeshShape(const FTriangleMeshCollision& Mesh, const FVector& BodyPosition)
	{
		if (!Mesh.IsValid())
		{
			return nullptr;
		}
		JPH::TriangleList Tris;
		Tris.reserve(static_cast<std::size_t>(Mesh.Indices.Num() / 3));
		const uint32 VertexCount = static_cast<uint32>(Mesh.Positions.Num());
		for (int32 I = 0; I + 2 < Mesh.Indices.Num(); I += 3)
		{
			const uint32 I0 = Mesh.Indices[I];
			const uint32 I1 = Mesh.Indices[I + 1];
			const uint32 I2 = Mesh.Indices[I + 2];
			if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
			{
				continue;
			}
			const FVector P0 = Mesh.Positions[static_cast<int32>(I0)] - BodyPosition;
			const FVector P1 = Mesh.Positions[static_cast<int32>(I1)] - BodyPosition;
			const FVector P2 = Mesh.Positions[static_cast<int32>(I2)] - BodyPosition;
			// Emit both windings so single-sided MeshShape collides from either side (floors/ceilings).
			Tris.push_back(
				JPH::Triangle(JPH::Vec3(P0.X, P0.Y, P0.Z), JPH::Vec3(P1.X, P1.Y, P1.Z), JPH::Vec3(P2.X, P2.Y, P2.Z)));
			Tris.push_back(
				JPH::Triangle(JPH::Vec3(P0.X, P0.Y, P0.Z), JPH::Vec3(P2.X, P2.Y, P2.Z), JPH::Vec3(P1.X, P1.Y, P1.Z)));
		}
		if (Tris.empty())
		{
			return nullptr;
		}
		// Heap settings: MeshShape retains cooked data; stack+SetEmbedded is fragile for meshes.
		JPH::Ref<JPH::MeshShapeSettings> MeshSettings = new JPH::MeshShapeSettings(Tris);
		JPH::ShapeSettings::ShapeResult ShapeResult = MeshSettings->Create();
		if (ShapeResult.HasError())
		{
			return nullptr;
		}
		return ShapeResult.Get();
	}

	class FJoltPhysicsBackend final : public IPhysicsBackend
	{
	public:
		FJoltPhysicsBackend()
		{
			EnsureJoltTypes();
			TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(4 * 1024 * 1024);
			JobSystem = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

			constexpr JPH::uint MaxBodies = 4096;
			constexpr JPH::uint MaxBodyPairs = 4096;
			constexpr JPH::uint MaxContactConstraints = 4096;
			PhysicsSystem.Init(MaxBodies, 0, MaxBodyPairs, MaxContactConstraints, BroadPhaseLayerInterface,
				ObjectVsBroadphaseLayerFilter, ObjectVsObjectLayerFilter);
		}

		~FJoltPhysicsBackend() override
		{
			RigidClear();
		}

		[[nodiscard]] const TCHAR* GetName() const override
		{
			return "Jolt";
		}
		[[nodiscard]] bool HasRigidWorld() const override
		{
			return true;
		}
		[[nodiscard]] bool HasNarrowPhaseTraces() const override
		{
			return true;
		}

		void RigidClear() override
		{
			JPH::BodyInterface& Bodies = PhysicsSystem.GetBodyInterface();
			for (const JPH::BodyID& Id : BodyIds)
			{
				DestroyBody(Bodies, Id);
			}
			BodyIds.clear();
			DestroyFloor(Bodies);
			LastSkip = NoLevelMeshIndex;
			FloorY = std::numeric_limits<float>::quiet_NaN();
		}

		void RigidRebuild(const TArray<FBodyInstance>& Bodies, const TArray<FTriangleMeshCollision>* TriangleMeshes,
			SIZE_T SkipLevelMeshIndex) override
		{
			RigidClear();
			BodyIds.assign(static_cast<std::size_t>(Bodies.Num()), JPH::BodyID());
			LastSkip = SkipLevelMeshIndex;

			JPH::BodyInterface& Iface = PhysicsSystem.GetBodyInterface();
			for (int32 I = 0; I < Bodies.Num(); ++I)
			{
				if (Bodies[I].LevelMeshIndex == SkipLevelMeshIndex)
				{
					continue;
				}
				const FTriangleMeshCollision* Tri =
					(TriangleMeshes != nullptr && I < TriangleMeshes->Num()) ? &(*TriangleMeshes)[I] : nullptr;
				BodyIds[static_cast<std::size_t>(I)] = CreateBody(Iface, Bodies[I], Tri);
			}
			PhysicsSystem.OptimizeBroadPhase();
		}

		void RigidPrepareStep(const TArray<FBodyInstance>& Bodies, SIZE_T SkipLevelMeshIndex) override
		{
			// Structure changed outside SyncFromLevel — rebuild as boxes (meshes need SyncFromLevel).
			if (BodyIds.size() != static_cast<std::size_t>(Bodies.Num()))
			{
				RigidRebuild(Bodies, nullptr, SkipLevelMeshIndex);
				return;
			}

			JPH::BodyInterface& Iface = PhysicsSystem.GetBodyInterface();
			LastSkip = SkipLevelMeshIndex;

			for (std::size_t I = 0; I < BodyIds.size(); ++I)
			{
				const FBodyInstance& Src = Bodies[static_cast<int32>(I)];
				const bool bSkip = Src.LevelMeshIndex == SkipLevelMeshIndex;

				if (bSkip)
				{
					if (!BodyIds[I].IsInvalid())
					{
						DestroyBody(Iface, BodyIds[I]);
						BodyIds[I] = JPH::BodyID();
					}
					continue;
				}

				if (BodyIds[I].IsInvalid())
				{
					BodyIds[I] = CreateBody(Iface, Src, nullptr);
					continue;
				}

				if (Src.Type != EBodyType::Dynamic)
				{
					continue;
				}

				// CMC / ResolveCapsuleSides may have nudged FBodyInstance state — push into Jolt.
				Iface.SetPosition(
					BodyIds[I], JPH::RVec3(Src.Position.X, Src.Position.Y, Src.Position.Z), JPH::EActivation::Activate);
				Iface.SetLinearVelocity(BodyIds[I], JPH::Vec3(Src.VelXz.X, Src.VelocityY, Src.VelXz.Y));
			}
		}

		void RigidStep(float DeltaTime, float GravityMagnitude, float InFloorY) override
		{
			if (DeltaTime <= 0.0f)
			{
				return;
			}
			EnsureFloor(InFloorY);
			PhysicsSystem.SetGravity(JPH::Vec3(0.0f, -std::abs(GravityMagnitude), 0.0f));

			const int CollisionSteps = std::max(1, static_cast<int>(std::ceil(DeltaTime * 60.0f)));
			PhysicsSystem.Update(DeltaTime, CollisionSteps, TempAllocator.get(), JobSystem.get());
		}

		void RigidReadBack(TArray<FBodyInstance>& Bodies) override
		{
			JPH::BodyInterface& Iface = PhysicsSystem.GetBodyInterface();
			const int32 N = FMath::Min(Bodies.Num(), static_cast<int32>(BodyIds.size()));
			for (int32 I = 0; I < N; ++I)
			{
				const JPH::BodyID Id = BodyIds[static_cast<std::size_t>(I)];
				if (Id.IsInvalid() || Bodies[I].Type != EBodyType::Dynamic)
				{
					continue;
				}
				const JPH::RVec3 Pos = Iface.GetCenterOfMassPosition(Id);
				const JPH::Vec3 Vel = Iface.GetLinearVelocity(Id);
				Bodies[I].Position = FVector(Pos.GetX(), Pos.GetY(), Pos.GetZ());
				Bodies[I].VelXz = FVector2D(Vel.GetX(), Vel.GetZ());
				Bodies[I].VelocityY = Vel.GetY();
			}
		}

		bool RigidLineTrace(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
			ECollisionChannel InChannel, SIZE_T SkipLevelMeshIndex) override
		{
			OutHits.Reset();
			const JPH::Vec3 Origin(Start.X, Start.Y, Start.Z);
			const JPH::Vec3 Direction(End.X - Start.X, End.Y - Start.Y, End.Z - Start.Z);
			const JPH::RRayCast Ray(Origin, Direction);

			JPH::AllHitCollisionCollector<JPH::CastRayCollector> Collector;
			JPH::RayCastSettings Settings;
			FChannelObjectLayerFilter LayerFilter(InChannel);
			FTraceBodyFilter BodyFilter(FloorId, SkipLevelMeshIndex);
			PhysicsSystem.GetNarrowPhaseQuery().CastRay(Ray, Settings, Collector, {}, LayerFilter, BodyFilter);
			Collector.Sort();

			const JPH::BodyLockInterface& Locks = PhysicsSystem.GetBodyLockInterface();
			for (const JPH::RayCastResult& Hit : Collector.mHits)
			{
				JPH::BodyLockRead Lock(Locks, Hit.mBodyID);
				if (!Lock.Succeeded())
				{
					continue;
				}
				const JPH::Body& Body = Lock.GetBody();
				const JPH::RVec3 Point = Ray.GetPointOnRay(Hit.mFraction);
				JPH::Vec3 Normal = Body.GetWorldSpaceSurfaceNormal(Hit.mSubShapeID2, Point);
				if (Normal.LengthSq() < 1.0e-8f)
				{
					Normal = JPH::Vec3(0, 1, 0);
				}
				else
				{
					Normal = Normal.Normalized();
				}
				// Point normal toward the trace start (Unreal ImpactNormal convention).
				const JPH::Vec3 ToStart = Origin - Point;
				if (Normal.Dot(ToStart) < 0.0f)
				{
					Normal = -Normal;
				}
				FHitResult Out;
				Out.bBlockingHit = true;
				Out.Time = Hit.mFraction;
				Out.Distance = Direction.Length() * Hit.mFraction;
				Out.Location = FVector(Point.GetX(), Point.GetY(), Point.GetZ());
				Out.ImpactPoint = Out.Location;
				Out.ImpactNormal = FVector(Normal.GetX(), Normal.GetY(), Normal.GetZ());
				Out.TraceStart = Start;
				Out.TraceEnd = End;
				Out.LevelMeshIndex = static_cast<SIZE_T>(Body.GetUserData());
				Out.bFloorPlane = false;
				OutHits.Add(Out);
			}
			return OutHits.Num() > 0;
		}

		bool RigidSphereTrace(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, float Radius,
			ECollisionChannel InChannel, SIZE_T SkipLevelMeshIndex) override
		{
			const float R = std::max(Radius, 1.0e-3f);
			JPH::RefConst<JPH::SphereShape> Sphere = new JPH::SphereShape(R);
			return CastShapeTrace(OutHits, Start, End, Sphere, InChannel, SkipLevelMeshIndex, R);
		}

		bool RigidCapsuleTrace(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, float Radius,
			float HalfHeight, ECollisionChannel InChannel, SIZE_T SkipLevelMeshIndex) override
		{
			const float R = std::max(Radius, 1.0e-3f);
			const float Hh = std::max(HalfHeight, 0.0f);
			JPH::RefConst<JPH::CapsuleShape> Capsule = new JPH::CapsuleShape(Hh, R);
			return CastShapeTrace(OutHits, Start, End, Capsule, InChannel, SkipLevelMeshIndex, R + Hh);
		}

	private:
		class FChannelObjectLayerFilter final : public JPH::ObjectLayerFilter
		{
		public:
			explicit FChannelObjectLayerFilter(ECollisionChannel InChannel)
				: Channel(InChannel)
			{
			}
			[[nodiscard]] bool ShouldCollide(JPH::ObjectLayer Layer) const override
			{
				switch (Channel)
				{
					case ECollisionChannel::WorldStatic:
						return Layer == Layers::NonMoving;
					case ECollisionChannel::WorldDynamic:
						return Layer == Layers::MOVING;
					case ECollisionChannel::Pawn:
					case ECollisionChannel::Visibility:
						return true;
				}
				return true;
			}

		private:
			ECollisionChannel Channel;
		};

		class FTraceBodyFilter final : public JPH::BodyFilter
		{
		public:
			FTraceBodyFilter(JPH::BodyID InFloorId, SIZE_T InSkipMesh)
				: FloorId(InFloorId)
				, SkipMesh(InSkipMesh)
			{
			}

			[[nodiscard]] bool ShouldCollide(const JPH::BodyID& Id) const override
			{
				return Id != FloorId;
			}

			[[nodiscard]] bool ShouldCollideLocked(const JPH::Body& Body) const override
			{
				if (Body.GetID() == FloorId)
				{
					return false;
				}
				if (SkipMesh == NoLevelMeshIndex)
				{
					return true;
				}
				return static_cast<SIZE_T>(Body.GetUserData()) != SkipMesh;
			}

		private:
			JPH::BodyID FloorId;
			SIZE_T SkipMesh;
		};

		bool CastShapeTrace(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
			const JPH::Shape* Shape, ECollisionChannel InChannel, SIZE_T SkipLevelMeshIndex, float InflateHint)
		{
			OutHits.Reset();
			if (Shape == nullptr)
			{
				return false;
			}
			const JPH::Vec3 Direction(End.X - Start.X, End.Y - Start.Y, End.Z - Start.Z);
			const JPH::RShapeCast ShapeCast = JPH::RShapeCast::sFromWorldTransform(
				Shape, JPH::Vec3::sOne(), JPH::RMat44::sTranslation(JPH::RVec3(Start.X, Start.Y, Start.Z)), Direction);

			JPH::AllHitCollisionCollector<JPH::CastShapeCollector> Collector;
			JPH::ShapeCastSettings Settings;
			FChannelObjectLayerFilter LayerFilter(InChannel);
			FTraceBodyFilter BodyFilter(FloorId, SkipLevelMeshIndex);
			PhysicsSystem.GetNarrowPhaseQuery().CastShape(
				ShapeCast, Settings, JPH::RVec3::sZero(), Collector, {}, LayerFilter, BodyFilter);
			Collector.Sort();

			const JPH::BodyLockInterface& Locks = PhysicsSystem.GetBodyLockInterface();
			for (const JPH::ShapeCastResult& Hit : Collector.mHits)
			{
				JPH::BodyLockRead Lock(Locks, Hit.mBodyID2);
				if (!Lock.Succeeded())
				{
					continue;
				}
				const JPH::Body& Body = Lock.GetBody();
				const JPH::RVec3 Point = ShapeCast.mCenterOfMassStart.GetTranslation() + (Direction * Hit.mFraction);
				// Contact on world body (base offset Zero → world space); Location = sweep COM.
				const JPH::Vec3 Contact = Hit.mContactPointOn2;
				JPH::Vec3 Normal = -Hit.mPenetrationAxis;
				if (Normal.LengthSq() > 1.0e-8f)
				{
					Normal = Normal.Normalized();
				}
				else
				{
					Normal = JPH::Vec3(0, 1, 0);
				}
				const JPH::Vec3 ToStart =
					JPH::Vec3(Start.X, Start.Y, Start.Z) - JPH::Vec3(Point.GetX(), Point.GetY(), Point.GetZ());
				if (Normal.Dot(ToStart) < 0.0f)
				{
					Normal = -Normal;
				}
				FHitResult Out;
				Out.bBlockingHit = true;
				Out.Time = Hit.mFraction;
				Out.Distance = Direction.Length() * Hit.mFraction;
				Out.Location = FVector(Point.GetX(), Point.GetY(), Point.GetZ());
				Out.ImpactPoint = FVector(Contact.GetX(), Contact.GetY(), Contact.GetZ());
				Out.ImpactNormal = FVector(Normal.GetX(), Normal.GetY(), Normal.GetZ());
				Out.TraceStart = Start;
				Out.TraceEnd = End;
				Out.LevelMeshIndex = static_cast<SIZE_T>(Body.GetUserData());
				Out.bFloorPlane = false;
				(void)InflateHint;
				OutHits.Add(Out);
			}
			return OutHits.Num() > 0;
		}

		static void EnsureJoltTypes()
		{
			static bool bSReady = false;
			if (bSReady)
			{
				return;
			}
			bSReady = true;
			JPH::RegisterDefaultAllocator();
			JPH::Trace = TraceImpl;
			JPH::Factory::sInstance = new JPH::Factory();
			JPH::RegisterTypes();
		}

		static void DestroyBody(JPH::BodyInterface& Iface, JPH::BodyID Id)
		{
			if (Id.IsInvalid())
			{
				return;
			}
			Iface.RemoveBody(Id);
			Iface.DestroyBody(Id);
		}

		[[nodiscard]] JPH::BodyID CreateBody(
			JPH::BodyInterface& Iface, const FBodyInstance& Src, const FTriangleMeshCollision* TriMesh) const
		{
			JPH::ShapeRefC Shape;
			JPH::RVec3 BodyPos(Src.Position.X, Src.Position.Y, Src.Position.Z);

			if (Src.Type == EBodyType::Static && Src.CollisionShape == EBodyCollisionShape::TriangleMesh &&
				TriMesh != nullptr && TriMesh->IsValid())
			{
				// Local-space cook (origin = Leon body/AABB center) keeps COM at the body position.
				Shape = CreateMeshShape(*TriMesh, Src.Position);
			}
			if (Shape == nullptr)
			{
				// Near-flat AABBs (zero Y from a plane mesh) need thickness > convex radius.
				FVector He = Src.HalfExtents;
				He.X = std::max(He.X, 0.05f);
				He.Y = std::max(He.Y, 0.05f);
				He.Z = std::max(He.Z, 0.05f);
				Shape = CreateBoxShape(He);
				BodyPos = JPH::RVec3(Src.Position.X, Src.Position.Y, Src.Position.Z);
			}
			if (Shape == nullptr)
			{
				return JPH::BodyID();
			}

			const bool bDynamic = Src.Type == EBodyType::Dynamic;
			JPH::BodyCreationSettings Settings(Shape, BodyPos, JPH::Quat::sIdentity(),
				bDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
				bDynamic ? Layers::MOVING : Layers::NonMoving);
			Settings.mUserData = static_cast<JPH::uint64>(Src.LevelMeshIndex);
			if (bDynamic)
			{
				Settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
				Settings.mMassPropertiesOverride.mMass = std::max(Src.Mass, 0.5f);
				if (!Src.bEnableGravity)
				{
					Settings.mGravityFactor = 0.0f;
				}
			}

			const JPH::BodyID Id = Iface.CreateAndAddBody(
				Settings, bDynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
			if (bDynamic && !Id.IsInvalid())
			{
				Iface.SetLinearVelocity(Id, JPH::Vec3(Src.VelXz.X, Src.VelocityY, Src.VelXz.Y));
			}
			return Id;
		}

		void DestroyFloor(JPH::BodyInterface& Iface)
		{
			if (!FloorId.IsInvalid())
			{
				DestroyBody(Iface, FloorId);
				FloorId = JPH::BodyID();
			}
		}

		void EnsureFloor(float InFloorY)
		{
			if (!FloorId.IsInvalid() && std::isfinite(FloorY) && std::abs(InFloorY - FloorY) < 1.0e-4f)
			{
				return;
			}
			JPH::BodyInterface& Iface = PhysicsSystem.GetBodyInterface();
			DestroyFloor(Iface);
			// Thin static slab under the world floor (Leon floorY is the support plane).
			constexpr float HalfThickness = 0.5f;
			JPH::ShapeRefC Shape = CreateBoxShape(FVector(500.0f, HalfThickness, 500.0f));
			if (Shape == nullptr)
			{
				return;
			}
			JPH::BodyCreationSettings Settings(Shape, JPH::RVec3(0.0f, InFloorY - HalfThickness, 0.0f),
				JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::NonMoving);
			FloorId = Iface.CreateAndAddBody(Settings, JPH::EActivation::DontActivate);
			FloorY = InFloorY;
		}

		FBPLayerInterfaceImpl BroadPhaseLayerInterface;
		FObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadphaseLayerFilter;
		FObjectLayerPairFilterImpl ObjectVsObjectLayerFilter;
		JPH::PhysicsSystem PhysicsSystem;
		std::unique_ptr<JPH::TempAllocatorImpl> TempAllocator;
		std::unique_ptr<JPH::JobSystemSingleThreaded> JobSystem;
		std::vector<JPH::BodyID> BodyIds;
		JPH::BodyID FloorId{};
		SIZE_T LastSkip = NoLevelMeshIndex;
		float FloorY = std::numeric_limits<float>::quiet_NaN();
	};

} // namespace

TUniquePtr<IPhysicsBackend> CreateJoltPhysicsBackend()
{
	return MakeUnique<FJoltPhysicsBackend>();
}
