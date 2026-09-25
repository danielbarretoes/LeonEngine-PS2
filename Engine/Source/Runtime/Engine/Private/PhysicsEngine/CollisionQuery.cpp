#include "Debug/DebugDraw.h"
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

	[[nodiscard]] bool BodyMatchesChannel(const FBodyInstance& Body, ECollisionChannel Channel)
	{
		switch (Channel)
		{
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

	[[nodiscard]] bool SegmentAabb(
		const FVector& Start, const FVector& End, const FVector& Mn, const FVector& Mx, float& OutT, FVector& OutNormal)
	{
		const FVector Dir = End - Start;
		float TEnter = 0.0f;
		float TExit = 1.0f;
		FVector EnterNormal(0.0f, 1.0f, 0.0f);
		bool bHitFace = false;

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (FMath::Abs(Dir[Axis]) < 1.0e-8f)
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
			OutNormal = FVector(0.0f, 1.0f, 0.0f);
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

	[[nodiscard]] bool SegmentFloorY(
		const FVector& Start, const FVector& End, float FloorY, float& OutT, FVector& OutNormal)
	{
		const float Dy = End.Y - Start.Y;
		if (FMath::Abs(Dy) < 1.0e-8f)
		{
			return false;
		}
		const float T = (FloorY - Start.Y) / Dy;
		if (T < 0.0f || T > 1.0f)
		{
			return false;
		}
		OutT = T;
		OutNormal = FVector(0.0f, Dy < 0.0f ? 1.0f : -1.0f, 0.0f);
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
		if (FMath::Abs(Denom) < 1.0e-8f)
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
		SIZE_T LevelMeshIndex, bool bFloorPlane)
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
		Out.LevelMeshIndex = LevelMeshIndex;
		Out.bFloorPlane = bFloorPlane;
	}

	void AppendSlopePlaneHits(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, float Radius,
		float HalfHeight, const TArray<FSlopePlane>& Planes)
	{
		for (const FSlopePlane& Plane : Planes)
		{
			const float NLen = Plane.Normal.Size();
			const float Ny = (NLen > 1.0e-6f) ? (FMath::Abs(Plane.Normal.Y) / NLen) : 1.0f;
			const float Inflate = Radius + (HalfHeight * Ny);
			float T = 1.0f;
			FVector Normal = FVector::ZeroVector;
			if (!SegmentSlopePlane(Start, End, Plane, Inflate, T, Normal))
			{
				continue;
			}
			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, ULevel::Npos, false);
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

	/** Copies the nearest Multi hit into OutHit (a UE Single trace returns the first blocking hit). */
	[[nodiscard]] bool TakeNearestHit(
		const TArray<FHitResult>& Hits, FHitResult& OutHit, const FVector& Start, const FVector& End)
	{
		OutHit = FHitResult();
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;
		OutHit.Time = 1.0f;
		if (Hits.Num() == 0)
		{
			return false;
		}
		OutHit = Hits[0];
		return true;
	}

	void AddRingXz(
		FDebugDraw& Draw, const FVector& Center, float Radius, const FLinearColor& Color, int32 Segments = 16)
	{
		const float SegCount = static_cast<float>(Segments);
		for (int32 I = 0; I < Segments; ++I)
		{
			const float A0 = (static_cast<float>(I) / SegCount) * (2.0f * PI);
			const float A1 = (static_cast<float>(I + 1) / SegCount) * (2.0f * PI);
			AddLine(Draw, Center + FVector(FMath::Cos(A0) * Radius, 0.0f, FMath::Sin(A0) * Radius),
				Center + FVector(FMath::Cos(A1) * Radius, 0.0f, FMath::Sin(A1) * Radius), Color);
		}
	}

	void AddImpactMarker(FDebugDraw& Draw, const FHitResult& Hit)
	{
		const float S = 0.08f;
		AddLine(Draw, Hit.ImpactPoint + FVector(-S, 0, 0), Hit.ImpactPoint + FVector(S, 0, 0), TraceNormal);
		AddLine(Draw, Hit.ImpactPoint + FVector(0, -S, 0), Hit.ImpactPoint + FVector(0, S, 0), TraceNormal);
		AddLine(Draw, Hit.ImpactPoint + FVector(0, 0, -S), Hit.ImpactPoint + FVector(0, 0, S), TraceNormal);
		Draw.AddArrow(Hit.ImpactPoint, Hit.ImpactPoint + (Hit.ImpactNormal * 0.45f), TraceNormal, 0.12f, 0.07f);
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

} // namespace

void DrawDebugLineTrace(FDebugDraw& Draw, const FVector& Start, const FVector& End, const TArray<FHitResult>& Hits)
{
	DrawTracePath(Draw, Start, End, Hits);
}

void DrawDebugSphereTrace(
	FDebugDraw& Draw, const FVector& Start, const FVector& End, float Radius, const TArray<FHitResult>& Hits)
{
	const float R = FMath::Max(Radius, 0.0f);
	DrawTracePath(Draw, Start, End, Hits);
	AddRingXz(Draw, Start, R, TraceShape);
	AddRingXz(Draw, End, R, TraceShape);
	if (Hits.Num() > 0)
	{
		AddRingXz(Draw, Hits[0].ImpactPoint + FVector(0.0f, R, 0.0f), R, TraceHitPath);
	}
}

void DrawDebugCapsuleTrace(FDebugDraw& Draw, const FVector& Start, const FVector& End, float Radius, float HalfHeight,
	const TArray<FHitResult>& Hits)
{
	const float R = FMath::Max(Radius, 0.0f);
	const float Hh = FMath::Max(HalfHeight, 0.0f);
	DrawTracePath(Draw, Start, End, Hits);
	AddRingXz(Draw, Start + FVector(0.0f, Hh, 0.0f), R, TraceShape);
	AddRingXz(Draw, Start - FVector(0.0f, Hh, 0.0f), R, TraceShape);
	AddRingXz(Draw, End + FVector(0.0f, Hh, 0.0f), R, TraceShape);
	AddRingXz(Draw, End - FVector(0.0f, Hh, 0.0f), R, TraceShape);
	AddLine(Draw, Start + FVector(R, -Hh, 0.0f), Start + FVector(R, Hh, 0.0f), TraceShape);
	AddLine(Draw, Start + FVector(-R, -Hh, 0.0f), Start + FVector(-R, Hh, 0.0f), TraceShape);
	if (Hits.Num() > 0)
	{
		AddRingXz(Draw, Hits[0].ImpactPoint, R, TraceHitPath);
	}
}

bool FPhysScene::LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw) const
{
	OutHits.Reset();

	if (BackendIface != nullptr && BackendIface->HasNarrowPhaseTraces())
	{
		// Body instances may have been nudged CMC without a Step: sync before CastRay.
		if (BackendIface->HasRigidWorld())
		{
			BackendIface->RigidPrepareStep(Bodies, Params.SkipLevelMeshIndex);
		}
		(void)BackendIface->RigidLineTrace(OutHits, Start, End, Channel, Params.SkipLevelMeshIndex);
	}
	else
	{
		for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
		{
			const FBodyInstance& Body = Bodies[Bi];
			if (Body.LevelMeshIndex == Params.SkipLevelMeshIndex)
			{
				continue;
			}
			if (!BodyMatchesChannel(Body, Channel))
			{
				continue;
			}
			const FVector Mn = Body.Position - Body.HalfExtents;
			const FVector Mx = Body.Position + Body.HalfExtents;
			float TAabb = 1.0f;
			FVector NAabb = FVector::ZeroVector;
			if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb))
			{
				continue;
			}

			float T = TAabb;
			FVector Normal = NAabb;
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

			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, Body.LevelMeshIndex, false);
			OutHits.Add(Hit);
		}
	}

	if (Params.bTraceFloorPlane)
	{
		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (SegmentFloorY(Start, End, Params.FloorY, T, Normal))
		{
			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, ULevel::Npos, true);
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
	ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw) const
{
	TArray<FHitResult> Hits;
	(void)LineTraceMultiByChannel(Hits, Start, End, Channel, Params, DebugDraw);
	return TakeNearestHit(Hits, OutHit, Start, End);
}

