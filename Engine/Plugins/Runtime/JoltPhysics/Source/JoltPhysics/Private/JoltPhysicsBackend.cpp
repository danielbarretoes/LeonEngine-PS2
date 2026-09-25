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

#include <cstdarg>

JPH_SUPPRESS_WARNINGS

DEFINE_LOG_CATEGORY_STATIC(LogJolt, Log, All);

namespace
{

	/** Jolt's trace hook, routed to the log. */
	void TraceImpl(const char* Fmt, ...)
	{
		ANSICHAR Buffer[1024];
		va_list Args;
		va_start(Args, Fmt);
		(void)FCStringAnsi::GetVarArgs(Buffer, sizeof(Buffer), Fmt, Args);
		va_end(Args);
		UE_LOG(LogJolt, Log, "%s", Buffer);
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

	/**
	 * Jolt works in metres (its tolerances are tuned for them); the engine world is in centimetres. Every position,
	 * extent, velocity and acceleration is scaled at this boundary; Jolt-side constants below stay in metres.
	 */
	constexpr float JoltMetresPerUnit = 0.01f;
	constexpr float UnitsPerJoltMetre = 100.0f;

	[[nodiscard]] float ToJoltLength(float WorldLength)
	{
		return WorldLength * JoltMetresPerUnit;
	}

	[[nodiscard]] JPH::Vec3 ToJoltVec3(const FVector& World)
	{
		return JPH::Vec3(World.X * JoltMetresPerUnit, World.Y * JoltMetresPerUnit, World.Z * JoltMetresPerUnit);
	}

	[[nodiscard]] JPH::RVec3 ToJoltRVec3(const FVector& World)
	{
		return JPH::RVec3(World.X * JoltMetresPerUnit, World.Y * JoltMetresPerUnit, World.Z * JoltMetresPerUnit);
	}

	/** A body instance's velocity (horizontal X / Z and vertical Y, cm/s) in Jolt metres per second. */
	[[nodiscard]] JPH::Vec3 ToJoltVelocity(const FBodyInstance& Body)
	{
		return ToJoltVec3(FVector(Body.VelXz.X, Body.VelocityY, Body.VelXz.Y));
	}

	[[nodiscard]] FVector FromJolt(const JPH::Vec3& Jolt)
	{
		return FVector(Jolt.GetX(), Jolt.GetY(), Jolt.GetZ()) * UnitsPerJoltMetre;
	}

	/** Half extents in Jolt metres. */
	[[nodiscard]] JPH::ShapeRefC CreateBoxShape(const FVector& HalfExtents)
	{
		// Half-extents must exceed convex radius or BoxShapeSettings::Create fails.
		constexpr float ConvexRadius = 0.001f;
		constexpr float MinHalf = 0.002f;
		const float Hx = FMath::Max(HalfExtents.X, MinHalf);
		const float Hy = FMath::Max(HalfExtents.Y, MinHalf);
		const float Hz = FMath::Max(HalfExtents.Z, MinHalf);
		JPH::BoxShapeSettings ShapeSettings(JPH::Vec3(Hx, Hy, Hz), ConvexRadius);
		ShapeSettings.SetEmbedded();
		JPH::ShapeSettings::ShapeResult ShapeResult = ShapeSettings.Create();
		if (ShapeResult.HasError())
		{
			return nullptr;
		}
		return ShapeResult.Get();
	}

	/// Bake FTriangleMeshCollision (cm) into a MeshShape in body-local space (origin = body.position), in metres.
	[[nodiscard]] JPH::ShapeRefC CreateMeshShape(const FTriangleMeshCollision& Mesh, const FVector& BodyPosition)
	{
		if (!Mesh.IsValid())
		{
			return nullptr;
		}
		JPH::TriangleList Tris;
		Tris.reserve(static_cast<size_t>(Mesh.Indices.Num() / 3));
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
			const JPH::Vec3 P0 = ToJoltVec3(Mesh.Positions[static_cast<int32>(I0)] - BodyPosition);
			const JPH::Vec3 P1 = ToJoltVec3(Mesh.Positions[static_cast<int32>(I1)] - BodyPosition);
			const JPH::Vec3 P2 = ToJoltVec3(Mesh.Positions[static_cast<int32>(I2)] - BodyPosition);
			// Emit both windings so single-sided MeshShape collides from either side (floors/ceilings).
			Tris.push_back(JPH::Triangle(P0, P1, P2));
			Tris.push_back(JPH::Triangle(P0, P2, P1));
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
			TempAllocator = MakeUnique<JPH::TempAllocatorImpl>(4 * 1024 * 1024);
			JobSystem = MakeUnique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

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
			BodyIds.Empty();
			DestroyFloor(Bodies);
			LastSkip = NoLevelMeshIndex;
			bHasFloorY = false;
		}

		void RigidRebuild(const TArray<FBodyInstance>& Bodies, const TArray<FTriangleMeshCollision>* TriangleMeshes,
			SIZE_T SkipLevelMeshIndex) override
		{
			RigidClear();
			BodyIds.Init(JPH::BodyID(), Bodies.Num());
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
				BodyIds[I] = CreateBody(Iface, Bodies[I], Tri);
			}
			PhysicsSystem.OptimizeBroadPhase();
		}

