#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Debug/DebugDraw.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/MemStack.h"
#include "Physics/PhysScene.h"
#include "Stats/Stats.h"
#include "TriangleCollision.h"

DECLARE_CYCLE_STAT(TEXT("Line Trace"), STAT_LineTrace, STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("Sweep"), STAT_Sweep, STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("Overlap"), STAT_Overlap, STATGROUP_Collision);

namespace
{

	const FLinearColor TraceMiss(0.25f, 0.85f, 1.0f);
	const FLinearColor TraceHitPath(0.2f, 1.0f, 0.35f);
	const FLinearColor TraceBeyond(1.0f, 0.25f, 0.2f);
	const FLinearColor TraceNormal(1.0f, 0.9f, 0.2f);
	const FLinearColor TraceShape(0.95f, 0.45f, 1.0f);

	void AddLine(FDebugDraw& Draw, const FVector& A, const FVector& B, const FLinearColor& Color)
	{
		Draw.AddLine(A, B, Color);
	}

	[[nodiscard]] bool PointInAabb(const FVector& P, const FVector& Center, const FVector& HalfExtents, float Inflate)
	{
		const FVector He = HalfExtents + FVector(Inflate);
		return FMath::Abs(P.X - Center.X) <= He.X && FMath::Abs(P.Y - Center.Y) <= He.Y &&
			FMath::Abs(P.Z - Center.Z) <= He.Z;
	}

	void WriteHit(FHitResult& Out, const FVector& Start, const FVector& End, float T, const FVector& Normal,
		SIZE_T ComponentID, bool bFloorPlane)
	{
		const FVector Delta = End - Start;
		const float SegLen = Delta.Size();
		Out.bBlockingHit = true;
		Out.Time = T;
		Out.Distance = SegLen * T;
		Out.Location = Start + (Delta * T);
		Out.ImpactPoint = Out.Location;
		Out.ImpactNormal = Normal;
		Out.TraceStart = Start;
		Out.TraceEnd = End;
		Out.ComponentID = ComponentID;
		Out.bFloorPlane = bFloorPlane;
	}

	/** A segment against a slope plane grown by a radius (and by a half height along its normal's Z). */
	[[nodiscard]] bool SweepSlopePlane(const FSlopePlane& Plane, const FVector& Start, const FVector& End, float Radius,
		float HalfHeight, FHitResult& OutHit)
	{
		const float NLen = Plane.Normal.Size();
		const float Nz = (NLen > 1.0e-6f) ? (FMath::Abs(Plane.Normal.Z) / NLen) : 1.0f;
		const float Inflate = Radius + (HalfHeight * Nz);
		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (!SegmentSlopePlane(Start, End, Plane, Inflate, T, Normal))
		{
			return false;
		}
		WriteHit(OutHit, Start, End, T, Normal, NoComponentID, false);
		OutHit.ImpactPoint = OutHit.Location - (Normal * Inflate);
		return true;
	}

	/** Nearest first. Stable, so equal times keep the order they were found in. */
	template <typename AllocatorType>
	void SortHitsByTime(TArray<FHitResult, AllocatorType>& Hits)
	{
		StableSort(
			Hits.GetData(), Hits.Num(), [](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });
	}

	/**
	 * Copies the nearest blocking Multi hit into OutHit (a UE Single trace returns the first blocking hit; the overlaps
	 * before it are not reported).
	 */
	[[nodiscard]] bool TakeNearestHit(
		TArrayView<const FHitResult> Hits, FHitResult& OutHit, const FVector& Start, const FVector& End)
	{
		OutHit = FHitResult();
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;
		OutHit.Time = 1.0f;
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.bBlockingHit)
			{
				OutHit = Hit;
				return true;
			}
		}
		return false;
	}

	/** A horizontal (XY) ring around Center. */
	void AddRingXY(
		FDebugDraw& Draw, const FVector& Center, float Radius, const FLinearColor& Color, int32 Segments = 16)
	{
		const float SegCount = static_cast<float>(Segments);
		for (int32 I = 0; I < Segments; ++I)
		{
			const float A0 = (static_cast<float>(I) / SegCount) * (2.0f * PI);
			const float A1 = (static_cast<float>(I + 1) / SegCount) * (2.0f * PI);
			AddLine(Draw, Center + FVector(FMath::Cos(A0) * Radius, FMath::Sin(A0) * Radius, 0.0f),
				Center + FVector(FMath::Cos(A1) * Radius, FMath::Sin(A1) * Radius, 0.0f), Color);
		}
	}

	void AddImpactMarker(FDebugDraw& Draw, const FHitResult& Hit)
	{
		/** Marker half size, normal arrow length and arrow head (cm). */
		constexpr float S = 8.0f;
		constexpr float NormalLength = 45.0f;
		AddLine(Draw, Hit.ImpactPoint + FVector(-S, 0, 0), Hit.ImpactPoint + FVector(S, 0, 0), TraceNormal);
		AddLine(Draw, Hit.ImpactPoint + FVector(0, -S, 0), Hit.ImpactPoint + FVector(0, S, 0), TraceNormal);
		AddLine(Draw, Hit.ImpactPoint + FVector(0, 0, -S), Hit.ImpactPoint + FVector(0, 0, S), TraceNormal);
		Draw.AddArrow(Hit.ImpactPoint, Hit.ImpactPoint + (Hit.ImpactNormal * NormalLength), TraceNormal, 12.0f, 7.0f);
	}

	void DrawTracePath(FDebugDraw& Draw, const FVector& Start, const FVector& End, TArrayView<const FHitResult> Hits)
	{
		if (Hits.Num() == 0)
		{
			AddLine(Draw, Start, End, TraceMiss);
			return;
		}
		const FHitResult& First = Hits[0];
		AddLine(Draw, Start, First.ImpactPoint, TraceHitPath);
		AddLine(Draw, First.ImpactPoint, End, TraceBeyond);
		for (const FHitResult& Hit : Hits)
		{
			AddImpactMarker(Draw, Hit);
		}
	}

	[[nodiscard]] bool ShouldDraw(const FCollisionQueryParams& Params, FDebugDraw* DebugDraw)
	{
		return DebugDraw != nullptr && Params.DrawDebugType == EDrawDebugTrace::ForOneFrame;
	}

	/** The capsule of an upright capsule body: its radius and the half height of its cylinder (without the caps). */
	void GetBodyCapsule(const FBodyInstance& Body, float& OutRadius, float& OutCylinderHalfHeight)
	{
		OutRadius = Body.HalfExtents.X;
		OutCylinderHalfHeight = FMath::Max(0.0f, Body.HalfExtents.Z - Body.HalfExtents.X);
	}

} // namespace

