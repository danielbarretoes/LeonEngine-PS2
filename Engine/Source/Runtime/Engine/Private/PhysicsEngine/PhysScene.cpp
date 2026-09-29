#include "Physics/PhysScene.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/DebugDraw.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Frustum.h"
#include "GameFramework/Actor.h"
#include "HAL/LowLevelMemTracker.h"
#include "MeshData.h"
#include "PhysicsEngine/BodySetup.h"
#include "Stats/Stats.h"
#include "TriangleCollision.h"

DECLARE_CYCLE_STAT(TEXT("Physics Step"), STAT_PhysSceneStep, STATGROUP_Physics);

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
	/** Height (cm) that stands for "any" in a broadphase box. */
	constexpr float UnboundedZ = 1.0e30f;
	/** How far (cm) ResolveCapsuleSides may push the feet before it looks the bodies around them up again. */
	constexpr float ContactSlack = 32.0f;
	/** How far (cm) the step may push a body before it looks its pairs up again. */
	constexpr float PairSlack = 25.0f;

	/** A pair of bodies of the step: the bodies' serials (the pair's order) and indices, the earlier body first. */
	struct FStepPair
	{
		uint32 SerialA;
		uint32 SerialB;
		int32 IndexA;
		int32 IndexB;
	};

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
	BodyOwners.Reset();
	BodySerials.Reset();
	NextBodySerial = 0;
	ComponentBodies.Reset();
	Broadphase.Reset();
	bBodiesEdited = false;
}

int32 FPhysScene::AddUnregisteredBody(const FBodyInstanceDesc& Desc)
{
	LLM_SCOPE(ELLMTag::Physics);
	SyncEditedBodies();
	FBodyInstance Body;
	Body.ComponentID = Desc.ComponentID;
	Body.Type = Desc.Type;
	Body.Mass = Desc.Mass > 0.0f ? Desc.Mass : 0.0f;
	Body.bEnableGravity = Desc.bEnableGravity;
	Body.SetDefaultCollision(Desc.Type);
	Bodies.Add(Body);
	TriangleMeshes.AddDefaulted();
	BodyOwners.Add(nullptr);
	BodySerials.Add(NextBodySerial++);
	return Bodies.Num() - 1;
}

int32 FPhysScene::AddBody(const FBodyInstanceDesc& Desc)
{
	const int32 BodyIndex = AddUnregisteredBody(Desc);
	Broadphase.AddBody(Bodies[BodyIndex], GetBodyMesh(BodyIndex), Bodies[BodyIndex].Type == EBodyType::Dynamic);
	return BodyIndex;
}

int32 FPhysScene::AddComponentBody(UPrimitiveComponent& Component)
{
	LLM_SCOPE(ELLMTag::Physics);
	// One body per component (UE: the component's FBodyInstance).
	RemoveComponentBody(Component);

	FBodyInstanceDesc Desc{};
	Desc.ComponentID = Component.GetUniqueID();
	Desc.Type = Component.IsSimulatingPhysics() ? EBodyType::Dynamic : EBodyType::Static;
	Desc.bEnableGravity = Component.IsGravityEnabled();
	const int32 BodyIndex = AddUnregisteredBody(Desc);
	BodyOwners[BodyIndex] = &Component;
	ComponentBodies.Add(Component.GetUniqueID(), BodyIndex);

	// The component's collision settings (UE: FBodyInstance's ObjectType, CollisionResponses, CollisionEnabled).
	FBodyInstance& Body = Bodies[BodyIndex];
	const AActor* Owner = Component.GetOwner();
	Body.OwnerID = Owner != nullptr ? static_cast<SIZE_T>(Owner->GetUniqueID()) : NoComponentID;
	Body.ObjectType = Component.GetCollisionObjectType();
	Body.CollisionResponses = Component.GetCollisionResponseToChannels();
	const ECollisionEnabled CollisionEnabled = Component.GetCollisionEnabled();
	Body.bQueryEnabled =
		CollisionEnabled == ECollisionEnabled::QueryOnly || CollisionEnabled == ECollisionEnabled::QueryAndPhysics;
	Body.bPhysicsEnabled =
		CollisionEnabled == ECollisionEnabled::PhysicsOnly || CollisionEnabled == ECollisionEnabled::QueryAndPhysics;

	ApplyComponentShape(BodyIndex, Component);
	// A movable component's body goes to the broadphase's list at once; a static one to the tree (and to the list the
	// first time it moves).
	Broadphase.AddBody(Body, GetBodyMesh(BodyIndex),
		Body.Type == EBodyType::Dynamic || Component.Mobility == EComponentMobility::Movable);
	return BodyIndex;
}

