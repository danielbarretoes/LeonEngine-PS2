#pragma once

#include "CoreMinimal.h"

/**
 * A hardware voice's volume as the PS2 sets it (Leon): audsrv_adpcm_set_volume_and_pan's volume (0 to 100) and pan
 * (-100 left to 100 right), which audsrv turns into the SPU2's left and right levels. The louder side keeps the volume,
 * the other gets `Volume * (100 - |Pan|) / 100`, and each goes through audsrv's table of 26 levels (`vol_values`, every
 * 4 percent, about 1.5 dB apart at the top). The desktop mixes with the same levels (GetLeftGain / GetRightGain), so a
 * sound is as loud on both platforms, steps included.
 */
struct FSpuVoiceVolume
{
	/** The SPU2's full level (VOLL / VOLR). */
	static constexpr int32 MaxLevel = 0x3FFF;
	/** audsrv's levels, one every 4 percent of volume (ps2sdk ee/rpc/audsrv: vol_values). */
	static constexpr int32 NumLevels = 26;
	static const uint16 Levels[NumLevels];

	int32 Volume = 0;
	int32 Pan = 0;

	/** The volume and pan whose levels are nearest the linear gains Left and Right (0 to 1). */
	[[nodiscard]] static FSpuVoiceVolume FromGains(float Left, float Right);

	/** The left level audsrv sets (0 to MaxLevel). */
	[[nodiscard]] int32 GetLeftLevel() const;
	[[nodiscard]] int32 GetRightLevel() const;

	[[nodiscard]] float GetLeftGain() const
	{
		return float(GetLeftLevel()) / float(MaxLevel);
	}

	[[nodiscard]] float GetRightGain() const
	{
		return float(GetRightLevel()) / float(MaxLevel);
	}

	[[nodiscard]] bool operator==(const FSpuVoiceVolume& Other) const
	{
		return Volume == Other.Volume && Pan == Other.Pan;
	}

	[[nodiscard]] bool operator!=(const FSpuVoiceVolume& Other) const
	{
		return !(*this == Other);
	}
};
