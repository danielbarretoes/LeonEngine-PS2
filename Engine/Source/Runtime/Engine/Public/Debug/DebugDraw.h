#pragma once

#include "CoreMinimal.h"

/**
 * A batch of coloured world-space lines for 3D debug (AABBs, light frustum, collision, navigation, the axes gizmo).
 * Colours are linear RGB. It only collects the lines: the renderer draws a batch it is given (the world's
 * UWorld::LineBatcher after the scene, its own bounds and gizmo batches), which UE does with ULineBatchComponent.
 */
class ENGINE_API FDebugDraw
{
public:
	struct FLineVertex
	{
		FVector Position = FVector::ZeroVector;
		FVector Color = FVector::ZeroVector;
	};

	void Clear();
	void AddLine(const FVector& A, const FVector& B, const FLinearColor& InColor);
	/** Shaft + V-shaped head for a world-space direction vector (head sizes in cm). */
	void AddArrow(const FVector& From, const FVector& To, const FLinearColor& InColor, float HeadLength = 28.0f,
		float HeadWidth = 14.0f);
	void AddAabb(const FVector& WorldMin, const FVector& WorldMax, const FLinearColor& InColor);
	/** World axes from Origin, X red, Y green, Z blue (UE's DrawDebugCoordinateSystem colours); Length in cm. */
	void AddAxes(const FVector& Origin, float Length = 100.0f);
	/** The transform's unit axes (rotation only, scale ignored) from its location, coloured like AddAxes. */
	void AddAxes(const FTransform& Transform, float Length = 100.0f);
	/**
	 * Orientation gizmo: the world axes seen through View's rotation (translation ignored), projected
	 * orthographically into the GL clip space of a square viewport: view x right and y up, scaled by Extent, z = 0.
	 * The axis farthest from the viewer is added first so the nearest one ends on top; flush with the identity and
	 * no depth test.
	 */
	void AddViewAxes(const FMatrix& View, float Extent = 0.8f);
	/** GL clip-space cube (+-1) transformed by inverse(LightSpace): the world-space ortho frustum. */
	void AddLightFrustum(const FMatrix& LightSpace, const FLinearColor& InColor);

	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.Num() == 0;
	}
	/** The batch as line vertex pairs (tests). */
	[[nodiscard]] const TArray<FLineVertex>& GetVertices() const
	{
		return Vertices;
	}

private:
	TArray<FLineVertex> Vertices;
};