int32 FPhysScene::FindComponentBody(const UPrimitiveComponent& Component) const
{
	const int32* BodyIndex = ComponentBodies.Find(Component.GetUniqueID());
	return BodyIndex != nullptr && BodyOwners.IsValidIndex(*BodyIndex) && BodyOwners[*BodyIndex] == &Component
		? *BodyIndex
		: INDEX_NONE;
}

void FPhysScene::UpdateComponentBodyTransform(const UPrimitiveComponent& Component)
{
	const int32 BodyIndex = FindComponentBody(Component);
	if (BodyIndex != INDEX_NONE)
	{
		UpdateBodyFromComponent(BodyIndex, Component);
	}
}

void FPhysScene::RemoveComponentBody(const UPrimitiveComponent& Component)
{
	const int32 BodyIndex = FindComponentBody(Component);
	if (BodyIndex != INDEX_NONE)
	{
		RemoveBodyAtSwap(BodyIndex);
	}
}

void FPhysScene::RemoveBodyAtSwap(int32 BodyIndex)
{
	SyncEditedBodies();
	if (const UPrimitiveComponent* Owner = BodyOwners[BodyIndex])
	{
		ComponentBodies.Remove(Owner->GetUniqueID());
	}
	Broadphase.RemoveBodyAtSwap(BodyIndex);
	Bodies.RemoveAtSwap(BodyIndex, 1, false);
	TriangleMeshes.RemoveAtSwap(BodyIndex, 1, false);
	BodyOwners.RemoveAtSwap(BodyIndex, 1, false);
	BodySerials.RemoveAtSwap(BodyIndex, 1, false);
	if (BodyIndex < Bodies.Num() && BodyOwners[BodyIndex] != nullptr)
	{
		ComponentBodies.Add(BodyOwners[BodyIndex]->GetUniqueID(), BodyIndex);
	}
}

UPrimitiveComponent* FPhysScene::GetBodyOwner(int32 BodyIndex) const
{
	return BodyOwners.IsValidIndex(BodyIndex) ? BodyOwners[BodyIndex] : nullptr;
}

void FPhysScene::SyncComponentsToBodies() const
{
	CatchUpEdits();
	FBodyIndexArray Simulated;
	Broadphase.ForEachMovable(
		[&](int32 BodyIndex)
		{
			if (Bodies[BodyIndex].Type == EBodyType::Dynamic && GetBodyOwner(BodyIndex) != nullptr)
			{
				Simulated.Add(BodyIndex);
			}
		});
	SortBodiesBySerial(Simulated);
	for (const int32 BodyIndex : Simulated)
	{
		BodyOwners[BodyIndex]->SetWorldLocation(Bodies[BodyIndex].Position);
	}
}

void FPhysScene::SyncEditedBodies()
{
	// Bodies appended to GetBodies(): no mesh, no owner, the next serials.
	if (BodySerials.Num() != Bodies.Num())
	{
		TriangleMeshes.SetNum(Bodies.Num());
		BodyOwners.SetNum(Bodies.Num());
		while (BodySerials.Num() < Bodies.Num())
		{
			BodySerials.Add(NextBodySerial++);
		}
		BodySerials.SetNum(Bodies.Num());
	}
	CatchUpEdits();
}

