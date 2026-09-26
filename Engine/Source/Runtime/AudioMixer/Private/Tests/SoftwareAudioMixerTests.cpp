#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "SoftwareAudioMixer.h"

#if WITH_DEV_AUTOMATION_TESTS

// The CPU mixer the PS2's audio device streams to the SPU2 (Docs/PLANS/ps2-engine.md E5): volumes, resampling, the end
// of a one-shot and a loop, the spatialized gains, the voice limit and clipping.

namespace
{

	FSoundWavePCM MakeSound(const TArray<int16>& Samples, int32 NumChannels, int32 SampleRate)
	{
		FSoundWavePCM Sound;
		Sound.Samples = Samples.GetData();
		Sound.NumChannels = NumChannels;
		Sound.NumFrames = Samples.Num() / NumChannels;
		Sound.SampleRate = SampleRate;
		return Sound;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoftwareAudioMixerVoicesTest, "System.AudioMixer.SoftwareMixer.Voices",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSoftwareAudioMixerVoicesTest::RunTest(const FString& Parameters)
{
	FSoftwareAudioMixer Mixer(48000);
	TArray<int16> Out;
	Out.SetNumZeroed(8 * 2);

	// A mono one-shot at the output rate, at half volume on both sides; it ends with its samples.
	TArray<int16> Constant;
	Constant.Init(1000, 4);
	const int32 Voice = Mixer.Play(MakeSound(Constant, 1, 48000), 0.5f, false);
	TestTrue("Playing", Mixer.IsPlaying(Voice));
	Mixer.Mix(Out.GetData(), 8);
	TestTrue("Half volume, both sides", Out[0] == 500 && Out[1] == 500 && Out[6] == 500 && Out[7] == 500);
	TestTrue("Silence after its end", Out[8] == 0 && Out[15] == 0);
	TestFalse("A one-shot stops", Mixer.IsPlaying(Voice));

	// A stereo loop at half the output rate: each source frame spans two output frames, the second one between it and
	// the next, and the loop goes back to its first frame.
	const TArray<int16> Ramp = {0, -100, 1000, -300};
	const int32 Loop = Mixer.Play(MakeSound(Ramp, 2, 24000), 1.0f, true);
	Mixer.Mix(Out.GetData(), 8);
	TestTrue("Frame 0", Out[0] == 0 && Out[1] == -100);
	TestTrue("Between 0 and 1", Out[2] == 500 && Out[3] == -200);
	TestTrue("Frame 1", Out[4] == 1000 && Out[5] == -300);
	TestTrue("Between 1 and 0 (the loop)", Out[6] == 500 && Out[7] == -200);
	TestTrue("Frame 0 again", Out[8] == 0 && Out[9] == -100);
	TestTrue("A loop keeps playing", Mixer.IsPlaying(Loop));
	Mixer.Stop(Loop);
	TestEqual("Stopped", Mixer.GetNumPlaying(), 0);

	// Two loud voices clip instead of wrapping; the master volume scales the sum.
	TArray<int16> Loud;
	Loud.Init(30000, 8);
	(void)Mixer.Play(MakeSound(Loud, 1, 48000), 1.0f, false);
	(void)Mixer.Play(MakeSound(Loud, 1, 48000), 1.0f, false);
	Mixer.Mix(Out.GetData(), 1);
	TestEqual("Clipped", int32(Out[0]), 32767);
	Mixer.SetMasterVolume(0.25f);
	Mixer.Mix(Out.GetData(), 1);
	TestEqual("A quarter of the sum", int32(Out[0]), 15000);
	Mixer.SetMasterVolume(1.0f);

	// The voice limit.
	TArray<int16> Long;
	Long.Init(1, 48000);
	int32 Started = 0;
	for (int32 Index = 0; Index < FSoftwareAudioMixer::MaxVoices + 2; ++Index)
	{
		Started += Mixer.Play(MakeSound(Long, 1, 48000), 1.0f, false) != INDEX_NONE ? 1 : 0;
	}
	TestEqual("At most MaxVoices", Started + 2, FSoftwareAudioMixer::MaxVoices);
	TestEqual("An invalid sound starts nothing", Mixer.Play(FSoundWavePCM(), 1.0f, false), int32(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoftwareAudioMixerSpatialTest, "System.AudioMixer.SoftwareMixer.Spatialized",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSoftwareAudioMixerSpatialTest::RunTest(const FString& Parameters)
{
	// A listener at the origin looking down +X (right is +Y): a sound 5 m to the right is a fifth as loud, all in the
	// right ear; one 50 cm ahead is at full volume in both; one 2 m to the left is half as loud, in the left ear.
	FSoftwareAudioMixer Mixer(48000);
	Mixer.SetListener(FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
	TArray<int16> Constant;
	Constant.Init(1000, 4);
	TArray<int16> Out;
	Out.SetNumZeroed(2);
	const auto Hear = [&](const FVector& Location)
	{
		const int32 Voice = Mixer.Play(MakeSound(Constant, 1, 48000), 1.0f, false, true, Location);
		Mixer.Mix(Out.GetData(), 1);
		Mixer.Stop(Voice);
	};
	Hear(FVector(0.0f, 500.0f, 0.0f));
	TestTrue("5 m right", Out[0] == 0 && Out[1] == 200);
	Hear(FVector(50.0f, 0.0f, 0.0f));
	TestTrue("50 cm ahead", Out[0] == 1000 && Out[1] == 1000);
	Hear(FVector(0.0f, -200.0f, 0.0f));
	TestTrue("2 m left", Out[0] == 500 && Out[1] == 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
