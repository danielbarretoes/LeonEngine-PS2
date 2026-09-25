#include "Physics/PhysScene.h"

#include "Debug/DebugDraw.h"
#include "Frustum.h"
#include "IPhysicsBackend.h"
#include "MeshData.h"
#include "StaticMesh.h"
#include "TriangleCollision.h"

FPhysScene::FPhysScene(EPhysicsBackend InBackend)
	: Backend(InBackend)
	, BackendIface(CreatePhysicsBackend(
		  InBackend == EPhysicsBackend::Jolt ? EPhysicsBackendKind::Jolt : EPhysicsBackendKind::Arcade))
{
	if (BackendIface != nullptr && FCString::Strcmp(BackendIface->GetName(), "Jolt") == 0)
	{
		Backend = EPhysicsBackend::Jolt;
	}
	else
	{
		Backend = EPhysicsBackend::Arcade;
	}
}

namespace
{

	void CancelVelocityInto(FVector2D& Vel, const FVector2D& OutwardNormal)
	{
		const float Into = FVector2D::DotProduct(Vel, -OutwardNormal);
		if (Into > 0.0f)
		{
			Vel += OutwardNormal * Into;
		}
	}

	void CancelVelocityZInto(float& VelocityZ, float OutwardNormalZ)
	{
		if (OutwardNormalZ > 0.5f && VelocityZ < 0.0f)
		{
			VelocityZ = 0.0f;
		}
		else if (OutwardNormalZ < -0.5f && VelocityZ > 0.0f)
		{
			VelocityZ = 0.0f;
		}
	}

	/** A horizontal (XY) ring around Center. */
	void AppendCapsuleRing(
		FDebugDraw& Draw, const FVector& Center, float Radius, const FLinearColor& Color, int32 Segments)
	{
		const float SegCount = static_cast<float>(Segments);
		for (int32 I = 0; I < Segments; ++I)
		{
			const float A0 = (static_cast<float>(I) / SegCount) * 6.2831853f;
			const float A1 = (static_cast<float>(I + 1) / SegCount) * 6.2831853f;
			const FVector P0(FMath::Cos(A0) * Radius, FMath::Sin(A0) * Radius, 0.0f);
			const FVector P1(FMath::Cos(A1) * Radius, FMath::Sin(A1) * Radius, 0.0f);
			Draw.AddLine(Center + P0, Center + P1, Color);
		}
	}

	/** Inset of the capsule disc against an AABB top it stands on (cm). */
	constexpr float SupportDiscInset = -2.0f;

	[[nodiscard]] float LandWindow(float VelocityZ, float InDeltaTime, float InSkin)
	{
		/** cm */
		constexpr float MinLandWindow = 12.0f;
		return FMath::Max(MinLandWindow, (FMath::Abs(VelocityZ) * InDeltaTime) + (InSkin * 4.0f));
	}

	/** Flow: wish into the contact normal gives lateral velocity + an optional contact shove (light props). */
	void ApplyDynamicWishPush(FBodyInstance& Body, const FVector2D& WishN, const FVector2D& InNormal,
		float InPushStrength, float InWalkBounds, bool bApplyContactShove)
	{
		const float Into = FMath::Max(0.0f, -FVector2D::DotProduct(WishN, InNormal));
		if (Into <= 1.0e-4f)
		{
			return;
		}

		constexpr float PlayerMass = 80.0f;
		const float BodyMass = FMath::Max(Body.Mass, 0.5f);
		const float InvMass = 1.0f / BodyMass;
		/** Push velocity per unit strength and kg of the body (cm/s * kg). */
		constexpr float PushScale = 280.0f;
		Body.VelXY += WishN * (Into * InPushStrength * InvMass * PushScale);

		/** cm/s */
		constexpr float MaxPushSpeed = 400.0f;
		const float Speed = Body.VelXY.Size();
		if (Speed > MaxPushSpeed)
		{
			Body.VelXY *= MaxPushSpeed / Speed;
		}

		if (!bApplyContactShove)
		{
			return;
		}

		// Sweep-based contact never overlaps; nudge the body so the walking shove is visible the same frame.
		const float BodyShare = PlayerMass / (PlayerMass + BodyMass);
		/** cm per unit strength */
		constexpr float ContactShove = 6.0f;
		const float Shove = Into * InPushStrength * BodyShare * ContactShove;
		Body.Position.X += WishN.X * Shove;
		Body.Position.Y += WishN.Y * Shove;
		ClampPositionXY(Body.Position, InWalkBounds);
	}

} // namespace

