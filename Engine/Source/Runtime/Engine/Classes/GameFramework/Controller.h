#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Controller.generated.h"

class ACharacter;
class APlayerState;

/**
 * Drives a possessed Pawn (UE: AController), an actor the world spawns. A controller that wants one (the player
 * controllers) spawns its APlayerState in PostInitializeComponents; the game mode's PlayerStateClass picks the class.
 */
UCLASS()
class ENGINE_API AController : public AActor
{
	GENERATED_BODY()

public:
	AController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Takes control of InPawn, releasing its previous controller and this controller's previous pawn (UE). */
	void Possess(APawn* InPawn);
	/** Releases the pawn (UE). */
	void UnPossess();

	[[nodiscard]] APawn* GetPawn() const
	{
		return Pawn;
	}
	[[nodiscard]] bool HasPawn() const
	{
		return Pawn != nullptr;
	}
	/** The pawn as a character, or null (UE: GetCharacter). */
	[[nodiscard]] ACharacter* GetCharacter() const;

	/** The rotation the controller aims and looks with (UE: ControlRotation), a UE rotation in degrees. */
	[[nodiscard]] virtual FRotator GetControlRotation() const
	{
		return ControlRotation;
	}
	virtual void SetControlRotation(const FRotator& NewRotation)
	{
		ControlRotation = NewRotation;
	}

	/**
	 * Places the controller and turns its control rotation (UE: SetInitialLocationAndRotation), when the game mode
	 * picks its start spot.
	 */
	virtual void SetInitialLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation);

	/** Turns the control rotation, and the pawn when it follows it (UE: ClientSetRotation; no RPC in Leon). */
	virtual void ClientSetRotation(const FRotator& NewRotation, bool bResetCamera = false);

	/** Where the controller started, chosen by the game mode when it logged in (UE: StartSpot). */
	UPROPERTY()
	TWeakObjectPtr<AActor> StartSpot;

	/** The controller's player state (UE: GetPlayerState<T>), or null. */
	template <class T>
	[[nodiscard]] T* GetPlayerState() const
	{
		return Cast<T>(PlayerState);
	}

	/** Spawns the player state (UE: InitPlayerState): the game mode's PlayerStateClass, APlayerState without one. */
	virtual void InitPlayerState();

	void PostInitializeComponents() override;
	/** Releases the pawn and destroys the player state (UE: Destroyed / CleanupPlayerState). */
	void Destroyed() override;
	/** Releases the pawn when the world ends play. */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Called after the pawn changed (UE: OnPossess / OnUnPossess). */
	virtual void OnPossess(APawn* InPawn);
	virtual void OnUnPossess();

	/** Spawns a player state in PostInitializeComponents (UE: bWantsPlayerState on AAIController). */
	UPROPERTY()
	bool bWantsPlayerState = false;

	/** The player state (UE: PlayerState). */
	UPROPERTY()
	APlayerState* PlayerState = nullptr;

private:
	/** The possessed pawn (UE: Pawn). */
	UPROPERTY()
	APawn* Pawn = nullptr;

	/** UE: ControlRotation. */
	UPROPERTY()
	FRotator ControlRotation = FRotator::ZeroRotator;
};
