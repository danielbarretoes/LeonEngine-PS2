#include "Components/PrimitiveComponent.h"
#include "Debug/DebugDraw.h"
#include "GameFramework/Actor.h"
#include "Physics/PhysScene.h"
#include "TriangleCollision.h"

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

	[[nodiscard]] bool SegmentAabb(
		const FVector& Start, const FVector& End, const FVector& Mn, const FVector& Mx, float& OutT, FVector& OutNormal)
	{
		const FVector Dir = End - Start;
		float TEnter = 0.0f;
		float TExit = 1.0f;
		FVector EnterNormal(0.0f, 0.0f, 1.0f);
		bool bHitFace = false;

		// X, then the vertical Z, then Y: on equal entry times the earlier axis gives the normal (the order of the
		// Y-up world: X, vertical, second horizontal).
		constexpr int32 AxisOrder[3] = {0, 2, 1};
		for (const int32 Axis : AxisOrder)
		{
			if (FMath::Abs(Dir[Axis]) < 1.0e-6f)
			{
				if (Start[Axis] < Mn[Axis] || Start[Axis] > Mx[Axis])
				{
					return false;
				}
				continue;
			}

			const float Inv = 1.0f / Dir[Axis];
			float T0 = (Mn[Axis] - Start[Axis]) * Inv;
			float T1 = (Mx[Axis] - Start[Axis]) * Inv;
			float NormalSign = -1.0f;
			if (Inv < 0.0f)
			{
				Swap(T0, T1);
				NormalSign = 1.0f;
			}

			if (T0 > TEnter)
			{
				TEnter = T0;
				EnterNormal = FVector::ZeroVector;
				EnterNormal[Axis] = NormalSign;
				bHitFace = true;
			}
			TExit = FMath::Min(TExit, T1);
			if (TEnter > TExit)
			{
				return false;
			}
		}

		if (TEnter < 0.0f || TEnter > 1.0f)
		{
			return false;
		}

		if (!bHitFace && TEnter <= 0.0f)
		{
			OutT = 0.0f;
			OutNormal = FVector(0.0f, 0.0f, 1.0f);
			return true;
		}

		OutT = TEnter;
		OutNormal = EnterNormal;
		const float Len = OutNormal.Size();
		if (Len > 1.0e-6f)
		{
			OutNormal /= Len;
		}
		return true;
	}

	/**
	 * Segment vs an upright capsule around Center: a cylinder of Radius from Center.Z - CylinderHalfHeight to
	 * Center.Z + CylinderHalfHeight, closed by two hemispheres. A swept sphere or upright capsule against it is the
	 * segment of its centre against the capsule grown by the swept radius and cylinder (the Minkowski sum of two
	 * upright capsules is one). The normal points from the capsule's axis to the entry point; a segment that starts
	 * inside hits at 0 with the normal from the axis toward the start.
	 */
	[[nodiscard]] bool SegmentUprightCapsule(const FVector& Start, const FVector& End, const FVector& Center,
		float Radius, float CylinderHalfHeight, float& OutT, FVector& OutNormal)
	{
		const float R = FMath::Max(Radius, 0.0f);
		const float Hc = FMath::Max(CylinderHalfHeight, 0.0f);
		const FVector Bottom = Center - FVector(0.0f, 0.0f, Hc);
		const FVector Top = Center + FVector(0.0f, 0.0f, Hc);

		auto ClosestOnAxis = [&](const FVector& P)
		{ return FVector(Center.X, Center.Y, FMath::Clamp(P.Z, Bottom.Z, Top.Z)); };
		auto NormalFrom = [&](const FVector& P)
		{
			const FVector Away = P - ClosestOnAxis(P);
			const float Len = Away.Size();
			return Len > 1.0e-6f ? Away / Len : FVector(0.0f, 0.0f, 1.0f);
		};

		// Starting inside: an immediate hit, as the boxes do.
		if ((Start - ClosestOnAxis(Start)).SizeSquared() <= R * R)
		{
			OutT = 0.0f;
			OutNormal = NormalFrom(Start);
			return true;
		}

		const FVector D = End - Start;
		float BestT = 2.0f;

		// The side of the infinite cylinder, kept where it is between the caps.
		const float A = (D.X * D.X) + (D.Y * D.Y);
		if (A > 1.0e-8f)
		{
			const float Sx = Start.X - Center.X;
			const float Sy = Start.Y - Center.Y;
			const float B = 2.0f * ((Sx * D.X) + (Sy * D.Y));
			const float C = (Sx * Sx) + (Sy * Sy) - (R * R);
			const float Disc = (B * B) - (4.0f * A * C);
			if (Disc >= 0.0f)
			{
				const float T = (-B - FMath::Sqrt(Disc)) / (2.0f * A);
				const float Z = Start.Z + (D.Z * T);
				if (T >= 0.0f && T <= 1.0f && Z >= Bottom.Z && Z <= Top.Z)
				{
					BestT = T;
				}
			}
		}

		// The two hemispheres (whole spheres: their parts inside the cylinder are entered through its side first).
		for (const FVector& SphereCenter : {Bottom, Top})
		{
			const FVector S = Start - SphereCenter;
			const float Qa = D.SizeSquared();
			if (Qa < 1.0e-8f)
			{
				continue;
			}
			const float Qb = 2.0f * (S | D);
			const float Qc = S.SizeSquared() - (R * R);
			const float Disc = (Qb * Qb) - (4.0f * Qa * Qc);
			if (Disc < 0.0f)
			{
				continue;
			}
			const float T = (-Qb - FMath::Sqrt(Disc)) / (2.0f * Qa);
			if (T >= 0.0f && T <= 1.0f && T < BestT)
			{
				BestT = T;
			}
		}

		if (BestT > 1.0f)
		{
			return false;
		}
		OutT = BestT;
		OutNormal = NormalFrom(Start + (D * BestT));
		return true;
	}

	/** Segment vs the horizontal plane z = FloorZ; the normal faces the segment's start. */
	[[nodiscard]] bool SegmentFloorZ(
		const FVector& Start, const FVector& End, float FloorZ, float& OutT, FVector& OutNormal)
	{
		const float Dz = End.Z - Start.Z;
		if (FMath::Abs(Dz) < 1.0e-6f)
		{
			return false;
		}
		const float T = (FloorZ - Start.Z) / Dz;
		if (T < 0.0f || T > 1.0f)
		{
			return false;
		}
		OutT = T;
		OutNormal = FVector(0.0f, 0.0f, Dz < 0.0f ? 1.0f : -1.0f);
		return true;
	}

	[[nodiscard]] bool PointInAabb(const FVector& P, const FVector& Center, const FVector& HalfExtents, float Inflate)
	{
		const FVector He = HalfExtents + FVector(Inflate);
		return FMath::Abs(P.X - Center.X) <= He.X && FMath::Abs(P.Y - Center.Y) <= He.Y &&
			FMath::Abs(P.Z - Center.Z) <= He.Z;
	}

	/**
	 * Segment vs FSlopePlane. Inflate expands the plane along its normal (sphere / capsule radius).
	 * Hits when the offset-plane distance changes sign (approach from either side).
	 */
	[[nodiscard]] bool SegmentSlopePlane(const FVector& Start, const FVector& End, const FSlopePlane& Plane,
		float Inflate, float& OutT, FVector& OutNormal)
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

	void AppendSlopePlaneHits(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, float Radius,
		float HalfHeight, const TArray<FSlopePlane>& Planes)
	{
		for (const FSlopePlane& Plane : Planes)
		{
			const float NLen = Plane.Normal.Size();
			const float Nz = (NLen > 1.0e-6f) ? (FMath::Abs(Plane.Normal.Z) / NLen) : 1.0f;
			const float Inflate = Radius + (HalfHeight * Nz);
			float T = 1.0f;
			FVector Normal = FVector::ZeroVector;
			if (!SegmentSlopePlane(Start, End, Plane, Inflate, T, Normal))
			{
				continue;
			}
			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, NoComponentID, false);
			Hit.ImpactPoint = Hit.Location - (Normal * Inflate);
			OutHits.Add(Hit);
		}
	}

	/** Nearest first. Stable, so equal times keep the order they were found in. */
	void SortHitsByTime(TArray<FHitResult>& Hits)
	{
		StableSort(
			Hits.GetData(), Hits.Num(), [](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });
	}

	/**
	 * Copies the nearest blocking Multi hit into OutHit (a UE Single trace returns the first blocking hit; the overlaps
	 * before it are not reported).
	 */
	[[nodiscard]] bool TakeNearestHit(
		const TArray<FHitResult>& Hits, FHitResult& OutHit, const FVector& Start, const FVector& End)
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

	void DrawTracePath(FDebugDraw& Draw, const FVector& Start, const FVector& End, const TArray<FHitResult>& Hits)
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