void FPhysScene::Clear()
{
	Bodies.Reset();
	TriangleMeshes.Reset();
	SlopePlanes.Reset();
	if (BackendIface != nullptr)
	{
		BackendIface->RigidClear();
	}
}

int32 FPhysScene::AddBody(const FBodyInstanceDesc& Desc)
{
	FBodyInstance Body;
	Body.LevelMeshIndex = Desc.LevelMeshIndex;
	Body.Type = Desc.Type;
	Body.Mass = Desc.Mass > 0.0f ? Desc.Mass : 0.0f;
	Body.bEnableGravity = Desc.bEnableGravity;
	Bodies.Add(Body);
	TriangleMeshes.AddDefaulted();
	return Bodies.Num() - 1;
}

int32 FPhysScene::AddSlopeRamp(
	const FVector& InBoundsCenter, const FVector& InBoundsHalfExtents, float PitchDegrees, float YawDegrees)
{
	constexpr float DegToRad = 0.01745329251f;
	const float Pitch = PitchDegrees * DegToRad;
	const float S = FMath::Sin(Pitch);
	const float C = FMath::Cos(Pitch);
	FSlopePlane Plane;
	Plane.Point = InBoundsCenter;
	// The surface rises with +X; the unit normal points to the walkable side (Normal.Z = cos(pitch)).
	FVector LocalNormal(-S, 0.0f, C);
	if (FMath::Abs(YawDegrees) > 1.0e-3f)
	{
		// A yaw about Z turns +X toward +Y.
		const float Yaw = YawDegrees * DegToRad;
		const float Cy = FMath::Cos(Yaw);
		const float Sy = FMath::Sin(Yaw);
		LocalNormal =
			FVector(LocalNormal.X * Cy - LocalNormal.Y * Sy, LocalNormal.X * Sy + LocalNormal.Y * Cy, LocalNormal.Z);
	}
	Plane.Normal = LocalNormal.GetSafeNormal();
	Plane.BoundsCenter = InBoundsCenter;
	Plane.BoundsHalfExtents = InBoundsHalfExtents;
	SlopePlanes.Add(Plane);
	return SlopePlanes.Num() - 1;
}

void FPhysScene::SyncFromLevel(const ULevel& Level)
{
	const auto& Meshes = Level.GetStaticMeshes();
	if (TriangleMeshes.Num() != Bodies.Num())
	{
		TriangleMeshes.SetNum(Bodies.Num());
	}
	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		FBodyInstance& Body = Bodies[Bi];
		FTriangleMeshCollision& TriMesh = TriangleMeshes[Bi];
		TriMesh.Clear();
		Body.CollisionShape = EBodyCollisionShape::Box;

		if (Body.LevelMeshIndex >= static_cast<SIZE_T>(Meshes.Num()))
		{
			continue;
		}
		const FLevelStaticMesh& Obj = Meshes[static_cast<int32>(Body.LevelMeshIndex)];
		if (Obj.Mesh != nullptr)
		{
			const FBox WorldAabb =
				TransformLocalBox(Obj.Mesh->GetLocalMin(), Obj.Mesh->GetLocalMax(), Obj.EffectiveModelMatrix());
			Body.Position = WorldAabb.GetCenter();
			Body.HalfExtents = WorldAabb.GetExtent();

			// UE ComplexAsSimple lite: static meshes with CPU triangles use triangle queries.
			if (Body.Type == EBodyType::Static && Obj.Mesh->HasCpuData())
			{
				const FMeshData& Cpu = Obj.Mesh->GetCpuData();
				const FMatrix Model = Obj.EffectiveModelMatrix();
				TriMesh.Positions.SetNum(Cpu.Vertices.Num());
				for (int32 Vi = 0; Vi < TriMesh.Positions.Num(); ++Vi)
				{
					TriMesh.Positions[Vi] = FVector(Model.TransformPosition(Cpu.Vertices[Vi].Position));
				}
				TriMesh.Indices = Cpu.Indices;
				if (TriMesh.IsValid())
				{
					Body.CollisionShape = EBodyCollisionShape::TriangleMesh;
				}
				else
				{
					TriMesh.Clear();
				}
			}
		}
		else
		{
			Body.Position = Obj.Transform.GetLocation();
			HalfExtentsFromScale(
				Obj.Transform.GetScale3D(), Body.HalfExtents.X, Body.HalfExtents.Y, Body.HalfExtents.Z);
		}
		if (Body.Mass <= 0.0f)
		{
			Body.Mass = MassFromHalfExtents(Body.HalfExtents.X, Body.HalfExtents.Y, Body.HalfExtents.Z);
		}
	}
	if (BackendIface != nullptr && BackendIface->HasRigidWorld())
	{
		BackendIface->RigidRebuild(Bodies, &TriangleMeshes);
	}
}

