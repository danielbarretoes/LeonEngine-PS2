#include "SpuAdpcm.h"

const TCHAR* const FSpuAdpcm::FormatName = TEXT("SPU2ADPCM");

bool FSpuAdpcmSound::IsValid() const
{
	return Blocks != nullptr && NumBlocks > 0 && SampleRate > 0 &&
		(LoopStartFrame == INDEX_NONE ||
			(LoopStartFrame >= 0 && LoopStartFrame < GetNumFrames() &&
				LoopStartFrame % FSpuAdpcm::SamplesPerBlock == 0));
}

int32 FSpuAdpcmSound::GetNumFrames() const
{
	return NumBlocks * FSpuAdpcm::SamplesPerBlock;
}

int32 FSpuAdpcmSound::GetNumBytes() const
{
	return NumBlocks * FSpuAdpcm::BytesPerBlock;
}

int32 FSpuAdpcm::GetPitch(int32 SampleRate)
{
	const int64 Pitch = ((int64(FMath::Max(SampleRate, 0)) * PitchOne) + (OutputRate / 2)) / OutputRate;
	return int32(FMath::Clamp<int64>(Pitch, 1, MaxPitch));
}

uint64 FSpuAdpcm::GetPlayMicroseconds(int32 NumFrames, int32 Pitch)
{
	// NumFrames source frames at Pitch / 4096 of them per 48 kHz output frame.
	const uint64 Numerator = uint64(FMath::Max(NumFrames, 0)) * uint64(PitchOne) * 1000000ull;
	const uint64 Denominator = uint64(FMath::Max(Pitch, 1)) * uint64(OutputRate);
	return (Numerator + Denominator - 1) / Denominator;
}

void FSpuAdpcm::DecodeBlock(const uint8* Block, int32& InOutPrevious1, int32& InOutPrevious2, int16* Out)
{
	const int32 Filter = FMath::Min(int32(Block[0] >> 4), NumFilters - 1);
	int32 Shift = int32(Block[0] & 0x0F);
	if (Shift > MaxShift)
	{
		Shift = 9;
	}
	const int32 F0 = FilterCoefficients[Filter][0];
	const int32 F1 = FilterCoefficients[Filter][1];
	int32 Previous1 = InOutPrevious1;
	int32 Previous2 = InOutPrevious2;
	for (int32 Index = 0; Index < SamplesPerBlock; ++Index)
	{
		const uint8 Byte = Block[2 + (Index >> 1)];
		const uint32 Nibble = (Index & 1) != 0 ? uint32(Byte >> 4) : uint32(Byte & 0x0F);
		// The nibble sign-extended from the top of 16 bits, then shifted down arithmetically.
		int32 Sample = int32(int16(uint16(Nibble << 12))) >> Shift;
		Sample += ((Previous1 * F0) + (Previous2 * F1) + 32) >> 6;
		Sample = FMath::Clamp(Sample, -32768, 32767);
		Out[Index] = int16(Sample);
		Previous2 = Previous1;
		Previous1 = Sample;
	}
	InOutPrevious1 = Previous1;
	InOutPrevious2 = Previous2;
}

void FSpuAdpcm::Decode(const FSpuAdpcmSound& Sound, TArray<int16>& OutSamples)
{
	OutSamples.Reset();
	if (Sound.Blocks == nullptr || Sound.NumBlocks <= 0)
	{
		return;
	}
	OutSamples.SetNumUninitialized(Sound.GetNumFrames());
	int32 Previous1 = 0;
	int32 Previous2 = 0;
	for (int32 Block = 0; Block < Sound.NumBlocks; ++Block)
	{
		DecodeBlock(Sound.Blocks + (Block * BytesPerBlock), Previous1, Previous2,
			OutSamples.GetData() + (Block * SamplesPerBlock));
	}
}
