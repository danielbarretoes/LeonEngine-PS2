#pragma once

#include "CoreMinimal.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "SpectatorPawnMovement.generated.h"

/**
 * The movement of a spectator (UE: USpectatorPawnMovement): the floating movement, free of collision, which keeps its
 * speed when the game's time is dilated (bIgnoreTimeDilation; Leon has no time dilation yet, so it is kept for UE's
 * shape).
 */
UCLASS()
class ENGINE_API USpectatorPawnMovement : public UFloatingPawnMovement
{
	GENERATED_BODY()

public:
	USpectatorPawnMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Moves at real time whatever the world's time dilation (UE: bIgnoreTimeDilation). */
	UPROPERTY()
	uint8 bIgnoreTimeDilation : 1;
};
