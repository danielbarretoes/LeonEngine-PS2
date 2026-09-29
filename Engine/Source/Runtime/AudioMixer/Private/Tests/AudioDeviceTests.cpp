#include "AudioDevice.h"
#include "AudioHardware.h"
#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// FAudioDevice's decisions, the same on every platform (Docs/PLANS/ps2-shipping.md N19), against a hardware of the
// test's that records what it is asked: the buffers in SPU2 RAM, the voices and their priorities, a voice the hardware
// refuses, the music's voices and the spatialized volumes.

namespace
{

	struct FHardwareLog
	{
		TArray<int32> Uploads;
		TArray<int32> Frees;
		/** Voice, buffer. */
		TArray<TPair<int32, int32>> Plays;
		TArray<TPair<int32, FSpuVoiceVolume>> Volumes;
		TArray<int32> Stops;
		/** Voices the hardware refuses once (still playing for the SPU2). */
		TArray<int32> Refuse;
	};

	FHardwareLog GHardwareLog;

	class FLoggingHardware final : public FAudioHardware
	{
	public:
		bool Start() override
		{
			return true;
		}
		bool UploadSound(int32 Buffer, const FSpuAdpcmSound& Sound, int32 Pitch) override
		{
			(void)Sound;
			(void)Pitch;
			GHardwareLog.Uploads.Add(Buffer);
			return true;
		}
		void FreeSound(int32 Buffer) override
		{
			GHardwareLog.Frees.Add(Buffer);
		}
		bool PlayVoice(int32 Voice, int32 Buffer, const FSpuVoiceVolume& Volume) override
		{
			if (GHardwareLog.Refuse.Remove(Voice) > 0)
			{
				return false;
			}
			GHardwareLog.Plays.Add(TPair<int32, int32>(Voice, Buffer));
			GHardwareLog.Volumes.Add(TPair<int32, FSpuVoiceVolume>(Voice, Volume));
			return true;
		}
		void SetVoiceVolume(int32 Voice, const FSpuVoiceVolume& Volume) override
		{
			GHardwareLog.Volumes.Add(TPair<int32, FSpuVoiceVolume>(Voice, Volume));
		}
		void StopVoice(int32 Voice) override
		{
			GHardwareLog.Stops.Add(Voice);
		}
	};

	TUniquePtr<FAudioHardware> MakeLoggingHardware()
	{
		return MakeUnique<FLoggingHardware>();
	}

	/** A device on the logging hardware, started from a clean log. */
	struct FScopedLoggingDevice
	{
		FAudioDevice Device;
		bool bStarted = false;

		FScopedLoggingDevice()
		{
			GHardwareLog = FHardwareLog();
			SetAudioHardwareFactory(&MakeLoggingHardware);
			bStarted = Device.Initialize(false);
			SetAudioHardwareFactory(nullptr);
		}
	};

	/** NumBlocks silent blocks (the last with the end flag) at SampleRate. */
	struct FTestSound
	{
		TArray<uint8> Blocks;
		FSpuAdpcmSound Sound;