void FPhysScene::SyncToLevel(ULevel& Level) const
{
	auto& Meshes = Level.GetStaticMeshes();
	for (const FBodyInstance& Body : Bodies)
	{
		if (Body.LevelMeshIndex >= static_cast<SIZE_T>(Meshes.Num()))
		{
			continue;
		}
		Meshes[static_cast<int32>(Body.LevelMeshIndex)].Transform.SetLocation(Body.Position);
	}
}

float FPhysScene::QuerySupportZ(const FCollisionShape& Capsule, const FVector& Feet, float InFloorZ, float InStepUp,
	float InSkin, SIZE_T InSkipLevelMeshIndex) const
{
	float Support = InFloorZ;
	const float R = Capsule.GetCapsuleRadius();

	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (Body.LevelMeshIndex == InSkipLevelMeshIndex)
		{
			continue;
		}
		if (!XYDiscOverlapsAabb(Feet.X, Feet.Y, R, Body.Position.X, Body.Position.Y, Body.HalfExtents.X,
				Body.HalfExtents.Y, SupportDiscInset))
		{
			continue;
		}

		if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
			TriangleMeshes[Bi].IsValid())
		{
			// Vertical probe: walkable triangle tops under the capsule disc ComplexAsSimple (margins in cm).
			constexpr float ProbeAbove = 50.0f;
			constexpr float ProbeBelowFloor = 100.0f;
			const float RayTop =
				FMath::Max(Feet.Z + InStepUp + InSkin + ProbeAbove, Body.Position.Z + Body.HalfExtents.Z + ProbeAbove);
			const FVector Start(Feet.X, Feet.Y, RayTop);
			const FVector End(Feet.X, Feet.Y, InFloorZ - ProbeBelowFloor);
			float T = 1.0f;
			FVector LocalNormal = FVector::ZeroVector;
			if (SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], 0.0f, T, LocalNormal) && LocalNormal.Z > 0.15f)
			{
				const float ZHit = Start.Z + ((End.Z - Start.Z) * T);
				if (ZHit <= Feet.Z + InStepUp + InSkin)
				{
					Support = FMath::Max(Support, ZHit);
				}
			}
			continue;
		}

		const float Top = Body.Position.Z + Body.HalfExtents.Z;
		// Skip tops too high to step onto (the side collision handles walls).
		if (Feet.Z + InStepUp + InSkin < Top)
		{
			continue;
		}
		Support = FMath::Max(Support, Top);
	}

	for (const FSlopePlane& Plane : SlopePlanes)
	{
		if (FMath::Abs(Plane.Normal.Z) < 1.0e-4f)
		{
			continue;
		}
		// Plane height at the feet XY: dot((x, y, z) - Point, Normal) = 0.
		const float ZOnPlane = Plane.Point.Z -
			((Plane.Normal.X * (Feet.X - Plane.Point.X)) + (Plane.Normal.Y * (Feet.Y - Plane.Point.Y))) /
				Plane.Normal.Z;
		if (Feet.Z + InStepUp + InSkin < ZOnPlane)
		{
			continue;
		}
		if (!XYDiscOverlapsAabb(Feet.X, Feet.Y, R, Plane.BoundsCenter.X, Plane.BoundsCenter.Y,
				Plane.BoundsHalfExtents.X, Plane.BoundsHalfExtents.Y, SupportDiscInset))
		{
			continue;
		}
		Support = FMath::Max(Support, ZOnPlane);
	}
	return Support;
}

