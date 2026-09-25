#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DefaultCameraActor.generated.h"

/**
 * Default possessed pawn for ADefaultGameMode (Unreal-like DefaultPawn / flying camera).
 * Free-look: LMB aims, WASD flies along look direction, Q/E world vertical (Z).
 */
UCLASS()
class ENGINE_API ADefaultCameraActor : public APawn
{
	GENERATED_BODY()

public:
	[[nodiscard]] float GetMoveSpeed() const
	{
		return MoveSpeed;
	}
	void SetMoveSpeed(float Speed)
	{
		MoveSpeed = Speed > 0.0f ? Speed : 0.0f;
	}

	[[nodiscard]] float GetLookSensitivity() const
	{
		return LookSensitivity;
	}
	void SetLookSensitivity(float DegreesPerPixel)
	{
		LookSensitivity = DegreesPerPixel > 0.0f ? DegreesPerPixel : 0.0f;
	}

private:
	/** cm/s */
	UPROPERTY()
	float MoveSpeed = 800.0f;

	/** Degrees per pixel of mouse movement. */
	UPROPERTY()
	float LookSensitivity = 0.15f;
};