void FPhysScene::CatchUpEdits() const
{
	if (bBodiesEdited)
	{
		Broadphase.Refresh(Bodies, TriangleMeshes);
		bBodiesEdited = false;
	}
}

void FPhysScene::UpdateBroadphase() const
{
	CatchUpEdits();
	Broadphase.Flush();
}

void FPhysScene::NotifyBodyMoved(int32 BodyIndex)
{
	CatchUpEdits();
	Broadphase.UpdateBody(BodyIndex, Bodies[BodyIndex], GetBodyMesh(BodyIndex));
}

void FPhysScene::SortBodiesBySerial(FBodyIndexArray& BodyIndices) const
{
	BodyIndices.Sort([this](const int32 A, const int32 B) { return GetBodySerial(A) < GetBodySerial(B); });
}

const FTriangleMeshCollision* FPhysScene::GetBodyMesh(int32 BodyIndex) const
{
	return FPhysSceneBroadphase::GetBodyMesh(Bodies[BodyIndex], BodyIndex, TriangleMeshes);
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

void FPhysScene::UpdateBodyFromComponent(int32 BodyIndex, const UPrimitiveComponent& Component)
{
	SyncEditedBodies();
	ApplyComponentShape(BodyIndex, Component);
	NotifyBodyMoved(BodyIndex);
}

void FPhysScene::ApplyComponentShape(int32 BodyIndex, const UPrimitiveComponent& Component)
{
	FBodyInstance& Body = Bodies[BodyIndex];
	FTriangleMeshCollision& TriMesh = TriangleMeshes[BodyIndex];
	TriMesh.Clear();
	Body.CollisionShape = EBodyCollisionShape::Box;

	const FTransform Transform = Component.GetComponentTransform();
	const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(&Component);
	const UStaticMesh* Mesh = MeshComponent != nullptr ? MeshComponent->GetStaticMesh() : nullptr;
	if (Mesh != nullptr)
	{
		// The mesh's body setup (UBodySetup): its boxes, else the mesh's bounding box, as the simple shape; the
		// triangles of a static body unless the setup asks for the simple shape everywhere.
		const UBodySetup* BodySetup = Mesh->GetBodySetup();
		const FMatrix Model = Transform.ToMatrixWithScale();
		const FBox LocalBox = Mesh->GetBoundingBox();
		const FBox WorldAabb = BodySetup != nullptr && BodySetup->AggGeom.GetElementCount() > 0
			? BodySetup->AggGeom.CalcAABB(Transform)
			: TransformLocalBox(LocalBox.Min, LocalBox.Max, Model);
		Body.Position = WorldAabb.GetCenter();
		Body.HalfExtents = WorldAabb.GetExtent();

		// UE ComplexAsSimple lite: static meshes with collision triangles use triangle queries.
		const bool bComplexAsSimple = BodySetup == nullptr || BodySetup->UsesComplexAsSimpleForStaticBodies();
		const FTriMeshCollisionData& Collision = Mesh->GetPhysicsTriMeshData();
		if (Body.Type == EBodyType::Static && bComplexAsSimple && !Collision.IsEmpty())
		{
			TriMesh.Positions.SetNum(Collision.Vertices.Num());
			for (int32 Vi = 0; Vi < TriMesh.Positions.Num(); ++Vi)
			{
				TriMesh.Positions[Vi] = FVector(Model.TransformPosition(Collision.Vertices[Vi]));
			}
			TriMesh.Indices = Collision.Indices;
			if (TriMesh.IsValid())
			{
				Body.CollisionShape = EBodyCollisionShape::TriangleMesh;
				TriMesh.BuildTree();
			}
			else
			{
				TriMesh.Clear();
			}
		}
	}
	else if (const UBoxComponent* Box = Cast<UBoxComponent>(&Component))
	{
		// Plan decision D16: a volume's brush is a box; its body is the axis-aligned scaled box.
		Body.Position = Transform.GetLocation();
		Body.HalfExtents = Box->GetScaledBoxExtent();
	}
	else if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(&Component))
	{
		// An upright capsule: centred on the component (UE), or standing on it (the character's, whose actor
		// location is the feet).
		const float Radius = Capsule->GetScaledCapsuleRadius();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		Body.CollisionShape = EBodyCollisionShape::Capsule;
		Body.Position =
			Transform.GetLocation() + FVector(0.0f, 0.0f, Capsule->bBaseAtComponentLocation ? HalfHeight : 0.0f);
		Body.HalfExtents = FVector(Radius, Radius, HalfHeight);
	}
	else if (const USphereComponent* Sphere = Cast<USphereComponent>(&Component))
	{
		// A sphere is a capsule without a cylinder.
		const float Radius = Sphere->GetScaledSphereRadius();
		Body.CollisionShape = EBodyCollisionShape::Capsule;
		Body.Position = Transform.GetLocation();
		Body.HalfExtents = FVector(Radius);
	}
	else
	{
		Body.Position = Transform.GetLocation();
		HalfExtentsFromScale(Transform.GetScale3D(), Body.HalfExtents.X, Body.HalfExtents.Y, Body.HalfExtents.Z);
	}
	if (Body.Mass <= 0.0f)
	{
		Body.Mass = MassFromHalfExtents(Body.HalfExtents.X, Body.HalfExtents.Y, Body.HalfExtents.Z);
	}
}

