#include "SpuAdpcmEncoder.h"

namespace
{

	/** The sinc's zero crossings on each side of the kernel (at the cut frequency). */
	constexpr int32 ZeroCrossings = 16;

	int32 ClampSample(int32 Value)
	{
		return FMath::Clamp(Value, -32768, 32767);
	}

	/** Round(Numerator / Denominator) for non-negative values. */
	int64 RoundDivide(int64 Numerator, int64 Denominator)
	{
		return (Numerator + (Denominator / 2)) / Denominator;
	}

	/**
	 * The nibble whose decoded sample is nearest Sample after the prediction, at Shift, and that sample. Each nibble
	 * step is 2^(12 - Shift); the nearest is a rounding shift, and the one next to it wins only when the 16-bit clamp
	 * took the rounding's result away.
	 */
	int32 QuantizeSample(int32 Sample, int32 Prediction, int32 Shift, int32& OutDecoded)
	{
		const int32 StepBits = 12 - Shift;
		const int32 Step = 1 << StepBits;
		// An arithmetic shift is a floor division.
		int32 Nibble = FMath::Clamp((Sample - Prediction + (Step / 2)) >> StepBits, -8, 7);
		int32 Decoded = ClampSample((Nibble * Step) + Prediction);
		if (Decoded != (Nibble * Step) + Prediction)
		{
			// Clamped: the next nibble toward the range may land nearer.
			const int32 Other = Decoded > 0 ? Nibble - 1 : Nibble + 1;
			if (Other >= -8 && Other <= 7)
			{
				const int32 OtherDecoded = ClampSample((Other * Step) + Prediction);
				if (FMath::Abs(Sample - OtherDecoded) < FMath::Abs(Sample - Decoded))
				{
					Nibble = Other;
					Decoded = OtherDecoded;
				}
			}
		}
		OutDecoded = Decoded;
		return Nibble;
	}

	/**
	 * Encodes 28 samples into Out's header and nibbles (not its flags), continuing from the decoder's two previous
	 * samples, which it updates. bNoPrediction keeps filter 0 (a loop's first block).
	 */
	void EncodeBlock(const int16* Samples, int32& InOutPrevious1, int32& InOutPrevious2, bool bNoPrediction, uint8* Out)
	{
		int64 BestError = MAX_int64;
		int32 BestFilter = 0;
		int32 BestShift = 0;
		const int32 NumFilters = bNoPrediction ? 1 : FSpuAdpcm::NumFilters;
		for (int32 Filter = 0; Filter < NumFilters; ++Filter)
		{
			const int32 F0 = FSpuAdpcm::FilterCoefficients[Filter][0];
			const int32 F1 = FSpuAdpcm::FilterCoefficients[Filter][1];
			for (int32 Shift = 0; Shift <= FSpuAdpcm::MaxShift; ++Shift)
			{
				int32 Previous1 = InOutPrevious1;
				int32 Previous2 = InOutPrevious2;
				int64 Error = 0;
				for (int32 Index = 0; Index < FSpuAdpcm::SamplesPerBlock && Error < BestError; ++Index)
				{
					const int32 Prediction = ((Previous1 * F0) + (Previous2 * F1) + 32) >> 6;
					int32 Decoded = 0;
					(void)QuantizeSample(Samples[Index], Prediction, Shift, Decoded);
					const int64 Difference = int64(Samples[Index]) - Decoded;
					Error += Difference * Difference;
					Previous2 = Previous1;
					Previous1 = Decoded;
				}
				if (Error < BestError)
				{
					BestError = Error;
					BestFilter = Filter;
					BestShift = Shift;
				}
			}
		}
		Out[0] = uint8((BestFilter << 4) | BestShift);
		FMemory::Memzero(Out + 2, FSpuAdpcm::BytesPerBlock - 2);
		const int32 F0 = FSpuAdpcm::FilterCoefficients[BestFilter][0];
		const int32 F1 = FSpuAdpcm::FilterCoefficients[BestFilter][1];
		for (int32 Index = 0; Index < FSpuAdpcm::SamplesPerBlock; ++Index)
		{
			const int32 Prediction = ((InOutPrevious1 * F0) + (InOutPrevious2 * F1) + 32) >> 6;
			int32 Decoded = 0;
			const int32 Nibble = QuantizeSample(Samples[Index], Prediction, BestShift, Decoded);
			Out[2 + (Index >> 1)] |= uint8((Nibble & 0x0F) << ((Index & 1) * 4));
			InOutPrevious2 = InOutPrevious1;
			InOutPrevious1 = Decoded;
		}
	}

