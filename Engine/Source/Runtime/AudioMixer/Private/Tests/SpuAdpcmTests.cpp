#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "SpuAdpcm.h"
#include "SpuVoiceVolume.h"

#if WITH_DEV_AUTOMATION_TESTS

// The SPU2's ADPCM as a voice decodes it, its pitch, and audsrv's volume steps (Docs/PLANS/ps2-shipping.md N19).

namespace
{

	/** A block of the filter, the shift and flags whose 28 nibbles are Nibbles, in order (-8 to 7). */
	void MakeBlock(int32 Filter, int32 Shift, uint8 Flags, const int32 (&Nibbles)[FSpuAdpcm::SamplesPerBlock],
		uint8 (&Out)[FSpuAdpcm::BytesPerBlock])
	{
		FMemory::Memzero(Out, sizeof(Out));
		Out[0] = uint8((Filter << 4) | Shift);
		Out[1] = Flags;
		for (int32 Index = 0; Index < FSpuAdpcm::SamplesPerBlock; ++Index)
		{
			Out[2 + (Index / 2)] |= uint8((Nibbles[Index] & 0x0F) << ((Index % 2) * 4));
		}
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpuAdpcmDecodeTest, "System.AudioMixer.SpuAdpcm.Decode",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSpuAdpcmDecodeTest::RunTest(const FString& Parameters)
{
	int32 Nibbles[FSpuAdpcm::SamplesPerBlock] = {};
	Nibbles[0] = 1;
	Nibbles[1] = -1;
	Nibbles[2] = 7;
	Nibbles[3] = -8;
	uint8 Block[FSpuAdpcm::BytesPerBlock];
	int16 Out[FSpuAdpcm::SamplesPerBlock];

	// Filter 0 (no prediction), shift 0: each nibble times 4096, the first in the low nibble.
	MakeBlock(0, 0, 0, Nibbles, Block);
	int32 Previous1 = 0;
	int32 Previous2 = 0;
	FSpuAdpcm::DecodeBlock(Block, Previous1, Previous2, Out);
	TestTrue("Filter 0, shift 0", Out[0] == 4096 && Out[1] == -4096 && Out[2] == 28672 && Out[3] == -32768);
	TestTrue("The history is the last two samples", Previous1 == 0 && Previous2 == 0 && Out[4] == 0);

	// Shift 4: a step of 256; the shifts past 12 are the hardware's 9.
	MakeBlock(0, 4, 0, Nibbles, Block);
	FSpuAdpcm::DecodeBlock(Block, Previous1, Previous2, Out);
	TestTrue("Shift 4", Out[0] == 256 && Out[1] == -256);
	MakeBlock(0, 13, 0, Nibbles, Block);
	FSpuAdpcm::DecodeBlock(Block, Previous1, Previous2, Out);
	TestTrue("Shift 13 is 9", Out[0] == 8 && Out[1] == -8);

	// Filter 1 (60 / 64 of the previous sample) from a history of 1000: 1000 * 60 / 64 = 937.5, +32 >> 6 rounds.
	const int32 Zero[FSpuAdpcm::SamplesPerBlock] = {};
	MakeBlock(1, 12, 0, Zero, Block);
	Previous1 = 1000;
	Previous2 = 0;
	FSpuAdpcm::DecodeBlock(Block, Previous1, Previous2, Out);
	TestTrue("Filter 1 decays", Out[0] == 938 && Out[1] == 879);
	// Filter 2 (115, -52) with the second previous sample.
	MakeBlock(2, 12, 0, Zero, Block);
	Previous1 = 1000;
	Previous2 = 500;
	FSpuAdpcm::DecodeBlock(Block, Previous1, Previous2, Out);
	TestEqual("Filter 2", int32(Out[0]), ((1000 * 115) + (500 * -52) + 32) >> 6);
	// A prediction past 16 bits clamps.
	int32 Loud[FSpuAdpcm::SamplesPerBlock] = {};
	Loud[0] = 7;
	MakeBlock(4, 0, 0, Loud, Block);
	Previous1 = 32000;
	Previous2 = 0;
	FSpuAdpcm::DecodeBlock(Block, Previous1, Previous2, Out);
	TestEqual("Clamped", int32(Out[0]), 32767);

	// A whole sound: two blocks decoded in order, 56 samples.
	uint8 Blocks[2 * FSpuAdpcm::BytesPerBlock];
	uint8 Second[FSpuAdpcm::BytesPerBlock];
	MakeBlock(0, 0, 0, Nibbles, Block);
	MakeBlock(1, 12, FSpuAdpcm::FlagLoopEnd, Zero, Second);
	FMemory::Memcpy(Blocks, Block, FSpuAdpcm::BytesPerBlock);
	FMemory::Memcpy(Blocks + FSpuAdpcm::BytesPerBlock, Second, FSpuAdpcm::BytesPerBlock);
	FSpuAdpcmSound Sound;
	Sound.Blocks = Blocks;
	Sound.NumBlocks = 2;
	Sound.SampleRate = 22050;
	TestTrue("Valid", Sound.IsValid());
	TestTrue("56 frames, 32 bytes", Sound.GetNumFrames() == 56 && Sound.GetNumBytes() == 32);
	TArray<int16> Decoded;
	FSpuAdpcm::Decode(Sound, Decoded);
	TestTrue("Decoded", Decoded.Num() == 56 && Decoded[0] == 4096 && Decoded[28] == 0);
	Sound.LoopStartFrame = 28;
	TestTrue("A loop on a block", Sound.IsValid() && Sound.IsLooping());
	Sound.LoopStartFrame = 30;
	TestFalse("A loop off a block", Sound.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpuAdpcmPitchTest, "System.AudioMixer.SpuAdpcm.Pitch",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSpuAdpcmPitchTest::RunTest(const FString& Parameters)
{
	// 4096 is 48 kHz; the others round to the nearest step; a voice's length follows its pitch.
	TestEqual("48 kHz", FSpuAdpcm::GetPitch(48000), 4096);
	TestEqual("22.05 kHz", FSpuAdpcm::GetPitch(22050), 1882);
	TestEqual("44.1 kHz", FSpuAdpcm::GetPitch(44100), 3763);
	TestEqual("At most 0x3FFF", FSpuAdpcm::GetPitch(1000000), FSpuAdpcm::MaxPitch);
	TestEqual("At least 1", FSpuAdpcm::GetPitch(0), 1);
	TestEqual("48 000 frames at 48 kHz: a second", FSpuAdpcm::GetPlayMicroseconds(48000, 4096), uint64(1000000));
	TestEqual("Half the pitch, twice as long", FSpuAdpcm::GetPlayMicroseconds(48000, 2048), uint64(2000000));
	TestEqual("The SPU2 RAM for sounds", FSpuAdpcm::SoundRamBytes, (2 * 1024 * 1024) - 0x5010);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpuVoiceVolumeTest, "System.AudioMixer.SpuAdpcm.VoiceVolume",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSpuVoiceVolumeTest::RunTest(const FString& Parameters)
{
	// Full on both sides; full on one; silence.
	const FSpuVoiceVolume Full = FSpuVoiceVolume::FromGains(1.0f, 1.0f);
	TestTrue(
		"Full", Full.Volume == 100 && Full.Pan == 0 && Full.GetLeftLevel() == 0x3FFF && Full.GetRightLevel() == 0x3FFF);
	const FSpuVoiceVolume Left = FSpuVoiceVolume::FromGains(1.0f, 0.0f);
	TestTrue("Left only", Left.Pan == -100 && Left.GetLeftLevel() == 0x3FFF && Left.GetRightLevel() == 0);
	const FSpuVoiceVolume Right = FSpuVoiceVolume::FromGains(0.0f, 1.0f);
	TestTrue("Right only", Right.Pan == 100 && Right.GetLeftLevel() == 0 && Right.GetRightLevel() == 0x3FFF);
	const FSpuVoiceVolume Silent = FSpuVoiceVolume::FromGains(0.0f, 0.0f);
	TestTrue("Silent", Silent.GetLeftLevel() == 0 && Silent.GetRightLevel() == 0);

	// Every pair of levels audsrv can set comes back through its volume and pan exactly: the desktop's gains are the
	// PS2's levels.
	bool bExact = true;
	for (int32 Louder = 2; Louder < FSpuVoiceVolume::NumLevels; ++Louder)
	{
		for (int32 Quieter = 0; Quieter <= Louder; ++Quieter)
		{
			const float LouderGain = float(FSpuVoiceVolume::Levels[Louder]) / float(FSpuVoiceVolume::MaxLevel);
			const float QuieterGain = float(FSpuVoiceVolume::Levels[Quieter]) / float(FSpuVoiceVolume::MaxLevel);
			const FSpuVoiceVolume OnLeft = FSpuVoiceVolume::FromGains(LouderGain, QuieterGain);
			const FSpuVoiceVolume OnRight = FSpuVoiceVolume::FromGains(QuieterGain, LouderGain);
			bExact &= OnLeft.GetLeftLevel() == FSpuVoiceVolume::Levels[Louder] &&
				OnLeft.GetRightLevel() == FSpuVoiceVolume::Levels[Quieter];
			bExact &= OnRight.GetRightLevel() == FSpuVoiceVolume::Levels[Louder] &&
				OnRight.GetLeftLevel() == FSpuVoiceVolume::Levels[Quieter];
			bExact &= OnLeft.Volume >= 0 && OnLeft.Volume <= 100 && OnLeft.Pan >= -100 && OnLeft.Pan <= 0;
		}
	}
	TestTrue("Every pair of levels", bExact);

	// A gain between two levels takes the nearest: a fifth (5 m away) lies between 0x0BC2 (0.184) and 0x0DC0 (0.215).
	const FSpuVoiceVolume Far = FSpuVoiceVolume::FromGains(0.2f, 0.2f);
	TestEqual("The nearest level", Far.GetLeftLevel(), int32(0x0DC0));
	TestTrue("Both sides", Far.GetLeftGain() == Far.GetRightGain() && FMath::Abs(Far.GetLeftGain() - 0.2f) < 0.02f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