void FPhysScene::ResolveCapsuleSides(const FCollisionShape& Capsule, FVector& Feet, const FVector2D& WishXY,
	const FCapsuleContactParams& Params, SIZE_T InSkipLevelMeshIndex, bool bApplyPush)
{
	const float R = Capsule.GetCapsuleRadius();
	const float FeetZ = Feet.Z;
	const float Head = FeetZ + Capsule.GetCapsuleHalfHeight() * 2.0f;
	const bool bHasWish = WishXY.Size() > 1.0e-4f;
	const FVector2D WishN = bHasWish ? WishXY.GetSafeNormal() : FVector2D::ZeroVector;

	for (FBodyInstance& Body : Bodies)
	{
		if (Body.LevelMeshIndex == InSkipLevelMeshIndex)
		{
			continue;
		}
		// ComplexAsSimple: the sides come from TriangleMesh traces; the world AABB is too fat for ramps.
		if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh)
		{
			continue;
		}
		const float Hx = Body.HalfExtents.X;
		const float Hy = Body.HalfExtents.Y;
		const float Hz = Body.HalfExtents.Z;
		const float Top = Body.Position.Z + Hz;
		const float Bottom = Body.Position.Z - Hz;

		if (Head < Bottom)
		{
			continue;
		}

		const bool bXYOnTop =
			XYDiscOverlapsAabb(Feet.X, Feet.Y, R, Body.Position.X, Body.Position.Y, Hx, Hy, SupportDiscInset);
		// Standing on this top: no side push.
		if (FeetZ >= Top - Params.Skin && bXYOnTop)
		{
			continue;
		}
		// Airborne over the volume (jump / clearance): no side push.
		// Still resolve when elevated beside a short ledge (step-up clearance).
		if (FeetZ > Top && bXYOnTop)
		{
			continue;
		}

		FVector2D LocalNormal = FVector2D::ZeroVector;
		float Penetration = 0.0f;
		if (!CapsuleAabbMtv(Feet.X, Feet.Y, R, Body.Position.X, Body.Position.Y, Hx, Hy, LocalNormal, Penetration))
		{
			continue;
		}

		if (Body.Type == EBodyType::Static)
		{
			Feet.X += LocalNormal.X * Penetration;
			Feet.Y += LocalNormal.Y * Penetration;
			ClampPositionXY(Feet, Params.WalkBounds);
			continue;
		}

		// Dynamic: mass-weighted depenetration (the player has a fixed mass of 80 for the share).
		constexpr float PlayerMass = 80.0f;
		const float BodyMass = FMath::Max(Body.Mass, 0.5f);
		const float InvSum = 1.0f / (PlayerMass + BodyMass);
		const float PlayerShare = BodyMass * InvSum;
		const float BodyShare = PlayerMass * InvSum;

		Feet.X += LocalNormal.X * (Penetration * PlayerShare);
		Feet.Y += LocalNormal.Y * (Penetration * PlayerShare);
		Body.Position.X -= LocalNormal.X * (Penetration * BodyShare);
		Body.Position.Y -= LocalNormal.Y * (Penetration * BodyShare);
		ClampPositionXY(Feet, Params.WalkBounds);
		ClampPositionXY(Body.Position, Params.WalkBounds);

		CancelVelocityInto(Body.VelXY, -LocalNormal);

		if (!bApplyPush || !bHasWish)
		{
			continue;
		}

		ApplyDynamicWishPush(Body, WishN, LocalNormal, Params.PushStrength, Params.WalkBounds, false);
	}
}

bool FPhysScene::ApplyCapsuleSweepPush(SIZE_T LevelMeshIndex, const FVector2D& WishXY, const FVector& ImpactNormal,
	float InPushStrength, float InWalkBounds)
{
	if (LevelMeshIndex == ULevel::Npos || WishXY.Size() <= 1.0e-4f)
	{
		return false;
	}

	FVector2D LocalNormal(ImpactNormal.X, ImpactNormal.Y);
	const float NLen = LocalNormal.Size();
	if (NLen <= 1.0e-4f)
	{
		return false;
	}
	LocalNormal /= NLen;
	const FVector2D WishN = WishXY.GetSafeNormal();

	for (FBodyInstance& Body : Bodies)
	{
		if (Body.LevelMeshIndex != LevelMeshIndex || Body.Type != EBodyType::Dynamic)
		{
			continue;
		}
		const float Into = FMath::Max(0.0f, -FVector2D::DotProduct(WishN, LocalNormal));
		if (Into <= 1.0e-4f)
		{
			return false;
		}
		ApplyDynamicWishPush(Body, WishN, LocalNormal, InPushStrength, InWalkBounds, true);
		return true;
	}
	return false;
}