		void RigidPrepareStep(const TArray<FBodyInstance>& Bodies, SIZE_T SkipLevelMeshIndex) override
		{
			// Structure changed outside SyncFromLevel — rebuild as boxes (meshes need SyncFromLevel).
			if (BodyIds.Num() != Bodies.Num())
			{
				RigidRebuild(Bodies, nullptr, SkipLevelMeshIndex);
				return;
			}

			JPH::BodyInterface& Iface = PhysicsSystem.GetBodyInterface();
			LastSkip = SkipLevelMeshIndex;

			for (int32 I = 0; I < BodyIds.Num(); ++I)
			{
				const FBodyInstance& Src = Bodies[I];
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
				Iface.SetPosition(BodyIds[I], ToJoltRVec3(Src.Position), JPH::EActivation::Activate);
				Iface.SetLinearVelocity(BodyIds[I], ToJoltVelocity(Src));
			}
		}

		void RigidStep(float DeltaTime, float GravityMagnitude, float InFloorY) override
		{
			if (DeltaTime <= 0.0f)
			{
				return;
			}
			EnsureFloor(ToJoltLength(InFloorY));
			PhysicsSystem.SetGravity(JPH::Vec3(0.0f, -FMath::Abs(ToJoltLength(GravityMagnitude)), 0.0f));

			const int32 CollisionSteps = FMath::Max(1, FMath::CeilToInt(DeltaTime * 60.0f));
			PhysicsSystem.Update(DeltaTime, CollisionSteps, TempAllocator.Get(), JobSystem.Get());
		}

		void RigidReadBack(TArray<FBodyInstance>& Bodies) override
		{
			JPH::BodyInterface& Iface = PhysicsSystem.GetBodyInterface();
			const int32 N = FMath::Min(Bodies.Num(), BodyIds.Num());
			for (int32 I = 0; I < N; ++I)
			{
				const JPH::BodyID Id = BodyIds[I];
				if (Id.IsInvalid() || Bodies[I].Type != EBodyType::Dynamic)
				{
					continue;
				}
				const FVector Vel = FromJolt(Iface.GetLinearVelocity(Id));
				Bodies[I].Position = FromJolt(Iface.GetCenterOfMassPosition(Id));
				Bodies[I].VelXz = FVector2D(Vel.X, Vel.Z);
				Bodies[I].VelocityY = Vel.Y;
			}
		}

		bool RigidLineTrace(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
			ECollisionChannel InChannel, SIZE_T SkipLevelMeshIndex) override
		{
			OutHits.Reset();
			const JPH::Vec3 Origin = ToJoltVec3(Start);
			const JPH::Vec3 Direction = ToJoltVec3(End - Start);
			const JPH::RRayCast Ray(Origin, Direction);
			const float TraceLength = (End - Start).Size();

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
				Out.Distance = TraceLength * Hit.mFraction;
				Out.Location = FromJolt(Point);
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
			// Jolt metres from here on.
			const float R = FMath::Max(ToJoltLength(Radius), 1.0e-3f);
			JPH::RefConst<JPH::SphereShape> Sphere = new JPH::SphereShape(R);
			return CastShapeTrace(OutHits, Start, End, Sphere, InChannel, SkipLevelMeshIndex, R);
		}

