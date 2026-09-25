#include "GameFramework/DefaultGameMode.h"

#include "GameFramework/DefaultCameraActor.h"

namespace
{

	void BeginFreeLookFromOrbit(UCameraComponent& Camera)
	{
		// Fly from the eye, looking at the target (an orbit camera keeps its view; a free-look one turns to it).
		const FVector Eye = Camera.GetCameraLocation();
		FVector Look = Camera.GetTarget() - Eye;
		const float LookLen = Look.Size();
		if (LookLen > 1.0e-3f)
		{
			Look /= LookLen;
		}
		else
		{
			Look = FVector(0.0f, -1.0f, 0.0f);
		}

		Camera.SetMode(ECameraMode::FreeLook);
		Camera.SetEyeLocation(Eye);
		Camera.SetViewRotation(Look.Rotation());
	}

} // namespace

void ADefaultGameMode::OnEnter(UGameEngine& Engine, const FString& /*levelPath*/)
{
	Player.UnPossess();
	GetWorld().Clear();
	GetGameState().Reset();
	Player.GetPlayerState().Reset();

	UCameraComponent& Camera = Engine.GetCamera();
	SavedOrbit.Target = Camera.GetTarget();
	SavedOrbit.Distance = Camera.GetDistance();
	SavedOrbit.ViewRotation = Camera.GetViewRotation();

	BeginFreeLookFromOrbit(Camera);

	auto* CameraActor = GetWorld().SpawnActor<ADefaultCameraActor>();
	CameraActor->SetActorLocationAndRotation(Camera.EyeLocation(), FRotator(0.0f, Camera.GetViewRotation().Yaw, 0.0f));
	Player.Possess(CameraActor);
	// The player looks where the camera looks; mouse look turns the control rotation, which the camera follows.
	Player.SetControlRotation(Camera.GetViewRotation());
	PostLogin(Player);

	GetGameState().HandleMatchHasStarted();
	Engine.GetGameInstance().NotifyLevelOpened();
	Engine.SetKeyboardOrbitEnabled(false);
	Engine.SetOrbitMouseEnabled(false);
	Engine.SetSuppressCameraDrag(false);
	Engine.SetPlayMouseLookActive(true);
	// Runtime / New Window: capture. Editor Selected Viewport clears this and gates look by hover.
	Engine.SetCursorCaptured(true);
	bMouseLookSampleValid = false;

	Engine.AddOnScreenDebugMessage(
		"DefaultCameraActor -- mouse look, WASD fly, Q/E up/down", 5.0f, {0.35f, 0.95f, 0.55f});
}

void ADefaultGameMode::OnExit(UGameEngine& Engine)
{
	GetGameState().HandleMatchHasEnded();
	Logout(Player);
	Player.UnPossess();
	GetWorld().Clear();
	bMouseLookSampleValid = false;

	UCameraComponent& Camera = Engine.GetCamera();
	Camera.SetMode(ECameraMode::Orbit);
	Camera.SetTarget(SavedOrbit.Target);
	Camera.SetDistance(SavedOrbit.Distance);
	Camera.SetViewRotation(SavedOrbit.ViewRotation);

	Engine.SetCursorCaptured(false);
	Engine.SetKeyboardOrbitEnabled(true);
	Engine.SetOrbitMouseEnabled(true);
}

void ADefaultGameMode::Tick(UGameEngine& Engine, float DeltaTime)
{
	GetGameState().Tick(DeltaTime);
	Player.GetPlayerState().Tick(DeltaTime);

	ADefaultCameraActor* CameraActor = Player.GetDefaultCameraActor();
	if (CameraActor == nullptr || CameraActor->IsPendingKillPending())
	{
		return;
	}

	UCameraComponent& Camera = Engine.GetCamera();
	// UGameEngine::HandleInput's free look may already have turned the shared camera this frame: start from it.
	Player.SetControlRotation(Camera.GetViewRotation());
	const FVector2D Cursor = Engine.GetPlayInputWindow().GetCursorPos();
	const double MouseX = Cursor.X;
	const double MouseY = Cursor.Y;
	// EditorApp does not call Engine::handleInput — apply look here for PIE + runtime.
	if (Engine.IsCursorCaptured() || Engine.IsPlayMouseLookActive())
	{
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
	else
	{
		bMouseLookSampleValid = false;
	}

	const FRotator ControlRotation = Player.GetControlRotation();
	Camera.SetViewRotation(ControlRotation);

	const FVector Wish = Player.TickInput(Engine);
	FVector Location = CameraActor->GetActorLocation();
	Location += Wish * CameraActor->GetMoveSpeed() * DeltaTime;
	// Like UE's pawns (bUseControllerRotationYaw), the camera actor turns with the control yaw.
	CameraActor->SetActorLocationAndRotation(Location, FRotator(0.0f, ControlRotation.Yaw, 0.0f));

	GetWorld().Tick(DeltaTime);

	CameraActor = Player.GetDefaultCameraActor();
	if (CameraActor == nullptr)
	{
		return;
	}

	Camera.SetEyeLocation(CameraActor->GetActorLocation());
}
