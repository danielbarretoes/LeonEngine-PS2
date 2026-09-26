#include "GameFramework/Pawn.h"

#include "Components/InputComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"

APawn::APawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUseControllerRotationPitch(false)
	, bUseControllerRotationYaw(true)
	, bUseControllerRotationRoll(false)
{
}

void APawn::PawnClientRestart()
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (PlayerController == nullptr || !PlayerController->IsLocalController() || InputComponent != nullptr)
	{
		return;
	}
	InputComponent = CreatePlayerInputComponent();
	if (InputComponent != nullptr)
	{
		SetupPlayerInputComponent(InputComponent);
		InputComponent->RegisterComponent();
	}
}

UInputComponent* APawn::CreatePlayerInputComponent()
{
	static const FName InputComponentName(TEXT("PawnInputComponent0"));
	return NewObject<UInputComponent>(this, UInputSettings::GetDefaultInputComponentClass(), InputComponentName);
}

void APawn::DestroyPlayerInputComponent()
{
	if (InputComponent != nullptr)
	{
		InputComponent->DestroyComponent();
		InputComponent = nullptr;
	}
}

void APawn::PossessedBy(AController* /*NewController*/)
{
}

void APawn::UnPossessed()
{
	DestroyPlayerInputComponent();
	ControlInputVector = FVector::ZeroVector;
}

UPawnMovementComponent* APawn::GetMovementComponent() const
{
	return FindComponentByClass<UPawnMovementComponent>();
}

void APawn::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	if (UPawnMovementComponent* MovementComponent = GetMovementComponent())
	{
		MovementComponent->AddInputVector(WorldDirection * ScaleValue, bForce);
	}
	else
	{
		Internal_AddMovementInput(WorldDirection * ScaleValue, bForce);
	}
}

FVector APawn::ConsumeMovementInputVector()
{
	if (UPawnMovementComponent* MovementComponent = GetMovementComponent())
	{
		return MovementComponent->ConsumeInputVector();
	}
	return Internal_ConsumeMovementInputVector();
}

void APawn::Internal_AddMovementInput(FVector WorldAccel, bool /*bForce*/)
{
	ControlInputVector += WorldAccel;
}

FVector APawn::Internal_ConsumeMovementInputVector()
{
	LastControlInputVector = ControlInputVector;
	ControlInputVector = FVector::ZeroVector;
	return LastControlInputVector;
}

void APawn::FaceRotation(FRotator NewControlRotation, float /*DeltaTime*/)
{
	if (!bUseControllerRotationPitch && !bUseControllerRotationYaw && !bUseControllerRotationRoll)
	{
		return;
	}
	const FRotator CurrentRotation = GetActorRotation();
	if (!bUseControllerRotationPitch)
	{
		NewControlRotation.Pitch = CurrentRotation.Pitch;
	}
	if (!bUseControllerRotationYaw)
	{
		NewControlRotation.Yaw = CurrentRotation.Yaw;
	}
	if (!bUseControllerRotationRoll)
	{
		NewControlRotation.Roll = CurrentRotation.Roll;
	}
	SetActorRotation(NewControlRotation);
}

void APawn::RecalculateBaseEyeHeight()
{
	BaseEyeHeight = GetClass()->GetDefaultObject<APawn>()->BaseEyeHeight;
}

FVector APawn::GetPawnViewLocation() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, BaseEyeHeight);
}

void APawn::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = GetPawnViewLocation();
	OutRotation = GetViewRotation();
}

FRotator APawn::GetControlRotation() const
{
	return Controller != nullptr ? Controller->GetControlRotation() : FRotator::ZeroRotator;
}

FRotator APawn::GetViewRotation() const
{
	return Controller != nullptr ? Controller->GetControlRotation() : GetActorRotation();
}

void APawn::AddControllerYawInput(float Val)
{
	if (Val == 0.0f)
	{
		return;
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->AddYawInput(Val);
	}
}

void APawn::AddControllerPitchInput(float Val)
{
	if (Val == 0.0f)
	{
		return;
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->AddPitchInput(Val);
	}
}

void APawn::DetachController()
{
	if (Controller == nullptr)
	{
		return;
	}
	// Controller::UnPossess clears Pawn and calls bindController(nullptr).
	Controller->UnPossess();
}

void APawn::Destroyed()
{
	DetachController();
	Super::Destroyed();
}

void APawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DetachController();
	Super::EndPlay(EndPlayReason);
}
