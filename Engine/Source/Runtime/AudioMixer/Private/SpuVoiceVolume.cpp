#include "SpuVoiceVolume.h"

const uint16 FSpuVoiceVolume::Levels[FSpuVoiceVolume::NumLevels] = {0x0000, 0x0000, 0x0096, 0x0190, 0x0230, 0x0320,
	0x042E, 0x0532, 0x05FA, 0x06C2, 0x088E, 0x09F6, 0x0BC2, 0x0DC0, 0x0FF0, 0x118A, 0x1482, 0x1752, 0x1B4E, 0x1F40,
	0x2378, 0x28D2, 0x2EFE, 0x34F8, 0x3A5C, 0x3FFF};

namespace
{

	/** The level nearest Gain times the full level (the lower one on a tie). */
	int32 GetNearestLevel(float Gain)
	{
		const float Target = FMath::Clamp(Gain, 0.0f, 1.0f) * float(FSpuVoiceVolume::MaxLevel);
		int32 Best = 0;
		float BestDistance = Target;
		for (int32 Index = 1; Index < FSpuVoiceVolume::NumLevels; ++Index)
		{
			const float Distance = FMath::Abs(float(FSpuVoiceVolume::Levels[Index]) - Target);
			if (Distance < BestDistance)
			{
				Best = Index;
				BestDistance = Distance;
			}
		}
		return Best;
	}

	/** audsrv's volume of one side: Volume, less the pan when it leans to the other side. */
	int32 GetSidePercent(int32 Volume, int32 Pan, bool bLeft)
	{
		if (bLeft && Pan > 0)
		{
			return Volume * (100 - Pan) / 100;
		}
		if (!bLeft && Pan < 0)
		{
			return Volume * (100 + Pan) / 100;
		}
		return Volume;
	}

} // namespace

FSpuVoiceVolume FSpuVoiceVolume::FromGains(float Left, float Right)
{
	FSpuVoiceVolume Result;
	const int32 Louder = GetNearestLevel(FMath::Max(Left, Right));
	if (Louder == 0)
	{
		return Result;
	}
	// The louder side's percent picks its level; the quieter side's comes from the pan, which audsrv applies before the
	// table: the least pan whose percent reaches the quieter level.
	Result.Volume = FMath::Min(Louder * 4, 100);
	const int32 Quieter = FMath::Min(GetNearestLevel(FMath::Min(Left, Right)), Louder);
	if (Quieter == Louder)
	{
		return Result;
	}
	const int32 Needed = ((400 * Quieter) + Result.Volume - 1) / Result.Volume;
	const int32 Amount = FMath::Clamp(100 - Needed, 0, 100);
	Result.Pan = Left < Right ? Amount : -Amount;
	return Result;
}

int32 FSpuVoiceVolume::GetLeftLevel() const
{
	const int32 Percent = FMath::Clamp(GetSidePercent(Volume, Pan, true), 0, 100);
	return Levels[Percent / 4];
}

int32 FSpuVoiceVolume::GetRightLevel() const
{
	const int32 Percent = FMath::Clamp(GetSidePercent(Volume, Pan, false), 0, 100);
	return Levels[Percent / 4];
}
