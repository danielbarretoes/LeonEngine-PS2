#pragma once

#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CameraActor.generated.h"

/**
 * A camera placed in a level (UE: ACameraActor): its root is the UCameraComponent whose view it holds. Leon's camera
 * component keeps its own orbit or free-look view (target, distance, eye, view rotation) instead of following the
 * component transform, so the camera actor saves that view with the map.
 *
 * The legacy levels' camera framing became one (P15): the migrated maps keep it as authored, next to the APlayerStart
 * at the view the level opens with. Nothing looks through it at runtime (UE: a camera actor becomes a player's view
 * only when gameplay code sets it as the view target).
 */
UCLASS()
class ENGINE_API ACameraActor : public AActor
{
	GENERATED_BODY()

public:
	ACameraActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: GetCameraComponent. */
	[[nodiscard]] UCameraComponent* GetCameraComponent() const
	{
		return CameraComponent;
	}

private:
	/** The root (UE: CameraComponent). */
	UPROPERTY()
	UCameraComponent* CameraComponent = nullptr;
};