bool SegmentSlopePlane(
	const FVector& Start, const FVector& End, const FSlopePlane& Plane, float Inflate, float& OutT, FVector& OutNormal)
{
	const float NLen = Plane.Normal.Size();
	if (NLen < 1.0e-6f)
	{
		return false;
	}
	const FVector N = Plane.Normal / NLen;
	const float D0 = ((Start - Plane.Point) | N) - Inflate;
	const float D1 = ((End - Plane.Point) | N) - Inflate;
	if (D0 < 0.0f && D1 < 0.0f)
	{
		return false;
	}
	if (D0 > 0.0f && D1 > 0.0f)
	{
		return false;
	}
	const float Denom = D0 - D1;
	if (FMath::Abs(Denom) < 1.0e-6f)
	{
		return false;
	}
	const float T = D0 / Denom;
	if (T < 0.0f || T > 1.0f)
	{
		return false;
	}
	const FVector HitLoc = Start + ((End - Start) * T);
	if (!PointInAabb(HitLoc, Plane.BoundsCenter, Plane.BoundsHalfExtents, Inflate))
	{
		return false;
	}
	OutT = T;
	OutNormal = (D0 >= 0.0f) ? N : -N;
	return true;
}

FCollisionQueryParams::FCollisionQueryParams(FName InTraceTag, bool bInTraceComplex, const AActor* InIgnoreActor)
	: bTraceComplex(bInTraceComplex)
	, TraceTag(InTraceTag)
{
	AddIgnoredActor(InIgnoreActor);
}

void FCollisionQueryParams::AddIgnoredComponent(const UPrimitiveComponent* InIgnoreComponent)
{
	if (InIgnoreComponent != nullptr)
	{
		AddIgnoredComponentID(static_cast<SIZE_T>(InIgnoreComponent->GetUniqueID()));
	}
}

void FCollisionQueryParams::AddIgnoredActor(const AActor* InIgnoreActor)
{
	if (InIgnoreActor != nullptr)
	{
		AddIgnoredActorID(static_cast<SIZE_T>(InIgnoreActor->GetUniqueID()));
	}
}

void DrawDebugLineTrace(FDebugDraw& Draw, const FVector& Start, const FVector& End, TArrayView<const FHitResult> Hits)
{
	DrawTracePath(Draw, Start, End, Hits);
}

void DrawDebugSphereTrace(
	FDebugDraw& Draw, const FVector& Start, const FVector& End, float Radius, TArrayView<const FHitResult> Hits)
{
	const float R = FMath::Max(Radius, 0.0f);
	DrawTracePath(Draw, Start, End, Hits);
	AddRingXY(Draw, Start, R, TraceShape);
	AddRingXY(Draw, End, R, TraceShape);
	if (Hits.Num() > 0)
	{
		AddRingXY(Draw, Hits[0].ImpactPoint + FVector(0.0f, 0.0f, R), R, TraceHitPath);
	}
}

void DrawDebugCapsuleTrace(FDebugDraw& Draw, const FVector& Start, const FVector& End, float Radius, float HalfHeight,
	TArrayView<const FHitResult> Hits)
{
	const float R = FMath::Max(Radius, 0.0f);
	const float Hh = FMath::Max(HalfHeight, 0.0f);
	DrawTracePath(Draw, Start, End, Hits);
	AddRingXY(Draw, Start + FVector(0.0f, 0.0f, Hh), R, TraceShape);
	AddRingXY(Draw, Start - FVector(0.0f, 0.0f, Hh), R, TraceShape);
	AddRingXY(Draw, End + FVector(0.0f, 0.0f, Hh), R, TraceShape);
	AddRingXY(Draw, End - FVector(0.0f, 0.0f, Hh), R, TraceShape);
	AddLine(Draw, Start + FVector(R, 0.0f, -Hh), Start + FVector(R, 0.0f, Hh), TraceShape);
	AddLine(Draw, Start + FVector(-R, 0.0f, -Hh), Start + FVector(-R, 0.0f, Hh), TraceShape);
	if (Hits.Num() > 0)
	{
		AddRingXY(Draw, Hits[0].ImpactPoint, R, TraceHitPath);
	}
}

