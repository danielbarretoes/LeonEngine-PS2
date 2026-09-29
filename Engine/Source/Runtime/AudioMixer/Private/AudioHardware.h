#pragma once

#include "CoreMinimal.h"
#include "SpuAdpcm.h"
#include "SpuVoiceVolume.h"
#include "Templates/UniquePtr.h"

/**
 * The platform's sound hardware (Leon; UE: the platform side of its FAudioDevice, the FSoundBuffer and FSoundSource of
 * each platform): sound buffers and 24 voices, as the SPU2 has them. FAudioDevice decides everything (which buffer is
 * resident, which voice plays what and how loud, when a voice is free) the same way on every platform; the hardware
 * only carries it out. The PS2's is the SPU2 through audsrv (the buffers in SPU2 RAM, the voices the hardware's); the
 * desktop's decodes the buffers and mixes the voices on the CPU into a device of miniaudio.
 */
class FAudioHardware
{
public:
	virtual ~FAudioHardware() = default;

	/** Opens the hardware; false (logged) when there is no sound to be had. */
	virtual bool Start() = 0;

	/**
	 * Makes Sound buffer Buffer (0 to FAudioDevice::MaxSoundBuffers - 1), played at Pitch (FSpuAdpcm::GetPitch). The
	 * device places the buffers in SPU2 RAM as audsrv does, one after the other, and frees them from the last; false
	 * (logged) when the hardware refuses.
	 */
	virtual bool UploadSound(int32 Buffer, const FSpuAdpcmSound& Sound, int32 Pitch) = 0;

	/** Frees a buffer no voice plays any more. */
	virtual void FreeSound(int32 Buffer) = 0;

	/**
	 * Starts Voice (0 to FAudioDevice::NumVoices - 1) on Buffer from its start at Volume. False when the voice is still
	 * busy for the hardware (the SPU2 has not reached its end yet): the device tries another.
	 */
	virtual bool PlayVoice(int32 Voice, int32 Buffer, const FSpuVoiceVolume& Volume) = 0;

	virtual void SetVoiceVolume(int32 Voice, const FSpuVoiceVolume& Volume) = 0;

	/**
	 * Silences a voice before its end (the music). The desktop stops it; audsrv cannot key a voice off, so the PS2
	 * mutes it and the device treats it as busy until its sound's end.
	 */
	virtual void StopVoice(int32 Voice) = 0;

	/** Once per engine frame, after the device's updates (the desktop mixes what played since the last tick). */
	virtual void Tick()
	{
	}
};

/** The platform's hardware (defined by the platform's AudioHardware source). */
[[nodiscard]] TUniquePtr<FAudioHardware> CreatePlatformAudioHardware();

/** Makes the hardware FAudioDevice opens; null: CreatePlatformAudioHardware (tests hand it one of their own). */
using FAudioHardwareFactory = TUniquePtr<FAudioHardware> (*)();
void SetAudioHardwareFactory(FAudioHardwareFactory Factory);
