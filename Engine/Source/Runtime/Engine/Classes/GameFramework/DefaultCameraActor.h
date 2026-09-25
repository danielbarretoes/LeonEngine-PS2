#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

/**
 * Default possessed pawn for ADefaultGameMode (Unreal-like DefaultPawn / flying camera).
 * Free-look: LMB aims, WASD flies along look direction, Q/E world vertical.
 */
class ENGINE_API ADefaultCameraActor : public APawn
{
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
	float MoveSpeed = 800.0f;
	float LookSensitivity = 0.15f;
};
