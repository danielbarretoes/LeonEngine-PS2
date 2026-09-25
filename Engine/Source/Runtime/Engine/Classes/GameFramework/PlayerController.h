#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "PlayerController.generated.h"

class ACharacter;
class AHUD;
class APlayerCameraManager;
class UInputComponent;
class UPlayer;
class UPlayerInput;

/**
 * A player's controller (UE: APlayerController). It spawns its APlayerState when spawned (bWantsPlayerState).
 *
 * When a local player logs in with it (SetPlayer), it builds its input (InitInputSystem: a UPlayerInput with the input
 * settings' mappings and its own UInputComponent) and spawns its camera manager (PlayerCameraManagerClass); the game
 * mode gives it its HUD (ClientSetHUD). Possessing a pawn restarts the pawn's input (APawn::PawnClientRestart).
 *
 * Each tick of a local controller (PlayerTick) processes the input (ProcessPlayerInput: the input stack of the pawn's
 * component, its own and the pushed ones, top first) and turns the pawn with the control rotation (UpdateRotation);
 * the world then updates the camera (UpdateCameraManager). The viewport client feeds it key events and mouse samples
 * (InputKey, InputAxis).
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

	/** The player this controller plays for (UE: Player); null for a controller that no player logged in with. */
	UPROPERTY(Transient)
	UPlayer* Player = nullptr;

	/** The player's input: key state and mappings (UE: PlayerInput); a local controller's only. */
	UPROPERTY(Transient)
	UPlayerInput* PlayerInput = nullptr;

	/** The HUD the game mode gave it (UE: MyHUD). */
	UPROPERTY(Transient)
	AHUD* MyHUD = nullptr;

	/** The camera (UE: PlayerCameraManager); a local controller's only. */
	UPROPERTY(Transient)
	APlayerCameraManager* PlayerCameraManager = nullptr;

	/** The camera manager's class (UE: PlayerCameraManagerClass). */
	UPROPERTY()
	TSubclassOf<APlayerCameraManager> PlayerCameraManagerClass;

	/** Takes the player that logged in with this controller and builds its input and camera (UE: SetPlayer). */
	virtual void SetPlayer(UPlayer* InPlayer);

	/** Whether a player at this machine plays through this controller (UE: IsLocalController). */
	[[nodiscard]] bool IsLocalController() const;
	[[nodiscard]] bool IsLocalPlayerController() const
	{
		return IsLocalController();
	}

	/** Creates the player input and the input component (UE: InitInputSystem). */
	virtual void InitInputSystem();

	/** Creates the controller's input component; games bind their controller actions here (UE: SetupInputComponent). */
	virtual void SetupInputComponent();

	/** Spawns the camera manager (UE: SpawnPlayerCameraManager). */
	virtual void SpawnPlayerCameraManager();

	/** Replaces the HUD with one of NewHUDClass (UE: ClientSetHUD; no RPC in Leon). */
	virtual void ClientSetHUD(TSubclassOf<AHUD> NewHUDClass);

	/** A key event from the viewport client (UE: InputKey). */
	virtual bool InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad);

	/** An axis sample from the viewport client (UE: InputAxis). */
	virtual bool InputAxis(FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad);

	/** Puts a component on top of the input stack (UE: PushInputComponent). */
	void PushInputComponent(UInputComponent* Input);
	/** Takes it off (UE: PopInputComponent). */
	bool PopInputComponent(UInputComponent* Input);

	/** A local controller's frame: input, then rotation (UE: PlayerTick). */
	virtual void PlayerTick(float DeltaTime);

	/** UE: TickPlayerInput. */
	virtual void TickPlayerInput(const float DeltaSeconds, const bool bGamePaused);

	/** Runs the input stack through the player input (UE: ProcessPlayerInput). */
	virtual void ProcessPlayerInput(const float DeltaTime, const bool bGamePaused);

	/**
	 * The input stack, bottom first (UE: BuildInputStack): the pawn's input component, then the controller's, then the
	 * pushed components.
	 */
	virtual void BuildInputStack(TArray<UInputComponent*>& InputStack);

	/**
	 * Turns the pawn with the control rotation (UE: UpdateRotation; Leon's look input already turned the control
	 * rotation, so only the pawn's FaceRotation is left).
	 */
	virtual void UpdateRotation(float DeltaTime);

	/** Updates the camera, after the world ticked its actors (UE: UpdateCameraManager). */
	virtual void UpdateCameraManager(float DeltaSeconds);

	/** The camera's view point, else the controller's (UE: GetPlayerViewPoint). */
	virtual void GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** The pawn restarts with a local player: its input (UE: ClientRestart; no RPC in Leon). */
	virtual void ClientRestart(APawn* NewPawn);

	/** Sets the camera's field of view, 0 to unlock it (UE: `FOV <degrees>`, vertical in Leon). */
	UFUNCTION(Exec)
	virtual void FOV(float NewFOV);

	/** A local controller ticks its player (UE: TickActor → PlayerTick). */
	void Tick(float DeltaSeconds) override;

	/** The HUD and the camera go with the controller (UE: Destroyed). */
	void Destroyed() override;

	/** Pitch limits of the control rotation in degrees (UE: APlayerCameraManager::ViewPitchMin / ViewPitchMax). */
	UPROPERTY()
	float ViewPitchMin = -89.0f;

	UPROPERTY()
	float ViewPitchMax = 89.0f;

protected:
	/** A local controller restarts the pawn it takes (UE: OnPossess → ClientRestart). */
	void OnPossess(APawn* InPawn) override;

private:
	/** Components pushed on the input stack (UE: CurrentInputStack). */
	UPROPERTY(Transient)
	TArray<UInputComponent*> CurrentInputStack;
};