/**
 * FPhysScene's queries: the broadphase gives the candidate bodies, which take the tests every body took before it, in
 * the order the bodies were added, so the hits and their order are the same.
 */
struct FPhysSceneQuery
{
	/** The shape a trace sweeps. */
	enum class EShape : uint8
	{
		Line,
		Sphere,
		Capsule,
		Box,
	};

	/** A trace's swept shape (cm). */
	struct FSweep
	{
		EShape Shape = EShape::Line;
		/** Sphere or capsule: the radius, at least 0. */
		float Radius = 0.0f;
		/** Capsule: the cylinder's half height, at least 0. */
		float HalfHeight = 0.0f;
		/** Box: its half extents, and the largest of them (its triangles and capsules see a sphere of it). */
		FVector Extent = FVector::ZeroVector;
		float Bound = 0.0f;
		/** The floor plane (FCollisionQueryParams::bTraceFloorPlane) and the slope planes: not for a box. */
		bool bPlanes = true;

		[[nodiscard]] static FSweep MakeLine(bool bInPlanes)
		{
			FSweep Sweep;
			Sweep.bPlanes = bInPlanes;
			return Sweep;
		}
		[[nodiscard]] static FSweep MakeSphere(float InRadius)
		{
			FSweep Sweep;
			Sweep.Shape = EShape::Sphere;
			Sweep.Radius = FMath::Max(InRadius, 0.0f);
			return Sweep;
		}
		[[nodiscard]] static FSweep MakeCapsule(float InRadius, float InHalfHeight)
		{
			FSweep Sweep;
			Sweep.Shape = EShape::Capsule;
			Sweep.Radius = FMath::Max(InRadius, 0.0f);
			Sweep.HalfHeight = FMath::Max(InHalfHeight, 0.0f);
			return Sweep;
		}
		[[nodiscard]] static FSweep MakeBox(const FVector& InExtent)
		{
			FSweep Sweep;
			Sweep.Shape = EShape::Box;
			Sweep.Extent = InExtent;
			Sweep.Bound = FMath::Max(InExtent.X, FMath::Max(InExtent.Y, InExtent.Z));
			Sweep.bPlanes = false;
			return Sweep;
		}

		/** How far a body's box grows for the shape's centre (the capsule's cylinder is upright). */
		[[nodiscard]] FVector GetBoxExpand() const
		{
			switch (Shape)
			{
				case EShape::Sphere:
					return FVector(Radius);
				case EShape::Capsule:
					return FVector(Radius, Radius, HalfHeight + Radius);
				case EShape::Box:
					return Extent;
				case EShape::Line:
				default:
					return FVector::ZeroVector;
			}
		}

		/**
		 * How far the broadphase grows its boxes: as the bodies' boxes grow, and by the triangles' inflate on every
		 * axis (along a triangle's normal, which may lie on X or Y), at least 0.
		 */
		[[nodiscard]] FVector GetBroadphaseExpand() const
		{
			switch (Shape)
			{
				case EShape::Sphere:
					return FVector(Radius);
				case EShape::Capsule:
					return FVector(Radius + HalfHeight);
				case EShape::Box:
					return FVector(FMath::Max(Bound, 0.0f));
				case EShape::Line:
				default:
					return FVector::ZeroVector;
			}
		}

		/** How far the triangles are grown along their normal. */
		[[nodiscard]] float GetMeshInflate() const
		{
			switch (Shape)
			{
				case EShape::Sphere:
					return Radius;
				case EShape::Capsule:
					// Like the slope planes: radius + |Nz| * half height, approximated with radius + half height.
					return Radius + HalfHeight;
				case EShape::Box:
					return Bound;
				case EShape::Line:
				default:
					return 0.0f;
			}
		}
	};

	/** Which bodies a query sees and how: a trace channel with its responses, or object types (every one blocks). */
	struct FFilter
	{
		ECollisionChannel Channel;
		const FCollisionQueryParams& Params;
		const FCollisionResponseParams& ResponseParam;
		const FCollisionObjectQueryParams* ObjectParams;

		[[nodiscard]] ECollisionResponse GetResponse(const FBodyInstance& Body) const
		{
			if (ObjectParams != nullptr)
			{
				return Body.bQueryEnabled && !Params.IsIgnored(Body.ComponentID, Body.OwnerID) &&
						ObjectParams->Contains(Body.ObjectType.GetValue())
					? ECR_Block
					: ECR_Ignore;
			}
			return FPhysScene::GetBodyQueryResponse(Body, Channel, Params, ResponseParam);
		}
	};

