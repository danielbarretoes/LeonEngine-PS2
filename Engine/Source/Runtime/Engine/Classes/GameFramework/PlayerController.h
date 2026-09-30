#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "PlayerController.generated.h"

class ACharacter;
class AHUD;
class APlayerCameraManager;
class ASpectatorPawn;
class UInputComponent;
class UGameViewportClient;
class UPlayer;
class UPlayerInput;

/**
 * How the player's input goes to the game and the UI (UE: FInputModeDataBase; APlayerController::SetInputMode). Leon's
 * HUD widgets always see the keys first (AHUD::InputKey); a mode sets how the viewport treats the mouse.
 */
struct ENGINE_API FInputModeDataBase
{
	virtual ~FInputModeDataBase() = default;

protected:
	/** Applies the mode to the viewport (UE: ApplyInputMode, with Slate's operations there). */
	virtual void ApplyInputMode(UGameViewportClient& GameViewportClient) const = 0;

	friend class APlayerController;
};

/** Only the UI: the cursor is free to point at the widgets, and the mouse does not look (UE: FInputModeUIOnly). */
struct ENGINE_API FInputModeUIOnly : public FInputModeDataBase
{
protected:
	void ApplyInputMode(UGameViewportClient& GameViewportClient) const override;
};

/** The game and the UI: the cursor is free, and dragging with a button down looks (UE: FInputModeGameAndUI). */
struct ENGINE_API FInputModeGameAndUI : public FInputModeDataBase
{
protected:
	void ApplyInputMode(UGameViewportClient& GameViewportClient) const override;
};

