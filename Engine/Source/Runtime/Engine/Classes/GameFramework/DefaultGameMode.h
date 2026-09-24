#pragma once

#include "Engine/GameEngine.h"
#include "GameFramework/DefaultPlayerController.h"
#include "GameFramework/GameModeBase.h"

#include <glm/vec3.hpp>

/// Default GameMode (Unreal default GameMode).
/// Spawns and possesses `ADefaultCameraActor` as the default pawn (free-look fly).
class ENGINE_API ADefaultGameMode final : public AGameModeBase
{
public:
	void OnEnter(UGameEngine& Engine, const std::string& LevelPath) override;
	void OnExit(UGameEngine& Engine) override;
	void Tick(UGameEngine& Engine, float DeltaTime) override;

private:
	struct FOrbitSnapshot
	{
		glm::vec3 Target{0.0f};
		float Distance = 8.0f;
		float YawDegrees = 45.0f;
		float PitchDegrees = 25.0f;
	};

	ADefaultPlayerController Player{};
	FOrbitSnapshot SavedOrbit{};
	double LastMouseX = 0.0;
	double LastMouseY = 0.0;
	bool bMouseLookSampleValid = false;
};