	/**
	 * The swept shape against a body: an upright capsule grown by the shape, else the box grown by the shape and, for
	 * a triangle mesh, its triangles (the nearest at most MaxT along the segment, its index to OutFaceIndex; INDEX_NONE
	 * for a simple shape).
	 */
	[[nodiscard]] static bool SweepBody(const FPhysScene& Scene, int32 BodyIndex, const FSweep& Sweep,
		const FVector& Start, const FVector& End, float MaxT, float& OutT, FVector& OutNormal, int32& OutFaceIndex)
	{
		OutFaceIndex = INDEX_NONE;
		const FBodyInstance& Body = Scene.Bodies[BodyIndex];
		if (Body.CollisionShape == EBodyCollisionShape::Capsule)
		{
			float CapsuleRadius = 0.0f;
			float CapsuleCylinder = 0.0f;
			GetBodyCapsule(Body, CapsuleRadius, CapsuleCylinder);
			if (Sweep.Shape == EShape::Sphere)
			{
				CapsuleRadius = CapsuleRadius + Sweep.Radius;
			}
			else if (Sweep.Shape == EShape::Capsule)
			{
				CapsuleRadius = CapsuleRadius + Sweep.Radius;
				CapsuleCylinder = CapsuleCylinder + Sweep.HalfHeight;
			}
			else if (Sweep.Shape == EShape::Box)
			{
				CapsuleRadius = CapsuleRadius + Sweep.Bound;
			}
			return SegmentUprightCapsule(Start, End, Body.Position, CapsuleRadius, CapsuleCylinder, OutT, OutNormal);
		}

		FVector Mn = Body.Position - Body.HalfExtents;
		FVector Mx = Body.Position + Body.HalfExtents;
		if (Sweep.Shape != EShape::Line)
		{
			const FVector Expand = Sweep.GetBoxExpand();
			Mn = Mn - Expand;
			Mx = Mx + Expand;
		}
		float TAabb = 1.0f;
		FVector NAabb = FVector::ZeroVector;
		if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb))
		{
			return false;
		}
		OutT = TAabb;
		OutNormal = NAabb;
		if (const FTriangleMeshCollision* Mesh = Scene.GetBodyMesh(BodyIndex))
		{
			// The box only culls: the hit is on a triangle.
			float TMesh = 1.0f;
			FVector NMesh = FVector::ZeroVector;
			if (!SegmentTriangleMesh(Start, End, *Mesh, Sweep.GetMeshInflate(), TMesh, NMesh, MaxT, &OutFaceIndex))
			{
				return false;
			}
			OutT = TMesh;
			OutNormal = NMesh;
		}
		return true;
	}

	/**
	 * The hit on a body (UE FHitResult: Location is the shape's centre, ImpactPoint the contact on the surface), with
	 * its physical material when the query asks for it.
	 */
	static void MakeBodyHit(const FPhysScene& Scene, int32 BodyIndex, bool bBlocking, const FSweep& Sweep,
		const FVector& Start, const FVector& End, float T, const FVector& Normal, int32 FaceIndex,
		const FCollisionQueryParams& Params, FHitResult& OutHit)
	{
		WriteHit(OutHit, Start, End, T, Normal, Scene.Bodies[BodyIndex].ComponentID, false);
		switch (Sweep.Shape)
		{
			case EShape::Sphere:
				OutHit.ImpactPoint = OutHit.Location - (Normal * Sweep.Radius);
				break;
			case EShape::Capsule:
			{
				const float Pull = (FMath::Abs(Normal.Z) > 0.5f) ? (Sweep.HalfHeight + Sweep.Radius) : Sweep.Radius;
				OutHit.ImpactPoint = OutHit.Location - (Normal * Pull);
				break;
			}
			case EShape::Box:
				OutHit.ImpactPoint = OutHit.Location - (Normal * FMath::Abs(Normal | Sweep.Extent));
				break;
			case EShape::Line:
			default:
				break;
		}
		OutHit.bBlockingHit = bBlocking;
		OutHit.FaceIndex = FaceIndex;
		Scene.SetHitBody(OutHit, BodyIndex);
		if (Params.bReturnPhysicalMaterial)
		{
			OutHit.PhysMaterial = FPhysScene::GetHitPhysicalMaterial(OutHit.GetComponent(), FaceIndex);
		}
	}

	/** The shape against the floor plane at FloorZ (the plane raised by the shape under its centre). */
	[[nodiscard]] static bool SweepFloor(
		const FSweep& Sweep, const FVector& Start, const FVector& End, float FloorZ, FHitResult& OutHit)
	{
		float PlaneZ = FloorZ;
		if (Sweep.Shape == EShape::Sphere)
		{
			PlaneZ = FloorZ + Sweep.Radius;
		}
		else if (Sweep.Shape == EShape::Capsule)
		{
			PlaneZ = FloorZ + Sweep.HalfHeight + Sweep.Radius;
		}
		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (!SegmentFloorZ(Start, End, PlaneZ, T, Normal))
		{
			return false;
		}
		WriteHit(OutHit, Start, End, T, Normal, NoComponentID, true);
		if (Sweep.Shape == EShape::Sphere)
		{
			OutHit.ImpactPoint = OutHit.Location - (Normal * Sweep.Radius);
		}
		else if (Sweep.Shape == EShape::Capsule)
		{
			OutHit.ImpactPoint = OutHit.Location - (Normal * (Sweep.HalfHeight + Sweep.Radius));
		}
		return true;
	}

	/** The shape against a slope plane. */
	[[nodiscard]] static bool SweepSlope(
		const FSweep& Sweep, const FSlopePlane& Plane, const FVector& Start, const FVector& End, FHitResult& OutHit)
	{
		return SweepSlopePlane(
			Plane, Start, End, Sweep.Radius, Sweep.Shape == EShape::Capsule ? Sweep.HalfHeight : 0.0f, OutHit);
	}

	/** Every hit, nearest first; at the same time the bodies in the order they were added, the floor, the slopes. */
	template <typename AllocatorType>
	static bool Multi(const FPhysScene& Scene, TArray<FHitResult, AllocatorType>& OutHits, const FVector& Start,
		const FVector& End, const FSweep& Sweep, const FFilter& Filter)
	{
		OutHits.Reset();
		Scene.UpdateBroadphase();
		FPhysScene::FBodyIndexArray Candidates;
		Scene.Broadphase.ForEachSegmentHit(FAabbTreeSegment(Start, End), Sweep.GetBroadphaseExpand(), 1.0f,
			[&](int32 BodyIndex, float MaxT)
			{
				if (Filter.GetResponse(Scene.Bodies[BodyIndex]) != ECR_Ignore)
				{
					Candidates.Add(BodyIndex);
				}
				return MaxT;
			});
		Scene.SortBodiesBySerial(Candidates);
		for (const int32 BodyIndex : Candidates)
		{
			float T = 1.0f;
			FVector Normal = FVector::ZeroVector;
			int32 FaceIndex = INDEX_NONE;
			if (!SweepBody(Scene, BodyIndex, Sweep, Start, End, 1.0f, T, Normal, FaceIndex))
			{
				continue;
			}
			MakeBodyHit(Scene, BodyIndex, Filter.GetResponse(Scene.Bodies[BodyIndex]) == ECR_Block, Sweep, Start, End,
				T, Normal, FaceIndex, Filter.Params, OutHits.AddDefaulted_GetRef());
		}
		if (Sweep.bPlanes)
		{
			FHitResult Hit;
			if (Filter.Params.bTraceFloorPlane && SweepFloor(Sweep, Start, End, Filter.Params.FloorZ, Hit))
			{
				OutHits.Add(Hit);
			}
			for (const FSlopePlane& Plane : Scene.SlopePlanes)
			{
				Hit = FHitResult();
				if (SweepSlope(Sweep, Plane, Start, End, Hit))
				{
					OutHits.Add(Hit);
				}
			}
		}
		SortHitsByTime(OutHits);
		return OutHits.Num() > 0;
	}

	/**
	 * The first blocking hit of Multi, without listing the others: the nearest, and at the same time the body added
	 * first, then the floor, then the slopes. The broadphase and the triangle trees skip what lies beyond the nearest
	 * hit so far.
	 */
	static bool Single(const FPhysScene& Scene, FHitResult& OutHit, const FVector& Start, const FVector& End,
		const FSweep& Sweep, const FFilter& Filter)
	{
		Scene.UpdateBroadphase();
		int32 BestBody = INDEX_NONE;
		float BestT = 1.0f;
		uint32 BestSerial = 0;
		FVector BestNormal = FVector::ZeroVector;
		int32 BestFace = INDEX_NONE;
		Scene.Broadphase.ForEachSegmentHit(FAabbTreeSegment(Start, End), Sweep.GetBroadphaseExpand(), 1.0f,
			[&](int32 BodyIndex, float MaxT)
			{
				if (Filter.GetResponse(Scene.Bodies[BodyIndex]) != ECR_Block)
				{
					return MaxT;
				}
				float T = 1.0f;
				FVector Normal = FVector::ZeroVector;
				int32 FaceIndex = INDEX_NONE;
				if (!SweepBody(Scene, BodyIndex, Sweep, Start, End, MaxT, T, Normal, FaceIndex))
				{
					return MaxT;
				}
				const uint32 Serial = Scene.GetBodySerial(BodyIndex);
				if (BestBody == INDEX_NONE || T < BestT || (T == BestT && Serial < BestSerial))
				{
					BestBody = BodyIndex;
					BestT = T;
					BestSerial = Serial;
					BestNormal = Normal;
					BestFace = FaceIndex;
				}
				return BestBody == INDEX_NONE ? MaxT : BestT;
			});

		// The planes come after the bodies: they win only when strictly nearer.
		bool bFound = BestBody != INDEX_NONE;
		float BestTime = BestT;
		bool bPlaneBest = false;
		FHitResult PlaneHit;
		if (Sweep.bPlanes)
		{
			FHitResult Hit;
			if (Filter.Params.bTraceFloorPlane && SweepFloor(Sweep, Start, End, Filter.Params.FloorZ, Hit) &&
				(!bFound || Hit.Time < BestTime))
			{
				PlaneHit = Hit;
				bPlaneBest = true;
				bFound = true;
				BestTime = Hit.Time;
			}
			for (const FSlopePlane& Plane : Scene.SlopePlanes)
			{
				Hit = FHitResult();
				if (SweepSlope(Sweep, Plane, Start, End, Hit) && (!bFound || Hit.Time < BestTime))
				{
					PlaneHit = Hit;
					bPlaneBest = true;
					bFound = true;
					BestTime = Hit.Time;
				}
			}
		}

		OutHit = FHitResult();
		if (bPlaneBest)
		{
			OutHit = PlaneHit;
			return true;
		}
		if (BestBody != INDEX_NONE)
		{
			MakeBodyHit(Scene, BestBody, true, Sweep, Start, End, BestT, BestNormal, BestFace, Filter.Params, OutHit);
			return true;
		}
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;
		OutHit.Time = 1.0f;
		return false;
	}
};

