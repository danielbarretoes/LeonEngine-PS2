#pragma once

#include "CoreMinimal.h"

/** Shapes a query can sweep (UE: ECollisionShape in CollisionShape.h). */
namespace ECollisionShape
{
	enum Type
	{
		Line,
		Box,
		Sphere,
		Capsule
	};
} // namespace ECollisionShape

/**
 * A query shape (UE: FCollisionShape). Leon's capsule stands on its feet in the legacy Y-up world: the
 * character capsule spans [Feet, Feet + 2 * HalfHeight] and the XZ disc of the given radius (until P7).
 */
struct PHYSICSCORE_API FCollisionShape
{
	ECollisionShape::Type ShapeType;

	union
	{
		struct
		{
			float HalfExtentX;
			float HalfExtentY;
			float HalfExtentZ;
		} Box;

		struct
		{
			float Radius;
		} Sphere;

		struct
		{
			float Radius;
			float HalfHeight;
		} Capsule;
	};

	FCollisionShape()
		: ShapeType(ECollisionShape::Line)
	{
		Box.HalfExtentX = 0.0f;
		Box.HalfExtentY = 0.0f;
		Box.HalfExtentZ = 0.0f;
	}

	bool IsLine() const
	{
		return ShapeType == ECollisionShape::Line;
	}

	bool IsBox() const
	{
		return ShapeType == ECollisionShape::Box;
	}

	bool IsSphere() const
	{
		return ShapeType == ECollisionShape::Sphere;
	}

	bool IsCapsule() const
	{
		return ShapeType == ECollisionShape::Capsule;
	}

	void SetBox(const FVector& HalfExtent)
	{
		ShapeType = ECollisionShape::Box;
		Box.HalfExtentX = HalfExtent.X;
		Box.HalfExtentY = HalfExtent.Y;
		Box.HalfExtentZ = HalfExtent.Z;
	}

	void SetSphere(float Radius)
	{
		ShapeType = ECollisionShape::Sphere;
		Sphere.Radius = Radius;
	}

	void SetCapsule(float Radius, float HalfHeight)
	{
		ShapeType = ECollisionShape::Capsule;
		Capsule.Radius = Radius;
		Capsule.HalfHeight = HalfHeight;
	}

	FVector GetBox() const
	{
		return FVector(Box.HalfExtentX, Box.HalfExtentY, Box.HalfExtentZ);
	}

	float GetSphereRadius() const
	{
		return Sphere.Radius;
	}

	float GetCapsuleRadius() const
	{
		return Capsule.Radius;
	}

	float GetCapsuleHalfHeight() const
	{
		return Capsule.HalfHeight;
	}

	/** Half the length of the capsule's segment, without the end caps (UE: GetCapsuleAxisHalfLength). */
	float GetCapsuleAxisHalfLength() const
	{
		return FMath::Max(Capsule.HalfHeight - Capsule.Radius, 1.e-2f);
	}

	static FCollisionShape MakeBox(const FVector& BoxHalfExtent)
	{
		FCollisionShape Shape;
		Shape.SetBox(BoxHalfExtent);
		return Shape;
	}

	static FCollisionShape MakeSphere(float SphereRadius)
	{
		FCollisionShape Shape;
		Shape.SetSphere(SphereRadius);
		return Shape;
	}

	static FCollisionShape MakeCapsule(float CapsuleRadius, float CapsuleHalfHeight)
	{
		FCollisionShape Shape;
		Shape.SetCapsule(CapsuleRadius, CapsuleHalfHeight);
		return Shape;
	}
};

/** World units (cm) in a metre: masses stay defined in kg per cubic metre. */
inline constexpr float PhysicsCentimetresPerMetre = 100.0f;

/**
 * Edge of the engine's basic shapes (RenderCore MakeCube / MakePlane), in cm: an actor's scale is its size in units of
 * this edge.
 */
inline constexpr float BasicShapeSize = 100.0f;

/** Half extents (cm) of a basic cube scaled by Scale: 0.5 * BasicShapeSize * |Scale|. */
void HalfExtentsFromScale(const FVector& Scale, float& HalfX, float& HalfY, float& HalfZ);

/** Mass (kg) of a box from its half extents in cm: its volume in cubic metres, at least 0.08. */
[[nodiscard]] float MassFromHalfExtents(float HalfX, float HalfY, float HalfZ);

void ClampPositionXZ(FVector& Pos, float Bounds);

[[nodiscard]] bool XzDiscOverlapsAabb(
	float X, float Z, float InRadius, float Cx, float Cz, float Hx, float Hz, float Inflate);

/** Capsule (XZ disc) vs AABB: outward normal (cube to capsule) and penetration. */
[[nodiscard]] bool CapsuleAabbMtv(float Px, float Pz, float InRadius, float Cx, float Cz, float Hx, float Hz,
	FVector2D& OutNormal, float& OutPenetration);

[[nodiscard]] bool AabbOverlapY(float Ay, float Ahy, float By, float Bhy);

/** Separate two XZ AABBs. MoveA / MoveB are MTV shares (static: 0). */
[[nodiscard]] bool SeparateAabbXZ(
	FVector& A, float Ahx, float Ahz, FVector& B, float Bhx, float Bhz, float MoveA, float MoveB);

/**
 * Separate two AABBs on the minimum-penetration axis (X, Y, or Z). OutNormal is the unit MTV direction from B to A
 * when not null. MoveA / MoveB are shares (static: 0).
 */
[[nodiscard]] bool SeparateAabb(FVector& A, const FVector& AHalfExtents, FVector& B, const FVector& BHalfExtents,
	float MoveA, float MoveB, FVector* OutNormal = nullptr);
