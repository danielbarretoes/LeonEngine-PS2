#include "UiTone.h"

void BuildUiTone(EUISound InSound, TArray<float>& OutSamples, int32& OutSampleRate)
{
	OutSampleRate = 44100;
	float Freq = 660.0f;
	float Duration = 0.045f;
	float Amp = 0.22f;
	switch (InSound)
	{
		case EUISound::Click:
			Freq = 880.0f;
			Duration = 0.035f;
			Amp = 0.18f;
			break;
		case EUISound::Confirm:
			Freq = 520.0f;
			Duration = 0.08f;
			Amp = 0.22f;
			break;
		case EUISound::Back:
			Freq = 320.0f;
			Duration = 0.05f;
			Amp = 0.16f;
			break;
		case EUISound::Error:
			Freq = 180.0f;
			Duration = 0.12f;
			Amp = 0.2f;
			break;
	}
	const int32 N = FMath::Max(1, static_cast<int32>(Duration * static_cast<float>(OutSampleRate)));
	OutSamples.SetNum(N);
	constexpr float Pi = 3.14159265f;
	for (int32 I = 0; I < N; ++I)
	{
		const float T = static_cast<float>(I) / static_cast<float>(OutSampleRate);
		const float Env = 1.0f - (static_cast<float>(I) / static_cast<float>(N));
		float Sample = FMath::Sin(2.0f * Pi * Freq * T) * Amp * Env;
		if (InSound == EUISound::Confirm && T > 0.04f)
		{
			Sample += FMath::Sin(2.0f * Pi * 780.0f * T) * Amp * 0.55f * Env;
		}
		if (InSound == EUISound::Error)
		{
			Sample = (FMath::Sin(2.0f * Pi * Freq * T) + 0.5f * FMath::Sin(2.0f * Pi * (Freq * 1.5f) * T)) * Amp * Env;
		}
		OutSamples[I] = Sample;
	}
}