ECollisionResponse FPhysScene::GetBodyQueryResponse(const FBodyInstance& Body, ECollisionChannel TraceChannel,
	const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	if (!Body.bQueryEnabled || Params.IsIgnored(Body.ComponentID, Body.OwnerID))
	{
		return ECR_Ignore;
	}
	return FMath::Min(Body.CollisionResponses.GetResponse(TraceChannel),
		ResponseParam.CollisionResponse.GetResponse(Body.ObjectType.GetValue()));
}

void FPhysScene::SetHitBody(FHitResult& Hit, int32 BodyIndex) const
{
	Hit.BodyIndex = BodyIndex;
	UPrimitiveComponent* Owner = GetBodyOwner(BodyIndex);
	Hit.Component = Owner;
	Hit.Actor = Owner != nullptr ? Owner->GetOwner() : nullptr;
}

UPhysicalMaterial* FPhysScene::GetHitPhysicalMaterial(const UPrimitiveComponent* Component, int32 FaceIndex)
{
	if (Component == nullptr)
	{
		return nullptr;
	}
	// A triangle: its section's material (UE: GetMaterialFromCollisionFaceIndex); a simple shape: the first material
	// (UE: FBodyInstance::GetSimplePhysicalMaterial, after the body setup's own, which Leon has not).
	const UMaterialInterface* Material = nullptr;
	if (FaceIndex != INDEX_NONE)
	{
		int32 SectionIndex = INDEX_NONE;
		Material = Component->GetMaterialFromCollisionFaceIndex(FaceIndex, SectionIndex);
	}
	else if (const UMeshComponent* Mesh = Cast<UMeshComponent>(Component))
	{
		Material = Mesh->GetMaterial(0);
	}
	return Material != nullptr ? Material->GetPhysicalMaterial() : nullptr;
}

