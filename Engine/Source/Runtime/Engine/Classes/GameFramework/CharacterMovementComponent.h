#pragma once

#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "CharacterMovementComponent.generated.h"

class ACharacter;

/** Unreal-like EMovementMode (CMC lite: Walking / Falling only). */
enum class EMovementMode : uint8
{
	None = 0,
	Walking,
	Falling,
};

/** Unreal-like FFindFloorResult (CMC floor query). */
struct ENGINE_API FFindFloorResult
{
	bool bBlockingHit = false;
	bool bWalkableFloor = false;
	/** Distance from capsule feet down to floor ImpactPoint.Z (>= 0 when hit below/at feet). */
	float FloorDist = 0.0f;
	FHitResult Hit{};
};

/**
 * The movement of a character (UE: UCharacterMovementComponent): the tunables of Leon's kinematic capsule movement.
 * ACharacter still runs the movement itself (PerformMovement: walking, falling, step-up, slides) with these values;
 * moving that code here is future work (a documented P12 deviation).
 */
UCLASS()
class ENGINE_API UCharacterMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Unreal MaxWalkSpeed (cm/s). */
	UPROPERTY()
	float MaxWalkSpeed = 450.0f;

	/** Unreal JumpZVelocity (cm/s). */
	UPROPERTY()
	float JumpZVelocity = 700.0f;

	/** World gravity acceleration, cm/s^2 (Leon absolute; UE uses GravityScale × world gravity). */
	UPROPERTY()
	float Gravity = 2400.0f;

	UPROPERTY()
	float TurnSharpness = 16.0f;

	/** Added to the yaw the character turns to when it orients to its movement (degrees). */
	UPROPERTY()
	float ModelYawOffset = 0.0f;

	/** Height of the infinite floor plane (cm). */
	UPROPERTY()
	float FloorZ = 0.0f;

	/** Contact skin (cm). */
	UPROPERTY()
	float Skin = 2.0f;

	/** Unreal MaxStepHeight (cm): geometric step-up + floor probe window. */
	UPROPERTY()
	float MaxStepHeight = 35.0f;

	/** Half size of the square the character may walk in (cm). */
	UPROPERTY()
	float WalkBounds = 1800.0f;

	/** Unitless push strength against dynamic bodies. */
	UPROPERTY()
	float PushStrength = 1.0f;

	UPROPERTY()
	float PushDamping = 6.0f;

	/** Unreal WalkableFloorZ (cos of max walkable slope). Default ~44° (UE). */
	UPROPERTY()
	float WalkableFloorZ = 0.71f;

	/** Unreal AirControl [0,1]: fraction of MaxWalkSpeed applied while Falling. */
	UPROPERTY()
	float AirControl = 0.35f;

	/** Max jumps from ground before landing (1 = normal, 2 = double jump). Projects may raise. */
	UPROPERTY()
	int32 MaxJumpCount = 1;

	/** The character that owns the component (UE: CharacterOwner). */
	[[nodiscard]] ACharacter* GetCharacterOwner() const
	{
		return CharacterOwner;
	}

	[[nodiscard]] float GetMaxSpeed() const override
	{
		return MaxWalkSpeed;
	}

	void PostInitProperties() override;

protected:
	/** UE: CharacterOwner. */
	UPROPERTY(Transient)
	ACharacter* CharacterOwner = nullptr;
};