void DrawDebugLineTrace(FDebugDraw& Draw, const FVector& Start, const FVector& End, const TArray<FHitResult>& Hits)
{
	DrawTracePath(Draw, Start, End, Hits);
}

void DrawDebugSphereTrace(
	FDebugDraw& Draw, const FVector& Start, const FVector& End, float Radius, const TArray<FHitResult>& Hits)
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
	const TArray<FHitResult>& Hits)
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

void FPhysScene::FilterBackendHits(TArray<FHitResult>& Hits, ECollisionChannel TraceChannel,
	const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam) const
{
	for (int32 HitIndex = Hits.Num() - 1; HitIndex >= 0; --HitIndex)
	{
		FHitResult& Hit = Hits[HitIndex];
		if (!Bodies.IsValidIndex(Hit.BodyIndex))
		{
			continue;
		}
		const ECollisionResponse Response =
			GetBodyQueryResponse(Bodies[Hit.BodyIndex], TraceChannel, Params, ResponseParam);
		if (Response == ECR_Ignore)
		{
			Hits.RemoveAt(HitIndex);
			continue;
		}
		Hit.bBlockingHit = Response == ECR_Block;
		SetHitBody(Hit, Hit.BodyIndex);
	}
}

bool FPhysScene::LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	OutHits.Reset();

	// A rigid-body backend traces the bodies it simulates; the Arcade shapes cover the others (query-only bodies).
	const bool bNarrowPhase = BackendIface != nullptr && BackendIface->HasNarrowPhaseTraces();
	if (bNarrowPhase)
	{
		// Body instances may have been nudged CMC without a Step: sync before CastRay.
		if (BackendIface->HasRigidWorld())
		{
			BackendIface->RigidPrepareStep(Bodies, Params.IgnoreComponentID);
		}
		(void)BackendIface->RigidLineTrace(OutHits, Start, End, Channel, Params.IgnoreComponentID);
		FilterBackendHits(OutHits, Channel, Params, ResponseParam);
	}
	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (bNarrowPhase && Body.bPhysicsEnabled)
		{
			continue;
		}
		const ECollisionResponse Response = GetBodyQueryResponse(Body, Channel, Params, ResponseParam);
		if (Response == ECR_Ignore)
		{
			continue;
		}

		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (Body.CollisionShape == EBodyCollisionShape::Capsule)
		{
			float CapsuleRadius = 0.0f;
			float CapsuleCylinder = 0.0f;
			GetBodyCapsule(Body, CapsuleRadius, CapsuleCylinder);
			if (!SegmentUprightCapsule(Start, End, Body.Position, CapsuleRadius, CapsuleCylinder, T, Normal))
			{
				continue;
			}
		}
		else
		{
			const FVector Mn = Body.Position - Body.HalfExtents;
			const FVector Mx = Body.Position + Body.HalfExtents;
			float TAabb = 1.0f;
			FVector NAabb = FVector::ZeroVector;
			if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb))
			{
				continue;
			}

			T = TAabb;
			Normal = NAabb;
			if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
				TriangleMeshes[Bi].IsValid())
			{
				float TMesh = 1.0f;
				FVector NMesh = FVector::ZeroVector;
				if (!SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], 0.0f, TMesh, NMesh))
				{
					continue; // Broadphase only: no actual triangle hit.
				}
				T = TMesh;
				Normal = NMesh;
			}
		}

		FHitResult Hit;
		WriteHit(Hit, Start, End, T, Normal, Body.ComponentID, false);
		Hit.bBlockingHit = Response == ECR_Block;
		SetHitBody(Hit, Bi);
		OutHits.Add(Hit);
	}

	if (Params.bTraceFloorPlane)
	{
		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (SegmentFloorZ(Start, End, Params.FloorZ, T, Normal))
		{
			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, NoComponentID, true);
			OutHits.Add(Hit);
		}
	}
	AppendSlopePlaneHits(OutHits, Start, End, 0.0f, 0.0f, SlopePlanes);

	SortHitsByTime(OutHits);
	if (ShouldDraw(Params, DebugDraw))
	{
		DrawDebugLineTrace(*DebugDraw, Start, End, OutHits);
	}
	return OutHits.Num() > 0;
}