void FPhysScene::Step(const FPhysSceneStepParams& Params)
{
	if (BackendIface != nullptr && BackendIface->HasRigidWorld())
	{
		BackendIface->RigidPrepareStep(Bodies, Params.SkipLevelMeshIndex);
		BackendIface->RigidStep(Params.DeltaTime, Params.Gravity, Params.FloorZ);
		BackendIface->RigidReadBack(Bodies);
		for (FBodyInstance& Body : Bodies)
		{
			if (Body.Type != EBodyType::Dynamic)
			{
				continue;
			}
			ClampPositionXY(Body.Position, Params.WalkBounds);
		}
		return;
	}

	const float Damp = FMath::Exp(-Params.Damping * Params.DeltaTime);

	auto SupportUnderAabb = [&](const FBodyInstance& Body) -> float
	{
		float Support = Params.FloorZ;
		const float Bx0 = Body.Position.X - Body.HalfExtents.X;
		const float Bx1 = Body.Position.X + Body.HalfExtents.X;
		const float By0 = Body.Position.Y - Body.HalfExtents.Y;
		const float By1 = Body.Position.Y + Body.HalfExtents.Y;
		const float Bottom = Body.Position.Z - Body.HalfExtents.Z;

		for (const FBodyInstance& Other : Bodies)
		{
			if (Other.LevelMeshIndex == Body.LevelMeshIndex || Other.LevelMeshIndex == Params.SkipLevelMeshIndex)
			{
				continue;
			}
			const float Ox0 = Other.Position.X - Other.HalfExtents.X;
			const float Ox1 = Other.Position.X + Other.HalfExtents.X;
			const float Oy0 = Other.Position.Y - Other.HalfExtents.Y;
			const float Oy1 = Other.Position.Y + Other.HalfExtents.Y;
			if (Bx1 < Ox0 || Bx0 > Ox1 || By1 < Oy0 || By0 > Oy1)
			{
				continue;
			}

			const float Top = Other.Position.Z + Other.HalfExtents.Z;
			// Dynamic support only when this body is clearly above the other stacking.
			if (Other.Type == EBodyType::Dynamic && Bottom + Params.Skin < Top - 2.0f &&
				Body.Position.Z <= Other.Position.Z)
			{
				continue;
			}
			Support = FMath::Max(Support, Top);
		}
		return Support;
	};

	// 1) Integrate velocities (no floor snap yet).
	for (FBodyInstance& Body : Bodies)
	{
		if (Body.Type != EBodyType::Dynamic)
		{
			Body.VelXY = FVector2D::ZeroVector;
			Body.VelocityZ = 0.0f;
			continue;
		}

		if (Body.bEnableGravity)
		{
			Body.VelocityZ -= Params.Gravity * Params.DeltaTime;
			Body.Position.Z += Body.VelocityZ * Params.DeltaTime;
		}
		else
		{
			Body.VelocityZ = 0.0f;
		}

		if (Body.VelXY.Size() >= 1.0e-1f)
		{
			Body.Position.X += Body.VelXY.X * Params.DeltaTime;
			Body.Position.Y += Body.VelXY.Y * Params.DeltaTime;
			ClampPositionXY(Body.Position, Params.WalkBounds);
			Body.VelXY *= Damp;
		}
		else
		{
			Body.VelXY = FVector2D::ZeroVector;
		}
	}

	// 2) Resolve overlaps on the min-penetration axis (XY sides vs Z stacking).
	constexpr int32 Iterations = 6;
	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		for (int32 I = 0; I < Bodies.Num(); ++I)
		{
			if (Bodies[I].LevelMeshIndex == Params.SkipLevelMeshIndex)
			{
				continue;
			}
			for (int32 J = I + 1; J < Bodies.Num(); ++J)
			{
				if (Bodies[J].LevelMeshIndex == Params.SkipLevelMeshIndex)
				{
					continue;
				}

				FBodyInstance& A = Bodies[I];
				FBodyInstance& B = Bodies[J];

				const bool bADyn = A.Type == EBodyType::Dynamic;
				const bool bDyn = B.Type == EBodyType::Dynamic;
				if (!bADyn && !bDyn)
				{
					continue;
				}

				float MoveA = 0.0f;
				float MoveB = 0.0f;
				if (bADyn && bDyn)
				{
					const float Sum = FMath::Max(A.Mass + B.Mass, 1.0e-3f);
					MoveA = B.Mass / Sum;
					MoveB = A.Mass / Sum;
				}
				else if (bADyn)
				{
					MoveA = 1.0f;
				}
				else
				{
					MoveB = 1.0f;
				}

				FVector LocalNormal = FVector::ZeroVector;
				if (!SeparateAabb(A.Position, A.HalfExtents, B.Position, B.HalfExtents, MoveA, MoveB, &LocalNormal))
				{
					continue;
				}

				ClampPositionXY(A.Position, Params.WalkBounds);
				ClampPositionXY(B.Position, Params.WalkBounds);

				if (bADyn)
				{
					CancelVelocityInto(A.VelXY, FVector2D(LocalNormal.X, LocalNormal.Y));
					CancelVelocityZInto(A.VelocityZ, LocalNormal.Z);
				}
				if (bDyn)
				{
					CancelVelocityInto(B.VelXY, FVector2D(-LocalNormal.X, -LocalNormal.Y));
					CancelVelocityZInto(B.VelocityZ, -LocalNormal.Z);
				}
			}
		}
	}

	// 3) Floor / platform snap only when landing from above.
	for (FBodyInstance& Body : Bodies)
	{
		if (Body.Type != EBodyType::Dynamic || !Body.bEnableGravity)
		{
			continue;
		}

		const float Support = SupportUnderAabb(Body);
		const float Bottom = Body.Position.Z - Body.HalfExtents.Z;
		const float Window = LandWindow(Body.VelocityZ, Params.DeltaTime, Params.Skin);
		if (Body.VelocityZ <= 0.0f && Bottom <= Support + Params.Skin && Bottom >= Support - Window)
		{
			Body.Position.Z = Support + Body.HalfExtents.Z;
			Body.VelocityZ = 0.0f;
			// Resting friction: kill a tiny residual slide when fully supported.
			if (Body.VelXY.Size() < 8.0f)
			{
				Body.VelXY = FVector2D::ZeroVector;
			}
		}
	}
}

