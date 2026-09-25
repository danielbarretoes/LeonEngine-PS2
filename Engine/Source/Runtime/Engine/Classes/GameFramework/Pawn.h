#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pawn.generated.h"

class AController;
class UInputComponent;
class UPawnMovementComponent;

/**
 * Possessable Actor (Unreal-style Pawn). Character derives from this.
 *
 * When a local player controller possesses it, PawnClientRestart makes its input component and calls
 * SetupPlayerInputComponent, where a pawn binds its actions and axes (UE). Movement input accumulates in the pawn's
 * input vector (AddMovementInput) until its movement component consumes it.
 */
UCLASS()
class ENGINE_API APawn : public AActor
{
	GENERATED_BODY()

public:
	APawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	[[nodiscard]] AController* GetController() const
	{
		return Controller;
	}
	[[nodiscard]] bool IsPossessed() const
	{
		return Controller != nullptr;
	}

	/** The controller's control rotation, or zero without a controller (UE: APawn::GetControlRotation). */
	[[nodiscard]] FRotator GetControlRotation() const;
	/** Where the pawn looks: the control rotation when possessed, else the actor rotation (UE: GetViewRotation). */
	[[nodiscard]] FRotator GetViewRotation() const;

	/**
	 * Look input in degrees, forwarded to a possessing APlayerController (UE: AddControllerYawInput /
	 * AddControllerPitchInput): a positive yaw turns right, a positive pitch looks up. Ignored without one.
	 */
	void AddControllerYawInput(float Val);
	void AddControllerPitchInput(float Val);

	/**
	 * Binds the pawn's input (UE: SetupPlayerInputComponent), from PawnClientRestart when a local player possesses it.
	 */
	virtual void SetupPlayerInputComponent(UInputComponent* /*PlayerInputComponent*/)
	{
	}

	/**
	 * A local player's controller took the pawn (UE: PawnClientRestart): the pawn makes its input component
	 * (CreatePlayerInputComponent) and binds it (SetupPlayerInputComponent), once.
	 */
	virtual void PawnClientRestart();

	/** The pawn's input component, registered (UE: CreatePlayerInputComponent). */
	virtual UInputComponent* CreatePlayerInputComponent();
	/** Destroys the input component (UE: DestroyPlayerInputComponent). */
	virtual void DestroyPlayerInputComponent();

	/** A controller took the pawn (UE: PossessedBy). */
	virtual void PossessedBy(AController* NewController);
	/** The controller left the pawn (UE: UnPossessed): the input component and the input vector go. */
	virtual void UnPossessed();

	/** The pawn's first UPawnMovementComponent, or null (UE: GetMovementComponent). */
	[[nodiscard]] virtual UPawnMovementComponent* GetMovementComponent() const;

	/**
	 * Movement input along a world direction, scaled (UE: AddMovementInput): the movement component consumes it. A
	 * pawn without one keeps it in its input vector.
	 */
	void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false);

	/** The input vector not consumed yet (UE: GetPendingMovementInputVector). */
	[[nodiscard]] FVector GetPendingMovementInputVector() const
	{
		return ControlInputVector;
	}
	/** The input vector consumed last (UE: GetLastMovementInputVector). */
	[[nodiscard]] FVector GetLastMovementInputVector() const
	{
		return LastControlInputVector;
	}
	/** Returns the input vector and clears it (UE: ConsumeMovementInputVector). */
	FVector ConsumeMovementInputVector();

	/** Adds to the input vector (UE: Internal_AddMovementInput, for the movement components). */
	void Internal_AddMovementInput(FVector WorldAccel, bool bForce = false);
	/** Returns the input vector and clears it (UE: Internal_ConsumeMovementInputVector). */
	FVector Internal_ConsumeMovementInputVector();

	/**
	 * Turns the pawn with the control rotation, on the axes it follows (UE: FaceRotation, from the player controller's
	 * UpdateRotation): by default only the yaw.
	 */
	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime = 0.0f);

	/** The eyes: the actor location raised by BaseEyeHeight (UE: GetPawnViewLocation). */
	[[nodiscard]] virtual FVector GetPawnViewLocation() const;

	/** The view point: the eyes and the view rotation (UE: GetActorEyesViewPoint). */
	void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	/** The pawn follows the control rotation's pitch / yaw / roll (UE). */
	UPROPERTY()
	uint8 bUseControllerRotationPitch : 1;

	UPROPERTY()
	uint8 bUseControllerRotationYaw : 1;

	UPROPERTY()
	uint8 bUseControllerRotationRoll : 1;

	/** The eyes above the actor location, cm (UE: BaseEyeHeight). */
	UPROPERTY()
	float BaseEyeHeight = 64.0f;

	/** Releases the Controller before the actor is destroyed (UE: APawn::Destroyed). */
	void Destroyed() override;
	/** Also releases the Controller when the world ends play (World::Clear, DestroyWorld). */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class AController;

	void BindController(AController* InController)
	{
		Controller = InController;
	}
	void DetachController();

	/** The possessing controller (UE: Controller). */
	UPROPERTY()
	AController* Controller = nullptr;

	/** Movement input not consumed yet (UE: ControlInputVector). */
	FVector ControlInputVector = FVector::ZeroVector;

	/** The input vector consumed last (UE: LastControlInputVector). */
	FVector LastControlInputVector = FVector::ZeroVector;
};