float FPhysScene::QuerySupportZ(const FCollisionShape& Capsule, const FVector& Feet, float InFloorZ, float InStepUp,
	float InSkin, SIZE_T InIgnoreComponentID, ECollisionChannel TraceChannel,
	const FCollisionResponseParams& ResponseParam) const
{
	UpdateBroadphase();
	float Support = InFloorZ;
	const float R = Capsule.GetCapsuleRadius();
	FCollisionQueryParams Query;
	Query.IgnoreComponentID = InIgnoreComponentID;

	// The bodies under the capsule's disc, at any height (a support is the highest top below the step).
	const float Reach = FMath::Abs(R) + (FMath::Abs(SupportDiscInset) * 2.0f);
	const FBox Column(
		FVector(Feet.X - Reach, Feet.Y - Reach, -UnboundedZ), FVector(Feet.X + Reach, Feet.Y + Reach, UnboundedZ));
	Broadphase.ForEachOverlap(Column,
		[&](int32 Bi)
		{
			const FBodyInstance& Body = Bodies[Bi];
			if (GetBodyQueryResponse(Body, TraceChannel, Query, ResponseParam) != ECR_Block)
			{
				return;
			}
			if (!XYDiscOverlapsAabb(Feet.X, Feet.Y, R, Body.Position.X, Body.Position.Y, Body.HalfExtents.X,
					Body.HalfExtents.Y, SupportDiscInset))
			{
				return;
			}

			if (const FTriangleMeshCollision* Mesh = GetBodyMesh(Bi))
			{
				// Vertical probe: walkable triangle tops under the capsule disc ComplexAsSimple (margins in cm).
				constexpr float ProbeAbove = 50.0f;
				constexpr float ProbeBelowFloor = 100.0f;
				const float RayTop = FMath::Max(
					Feet.Z + InStepUp + InSkin + ProbeAbove, Body.Position.Z + Body.HalfExtents.Z + ProbeAbove);
				const FVector Start(Feet.X, Feet.Y, RayTop);
				const FVector End(Feet.X, Feet.Y, InFloorZ - ProbeBelowFloor);
				float T = 1.0f;
				FVector LocalNormal = FVector::ZeroVector;
				if (SegmentTriangleMesh(Start, End, *Mesh, 0.0f, T, LocalNormal) && LocalNormal.Z > 0.15f)
				{
					const float ZHit = Start.Z + ((End.Z - Start.Z) * T);
					if (ZHit <= Feet.Z + InStepUp + InSkin)
					{
						Support = FMath::Max(Support, ZHit);
					}
				}
				return;
			}

			const float Top = Body.Position.Z + Body.HalfExtents.Z;
			// Skip tops too high to step onto (the side collision handles walls).
			if (Feet.Z + InStepUp + InSkin < Top)
			{
				return;
			}
			Support = FMath::Max(Support, Top);
		});

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
	const FCapsuleContactParams& Params, SIZE_T InIgnoreComponentID, bool bApplyPush, ECollisionChannel TraceChannel,
	const FCollisionResponseParams& ResponseParam)
{
	SyncEditedBodies();
	UpdateBroadphase();
	const float R = Capsule.GetCapsuleRadius();
	const float FeetZ = Feet.Z;
	const float Head = FeetZ + Capsule.GetCapsuleHalfHeight() * 2.0f;
	const bool bHasWish = WishXY.Size() > 1.0e-4f;
	const FVector2D WishN = bHasWish ? WishXY.GetSafeNormal() : FVector2D::ZeroVector;
	FCollisionQueryParams Query;
	Query.IgnoreComponentID = InIgnoreComponentID;

	auto ResolveSide = [&](int32 BodyIndex)
	{
		FBodyInstance& Body = Bodies[BodyIndex];
		if (GetBodyQueryResponse(Body, TraceChannel, Query, ResponseParam) != ECR_Block)
		{
			return;
		}
		// ComplexAsSimple: the sides come from TriangleMesh traces; the world AABB is too fat for ramps.
		if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh)
		{
			return;
		}
		const float Hx = Body.HalfExtents.X;
		const float Hy = Body.HalfExtents.Y;
		const float Hz = Body.HalfExtents.Z;
		const float Top = Body.Position.Z + Hz;
		const float Bottom = Body.Position.Z - Hz;

		if (Head < Bottom)
		{
			return;
		}

		// A top at the feet (within the skin) is floor, not a wall (N29): the capsule's lowest point is the feet, so a
		// box under them cannot push it sideways, however little of the disc is over it (a seam between two floor
		// boxes whose tops meet, as UE's CharacterMovementComponent walks across).
		if (FeetZ >= Top - Params.Skin)
		{
			return;
		}

		FVector2D LocalNormal = FVector2D::ZeroVector;
		float Penetration = 0.0f;
		if (!CapsuleAabbMtv(Feet.X, Feet.Y, R, Body.Position.X, Body.Position.Y, Hx, Hy, LocalNormal, Penetration))
		{
			return;
		}

		if (Body.Type == EBodyType::Static)
		{
			Feet.X += LocalNormal.X * Penetration;
			Feet.Y += LocalNormal.Y * Penetration;
			ClampPositionXY(Feet, Params.WalkBounds);
			return;
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

		if (bApplyPush && bHasWish)
		{
			ApplyDynamicWishPush(Body, WishN, LocalNormal, Params.PushStrength, Params.WalkBounds, false);
		}
		NotifyBodyMoved(BodyIndex);
	};

	// The bodies around the feet, in the order they were added; when a push takes the feet further than the slack,
	// the bodies not seen yet are looked up again around the new feet.
	const float Reach = FMath::Abs(R) + ContactSlack + (FMath::Abs(SupportDiscInset) * 2.0f);
	bool bAnySeen = false;
	uint32 LastSerial = 0;
	for (;;)
	{
		const FVector Anchor = Feet;
		FBodyIndexArray Candidates;
		Broadphase.ForEachOverlap(FBox(FVector(Anchor.X - Reach, Anchor.Y - Reach, -UnboundedZ),
									  FVector(Anchor.X + Reach, Anchor.Y + Reach, Head)),
			[&](int32 BodyIndex)
			{
				if (!bAnySeen || GetBodySerial(BodyIndex) > LastSerial)
				{
					Candidates.Add(BodyIndex);
				}
			});
		SortBodiesBySerial(Candidates);
		bool bDrifted = false;
		for (const int32 BodyIndex : Candidates)
		{
			bAnySeen = true;
			LastSerial = GetBodySerial(BodyIndex);
			ResolveSide(BodyIndex);
			if (FMath::Abs(Feet.X - Anchor.X) > ContactSlack || FMath::Abs(Feet.Y - Anchor.Y) > ContactSlack)
			{
				bDrifted = true;
				break;
			}
		}
		if (!bDrifted)
		{
			return;
		}
	}
}

