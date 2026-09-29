#include "GameFramework/ForceFeedbackEffect.h"

UForceFeedbackEffect::UForceFeedbackEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UForceFeedbackEffect::GetValues(float EvalTime, FForceFeedbackValues& Values, float ValueMultiplier) const
{
	const float Alpha = Duration > 0.0f ? FMath::Clamp(EvalTime / Duration, 0.0f, 1.0f) : 1.0f;
	for (const FForceFeedbackChannelDetails& Details : ChannelDetails)
	{
		const float Value = FMath::Clamp(
			FMath::Lerp(Details.StartIntensity, Details.EndIntensity, Alpha) * ValueMultiplier, 0.0f, 1.0f);
		if (Details.bAffectsLeftLarge)
		{
			Values.LeftLarge = FMath::Max(Values.LeftLarge, Value);
		}
		if (Details.bAffectsLeftSmall)
		{
			Values.LeftSmall = FMath::Max(Values.LeftSmall, Value);
		}
		if (Details.bAffectsRightLarge)
		{
			Values.RightLarge = FMath::Max(Values.RightLarge, Value);
		}
		if (Details.bAffectsRightSmall)
		{
			Values.RightSmall = FMath::Max(Values.RightSmall, Value);
		}
	}
}

bool FActiveForceFeedbackEffect::Update(float DeltaTime, FForceFeedbackValues& Values)
{
	if (ForceFeedbackEffect == nullptr)
	{
		return false;
	}
	const float Duration = ForceFeedbackEffect->GetDuration();
	PlayTime += DeltaTime;
	if (PlayTime > Duration && (!Parameters.bLooping || Duration <= 0.0f))
	{
		return false;
	}
	if (Parameters.bLooping && PlayTime > Duration)
	{
		PlayTime -= Duration * FMath::FloorToFloat(PlayTime / Duration);
	}
	ForceFeedbackEffect->GetValues(PlayTime, Values);
	return true;
}
