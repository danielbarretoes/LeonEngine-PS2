#include "GameFramework/DefaultGameMode.h"

#include "GameFramework/DefaultCameraActor.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>

namespace
{

	void BeginFreeLookFromOrbit(UCameraComponent& Camera)
	{
		const glm::vec3 Eye = Camera.GetCameraLocation();
		const glm::vec3 LocalTarget = Camera.GetTarget();
		glm::vec3 Look = LocalTarget - Eye;
		const float LookLen = glm::length(Look);
		if (LookLen > 1.0e-5f)
		{
			Look /= LookLen;
		}
		else
		{
			Look = glm::vec3{0.0f, 0.0f, -1.0f};
		}

		const float Pitch = std::asin(std::clamp(Look.y, -1.0f, 1.0f)) * (180.0f / glm::pi<float>());
		const float Yaw = std::atan2(Look.z, Look.x) * (180.0f / glm::pi<float>());

		Camera.SetMode(ECameraMode::FreeLook);
		Camera.SetEyeLocation(Eye);
		Camera.SetYawPitch(Yaw, Pitch);
	}

} // namespace

bool ADefaultGameMode::Matches(const FLevelEntry& /*entry*/, const std::string& GameModeId) const
{
	return GameModeId.empty() || GameModeId == Id();
}

void ADefaultGameMode::OnEnter(UGameEngine& Engine, const std::string& /*levelPath*/)
{
	Player.UnPossess();
	GetWorld().Clear();
	GetGameState().Reset();
	Player.GetPlayerState().Reset();

	UCameraComponent& Camera = Engine.GetCamera();
	SavedOrbit.Target = Camera.GetTarget();
	SavedOrbit.Distance = Camera.GetDistance();
	SavedOrbit.YawDegrees = Camera.GetYawDegrees();
	SavedOrbit.PitchDegrees = Camera.GetPitchDegrees();

	BeginFreeLookFromOrbit(Camera);

	auto* CameraActor = GetWorld().SpawnActor<ADefaultCameraActor>();
	CameraActor->SetActorLocation(Camera.EyeLocation());
	CameraActor->SetActorYaw(Camera.GetYawDegrees());
	Player.Possess(CameraActor);
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
		"DefaultCameraActor — mouse look, WASD fly, Q/E up/down", 5.0f, {0.35f, 0.95f, 0.55f});
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
	Camera.SetYawPitch(SavedOrbit.YawDegrees, SavedOrbit.PitchDegrees);

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
	double MouseX = 0.0;
	double MouseY = 0.0;
	Engine.GetPlayInputWindow().GetCursorPos(MouseX, MouseY);
	// EditorApp does not call Engine::handleInput — apply look here for PIE + runtime.
	if (Engine.IsCursorCaptured() || Engine.IsPlayMouseLookActive())
	{
		if (bMouseLookSampleValid)
		{
			const float Dx = static_cast<float>(MouseX - LastMouseX);
			const float Dy = static_cast<float>(MouseY - LastMouseY);
			constexpr float LookDegreesPerPixel = 0.15f;
			Camera.AddLook(Dx * LookDegreesPerPixel, -Dy * LookDegreesPerPixel);
		}
		bMouseLookSampleValid = true;
		LastMouseX = MouseX;
		LastMouseY = MouseY;
	}
	else
	{
		bMouseLookSampleValid = false;
	}

	const glm::vec3 Wish = Player.TickInput(Engine);
	glm::vec3 Location = CameraActor->GetActorLocation();
	Location += Wish * CameraActor->GetMoveSpeed() * DeltaTime;
	CameraActor->SetActorLocation(Location);
	CameraActor->SetActorYaw(Camera.GetYawDegrees());

	GetWorld().Tick(DeltaTime);

	CameraActor = Player.GetDefaultCameraActor();
	if (CameraActor == nullptr)
	{
		return;
	}

	Camera.SetEyeLocation(CameraActor->GetActorLocation());
}