bool FPhysScene::SphereTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	float Radius, ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw) const
{
	const float R = FMath::Max(Radius, 0.0f);
	OutHits.Reset();

	if (BackendIface != nullptr && BackendIface->HasNarrowPhaseTraces())
	{
		// Push the body instances to Jolt before CastShape (same as the Step prepare).
		if (BackendIface->HasRigidWorld())
		{
			BackendIface->RigidPrepareStep(Bodies, Params.SkipLevelMeshIndex);
		}
		(void)BackendIface->RigidSphereTrace(OutHits, Start, End, R, Channel, Params.SkipLevelMeshIndex);
	}
	else
	{
		for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
		{
			const FBodyInstance& Body = Bodies[Bi];
			if (Body.LevelMeshIndex == Params.SkipLevelMeshIndex)
			{
				continue;
			}
			if (!BodyMatchesChannel(Body, Channel))
			{
				continue;
			}
			const FVector Expand(R, R, R);
			const FVector Mn = Body.Position - Body.HalfExtents - Expand;
			const FVector Mx = Body.Position + Body.HalfExtents + Expand;
			float TAabb = 1.0f;
			FVector NAabb = FVector::ZeroVector;
			if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb))
			{
				continue;
			}

			float T = TAabb;
			FVector Normal = NAabb;
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

			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, Body.LevelMeshIndex, false);
			// UE FHitResult: Location = sweep shape center; ImpactPoint = surface contact.
			Hit.ImpactPoint = Hit.Location - (Normal * R);
			OutHits.Add(Hit);
		}
	}

	if (Params.bTraceFloorPlane)
	{
		const float PlaneY = Params.FloorY + R;
		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (SegmentFloorY(Start, End, PlaneY, T, Normal))
		{
			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, ULevel::Npos, true);
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
	ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw) const
{
	TArray<FHitResult> Hits;
	(void)SphereTraceMultiByChannel(Hits, Start, End, Radius, Channel, Params, DebugDraw);
	return TakeNearestHit(Hits, OutHit, Start, End);
}

