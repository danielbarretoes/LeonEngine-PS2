#include "AudioDevice.h"
#include "CoreMinimal.h"
#include "Desktop/AudioOutput.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** The frames the desktop's hardware queued, as an output of the test's. */
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoftwareAudioHardwareQueuesTheMixTest, "System.AudioMixer.Device.QueuesTheMix",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSoftwareAudioHardwareQueuesTheMixTest::RunTest(const FString& Parameters)
{
	// The desktop's hardware opens its output at the SPU2's 48 kHz, queues 66 ms of silence ahead, then each tick the
	// mix of its voices (Docs/PLANS/ps2-shipping.md N19): an ADPCM buffer decoded as the SPU2 decodes it, played at
	// full volume, comes out as its samples on both sides.
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
	// Filter 0, shift 2, every nibble 1: each sample is (1 << 12) >> 2 = 1024; a second of blocks at 48 kHz.
	constexpr int32 NumBlocks = 48000 / FSpuAdpcm::SamplesPerBlock;
	TArray<uint8> Blocks;
	Blocks.SetNumZeroed(NumBlocks * FSpuAdpcm::BytesPerBlock);
	for (int32 Block = 0; Block < NumBlocks; ++Block)
	{
		uint8* Data = Blocks.GetData() + (Block * FSpuAdpcm::BytesPerBlock);
		Data[0] = 0x02;
		Data[1] = Block == NumBlocks - 1 ? FSpuAdpcm::FlagLoopEnd : 0;
		FMemory::Memset(Data + 2, 0x11, FSpuAdpcm::BytesPerBlock - 2);
	}
	FSpuAdpcmSound Sound;
	Sound.Blocks = Blocks.GetData();
	Sound.NumBlocks = NumBlocks;
	Sound.SampleRate = 48000;
	const int32 Buffer = Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_Constant")), Sound);
	TestTrue("A buffer", Buffer != INDEX_NONE);
	Audio.PlaySound2D(Buffer);
	FPlatformProcess::Sleep(0.02f);
	Audio.Tick();
	TestTrue("A tick queues what played since the last", GQueued.Num() > 3200 * 2);
	TestTrue("The sound, both sides",
		GQueued.Num() > (3200 * 2) + 1 && GQueued[3200 * 2] == 1024 && GQueued[(3200 * 2) + 1] == 1024);
	TestEqual("One voice", Audio.GetNumPlayingVoices(), 1);
	Audio.Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
