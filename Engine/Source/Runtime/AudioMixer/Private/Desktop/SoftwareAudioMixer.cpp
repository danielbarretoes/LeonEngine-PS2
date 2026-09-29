#include "Desktop/SoftwareAudioMixer.h"

#include "SpuAdpcm.h"
#include "Stats/Stats.h"

DECLARE_CYCLE_STAT(TEXT("Audio Mix"), STAT_AudioMix, STATGROUP_Audio);

FSoftwareAudioMixer::FSoftwareAudioMixer(int32 InOutputRate)
	: OutputRate(FMath::Max(1, InOutputRate))
{
}

void FSoftwareAudioMixer::Play(int32 Voice, const int16* Samples, int32 NumFrames, int32 LoopStartFrame, int32 Pitch,
	float GainLeft, float GainRight)
{
	if (Voice < 0 || Voice >= MaxVoices || Samples == nullptr || NumFrames <= 0)
	{
		return;
	}
	FVoice& State = Voices[Voice];
	State.Samples = Samples;
	State.NumFrames = NumFrames;
	State.LoopStartFrame = LoopStartFrame >= 0 && LoopStartFrame < NumFrames ? LoopStartFrame : INDEX_NONE;
	// Pitch / 4096 source frames per 48 kHz frame, in source frames per output frame.
	State.Step = (uint64(FMath::Max(Pitch, 1)) * uint64(FSpuAdpcm::OutputRate) << FractionBits) /
		(uint64(FSpuAdpcm::PitchOne) * uint64(OutputRate));
	State.Position = 0;
	State.GainLeft = GainLeft;
	State.GainRight = GainRight;
	State.bPlaying = true;
}

void FSoftwareAudioMixer::SetGains(int32 Voice, float GainLeft, float GainRight)
{
	if (Voice >= 0 && Voice < MaxVoices)
	{
		Voices[Voice].GainLeft = GainLeft;
		Voices[Voice].GainRight = GainRight;
	}
}

void FSoftwareAudioMixer::Stop(int32 Voice)
{
	if (Voice >= 0 && Voice < MaxVoices)
	{
		Voices[Voice].bPlaying = false;
		Voices[Voice].Samples = nullptr;
	}
}

bool FSoftwareAudioMixer::IsPlaying(int32 Voice) const
{
	return Voice >= 0 && Voice < MaxVoices && Voices[Voice].bPlaying;
}

int32 FSoftwareAudioMixer::GetNumPlaying() const
{
	int32 Count = 0;
	for (const FVoice& Voice : Voices)
	{
		Count += Voice.bPlaying ? 1 : 0;
	}
	return Count;
}

void FSoftwareAudioMixer::Mix(int16* Out, int32 NumFrames)
{
	SCOPE_CYCLE_COUNTER(STAT_AudioMix);
	if (Out == nullptr || NumFrames <= 0)
	{
		return;
	}
	MixBuffer.SetNumUninitialized(NumFrames * 2, false);
	FMemory::Memzero(MixBuffer.GetData(), SIZE_T(MixBuffer.Num()) * sizeof(float));
	// Raw pointers in the loops: they run for every output frame of every voice.
	float* const Mixed = MixBuffer.GetData();
	constexpr float FractionScale = 1.0f / float(uint64(1) << FractionBits);
	for (FVoice& Voice : Voices)
	{
		if (!Voice.bPlaying)
		{
			continue;
		}
		const bool bLooping = Voice.LoopStartFrame != INDEX_NONE;
		const int16* const Samples = Voice.Samples;
		const uint64 End = uint64(Voice.NumFrames) << FractionBits;
		const uint64 LoopLength = bLooping ? uint64(Voice.NumFrames - Voice.LoopStartFrame) << FractionBits : 0;
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			if (Voice.Position >= End)
			{
				if (!bLooping)
				{
					Voice.bPlaying = false;
					Voice.Samples = nullptr;
					break;
				}
				Voice.Position -= LoopLength;
			}
			// Linear between the frame and the next (the loop's start after its end; held at a one-shot's end).
			const int32 Index = int32(Voice.Position >> FractionBits);
			const float Fraction = float(int32(Voice.Position & FractionMask)) * FractionScale;
			int32 Next = Index + 1;
			if (Next >= Voice.NumFrames)
			{
				Next = bLooping ? Voice.LoopStartFrame : Index;
			}
			const float A = float(Samples[Index]);
			const float Sample = A + ((float(Samples[Next]) - A) * Fraction);
			Mixed[Frame * 2] += Sample * Voice.GainLeft;
			Mixed[(Frame * 2) + 1] += Sample * Voice.GainRight;
			Voice.Position += Voice.Step;
		}
	}
	for (int32 Index = 0; Index < NumFrames * 2; ++Index)
	{
		Out[Index] = int16(FMath::Clamp(FMath::RoundToInt(Mixed[Index]), -32768, 32767));
	}
}
