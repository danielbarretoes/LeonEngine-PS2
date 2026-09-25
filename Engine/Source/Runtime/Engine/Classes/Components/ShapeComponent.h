#pragma once

#include "Components/PrimitiveComponent.h"
#include "CoreMinimal.h"
#include "ShapeComponent.generated.h"

/**
 * A primitive whose geometry is a simple collision shape (UE: UShapeComponent). Shapes are not drawn in game; the F2
 * collision debug view draws them.
 */
UCLASS(Abstract)
class ENGINE_API UShapeComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UShapeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Debug color of the shape (UE: ShapeColor). */
	UPROPERTY()
	FColor ShapeColor;

	/** Only drawn when selected (UE: bDrawOnlyIfSelected; kept for the UE shape). */
	UPROPERTY()
	uint8 bDrawOnlyIfSelected : 1;

protected:
	/** The smallest absolute scale of the component: shapes stay round under non-uniform scale (UE: GetShapeScale). */
	[[nodiscard]] float GetShapeScale() const;
};