bool FPhysScene::ApplyCapsuleSweepPush(
	SIZE_T ComponentID, const FVector2D& WishXY, const FVector& ImpactNormal, float InPushStrength, float InWalkBounds)
{
	if (ComponentID == NoComponentID || WishXY.Size() <= 1.0e-4f)
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

	// The first simulated body of the component (the simulated bodies are all in the broadphase's list).
	SyncEditedBodies();
	int32 Found = INDEX_NONE;
	Broadphase.ForEachMovable(
		[&](int32 BodyIndex)
		{
			const FBodyInstance& Body = Bodies[BodyIndex];
			if (Body.ComponentID == ComponentID && Body.Type == EBodyType::Dynamic &&
				(Found == INDEX_NONE || GetBodySerial(BodyIndex) < GetBodySerial(Found)))
			{
				Found = BodyIndex;
			}
		});
	if (Found == INDEX_NONE)
	{
		return false;
	}
	const float Into = FMath::Max(0.0f, -FVector2D::DotProduct(WishN, LocalNormal));
	if (Into <= 1.0e-4f)
	{
		return false;
	}
	ApplyDynamicWishPush(Bodies[Found], WishN, LocalNormal, InPushStrength, InWalkBounds, true);
	NotifyBodyMoved(Found);
	return true;
}

