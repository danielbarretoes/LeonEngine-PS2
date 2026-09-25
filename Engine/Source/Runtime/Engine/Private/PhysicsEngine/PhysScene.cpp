#include "Physics/PhysScene.h"

#include "Debug/DebugDraw.h"
#include "Frustum.h"
#include "IPhysicsBackend.h"
#include "MeshData.h"
#include "Migration/GlmInterop.h"
#include "StaticMesh.h"
#include "TriangleCollision.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

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

	void CancelVelocityYInto(float& VelocityY, float OutwardNormalY)
	{
		if (OutwardNormalY > 0.5f && VelocityY < 0.0f)
		{
			VelocityY = 0.0f;
		}
		else if (OutwardNormalY < -0.5f && VelocityY > 0.0f)
		{
			VelocityY = 0.0f;
		}
	}

	void AppendCapsuleRing(FDebugDraw& Draw, const FVector& Center, float Radius, const FVector& Color, int32 Segments)
	{
		const float SegCount = static_cast<float>(Segments);
		for (int32 I = 0; I < Segments; ++I)
		{
			const float A0 = (static_cast<float>(I) / SegCount) * 6.2831853f;
			const float A1 = (static_cast<float>(I + 1) / SegCount) * 6.2831853f;
			const FVector P0(FMath::Cos(A0) * Radius, 0.0f, FMath::Sin(A0) * Radius);
			const FVector P1(FMath::Cos(A1) * Radius, 0.0f, FMath::Sin(A1) * Radius);
			Draw.AddLine(ToGlm(Center + P0), ToGlm(Center + P1), ToGlm(Color));
		}
	}

	[[nodiscard]] float LandWindow(float VelocityY, float InDeltaTime, float InSkin)
	{
		return FMath::Max(0.12f, (FMath::Abs(VelocityY) * InDeltaTime) + (InSkin * 4.0f));
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
		constexpr float PushScale = 2.8f;
		Body.VelXz += WishN * (Into * InPushStrength * InvMass * PushScale);

		constexpr float MaxPushSpeed = 4.0f;
		const float Speed = Body.VelXz.Size();
		if (Speed > MaxPushSpeed)
		{
			Body.VelXz *= MaxPushSpeed / Speed;
		}

		if (!bApplyContactShove)
		{
			return;
		}

		// Sweep-based contact never overlaps; nudge the body so the walking shove is visible the same frame.
		const float BodyShare = PlayerMass / (PlayerMass + BodyMass);
		constexpr float ContactShove = 0.06f;
		const float Shove = Into * InPushStrength * BodyShare * ContactShove;
		Body.Position.X += WishN.X * Shove;
		Body.Position.Z += WishN.Y * Shove;
		ClampPositionXZ(Body.Position, InWalkBounds);
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
	// The surface rises with +X; the unit normal points to the walkable side (Normal.Y = cos(pitch)).
	FVector LocalNormal(-S, C, 0.0f);
	if (FMath::Abs(YawDegrees) > 1.0e-3f)
	{
		const float Yaw = YawDegrees * DegToRad;
		const float Cy = FMath::Cos(Yaw);
		const float Sy = FMath::Sin(Yaw);
		LocalNormal =
			FVector(LocalNormal.X * Cy - LocalNormal.Z * Sy, LocalNormal.Y, LocalNormal.X * Sy + LocalNormal.Z * Cy);
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

		if (Body.LevelMeshIndex >= Meshes.size())
		{
			continue;
		}
		const UStaticMeshComponent& Obj = Meshes[Body.LevelMeshIndex];
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
				const glm::mat4 Model = Obj.EffectiveModelMatrix();
				TriMesh.Positions.SetNum(static_cast<int32>(Cpu.Vertices.size()));
				for (int32 Vi = 0; Vi < TriMesh.Positions.Num(); ++Vi)
				{
					const glm::vec4 World = Model * glm::vec4(Cpu.Vertices[static_cast<SIZE_T>(Vi)].Position, 1.0f);
					TriMesh.Positions[Vi] = FVector(World.x, World.y, World.z);
				}
				TriMesh.Indices.Append(Cpu.Indices.data(), static_cast<int32>(Cpu.Indices.size()));
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
			Body.Position = FromGlm(Obj.Transform.Position);
			HalfExtentsFromScale(
				FromGlm(Obj.Transform.Scale), Body.HalfExtents.X, Body.HalfExtents.Y, Body.HalfExtents.Z);
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
		if (Body.LevelMeshIndex >= Meshes.size())
		{
			continue;
		}
		Meshes[Body.LevelMeshIndex].Transform.Position = ToGlm(Body.Position);
	}
}

