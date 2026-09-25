#pragma once

#include "Components/ShapeComponent.h"
#include "CoreMinimal.h"
#include "BoxComponent.generated.h"

/** An oriented box (UE: UBoxComponent). */
UCLASS()
class ENGINE_API UBoxComponent : public UShapeComponent
{
	GENERATED_BODY()

public:
	UBoxComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Half extents, cm (UE: SetBoxExtent; InitBoxExtent before registration). */
	void SetBoxExtent(const FVector& InBoxExtent)
	{
		BoxExtent = InBoxExtent;
	}
	void InitBoxExtent(const FVector& InBoxExtent)
	{
		BoxExtent = InBoxExtent;
	}
	[[nodiscard]] FVector GetUnscaledBoxExtent() const
	{
		return BoxExtent;
	}
	[[nodiscard]] FVector GetScaledBoxExtent() const;

	[[nodiscard]] FCollisionShape GetCollisionShape(float Inflation = 0.0f) const override;

private:
	/** Half extents, cm (UE: BoxExtent). */
	UPROPERTY()
	FVector BoxExtent = FVector(32.0f, 32.0f, 32.0f);
};