bool FPhysScene::CapsuleTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End,
	float Radius, float HalfHeight, ECollisionChannel Channel, const FCollisionQueryParams& Params,
	FDebugDraw* DebugDraw) const
{
	const float R = FMath::Max(Radius, 0.0f);
	const float Hh = FMath::Max(HalfHeight, 0.0f);
	OutHits.Reset();
	const FVector Expand(R, Hh + R, R);

	if (BackendIface != nullptr && BackendIface->HasNarrowPhaseTraces())
	{
		if (BackendIface->HasRigidWorld())
		{
			BackendIface->RigidPrepareStep(Bodies, Params.SkipLevelMeshIndex);
		}
		(void)BackendIface->RigidCapsuleTrace(OutHits, Start, End, R, Hh, Channel, Params.SkipLevelMeshIndex);
	}
	else
	{
		for (int32 Bi = 0; Bi < Bodies.Num(); ++Bi)
		{
			const FBodyInstance& Body = Bodies[Bi];
			if (Body.LevelMeshIndex == Params.SkipLevelMeshIndex)
			{
				continue;
			}
			if (!BodyMatchesChannel(Body, Channel))
			{
				continue;
			}
			const FVector Mn = Body.Position - Body.HalfExtents - Expand;
			const FVector Mx = Body.Position + Body.HalfExtents + Expand;
			float TAabb = 1.0f;
			FVector NAabb = FVector::ZeroVector;
			if (!SegmentAabb(Start, End, Mn, Mx, TAabb, NAabb))
			{
				continue;
			}

			float T = TAabb;
			FVector Normal = NAabb;
			if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < TriangleMeshes.Num() &&
				TriangleMeshes[Bi].IsValid())
			{
				// Inflate like the slope planes: radius + |Ny| * half height, approximated with radius + half height.
				float TMesh = 1.0f;
				FVector NMesh = FVector::ZeroVector;
				if (!SegmentTriangleMesh(Start, End, TriangleMeshes[Bi], R + Hh, TMesh, NMesh))
				{
					continue;
				}
				T = TMesh;
				Normal = NMesh;
			}

			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, Body.LevelMeshIndex, false);
			const float Pull = (FMath::Abs(Normal.Y) > 0.5f) ? (Hh + R) : R;
			Hit.ImpactPoint = Hit.Location - (Normal * Pull);
			OutHits.Add(Hit);
		}
	}

	if (Params.bTraceFloorPlane)
	{
		const float PlaneY = Params.FloorY + Hh + R;
		float T = 1.0f;
		FVector Normal = FVector::ZeroVector;
		if (SegmentFloorY(Start, End, PlaneY, T, Normal))
		{
			FHitResult Hit;
			WriteHit(Hit, Start, End, T, Normal, ULevel::Npos, true);
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
	float HalfHeight, ECollisionChannel Channel, const FCollisionQueryParams& Params, FDebugDraw* DebugDraw) const
{
	TArray<FHitResult> Hits;
	(void)CapsuleTraceMultiByChannel(Hits, Start, End, Radius, HalfHeight, Channel, Params, DebugDraw);
	return TakeNearestHit(Hits, OutHit, Start, End);
}