bool FPhysScene::LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	SCOPE_CYCLE_COUNTER(STAT_LineTrace);
	const bool bHit = FPhysSceneQuery::Multi(
		*this, OutHits, Start, End, FPhysSceneQuery::FSweep::MakeLine(true), {Channel, Params, ResponseParam, nullptr});
	if (ShouldDraw(Params, DebugDraw))
	{
		DrawDebugLineTrace(*DebugDraw, Start, End, OutHits);
	}
	return bHit;
}

bool FPhysScene::LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End,
	ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	if (ShouldDraw(Params, DebugDraw))
	{
		// The debug view draws every hit.
		FMemMark Mark(FMemStack::Get());
		TArray<FHitResult, TMemStackAllocator<>> Hits;
		(void)FPhysSceneQuery::Multi(*this, Hits, Start, End, FPhysSceneQuery::FSweep::MakeLine(true),
			{Channel, Params, ResponseParam, nullptr});
		DrawDebugLineTrace(*DebugDraw, Start, End, Hits);
		return TakeNearestHit(Hits, OutHit, Start, End);
	}
	return FPhysSceneQuery::Single(
		*this, OutHit, Start, End, FPhysSceneQuery::FSweep::MakeLine(true), {Channel, Params, ResponseParam, nullptr});
}

bool FPhysScene::SphereTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	float Radius, ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	SCOPE_CYCLE_COUNTER(STAT_Sweep);
	const FPhysSceneQuery::FSweep Sweep = FPhysSceneQuery::FSweep::MakeSphere(Radius);
	const bool bHit =
		FPhysSceneQuery::Multi(*this, OutHits, Start, End, Sweep, {Channel, Params, ResponseParam, nullptr});
	if (ShouldDraw(Params, DebugDraw))
	{
		DrawDebugSphereTrace(*DebugDraw, Start, End, Sweep.Radius, OutHits);
	}
	return bHit;
}

bool FPhysScene::SphereTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, float Radius,
	ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	if (ShouldDraw(Params, DebugDraw))
	{
		FMemMark Mark(FMemStack::Get());
		TArray<FHitResult, TMemStackAllocator<>> Hits;
		const FPhysSceneQuery::FSweep Sweep = FPhysSceneQuery::FSweep::MakeSphere(Radius);
		(void)FPhysSceneQuery::Multi(*this, Hits, Start, End, Sweep, {Channel, Params, ResponseParam, nullptr});
		DrawDebugSphereTrace(*DebugDraw, Start, End, Sweep.Radius, Hits);
		return TakeNearestHit(Hits, OutHit, Start, End);
	}
	return FPhysSceneQuery::Single(*this, OutHit, Start, End, FPhysSceneQuery::FSweep::MakeSphere(Radius),
		{Channel, Params, ResponseParam, nullptr});
}