void FPhysScene::Step(const FPhysSceneStepParams& Params)
{
	SCOPE_CYCLE_COUNTER(STAT_PhysSceneStep);
	LLM_SCOPE(ELLMTag::Physics);
	SyncEditedBodies();
	UpdateBroadphase();
	const float Damp = FMath::Exp(-Params.Damping * Params.DeltaTime);

	// The simulated bodies, in the order they were added: only they move, so static pairs are never tested.
	FBodyIndexArray Simulated;
	Broadphase.ForEachMovable(
		[&](int32 BodyIndex)
		{
			if (Bodies[BodyIndex].Type == EBodyType::Dynamic)
			{
				Simulated.Add(BodyIndex);
			}
		});
	SortBodiesBySerial(Simulated);

	auto SupportUnderAabb = [&](const FBodyInstance& Body) -> float
	{
		float Support = Params.FloorZ;
		const float Bx0 = Body.Position.X - Body.HalfExtents.X;
		const float Bx1 = Body.Position.X + Body.HalfExtents.X;
		const float By0 = Body.Position.Y - Body.HalfExtents.Y;
		const float By1 = Body.Position.Y + Body.HalfExtents.Y;
		const float Bottom = Body.Position.Z - Body.HalfExtents.Z;

		// The bodies over the box's footprint, at any height.
		const FBox Column(FVector(FMath::Min(Bx0, Bx1), FMath::Min(By0, By1), -UnboundedZ),
			FVector(FMath::Max(Bx0, Bx1), FMath::Max(By0, By1), UnboundedZ));
		Broadphase.ForEachOverlap(Column,
			[&](int32 OtherIndex)
			{
				const FBodyInstance& Other = Bodies[OtherIndex];
				if (Other.ComponentID == Body.ComponentID || Other.ComponentID == Params.IgnoreComponentID ||
					!Other.bPhysicsEnabled)
				{
					return;
				}
				const float Ox0 = Other.Position.X - Other.HalfExtents.X;
				const float Ox1 = Other.Position.X + Other.HalfExtents.X;
				const float Oy0 = Other.Position.Y - Other.HalfExtents.Y;
				const float Oy1 = Other.Position.Y + Other.HalfExtents.Y;
				if (Bx1 < Ox0 || Bx0 > Ox1 || By1 < Oy0 || By0 > Oy1)
				{
					return;
				}

				const float Top = Other.Position.Z + Other.HalfExtents.Z;
				// Dynamic support only when this body is clearly above the other stacking.
				if (Other.Type == EBodyType::Dynamic && Bottom + Params.Skin < Top - 2.0f &&
					Body.Position.Z <= Other.Position.Z)
				{
					return;
				}
				Support = FMath::Max(Support, Top);
			});
		return Support;
	};

	// 1) Integrate velocities (no floor snap yet).
	for (const int32 BodyIndex : Simulated)
	{
		FBodyInstance& Body = Bodies[BodyIndex];
		if (!Body.bPhysicsEnabled)
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
		NotifyBodyMoved(BodyIndex);
	}

	// 2) Resolve overlaps on the min-penetration axis (XY sides vs Z stacking), pair by pair in the order of the
	// bodies (the first's serial, then the second's). The pairs come from the broadphase around each simulated body;
	// when a push takes a body further than the slack, the pairs not resolved yet are looked up again.
	auto IsStepping = [&](const FBodyInstance& Body)
	{ return Body.ComponentID != Params.IgnoreComponentID && Body.bPhysicsEnabled; };
	auto FindSimulated = [&](int32 BodyIndex)
	{
		for (int32 Slot = 0; Slot < Simulated.Num(); ++Slot)
		{
			if (Simulated[Slot] == BodyIndex)
			{
				return Slot;
			}
		}
		return static_cast<int32>(INDEX_NONE);
	};
	auto SeparatePairs = [&]()
	{
		bool bAnyDone = false;
		uint32 LastA = 0;
		uint32 LastB = 0;
		for (;;)
		{
			TArray<FStepPair, TInlineAllocator<64>> Pairs;
			TArray<FVector, TInlineAllocator<16>> Anchors;
			for (const int32 BodyIndex : Simulated)
			{
				const FBodyInstance& Body = Bodies[BodyIndex];
				Anchors.Add(Body.Position);
				if (!IsStepping(Body))
				{
					continue;
				}
				const FVector Reach = Body.HalfExtents.GetAbs() + FVector(2.0f * PairSlack);
				const uint32 Serial = GetBodySerial(BodyIndex);
				Broadphase.ForEachOverlap(FBox(Body.Position - Reach, Body.Position + Reach),
					[&](int32 OtherIndex)
					{
						if (OtherIndex == BodyIndex || !IsStepping(Bodies[OtherIndex]))
						{
							return;
						}
						const uint32 OtherSerial = GetBodySerial(OtherIndex);
						const FStepPair Pair = Serial < OtherSerial
							? FStepPair{Serial, OtherSerial, BodyIndex, OtherIndex}
							: FStepPair{OtherSerial, Serial, OtherIndex, BodyIndex};
						if (bAnyDone && (Pair.SerialA < LastA || (Pair.SerialA == LastA && Pair.SerialB <= LastB)))
						{
							return;
						}
						Pairs.Add(Pair);
					});
			}
			Pairs.Sort([](const FStepPair& L, const FStepPair& R)
				{ return L.SerialA < R.SerialA || (L.SerialA == R.SerialA && L.SerialB < R.SerialB); });

			bool bDrifted = false;
			for (int32 PairIndex = 0; PairIndex < Pairs.Num(); ++PairIndex)
			{
				const FStepPair& Pair = Pairs[PairIndex];
				// A pair of simulated bodies is found from both.
				if (PairIndex > 0 && Pairs[PairIndex - 1].SerialA == Pair.SerialA &&
					Pairs[PairIndex - 1].SerialB == Pair.SerialB)
				{
					continue;
				}
				bAnyDone = true;
				LastA = Pair.SerialA;
				LastB = Pair.SerialB;

				FBodyInstance& A = Bodies[Pair.IndexA];
				FBodyInstance& B = Bodies[Pair.IndexB];
				const FVector StartA = A.Position;
				const FVector StartB = B.Position;

				const bool bADyn = A.Type == EBodyType::Dynamic;
				const bool bDyn = B.Type == EBodyType::Dynamic;

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
				NotifyBodyMoved(Pair.IndexA);
				NotifyBodyMoved(Pair.IndexB);

				// A static body only moves when it is clamped into the walk bounds.
				auto HasDrifted = [&](int32 BodyIndex, const FVector& Before)
				{
					const int32 Slot = FindSimulated(BodyIndex);
					if (Slot == INDEX_NONE)
					{
						return Bodies[BodyIndex].Position != Before;
					}
					const FVector Moved = (Bodies[BodyIndex].Position - Anchors[Slot]).GetAbs();
					return Moved.GetMax() > PairSlack;
				};
				if (HasDrifted(Pair.IndexA, StartA) || HasDrifted(Pair.IndexB, StartB))
				{
					bDrifted = true;
					break;
				}
			}
			if (!bDrifted)
			{
				return;
			}
		}
	};
	constexpr int32 Iterations = 6;
	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		SeparatePairs();
	}

	// 3) Floor / platform snap only when landing from above.
	for (const int32 BodyIndex : Simulated)
	{
		FBodyInstance& Body = Bodies[BodyIndex];
		if (!Body.bEnableGravity || !Body.bPhysicsEnabled)
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
			NotifyBodyMoved(BodyIndex);
		}
	}
}

