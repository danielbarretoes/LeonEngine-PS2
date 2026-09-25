#include "GameFramework/Volume.h"

AVolume::AVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(AActor::DefaultSceneRootName))
{
	bCanEverTick = false;
	// Volumes are invisible in game (UE: ABrush volumes are hidden).
	bHidden = true;
	BrushComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BrushComponent0"));
	BrushComponent->InitBoxExtent(FVector(BrushExtent, BrushExtent, BrushExtent));
	RootComponent = BrushComponent;
}

FBox AVolume::GetBrushBounds() const
{
	const FVector Half = BrushComponent->GetScaledBoxExtent();
	const FVector Center = GetActorLocation();
	return FBox(Center - Half, Center + Half);
}

bool AVolume::EncompassesPoint(const FVector& Point, float SphereRadius) const
{
	const FBox Bounds = GetBrushBounds().ExpandBy(SphereRadius);
	return Point.X >= Bounds.Min.X && Point.X <= Bounds.Max.X && Point.Y >= Bounds.Min.Y && Point.Y <= Bounds.Max.Y &&
		Point.Z >= Bounds.Min.Z && Point.Z <= Bounds.Max.Z;
}