bool FPhysScene::CapsuleTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	float Radius, float HalfHeight, ECollisionChannel Channel, const FCollisionQueryParams& Params,
	FDebugDraw* DebugDraw, const FCollisionResponseParams& ResponseParam) const
{
	SCOPE_CYCLE_COUNTER(STAT_Sweep);
	const FPhysSceneQuery::FSweep Sweep = FPhysSceneQuery::FSweep::MakeCapsule(Radius, HalfHeight);
	const bool bHit =
		FPhysSceneQuery::Multi(*this, OutHits, Start, End, Sweep, {Channel, Params, ResponseParam, nullptr});
	if (ShouldDraw(Params, DebugDraw))
	{
		DrawDebugCapsuleTrace(*DebugDraw, Start, End, Sweep.Radius, Sweep.HalfHeight, OutHits);
	}
	return bHit;
}

bool FPhysScene::CapsuleTraceMultiByChannel(TArray<FHitResult, TMemStackAllocator<>>& OutHits, const FVector& Start,
	const FVector& End, float Radius, float HalfHeight, ECollisionChannel Channel, const FCollisionQueryParams& Params,
	FDebugDraw* DebugDraw, const FCollisionResponseParams& ResponseParam) const
{
	SCOPE_CYCLE_COUNTER(STAT_Sweep);
	const FPhysSceneQuery::FSweep Sweep = FPhysSceneQuery::FSweep::MakeCapsule(Radius, HalfHeight);
	const bool bHit =
		FPhysSceneQuery::Multi(*this, OutHits, Start, End, Sweep, {Channel, Params, ResponseParam, nullptr});
	if (ShouldDraw(Params, DebugDraw))
	{
		DrawDebugCapsuleTrace(*DebugDraw, Start, End, Sweep.Radius, Sweep.HalfHeight, OutHits);
	}
	return bHit;
}

bool FPhysScene::CapsuleTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, float Radius,
	float HalfHeight, ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	if (ShouldDraw(Params, DebugDraw))
	{
		FMemMark Mark(FMemStack::Get());
		TArray<FHitResult, TMemStackAllocator<>> Hits;
		(void)CapsuleTraceMultiByChannel(
			Hits, Start, End, Radius, HalfHeight, Channel, Params, DebugDraw, ResponseParam);
		return TakeNearestHit(Hits, OutHit, Start, End);
	}
	return FPhysSceneQuery::Single(*this, OutHit, Start, End, FPhysSceneQuery::FSweep::MakeCapsule(Radius, HalfHeight),
		{Channel, Params, ResponseParam, nullptr});
}

bool FPhysScene::SweepMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	const FQuat& /*Rot*/, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape,
	const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam) const
{
	switch (CollisionShape.ShapeType)
	{
		case ECollisionShape::Sphere:
			return SphereTraceMultiByChannel(
				OutHits, Start, End, CollisionShape.GetSphereRadius(), TraceChannel, Params, nullptr, ResponseParam);
		case ECollisionShape::Capsule:
			// FCollisionShape's half height includes the caps; the capsule trace takes the cylinder's.
			return CapsuleTraceMultiByChannel(OutHits, Start, End, CollisionShape.GetCapsuleRadius(),
				FMath::Max(0.0f, CollisionShape.GetCapsuleHalfHeight() - CollisionShape.GetCapsuleRadius()),
				TraceChannel, Params, nullptr, ResponseParam);
		case ECollisionShape::Box:
		{
			SCOPE_CYCLE_COUNTER(STAT_Sweep);
			// An axis-aligned box: the segment against every body grown by the box (a sphere of the box's largest
			// extent against triangles and capsules); no planes.
			return FPhysSceneQuery::Multi(*this, OutHits, Start, End,
				FPhysSceneQuery::FSweep::MakeBox(CollisionShape.GetBox()),
				{TraceChannel, Params, ResponseParam, nullptr});
		}
		case ECollisionShape::Line:
		default:
			return LineTraceMultiByChannel(OutHits, Start, End, TraceChannel, Params, nullptr, ResponseParam);
	}
}

bool FPhysScene::SweepSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End,
	const FQuat& /*Rot*/, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape,
	const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam) const
{
	switch (CollisionShape.ShapeType)
	{
		case ECollisionShape::Sphere:
			return SphereTraceSingleByChannel(
				OutHit, Start, End, CollisionShape.GetSphereRadius(), TraceChannel, Params, nullptr, ResponseParam);
		case ECollisionShape::Capsule:
			return CapsuleTraceSingleByChannel(OutHit, Start, End, CollisionShape.GetCapsuleRadius(),
				FMath::Max(0.0f, CollisionShape.GetCapsuleHalfHeight() - CollisionShape.GetCapsuleRadius()),
				TraceChannel, Params, nullptr, ResponseParam);
		case ECollisionShape::Box:
			return FPhysSceneQuery::Single(*this, OutHit, Start, End,
				FPhysSceneQuery::FSweep::MakeBox(CollisionShape.GetBox()),
				{TraceChannel, Params, ResponseParam, nullptr});
		case ECollisionShape::Line:
		default:
			return LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, Params, nullptr, ResponseParam);
	}
}