/** Only the game: the viewport captures the mouse, which looks (UE: FInputModeGameOnly). */
struct ENGINE_API FInputModeGameOnly : public FInputModeDataBase
{
protected:
	void ApplyInputMode(UGameViewportClient& GameViewportClient) const override;
};

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
 *
 * Pause (UE's): SetPause asks the game mode to pause or go on (AGameModeBase::SetPause / ClearPause; the `Pause`
 * command toggles it). A player controller ticks while the world is paused (bTickEvenWhenPaused), but then only
 * processes its input: the bindings that run when paused (FInputBinding::bExecuteWhenPaused) and its HUD's widgets,
 * which see the keys first anyway. SetInputMode says whether the mouse looks or points at the UI.
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

	/**
	 * Ends the old state and begins the new one (UE): leaving NAME_Spectating destroys the spectator pawn, entering it
	 * begins spectating (BeginSpectatingState). Possessing a pawn other than the spectator enters NAME_Playing.
	 */
	void ChangeState(FName NewState) override;

	/**
	 * Enters the spectating state (UE: BeginSpectatingState): releases the pawn and flies a new spectator pawn
	 * (SpawnSpectatorPawn) from where the player looked.
	 */
	virtual void BeginSpectatingState();
	/** Leaves it: the spectator pawn goes (UE: EndSpectatingState). */
	virtual void EndSpectatingState();

	/**
	 * Spawns the game mode's SpectatorClass (ASpectatorPawn without a game mode) at the player's view point, facing the
	 * control rotation (UE: SpawnSpectatorPawn). Transient; null outside a world.
	 */
	virtual ASpectatorPawn* SpawnSpectatorPawn();
	/** Destroys the spectator pawn (UE: DestroySpectatorPawn). */
	virtual void DestroySpectatorPawn();
	/**
	 * Takes the pawn to spectate with while spectating (UE: SetSpectatorPawn); Leon possesses it (the class comment of
	 * ASpectatorPawn).
	 */
	void SetSpectatorPawn(ASpectatorPawn* NewSpectatorPawn);
	/** The spectator pawn while spectating, else null (UE: GetSpectatorPawn). */
	[[nodiscard]] ASpectatorPawn* GetSpectatorPawn() const
	{
		return SpectatorPawn;
	}

	/** Sets the camera's field of view, 0 to unlock it (UE: `FOV <degrees>`, vertical in Leon). */
	UFUNCTION(Exec)
	virtual void FOV(float NewFOV);

	/** A local controller ticks its player (UE: TickActor → PlayerTick). */
	void Tick(float DeltaSeconds) override;
	/**
	 * A paused world's step only processes the input (TickPlayerInput with the game paused) and the force feedback that
	 * plays while paused, unless bShouldPerformFullTickWhenPaused (UE: TickActor).
	 */
	void TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	/** The whole tick runs while the world is paused too (UE: bShouldPerformFullTickWhenPaused). */
	uint8 bShouldPerformFullTickWhenPaused : 1;

	/**
	 * Pauses the game or lets it go on (UE: SetPause): the game mode's SetPause / ClearPause. True when the state
	 * changed (a paused game asked to pause again, or one playing asked to go on, does nothing).
	 */
	virtual bool SetPause(bool bPause, FCanUnpause CanUnpauseDelegate = FCanUnpause());
	/** The game is paused (UE: IsPaused). */
	[[nodiscard]] virtual bool IsPaused() const;
	/** This player paused the game, so it may let it go on (UE: CanUnpause). */
	[[nodiscard]] virtual bool CanUnpause();
	/** Pauses the game, or lets it go on when paused (UE: the Pause command). */
	UFUNCTION(Exec)
	virtual void Pause();

	/** How the mouse works for the local player's viewport: the UI, the game or both (UE: SetInputMode). */
	void SetInputMode(const FInputModeDataBase& InData);

	/** The HUD and the camera go with the controller (UE: Destroyed). */
	void Destroyed() override;

	/**
	 * Plays a force feedback effect on the player's controller (UE; Docs/PLANS/ps2-shipping.md N24): the effects that
	 * play add up (the stronger on each motor) each tick (ProcessForceFeedbackAndHaptics) and go to the local player's
	 * controller id, the DualShock's motors on the PS2. A controller without a local player (a bot's) plays nothing.
	 */
	virtual void ClientPlayForceFeedback(
		UForceFeedbackEffect* ForceFeedbackEffect, FForceFeedbackParameters Params = FForceFeedbackParameters());

	/** Stops an effect, the ones with a tag, or everything with null and NAME_None (UE). */
	virtual void ClientStopForceFeedback(UForceFeedbackEffect* ForceFeedbackEffect, FName Tag);

	/** Advances the effects and sends their motors to the controller (UE); PlayerTick calls it. */
	virtual void ProcessForceFeedbackAndHaptics(const float DeltaTime, const bool bGamePaused);

	/** Whether the controller vibrates at all (UE). */
	UPROPERTY()
	bool bForceFeedbackEnabled = true;

	/** The effects' strength, 0 to 1 (UE). */
	UPROPERTY()
	float ForceFeedbackScale = 1.0f;

	/** The motors as the last tick sent them (UE). */
	FForceFeedbackValues ForceFeedbackValues;

	/** The effects playing (UE). */
	UPROPERTY(Transient)
	TArray<FActiveForceFeedbackEffect> ActiveForceFeedbackEffects;

	/** Pitch limits of the control rotation in degrees (UE: APlayerCameraManager::ViewPitchMin / ViewPitchMax). */
	UPROPERTY()
	float ViewPitchMin = -89.0f;

	UPROPERTY()
	float ViewPitchMax = 89.0f;

protected:
	/** A local controller restarts the pawn it takes (UE: OnPossess → ClientRestart). */
	void OnPossess(APawn* InPawn) override;

private:
	/** Gives the local player's controller its motors. */
	void SendForceFeedback(const FForceFeedbackValues& Values) const;

	/** Components pushed on the input stack (UE: CurrentInputStack). */
	UPROPERTY(Transient)
	TArray<UInputComponent*> CurrentInputStack;

	/** The pawn of the spectating state (UE: SpectatorPawn). */
	UPROPERTY(Transient)
	ASpectatorPawn* SpectatorPawn = nullptr;
};