bool FPhysScene::LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End,
	ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	TArray<FHitResult> Hits;
	(void)LineTraceMultiByChannel(Hits, Start, End, Channel, Params, DebugDraw, ResponseParam);
	return TakeNearestHit(Hits, OutHit, Start, End);
}

bool FPhysScene::SphereTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	float Radius, ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	const float R = FMath::Max(Radius, 0.0f);
	OutHits.Reset();

	const bool bNarrowPhase = BackendIface != nullptr && BackendIface->HasNarrowPhaseTraces();
	if (bNarrowPhase)
	{
		// Push the body instances to Jolt before CastShape (same as the Step prepare).
		if (BackendIface->HasRigidWorld())
		{
			BackendIface->RigidPrepareStep(Bodies, Params.IgnoreComponentID);
		}
		(void)BackendIface->RigidSphereTrace(OutHits, Start, End, R, Channel, Params.IgnoreComponentID);
		FilterBackendHits(OutHits, Channel, Params, ResponseParam);
	}
	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (bNarrowPhase && Body.bPhysicsEnabled)
		{
			continue;
		}
		const ECollisionResponse Response = GetBodyQueryResponse(Body, Channel, Params, ResponseParam);
		if (Response == ECR_Ignore)
		{
			continue;
		}

		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (Body.CollisionShape == EBodyCollisionShape::Capsule)
		{
			float CapsuleRadius = 0.0f;
			float CapsuleCylinder = 0.0f;
			GetBodyCapsule(Body, CapsuleRadius, CapsuleCylinder);
			if (!SegmentUprightCapsule(Start, End, Body.Position, CapsuleRadius + R, CapsuleCylinder, T, Normal))
			{
				continue;
			}
		}
		else
		{
			const FVector Expand(R, R, R);
			const FVector Mn = Body.Position - Body.HalfExtents - Expand;
			const FVector Mx = Body.Position + Body.HalfExtents + Expand;
			float TAabb = 1.0f;
			FVector NAabb = FVector::ZeroVector;
			if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb))
			{
				continue;
			}

			T = TAabb;
			Normal = NAabb;
			if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
				TriangleMeshes[Bi].IsValid())
			{
				float TMesh = 1.0f;
				FVector NMesh = FVector::ZeroVector;
				if (!SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], R, TMesh, NMesh))
				{
					continue;
				}
				T = TMesh;
				Normal = NMesh;
			}
		}

		FHitResult Hit;
		WriteHit(Hit, Start, End, T, Normal, Body.ComponentID, false);
		// UE FHitResult: Location = sweep shape center; ImpactPoint = surface contact.
		Hit.ImpactPoint = Hit.Location - (Normal * R);
		Hit.bBlockingHit = Response == ECR_Block;
		SetHitBody(Hit, Bi);
		OutHits.Add(Hit);
	}

	if (Params.bTraceFloorPlane)
	{
		const float PlaneZ = Params.FloorZ + R;
		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (SegmentFloorZ(Start, End, PlaneZ, T, Normal))
		{
			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, NoComponentID, true);
			Hit.ImpactPoint = Hit.Location - (Normal * R);
			OutHits.Add(Hit);
		}
	}
	AppendSlopePlaneHits(OutHits, Start, End, R, 0.0f, SlopePlanes);

	SortHitsByTime(OutHits);
	if (ShouldDraw(Params, DebugDraw))
	{
		DrawDebugSphereTrace(*DebugDraw, Start, End, R, OutHits);
	}
	return OutHits.Num() > 0;
}

