#include "SoftwareAudioMixer.h"

namespace
{

	/** Centimetres to the metres the attenuation is tuned for (the desktop's miniaudio works in metres). */
	constexpr float MetresPerUnit = 0.01f;

} // namespace

FSoftwareAudioMixer::FSoftwareAudioMixer(int32 InOutputRate)
	: OutputRate(FMath::Max(1, InOutputRate))
{
}

void FSoftwareAudioMixer::SetListener(const FVector& Location, const FVector& Forward, const FVector& Up)
{
	ListenerLocation = Location;
	// The world is left-handed, Z up: the right side is Up ^ Forward.
	const FVector Right = Up ^ Forward;
	if (Right.SizeSquared() > 1.0e-8f)
	{
		ListenerRight = Right.GetSafeNormal();
	}
}

void FSoftwareAudioMixer::SetMasterVolume(float Volume01)
{
	MasterVolume = FMath::Clamp(Volume01, 0.0f, 1.0f);
}

int32 FSoftwareAudioMixer::Play(
	const FSoundWavePCM& Sound, float Volume, bool bLooping, bool bSpatialized, const FVector& Location)
{
	if (!Sound.IsValid())
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < MaxVoices; ++Index)
	{
		FVoice& Voice = Voices[Index];
		if (Voice.bPlaying)
		{
			continue;
		}
		Voice.Samples.SetNumUninitialized(Sound.NumFrames * Sound.NumChannels);
		FMemory::Memcpy(Voice.Samples.GetData(), Sound.Samples, SIZE_T(Voice.Samples.Num()) * sizeof(int16));
		Voice.NumFrames = Sound.NumFrames;
		Voice.NumChannels = Sound.NumChannels;
		Voice.Step = double(Sound.SampleRate) / double(OutputRate);
		Voice.Position = 0.0;
		Voice.Volume = FMath::Clamp(Volume, 0.0f, 1.0f);
		Voice.bLooping = bLooping;
		Voice.bSpatialized = bSpatialized;
		Voice.Location = Location;
		Voice.bPlaying = true;
		return Index;
	}
	return INDEX_NONE;
}

void FSoftwareAudioMixer::Stop(int32 Voice)
{
	if (Voice >= 0 && Voice < MaxVoices)
	{
		Voices[Voice].bPlaying = false;
		Voices[Voice].Samples.Empty();
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

void FSoftwareAudioMixer::SpatialGains(const FVector& Location, float& OutLeft, float& OutRight) const
{
	const FVector ToSound = Location - ListenerLocation;
	const float Distance = ToSound.Size() * MetresPerUnit;
	const float Attenuation = 1.0f / FMath::Max(Distance, 1.0f);
	// -1 on the left, +1 on the right: the near side stays full, the far side fades.
	const float Pan = Distance > 1.0e-4f ? FMath::Clamp(ToSound.GetSafeNormal() | ListenerRight, -1.0f, 1.0f) : 0.0f;
	OutLeft = Attenuation * FMath::Min(1.0f, 1.0f - Pan);
	OutRight = Attenuation * FMath::Min(1.0f, 1.0f + Pan);
}

void FSoftwareAudioMixer::Mix(int16* Out, int32 NumFrames)
{
	if (Out == nullptr || NumFrames <= 0)
	{
		return;
	}
	MixBuffer.SetNumUninitialized(NumFrames * 2);
	FMemory::Memzero(MixBuffer.GetData(), SIZE_T(MixBuffer.Num()) * sizeof(float));
	for (FVoice& Voice : Voices)
	{
		if (!Voice.bPlaying)
		{
			continue;
		}
		float GainLeft = Voice.Volume;
		float GainRight = Voice.Volume;
		if (Voice.bSpatialized)
		{
			float Left = 1.0f;
			float Right = 1.0f;
			SpatialGains(Voice.Location, Left, Right);
			GainLeft *= Left;
			GainRight *= Right;
		}
		const bool bStereo = Voice.NumChannels >= 2;
		const int32 Stride = Voice.NumChannels;
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			if (Voice.Position >= double(Voice.NumFrames))
			{
				if (!Voice.bLooping)
				{
					Voice.bPlaying = false;
					Voice.Samples.Empty();
					break;
				}
				Voice.Position -= double(Voice.NumFrames);
			}
			// Linear between the frame and the next (the next wraps for a loop, holds at the end otherwise).
			const int32 Index = int32(Voice.Position);
			const float Fraction = float(Voice.Position - double(Index));
			int32 Next = Index + 1;
			if (Next >= Voice.NumFrames)
			{
				Next = Voice.bLooping ? 0 : Index;
			}
			const auto Sample = [&Voice, Stride, Fraction, Index, Next](int32 Channel)
			{
				const float A = float(Voice.Samples[(Index * Stride) + Channel]);
				const float B = float(Voice.Samples[(Next * Stride) + Channel]);
				return A + ((B - A) * Fraction);
			};
			float Left = Sample(0);
			float Right = bStereo ? Sample(1) : Left;
			if (Voice.bSpatialized && bStereo)
			{
				// A spatialized sound is a point: its channels meet before they pan.
				Left = (Left + Right) * 0.5f;
				Right = Left;
			}
			MixBuffer[Frame * 2] += Left * GainLeft;
			MixBuffer[(Frame * 2) + 1] += Right * GainRight;
			Voice.Position += Voice.Step;
		}
	}
	for (int32 Index = 0; Index < NumFrames * 2; ++Index)
	{
		Out[Index] = int16(FMath::Clamp(FMath::RoundToInt(MixBuffer[Index] * MasterVolume), -32768, 32767));
	}
}
