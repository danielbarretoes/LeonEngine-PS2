#pragma once

#include "CoreTypes.h"
#include "InputCoreTypes.h"
#include "PS2RHITypes.h"
#include "PS2Texture.h"
#include "ThirdPersonCameraBoom.h"
#include "ThirdPersonCharacter.h"
#include "ThirdPersonLevel.h"

class FGenericWindow;
class IInputInterface;

/**
 * Runs the ThirdPerson demo: builds the level, spawns the character and drives one frame per tick
 * (TP_ThirdPerson: ThirdPersonGameMode). Start requests engine exit.
 */
class THIRDPERSON_API FThirdPersonGameMode
{
public:
	FThirdPersonGameMode(FGenericWindow& InWindow, IInputInterface* InInputInterface);
	~FThirdPersonGameMode();

	/** Clears the display, creates textures / materials / level and places the camera. */
	void StartPlay();

	/** One gameplay frame (input, movement, camera, draw). Returns false once the game ended. */
	bool Tick(float DeltaTime);

private:
	bool IsGamepadKeyDown(const FKey& Key) const;
	float GetGamepadAnalog(const FKey& Axis) const;
	void UpdateStatsMessages();

	FGenericWindow& Window;
	IInputInterface* InputInterface = nullptr;

	FPS2Texture GridTexture;
	FPS2Texture CheckerTexture;
	FPS2Material GroundMaterial;
	FPS2Material PlatformMaterial;
	FPS2Material CrateMaterial;
	FPS2Material CharacterMaterial;
	FPS2DirectionalLight Sun;

	FThirdPersonLevel Level;
	FThirdPersonCharacter Character;
	FThirdPersonCameraBoom CameraBoom;

	uint32 FrameNumber = 0;
	bool bPrevJump = false;
};