void FPhysScene::AppendCollisionDebug(
	FDebugDraw& Draw, const FCollisionShape& Capsule, const FVector& Feet, SIZE_T InIgnoreComponentID) const
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

	AppendBodiesCollisionDebug(Draw, InIgnoreComponentID);
}

void FPhysScene::AppendBodiesCollisionDebug(FDebugDraw& Draw, SIZE_T InIgnoreComponentID) const
{
	const FLinearColor DynamicColor(1.0f, 0.55f, 0.15f);
	const FLinearColor StaticColor(0.35f, 0.65f, 1.0f);
	const FLinearColor TriMeshColor(0.25f, 0.9f, 1.0f);
	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (Body.ComponentID == InIgnoreComponentID)
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

		if (Body.CollisionShape == EBodyCollisionShape::Capsule)
		{
			// An upright capsule: rings at the ends of the cylinder and at the tips, four side lines.
			const float Radius = Body.HalfExtents.X;
			const float Cylinder = FMath::Max(0.0f, Body.HalfExtents.Z - Radius);
			const FVector Top = Body.Position + FVector(0.0f, 0.0f, Cylinder);
			const FVector Bottom = Body.Position - FVector(0.0f, 0.0f, Cylinder);
			constexpr int32 CapsuleSegments = 12;
			AppendCapsuleRing(Draw, Top, Radius, StaticColor, CapsuleSegments);
			AppendCapsuleRing(Draw, Bottom, Radius, StaticColor, CapsuleSegments);
			Draw.AddLine(Top + FVector(Radius, 0.0f, 0.0f), Bottom + FVector(Radius, 0.0f, 0.0f), StaticColor);
			Draw.AddLine(Top - FVector(Radius, 0.0f, 0.0f), Bottom - FVector(Radius, 0.0f, 0.0f), StaticColor);
			Draw.AddLine(Top + FVector(0.0f, Radius, 0.0f), Bottom + FVector(0.0f, Radius, 0.0f), StaticColor);
			Draw.AddLine(Top - FVector(0.0f, Radius, 0.0f), Bottom - FVector(0.0f, Radius, 0.0f), StaticColor);
			AppendCapsuleRing(Draw, Top + FVector(0.0f, 0.0f, Radius * 0.7f), Radius * 0.7f, StaticColor, 8);
			AppendCapsuleRing(Draw, Bottom - FVector(0.0f, 0.0f, Radius * 0.7f), Radius * 0.7f, StaticColor, 8);
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
