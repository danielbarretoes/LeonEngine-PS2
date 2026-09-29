#include "GenericPlatform/IInputInterface.h"

#include "Math/UnrealMathUtility.h"

namespace
{
	const FForceFeedbackValues NoForceFeedback;
} // namespace

void IInputInterface::SetForceFeedbackChannelValue(
	int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	if (ControllerId < 0 || ControllerId >= MaxControllers)
	{
		return;
	}
	FForceFeedbackValues Values = ForceFeedbackValues[ControllerId];
	const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
	switch (ChannelType)
	{
		case FForceFeedbackChannelType::LEFT_LARGE:
			Values.LeftLarge = Clamped;
			break;
		case FForceFeedbackChannelType::LEFT_SMALL:
			Values.LeftSmall = Clamped;
			break;
		case FForceFeedbackChannelType::RIGHT_LARGE:
			Values.RightLarge = Clamped;
			break;
		case FForceFeedbackChannelType::RIGHT_SMALL:
			Values.RightSmall = Clamped;
			break;
	}
	SetForceFeedbackChannelValues(ControllerId, Values);
}

void IInputInterface::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues& Values)
{
	if (ControllerId < 0 || ControllerId >= MaxControllers)
	{
		return;
	}
	FForceFeedbackValues& Stored = ForceFeedbackValues[ControllerId];
	Stored.LeftLarge = FMath::Clamp(Values.LeftLarge, 0.0f, 1.0f);
	Stored.LeftSmall = FMath::Clamp(Values.LeftSmall, 0.0f, 1.0f);
	Stored.RightLarge = FMath::Clamp(Values.RightLarge, 0.0f, 1.0f);
	Stored.RightSmall = FMath::Clamp(Values.RightSmall, 0.0f, 1.0f);
}

const FForceFeedbackValues& IInputInterface::GetForceFeedbackValues(int32 ControllerId) const
{
	return ControllerId >= 0 && ControllerId < MaxControllers ? ForceFeedbackValues[ControllerId] : NoForceFeedback;
}
