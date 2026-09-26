#include "ShooterCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
#include "ShooterCharacterMovement.h"
#include "ShooterGame.h"
#include "ShooterPlayerState.h"

namespace
{

	/** CS's hull at 2.54 cm a unit: 32 units wide, 72 tall (cm). */
	constexpr float ShooterCapsuleRadius = 40.0f;
	constexpr float ShooterCapsuleHalfHeight = 91.5f;

	/** The vertical field of view of CS's 90 degrees at 4:3 (degrees). */
	constexpr float ShooterFieldOfView = 74.0f;

} // namespace

AShooterCharacter::AShooterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShooterCharacterMovement>(
		  ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(ShooterCapsuleRadius, ShooterCapsuleHalfHeight);
	BaseEyeHeight = StandingEyeHeight;
	CrouchedEyeHeight = CrouchingEyeHeight;

	// UE FPS template: the pawn turns with the controller's yaw; the camera takes the pitch.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bOrientRotationToMovement = false;

	// UE FPS template: FirstPersonCameraComponent on the capsule at the eyes, following the control rotation. Leon's
	// capsule stands on the feet, so the eyes are BaseEyeHeight above the component.
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->RelativeLocation = FVector(0.0f, 0.0f, StandingEyeHeight);
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->SetFieldOfView(ShooterFieldOfView);

	// The body the others see: a team mesh standing on the feet (UpdateBody). No collision: the capsule collides.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
}

UShooterCharacterMovement* AShooterCharacter::GetShooterCharacterMovement() const
{
	return Cast<UShooterCharacterMovement>(const_cast<UCharacterMovementComponent*>(&GetCharacterMovement()));
}

EShooterTeam AShooterCharacter::GetTeam() const
{
	const AController* OwningController = GetController();
	const AShooterPlayerState* State =
		OwningController != nullptr ? OwningController->GetPlayerState<AShooterPlayerState>() : nullptr;
	return State != nullptr ? State->GetTeam() : EShooterTeam::None;
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent != nullptr);
	// Look first, then move: a frame moves along the view the mouse just turned.
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AShooterCharacter::AddControllerYawInput);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AShooterCharacter::AddControllerPitchInput);
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AShooterCharacter::OnJumpPressed);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &AShooterCharacter::OnCrouchPressed);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Released, this, &AShooterCharacter::OnCrouchReleased);
	PlayerInputComponent->BindAction(TEXT("Walk"), IE_Pressed, this, &AShooterCharacter::OnWalkPressed);
	PlayerInputComponent->BindAction(TEXT("Walk"), IE_Released, this, &AShooterCharacter::OnWalkReleased);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AShooterCharacter::OnFirePressed);
}

void AShooterCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// Along the view's yaw, level (the pitch does not slow the walk). ACharacter's one-argument AddMovementInput
		// is Leon's wish setter; the pawn's input vector is APawn's (UE's).
		const FRotator YawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
		APawn::AddMovementInput(YawRotation.Vector(), Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		const FRotator YawRotation(0.0f, GetControlRotation().Yaw + 90.0f, 0.0f);
		APawn::AddMovementInput(YawRotation.Vector(), Value);
	}
}

void AShooterCharacter::OnJumpPressed()
{
	Jump();
}

void AShooterCharacter::OnCrouchPressed()
{
	Crouch();
}

void AShooterCharacter::OnCrouchReleased()
{
	UnCrouch();
}

void AShooterCharacter::OnWalkPressed()
{
	SetWalking(true);
}

void AShooterCharacter::OnWalkReleased()
{
	SetWalking(false);
}

void AShooterCharacter::SetWalking(bool bNewWalking)
{
	if (UShooterCharacterMovement* Move = GetShooterCharacterMovement())
	{
		Move->bIsWalking = bNewWalking;
	}
}

void AShooterCharacter::OnFirePressed()
{
	// P18 brings the weapons (AShooterWeapon, the weapon trace channel, damage).
	UE_LOG(LogShooter, Log, TEXT("%s: Fire (no weapon until P18)"), *GetName());
}

void AShooterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The eyes follow the crouch smoothly (the capsule changes at once; CS lowers the view over a moment).
	FVector& EyeLocation = FirstPersonCameraComponent->RelativeLocation;
	EyeLocation.Z = FMath::FInterpTo(EyeLocation.Z, BaseEyeHeight, DeltaSeconds, EyeHeightInterpSpeed);
}

void AShooterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UpdateBody(NewController);
}

void AShooterCharacter::UpdateBody(AController* NewController)
{
	// A local player never sees its own body (UE: bOwnerNoSee; Leon's renderer has no owner filter).
	const APlayerController* PlayerController = Cast<APlayerController>(NewController);
	if (PlayerController != nullptr && PlayerController->IsLocalPlayerController())
	{
		BodyMesh->SetVisibility(false);
		return;
	}
	const EShooterTeam Team = GetTeam();
	const FSoftObjectPath& MeshName = Team == EShooterTeam::T ? TBodyMeshName : CTBodyMeshName;
	if (Team == EShooterTeam::None || MeshName.IsNull() ||
		!FPackageName::DoesPackageExist(MeshName.GetLongPackageName()))
	{
		return;
	}
	if (UStaticMesh* TeamMesh = Cast<UStaticMesh>(MeshName.TryLoad()))
	{
		(void)BodyMesh->SetStaticMesh(TeamMesh);
		BodyMesh->SetVisibility(true);
	}
}