bool FPhysScene::SphereTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, float Radius,
	ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	TArray<FHitResult> Hits;
	(void)SphereTraceMultiByChannel(Hits, Start, End, Radius, Channel, Params, DebugDraw, ResponseParam);
	return TakeNearestHit(Hits, OutHit, Start, End);
}

bool FPhysScene::CapsuleTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	float Radius, float HalfHeight, ECollisionChannel Channel, const FCollisionQueryParams& Params,
	FDebugDraw* DebugDraw, const FCollisionResponseParams& ResponseParam) const
{
	const float R = FMath::Max(Radius, 0.0f);
	const float Hh = FMath::Max(HalfHeight, 0.0f);
	OutHits.Reset();
	const FVector Expand(R, R, Hh + R);

	const bool bNarrowPhase = BackendIface != nullptr && BackendIface->HasNarrowPhaseTraces();
	if (bNarrowPhase)
	{
		if (BackendIface->HasRigidWorld())
		{
			BackendIface->RigidPrepareStep(Bodies, Params.IgnoreComponentID);
		}
		(void)BackendIface->RigidCapsuleTrace(OutHits, Start, End, R, Hh, Channel, Params.IgnoreComponentID);
		FilterBackendHits(OutHits, Channel, Params, ResponseParam);
	}
	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (bNarrowPhase && Body.bPhysicsEnabled)
		{
			continue;
		}
		const ECollisionResponse Response = GetBodyQueryResponse(Body, Channel, Params, ResponseParam);
		if (Response == ECR_Ignore)
		{
			continue;
		}

		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (Body.CollisionShape == EBodyCollisionShape::Capsule)
		{
			float CapsuleRadius = 0.0f;
			float CapsuleCylinder = 0.0f;
			GetBodyCapsule(Body, CapsuleRadius, CapsuleCylinder);
			if (!SegmentUprightCapsule(Start, End, Body.Position, CapsuleRadius + R, CapsuleCylinder + Hh, T, Normal))
			{
				continue;
			}
		}
		else
		{
			const FVector Mn = Body.Position - Body.HalfExtents - Expand;
			const FVector Mx = Body.Position + Body.HalfExtents + Expand;
			float TAabb = 1.0f;
			FVector NAabb = FVector::ZeroVector;
			if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb))
			{
				continue;
			}

			T = TAabb;
			Normal = NAabb;
			if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
				TriangleMeshes[Bi].IsValid())
			{
				// Inflate like the slope planes: radius + |Nz| * half height, approximated with radius + half height.
				float TMesh = 1.0f;
				FVector NMesh = FVector::ZeroVector;
				if (!SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], R + Hh, TMesh, NMesh))
				{
					continue;
				}
				T = TMesh;
				Normal = NMesh;
			}
		}

		FHitResult Hit;
		WriteHit(Hit, Start, End, T, Normal, Body.ComponentID, false);
		const float Pull = (FMath::Abs(Normal.Z) > 0.5f) ? (Hh + R) : R;
		Hit.ImpactPoint = Hit.Location - (Normal * Pull);
		Hit.bBlockingHit = Response == ECR_Block;
		SetHitBody(Hit, Bi);
		OutHits.Add(Hit);
	}

	if (Params.bTraceFloorPlane)
	{
		const float PlaneZ = Params.FloorZ + Hh + R;
		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (SegmentFloorZ(Start, End, PlaneZ, T, Normal))
		{
			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, NoComponentID, true);
			Hit.ImpactPoint = Hit.Location - (Normal * (Hh + R));
			OutHits.Add(Hit);
		}
	}
	AppendSlopePlaneHits(OutHits, Start, End, R, Hh, SlopePlanes);

	SortHitsByTime(OutHits);
	if (ShouldDraw(Params, DebugDraw))
	{
		DrawDebugCapsuleTrace(*DebugDraw, Start, End, R, Hh, OutHits);
	}
	return OutHits.Num() > 0;
}

