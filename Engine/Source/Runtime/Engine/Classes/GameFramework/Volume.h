#pragma once

#include "Components/BoxComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Volume.generated.h"

/**
 * A region of a level (UE: AVolume, an ABrush). Plan decision D16: Leon's volumes are boxes, not BSP brushes. The
 * root, BrushComponent, is a UBoxComponent whose unscaled extent is BrushExtent (a 100 cm cube, the size of the legacy
 * `.llev` volumes); the actor scale sizes it. Collision is off unless a subclass or the level reader turns it on.
 */
UCLASS(Abstract)
class ENGINE_API AVolume : public AActor
{
	GENERATED_BODY()

public:
	AVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Half size of the unscaled brush box, cm: a 100 cm cube. */
	static constexpr float BrushExtent = 50.0f;

	/** The brush (UE: ABrush::GetBrushComponent, a UBrushComponent there). */
	[[nodiscard]] UBoxComponent* GetBrushComponent() const
	{
		return BrushComponent;
	}

	/**
	 * True when Point lies in the volume (UE: EncompassesPoint). Leon tests the axis-aligned box around the actor
	 * location with the scaled brush extent, ignoring the rotation, as the legacy volumes did; SphereRadius grows it.
	 */
	[[nodiscard]] bool EncompassesPoint(const FVector& Point, float SphereRadius = 0.0f) const;

	/** The scaled brush box's world bounds, ignoring the rotation (see EncompassesPoint). */
	[[nodiscard]] FBox GetBrushBounds() const;

private:
	/** The root (UE: BrushComponent). */
	UPROPERTY()
	UBoxComponent* BrushComponent = nullptr;
};
