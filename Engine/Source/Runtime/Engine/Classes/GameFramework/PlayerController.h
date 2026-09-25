#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "PlayerController.generated.h"

class ACharacter;
class UGameEngine;

/**
 * Drives a possessed Character from player input (UE: APlayerController). It spawns its APlayerState when spawned
 * (bWantsPlayerState).
 *
 * Until P13 (UPlayerInput per player, UInputComponent bindings, APlayerCameraManager) input comes from the engine's
 * UPlayerInput through TickInput and the view through UpdateCamera, both called by the game mode.
 */
UCLASS()
class ENGINE_API APlayerController : public AController
{
	GENERATED_BODY()

public:
	APlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	using AController::Possess;
	void Possess(ACharacter* Character);

	[[nodiscard]] ACharacter* GetCharacter() const
	{
		return AController::GetCharacter();
	}
	[[nodiscard]] bool HasCharacter() const
	{
		return GetCharacter() != nullptr;
	}

	/**
	 * Look input in degrees (UE: AddYawInput / AddPitchInput with an input scale of 1), applied to the control rotation
	 * at once: a positive yaw turns right, a positive pitch looks up. The pitch stays in [ViewPitchMin, ViewPitchMax].
	 */
	void AddYawInput(float Val);
	void AddPitchInput(float Val);

	/** Pitch limits of the control rotation in degrees (UE: APlayerCameraManager::ViewPitchMin / ViewPitchMax). */
	UPROPERTY()
	float ViewPitchMin = -89.0f;

	UPROPERTY()
	float ViewPitchMax = 89.0f;

	/**
	 * Apply input to the possessed Character. Games override. Returns wish direction for
	 * debug HUD; default is a no-op.
	 */
	virtual FVector TickInput(UGameEngine& Engine);

	/** Unreal-like: drive view from possessed pawn SpringArm (games override). */
	virtual void UpdateCamera(UGameEngine& Engine, float DeltaTime);
};