float FPhysScene::QuerySupportY(const FCollisionShape& Capsule, const FVector& Feet, float InFloorY, float InStepUp,
	float InSkin, SIZE_T InSkipLevelMeshIndex) const
{
	float Support = InFloorY;
	const float R = Capsule.GetCapsuleRadius();

	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (Body.LevelMeshIndex == InSkipLevelMeshIndex)
		{
			continue;
		}
		if (!XzDiscOverlapsAabb(
				Feet.X, Feet.Z, R, Body.Position.X, Body.Position.Z, Body.HalfExtents.X, Body.HalfExtents.Z, -0.02f))
		{
			continue;
		}

		if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
			TriangleMeshes[Bi].IsValid())
		{
			// Vertical probe: walkable triangle tops under the capsule disc (ComplexAsSimple).
			const float RayTop =
				FMath::Max(Feet.Y + InStepUp + InSkin + 0.5f, Body.Position.Y + Body.HalfExtents.Y + 0.5f);
			const FVector Start(Feet.X, RayTop, Feet.Z);
			const FVector End(Feet.X, InFloorY - 1.0f, Feet.Z);
			float T = 1.0f;
			FVector LocalNormal = FVector::ZeroVector;
			if (SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], 0.0f, T, LocalNormal) && LocalNormal.Y > 0.15f)
			{
				const float YHit = Start.Y + ((End.Y - Start.Y) * T);
				if (YHit <= Feet.Y + InStepUp + InSkin)
				{
					Support = FMath::Max(Support, YHit);
				}
			}
			continue;
		}

		const float Top = Body.Position.Y + Body.HalfExtents.Y;
		// Skip tops too high to step onto (the side collision handles walls).
		if (Feet.Y + InStepUp + InSkin < Top)
		{
			continue;
		}
		Support = FMath::Max(Support, Top);
	}

	for (const FSlopePlane& Plane : SlopePlanes)
	{
		if (FMath::Abs(Plane.Normal.Y) < 1.0e-4f)
		{
			continue;
		}
		// Plane height at the feet XZ: dot((x, y, z) - Point, Normal) = 0.
		const float YOnPlane = Plane.Point.Y -
			((Plane.Normal.X * (Feet.X - Plane.Point.X)) + (Plane.Normal.Z * (Feet.Z - Plane.Point.Z))) /
				Plane.Normal.Y;
		if (Feet.Y + InStepUp + InSkin < YOnPlane)
		{
			continue;
		}
		if (!XzDiscOverlapsAabb(Feet.X, Feet.Z, R, Plane.BoundsCenter.X, Plane.BoundsCenter.Z,
				Plane.BoundsHalfExtents.X, Plane.BoundsHalfExtents.Z, -0.02f))
		{
			continue;
		}
		Support = FMath::Max(Support, YOnPlane);
	}
	return Support;
}

