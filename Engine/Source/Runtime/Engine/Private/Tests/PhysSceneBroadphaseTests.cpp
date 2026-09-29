#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Math/RandomStream.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "Tests/ScopedTestWorld.h"
#include "TriangleCollision.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/**
	 * The scene without its broadphase (the reference): every body in the order it was added and every triangle, as
	 * FPhysScene ran before ps2-shipping N16.
	 */
	namespace BruteForce
	{

		/** Every triangle's nearest hit, the last one at equal times. */
		bool SegmentEveryTriangle(const FVector& Start, const FVector& End, const FTriangleMeshCollision& Mesh,
			float Inflate, float& OutT, FVector& OutNormal)
		{
			bool bAny = false;
			float BestT = 1.0f;
			FVector BestN(0.0f, 0.0f, 1.0f);
			const uint32 VertexCount = static_cast<uint32>(Mesh.Positions.Num());
			for (int32 Tri = 0; Tri < Mesh.Indices.Num() / 3; ++Tri)
			{
				const uint32 I0 = Mesh.Indices[Tri * 3 + 0];
				const uint32 I1 = Mesh.Indices[Tri * 3 + 1];
				const uint32 I2 = Mesh.Indices[Tri * 3 + 2];
				if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
				{
					continue;
				}
				float HitT = 1.0f;
				FVector HitN = FVector::ZeroVector;
				const bool bOk = (Inflate > 1.0e-4f) ? SegmentTriangleInflated(Start, End, Mesh.Positions[I0],
														   Mesh.Positions[I1], Mesh.Positions[I2], Inflate, HitT, HitN)
													 : SegmentTriangle(Start, End, Mesh.Positions[I0],
														   Mesh.Positions[I1], Mesh.Positions[I2], HitT, HitN);
				if (!bOk || HitT > BestT)
				{
					continue;
				}
				BestT = HitT;
				BestN = HitN;
				bAny = true;
			}
			if (bAny)
			{
				OutT = BestT;
				OutNormal = BestN;
			}
			return bAny;
		}

		enum class EShape : uint8
		{
			Line,
			Sphere,
			Capsule,
			Box,
		};

		/** A trace: its shape (cm), its channel or object types, its parameters. */
		struct FTrace
		{
			EShape Shape = EShape::Line;
			float Radius = 0.0f;
			/** Capsule: the cylinder's half height. */
			float HalfHeight = 0.0f;
			FVector Extent = FVector::ZeroVector;
			ECollisionChannel Channel = ECC_Visibility;
			FCollisionQueryParams Params;
			FCollisionResponseParams ResponseParam;
			/** By object type when set. */
			const FCollisionObjectQueryParams* Objects = nullptr;
		};

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

		ECollisionResponse GetResponse(const FBodyInstance& Body, const FTrace& Trace)
		{
			if (Trace.Objects != nullptr)
			{
				return Body.bQueryEnabled && !Trace.Params.IsIgnored(Body.ComponentID, Body.OwnerID) &&
						Trace.Objects->Contains(Body.ObjectType.GetValue())
					? ECR_Block
					: ECR_Ignore;
			}
			return FPhysScene::GetBodyQueryResponse(Body, Trace.Channel, Trace.Params, Trace.ResponseParam);
		}

		/** Every hit, nearest first (a stable sort: at the same time, the bodies in Order, the floor, the slopes). */
		TArray<FHitResult> Multi(const FPhysScene& Scene, const TArray<int32>& Order, const FVector& Start,
			const FVector& End, const FTrace& Trace)
		{
			const TArray<FBodyInstance>& Bodies = Scene.GetBodies();
			const TArray<FTriangleMeshCollision>& Meshes = Scene.GetTriangleMeshes();
			const float R = FMath::Max(Trace.Radius, 0.0f);
			const float Hh = FMath::Max(Trace.HalfHeight, 0.0f);
			const float Bound = FMath::Max(Trace.Extent.X, FMath::Max(Trace.Extent.Y, Trace.Extent.Z));
			TArray<FHitResult> Hits;
			for (const int32 Bi : Order)
			{
				const FBodyInstance& Body = Bodies[Bi];
				const ECollisionResponse Response = GetResponse(Body, Trace);
				if (Response == ECR_Ignore)
				{
					continue;
				}
				float T = 1.0f;
				FVector Normal = FVector::ZeroVector;
				if (Body.CollisionShape == EBodyCollisionShape::Capsule)
				{
					float CapsuleRadius = Body.HalfExtents.X;
					float Cylinder = FMath::Max(0.0f, Body.HalfExtents.Z - Body.HalfExtents.X);
					if (Trace.Shape == EShape::Sphere)
					{
						CapsuleRadius = CapsuleRadius + R;
					}
					else if (Trace.Shape == EShape::Capsule)
					{
						CapsuleRadius = CapsuleRadius + R;
						Cylinder = Cylinder + Hh;
					}
					else if (Trace.Shape == EShape::Box)
					{
						CapsuleRadius = CapsuleRadius + Bound;
					}
					if (!SegmentUprightCapsule(Start, End, Body.Position, CapsuleRadius, Cylinder, T, Normal))
					{
						continue;
					}
				}
				else
				{
					FVector Mn = Body.Position - Body.HalfExtents;
					FVector Mx = Body.Position + Body.HalfExtents;
					float Inflate = 0.0f;
					if (Trace.Shape == EShape::Sphere)
					{
						const FVector Expand(R, R, R);
						Mn = Body.Position - Body.HalfExtents - Expand;
						Mx = Body.Position + Body.HalfExtents + Expand;
						Inflate = R;
					}
					else if (Trace.Shape == EShape::Capsule)
					{
						const FVector Expand(R, R, Hh + R);
						Mn = Body.Position - Body.HalfExtents - Expand;
						Mx = Body.Position + Body.HalfExtents + Expand;
						Inflate = R + Hh;
					}
					else if (Trace.Shape == EShape::Box)
					{
						Mn = Body.Position - Body.HalfExtents - Trace.Extent;
						Mx = Body.Position + Body.HalfExtents + Trace.Extent;
						Inflate = Bound;
					}
					if (!SegmentAabb(Start, End, Mn, Mx, T, Normal))
					{
						continue;
					}
					if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < Meshes.Num() &&
						Meshes[Bi].IsValid())
					{
						float TMesh = 1.0f;
						FVector NMesh = FVector::ZeroVector;
						if (!SegmentEveryTriangle(Start, End, Meshes[Bi], Inflate, TMesh, NMesh))
						{
							continue;
						}
						T = TMesh;
						Normal = NMesh;
					}
				}
				FHitResult Hit;
				WriteHit(Hit, Start, End, T, Normal, Body.ComponentID, false);
				if (Trace.Shape == EShape::Sphere)
				{
					Hit.ImpactPoint = Hit.Location - (Normal * R);
				}
				else if (Trace.Shape == EShape::Capsule)
				{
					const float Pull = (FMath::Abs(Normal.Z) > 0.5f) ? (Hh + R) : R;
					Hit.ImpactPoint = Hit.Location - (Normal * Pull);
				}
				else if (Trace.Shape == EShape::Box)
				{
					Hit.ImpactPoint = Hit.Location - (Normal * FMath::Abs(Normal | Trace.Extent));
				}
				Hit.bBlockingHit = Response == ECR_Block;
				Hit.BodyIndex = Bi;
				UPrimitiveComponent* Owner = Scene.GetBodyOwner(Bi);
				Hit.Component = Owner;
				Hit.Actor = Owner != nullptr ? Owner->GetOwner() : nullptr;
				Hits.Add(Hit);
			}

			if (Trace.Shape != EShape::Box && Trace.Objects == nullptr)
			{
				if (Trace.Params.bTraceFloorPlane)
				{
					const float PlaneZ = Trace.Shape == EShape::Sphere ? Trace.Params.FloorZ + R
						: Trace.Shape == EShape::Capsule               ? Trace.Params.FloorZ + Hh + R
																	   : Trace.Params.FloorZ;
					float T = 1.0f;
					FVector Normal = FVector::ZeroVector;
					if (SegmentFloorZ(Start, End, PlaneZ, T, Normal))
					{
						FHitResult Hit;
						WriteHit(Hit, Start, End, T, Normal, NoComponentID, true);
						if (Trace.Shape == EShape::Sphere)
						{
							Hit.ImpactPoint = Hit.Location - (Normal * R);
						}
						else if (Trace.Shape == EShape::Capsule)
						{
							Hit.ImpactPoint = Hit.Location - (Normal * (Hh + R));
						}
						Hits.Add(Hit);
					}
				}
				const float SlopeRadius = Trace.Shape == EShape::Line ? 0.0f : R;
				const float SlopeHalfHeight = Trace.Shape == EShape::Capsule ? Hh : 0.0f;
				for (const FSlopePlane& Plane : Scene.GetSlopePlanes())
				{
					const float NLen = Plane.Normal.Size();
					const float Nz = (NLen > 1.0e-6f) ? (FMath::Abs(Plane.Normal.Z) / NLen) : 1.0f;
					const float Inflate = SlopeRadius + (SlopeHalfHeight * Nz);
					float T = 1.0f;
					FVector Normal = FVector::ZeroVector;
					if (!SegmentSlopePlane(Start, End, Plane, Inflate, T, Normal))
					{
						continue;
					}
					FHitResult Hit;
					WriteHit(Hit, Start, End, T, Normal, NoComponentID, false);
					Hit.ImpactPoint = Hit.Location - (Normal * Inflate);
					Hits.Add(Hit);
				}
			}
			StableSort(
				Hits.GetData(), Hits.Num(), [](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });
			return Hits;
		}

		/** The first blocking hit of Multi. */
		bool Single(const FPhysScene& Scene, const TArray<int32>& Order, FHitResult& OutHit, const FVector& Start,
			const FVector& End, const FTrace& Trace)
		{
			OutHit = FHitResult();
			OutHit.TraceStart = Start;
			OutHit.TraceEnd = End;
			OutHit.Time = 1.0f;
			for (const FHitResult& Hit : Multi(Scene, Order, Start, End, Trace))
			{
				if (Hit.bBlockingHit)
				{
					OutHit = Hit;
					return true;
				}
			}
			return false;
		}

		/** OverlapMultiByObjectType: the bodies in Order the shape touches. */
		TArray<int32> Overlap(const FPhysScene& Scene, const TArray<int32>& Order, const FVector& Pos,
			const FCollisionObjectQueryParams& Objects, const FCollisionShape& Shape,
			const FCollisionQueryParams& Params)
		{
			const bool bSphere = Shape.IsSphere();
			const float SphereRadius = bSphere ? Shape.GetSphereRadius() : 0.0f;
			const FVector QueryExtent = Shape.IsBox() ? Shape.GetBox()
				: Shape.IsCapsule()
				? FVector(Shape.GetCapsuleRadius(), Shape.GetCapsuleRadius(), Shape.GetCapsuleHalfHeight())
				: FVector(SphereRadius);
			TArray<int32> Found;
			for (const int32 Bi : Order)
			{
				const FBodyInstance& Body = Scene.GetBodies()[Bi];
				if (!Body.bQueryEnabled || Params.IsIgnored(Body.ComponentID, Body.OwnerID) ||
					!Objects.Contains(Body.ObjectType.GetValue()))
				{
					continue;
				}
				bool bOverlaps = false;
				if (bSphere && Body.CollisionShape == EBodyCollisionShape::Capsule)
				{
					const float CapsuleRadius = Body.HalfExtents.X;
					const float Cylinder = FMath::Max(0.0f, Body.HalfExtents.Z - Body.HalfExtents.X);
					const float SegmentZ = FMath::Clamp(Pos.Z, Body.Position.Z - Cylinder, Body.Position.Z + Cylinder);
					const FVector Closest(Body.Position.X, Body.Position.Y, SegmentZ);
					bOverlaps = FVector::DistSquared(Pos, Closest) <= FMath::Square(SphereRadius + CapsuleRadius);
				}
				else if (bSphere)
				{
					const FVector Mn = Body.Position - Body.HalfExtents;
					const FVector Mx = Body.Position + Body.HalfExtents;
					const FVector Closest(FMath::Clamp(Pos.X, Mn.X, Mx.X), FMath::Clamp(Pos.Y, Mn.Y, Mx.Y),
						FMath::Clamp(Pos.Z, Mn.Z, Mx.Z));
					bOverlaps = FVector::DistSquared(Pos, Closest) <= FMath::Square(SphereRadius);
				}
				else
				{
					const FVector Delta = (Pos - Body.Position).GetAbs();
					const FVector Reach = QueryExtent + Body.HalfExtents;
					bOverlaps = Delta.X <= Reach.X && Delta.Y <= Reach.Y && Delta.Z <= Reach.Z;
				}
				if (bOverlaps)
				{
					Found.Add(Bi);
				}
			}
			return Found;
		}

		/** The disc's inset against a top it stands on (cm). */
		constexpr float SupportDiscInset = -2.0f;

		float QuerySupportZ(const FPhysScene& Scene, const TArray<int32>& Order, const FCollisionShape& Capsule,
			const FVector& Feet, float FloorZ, float StepUp, float Skin, SIZE_T IgnoreComponentID)
		{
			float Support = FloorZ;
			const float R = Capsule.GetCapsuleRadius();
			FCollisionQueryParams Query;
			Query.IgnoreComponentID = IgnoreComponentID;
			const TArray<FBodyInstance>& Bodies = Scene.GetBodies();
			const TArray<FTriangleMeshCollision>& Meshes = Scene.GetTriangleMeshes();
			for (const int32 Bi : Order)
			{
				const FBodyInstance& Body = Bodies[Bi];
				if (FPhysScene::GetBodyQueryResponse(Body, ECC_Pawn, Query) != ECR_Block)
				{
					continue;
				}
				if (!XYDiscOverlapsAabb(Feet.X, Feet.Y, R, Body.Position.X, Body.Position.Y, Body.HalfExtents.X,
						Body.HalfExtents.Y, SupportDiscInset))
				{
					continue;
				}
				if (Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Bi < Meshes.Num() &&
					Meshes[Bi].IsValid())
				{
					const float RayTop =
						FMath::Max(Feet.Z + StepUp + Skin + 50.0f, Body.Position.Z + Body.HalfExtents.Z + 50.0f);
					const FVector Start(Feet.X, Feet.Y, RayTop);
					const FVector End(Feet.X, Feet.Y, FloorZ - 100.0f);
					float T = 1.0f;
					FVector Normal = FVector::ZeroVector;
					if (SegmentEveryTriangle(Start, End, Meshes[Bi], 0.0f, T, Normal) && Normal.Z > 0.15f)
					{
						const float ZHit = Start.Z + ((End.Z - Start.Z) * T);
						if (ZHit <= Feet.Z + StepUp + Skin)
						{
							Support = FMath::Max(Support, ZHit);
						}
					}
					continue;
				}
				const float Top = Body.Position.Z + Body.HalfExtents.Z;
				if (Feet.Z + StepUp + Skin < Top)
				{
					continue;
				}
				Support = FMath::Max(Support, Top);
			}
			for (const FSlopePlane& Plane : Scene.GetSlopePlanes())
			{
				if (FMath::Abs(Plane.Normal.Z) < 1.0e-4f)
				{
					continue;
				}
				const float ZOnPlane = Plane.Point.Z -
					((Plane.Normal.X * (Feet.X - Plane.Point.X)) + (Plane.Normal.Y * (Feet.Y - Plane.Point.Y))) /
						Plane.Normal.Z;
				if (Feet.Z + StepUp + Skin < ZOnPlane)
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

		void ApplyDynamicWishPush(FBodyInstance& Body, const FVector2D& WishN, const FVector2D& Normal,
			float PushStrength, float WalkBounds, bool bApplyContactShove)
		{
			const float Into = FMath::Max(0.0f, -FVector2D::DotProduct(WishN, Normal));
			if (Into <= 1.0e-4f)
			{
				return;
			}
			const float BodyMass = FMath::Max(Body.Mass, 0.5f);
			const float InvMass = 1.0f / BodyMass;
			Body.VelXY += WishN * (Into * PushStrength * InvMass * 280.0f);
			const float Speed = Body.VelXY.Size();
			if (Speed > 400.0f)
			{
				Body.VelXY *= 400.0f / Speed;
			}
			if (!bApplyContactShove)
			{
				return;
			}
			const float BodyShare = 80.0f / (80.0f + BodyMass);
			const float Shove = Into * PushStrength * BodyShare * 6.0f;
			Body.Position.X += WishN.X * Shove;
			Body.Position.Y += WishN.Y * Shove;
			ClampPositionXY(Body.Position, WalkBounds);
		}

		void ResolveCapsuleSides(TArray<FBodyInstance>& Bodies, const TArray<int32>& Order,
			const FCollisionShape& Capsule, FVector& Feet, const FVector2D& WishXY, const FCapsuleContactParams& Params,
			SIZE_T IgnoreComponentID)
		{
			const float R = Capsule.GetCapsuleRadius();
			const float FeetZ = Feet.Z;
			const float Head = FeetZ + Capsule.GetCapsuleHalfHeight() * 2.0f;
			const bool bHasWish = WishXY.Size() > 1.0e-4f;
			const FVector2D WishN = bHasWish ? WishXY.GetSafeNormal() : FVector2D::ZeroVector;
			FCollisionQueryParams Query;
			Query.IgnoreComponentID = IgnoreComponentID;
			for (const int32 Bi : Order)
			{
				FBodyInstance& Body = Bodies[Bi];
				if (FPhysScene::GetBodyQueryResponse(Body, ECC_Pawn, Query) != ECR_Block ||
					Body.CollisionShape == EBodyCollisionShape::TriangleMesh)
				{
					continue;
				}
				const float Hx = Body.HalfExtents.X;
				const float Hy = Body.HalfExtents.Y;
				const float Top = Body.Position.Z + Body.HalfExtents.Z;
				const float Bottom = Body.Position.Z - Body.HalfExtents.Z;
				if (Head < Bottom)
				{
					continue;
				}
				if (FeetZ >= Top - Params.Skin)
				{
					continue;
				}
				FVector2D Normal = FVector2D::ZeroVector;
				float Penetration = 0.0f;
				if (!CapsuleAabbMtv(Feet.X, Feet.Y, R, Body.Position.X, Body.Position.Y, Hx, Hy, Normal, Penetration))
				{
					continue;
				}
				if (Body.Type == EBodyType::Static)
				{
					Feet.X += Normal.X * Penetration;
					Feet.Y += Normal.Y * Penetration;
					ClampPositionXY(Feet, Params.WalkBounds);
					continue;
				}
				const float BodyMass = FMath::Max(Body.Mass, 0.5f);
				const float InvSum = 1.0f / (80.0f + BodyMass);
				const float PlayerShare = BodyMass * InvSum;
				const float BodyShare = 80.0f * InvSum;
				Feet.X += Normal.X * (Penetration * PlayerShare);
				Feet.Y += Normal.Y * (Penetration * PlayerShare);
				Body.Position.X -= Normal.X * (Penetration * BodyShare);
				Body.Position.Y -= Normal.Y * (Penetration * BodyShare);
				ClampPositionXY(Feet, Params.WalkBounds);
				ClampPositionXY(Body.Position, Params.WalkBounds);
				CancelVelocityInto(Body.VelXY, -Normal);
				if (bHasWish)
				{
					ApplyDynamicWishPush(Body, WishN, Normal, Params.PushStrength, Params.WalkBounds, false);
				}
			}
		}

		bool ApplyCapsuleSweepPush(TArray<FBodyInstance>& Bodies, const TArray<int32>& Order, SIZE_T ComponentID,
			const FVector2D& WishXY, const FVector& ImpactNormal, float PushStrength, float WalkBounds)
		{
			FVector2D Normal(ImpactNormal.X, ImpactNormal.Y);
			const float NLen = Normal.Size();
			if (ComponentID == NoComponentID || WishXY.Size() <= 1.0e-4f || NLen <= 1.0e-4f)
			{
				return false;
			}
			Normal /= NLen;
			const FVector2D WishN = WishXY.GetSafeNormal();
			for (const int32 Bi : Order)
			{
				FBodyInstance& Body = Bodies[Bi];
				if (Body.ComponentID != ComponentID || Body.Type != EBodyType::Dynamic)
				{
					continue;
				}
				if (FMath::Max(0.0f, -FVector2D::DotProduct(WishN, Normal)) <= 1.0e-4f)
				{
					return false;
				}
				ApplyDynamicWishPush(Body, WishN, Normal, PushStrength, WalkBounds, true);
				return true;
			}
			return false;
		}

		void Step(TArray<FBodyInstance>& Bodies, const TArray<int32>& Order, const FPhysSceneStepParams& Params)
		{
			const float Damp = FMath::Exp(-Params.Damping * Params.DeltaTime);
			for (const int32 Bi : Order)
			{
				FBodyInstance& Body = Bodies[Bi];
				if (Body.Type != EBodyType::Dynamic || !Body.bPhysicsEnabled)
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

			for (int32 Iter = 0; Iter < 6; ++Iter)
			{
				for (int32 I = 0; I < Order.Num(); ++I)
				{
					if (Bodies[Order[I]].ComponentID == Params.IgnoreComponentID || !Bodies[Order[I]].bPhysicsEnabled)
					{
						continue;
					}
					for (int32 J = I + 1; J < Order.Num(); ++J)
					{
						if (Bodies[Order[J]].ComponentID == Params.IgnoreComponentID ||
							!Bodies[Order[J]].bPhysicsEnabled)
						{
							continue;
						}
						FBodyInstance& A = Bodies[Order[I]];
						FBodyInstance& B = Bodies[Order[J]];
						const bool bADyn = A.Type == EBodyType::Dynamic;
						const bool bBDyn = B.Type == EBodyType::Dynamic;
						if (!bADyn && !bBDyn)
						{
							continue;
						}
						float MoveA = 0.0f;
						float MoveB = 0.0f;
						if (bADyn && bBDyn)
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
						FVector Normal = FVector::ZeroVector;
						if (!SeparateAabb(A.Position, A.HalfExtents, B.Position, B.HalfExtents, MoveA, MoveB, &Normal))
						{
							continue;
						}
						ClampPositionXY(A.Position, Params.WalkBounds);
						ClampPositionXY(B.Position, Params.WalkBounds);
						if (bADyn)
						{
							CancelVelocityInto(A.VelXY, FVector2D(Normal.X, Normal.Y));
							CancelVelocityZInto(A.VelocityZ, Normal.Z);
						}
						if (bBDyn)
						{
							CancelVelocityInto(B.VelXY, FVector2D(-Normal.X, -Normal.Y));
							CancelVelocityZInto(B.VelocityZ, -Normal.Z);
						}
					}
				}
			}

			for (const int32 Bi : Order)
			{
				FBodyInstance& Body = Bodies[Bi];
				if (Body.Type != EBodyType::Dynamic || !Body.bEnableGravity || !Body.bPhysicsEnabled)
				{
					continue;
				}
				float Support = Params.FloorZ;
				const float Bx0 = Body.Position.X - Body.HalfExtents.X;
				const float Bx1 = Body.Position.X + Body.HalfExtents.X;
				const float By0 = Body.Position.Y - Body.HalfExtents.Y;
				const float By1 = Body.Position.Y + Body.HalfExtents.Y;
				const float Bottom = Body.Position.Z - Body.HalfExtents.Z;
				for (const int32 Oi : Order)
				{
					const FBodyInstance& Other = Bodies[Oi];
					if (Other.ComponentID == Body.ComponentID || Other.ComponentID == Params.IgnoreComponentID ||
						!Other.bPhysicsEnabled)
					{
						continue;
					}
					if (Bx1 < Other.Position.X - Other.HalfExtents.X || Bx0 > Other.Position.X + Other.HalfExtents.X ||
						By1 < Other.Position.Y - Other.HalfExtents.Y || By0 > Other.Position.Y + Other.HalfExtents.Y)
					{
						continue;
					}
					const float Top = Other.Position.Z + Other.HalfExtents.Z;
					if (Other.Type == EBodyType::Dynamic && Bottom + Params.Skin < Top - 2.0f &&
						Body.Position.Z <= Other.Position.Z)
					{
						continue;
					}
					Support = FMath::Max(Support, Top);
				}
				const float Window =
					FMath::Max(12.0f, (FMath::Abs(Body.VelocityZ) * Params.DeltaTime) + (Params.Skin * 4.0f));
				if (Body.VelocityZ <= 0.0f && Bottom <= Support + Params.Skin && Bottom >= Support - Window)
				{
					Body.Position.Z = Support + Body.HalfExtents.Z;
					Body.VelocityZ = 0.0f;
					if (Body.VelXY.Size() < 8.0f)
					{
						Body.VelXY = FVector2D::ZeroVector;
					}
				}
			}
		}

	} // namespace BruteForce

	/** What differs between two hits ("" when nothing does). */
	FString DiffHits(const FHitResult& A, const FHitResult& B)
	{
		FString Diff;
		auto Check = [&Diff](bool bSame, const TCHAR* Field)
		{
			if (!bSame)
			{
				Diff += FString::Printf("%s ", Field);
			}
		};
		Check(A.bBlockingHit == B.bBlockingHit, "bBlockingHit");
		Check(A.Time == B.Time, "Time");
		Check(A.Distance == B.Distance, "Distance");
		Check(A.Location == B.Location, "Location");
		Check(A.ImpactPoint == B.ImpactPoint, "ImpactPoint");
		Check(A.ImpactNormal == B.ImpactNormal, "ImpactNormal");
		Check(A.TraceStart == B.TraceStart, "TraceStart");
		Check(A.TraceEnd == B.TraceEnd, "TraceEnd");
		Check(A.ComponentID == B.ComponentID, "ComponentID");
		Check(A.bFloorPlane == B.bFloorPlane, "bFloorPlane");
		Check(A.BodyIndex == B.BodyIndex, "BodyIndex");
		Check(A.GetComponent() == B.GetComponent(), "Component");
		Check(A.GetActor() == B.GetActor(), "Actor");
		return Diff;
	}

	/** Counts what differs between the scene's answers and the brute force's, and reports the first few. */
	struct FMismatchLog
	{
		FAutomationTestBase& Test;
		int32 Count = 0;
		int32 Compared = 0;

		void Report(bool bSame, const FString& What)
		{
			++Compared;
			if (bSame)
			{
				return;
			}
			++Count;
			if (Count <= 8)
			{
				Test.AddError(What);
			}
		}

		void CompareHits(const TArray<FHitResult>& Scene, const TArray<FHitResult>& Reference, const FString& What)
		{
			if (Scene.Num() != Reference.Num())
			{
				Report(false, FString::Printf("%s: %d hits, the brute force %d", *What, Scene.Num(), Reference.Num()));
				return;
			}
			FString Diffs;
			for (int32 Index = 0; Index < Scene.Num(); ++Index)
			{
				const FString Diff = DiffHits(Scene[Index], Reference[Index]);
				if (!Diff.IsEmpty())
				{
					Diffs += FString::Printf("hit %d: %s; ", Index, *Diff);
				}
			}
			Report(Diffs.IsEmpty(), FString::Printf("%s: %s", *What, *Diffs));
		}

		void CompareHit(
			bool bScene, const FHitResult& Scene, bool bReference, const FHitResult& Reference, const FString& What)
		{
			const FString Diff = DiffHits(Scene, Reference);
			Report(bScene == bReference && Diff.IsEmpty(),
				FString::Printf("%s: hit %d / %d, differs: %s", *What, bScene ? 1 : 0, bReference ? 1 : 0, *Diff));
		}

		void CompareBodies(
			const TArray<FBodyInstance>& Scene, const TArray<FBodyInstance>& Reference, const FString& What)
		{
			bool bSame = Scene.Num() == Reference.Num();
			for (int32 Index = 0; bSame && Index < Scene.Num(); ++Index)
			{
				bSame = Scene[Index].Position == Reference[Index].Position &&
					Scene[Index].VelXY == Reference[Index].VelXY &&
					Scene[Index].VelocityZ == Reference[Index].VelocityZ;
			}
			Report(bSame, FString::Printf("%s: the bodies differ", *What));
		}
	};

	/** 0 .. Num - 1: a scene's body order while nothing was removed. */
	TArray<int32> IndexOrder(const FPhysScene& Scene)
	{
		TArray<int32> Order;
		for (int32 Index = 0; Index < Scene.GetBodies().Num(); ++Index)
		{
			Order.Add(Index);
		}
		return Order;
	}

	FVector RandomInBox(const FRandomStream& Random, const FVector& Min, const FVector& Max)
	{
		return FVector(
			Random.FRandRange(Min.X, Max.X), Random.FRandRange(Min.Y, Max.Y), Random.FRandRange(Min.Z, Max.Z));
	}

	/**
	 * A random scene of NumBodies bodies within Extent (cm) of the origin on X and Y: boxes (some flat or thin),
	 * capsules (some shorter than wide), triangle meshes (static; triangles of all sizes, some outside the body's box),
	 * static and dynamic, with random object types, responses, owners and query / physics parts, and two slopes.
	 */
	void BuildRandomScene(FPhysScene& Scene, const FRandomStream& Random, int32 NumBodies, float Extent)
	{
		for (int32 Index = 0; Index < NumBodies; ++Index)
		{
			const EBodyType Type = Random.FRand() < 0.3f ? EBodyType::Dynamic : EBodyType::Static;
			// Some bodies share their predecessor's component id (the step skips its own component's bodies).
			const SIZE_T ComponentID = static_cast<SIZE_T>(Random.FRand() < 0.1f ? 999 + Index : 1000 + Index);
			(void)Scene.AddBody({ComponentID, Type, Random.FRandRange(0.5f, 60.0f), Random.FRand() < 0.8f});
		}
		TArray<FBodyInstance>& Bodies = Scene.GetBodies();
		TArray<FTriangleMeshCollision>& Meshes = Scene.GetTriangleMeshes();
		const ECollisionChannel ObjectTypes[] = {ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn, ECC_PhysicsBody};
		const ECollisionChannel Channels[] = {ECC_Visibility, ECC_Camera, ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn};
		for (int32 Index = 0; Index < NumBodies; ++Index)
		{
			FBodyInstance& Body = Bodies[Index];
			Body.Position = RandomInBox(Random, FVector(-Extent, -Extent, -50.0f), FVector(Extent, Extent, 400.0f));
			Body.OwnerID = static_cast<SIZE_T>(5000 + (Index / 3));
			Body.ObjectType = ObjectTypes[Random.RandRange(0, 3)];
			for (const ECollisionChannel Channel : Channels)
			{
				const float Roll = Random.FRand();
				(void)Body.CollisionResponses.SetResponse(
					Channel, Roll < 0.7f ? ECR_Block : (Roll < 0.85f ? ECR_Overlap : ECR_Ignore));
			}
			Body.bQueryEnabled = Random.FRand() < 0.9f;
			Body.bPhysicsEnabled = Random.FRand() < 0.9f;
			if (Body.Type == EBodyType::Dynamic)
			{
				Body.VelXY = FVector2D(Random.FRandRange(-300.0f, 300.0f), Random.FRandRange(-300.0f, 300.0f));
			}

			const float Shape = Random.FRand();
			if (Shape < 0.5f || (Shape >= 0.75f && Body.Type == EBodyType::Dynamic))
			{
				Body.CollisionShape = EBodyCollisionShape::Box;
				Body.HalfExtents = RandomInBox(Random, FVector(5.0f, 5.0f, 5.0f), FVector(200.0f, 200.0f, 150.0f));
				if (Random.FRand() < 0.15f)
				{
					Body.HalfExtents.Z = 0.5f;
				}
			}
			else if (Shape < 0.75f)
			{
				Body.CollisionShape = EBodyCollisionShape::Capsule;
				const float Radius = Random.FRandRange(10.0f, 60.0f);
				Body.HalfExtents = FVector(Radius, Radius, Radius * Random.FRandRange(0.4f, 3.0f));
			}
			else
			{
				// A static triangle mesh: a soup around the body, whose box may not hold all of it.
				Body.CollisionShape = EBodyCollisionShape::TriangleMesh;
				const float Size = Random.FRandRange(20.0f, 300.0f);
				Body.HalfExtents = FVector(Size * Random.FRandRange(0.6f, 1.2f));
				FTriangleMeshCollision Mesh;
				const int32 NumTriangles = Random.RandRange(2, 30);
				for (int32 Tri = 0; Tri < NumTriangles; ++Tri)
				{
					const FVector Center = Body.Position + RandomInBox(Random, FVector(-Size), FVector(Size));
					const float TriangleSize = Random.FRand() < 0.2f ? Random.FRandRange(0.05f, 2.0f) : Size;
					for (int32 Corner = 0; Corner < 3; ++Corner)
					{
						Mesh.Indices.Add(static_cast<uint32>(Mesh.Positions.Num()));
						Mesh.Positions.Add(Center + RandomInBox(Random, FVector(-TriangleSize), FVector(TriangleSize)));
					}
				}
				Meshes[Index] = MoveTemp(Mesh);
			}

			// Some bodies sit exactly where the one before sits: hits at the same time.
			const FBodyInstance& Previous = Bodies[FMath::Max(Index - 1, 0)];
			if (Index > 0 && Random.FRand() < 0.15f &&
				(Previous.CollisionShape != EBodyCollisionShape::TriangleMesh || Body.Type == EBodyType::Static))
			{
				Body.Position = Previous.Position;
				Body.HalfExtents = Previous.HalfExtents;
				Body.CollisionShape = Previous.CollisionShape;
				Meshes[Index] = Meshes[Index - 1];
			}
		}
		Scene.AddSlopeRamp(FVector(Extent * 0.3f, 0.0f, 100.0f), FVector(300.0f, 300.0f, 200.0f), 25.0f);
		Scene.AddSlopeRamp(
			FVector(-Extent * 0.4f, Extent * 0.2f, 50.0f), FVector(200.0f, 400.0f, 150.0f), 35.0f, 90.0f);
	}

	/** A segment in the scene: mostly anywhere, some along an axis, straight down, short or of no length. */
	void RandomSegment(const FRandomStream& Random, float Extent, FVector& OutStart, FVector& OutEnd)
	{
		const FVector Min(-Extent * 1.1f, -Extent * 1.1f, -100.0f);
		const FVector Max(Extent * 1.1f, Extent * 1.1f, 500.0f);
		OutStart = RandomInBox(Random, Min, Max);
		const float Kind = Random.FRand();
		if (Kind < 0.6f)
		{
			OutEnd = RandomInBox(Random, Min, Max);
		}
		else if (Kind < 0.75f)
		{
			OutEnd = OutStart;
			const int32 Axis = Random.RandRange(0, 2);
			OutEnd[Axis] = Random.FRandRange(Min[Axis], Max[Axis]);
		}
		else if (Kind < 0.85f)
		{
			OutEnd = FVector(OutStart.X, OutStart.Y, -150.0f);
		}
		else if (Kind < 0.97f)
		{
			OutEnd = OutStart + RandomInBox(Random, FVector(-30.0f), FVector(30.0f));
		}
		else
		{
			OutEnd = OutStart;
		}
	}

	/** Random trace parameters: channel, floor plane, ignored bodies and actors, and responses to object types. */
	BruteForce::FTrace RandomTrace(const FRandomStream& Random, int32 NumBodies)
	{
		const ECollisionChannel Channels[] = {ECC_Visibility, ECC_Camera, ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn};
		BruteForce::FTrace Trace;
		Trace.Channel = Channels[Random.RandRange(0, 4)];
		Trace.Params.bTraceFloorPlane = Random.FRand() < 0.4f;
		Trace.Params.FloorZ = Random.FRandRange(-60.0f, 20.0f);
		if (Random.FRand() < 0.3f)
		{
			Trace.Params.IgnoreComponentID = static_cast<SIZE_T>(1000 + Random.RandRange(0, NumBodies - 1));
		}
		if (Random.FRand() < 0.3f)
		{
			Trace.Params.AddIgnoredActorID(static_cast<SIZE_T>(5000 + Random.RandRange(0, NumBodies / 3)));
		}
		if (Random.FRand() < 0.3f)
		{
			(void)Trace.ResponseParam.CollisionResponse.SetResponse(ECC_Pawn, ECR_Overlap);
			(void)Trace.ResponseParam.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);
		}
		return Trace;
	}

	/** Every trace of the scene (Single and Multi, each shape, by channel and by object type) against the brute force.
	 */
	void CompareTraces(FMismatchLog& Log, const FPhysScene& Scene, const TArray<int32>& Order,
		const FRandomStream& Random, int32 NumTraces, float Extent, const FString& What)
	{
		const FCollisionObjectQueryParams Objects[] = {
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
			FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionObjectQueryParams(ECC_Pawn)};
		for (int32 Index = 0; Index < NumTraces; ++Index)
		{
			FVector Start;
			FVector End;
			RandomSegment(Random, Extent, Start, End);
			BruteForce::FTrace Trace = RandomTrace(Random, Order.Num());
			const FString Label = FString::Printf("%s trace %d", *What, Index);
			TArray<FHitResult> Hits;
			FHitResult Hit;
			FHitResult ReferenceHit;
			bool bHit = false;
			bool bReference = false;

			Trace.Shape = BruteForce::EShape::Line;
			(void)Scene.LineTraceMultiByChannel(
				Hits, Start, End, Trace.Channel, Trace.Params, nullptr, Trace.ResponseParam);
			Log.CompareHits(Hits, BruteForce::Multi(Scene, Order, Start, End, Trace), Label + " line multi");
			bHit = Scene.LineTraceSingleByChannel(
				Hit, Start, End, Trace.Channel, Trace.Params, nullptr, Trace.ResponseParam);
			bReference = BruteForce::Single(Scene, Order, ReferenceHit, Start, End, Trace);
			Log.CompareHit(bHit, Hit, bReference, ReferenceHit, Label + " line single");

			Trace.Shape = BruteForce::EShape::Sphere;
			Trace.Radius = Random.FRandRange(-5.0f, 80.0f);
			(void)Scene.SphereTraceMultiByChannel(
				Hits, Start, End, Trace.Radius, Trace.Channel, Trace.Params, nullptr, Trace.ResponseParam);
			Log.CompareHits(Hits, BruteForce::Multi(Scene, Order, Start, End, Trace), Label + " sphere multi");
			bHit = Scene.SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Trace.Channel,
				FCollisionShape::MakeSphere(Trace.Radius), Trace.Params, Trace.ResponseParam);
			bReference = BruteForce::Single(Scene, Order, ReferenceHit, Start, End, Trace);
			Log.CompareHit(bHit, Hit, bReference, ReferenceHit, Label + " sphere single");

			Trace.Shape = BruteForce::EShape::Capsule;
			Trace.Radius = Random.FRandRange(5.0f, 50.0f);
			Trace.HalfHeight = Random.FRandRange(0.0f, 90.0f);
			(void)Scene.CapsuleTraceMultiByChannel(Hits, Start, End, Trace.Radius, Trace.HalfHeight, Trace.Channel,
				Trace.Params, nullptr, Trace.ResponseParam);
			Log.CompareHits(Hits, BruteForce::Multi(Scene, Order, Start, End, Trace), Label + " capsule multi");
			bHit = Scene.CapsuleTraceSingleByChannel(Hit, Start, End, Trace.Radius, Trace.HalfHeight, Trace.Channel,
				Trace.Params, nullptr, Trace.ResponseParam);
			bReference = BruteForce::Single(Scene, Order, ReferenceHit, Start, End, Trace);
			Log.CompareHit(bHit, Hit, bReference, ReferenceHit, Label + " capsule single");

			Trace.Shape = BruteForce::EShape::Box;
			Trace.Extent = RandomInBox(Random, FVector(1.0f), FVector(60.0f));
			const FCollisionShape Box = FCollisionShape::MakeBox(Trace.Extent);
			(void)Scene.SweepMultiByChannel(
				Hits, Start, End, FQuat::Identity, Trace.Channel, Box, Trace.Params, Trace.ResponseParam);
			Log.CompareHits(Hits, BruteForce::Multi(Scene, Order, Start, End, Trace), Label + " box multi");
			bHit = Scene.SweepSingleByChannel(
				Hit, Start, End, FQuat::Identity, Trace.Channel, Box, Trace.Params, Trace.ResponseParam);
			bReference = BruteForce::Single(Scene, Order, ReferenceHit, Start, End, Trace);
			Log.CompareHit(bHit, Hit, bReference, ReferenceHit, Label + " box single");

			Trace.Shape = BruteForce::EShape::Line;
			Trace.Objects = &Objects[Index % 3];
			(void)Scene.LineTraceMultiByObjectType(Hits, Start, End, *Trace.Objects, Trace.Params);
			Log.CompareHits(Hits, BruteForce::Multi(Scene, Order, Start, End, Trace), Label + " object multi");
			bHit = Scene.LineTraceSingleByObjectType(Hit, Start, End, *Trace.Objects, Trace.Params);
			bReference = BruteForce::Single(Scene, Order, ReferenceHit, Start, End, Trace);
			Log.CompareHit(bHit, Hit, bReference, ReferenceHit, Label + " object single");
		}
	}

	/** Overlaps and support heights at random places against the brute force. */
	void CompareContacts(FMismatchLog& Log, const FPhysScene& Scene, const TArray<int32>& Order,
		const FRandomStream& Random, int32 NumQueries, float Extent, const FString& What)
	{
		const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
		for (int32 Index = 0; Index < NumQueries; ++Index)
		{
			const FVector Pos = RandomInBox(Random, FVector(-Extent, -Extent, -50.0f), FVector(Extent, Extent, 400.0f));
			const FCollisionShape Shape = Index % 3 == 0
				? FCollisionShape::MakeSphere(Random.FRandRange(1.0f, 150.0f))
				: (Index % 3 == 1 ? FCollisionShape::MakeBox(RandomInBox(Random, FVector(1.0f), FVector(120.0f)))
								  : FCollisionShape::MakeCapsule(
										Random.FRandRange(5.0f, 60.0f), Random.FRandRange(60.0f, 120.0f)));
			FCollisionQueryParams Params;
			if (Random.FRand() < 0.3f)
			{
				Params.AddIgnoredActorID(static_cast<SIZE_T>(5000 + Random.RandRange(0, Order.Num() / 3)));
			}
			TArray<FOverlapResult> Overlaps;
			(void)Scene.OverlapMultiByObjectType(Overlaps, Pos, FQuat::Identity, Objects, Shape, Params);
			const TArray<int32> Reference = BruteForce::Overlap(Scene, Order, Pos, Objects, Shape, Params);
			bool bSame = Overlaps.Num() == Reference.Num();
			for (int32 Item = 0; bSame && Item < Overlaps.Num(); ++Item)
			{
				bSame = Overlaps[Item].ItemIndex == Reference[Item] &&
					Overlaps[Item].GetComponent() == Scene.GetBodyOwner(Reference[Item]);
			}
			Log.Report(bSame, FString::Printf("%s overlap %d differs", *What, Index));

			const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Random.FRandRange(10.0f, 45.0f), 90.0f);
			const float FloorZ = Random.FRandRange(-60.0f, 0.0f);
			const float Support = Scene.QuerySupportZ(Capsule, Pos, FloorZ, 35.0f, 2.0f, NoComponentID);
			const float ReferenceSupport =
				BruteForce::QuerySupportZ(Scene, Order, Capsule, Pos, FloorZ, 35.0f, 2.0f, NoComponentID);
			Log.Report(Support == ReferenceSupport,
				FString::Printf("%s support %d: %.6g, the brute force %.6g", *What, Index, static_cast<double>(Support),
					static_cast<double>(ReferenceSupport)));
		}
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneBroadphaseTracesTest,
	"System.Engine.PhysScene.Broadphase.TracesMatchBruteForce",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPhysSceneBroadphaseTracesTest::RunTest(const FString& Parameters)
{
	// Random scenes of boxes, capsules and triangle meshes: every trace (line, sphere, capsule and box, Single and
	// Multi, by channel and by object type) gives the brute force's hits, in its order, field for field.
	FMismatchLog Log{*this};
	for (int32 Seed = 0; Seed < 4; ++Seed)
	{
		const FRandomStream Random(1600 + Seed);
		FPhysScene Scene;
		constexpr float Extent = 1500.0f;
		BuildRandomScene(Scene, Random, 80, Extent);
		const FPhysScene& ConstScene = Scene;
		CompareTraces(Log, ConstScene, IndexOrder(ConstScene), Random, 250, Extent, FString::Printf("Seed %d", Seed));
	}
	TestEqual("Mismatches", Log.Count, 0);
	TestEqual("Every query compared", Log.Compared, 4 * 250 * 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneBroadphaseContactsTest,
	"System.Engine.PhysScene.Broadphase.ContactsMatchBruteForce",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPhysSceneBroadphaseContactsTest::RunTest(const FString& Parameters)
{
	// Overlaps, support heights, side contacts (the feet pushed out of the bodies, dynamic ones pushed back, some far
	// enough to look the bodies up again) and sweep pushes give the brute force's results in random scenes.
	FMismatchLog Log{*this};
	for (int32 Seed = 0; Seed < 4; ++Seed)
	{
		const FRandomStream Random(1700 + Seed);
		FPhysScene Scene;
		constexpr float Extent = 1200.0f;
		BuildRandomScene(Scene, Random, 90, Extent);
		const FPhysScene& ConstScene = Scene;
		const TArray<int32> Order = IndexOrder(ConstScene);
		const FString What = FString::Printf("Seed %d", Seed);
		CompareContacts(Log, ConstScene, Order, Random, 300, Extent, What);

		TArray<FBodyInstance> Reference = ConstScene.GetBodies();
		for (int32 Index = 0; Index < 300; ++Index)
		{
			const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Random.FRandRange(10.0f, 45.0f), 90.0f);
			const FVector Start =
				RandomInBox(Random, FVector(-Extent, -Extent, -20.0f), FVector(Extent, Extent, 300.0f));
			const FVector2D Wish(Random.FRandRange(-1.0f, 1.0f), Random.FRandRange(-1.0f, 1.0f));
			FCapsuleContactParams Contact;
			Contact.WalkBounds = Index % 5 == 0 ? 800.0f : 1800.0f;
			FVector Feet = Start;
			FVector ReferenceFeet = Start;
			Scene.ResolveCapsuleSides(Capsule, Feet, Wish, Contact, NoComponentID);
			BruteForce::ResolveCapsuleSides(Reference, Order, Capsule, ReferenceFeet, Wish, Contact, NoComponentID);
			Log.Report(Feet == ReferenceFeet, FString::Printf("%s side contact %d: the feet differ", *What, Index));
			Log.CompareBodies(ConstScene.GetBodies(), Reference, FString::Printf("%s side contact %d", *What, Index));

			const SIZE_T ComponentID = static_cast<SIZE_T>(1000 + Random.RandRange(0, Order.Num() - 1));
			const FVector Normal(Random.FRandRange(-1.0f, 1.0f), Random.FRandRange(-1.0f, 1.0f), 0.0f);
			const bool bPushed = Scene.ApplyCapsuleSweepPush(ComponentID, Wish, Normal, 0.85f, 1800.0f);
			const bool bReferencePushed =
				BruteForce::ApplyCapsuleSweepPush(Reference, Order, ComponentID, Wish, Normal, 0.85f, 1800.0f);
			Log.Report(bPushed == bReferencePushed, FString::Printf("%s sweep push %d", *What, Index));
			Log.CompareBodies(ConstScene.GetBodies(), Reference, FString::Printf("%s sweep push %d", *What, Index));
		}
		// The pushed bodies moved in the broadphase too.
		CompareTraces(Log, ConstScene, Order, Random, 60, Extent, What + " after the pushes");
	}
	TestEqual("Mismatches", Log.Count, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneBroadphaseStepTest,
	"System.Engine.PhysScene.Broadphase.StepMatchesBruteForce",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPhysSceneBroadphaseStepTest::RunTest(const FString& Parameters)
{
	// The step of a crowded random scene (dynamic bodies falling into each other and onto static ones, some pushed
	// past the walk bounds, where static bodies are clamped too) moves every body as the step over every pair does,
	// step after step; then the traces still match.
	FMismatchLog Log{*this};
	for (int32 Seed = 0; Seed < 3; ++Seed)
	{
		const FRandomStream Random(1800 + Seed);
		FPhysScene Scene;
		constexpr float Extent = 700.0f;
		BuildRandomScene(Scene, Random, 70, Extent);
		const FPhysScene& ConstScene = Scene;
		const TArray<int32> Order = IndexOrder(ConstScene);
		TArray<FBodyInstance> Reference = ConstScene.GetBodies();
		FPhysSceneStepParams Params;
		Params.DeltaTime = 1.0f / 30.0f;
		Params.WalkBounds = 600.0f;
		Params.FloorZ = -40.0f;
		Params.IgnoreComponentID = static_cast<SIZE_T>(1003);
		for (int32 Frame = 0; Frame < 90; ++Frame)
		{
			Scene.Step(Params);
			BruteForce::Step(Reference, Order, Params);
			Log.CompareBodies(ConstScene.GetBodies(), Reference, FString::Printf("Seed %d frame %d", Seed, Frame));
		}
		CompareTraces(Log, ConstScene, Order, Random, 60, Extent, FString::Printf("Seed %d after the steps", Seed));
	}
	TestEqual("Mismatches", Log.Count, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneBroadphaseManyBodiesTest, "System.Engine.PhysScene.Broadphase.ManyBodies",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPhysSceneBroadphaseManyBodiesTest::RunTest(const FString& Parameters)
{
	// Two thousand bodies: the traces and contacts match the brute force, and the static tree is built once for all
	// the queries.
	FMismatchLog Log{*this};
	const FRandomStream Random(1900);
	FPhysScene Scene;
	constexpr float Extent = 8000.0f;
	BuildRandomScene(Scene, Random, 2000, Extent);
	const FPhysScene& ConstScene = Scene;
	const TArray<int32> Order = IndexOrder(ConstScene);
	CompareTraces(Log, ConstScene, Order, Random, 40, Extent, TEXT("Many"));
	CompareContacts(Log, ConstScene, Order, Random, 40, Extent, TEXT("Many"));
	TestEqual("One tree build", ConstScene.GetBroadphase().GetNumTreeBuilds(), 1);
	TestEqual("Mismatches", Log.Count, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneBroadphaseComponentsTest,
	"System.Engine.PhysScene.Broadphase.ComponentsAddMoveRemove",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPhysSceneBroadphaseComponentsTest::RunTest(const FString& Parameters)
{
	// Component bodies in a world: blocking volumes (static: in the tree), a triangle-mesh static mesh, characters
	// (movable capsules). Volumes move (their bodies leave the tree without a rebuild), characters walk, actors are
	// destroyed (the last body takes the removed one's index) and spawned: each component finds its own body, and the
	// traces match the brute force over the bodies in the order they were added.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	const FRandomStream Random(2000);
	FPhysScene& Scene = World.GetPhysicsScene();
	const FPhysScene& ConstScene = Scene;

	// The bodies' components in the order they were added.
	TArray<UPrimitiveComponent*> Added;
	for (int32 Index = 0; Index < ConstScene.GetBodies().Num(); ++Index)
	{
		Added.Add(ConstScene.GetBodyOwner(Index));
	}
	auto AddBodiesOf = [&](AActor* Actor)
	{
		for (UActorComponent* Component : Actor->GetComponents())
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			if (Primitive != nullptr && ConstScene.FindComponentBody(*Primitive) != INDEX_NONE)
			{
				Added.Add(Primitive);
			}
		}
	};

	TArray<AActor*> Volumes;
	for (int32 Index = 0; Index < 30; ++Index)
	{
		const FVector Location =
			RandomInBox(Random, FVector(-1500.0f, -1500.0f, 0.0f), FVector(1500.0f, 1500.0f, 300.0f));
		const FVector Scale = RandomInBox(Random, FVector(0.2f), FVector(4.0f));
		ABlockingVolume* Volume = World.SpawnActor<ABlockingVolume>(
			ABlockingVolume::StaticClass(), FTransform(FQuat::Identity, Location, Scale));
		AddBodiesOf(Volume);
		Volumes.Add(Volume);
	}
	{
		FMeshData Data;
		const FVector Up(0.0f, 0.0f, 1.0f);
		for (int32 Vertex = 0; Vertex < 30; ++Vertex)
		{
			Data.Vertices.Add(
				FVertex(RandomInBox(Random, FVector(-300.0f, -300.0f, 0.0f), FVector(300.0f, 300.0f, 200.0f)), Up,
					FVector2D(0.0f, 0.0f)));
		}
		for (int32 Tri = 0; Tri < 10; ++Tri)
		{
			Data.Indices.Add(static_cast<uint32>(Tri * 3));
			Data.Indices.Add(static_cast<uint32>(Tri * 3 + 1));
			Data.Indices.Add(static_cast<uint32>(Tri * 3 + 2));
		}
		Data.Submeshes.Add(FMeshSection{0, 30, 0});
		AStaticMeshActor* MeshActor = World.SpawnActor<AStaticMeshActor>();
		UStaticMesh* Mesh = NewObject<UStaticMesh>();
		(void)Mesh->BuildFromMeshData(Data);
		(void)MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		MeshActor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Added.Remove(MeshActor->GetStaticMeshComponent());
		AddBodiesOf(MeshActor);
	}
	TArray<ACharacter*> Characters;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		ACharacter* Character = World.SpawnActor<ACharacter>();
		Character->Reset(RandomInBox(Random, FVector(-1200.0f, -1200.0f, 0.0f), FVector(1200.0f, 1200.0f, 0.0f)));
		AddBodiesOf(Character);
		Characters.Add(Character);
	}

	FMismatchLog Log{*this};
	auto Compare = [&](const FString& What)
	{
		TArray<int32> Order;
		for (const UPrimitiveComponent* Component : Added)
		{
			const int32 BodyIndex = ConstScene.FindComponentBody(*Component);
			Log.Report(BodyIndex != INDEX_NONE && ConstScene.GetBodyOwner(BodyIndex) == Component,
				What + TEXT(": a component lost its body"));
			Order.Add(BodyIndex);
		}
		Log.Report(Order.Num() == ConstScene.GetBodies().Num(), What + TEXT(": bodies without a component"));
		CompareTraces(Log, ConstScene, Order, Random, 40, 1500.0f, What);
	};
	Compare(TEXT("Spawned"));
	const int32 BuildsAfterSpawn = ConstScene.GetBroadphase().GetNumTreeBuilds();

	for (int32 Round = 0; Round < 4; ++Round)
	{
		// Two volumes move; the characters walk.
		for (const int32 Index : {(Round * 2) + 1, Volumes.Num() - 1 - Round})
		{
			(void)Volumes[Index]->SetActorLocation(
				RandomInBox(Random, FVector(-1500.0f, -1500.0f, 0.0f), FVector(1500.0f, 1500.0f, 300.0f)));
			Cast<UPrimitiveComponent>(Volumes[Index]->GetRootComponent())->SendPhysicsTransform();
		}
		for (ACharacter* Character : Characters)
		{
			Character->Reset(Character->GetActorLocation() +
				RandomInBox(Random, FVector(-150.0f, -150.0f, 0.0f), FVector(150.0f, 150.0f, 0.0f)));
		}
		Compare(FString::Printf("Round %d moved", Round));

		// A volume and a character go, a character comes.
		AActor* Gone = Volumes[Round * 3];
		Added.Remove(Cast<UPrimitiveComponent>(Gone->GetRootComponent()));
		(void)Gone->Destroy();
		Volumes.RemoveAt(Round * 3);
		ACharacter* GoneCharacter = Characters[0];
		Added.Remove(GoneCharacter->GetCapsuleComponent());
		(void)GoneCharacter->Destroy();
		Characters.RemoveAt(0);
		ACharacter* NewCharacter = World.SpawnActor<ACharacter>();
		NewCharacter->Reset(RandomInBox(Random, FVector(-1200.0f, -1200.0f, 0.0f), FVector(1200.0f, 1200.0f, 0.0f)));
		AddBodiesOf(NewCharacter);
		Characters.Add(NewCharacter);
		Compare(FString::Printf("Round %d removed", Round));
	}
	TestEqual("Moving and removing bodies does not rebuild the tree", ConstScene.GetBroadphase().GetNumTreeBuilds(),
		BuildsAfterSpawn);
	TestEqual("Mismatches", Log.Count, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneBroadphaseCharacterPairsTest,
	"System.Engine.PhysScene.Broadphase.CharacterPairsMatchBruteForce",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPhysSceneBroadphaseCharacterPairsTest::RunTest(const FString& Parameters)
{
	// A crowd of characters, standing at different heights, some held in tight walk bounds (pushed back far): the world
	// separates them as three passes over every pair in actor order do, round after round.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	const FRandomStream Random(2100);
	TArray<ACharacter*> Characters;
	for (int32 Index = 0; Index < 24; ++Index)
	{
		ACharacter* Character = World.SpawnActor<ACharacter>();
		if (Index % 6 == 5)
		{
			Character->GetCharacterMovement().WalkBounds = 150.0f;
		}
		Characters.Add(Character);
	}
	TArray<ACharacter*> ActorOrder;
	World.ForEach<ACharacter>([&](ACharacter& Character) { ActorOrder.Add(&Character); });

	auto Place = [&](const TArray<FVector>& Locations)
	{
		for (int32 Index = 0; Index < Characters.Num(); ++Index)
		{
			Characters[Index]->Reset(Locations[Index]);
		}
	};
	auto Locations = [&]()
	{
		TArray<FVector> Result;
		for (const ACharacter* Character : Characters)
		{
			Result.Add(Character->GetRootComponent()->GetRelativeLocation());
		}
		return Result;
	};

	int32 Mismatches = 0;
	TArray<FVector> Start;
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		Start.Add(RandomInBox(Random, FVector(-300.0f, -300.0f, 0.0f), FVector(300.0f, 300.0f, 0.0f)));
		Start.Last().Z = Index % 4 == 3 ? Random.FRandRange(0.0f, 250.0f) : 0.0f;
	}
	for (int32 Round = 0; Round < 30; ++Round)
	{
		Place(Start);
		World.ResolveCharacterOverlaps();
		const TArray<FVector> Resolved = Locations();

		Place(Start);
		for (int32 Iter = 0; Iter < 3; ++Iter)
		{
			for (int32 I = 0; I < ActorOrder.Num(); ++I)
			{
				for (int32 J = I + 1; J < ActorOrder.Num(); ++J)
				{
					ActorOrder[I]->ResolvePawnOverlap(*ActorOrder[J]);
				}
			}
		}
		const TArray<FVector> Reference = Locations();
		for (int32 Index = 0; Index < Characters.Num(); ++Index)
		{
			if (Resolved[Index] != Reference[Index])
			{
				++Mismatches;
			}
		}

		// The next round starts from here, shaken into new overlaps.
		Start = Reference;
		for (FVector& Location : Start)
		{
			Location += RandomInBox(Random, FVector(-60.0f, -60.0f, 0.0f), FVector(60.0f, 60.0f, 0.0f));
		}
	}
	TestEqual("Mismatches", Mismatches, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