void FPhysScene::AppendCollisionDebug(
	FDebugDraw& Draw, const FCollisionShape& Capsule, const FVector& Feet, SIZE_T InSkipLevelMeshIndex) const
{
	const float R = Capsule.GetCapsuleRadius();
	const float H = Capsule.GetCapsuleHalfHeight() * 2.0f;
	const float CylBottom = FMath::Min(R, H * 0.5f);
	const float CylTop = FMath::Max(H - R, CylBottom);
	const FLinearColor CapsuleColor(0.2f, 1.0f, 0.45f);
	constexpr int32 Seg = 12;

	const FVector B0 = Feet + FVector(0.0f, 0.0f, CylBottom);
	const FVector T0 = Feet + FVector(0.0f, 0.0f, CylTop);
	const FLinearColor Color = CapsuleColor;
	Draw.AddLine(B0 + FVector(R, 0, 0), T0 + FVector(R, 0, 0), Color);
	Draw.AddLine(B0 + FVector(-R, 0, 0), T0 + FVector(-R, 0, 0), Color);
	Draw.AddLine(B0 + FVector(0, R, 0), T0 + FVector(0, R, 0), Color);
	Draw.AddLine(B0 + FVector(0, -R, 0), T0 + FVector(0, -R, 0), Color);

	AppendCapsuleRing(Draw, Feet + FVector(0.0f, 0.0f, CylBottom), R, CapsuleColor, Seg);
	AppendCapsuleRing(Draw, Feet + FVector(0.0f, 0.0f, CylTop), R, CapsuleColor, Seg);

	if (CylBottom > 1.0e-1f)
	{
		AppendCapsuleRing(Draw, Feet + FVector(0.0f, 0.0f, CylBottom * 0.5f), R * 0.85f, CapsuleColor, Seg / 2);
	}
	if (H - CylTop > 1.0e-1f)
	{
		const float CapMidZ = CylTop + ((H - CylTop) * 0.5f);
		AppendCapsuleRing(Draw, Feet + FVector(0.0f, 0.0f, CapMidZ), R * 0.85f, CapsuleColor, Seg / 2);
	}
	AppendCapsuleRing(Draw, Feet, R * 0.35f, CapsuleColor, 6);
	AppendCapsuleRing(Draw, Feet + FVector(0.0f, 0.0f, H), R * 0.35f, CapsuleColor, 6);

	AppendBodiesCollisionDebug(Draw, InSkipLevelMeshIndex);
}

