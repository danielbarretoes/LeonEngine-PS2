#pragma once

#include "CoreMinimal.h"
#include "NavigationTypes.generated.h"

/**
 * What a moving agent can do and how big it is (UE: FNavAgentProperties, with FMovementProperties' flags folded in).
 * A pawn's movement component keeps one (UPawnMovementComponent::NavAgentProps); the character movement reads
 * bCanCrouch (UE: CanEverCrouch).
 */
USTRUCT()
struct ENGINE_API FNavAgentProperties
{
	GENERATED_BODY()

	/** The agent may crouch (UE: bCanCrouch; off by default, as UE's). */
	UPROPERTY()
	uint8 bCanCrouch : 1;

	/** The agent may jump (UE: bCanJump). */
	UPROPERTY()
	uint8 bCanJump : 1;

	/** The agent may walk (UE: bCanWalk). */
	UPROPERTY()
	uint8 bCanWalk : 1;

	/** The agent may swim (UE: bCanSwim; Leon has no water). */
	UPROPERTY()
	uint8 bCanSwim : 1;

	/** The agent may fly (UE: bCanFly). */
	UPROPERTY()
	uint8 bCanFly : 1;

	/** The agent's radius, cm; negative when unset (UE: AgentRadius). */
	UPROPERTY()
	float AgentRadius = -1.0f;

	/** The agent's height, cm; negative when unset (UE: AgentHeight). */
	UPROPERTY()
	float AgentHeight = -1.0f;

	FNavAgentProperties()
		: bCanCrouch(0)
		, bCanJump(1)
		, bCanWalk(1)
		, bCanSwim(0)
		, bCanFly(0)
	{
	}
};
