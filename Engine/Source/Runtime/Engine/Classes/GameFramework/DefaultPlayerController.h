#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DefaultPlayerController.generated.h"

class ADefaultCameraActor;

/** APlayerController for ADefaultCameraActor: fly along look (Move*) + world up (MoveUp). */
UCLASS()
class ENGINE_API ADefaultPlayerController final : public APlayerController
{
	GENERATED_BODY()

public:
	[[nodiscard]] ADefaultCameraActor* GetDefaultCameraActor() const;

	FVector TickInput(UGameEngine& Engine) override;
};
