#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "SpuAdpcmEncoder.h"

#if WITH_DEV_AUTOMATION_TESTS

// The SPU2 ADPCM encoder (Docs/PLANS/ps2-shipping.md N19): what comes back through the console's decoder (FSpuAdpcm,
// the same the desktop plays), the loop flags, the rate and the channels, and the same bytes every run.

namespace
{

	constexpr int32 TestRate = 22050;

	/** A deterministic noise source (a 32-bit LCG), -1 to 1. */
	struct FTestNoise
	{
		uint32 State = 12345;

		float Next()
		{
			State = (State * 1664525u) + 1013904223u;
			return (float(State >> 8) / float(1 << 23)) - 1.0f;
		}
	};

	TArray<int16> MakeSine(int32 NumFrames, float Frequency, int32 Rate, float Amplitude)
	{
		TArray<int16> Samples;
		Samples.SetNumUninitialized(NumFrames);
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			const float Phase = 2.0f * PI * Frequency * float(Frame) / float(Rate);
			Samples[Frame] = int16(FMath::RoundToInt(FMath::Sin(Phase) * Amplitude * 32767.0f));
		}
		return Samples;
	}

	/** Signal-to-noise ratio in dB of Decoded against Source over their first NumFrames (Skip frames left out). */
	float GetSnr(const TArray<int16>& Source, const TArray<int16>& Decoded, int32 NumFrames, int32 Skip = 0)
	{
		double Signal = 0.0;
		double Noise = 0.0;
		for (int32 Frame = Skip; Frame < NumFrames - Skip; ++Frame)
		{
			const double Value = double(Source[Frame]);
			const double Error = double(Decoded[Frame]) - Value;
			Signal += Value * Value;
			Noise += Error * Error;
		}
		return Noise <= 0.0 ? 200.0f : 10.0f * FMath::Loge(float(Signal / Noise)) / FMath::Loge(10.0f);
	}

	/** Encodes Samples at their rate (a one-shot) and decodes them back. */
	TArray<int16> RoundTrip(const TArray<int16>& Samples, TArray<uint8>* OutBlocks = nullptr)
	{
		TArray<uint8> Blocks;
		FSpuAdpcmEncoder::EncodeBlocks(Samples.GetData(), Samples.Num(), INDEX_NONE, Blocks);
		FSpuAdpcmSound Sound;
		Sound.Blocks = Blocks.GetData();
		Sound.NumBlocks = Blocks.Num() / FSpuAdpcm::BytesPerBlock;
		Sound.SampleRate = TestRate;
		TArray<int16> Decoded;
		FSpuAdpcm::Decode(Sound, Decoded);
		if (OutBlocks != nullptr)
		{
			*OutBlocks = MoveTemp(Blocks);
		}
		return Decoded;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpuAdpcmEncoderRoundTripTest, "System.AudioCompressor.SpuAdpcm.RoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSpuAdpcmEncoderRoundTripTest::RunTest(const FString& Parameters)
{
	// A second of each signal at 22 050 Hz, encoded and decoded as the SPU2 decodes it (measured: a 440 Hz tone 56 dB
	// above its noise, a full-scale 3 kHz one 32 dB, a quiet one exact, loud white noise, which no filter predicts,
	// 23.5 dB, a sparse click track 25.6 dB); silence stays exact. The blocks are 16 bytes for 28 samples: 3.5 times
	// smaller than PCM16.
	struct FCase
	{
		const TCHAR* Name;
		TArray<int16> Samples;
		float MinSnr;
	};
	TArray<FCase> Cases;
	Cases.Add({TEXT("Sine 440 Hz, half scale"), MakeSine(TestRate, 440.0f, TestRate, 0.5f), 50.0f});
	Cases.Add({TEXT("Sine 3 kHz, full scale"), MakeSine(TestRate, 3000.0f, TestRate, 0.99f), 30.0f});
	Cases.Add({TEXT("Sine 50 Hz, quiet"), MakeSine(TestRate, 50.0f, TestRate, 0.01f), 60.0f});
	TArray<int16> Noise;
	FTestNoise Random;
	for (int32 Frame = 0; Frame < TestRate; ++Frame)
	{
		Noise.Add(int16(FMath::RoundToInt(Random.Next() * 16000.0f)));
	}
	Cases.Add({TEXT("White noise"), Noise, 20.0f});
	TArray<int16> Clicks;
	Clicks.SetNumZeroed(TestRate);
	for (int32 Frame = 100; Frame < TestRate; Frame += 997)
	{
		Clicks[Frame] = (Frame / 997) % 2 == 0 ? int16(20000) : int16(-24000);
		Clicks[Frame + 1] = int16(-Clicks[Frame] / 2);
	}
	Cases.Add({TEXT("Clicks"), Clicks, 23.0f});
	for (const FCase& Case : Cases)
	{
		TArray<uint8> Blocks;
		const TArray<int16> Decoded = RoundTrip(Case.Samples, &Blocks);
		const float Snr = GetSnr(Case.Samples, Decoded, Case.Samples.Num());
		UE_LOG(LogTemp, Display, "%s: SNR %.1f dB", Case.Name, double(Snr));
		TestTrue(*FString::Printf(TEXT("%s: %.1f dB, at least %.0f"), Case.Name, double(Snr), double(Case.MinSnr)),
			Snr >= Case.MinSnr);
		TestEqual(*FString::Printf(TEXT("%s: 16 bytes a block"), Case.Name), Blocks.Num(),
			((TestRate + 27) / 28) * FSpuAdpcm::BytesPerBlock);
	}

	TArray<int16> Silence;
	Silence.SetNumZeroed(1000);
	const TArray<int16> DecodedSilence = RoundTrip(Silence);
	bool bSilent = DecodedSilence.Num() == 1008;
	for (const int16 Sample : DecodedSilence)
	{
		bSilent &= Sample == 0;
	}
	TestTrue("Silence stays silent (and the last block is padded with it)", bSilent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpuAdpcmEncoderLoopTest, "System.AudioCompressor.SpuAdpcm.LoopFlags",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSpuAdpcmEncoderLoopTest::RunTest(const FString& Parameters)
{
	// A one-shot ends with the end flag alone. A loop starts on a block (silence ahead of the sound moves it there),
	// which has the loop start flag and no prediction; every block from it repeats, and the last ends with a jump back.
	const TArray<int16> Tone = MakeSine(100, 1000.0f, TestRate, 0.5f);
	FSpuAdpcmSettings OneShot;
	OneShot.SampleRate = TestRate;
	FSpuAdpcmCompressed Compressed;
	TestTrue("Compressed", FSpuAdpcmEncoder::Compress(Tone.GetData(), Tone.Num(), 1, TestRate, OneShot, Compressed));
	TestEqual("4 blocks", Compressed.Blocks.Num(), 4 * FSpuAdpcm::BytesPerBlock);
	TestTrue("One-shot flags",
		Compressed.Blocks[1] == 0 && Compressed.Blocks[17] == 0 && Compressed.Blocks[33] == 0 &&
			Compressed.Blocks[49] == FSpuAdpcm::FlagLoopEnd);
	TestEqual("No loop", Compressed.LoopStartFrame, int32(INDEX_NONE));

	FSpuAdpcmSettings Looping = OneShot;
	Looping.bLooping = true;
	Looping.LoopStartFrame = 30;
	TestTrue(
		"Compressed looping", FSpuAdpcmEncoder::Compress(Tone.GetData(), Tone.Num(), 1, TestRate, Looping, Compressed));
	// 30 frames ahead of the loop: 26 of silence in front put its start on frame 56, block 2; its 70 frames become 84
	// (whole blocks): 5 blocks.
	TestEqual("The loop starts on a block", Compressed.LoopStartFrame, 56);
	TestEqual("5 blocks", Compressed.Blocks.Num(), 5 * FSpuAdpcm::BytesPerBlock);
	const uint8 Repeat = FSpuAdpcm::FlagRepeat;
	TestTrue("Loop flags",
		Compressed.Blocks[1] == 0 && Compressed.Blocks[17] == 0 &&
			Compressed.Blocks[33] == (FSpuAdpcm::FlagLoopStart | Repeat) && Compressed.Blocks[49] == Repeat &&
			Compressed.Blocks[65] == (FSpuAdpcm::FlagLoopEnd | Repeat));
	TestEqual("The loop's first block predicts nothing", Compressed.Blocks[32] >> 4, 0);
	TestTrue("A valid looping sound", Compressed.GetSound().IsValid() && Compressed.GetSound().IsLooping());
	TArray<int16> Decoded;
	FSpuAdpcm::Decode(Compressed.GetSound(), Decoded);
	bool bSilentAhead = true;
	for (int32 Frame = 0; Frame < 26; ++Frame)
	{
		bSilentAhead &= Decoded[Frame] == 0;
	}
	TestTrue("Silence ahead", bSilentAhead);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpuAdpcmEncoderRateTest, "System.AudioCompressor.SpuAdpcm.RateAndChannels",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSpuAdpcmEncoderRateTest::RunTest(const FString& Parameters)
{
	// The rate goes down, never up, and at most to the SPU2's 48 kHz.
	TestEqual("44.1 kHz for effects", FSpuAdpcmEncoder::GetTargetSampleRate(44100, 22050), 22050);
	TestEqual("A lower source keeps its rate", FSpuAdpcmEncoder::GetTargetSampleRate(11025, 22050), 11025);
	TestEqual("0 keeps the source's", FSpuAdpcmEncoder::GetTargetSampleRate(44100, 0), 44100);
	TestEqual("At most 48 kHz", FSpuAdpcmEncoder::GetTargetSampleRate(96000, 0), 48000);

	// A 1 kHz tone at 44.1 kHz resampled to 22.05 kHz is the tone at 22.05 kHz; a 15 kHz one, above the new Nyquist
	// (11 025 Hz), is filtered out instead of folding back to 7 kHz.
	const TArray<int16> Tone = MakeSine(44100, 1000.0f, 44100, 0.5f);
	TArray<int16> Resampled;
	FSpuAdpcmEncoder::ResampleSegment(Tone.GetData(), Tone.Num(), 0, Tone.Num(), 22050, false, Resampled);
	const TArray<int16> Ideal = MakeSine(22050, 1000.0f, 22050, 0.5f);
	const float ToneSnr = GetSnr(Ideal, Resampled, 22050, 64);
	UE_LOG(LogTemp, Display, "1 kHz resampled: SNR %.1f dB", double(ToneSnr));
	TestTrue(*FString::Printf(TEXT("The tone survives: %.1f dB"), double(ToneSnr)), ToneSnr > 70.0f);
	const TArray<int16> High = MakeSine(44100, 15000.0f, 44100, 0.5f);
	FSpuAdpcmEncoder::ResampleSegment(High.GetData(), High.Num(), 0, High.Num(), 22050, false, Resampled);
	double Energy = 0.0;
	for (int32 Frame = 64; Frame < 22050 - 64; ++Frame)
	{
		Energy += double(Resampled[Frame]) * double(Resampled[Frame]);
	}
	const float Rms = FMath::Sqrt(float(Energy / double(22050 - 128)));
	const float Attenuation = 20.0f * FMath::Loge(Rms / (0.5f * 32767.0f / FMath::Sqrt(2.0f))) / FMath::Loge(10.0f);
	UE_LOG(LogTemp, Display, "15 kHz resampled: %.1f dB", double(Attenuation));
	TestTrue(
		*FString::Printf(TEXT("Above the new Nyquist, filtered: %.1f dB"), double(Attenuation)), Attenuation < -50.0f);

	// Stereo is averaged into mono; a 44.1 kHz source becomes 22.05 kHz, half the frames.
	const TArray<int16> Stereo = {1000, 3000, -1000, -2001, 5, 6};
	TArray<int16> Mono;
	FSpuAdpcmEncoder::DownmixToMono(Stereo.GetData(), 3, 2, Mono);
	TestTrue("Averaged", Mono.Num() == 3 && Mono[0] == 2000 && Mono[1] == -1501 && Mono[2] == 6);
	FSpuAdpcmCompressed Compressed;
	FSpuAdpcmSettings Settings;
	TArray<int16> StereoTone;
	for (const int16 Sample : Tone)
	{
		StereoTone.Add(Sample);
		StereoTone.Add(Sample);
	}
	TestTrue(
		"Compressed", FSpuAdpcmEncoder::Compress(StereoTone.GetData(), Tone.Num(), 2, 44100, Settings, Compressed));
	TestTrue("22.05 kHz mono", Compressed.SampleRate == 22050 && Compressed.GetSound().GetNumFrames() == 22064);
	TArray<int16> Decoded;
	FSpuAdpcm::Decode(Compressed.GetSound(), Decoded);
	const float CompressedSnr = GetSnr(Ideal, Decoded, 22050, 64);
	UE_LOG(LogTemp, Display, "1 kHz stereo 44.1 kHz to ADPCM 22.05 kHz: SNR %.1f dB", double(CompressedSnr));
	TestTrue(*FString::Printf(TEXT("Resampled and encoded: %.1f dB"), double(CompressedSnr)), CompressedSnr > 45.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpuAdpcmEncoderDeterminismTest, "System.AudioCompressor.SpuAdpcm.Deterministic",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSpuAdpcmEncoderDeterminismTest::RunTest(const FString& Parameters)
{
	// The same samples give the same bytes, run after run and build after build (the encoder is integers only: the
	// CRC is pinned); the resampling path too, within a run.
	TArray<int16> Samples = MakeSine(4000, 440.0f, TestRate, 0.5f);
	FTestNoise Random;
	for (int16& Sample : Samples)
	{
		Sample = int16(FMath::Clamp(int32(Sample) + FMath::RoundToInt(Random.Next() * 2000.0f), -32768, 32767));
	}
	TArray<uint8> First;
	TArray<uint8> Second;
	FSpuAdpcmEncoder::EncodeBlocks(Samples.GetData(), Samples.Num(), INDEX_NONE, First);
	FSpuAdpcmEncoder::EncodeBlocks(Samples.GetData(), Samples.Num(), INDEX_NONE, Second);
	TestTrue("Twice the same", First == Second);
	const uint32 Crc = FCrc::MemCrc32(First.GetData(), First.Num());
	UE_LOG(LogTemp, Display, "CRC 0x%08x", Crc);
	TestEqual("The pinned bytes", Crc, 622329765u);

	FSpuAdpcmSettings Settings;
	Settings.bLooping = true;
	Settings.LoopStartFrame = 1234;
	FSpuAdpcmCompressed A;
	FSpuAdpcmCompressed B;
	TestTrue("A", FSpuAdpcmEncoder::Compress(Samples.GetData(), Samples.Num(), 1, 44100, Settings, A));
	TestTrue("B", FSpuAdpcmEncoder::Compress(Samples.GetData(), Samples.Num(), 1, 44100, Settings, B));
	TestTrue("Resampled and looped: twice the same",
		A.Blocks == B.Blocks && A.LoopStartFrame == B.LoopStartFrame && A.SampleRate == 22050);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
