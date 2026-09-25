#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class AController;

/** Possessable Actor (Unreal-style Pawn). Character derives from this. */
class ENGINE_API APawn : public AActor
{
public:
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

	/** UnPossess any Controller, then mark pending kill. */
	void Destroy() override;
	/** Also UnPossess when removed via World::Clear. */
	void EndPlay() override;

protected:
	APawn() = default;

private:
	friend class AController;

	void BindController(AController* InController)
	{
		Controller = InController;
	}
	void DetachController();

	AController* Controller = nullptr;
};
