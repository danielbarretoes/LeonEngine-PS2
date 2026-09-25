#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DefaultPawn.generated.h"

class UFloatingPawnMovement;
class UInputComponent;
class USphereComponent;

/**
 * The game mode's default pawn (UE: ADefaultPawn, AGameModeBase::DefaultPawnClass): a flying view with a sphere root
 * and a UFloatingPawnMovement. A local player moves it with the input settings' axes: MoveForward along the view,
 * MoveRight, MoveUp along world Z, and turns with Turn and LookUp (the mouse, TurnRate / LookUpRate for sticks).
 *
 * Leon: the pawn binds the config's axis names (BaseInput.ini: MoveForward, MoveRight, MoveUp, Turn, LookUp), where UE
 * binds engine-defined DefaultPawn_* mappings; it flies at 800 cm/s (the legacy fly camera's speed); it has no sphere
 * mesh; and its look axes are bound before its move axes, so a frame moves along the view the mouse just turned (the
 * legacy fly camera's order).
 */
UCLASS()
class ENGINE_API ADefaultPawn : public APawn
{
	GENERATED_BODY()

public:
	ADefaultPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Names of the default subobjects (UE). */
	static const FName MovementComponentName;
	static const FName CollisionComponentName;

	/** Stick turn rates, degrees per second at full deflection (UE: BaseTurnRate / BaseLookUpRate). */
	UPROPERTY()
	float BaseTurnRate = 45.0f;

	UPROPERTY()
	float BaseLookUpRate = 45.0f;

	/** Binds the movement and look axes (UE: bAddDefaultMovementBindings). */
	UPROPERTY()
	uint8 bAddDefaultMovementBindings : 1;

	// APawn
	UPawnMovementComponent* GetMovementComponent() const override;
	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Along the control rotation's forward, pitch included (UE: MoveForward). */
	virtual void MoveForward(float Val);
	/** Along the control rotation's right (UE: MoveRight). */
	virtual void MoveRight(float Val);
	/** Along world Z (UE: MoveUp_World). */
	virtual void MoveUp_World(float Val);
	/** Stick turn, scaled by the rate and the frame time (UE: TurnAtRate / LookUpAtRate). */
	virtual void TurnAtRate(float Rate);
	virtual void LookUpAtRate(float Rate);

	[[nodiscard]] USphereComponent* GetCollisionComponent() const
	{
		return CollisionComponent;
	}

private:
	/** The movement (UE: MovementComponent, a UFloatingPawnMovement). */
	UPROPERTY()
	UFloatingPawnMovement* MovementComponent = nullptr;

	/** The root, radius 35 cm, no collision (UE: CollisionComponent). */
	UPROPERTY()
	USphereComponent* CollisionComponent = nullptr;
};
