#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AnimationAsset.generated.h"

class USkeleton;

/** The base of the animation assets (UE: UAnimationAsset): what animates a skeleton. */
UCLASS(Abstract)
class ENGINE_API UAnimationAsset : public UObject
{
	GENERATED_BODY()

public:
	UAnimationAsset(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: GetSkeleton. */
	[[nodiscard]] USkeleton* GetSkeleton() const
	{
		return Skeleton;
	}
	/** UE: SetSkeleton. */
	void SetSkeleton(USkeleton* NewSkeleton)
	{
		Skeleton = NewSkeleton;
	}

private:
	/** The skeleton the asset animates (UE: Skeleton). */
	UPROPERTY()
	USkeleton* Skeleton = nullptr;
};