		FTestSound(int32 NumBlocks, int32 SampleRate, int32 LoopStartFrame = INDEX_NONE)
		{
			Blocks.SetNumZeroed(NumBlocks * FSpuAdpcm::BytesPerBlock);
			Blocks[((NumBlocks - 1) * FSpuAdpcm::BytesPerBlock) + 1] = FSpuAdpcm::FlagLoopEnd;
			Sound.Blocks = Blocks.GetData();
			Sound.NumBlocks = NumBlocks;
			Sound.SampleRate = SampleRate;
			Sound.LoopStartFrame = LoopStartFrame;
		}
	};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioDeviceSoundBuffersTest, "System.AudioMixer.Device.SoundBuffers",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAudioDeviceSoundBuffersTest::RunTest(const FString& Parameters)
{
	// A sound is uploaded once and shared by its name; the SPU2 RAM is a stack, so only the unreferenced buffers on
	// top make room, and a sound that does not fit is an error that names it.
	FScopedLoggingDevice Scope;
	FAudioDevice& Audio = Scope.Device;
	if (!TestTrue("Started", Scope.bStarted))
	{
		return false;
	}
	const FTestSound Small(10, 22050);
	const int32 A = Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_A")), Small.Sound);
	TestTrue("Uploaded", A != INDEX_NONE && GHardwareLog.Uploads.Num() == 1);
	TestEqual("Shared by its name", Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_A")), Small.Sound), A);
	TestEqual("Uploaded once", GHardwareLog.Uploads.Num(), 1);
	TestEqual("160 bytes", Audio.GetSoundRamUsed(), 160);

	// A sound that leaves room for 10 blocks only, then one of 20: it fits only once the big one is unreferenced.
	const FTestSound Big((FSpuAdpcm::SoundRamBytes / FSpuAdpcm::BytesPerBlock) - 20, 22050);
	const int32 B = Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_Big")), Big.Sound);
	TestTrue("The big one", B != INDEX_NONE);
	const FTestSound Medium(20, 22050);
	AddExpectedError(TEXT("bytes of SPU2 RAM"), 1);
	TestEqual("No room while the big one is referenced",
		Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_C")), Medium.Sound), int32(INDEX_NONE));
	Audio.ReleaseSoundBuffer(B);
	TestEqual("Still resident unreferenced", Audio.GetNumSoundBuffers(), 2);
	const int32 C = Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_C")), Medium.Sound);
	TestTrue("Room made from the top", C != INDEX_NONE && GHardwareLog.Frees.Num() == 1 && GHardwareLog.Frees[0] == B);
	TestEqual("A and C", Audio.GetSoundRamUsed(), 160 + 320);

	// A is below C: releasing it frees nothing until C goes too.
	Audio.ReleaseSoundBuffer(A);
	Audio.ReleaseSoundBuffer(A);
	AddExpectedError(TEXT("bytes of SPU2 RAM"), 1);
	TestEqual(
		"A cannot go from under C", Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_Big")), Big.Sound), int32(INDEX_NONE));
	Audio.ReleaseSoundBuffer(C);
	TestTrue("C goes for the big one; A, unreferenced, stays since the big one fits over it",
		Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_Big")), Big.Sound) != INDEX_NONE &&
			GHardwareLog.Frees.Num() == 2 && GHardwareLog.Frees[1] == C && Audio.GetNumSoundBuffers() == 2);
	TestEqual(
		"An invalid sound", Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_None")), FSpuAdpcmSound()), int32(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioDeviceVoicesTest, "System.AudioMixer.Device.Voices",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAudioDeviceVoicesTest::RunTest(const FString& Parameters)
{
	// Plays start at the next tick. A default priority sound leaves the last 4 effect voices free, a higher one takes
	// them; the last 2 voices are the music's; a sound that finds no voice is dropped (audsrv cannot stop one), and a
	// voice is free again once its sound's time has passed.
	FScopedLoggingDevice Scope;
	FAudioDevice& Audio = Scope.Device;
	if (!TestTrue("Started", Scope.bStarted))
	{
		return false;
	}
	const FTestSound Long(3000, 48000);
	const int32 Buffer = Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_Long")), Long.Sound);
	for (int32 Index = 0; Index < 30; ++Index)
	{
		Audio.PlaySound2D(Buffer);
	}
	TestEqual("Queued, not started", GHardwareLog.Plays.Num(), 0);
	Audio.Tick();
	TestEqual("18 of 22 effect voices for default sounds", GHardwareLog.Plays.Num(), 18);
	TestTrue("In voice order", GHardwareLog.Plays[0].Key == 0 && GHardwareLog.Plays[17].Key == 17);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Audio.PlaySound2D(Buffer, 1.0f, 2.0f);
	}
	Audio.Tick();
	TestEqual("The 4 kept voices for higher ones", GHardwareLog.Plays.Num(), 22);
	TestEqual("22 busy", Audio.GetNumPlayingVoices(), 22);

	Audio.PlayMusic(Buffer);
	TestTrue("Music queued", Audio.IsMusicPlaying());
	Audio.Tick();
	TestTrue("On a music voice", GHardwareLog.Plays.Num() == 23 && GHardwareLog.Plays.Last().Key >= 22);
	TestTrue("Music plays", Audio.IsMusicPlaying());
	Audio.StopMusic();
	TestFalse("Stopped", Audio.IsMusicPlaying());
	TestTrue(
		"Its voice stopped", GHardwareLog.Stops.Num() == 1 && GHardwareLog.Stops[0] == GHardwareLog.Plays.Last().Key);
	TestEqual("Busy until its sound's end (the PS2 mutes it)", Audio.GetNumPlayingVoices(), 23);
	Audio.PlayMusic(Buffer);
	Audio.Tick();
	TestTrue("The other music voice",
		GHardwareLog.Plays.Num() == 24 && GHardwareLog.Plays.Last().Key != GHardwareLog.Stops[0]);

	// A short sound frees its voice after its length (28 frames at 48 kHz) and the margin.
	FScopedLoggingDevice ShortScope;
	FAudioDevice& ShortAudio = ShortScope.Device;
	const FTestSound Short(1, 48000);
	const int32 ShortBuffer = ShortAudio.AcquireSoundBuffer(FName(TEXT("/Test/S_Short")), Short.Sound);
	ShortAudio.PlaySoundAtLocation(ShortBuffer, FVector::ZeroVector);
	ShortAudio.Tick();
	TestEqual("Playing", ShortAudio.GetNumPlayingVoices(), 1);
	FPlatformProcess::Sleep(0.02f);
	ShortAudio.Tick();
	TestEqual("Free again", ShortAudio.GetNumPlayingVoices(), 0);

	// The voice free the longest goes first (voice 0 just played: voice 1); one the hardware still plays is skipped for
	// the next.
	GHardwareLog.Plays.Reset();
	GHardwareLog.Refuse.Add(1);
	ShortAudio.PlaySound2D(ShortBuffer);
	ShortAudio.Tick();
	TestTrue("The next voice", GHardwareLog.Plays.Num() == 1 && GHardwareLog.Plays[0].Key == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioDeviceSpatialTest, "System.AudioMixer.Device.Spatialized",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAudioDeviceSpatialTest::RunTest(const FString& Parameters)
{
	// A listener at the origin looking down +X (right is +Y): a sound 5 m to the right is a fifth as loud, all in the
	// right ear (audsrv's nearest level); turning around moves it to the left ear, sent once; an unchanged listener
	// sends nothing. The master volume scales every voice.
	FScopedLoggingDevice Scope;
	FAudioDevice& Audio = Scope.Device;
	const FTestSound Long(3000, 48000);
	const int32 Buffer = Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_Long")), Long.Sound);
	Audio.SetListener(FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
	Audio.PlaySoundAtLocation(Buffer, FVector(0.0f, 500.0f, 0.0f));
	Audio.PlaySound2D(Buffer, 0.5f);
	Audio.Tick();
	if (!TestEqual("Two plays", GHardwareLog.Volumes.Num(), 2))
	{
		return false;
	}
	const FSpuVoiceVolume Right = GHardwareLog.Volumes[0].Value;
	TestTrue("5 m right", Right == FSpuVoiceVolume::FromGains(0.0f, 0.2f) && Right.GetLeftLevel() == 0);
	TestTrue("2D at half", GHardwareLog.Volumes[1].Value == FSpuVoiceVolume::FromGains(0.5f, 0.5f));
	Audio.SetListener(FVector::ZeroVector, FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
	Audio.Tick();
	TestTrue("Turned around: the left ear",
		GHardwareLog.Volumes.Num() == 3 && GHardwareLog.Volumes[2].Value == FSpuVoiceVolume::FromGains(0.2f, 0.0f));
	Audio.Tick();
	TestEqual("Nothing new to send", GHardwareLog.Volumes.Num(), 3);
	Audio.SetMasterVolume(0.5f);
	Audio.Tick();
	TestTrue("The master volume, on both voices",
		GHardwareLog.Volumes.Num() == 5 && GHardwareLog.Volumes[4].Value == FSpuVoiceVolume::FromGains(0.25f, 0.25f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioDeviceLowPriorityTest, "System.AudioMixer.Device.LowPriorityAndInaudible",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAudioDeviceLowPriorityTest::RunTest(const FString& Parameters)
{
	// ps2-shipping N30f: a sound below the default priority (a step, an impact) leaves the last 10 effect voices free,
	// so the shots still find 6 and the important sounds the 4 kept after them; a spatialized sound too far to be heard
	// (audsrv's level 0 on both sides: past about 218 m at full volume) takes no voice.
	FScopedLoggingDevice Scope;
	FAudioDevice& Audio = Scope.Device;
	if (!TestTrue("Started", Scope.bStarted))
	{
		return false;
	}
	const FTestSound Long(3000, 48000);
	const int32 Buffer = Audio.AcquireSoundBuffer(FName(TEXT("/Test/S_Long")), Long.Sound);
	Audio.SetListener(FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
	Audio.PlaySoundAtLocation(Buffer, FVector(30000.0f, 0.0f, 0.0f));
	Audio.Tick();
	TestEqual("300 m away: inaudible, no voice", GHardwareLog.Plays.Num(), 0);
	Audio.PlaySoundAtLocation(Buffer, FVector(10000.0f, 0.0f, 0.0f));
	Audio.Tick();
	TestEqual("100 m away: a voice", GHardwareLog.Plays.Num(), 1);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		Audio.PlaySound2D(Buffer, 1.0f, 0.5f);
	}
	Audio.Tick();
	TestEqual("12 of 22 effect voices for the low priority ones", GHardwareLog.Plays.Num(), 12);
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Audio.PlaySound2D(Buffer);
	}
	Audio.Tick();
	TestEqual("The default ones take 6 more", GHardwareLog.Plays.Num(), 18);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Audio.PlaySound2D(Buffer, 1.0f, 2.0f);
	}
	Audio.Tick();
	TestEqual("The higher ones the last 4", GHardwareLog.Plays.Num(), 22);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