	/** Blackman's window over [-1, 1]. */
	float BlackmanWindow(float T)
	{
		if (T <= -1.0f || T >= 1.0f)
		{
			return 0.0f;
		}
		return 0.42f + (0.5f * FMath::Cos(PI * T)) + (0.08f * FMath::Cos(2.0f * PI * T));
	}

	float Sinc(float X)
	{
		return FMath::Abs(X) < 1.0e-6f ? 1.0f : FMath::Sin(PI * X) / (PI * X);
	}

} // namespace

FSpuAdpcmSound FSpuAdpcmCompressed::GetSound() const
{
	FSpuAdpcmSound Sound;
	Sound.Blocks = Blocks.GetData();
	Sound.NumBlocks = Blocks.Num() / FSpuAdpcm::BytesPerBlock;
	Sound.SampleRate = SampleRate;
	Sound.LoopStartFrame = LoopStartFrame;
	return Sound;
}

int32 FSpuAdpcmEncoder::GetTargetSampleRate(int32 SourceRate, int32 RequestedRate)
{
	const int32 Rate = RequestedRate > 0 ? FMath::Min(SourceRate, RequestedRate) : SourceRate;
	return FMath::Clamp(Rate, 1, FSpuAdpcm::OutputRate);
}

void FSpuAdpcmEncoder::DownmixToMono(
	const int16* Interleaved, int32 NumFrames, int32 NumChannels, TArray<int16>& OutMono)
{
	OutMono.SetNumUninitialized(FMath::Max(NumFrames, 0));
	const int32 Channels = FMath::Max(NumChannels, 1);
	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		int32 Sum = 0;
		for (int32 Channel = 0; Channel < Channels; ++Channel)
		{
			Sum += Interleaved[(Frame * Channels) + Channel];
		}
		// Rounded to the nearest, halves away from zero.
		const int32 Rounded = Sum >= 0 ? (Sum + (Channels / 2)) / Channels : -((-Sum + (Channels / 2)) / Channels);
		OutMono[Frame] = int16(ClampSample(Rounded));
	}
}

void FSpuAdpcmEncoder::ResampleSegment(const int16* Source, int32 SourceFrames, int32 SegmentStart, int32 SegmentFrames,
	int32 OutFrames, bool bWrap, TArray<int16>& OutSamples)
{
	OutSamples.SetNumUninitialized(FMath::Max(OutFrames, 0));
	if (OutFrames <= 0 || SegmentFrames <= 0)
	{
		FMemory::Memzero(OutSamples.GetData(), SIZE_T(OutSamples.Num()) * sizeof(int16));
		return;
	}
	if (OutFrames == SegmentFrames)
	{
		FMemory::Memcpy(OutSamples.GetData(), Source + SegmentStart, SIZE_T(OutFrames) * sizeof(int16));
		return;
	}
	// Source frames per output frame, and the cut: the lower of the two Nyquists, in the source's terms.
	const float Ratio = float(SegmentFrames) / float(OutFrames);
	const float Cutoff = FMath::Min(1.0f, 1.0f / Ratio);
	const float HalfWidth = float(ZeroCrossings) / Cutoff;
	const int32 Taps = FMath::CeilToInt(HalfWidth);
	for (int32 Out = 0; Out < OutFrames; ++Out)
	{
		const float Position = float(Out) * Ratio;
		const int32 Center = FMath::FloorToInt(Position);
		float Sum = 0.0f;
		float WeightSum = 0.0f;
		for (int32 Tap = Center - Taps + 1; Tap <= Center + Taps; ++Tap)
		{
			const float Distance = Position - float(Tap);
			const float Weight = Cutoff * Sinc(Cutoff * Distance) * BlackmanWindow(Distance / HalfWidth);
			if (Weight == 0.0f)
			{
				continue;
			}
			float Sample = 0.0f;
			if (bWrap)
			{
				const int32 Wrapped = ((Tap % SegmentFrames) + SegmentFrames) % SegmentFrames;
				Sample = float(Source[SegmentStart + Wrapped]);
			}
			else if (SegmentStart + Tap >= 0 && SegmentStart + Tap < SourceFrames)
			{
				Sample = float(Source[SegmentStart + Tap]);
			}
			Sum += Sample * Weight;
			WeightSum += Weight;
		}
		OutSamples[Out] = int16(ClampSample(FMath::RoundToInt(WeightSum > 0.0f ? Sum / WeightSum : 0.0f)));
	}
}

