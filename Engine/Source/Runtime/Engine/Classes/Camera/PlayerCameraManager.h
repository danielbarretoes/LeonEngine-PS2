#pragma once

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerCameraManager.generated.h"

class APlayerController;

/**
 * A player's camera (UE: APlayerCameraManager), spawned by its player controller (PlayerCameraManagerClass) and updated
 * by the world after every actor ticked (APlayerController::UpdateCameraManager). Each update asks the view target (the
 * possessed pawn, else the controller) for its view (AActor::CalcCamera: a camera component's view, else the eyes and
 * the control rotation) and keeps it.
 *
 * Leon keeps the view in a UCameraComponent (GetViewCamera), which builds the matrices the renderer draws with
 * (FSceneView::FromCamera; its free-look mode, a vertical field of view); UE keeps an FMinimalViewInfo cache.
 */
UCLASS(NotPlaceable, Transient)
class ENGINE_API APlayerCameraManager : public AActor
{
	GENERATED_BODY()

public:
	APlayerCameraManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The controller that owns this camera (UE: PCOwner). */
	UPROPERTY(Transient)
	APlayerController* PCOwner = nullptr;

	/** The vertical field of view a view starts with, degrees (UE: DefaultFOV, horizontal there). */
	UPROPERTY()
	float DefaultFOV = 60.0f;

	/** Takes its controller (UE: InitializeFor). */
	virtual void InitializeFor(APlayerController* PC);

	/** Views another actor (UE: SetViewTarget). */
	virtual void SetViewTarget(AActor* NewViewTarget);

	/** The actor viewed: the view target, else the controller's pawn, else the controller (UE: GetViewTarget). */
	[[nodiscard]] AActor* GetViewTarget() const;

	/** Asks the view target for its view and keeps it (UE: UpdateCamera). */
	virtual void UpdateCamera(float DeltaTime);

	/** The view of the last update (UE: GetCameraCacheView). */
	[[nodiscard]] const FMinimalViewInfo& GetCameraCacheView() const
	{
		return CameraCachePOV;
	}
	[[nodiscard]] FVector GetCameraLocation() const
	{
		return CameraCachePOV.Location;
	}
	[[nodiscard]] FRotator GetCameraRotation() const
	{
		return CameraCachePOV.Rotation;
	}

	/** The field of view in use (UE: GetFOVAngle). */
	[[nodiscard]] float GetFOVAngle() const;

	/** Locks the field of view; 0 unlocks it (UE: SetFOV / UnlockFOV). */
	void SetFOV(float NewFOV);
	void UnlockFOV();

	/** The camera the renderer draws the view with (Leon; see the class comment). */
	[[nodiscard]] UCameraComponent* GetViewCamera() const
	{
		return ViewCamera;
	}

private:
	/** The last view (UE: CameraCachePrivate.POV). */
	FMinimalViewInfo CameraCachePOV;

	/** A locked field of view, 0 when unlocked (UE: LockedFOV). */
	float LockedFOV = 0.0f;

	/** The actor viewed, when not the controller's pawn (UE: ViewTarget.Target). */
	UPROPERTY(Transient)
	AActor* ViewTargetActor = nullptr;

	/** The view's camera (Leon). */
	UPROPERTY()
	UCameraComponent* ViewCamera = nullptr;
};
