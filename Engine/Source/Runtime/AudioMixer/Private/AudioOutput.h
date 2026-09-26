#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

/**
 * Where the mix goes (Leon; UE: the platform's audio mixer device): 16-bit stereo frames queued in order, played after
 * what the output holds. The PS2's queues them on the SPU2 through audsrv; the desktop's in a device of miniaudio.
 * FAudioDevice mixes on the game thread and queues each tick what the output played since the last one, on every
 * platform (Docs/PLANS/ps2-preview.md V1).
 */
class FAudioOutput
{
public:
	virtual ~FAudioOutput() = default;

	/** Opens the output at SampleRate; false (logged) when there is no sound to be had. */
	virtual bool Start(int32 SampleRate) = 0;

	/** Queues NumFrames interleaved stereo frames; what does not fit is dropped. */
	virtual void Queue(const int16* Frames, int32 NumFrames) = 0;
};

/** The platform's output (defined by the platform's AudioOutput source). */
[[nodiscard]] TUniquePtr<FAudioOutput> CreatePlatformAudioOutput();

/** Makes the outputs FAudioDevice opens; null: CreatePlatformAudioOutput (the tests hand it an output of their own). */
using FAudioOutputFactory = TUniquePtr<FAudioOutput> (*)();
void SetAudioOutputFactory(FAudioOutputFactory Factory);
