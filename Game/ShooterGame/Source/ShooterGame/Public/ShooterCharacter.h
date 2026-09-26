#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShooterTypes.h"
#include "UObject/SoftObjectPath.h"
#include "ShooterCharacter.generated.h"

class UCameraComponent;
class UInputComponent;
class UShooterCharacterMovement;
class UStaticMeshComponent;

/**
 * ShooterGame's player and bot pawn (UE ShooterGame: AShooterCharacter; UE's FPS template for the camera): a
 * first-person camera at the eyes that follows the control rotation, Counter-Strike's movement
 * (UShooterCharacterMovement), crouching, walking and jumping, and a team-coloured body the other players see.
 *
 * Sizes at CS's 1 unit = 2.54 cm: the capsule is 32 x 72 units (radius 40 cm, half height 91.5 cm), crouched 36 units
 * (half height 46 cm); the eyes are 64 units above the feet (163 cm), 30 crouched (76 cm). The field of view is CS's 90
 * degrees wide at 4:3, 74 degrees high (Leon's cameras keep a vertical field of view).
 *
 * Input (Config/DefaultInput.ini): MoveForward / MoveRight (W A S D), Turn / LookUp (the mouse), Jump (Space), Crouch
 * (Left Ctrl, C: held), Walk (Left Shift: held), Fire (the left button: no weapon until P18).
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AShooterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The eyes: CS's view height above the feet (64 units standing), cm. */
	static constexpr float StandingEyeHeight = 163.0f;
	/** The eyes while crouched (30 units), cm. */
	static constexpr float CrouchingEyeHeight = 76.0f;

	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	void Tick(float DeltaSeconds) override;
	/** Takes the team's body when a controller with a player state takes the pawn. */
	void PossessedBy(AController* NewController) override;

	/** The first-person camera (UE FPS template: FirstPersonCameraComponent). */
	[[nodiscard]] UCameraComponent* GetFirstPersonCameraComponent() const
	{
		return FirstPersonCameraComponent;
	}
	/** The body the other players see (hidden for a local player's own pawn). */
	[[nodiscard]] UStaticMeshComponent* GetBodyMesh() const
	{
		return BodyMesh;
	}
	/** The movement, as the game's class. */
	[[nodiscard]] UShooterCharacterMovement* GetShooterCharacterMovement() const;

	/** The team of the controller's player state, None without one. */
	[[nodiscard]] EShooterTeam GetTeam() const;

	/** Holds the walk key (the walk speed until released). */
	void SetWalking(bool bNewWalking);

	/** The axes (UE FPS template: MoveForward / MoveRight along the view's yaw). */
	void MoveForward(float Value);
	void MoveRight(float Value);

	/** How fast the camera follows the eyes' height when crouching or standing (FMath::FInterpTo speed). */
	UPROPERTY(Config)
	float EyeHeightInterpSpeed = 14.0f;

	/** The bodies of the teams (a placeholder until P18's models): meshes of /Game/Characters. */
	UPROPERTY(Config)
	FSoftObjectPath CTBodyMeshName;

	UPROPERTY(Config)
	FSoftObjectPath TBodyMeshName;

private:
	void OnJumpPressed();
	void OnCrouchPressed();
	void OnCrouchReleased();
	void OnWalkPressed();
	void OnWalkReleased();
	/** No weapon until P18: logs the shot. */
	void OnFirePressed();

	/** Sets the body's mesh for the team, and hides it from its own local player. */
	void UpdateBody(AController* Controller);

	/** UE FPS template: FirstPersonCameraComponent (bUsePawnControlRotation). */
	UPROPERTY()
	UCameraComponent* FirstPersonCameraComponent = nullptr;

	/** The team-coloured body, standing on the feet. */
	UPROPERTY()
	UStaticMeshComponent* BodyMesh = nullptr;
};