void FPhysScene::ResolveCapsuleSides(const FCollisionShape& Capsule, FVector& Feet, const FVector2D& WishXz,
	const FCapsuleContactParams& Params, SIZE_T InSkipLevelMeshIndex, bool bApplyPush)
{
	const float R = Capsule.GetCapsuleRadius();
	const float FeetY = Feet.Y;
	const float Head = FeetY + Capsule.GetCapsuleHalfHeight() * 2.0f;
	const bool bHasWish = WishXz.Size() > 1.0e-4f;
	const FVector2D WishN = bHasWish ? WishXz.GetSafeNormal() : FVector2D::ZeroVector;

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
		const float Top = Body.Position.Y + Hy;
		const float Bottom = Body.Position.Y - Hy;

		if (Head < Bottom)
		{
			continue;
		}

		const bool bXzOnTop = XzDiscOverlapsAabb(Feet.X, Feet.Z, R, Body.Position.X, Body.Position.Z, Hx, Hz, -0.02f);
		// Standing on this top: no side push.
		if (FeetY >= Top - Params.Skin && bXzOnTop)
		{
			continue;
		}
		// Airborne over the volume (jump / clearance): no side push.
		// Still resolve when elevated beside a short ledge (step-up clearance).
		if (FeetY > Top && bXzOnTop)
		{
			continue;
		}

		FVector2D LocalNormal = FVector2D::ZeroVector;
		float Penetration = 0.0f;
		if (!CapsuleAabbMtv(Feet.X, Feet.Z, R, Body.Position.X, Body.Position.Z, Hx, Hz, LocalNormal, Penetration))
		{
			continue;
		}

		if (Body.Type == EBodyType::Static)
		{
			Feet.X += LocalNormal.X * Penetration;
			Feet.Z += LocalNormal.Y * Penetration;
			ClampPositionXZ(Feet, Params.WalkBounds);
			continue;
		}

		// Dynamic: mass-weighted depenetration (the player has a fixed mass of 80 for the share).
		constexpr float PlayerMass = 80.0f;
		const float BodyMass = FMath::Max(Body.Mass, 0.5f);
		const float InvSum = 1.0f / (PlayerMass + BodyMass);
		const float PlayerShare = BodyMass * InvSum;
		const float BodyShare = PlayerMass * InvSum;

		Feet.X += LocalNormal.X * (Penetration * PlayerShare);
		Feet.Z += LocalNormal.Y * (Penetration * PlayerShare);
		Body.Position.X -= LocalNormal.X * (Penetration * BodyShare);
		Body.Position.Z -= LocalNormal.Y * (Penetration * BodyShare);
		ClampPositionXZ(Feet, Params.WalkBounds);
		ClampPositionXZ(Body.Position, Params.WalkBounds);

		CancelVelocityInto(Body.VelXz, -LocalNormal);

		if (!bApplyPush || !bHasWish)
		{
			continue;
		}

		ApplyDynamicWishPush(Body, WishN, LocalNormal, Params.PushStrength, Params.WalkBounds, false);
	}
}

