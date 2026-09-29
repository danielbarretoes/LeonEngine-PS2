#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SoundBase.generated.h"

/**
 * The base of the sound assets (UE: USoundBase): anything that can be played. Leon has sound waves only (no sound
 * cues, classes, attenuation or concurrency yet).
 */
UCLASS(Abstract)
class ENGINE_API USoundBase : public UObject
{
	GENERATED_BODY()

public:
	USoundBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Length in seconds (UE: Duration). */
	UPROPERTY()
	float Duration = 0.0f;

	/**
	 * How much the sound matters when voices run out (UE: Priority): the default, 1, leaves the last free voices
	 * (FAudioDevice::NumReservedVoices) to sounds above it.
	 */
	UPROPERTY()
	float Priority = 1.0f;

	/** UE: GetDuration. */
	[[nodiscard]] virtual float GetDuration() const
	{
		return Duration;
	}
};
