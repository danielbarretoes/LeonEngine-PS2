#pragma once

#include "AudioDevice.h"
#include "CoreMinimal.h"

/** The procedural tone of a UI cue without samples of its own (mono floats at OutSampleRate). */
void BuildUiTone(EUISound InSound, TArray<float>& OutSamples, int32& OutSampleRate);
