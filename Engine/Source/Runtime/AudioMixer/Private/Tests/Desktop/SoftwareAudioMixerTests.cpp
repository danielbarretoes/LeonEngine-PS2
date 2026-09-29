#include "CoreMinimal.h"
#include "Desktop/SoftwareAudioMixer.h"
#include "Misc/AutomationTest.h"
#include "SpuAdpcm.h"

#if WITH_DEV_AUTOMATION_TESTS

// The desktop's stand-in for the SPU2's voices (Docs/PLANS/ps2-shipping.md N19): gains, the pitch, the end of a
// one-shot, a loop from its loop start, the voice limit and clipping.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoftwareAudioMixerVoicesTest, "System.AudioMixer.SoftwareMixer.Voices",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSoftwareAudioMixerVoicesTest::RunTest(const FString& Parameters)
{
	FSoftwareAudioMixer Mixer(48000);
	TArray<int16> Out;
	Out.SetNumZeroed(8 * 2);

	// A one-shot at 48 kHz (pitch 4096), half on the left, a quarter on the right; it ends with its samples.
	TArray<int16> Constant;
	Constant.Init(1000, 4);
	Mixer.Play(0, Constant.GetData(), Constant.Num(), INDEX_NONE, FSpuAdpcm::PitchOne, 0.5f, 0.25f);
	TestTrue("Playing", Mixer.IsPlaying(0));
	Mixer.Mix(Out.GetData(), 8);
	TestTrue("Its gains", Out[0] == 500 && Out[1] == 250 && Out[6] == 500 && Out[7] == 250);
	TestTrue("Silence after its end", Out[8] == 0 && Out[15] == 0);
	TestFalse("A one-shot stops", Mixer.IsPlaying(0));

	// A loop at 24 kHz (pitch 2048: each source frame spans two output frames) from its second frame: 0, 1000, 2000,
	// then back to 1000.
	const TArray<int16> Ramp = {0, 1000, 2000};
	Mixer.Play(1, Ramp.GetData(), Ramp.Num(), 1, FSpuAdpcm::GetPitch(24000), 1.0f, 1.0f);
	Mixer.Mix(Out.GetData(), 8);
	TestTrue("Frame 0", Out[0] == 0);
	TestTrue("Between 0 and 1", Out[2] == 500);
	TestTrue("Frame 1", Out[4] == 1000);
	TestTrue("Frame 2", Out[8] == 2000);
	TestTrue("Between 2 and the loop start", Out[10] == 1500);
	TestTrue("The loop start again", Out[12] == 1000);
	TestTrue("A loop keeps playing", Mixer.IsPlaying(1));
	Mixer.Stop(1);
	TestEqual("Stopped", Mixer.GetNumPlaying(), 0);

	// Two loud voices clip instead of wrapping; a voice's gains change while it plays.
	TArray<int16> Loud;
	Loud.Init(30000, 8);
	Mixer.Play(0, Loud.GetData(), Loud.Num(), INDEX_NONE, FSpuAdpcm::PitchOne, 1.0f, 1.0f);
	Mixer.Play(1, Loud.GetData(), Loud.Num(), INDEX_NONE, FSpuAdpcm::PitchOne, 1.0f, 1.0f);
	Mixer.Mix(Out.GetData(), 1);
	TestEqual("Clipped", int32(Out[0]), 32767);
	Mixer.SetGains(1, 0.0f, 0.0f);
	Mixer.SetGains(0, 0.25f, 0.5f);
	Mixer.Mix(Out.GetData(), 1);
	TestTrue("New gains", Out[0] == 7500 && Out[1] == 15000);
	TestEqual("24 voices", FSoftwareAudioMixer::MaxVoices, 24);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoftwareAudioMixerPitchTest, "System.AudioMixer.SoftwareMixer.SpuPitch",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSoftwareAudioMixerPitchTest::RunTest(const FString& Parameters)
{
	// A 44.1 kHz loop for ten minutes plays at the SPU2's pitch for it (3763 / 4096 of 48 kHz: 44 097.7 Hz), exactly:
	// the voice's 16.16 step is the pitch's 4.12 (the EE's native integers, no rounding to drift). The loop is a ramp
	// of its frame numbers, so the output reads the position back.
	constexpr int32 SourceRate = 44100;
	constexpr int32 OutputRate = 48000;
	constexpr int32 Seconds = 600;
	TArray<int16> Ramp;
	Ramp.SetNumUninitialized(SourceRate);
	for (int32 Frame = 0; Frame < SourceRate; ++Frame)
	{
		Ramp[Frame] = int16(Frame - (SourceRate / 2));
	}
	const int32 Pitch = FSpuAdpcm::GetPitch(SourceRate);
	TestEqual("The SPU2's pitch for 44.1 kHz", Pitch, 3763);
	FSoftwareAudioMixer Mixer(OutputRate);
	Mixer.Play(0, Ramp.GetData(), Ramp.Num(), 0, Pitch, 1.0f, 1.0f);
	constexpr int32 Chunk = OutputRate / 10;
	TArray<int16> Out;
	Out.SetNumUninitialized(Chunk * 2);
	for (int32 Mixed = 0; Mixed < OutputRate * Seconds; Mixed += Chunk)
	{
		Mixer.Mix(Out.GetData(), Chunk);
	}
	Mixer.Mix(Out.GetData(), 1);
	// Frame N of the output is at N * Pitch / 4096 source frames: its whole part, and the ramp between it and the next
	// frame for the rest (a quarter frame here).
	const int64 Expected = (int64(OutputRate) * Seconds * Pitch / FSpuAdpcm::PitchOne) % SourceRate;
	const int32 Position = int32(Out[0]) + (SourceRate / 2);
	TestTrue(
		*FString::Printf(TEXT("At %d after ten minutes, the SPU2 at %lld"), Position, static_cast<long long>(Expected)),
		Position >= Expected && Position <= Expected + 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
