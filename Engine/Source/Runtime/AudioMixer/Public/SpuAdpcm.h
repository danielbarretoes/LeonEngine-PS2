#pragma once

#include "CoreMinimal.h"

/**
 * A mono sound in the SPU2's ADPCM (the PS1 / PS2 "VAG" blocks), in memory: what a cooked USoundWave holds and what the
 * audio device plays on every platform (Docs/PLANS/ps2-shipping.md N19). The PS2 uploads the blocks to SPU2 RAM and a
 * hardware voice decodes them; the desktop decodes them with FSpuAdpcm::Decode, so both play the same samples.
 */
struct FSpuAdpcmSound
{
	/** NumBlocks blocks of FSpuAdpcm::BytesPerBlock bytes, their loop flags set (FSpuAdpcm::Flag*). */
	const uint8* Blocks = nullptr;
	int32 NumBlocks = 0;
	/** Frames per second the blocks play at (the voice's pitch: FSpuAdpcm::GetPitch). */
	int32 SampleRate = 0;
	/** The loop's first frame (a block's first), or INDEX_NONE for a one-shot. */
	int32 LoopStartFrame = INDEX_NONE;

	[[nodiscard]] bool IsValid() const;

	[[nodiscard]] bool IsLooping() const
	{
		return LoopStartFrame != INDEX_NONE;
	}

	/** The frames the blocks hold (28 a block). */
	[[nodiscard]] int32 GetNumFrames() const;

	/** The bytes the blocks take (in SPU2 RAM on the PS2). */
	[[nodiscard]] int32 GetNumBytes() const;
};

/**
 * The SPU2's ADPCM format and its decoder (Leon; UE's ADPCM is Microsoft's, this is the console's own). A block is 16
 * bytes: a header byte (the predictor filter in the high nibble, the shift in the low one), a flags byte and 28 4-bit
 * samples, the first in the low nibble. A sample decodes as `(Nibble << 12) >> Shift` plus the filter's prediction from
 * the two decoded samples before it, `(Previous1 * F0 + Previous2 * F1 + 32) >> 6`, clamped to 16 bits; the filters are
 * the hardware's five. The encoder is the AudioCompressor module's (FSpuAdpcmEncoder, host tools only).
 *
 * The flags drive the voice: LoopStart marks the block its loop returns to, LoopEnd ends a block (with Repeat the voice
 * jumps back to the loop start, without it the voice goes silent: a one-shot's last block).
 */
class AUDIOMIXER_API FSpuAdpcm
{
public:
	static constexpr int32 BytesPerBlock = 16;
	static constexpr int32 SamplesPerBlock = 28;
	static constexpr int32 NumFilters = 5;
	/** The largest shift the encoder writes (the hardware treats 13 to 15 as 9). */
	static constexpr int32 MaxShift = 12;

	static constexpr uint8 FlagLoopEnd = 1;
	static constexpr uint8 FlagRepeat = 2;
	static constexpr uint8 FlagLoopStart = 4;

	/** The predictor filters' coefficients, in 64ths: F0 weighs the previous sample, F1 the one before. */
	static constexpr int32 FilterCoefficients[NumFilters][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};

	/** The SPU2 mixes at 48 kHz; a voice's pitch is its rate against it in 4.12 fixed point (4096: 48 kHz). */
	static constexpr int32 OutputRate = 48000;
	static constexpr int32 PitchOne = 4096;
	static constexpr int32 MaxPitch = 0x3FFF;

	/**
	 * The SPU2 RAM the sounds may take: its 2 MB less what audsrv keeps below its first sample (0x5010 bytes: the
	 * cores' input and output areas and audsrv's stream buffers; audsrv's own check, `audsrv_load_adpcm`). The effects'
	 * work areas are not reserved: audsrv never turns the reverb on.
	 */
	static constexpr int32 SoundRamBytes = (2 * 1024 * 1024) - 0x5010;

	/** The name of the format (ITargetPlatform::GetAllWaveFormats; recorded in cooked sound waves). */
	static const TCHAR* const FormatName;

	/** The voice pitch that plays SampleRate frames a second: rounded, between 1 and MaxPitch. */
	[[nodiscard]] static int32 GetPitch(int32 SampleRate);

	/** How long a voice at Pitch takes to play NumFrames frames, in microseconds (rounded up). */
	[[nodiscard]] static uint64 GetPlayMicroseconds(int32 NumFrames, int32 Pitch);

	/**
	 * Decodes one block into Out (SamplesPerBlock samples), continuing from the two samples before it (InOutPrevious1
	 * the last), which it updates: exactly what a voice does.
	 */
	static void DecodeBlock(const uint8* Block, int32& InOutPrevious1, int32& InOutPrevious2, int16* Out);

	/** Decodes a sound's blocks in order from silence, 28 samples a block (the loop is not unrolled). */
	static void Decode(const FSpuAdpcmSound& Sound, TArray<int16>& OutSamples);
};
