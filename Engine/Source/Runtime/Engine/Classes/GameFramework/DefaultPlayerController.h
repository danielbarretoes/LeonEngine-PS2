#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

class ADefaultCameraActor;

/** APlayerController for ADefaultCameraActor: fly along look (Move*) + world up (MoveUp). */
class ENGINE_API ADefaultPlayerController final : public APlayerController
{
public:
	[[nodiscard]] ADefaultCameraActor* GetDefaultCameraActor() const;

	FVector TickInput(UGameEngine& Engine) override;
};