void FPhysScene::AppendBodiesCollisionDebug(FDebugDraw& Draw, SIZE_T InSkipLevelMeshIndex) const
{
	const FLinearColor DynamicColor(1.0f, 0.55f, 0.15f);
	const FLinearColor StaticColor(0.35f, 0.65f, 1.0f);
	const FLinearColor TriMeshColor(0.25f, 0.9f, 1.0f);
	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (Body.LevelMeshIndex == InSkipLevelMeshIndex)
		{
			continue;
		}

		// TriangleMesh: draw the actual triangles oriented; the AABB alone looks like a fat unrotated box.
		if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
			TriangleMeshes[Bi].IsValid())
		{
			const FTriangleMeshCollision& Mesh = TriangleMeshes[Bi];
			for (int32 I = 0; I + 2 < Mesh.Indices.Num(); I += 3)
			{
				const FVector V0 = Mesh.Positions[static_cast<int32>(Mesh.Indices[I])];
				const FVector V1 = Mesh.Positions[static_cast<int32>(Mesh.Indices[I + 1])];
				const FVector V2 = Mesh.Positions[static_cast<int32>(Mesh.Indices[I + 2])];
				Draw.AddLine(V0, V1, TriMeshColor);
				Draw.AddLine(V1, V2, TriMeshColor);
				Draw.AddLine(V2, V0, TriMeshColor);
			}
			continue;
		}

		const FVector Mn = Body.Position - Body.HalfExtents;
		const FVector Mx = Body.Position + Body.HalfExtents;
		Draw.AddAabb(Mn, Mx, Body.Type == EBodyType::Dynamic ? DynamicColor : StaticColor);
	}

	// Walkable slope planes (AddSlopeRamp): magenta wire quads for F2.
	const FLinearColor SlopeColor(0.95f, 0.2f, 0.85f);
	for (const FSlopePlane& Plane : SlopePlanes)
	{
		const float Hx = Plane.BoundsHalfExtents.X;
		const float Hy = Plane.BoundsHalfExtents.Y;
		const FVector& C = Plane.BoundsCenter;
		const float NLen = Plane.Normal.Size();
		if (NLen < 1.0e-6f || FMath::Abs(Plane.Normal.Z) < 1.0e-4f)
		{
			continue;
		}
		const FVector N = Plane.Normal / NLen;
		auto ZAt = [&](float X, float Y)
		{ return Plane.Point.Z - ((N.X * (X - Plane.Point.X)) + (N.Y * (Y - Plane.Point.Y))) / N.Z; };
		const FVector P00(C.X - Hx, C.Y - Hy, ZAt(C.X - Hx, C.Y - Hy));
		const FVector P10(C.X + Hx, C.Y - Hy, ZAt(C.X + Hx, C.Y - Hy));
		const FVector P11(C.X + Hx, C.Y + Hy, ZAt(C.X + Hx, C.Y + Hy));
		const FVector P01(C.X - Hx, C.Y + Hy, ZAt(C.X - Hx, C.Y + Hy));
		Draw.AddLine(P00, P10, SlopeColor);
		Draw.AddLine(P10, P11, SlopeColor);
		Draw.AddLine(P11, P01, SlopeColor);
		Draw.AddLine(P01, P00, SlopeColor);
		Draw.AddLine(P00, P11, SlopeColor);
		Draw.AddLine(P10, P01, SlopeColor);
	}
}
