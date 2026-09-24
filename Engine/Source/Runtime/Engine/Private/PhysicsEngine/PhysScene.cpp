#include "Physics/PhysScene.h"

#include "Debug/DebugDraw.h"
#include "Frustum.h"
#include "IPhysicsBackend.h"
#include "MeshData.h"
#include "StaticMesh.h"
#include "TriangleCollision.h"

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

FPhysScene::FPhysScene(EPhysicsBackend InBackend)
	: Backend(InBackend)
	, BackendIface(CreatePhysicsBackend(
		  InBackend == EPhysicsBackend::Jolt ? EPhysicsBackendKind::Jolt : EPhysicsBackendKind::Arcade))
{
	if (BackendIface != nullptr && std::strcmp(BackendIface->GetName(), "Jolt") == 0)
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

	void CancelVelocityInto(glm::vec2& Vel, const glm::vec2& OutwardNormal)
	{
		const float Into = glm::dot(Vel, -OutwardNormal);
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

	void AppendCapsuleRing(
		FDebugDraw& Draw, const glm::vec3& Center, float Radius, const glm::vec3& Color, int Segments)
	{
		const float SegCount = static_cast<float>(Segments);
		for (int I = 0; I < Segments; ++I)
		{
			const float A0 = (static_cast<float>(I) / SegCount) * 6.2831853f;
			const float A1 = (static_cast<float>(I + 1) / SegCount) * 6.2831853f;
			const glm::vec3 P0{std::cos(A0) * Radius, 0.0f, std::sin(A0) * Radius};
			const glm::vec3 P1{std::cos(A1) * Radius, 0.0f, std::sin(A1) * Radius};
			Draw.AddLine(Center + P0, Center + P1, Color);
		}
	}

	[[nodiscard]] float LandWindow(float VelocityY, float InDeltaTime, float InSkin)
	{
		return std::max(0.12f, (std::abs(VelocityY) * InDeltaTime) + (InSkin * 4.0f));
	}

	/// Flow: wish into contact normal → lateral vel + optional contact shove (light props).
	void ApplyDynamicWishPush(FBodyInstance& Body, const glm::vec2& WishN, const glm::vec2& InNormal,
		float InPushStrength, float InWalkBounds, bool bApplyContactShove)
	{
		const float Into = std::max(0.0f, -glm::dot(WishN, InNormal));
		if (Into <= 1.0e-4f)
		{
			return;
		}

		constexpr float PlayerMass = 80.0f;
		const float BodyMass = std::max(Body.Mass, 0.5f);
		const float InvMass = 1.0f / BodyMass;
		constexpr float PushScale = 2.8f;
		Body.VelXz += WishN * (Into * InPushStrength * InvMass * PushScale);

		constexpr float MaxPushSpeed = 4.0f;
		const float Speed = glm::length(Body.VelXz);
		if (Speed > MaxPushSpeed)
		{
			Body.VelXz *= MaxPushSpeed / Speed;
		}

		if (!bApplyContactShove)
		{
			return;
		}

		// Sweep-based contact never overlaps; nudge the body so walking shove is visible same frame.
		const float BodyShare = PlayerMass / (PlayerMass + BodyMass);
		constexpr float ContactShove = 0.06f;
		const float Shove = Into * InPushStrength * BodyShare * ContactShove;
		Body.Position.x += WishN.x * Shove;
		Body.Position.z += WishN.y * Shove;
		ClampPositionXZ(Body.Position, InWalkBounds);
	}

} // namespace

void FPhysScene::Clear()
{
	Bodies.clear();
	TriangleMeshes.clear();
	SlopePlanes.clear();
	if (BackendIface != nullptr)
	{
		BackendIface->RigidClear();
	}
}

std::size_t FPhysScene::AddBody(const FBodyInstanceDesc& Desc)
{
	FBodyInstance Body{};
	Body.LevelMeshIndex = Desc.LevelMeshIndex;
	Body.Type = Desc.Type;
	Body.Mass = Desc.Mass > 0.0f ? Desc.Mass : 0.0f;
	Body.bEnableGravity = Desc.bEnableGravity;
	Bodies.push_back(Body);
	TriangleMeshes.emplace_back();
	return Bodies.size() - 1;
}

std::size_t FPhysScene::AddSlopeRamp(
	const glm::vec3& InBoundsCenter, const glm::vec3& InBoundsHalfExtents, float PitchDegrees, float YawDegrees)
{
	constexpr float DegToRad = 0.01745329251f;
	const float Pitch = PitchDegrees * DegToRad;
	const float S = std::sin(Pitch);
	const float C = std::cos(Pitch);
	FSlopePlane Plane{};
	Plane.Point = InBoundsCenter;
	// Surface rises with +X; unit normal points to the walkable side (normal.y = cos(pitch)).
	glm::vec3 LocalNormal{-S, C, 0.0f};
	if (std::abs(YawDegrees) > 1.0e-3f)
	{
		const float Yaw = YawDegrees * DegToRad;
		const float Cy = std::cos(Yaw);
		const float Sy = std::sin(Yaw);
		LocalNormal = {LocalNormal.x * Cy - LocalNormal.z * Sy, LocalNormal.y, LocalNormal.x * Sy + LocalNormal.z * Cy};
	}
	Plane.Normal = glm::normalize(LocalNormal);
	Plane.BoundsCenter = InBoundsCenter;
	Plane.BoundsHalfExtents = InBoundsHalfExtents;
	SlopePlanes.push_back(Plane);
	return SlopePlanes.size() - 1;
}

void FPhysScene::SyncFromLevel(const ULevel& Level)
{
	const auto& Meshes = Level.GetStaticMeshes();
	if (TriangleMeshes.size() != Bodies.size())
	{
		TriangleMeshes.resize(Bodies.size());
	}
	for (std::size_t Bi = 0; Bi < Bodies.size(); ++Bi)
	{
		FBodyInstance& Body = Bodies[Bi];
		FTriangleMeshCollision& TriMesh = TriangleMeshes[Bi];
		TriMesh.Clear();
		Body.CollisionShape = ECollisionShape::Box;

		if (Body.LevelMeshIndex >= Meshes.size())
		{
			continue;
		}
		const UStaticMeshComponent& Obj = Meshes[Body.LevelMeshIndex];
		if (Obj.Mesh != nullptr)
		{
			const FBox WorldAabb = FBox::FromLocalTransformed(
				Obj.Mesh->GetLocalMin(), Obj.Mesh->GetLocalMax(), Obj.EffectiveModelMatrix());
			Body.Position = (WorldAabb.Min + WorldAabb.Max) * 0.5f;
			Body.HalfExtents = (WorldAabb.Max - WorldAabb.Min) * 0.5f;

			// Unreal ComplexAsSimple lite: static meshes with CPU tris use triangle queries.
			if (Body.Type == EBodyType::Static && Obj.Mesh->HasCpuData())
			{
				const FMeshData& Cpu = Obj.Mesh->GetCpuData();
				const glm::mat4 Model = Obj.EffectiveModelMatrix();
				TriMesh.Positions.resize(Cpu.Vertices.size());
				for (std::size_t Vi = 0; Vi < Cpu.Vertices.size(); ++Vi)
				{
					const glm::vec4 World = Model * glm::vec4(Cpu.Vertices[Vi].Position, 1.0f);
					TriMesh.Positions[Vi] = glm::vec3(World);
				}
				TriMesh.Indices = Cpu.Indices;
				if (TriMesh.IsValid())
				{
					Body.CollisionShape = ECollisionShape::TriangleMesh;
				}
				else
				{
					TriMesh.Clear();
				}
			}
		}
		else
		{
			Body.Position = Obj.Transform.Position;
			HalfExtentsFromScale(Obj.Transform.Scale, Body.HalfExtents.x, Body.HalfExtents.y, Body.HalfExtents.z);
		}
		if (Body.Mass <= 0.0f)
		{
			Body.Mass = MassFromHalfExtents(Body.HalfExtents.x, Body.HalfExtents.y, Body.HalfExtents.z);
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
		Meshes[Body.LevelMeshIndex].Transform.Position = Body.Position;
	}
}

float FPhysScene::QuerySupportY(const FCapsuleShape& Capsule, const glm::vec3& Feet, float InFloorY, float InStepUp,
	float InSkin, std::size_t InSkipLevelMeshIndex) const
{
	float Support = InFloorY;
	const float R = Capsule.Radius;

	for (std::size_t Bi = 0; Bi < Bodies.size(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (Body.LevelMeshIndex == InSkipLevelMeshIndex)
		{
			continue;
		}
		if (!XzDiscOverlapsAabb(
				Feet.x, Feet.z, R, Body.Position.x, Body.Position.z, Body.HalfExtents.x, Body.HalfExtents.z, -0.02f))
		{
			continue;
		}

		if (Body.CollisionShape == ECollisionShape::TriangleMesh && Bi < TriangleMeshes.size() &&
			TriangleMeshes[Bi].IsValid())
		{
			// Vertical probe: walkable triangle tops under the capsule disc (ComplexAsSimple).
			const float RayTop =
				std::max(Feet.y + InStepUp + InSkin + 0.5f, Body.Position.y + Body.HalfExtents.y + 0.5f);
			const glm::vec3 Start{Feet.x, RayTop, Feet.z};
			const glm::vec3 End{Feet.x, InFloorY - 1.0f, Feet.z};
			float T = 1.0f;
			glm::vec3 LocalNormal{};
			if (SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], 0.0f, T, LocalNormal) && LocalNormal.y > 0.15f)
			{
				const float YHit = Start.y + ((End.y - Start.y) * T);
				if (YHit <= Feet.y + InStepUp + InSkin)
				{
					Support = std::max(Support, YHit);
				}
			}
			continue;
		}

		const float Top = Body.Position.y + Body.HalfExtents.y;
		// Skip tops too high to step onto (side collision handles walls).
		if (Feet.y + InStepUp + InSkin < Top)
		{
			continue;
		}
		Support = std::max(Support, Top);
	}

	for (const FSlopePlane& Plane : SlopePlanes)
	{
		if (std::abs(Plane.Normal.y) < 1.0e-4f)
		{
			continue;
		}
		// FPlane height at feet XZ: dot((x,y,z)-point, n) = 0.
		const float YOnPlane = Plane.Point.y -
			((Plane.Normal.x * (Feet.x - Plane.Point.x)) + (Plane.Normal.z * (Feet.z - Plane.Point.z))) /
				Plane.Normal.y;
		if (Feet.y + InStepUp + InSkin < YOnPlane)
		{
			continue;
		}
		if (!XzDiscOverlapsAabb(Feet.x, Feet.z, R, Plane.BoundsCenter.x, Plane.BoundsCenter.z,
				Plane.BoundsHalfExtents.x, Plane.BoundsHalfExtents.z, -0.02f))
		{
			continue;
		}
		Support = std::max(Support, YOnPlane);
	}
	return Support;
}

void FPhysScene::ResolveCapsuleSides(const FCapsuleShape& Capsule, glm::vec3& Feet, const glm::vec2& WishXz,
	const FCapsuleContactParams& Params, std::size_t InSkipLevelMeshIndex, bool bApplyPush)
{
	const float R = Capsule.Radius;
	const float FeetY = Feet.y;
	const float Head = FeetY + Capsule.Height;
	const bool bHasWish = glm::length(WishXz) > 1.0e-4f;
	const glm::vec2 WishN = bHasWish ? glm::normalize(WishXz) : glm::vec2{0.0f};

	for (FBodyInstance& Body : Bodies)
	{
		if (Body.LevelMeshIndex == InSkipLevelMeshIndex)
		{
			continue;
		}
		// ComplexAsSimple: sides come from TriangleMesh traces; world AABB is too fat for ramps.
		if (Body.CollisionShape == ECollisionShape::TriangleMesh)
		{
			continue;
		}
		const float Hx = Body.HalfExtents.x;
		const float Hy = Body.HalfExtents.y;
		const float Hz = Body.HalfExtents.z;
		const float Top = Body.Position.y + Hy;
		const float Bottom = Body.Position.y - Hy;

		if (Head < Bottom)
		{
			continue;
		}

		const bool bXzOnTop = XzDiscOverlapsAabb(Feet.x, Feet.z, R, Body.Position.x, Body.Position.z, Hx, Hz, -0.02f);
		// Standing on this top — no side push.
		if (FeetY >= Top - Params.Skin && bXzOnTop)
		{
			continue;
		}
		// Airborne over the volume (jump/clearance) — no side push.
		// Still resolve when elevated *beside* a short ledge (step-up clearance).
		if (FeetY > Top && bXzOnTop)
		{
			continue;
		}

		glm::vec2 LocalNormal{};
		float Penetration = 0.0f;
		if (!CapsuleAabbMtv(Feet.x, Feet.z, R, Body.Position.x, Body.Position.z, Hx, Hz, LocalNormal, Penetration))
		{
			continue;
		}

		if (Body.Type == EBodyType::Static)
		{
			Feet.x += LocalNormal.x * Penetration;
			Feet.z += LocalNormal.y * Penetration;
			ClampPositionXZ(Feet, Params.WalkBounds);
			continue;
		}

		// Dynamic: mass-weighted depenetration (player ≈ fixed mass 80 for share).
		constexpr float PlayerMass = 80.0f;
		const float BodyMass = std::max(Body.Mass, 0.5f);
		const float InvSum = 1.0f / (PlayerMass + BodyMass);
		const float PlayerShare = BodyMass * InvSum;
		const float BodyShare = PlayerMass * InvSum;

		Feet.x += LocalNormal.x * (Penetration * PlayerShare);
		Feet.z += LocalNormal.y * (Penetration * PlayerShare);
		Body.Position.x -= LocalNormal.x * (Penetration * BodyShare);
		Body.Position.z -= LocalNormal.y * (Penetration * BodyShare);
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

bool FPhysScene::ApplyCapsuleSweepPush(std::size_t LevelMeshIndex, const glm::vec2& WishXz,
	const glm::vec3& ImpactNormal, float InPushStrength, float InWalkBounds)
{
	if (LevelMeshIndex == ULevel::Npos || glm::length(WishXz) <= 1.0e-4f)
	{
		return false;
	}

	glm::vec2 LocalNormal{ImpactNormal.x, ImpactNormal.z};
	const float NLen = glm::length(LocalNormal);
	if (NLen <= 1.0e-4f)
	{
		return false;
	}
	LocalNormal /= NLen;
	const glm::vec2 WishN = glm::normalize(WishXz);

	for (FBodyInstance& Body : Bodies)
	{
		if (Body.LevelMeshIndex != LevelMeshIndex || Body.Type != EBodyType::Dynamic)
		{
			continue;
		}
		const float Into = std::max(0.0f, -glm::dot(WishN, LocalNormal));
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

	const float Damp = std::exp(-Params.Damping * Params.DeltaTime);

	auto SupportUnderAabb = [&](const FBodyInstance& Body) -> float
	{
		float Support = Params.FloorY;
		const float Bx0 = Body.Position.x - Body.HalfExtents.x;
		const float Bx1 = Body.Position.x + Body.HalfExtents.x;
		const float Bz0 = Body.Position.z - Body.HalfExtents.z;
		const float Bz1 = Body.Position.z + Body.HalfExtents.z;
		const float Bottom = Body.Position.y - Body.HalfExtents.y;

		for (const FBodyInstance& Other : Bodies)
		{
			if (Other.LevelMeshIndex == Body.LevelMeshIndex || Other.LevelMeshIndex == Params.SkipLevelMeshIndex)
			{
				continue;
			}
			const float Ox0 = Other.Position.x - Other.HalfExtents.x;
			const float Ox1 = Other.Position.x + Other.HalfExtents.x;
			const float Oz0 = Other.Position.z - Other.HalfExtents.z;
			const float Oz1 = Other.Position.z + Other.HalfExtents.z;
			if (Bx1 < Ox0 || Bx0 > Ox1 || Bz1 < Oz0 || Bz0 > Oz1)
			{
				continue;
			}

			const float Top = Other.Position.y + Other.HalfExtents.y;
			// Dynamic support only when this body is clearly above the other (stacking).
			if (Other.Type == EBodyType::Dynamic && Bottom + Params.Skin < Top - 0.02f &&
				Body.Position.y <= Other.Position.y)
			{
				continue;
			}
			Support = std::max(Support, Top);
		}
		return Support;
	};

	// 1) Integrate velocities (no floor snap yet).
	for (FBodyInstance& Body : Bodies)
	{
		if (Body.Type != EBodyType::Dynamic)
		{
			Body.VelXz = {};
			Body.VelocityY = 0.0f;
			continue;
		}

		if (Body.bEnableGravity)
		{
			Body.VelocityY -= Params.Gravity * Params.DeltaTime;
			Body.Position.y += Body.VelocityY * Params.DeltaTime;
		}
		else
		{
			Body.VelocityY = 0.0f;
		}

		if (glm::length(Body.VelXz) >= 1.0e-3f)
		{
			Body.Position.x += Body.VelXz.x * Params.DeltaTime;
			Body.Position.z += Body.VelXz.y * Params.DeltaTime;
			ClampPositionXZ(Body.Position, Params.WalkBounds);
			Body.VelXz *= Damp;
		}
		else
		{
			Body.VelXz = {};
		}
	}

	// 2) Resolve overlaps on min-penetration axis (XZ sides vs Y stacking).
	constexpr int Iterations = 6;
	for (int Iter = 0; Iter < Iterations; ++Iter)
	{
		for (std::size_t I = 0; I < Bodies.size(); ++I)
		{
			if (Bodies[I].LevelMeshIndex == Params.SkipLevelMeshIndex)
			{
				continue;
			}
			for (std::size_t J = I + 1; J < Bodies.size(); ++J)
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
					const float Sum = std::max(A.Mass + B.Mass, 1.0e-3f);
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

				glm::vec3 LocalNormal{};
				if (!SeparateAabb(A.Position, A.HalfExtents, B.Position, B.HalfExtents, MoveA, MoveB, &LocalNormal))
				{
					continue;
				}

				ClampPositionXZ(A.Position, Params.WalkBounds);
				ClampPositionXZ(B.Position, Params.WalkBounds);

				if (bADyn)
				{
					CancelVelocityInto(A.VelXz, {LocalNormal.x, LocalNormal.z});
					CancelVelocityYInto(A.VelocityY, LocalNormal.y);
				}
				if (bDyn)
				{
					CancelVelocityInto(B.VelXz, {-LocalNormal.x, -LocalNormal.z});
					CancelVelocityYInto(B.VelocityY, -LocalNormal.y);
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
		const float Bottom = Body.Position.y - Body.HalfExtents.y;
		const float Window = LandWindow(Body.VelocityY, Params.DeltaTime, Params.Skin);
		if (Body.VelocityY <= 0.0f && Bottom <= Support + Params.Skin && Bottom >= Support - Window)
		{
			Body.Position.y = Support + Body.HalfExtents.y;
			Body.VelocityY = 0.0f;
			// Resting friction: kill tiny residual slide when fully supported.
			if (glm::length(Body.VelXz) < 0.08f)
			{
				Body.VelXz = {};
			}
		}
	}
}

void FPhysScene::AppendCollisionDebug(
	FDebugDraw& Draw, const FCapsuleShape& Capsule, const glm::vec3& Feet, std::size_t InSkipLevelMeshIndex) const
{
	const float R = Capsule.Radius;
	const float H = Capsule.Height;
	const float CylBottom = std::min(R, H * 0.5f);
	const float CylTop = std::max(H - R, CylBottom);
	constexpr glm::vec3 CapsuleColor{0.2f, 1.0f, 0.45f};
	constexpr int Seg = 12;

	const glm::vec3 B0 = Feet + glm::vec3{0.0f, CylBottom, 0.0f};
	const glm::vec3 T0 = Feet + glm::vec3{0.0f, CylTop, 0.0f};
	Draw.AddLine(B0 + glm::vec3{R, 0, 0}, T0 + glm::vec3{R, 0, 0}, CapsuleColor);
	Draw.AddLine(B0 + glm::vec3{-R, 0, 0}, T0 + glm::vec3{-R, 0, 0}, CapsuleColor);
	Draw.AddLine(B0 + glm::vec3{0, 0, R}, T0 + glm::vec3{0, 0, R}, CapsuleColor);
	Draw.AddLine(B0 + glm::vec3{0, 0, -R}, T0 + glm::vec3{0, 0, -R}, CapsuleColor);

	AppendCapsuleRing(Draw, Feet + glm::vec3{0.0f, CylBottom, 0.0f}, R, CapsuleColor, Seg);
	AppendCapsuleRing(Draw, Feet + glm::vec3{0.0f, CylTop, 0.0f}, R, CapsuleColor, Seg);

	if (CylBottom > 1.0e-3f)
	{
		AppendCapsuleRing(Draw, Feet + glm::vec3{0.0f, CylBottom * 0.5f, 0.0f}, R * 0.85f, CapsuleColor, Seg / 2);
	}
	if (H - CylTop > 1.0e-3f)
	{
		const float CapMidY = CylTop + ((H - CylTop) * 0.5f);
		AppendCapsuleRing(Draw, Feet + glm::vec3{0.0f, CapMidY, 0.0f}, R * 0.85f, CapsuleColor, Seg / 2);
	}
	AppendCapsuleRing(Draw, Feet, R * 0.35f, CapsuleColor, 6);
	AppendCapsuleRing(Draw, Feet + glm::vec3{0.0f, H, 0.0f}, R * 0.35f, CapsuleColor, 6);

	AppendBodiesCollisionDebug(Draw, InSkipLevelMeshIndex);
}

void FPhysScene::AppendBodiesCollisionDebug(FDebugDraw& Draw, std::size_t InSkipLevelMeshIndex) const
{
	constexpr glm::vec3 DynamicColor{1.0f, 0.55f, 0.15f};
	constexpr glm::vec3 StaticColor{0.35f, 0.65f, 1.0f};
	constexpr glm::vec3 TriMeshColor{0.25f, 0.9f, 1.0f};
	for (std::size_t Bi = 0; Bi < Bodies.size(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (Body.LevelMeshIndex == InSkipLevelMeshIndex)
		{
			continue;
		}

		// TriangleMesh: draw actual tris (oriented). AABB alone looks like a fat unrotated box.
		if (Body.CollisionShape == ECollisionShape::TriangleMesh && Bi < TriangleMeshes.size() &&
			TriangleMeshes[Bi].IsValid())
		{
			const FTriangleMeshCollision& Mesh = TriangleMeshes[Bi];
			for (std::size_t I = 0; I + 2 < Mesh.Indices.size(); I += 3)
			{
				const glm::vec3& V0 = Mesh.Positions[Mesh.Indices[I]];
				const glm::vec3& V1 = Mesh.Positions[Mesh.Indices[I + 1]];
				const glm::vec3& V2 = Mesh.Positions[Mesh.Indices[I + 2]];
				Draw.AddLine(V0, V1, TriMeshColor);
				Draw.AddLine(V1, V2, TriMeshColor);
				Draw.AddLine(V2, V0, TriMeshColor);
			}
			continue;
		}

		const glm::vec3 Mn = Body.Position - Body.HalfExtents;
		const glm::vec3 Mx = Body.Position + Body.HalfExtents;
		Draw.AddAabb(Mn, Mx, Body.Type == EBodyType::Dynamic ? DynamicColor : StaticColor);
	}

	// Walkable slope planes (AddSlopeRamp) — magenta wire quads for F2.
	constexpr glm::vec3 SlopeColor{0.95f, 0.2f, 0.85f};
	for (const FSlopePlane& Plane : SlopePlanes)
	{
		const float Hx = Plane.BoundsHalfExtents.x;
		const float Hz = Plane.BoundsHalfExtents.z;
		const glm::vec3& C = Plane.BoundsCenter;
		const float NLen = glm::length(Plane.Normal);
		if (NLen < 1.0e-6f || std::abs(Plane.Normal.y) < 1.0e-4f)
		{
			continue;
		}
		const glm::vec3 N = Plane.Normal / NLen;
		auto YAt = [&](float X, float Z)
		{ return Plane.Point.y - ((N.x * (X - Plane.Point.x)) + (N.z * (Z - Plane.Point.z))) / N.y; };
		const glm::vec3 P00{C.x - Hx, YAt(C.x - Hx, C.z - Hz), C.z - Hz};
		const glm::vec3 P10{C.x + Hx, YAt(C.x + Hx, C.z - Hz), C.z - Hz};
		const glm::vec3 P11{C.x + Hx, YAt(C.x + Hx, C.z + Hz), C.z + Hz};
		const glm::vec3 P01{C.x - Hx, YAt(C.x - Hx, C.z + Hz), C.z + Hz};
		Draw.AddLine(P00, P10, SlopeColor);
		Draw.AddLine(P10, P11, SlopeColor);
		Draw.AddLine(P11, P01, SlopeColor);
		Draw.AddLine(P01, P00, SlopeColor);
		Draw.AddLine(P00, P11, SlopeColor);
		Draw.AddLine(P10, P01, SlopeColor);
	}
}
