#pragma once

#include "Animation/BlendSpaceBase.h"
#include "AnimationRuntime.h"
#include "CoreMinimal.h"
#include "BlendSpace.generated.h"

/**
 * A blend space on two axes (UE: UBlendSpace): locomotion by speed (X) and direction (Y), for example. Its samples are
 * triangulated on the axes normalized to their ranges (FBlendSpaceTriangulation: Delaunay, rebuilt by ResampleData
 * after a change and after loading, not saved), and an input takes the barycentric weights of its triangle, or the
 * nearest edge's outside them (UE bakes the triangles' weights into a grid, GridNum; Leon interpolates the triangles).
 */
UCLASS()
class ENGINE_API UBlendSpace : public UBlendSpaceBase
{
	GENERATED_BODY()

public:
	UBlendSpace(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** At most three samples: the triangle around the input, or the edge nearest it. */
	void GetSamplesFromBlendInput(const FVector& BlendInput, FBlendSampleDataArray& OutSampleDataList) const override;

	/** Triangulates the samples again. */
	void ResampleData() override;

	/** The triangles of the samples (indices into SampleData). */
	[[nodiscard]] const TArray<FBlendSpaceTriangle>& GetTriangles() const
	{
		return Triangles;
	}

private:
	/** A sample's position with each axis mapped to 0..1 (the triangulation's space). */
	[[nodiscard]] FVector2D NormalizeInput(const FVector& Value) const;

	TArray<FVector2D> NormalizedSamples;
	TArray<FBlendSpaceTriangle> Triangles;
};
