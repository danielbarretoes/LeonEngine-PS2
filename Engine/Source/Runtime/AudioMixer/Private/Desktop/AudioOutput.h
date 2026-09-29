#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

/**
 * Where the desktop's mix goes (Leon; UE: the platform's audio mixer device): 16-bit stereo frames queued in order,
 * played after what the output holds; the desktop's is a device of miniaudio. The desktop's FAudioHardware mixes its
 * voices on the game thread and queues each tick what the output played since the last one. The PS2 has no such
 * stream: its voices are the SPU2's (Docs/PLANS/ps2-shipping.md N19).
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

/** The desktop's output (miniaudio: MiniAudioOutput.cpp). */
[[nodiscard]] TUniquePtr<FAudioOutput> CreatePlatformAudioOutput();

/** Makes the outputs the desktop's hardware opens; null: CreatePlatformAudioOutput (tests hand it one of their own). */
using FAudioOutputFactory = TUniquePtr<FAudioOutput> (*)();
void SetAudioOutputFactory(FAudioOutputFactory Factory);