bool FPhysScene::CapsuleTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, float Radius,
	float HalfHeight, ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw,
	const FCollisionResponseParams& ResponseParam) const
{
	TArray<FHitResult> Hits;
	(void)CapsuleTraceMultiByChannel(Hits, Start, End, Radius, HalfHeight, Channel, Params, DebugDraw, ResponseParam);
	return TakeNearestHit(Hits, OutHit, Start, End);
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
			// An axis-aligned box: the segment against every body grown by the box (a sphere of the box's largest
			// extent against triangles and capsules).
			const FVector Extent = CollisionShape.GetBox();
			const float Bound = FMath::Max(Extent.X, FMath::Max(Extent.Y, Extent.Z));
			OutHits.Reset();
			for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
			{
				const FBodyInstance& Body = Bodies[Bi];
				const ECollisionResponse Response = GetBodyQueryResponse(Body, TraceChannel, Params, ResponseParam);
				if (Response == ECR_Ignore)
				{
					continue;
				}
				float T = 1.0f;
				FVector Normal = FVector::ZeroVector;
				if (Body.CollisionShape == EBodyCollisionShape::Capsule)
				{
					float CapsuleRadius = 0.0f;
					float CapsuleCylinder = 0.0f;
					GetBodyCapsule(Body, CapsuleRadius, CapsuleCylinder);
					if (!SegmentUprightCapsule(
							Start, End, Body.Position, CapsuleRadius + Bound, CapsuleCylinder, T, Normal))
					{
						continue;
					}
				}
				else if (!SegmentAabb(Start, End, Body.Position - Body.HalfExtents - Extent,
							 Body.Position + Body.HalfExtents + Extent, T, Normal))
				{
					continue;
				}
				else if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
					TriangleMeshes[Bi].IsValid() &&
					!SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], Bound, T, Normal))
				{
					continue;
				}
				FHitResult Hit;
				WriteHit(Hit, Start, End, T, Normal, Body.ComponentID, false);
				Hit.ImpactPoint = Hit.Location - (Normal * FMath::Abs(Normal | Extent));
				Hit.bBlockingHit = Response == ECR_Block;
				SetHitBody(Hit, Bi);
				OutHits.Add(Hit);
			}
			SortHitsByTime(OutHits);
			return OutHits.Num() > 0;
		}
		case ECollisionShape::Line:
		default:
			return LineTraceMultiByChannel(OutHits, Start, End, TraceChannel, Params, nullptr, ResponseParam);
	}
}