bool FPhysScene::LineTraceMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params) const
{
	SCOPE_CYCLE_COUNTER(STAT_LineTrace);
	return FPhysSceneQuery::Multi(*this, OutHits, Start, End, FPhysSceneQuery::FSweep::MakeLine(false),
		{ECC_WorldStatic, Params, FCollisionResponseParams::DefaultResponseParam, &ObjectQueryParams});
}

bool FPhysScene::LineTraceSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End,
	const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params) const
{
	return FPhysSceneQuery::Single(*this, OutHit, Start, End, FPhysSceneQuery::FSweep::MakeLine(false),
		{ECC_WorldStatic, Params, FCollisionResponseParams::DefaultResponseParam, &ObjectQueryParams});
}

FBox FPhysScene::GetBodyBounds(int32 BodyIndex) const
{
	if (!Bodies.IsValidIndex(BodyIndex))
	{
		return FBox(FVector::ZeroVector, FVector::ZeroVector);
	}
	const FBodyInstance& Body = Bodies[BodyIndex];
	return FBox(Body.Position - Body.HalfExtents, Body.Position + Body.HalfExtents);
}

bool FPhysScene::OverlapMultiByObjectType(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& /*Rot*/,
	const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape,
	const FCollisionQueryParams& Params) const
{
	SCOPE_CYCLE_COUNTER(STAT_Overlap);
	OutOverlaps.Reset();
	UpdateBroadphase();
	const bool bSphere = CollisionShape.IsSphere();
	const float SphereRadius = bSphere ? CollisionShape.GetSphereRadius() : 0.0f;
	const FVector QueryExtent = CollisionShape.IsBox() ? CollisionShape.GetBox()
		: CollisionShape.IsCapsule() ? FVector(CollisionShape.GetCapsuleRadius(), CollisionShape.GetCapsuleRadius(),
										   CollisionShape.GetCapsuleHalfHeight())
									 : FVector(SphereRadius);
	// The bodies whose boxes the shape's box reaches, in the order they were added.
	const FVector Reach = QueryExtent.GetAbs();
	FBodyIndexArray Candidates;
	Broadphase.ForEachOverlap(FBox(Pos - Reach, Pos + Reach),
		[&](int32 BodyIndex)
		{
			const FBodyInstance& Body = Bodies[BodyIndex];
			if (Body.bQueryEnabled && !Params.IsIgnored(Body.ComponentID, Body.OwnerID) &&
				ObjectQueryParams.Contains(Body.ObjectType.GetValue()))
			{
				Candidates.Add(BodyIndex);
			}
		});
	SortBodiesBySerial(Candidates);
	for (const int32 Bi : Candidates)
	{
		const FBodyInstance& Body = Bodies[Bi];
		bool bOverlaps = false;
		if (bSphere && Body.CollisionShape == EBodyCollisionShape::Capsule)
		{
			// The distance from the centre to the capsule's segment against the two radii.
			float CapsuleRadius = 0.0f;
			float CapsuleCylinder = 0.0f;
			GetBodyCapsule(Body, CapsuleRadius, CapsuleCylinder);
			const float SegmentZ =
				FMath::Clamp(Pos.Z, Body.Position.Z - CapsuleCylinder, Body.Position.Z + CapsuleCylinder);
			const FVector Closest(Body.Position.X, Body.Position.Y, SegmentZ);
			bOverlaps = FVector::DistSquared(Pos, Closest) <= FMath::Square(SphereRadius + CapsuleRadius);
		}
		else if (bSphere)
		{
			// The distance from the centre to the box.
			const FVector Mn = Body.Position - Body.HalfExtents;
			const FVector Mx = Body.Position + Body.HalfExtents;
			const FVector Closest(
				FMath::Clamp(Pos.X, Mn.X, Mx.X), FMath::Clamp(Pos.Y, Mn.Y, Mx.Y), FMath::Clamp(Pos.Z, Mn.Z, Mx.Z));
			bOverlaps = FVector::DistSquared(Pos, Closest) <= FMath::Square(SphereRadius);
		}
		else
		{
			const FVector Delta = (Pos - Body.Position).GetAbs();
			const FVector Reach2 = QueryExtent + Body.HalfExtents;
			bOverlaps = Delta.X <= Reach2.X && Delta.Y <= Reach2.Y && Delta.Z <= Reach2.Z;
		}
		if (!bOverlaps)
		{
			continue;
		}
		FOverlapResult& Overlap = OutOverlaps.AddDefaulted_GetRef();
		Overlap.ItemIndex = Bi;
		Overlap.bBlockingHit = true;
		UPrimitiveComponent* Owner = GetBodyOwner(Bi);
		Overlap.Component = Owner;
		Overlap.Actor = Owner != nullptr ? Owner->GetOwner() : nullptr;
	}
	return OutOverlaps.Num() > 0;
}