bool FPhysScene::ApplyCapsuleSweepPush(SIZE_T LevelMeshIndex, const FVector2D& WishXz, const FVector& ImpactNormal,
	float InPushStrength, float InWalkBounds)
{
	if (LevelMeshIndex == ULevel::Npos || WishXz.Size() <= 1.0e-4f)
	{
		return false;
	}

	FVector2D LocalNormal(ImpactNormal.X, ImpactNormal.Z);
	const float NLen = LocalNormal.Size();
	if (NLen <= 1.0e-4f)
	{
		return false;
	}
	LocalNormal /= NLen;
	const FVector2D WishN = WishXz.GetSafeNormal();

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
		BackendIface->RigidStep(Params.DeltaTime, Params.Gravity, Params.FloorY);
		BackendIface->RigidReadBack(Bodies);
		for (FBodyInstance& Body : Bodies)
		{
			if (Body.Type != EBodyType::Dynamic)
			{
				continue;
			}
			ClampPositionXZ(Body.Position, Params.WalkBounds);
		}
		return;
	}

	const float Damp = FMath::Exp(-Params.Damping * Params.DeltaTime);

	auto SupportUnderAabb = [&](const FBodyInstance& Body) -> float
	{
		float Support = Params.FloorY;
		const float Bx0 = Body.Position.X - Body.HalfExtents.X;
		const float Bx1 = Body.Position.X + Body.HalfExtents.X;
		const float Bz0 = Body.Position.Z - Body.HalfExtents.Z;
		const float Bz1 = Body.Position.Z + Body.HalfExtents.Z;
		const float Bottom = Body.Position.Y - Body.HalfExtents.Y;

		for (const FBodyInstance& Other : Bodies)
		{
			if (Other.LevelMeshIndex == Body.LevelMeshIndex || Other.LevelMeshIndex == Params.SkipLevelMeshIndex)
			{
				continue;
			}
			const float Ox0 = Other.Position.X - Other.HalfExtents.X;
			const float Ox1 = Other.Position.X + Other.HalfExtents.X;
			const float Oz0 = Other.Position.Z - Other.HalfExtents.Z;
			const float Oz1 = Other.Position.Z + Other.HalfExtents.Z;
			if (Bx1 < Ox0 || Bx0 > Ox1 || Bz1 < Oz0 || Bz0 > Oz1)
			{
				continue;
			}

			const float Top = Other.Position.Y + Other.HalfExtents.Y;
			// Dynamic support only when this body is clearly above the other (stacking).
			if (Other.Type == EBodyType::Dynamic && Bottom + Params.Skin < Top - 0.02f &&
				Body.Position.Y <= Other.Position.Y)
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
			Body.VelXz = FVector2D::ZeroVector;
			Body.VelocityY = 0.0f;
			continue;
		}

		if (Body.bEnableGravity)
		{
			Body.VelocityY -= Params.Gravity * Params.DeltaTime;
			Body.Position.Y += Body.VelocityY * Params.DeltaTime;
		}
		else
		{
			Body.VelocityY = 0.0f;
		}

		if (Body.VelXz.Size() >= 1.0e-3f)
		{
			Body.Position.X += Body.VelXz.X * Params.DeltaTime;
			Body.Position.Z += Body.VelXz.Y * Params.DeltaTime;
			ClampPositionXZ(Body.Position, Params.WalkBounds);
			Body.VelXz *= Damp;
		}
		else
		{
			Body.VelXz = FVector2D::ZeroVector;
		}
	}

	// 2) Resolve overlaps on the min-penetration axis (XZ sides vs Y stacking).
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

				ClampPositionXZ(A.Position, Params.WalkBounds);
				ClampPositionXZ(B.Position, Params.WalkBounds);

				if (bADyn)
				{
					CancelVelocityInto(A.VelXz, FVector2D(LocalNormal.X, LocalNormal.Z));
					CancelVelocityYInto(A.VelocityY, LocalNormal.Y);
				}
				if (bDyn)
				{
					CancelVelocityInto(B.VelXz, FVector2D(-LocalNormal.X, -LocalNormal.Z));
					CancelVelocityYInto(B.VelocityY, -LocalNormal.Y);
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
		const float Bottom = Body.Position.Y - Body.HalfExtents.Y;
		const float Window = LandWindow(Body.VelocityY, Params.DeltaTime, Params.Skin);
		if (Body.VelocityY <= 0.0f && Bottom <= Support + Params.Skin && Bottom >= Support - Window)
		{
			Body.Position.Y = Support + Body.HalfExtents.Y;
			Body.VelocityY = 0.0f;
			// Resting friction: kill a tiny residual slide when fully supported.
			if (Body.VelXz.Size() < 0.08f)
			{
				Body.VelXz = FVector2D::ZeroVector;
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
	const FVector CapsuleColor(0.2f, 1.0f, 0.45f);
	constexpr int32 Seg = 12;

	const FVector B0 = Feet + FVector(0.0f, CylBottom, 0.0f);
	const FVector T0 = Feet + FVector(0.0f, CylTop, 0.0f);
	const glm::vec3 Color = ToGlm(CapsuleColor);
	Draw.AddLine(ToGlm(B0 + FVector(R, 0, 0)), ToGlm(T0 + FVector(R, 0, 0)), Color);
	Draw.AddLine(ToGlm(B0 + FVector(-R, 0, 0)), ToGlm(T0 + FVector(-R, 0, 0)), Color);
	Draw.AddLine(ToGlm(B0 + FVector(0, 0, R)), ToGlm(T0 + FVector(0, 0, R)), Color);
	Draw.AddLine(ToGlm(B0 + FVector(0, 0, -R)), ToGlm(T0 + FVector(0, 0, -R)), Color);

	AppendCapsuleRing(Draw, Feet + FVector(0.0f, CylBottom, 0.0f), R, CapsuleColor, Seg);
	AppendCapsuleRing(Draw, Feet + FVector(0.0f, CylTop, 0.0f), R, CapsuleColor, Seg);

	if (CylBottom > 1.0e-3f)
	{
		AppendCapsuleRing(Draw, Feet + FVector(0.0f, CylBottom * 0.5f, 0.0f), R * 0.85f, CapsuleColor, Seg / 2);
	}
	if (H - CylTop > 1.0e-3f)
	{
		const float CapMidY = CylTop + ((H - CylTop) * 0.5f);
		AppendCapsuleRing(Draw, Feet + FVector(0.0f, CapMidY, 0.0f), R * 0.85f, CapsuleColor, Seg / 2);
	}
	AppendCapsuleRing(Draw, Feet, R * 0.35f, CapsuleColor, 6);
	AppendCapsuleRing(Draw, Feet + FVector(0.0f, H, 0.0f), R * 0.35f, CapsuleColor, 6);

	AppendBodiesCollisionDebug(Draw, InSkipLevelMeshIndex);
}

void FPhysScene::AppendBodiesCollisionDebug(FDebugDraw& Draw, SIZE_T InSkipLevelMeshIndex) const
{
	const glm::vec3 DynamicColor(1.0f, 0.55f, 0.15f);
	const glm::vec3 StaticColor(0.35f, 0.65f, 1.0f);
	const glm::vec3 TriMeshColor(0.25f, 0.9f, 1.0f);
	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (Body.LevelMeshIndex == InSkipLevelMeshIndex)
		{
			continue;
		}

		// TriangleMesh: draw the actual triangles (oriented); the AABB alone looks like a fat unrotated box.
		if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
			TriangleMeshes[Bi].IsValid())
		{
			const FTriangleMeshCollision& Mesh = TriangleMeshes[Bi];
			for (int32 I = 0; I + 2 < Mesh.Indices.Num(); I += 3)
			{
				const glm::vec3 V0 = ToGlm(Mesh.Positions[static_cast<int32>(Mesh.Indices[I])]);
				const glm::vec3 V1 = ToGlm(Mesh.Positions[static_cast<int32>(Mesh.Indices[I + 1])]);
				const glm::vec3 V2 = ToGlm(Mesh.Positions[static_cast<int32>(Mesh.Indices[I + 2])]);
				Draw.AddLine(V0, V1, TriMeshColor);
				Draw.AddLine(V1, V2, TriMeshColor);
				Draw.AddLine(V2, V0, TriMeshColor);
			}
			continue;
		}

		const FVector Mn = Body.Position - Body.HalfExtents;
		const FVector Mx = Body.Position + Body.HalfExtents;
		Draw.AddAabb(ToGlm(Mn), ToGlm(Mx), Body.Type == EBodyType::Dynamic ? DynamicColor : StaticColor);
	}

	// Walkable slope planes (AddSlopeRamp): magenta wire quads for F2.
	const glm::vec3 SlopeColor(0.95f, 0.2f, 0.85f);
	for (const FSlopePlane& Plane : SlopePlanes)
	{
		const float Hx = Plane.BoundsHalfExtents.X;
		const float Hz = Plane.BoundsHalfExtents.Z;
		const FVector& C = Plane.BoundsCenter;
		const float NLen = Plane.Normal.Size();
		if (NLen < 1.0e-6f || FMath::Abs(Plane.Normal.Y) < 1.0e-4f)
		{
			continue;
		}
		const FVector N = Plane.Normal / NLen;
		auto YAt = [&](float X, float Z)
		{ return Plane.Point.Y - ((N.X * (X - Plane.Point.X)) + (N.Z * (Z - Plane.Point.Z))) / N.Y; };
		const glm::vec3 P00(C.X - Hx, YAt(C.X - Hx, C.Z - Hz), C.Z - Hz);
		const glm::vec3 P10(C.X + Hx, YAt(C.X + Hx, C.Z - Hz), C.Z - Hz);
		const glm::vec3 P11(C.X + Hx, YAt(C.X + Hx, C.Z + Hz), C.Z + Hz);
		const glm::vec3 P01(C.X - Hx, YAt(C.X - Hx, C.Z + Hz), C.Z + Hz);
		Draw.AddLine(P00, P10, SlopeColor);
		Draw.AddLine(P10, P11, SlopeColor);
		Draw.AddLine(P11, P01, SlopeColor);
		Draw.AddLine(P01, P00, SlopeColor);
		Draw.AddLine(P00, P11, SlopeColor);
		Draw.AddLine(P10, P01, SlopeColor);
	}
}
