#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pawn.generated.h"

class AController;

/** Possessable Actor (Unreal-style Pawn). Character derives from this. */
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
};
