#pragma once

#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "GameFramework/DefaultPlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "DefaultGameMode.generated.h"

/**
 * Default GameMode (Unreal default GameMode).
 * Spawns and possesses ADefaultCameraActor as the default pawn (free-look fly).
 */
UCLASS()
class ENGINE_API ADefaultGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	using Super::Tick;

	void OnEnter(UGameEngine& Engine, const FString& LevelPath) override;
	void OnExit(UGameEngine& Engine) override;
	void Tick(UGameEngine& Engine, float DeltaTime) override;

private:
	struct FOrbitSnapshot
	{
		FVector Target = FVector::ZeroVector;
		/** cm */
		float Distance = 800.0f;
		FRotator ViewRotation = FRotator(-25.0f, 225.0f, 0.0f);
	};

	ADefaultPlayerController Player{};
	FOrbitSnapshot SavedOrbit{};
	double LastMouseX = 0.0;
	double LastMouseY = 0.0;
	bool bMouseLookSampleValid = false;
};
