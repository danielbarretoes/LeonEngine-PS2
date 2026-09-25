#include "GameFramework/DefaultGameMode.h"

#include "GameFramework/DefaultCameraActor.h"
#include "GameFramework/PlayerState.h"

ADefaultGameMode::ADefaultGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerControllerClass = ADefaultPlayerController::StaticClass();
	DefaultPawnClass = ADefaultCameraActor::StaticClass();
}

void ADefaultGameMode::OnEnter(UGameEngine& Engine, const FString& /*levelPath*/)
{
	// LoadMap logged the player in: its controller possesses the camera actor at the level's Play From Here start.
	Player = Cast<ADefaultPlayerController>(GetWorld()->GetFirstPlayerController());
	if (Player == nullptr)
	{
		return;
	}

	UCameraComponent& Camera = Engine.GetCamera();
	SavedOrbit.Target = Camera.GetTarget();
	SavedOrbit.Distance = Camera.GetDistance();
	SavedOrbit.ViewRotation = Camera.GetViewRotation();

	// The camera flies from the pawn, looking where the player looks.
	Camera.SetMode(ECameraMode::FreeLook);
	if (const ADefaultCameraActor* CameraActor = Player->GetDefaultCameraActor())
	{
		Camera.SetEyeLocation(CameraActor->GetActorLocation());
	}
	Camera.SetViewRotation(Player->GetControlRotation());

	// Runtime / New Window: capture.
	Engine.SetCursorCaptured(true);
	bMouseLookSampleValid = false;

	Engine.AddOnScreenDebugMessage(
		-1, 5.0f, {0.35f, 0.95f, 0.55f}, "DefaultCameraActor -- mouse look, WASD fly, Q/E up/down");
}

void ADefaultGameMode::OnExit(UGameEngine& Engine)
{
	if (Player == nullptr)
	{
		return;
	}
	GetGameState().HandleMatchHasEnded();
	Logout(Player);
	ADefaultCameraActor* CameraActor = Player->GetDefaultCameraActor();
	Player->UnPossess();
	if (CameraActor != nullptr)
	{
		CameraActor->Destroy();
	}
	bMouseLookSampleValid = false;

	UCameraComponent& Camera = Engine.GetCamera();
	Camera.SetMode(ECameraMode::Orbit);
	Camera.SetTarget(SavedOrbit.Target);
	Camera.SetDistance(SavedOrbit.Distance);
	Camera.SetViewRotation(SavedOrbit.ViewRotation);

	Engine.SetCursorCaptured(false);
}

void ADefaultGameMode::Tick(UGameEngine& Engine, float DeltaTime)
{
	if (Player == nullptr)
	{
		GetWorld()->Tick(DeltaTime);
		return;
	}

	ADefaultCameraActor* CameraActor = Player->GetDefaultCameraActor();
	if (CameraActor == nullptr || CameraActor->IsPendingKillPending())
	{
		return;
	}

	UCameraComponent& Camera = Engine.GetCamera();
	// UGameEngine::HandleInput's free look may already have turned the shared camera this frame: start from it.
	Player->SetControlRotation(Camera.GetViewRotation());
	if (const FGenericWindow* Window = Engine.GetWindow())
	{
		const FVector2D Cursor = Window->GetCursorPos();
		const double MouseX = Cursor.X;
		const double MouseY = Cursor.Y;
		if (bMouseLookSampleValid)
		{
			const float Dx = static_cast<float>(MouseX - LastMouseX);
			const float Dy = static_cast<float>(MouseY - LastMouseY);
			constexpr float LookDegreesPerPixel = 0.15f;
			CameraActor->AddControllerYawInput(Dx * LookDegreesPerPixel);
			CameraActor->AddControllerPitchInput(-Dy * LookDegreesPerPixel);
		}
		bMouseLookSampleValid = true;
		LastMouseX = MouseX;
		LastMouseY = MouseY;
	}

	const FRotator ControlRotation = Player->GetControlRotation();
	Camera.SetViewRotation(ControlRotation);

	const FVector Wish = Player->TickInput(Engine);
	FVector Location = CameraActor->GetActorLocation();
	Location += Wish * CameraActor->GetMoveSpeed() * DeltaTime;
	// Like UE's pawns (bUseControllerRotationYaw), the camera actor turns with the control yaw.
	CameraActor->SetActorLocationAndRotation(Location, FRotator(0.0f, ControlRotation.Yaw, 0.0f));

	// The world ticks here: the game mode's own tick advances the game state's clock.
	GetWorld()->Tick(DeltaTime);

	CameraActor = Player->GetDefaultCameraActor();
	if (CameraActor == nullptr)
	{
		return;
	}

	Camera.SetEyeLocation(CameraActor->GetActorLocation());
}