void FSpuAdpcmEncoder::EncodeBlocks(
	const int16* Samples, int32 NumFrames, int32 LoopStartFrame, TArray<uint8>& OutBlocks)
{
	constexpr int32 BlockSamples = FSpuAdpcm::SamplesPerBlock;
	const int32 NumBlocks = FMath::Max(1, (NumFrames + BlockSamples - 1) / BlockSamples);
	const int32 LoopStartBlock = LoopStartFrame != INDEX_NONE ? LoopStartFrame / BlockSamples : INDEX_NONE;
	OutBlocks.SetNumZeroed(NumBlocks * FSpuAdpcm::BytesPerBlock);
	int32 Previous1 = 0;
	int32 Previous2 = 0;
	int16 Padded[BlockSamples];
	for (int32 Block = 0; Block < NumBlocks; ++Block)
	{
		const int32 First = Block * BlockSamples;
		for (int32 Index = 0; Index < BlockSamples; ++Index)
		{
			Padded[Index] = First + Index < NumFrames ? Samples[First + Index] : int16(0);
		}
		uint8* Out = OutBlocks.GetData() + (Block * FSpuAdpcm::BytesPerBlock);
		EncodeBlock(Padded, Previous1, Previous2, Block == LoopStartBlock, Out);
		uint8 Flags = 0;
		if (LoopStartBlock != INDEX_NONE && Block >= LoopStartBlock)
		{
			Flags |= FSpuAdpcm::FlagRepeat;
		}
		if (Block == LoopStartBlock)
		{
			Flags |= FSpuAdpcm::FlagLoopStart;
		}
		if (Block == NumBlocks - 1)
		{
			Flags |= FSpuAdpcm::FlagLoopEnd;
		}
		Out[1] = Flags;
	}
}

bool FSpuAdpcmEncoder::Compress(const int16* Interleaved, int32 NumFrames, int32 NumChannels, int32 SampleRate,
	const FSpuAdpcmSettings& Settings, FSpuAdpcmCompressed& Out)
{
	Out = FSpuAdpcmCompressed();
	if (Interleaved == nullptr || NumFrames <= 0 || NumChannels <= 0 || SampleRate <= 0)
	{
		return false;
	}
	TArray<int16> Mono;
	DownmixToMono(Interleaved, NumFrames, NumChannels, Mono);
	const int32 TargetRate = GetTargetSampleRate(SampleRate, Settings.SampleRate);
	TArray<int16> Prepared;
	int32 LoopStartFrame = INDEX_NONE;
	if (!Settings.bLooping)
	{
		const int32 OutFrames = FMath::Max(1, int32(RoundDivide(int64(NumFrames) * TargetRate, int64(SampleRate))));
		ResampleSegment(Mono.GetData(), NumFrames, 0, NumFrames, OutFrames, false, Prepared);
	}
	else
	{
		// The part before the loop, then the loop resampled to whole blocks (wrapping around itself), with silence
		// ahead so that the loop starts on a block.
		constexpr int32 BlockSamples = FSpuAdpcm::SamplesPerBlock;
		const int32 SourceLoopStart = FMath::Clamp(Settings.LoopStartFrame, 0, NumFrames - 1);
		const int32 PreludeFrames = int32(RoundDivide(int64(SourceLoopStart) * TargetRate, int64(SampleRate)));
		const int32 LoopFrames = int32(RoundDivide(int64(NumFrames - SourceLoopStart) * TargetRate, int64(SampleRate)));
		const int32 AlignedLoopFrames =
			FMath::Max(BlockSamples, ((LoopFrames + (BlockSamples / 2)) / BlockSamples) * BlockSamples);
		const int32 Padding = (BlockSamples - (PreludeFrames % BlockSamples)) % BlockSamples;
		TArray<int16> Prelude;
		TArray<int16> Loop;
		ResampleSegment(Mono.GetData(), NumFrames, 0, SourceLoopStart, PreludeFrames, false, Prelude);
		ResampleSegment(
			Mono.GetData(), NumFrames, SourceLoopStart, NumFrames - SourceLoopStart, AlignedLoopFrames, true, Loop);
		Prepared.SetNumZeroed(Padding);
		Prepared.Append(Prelude);
		Prepared.Append(Loop);
		LoopStartFrame = Padding + PreludeFrames;
	}
	EncodeBlocks(Prepared.GetData(), Prepared.Num(), LoopStartFrame, Out.Blocks);
	Out.SampleRate = TargetRate;
	Out.LoopStartFrame = LoopStartFrame;
	return true;
}