		bool RigidCapsuleTrace(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, float Radius,
			float HalfHeight, ECollisionChannel InChannel, SIZE_T SkipLevelMeshIndex) override
		{
			// Jolt metres from here on.
			const float R = FMath::Max(ToJoltLength(Radius), 1.0e-3f);
			const float Hh = FMath::Max(ToJoltLength(HalfHeight), 0.0f);
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
			const JPH::Vec3 Direction = ToJoltVec3(End - Start);
			const JPH::Vec3 JoltStart = ToJoltVec3(Start);
			const float TraceLength = (End - Start).Size();
			const JPH::RShapeCast ShapeCast = JPH::RShapeCast::sFromWorldTransform(
				Shape, JPH::Vec3::sOne(), JPH::RMat44::sTranslation(ToJoltRVec3(Start)), Direction);

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
				const JPH::Vec3 ToStart = JoltStart - JPH::Vec3(Point.GetX(), Point.GetY(), Point.GetZ());
				if (Normal.Dot(ToStart) < 0.0f)
				{
					Normal = -Normal;
				}
				FHitResult Out;
				Out.bBlockingHit = true;
				Out.Time = Hit.mFraction;
				Out.Distance = TraceLength * Hit.mFraction;
				Out.Location = FromJolt(JPH::Vec3(Point.GetX(), Point.GetY(), Point.GetZ()));
				Out.ImpactPoint = FromJolt(Contact);
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
			const JPH::RVec3 BodyPos = ToJoltRVec3(Src.Position);

			if (Src.Type == EBodyType::Static && Src.CollisionShape == EBodyCollisionShape::TriangleMesh &&
				TriMesh != nullptr && TriMesh->IsValid())
			{
				// Local-space cook (origin = Leon body/AABB center) keeps COM at the body position.
				Shape = CreateMeshShape(*TriMesh, Src.Position);
			}
			if (Shape == nullptr)
			{
				// Near-flat AABBs (zero Y from a plane mesh) need thickness > convex radius (Jolt metres).
				FVector He = Src.HalfExtents * JoltMetresPerUnit;
				He.X = FMath::Max(He.X, 0.05f);
				He.Y = FMath::Max(He.Y, 0.05f);
				He.Z = FMath::Max(He.Z, 0.05f);
				Shape = CreateBoxShape(He);
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
				Settings.mMassPropertiesOverride.mMass = FMath::Max(Src.Mass, 0.5f);
				if (!Src.bEnableGravity)
				{
					Settings.mGravityFactor = 0.0f;
				}
			}

			const JPH::BodyID Id = Iface.CreateAndAddBody(
				Settings, bDynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
			if (bDynamic && !Id.IsInvalid())
			{
				Iface.SetLinearVelocity(Id, ToJoltVelocity(Src));
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

		/** InFloorY in Jolt metres. */
		void EnsureFloor(float InFloorY)
		{
			if (!FloorId.IsInvalid() && bHasFloorY && FMath::Abs(InFloorY - FloorY) < 1.0e-4f)
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
			bHasFloorY = true;
		}

		FBPLayerInterfaceImpl BroadPhaseLayerInterface;
		FObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadphaseLayerFilter;
		FObjectLayerPairFilterImpl ObjectVsObjectLayerFilter;
		JPH::PhysicsSystem PhysicsSystem;
		TUniquePtr<JPH::TempAllocatorImpl> TempAllocator;
		TUniquePtr<JPH::JobSystemSingleThreaded> JobSystem;
		TArray<JPH::BodyID> BodyIds;
		JPH::BodyID FloorId{};
		SIZE_T LastSkip = NoLevelMeshIndex;
		float FloorY = 0.0f;
		bool bHasFloorY = false;
	};

} // namespace

TUniquePtr<IPhysicsBackend> CreateJoltPhysicsBackend()
{
	return MakeUnique<FJoltPhysicsBackend>();
}
