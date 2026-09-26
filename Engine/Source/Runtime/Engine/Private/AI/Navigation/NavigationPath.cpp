#include "AI/Navigation/NavigationPath.h"

UNavigationPath::UNavigationPath(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float UNavigationPath::GetPathLength() const
{
	float Length = 0.0f;
	for (int32 Index = 1; Index < PathPoints.Num(); ++Index)
	{
		Length += FVector::Dist(PathPoints[Index - 1], PathPoints[Index]);
	}
	return Length;
}
