#include "AudioDevice.h"
#include "AudioOutput.h"
#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** The frames the device queued, as an output of the test's. */
	TArray<int16> GQueued;
	int32 GStartRate = 0;

	class FCapturingOutput final : public FAudioOutput
	{
	public:
		bool Start(int32 SampleRate) override
		{
			GStartRate = SampleRate;
			return true;
		}
		void Queue(const int16* Frames, int32 NumFrames) override
		{
			GQueued.Append(Frames, NumFrames * 2);
		}
	};

	TUniquePtr<FAudioOutput> MakeCapturingOutput()
	{
		return MakeUnique<FCapturingOutput>();
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioDeviceQueuesTheMixTest, "System.AudioMixer.Device.QueuesTheMix",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAudioDeviceQueuesTheMixTest::RunTest(const FString& Parameters)
{
	// The device opens its output at 48 kHz, queues 66 ms of silence ahead, then each tick the mix of what plays
	// (every platform, Docs/PLANS/ps2-preview.md V1): a sound played at full volume comes out as its samples.
	GQueued.Reset();
	SetAudioOutputFactory(&MakeCapturingOutput);
	FAudioDevice Audio;
	const bool bStarted = Audio.Initialize(false);
	SetAudioOutputFactory(nullptr);
	if (!TestTrue("Started", bStarted) || !TestFalse("Not silent", Audio.IsSilent()))
	{
		return false;
	}
	TestEqual("48 kHz", GStartRate, 48000);
	TestEqual("The lead: 3200 frames of silence", GQueued.Num(), 3200 * 2);
	TArray<int16> Samples;
	Samples.Init(1234, 48000);
	FSoundWavePCM Sound;
	Sound.Samples = Samples.GetData();
	Sound.NumFrames = Samples.Num();
	Sound.NumChannels = 1;
	Sound.SampleRate = 48000;
	Audio.PlaySound2D(Sound);
	FPlatformProcess::Sleep(0.02f);
	Audio.Tick();
	TestTrue("A tick queues what played since the last", GQueued.Num() > 3200 * 2);
	TestTrue("The sound, both sides",
		GQueued.Num() > (3200 * 2) + 1 && GQueued[3200 * 2] == 1234 && GQueued[(3200 * 2) + 1] == 1234);
	Audio.Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