bool FPhysScene::SweepSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot,
	ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params,
	const FCollisionResponseParams& ResponseParam) const
{
	TArray<FHitResult> Hits;
	(void)SweepMultiByChannel(Hits, Start, End, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
	return TakeNearestHit(Hits, OutHit, Start, End);
}

bool FPhysScene::LineTraceMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params) const
{
	OutHits.Reset();
	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (!Body.bQueryEnabled || Params.IsIgnored(Body.ComponentID, Body.OwnerID) ||
			!ObjectQueryParams.Contains(Body.ObjectType.GetValue()))
		{
			continue;
		}
		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (Body.CollisionShape == EBodyCollisionShape::Capsule)
		{
			float CapsuleRadius = 0.0f;
			float CapsuleCylinder = 0.0f;
			GetBodyCapsule(Body, CapsuleRadius, CapsuleCylinder);
			if (!SegmentUprightCapsule(Start, End, Body.Position, CapsuleRadius, CapsuleCylinder, T, Normal))
			{
				continue;
			}
		}
		else if (!SegmentAabb(
					 Start, End, Body.Position - Body.HalfExtents, Body.Position + Body.HalfExtents, T, Normal))
		{
			continue;
		}
		else if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
			TriangleMeshes[Bi].IsValid() && !SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], 0.0f, T, Normal))
		{
			continue;
		}
		FHitResult Hit;
		WriteHit(Hit, Start, End, T, Normal, Body.ComponentID, false);
		SetHitBody(Hit, Bi);
		OutHits.Add(Hit);
	}
	SortHitsByTime(OutHits);
	return OutHits.Num() > 0;
}

bool FPhysScene::LineTraceSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End,
	const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params) const
{
	TArray<FHitResult> Hits;
	(void)LineTraceMultiByObjectType(Hits, Start, End, ObjectQueryParams, Params);
	return TakeNearestHit(Hits, OutHit, Start, End);
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
	OutOverlaps.Reset();
	const bool bSphere = CollisionShape.IsSphere();
	const float SphereRadius = bSphere ? CollisionShape.GetSphereRadius() : 0.0f;
	const FVector QueryExtent = CollisionShape.IsBox() ? CollisionShape.GetBox()
		: CollisionShape.IsCapsule() ? FVector(CollisionShape.GetCapsuleRadius(), CollisionShape.GetCapsuleRadius(),
										   CollisionShape.GetCapsuleHalfHeight())
									 : FVector(SphereRadius);
	for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
	{
		const FBodyInstance& Body = Bodies[Bi];
		if (!Body.bQueryEnabled || Params.IsIgnored(Body.ComponentID, Body.OwnerID) ||
			!ObjectQueryParams.Contains(Body.ObjectType.GetValue()))
		{
			continue;
		}
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
			const FVector Reach = QueryExtent + Body.HalfExtents;
			bOverlaps = Delta.X <= Reach.X && Delta.Y <= Reach.Y && Delta.Z <= Reach.Z;
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
