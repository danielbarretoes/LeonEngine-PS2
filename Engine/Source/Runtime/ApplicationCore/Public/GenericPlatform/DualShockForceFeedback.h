#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/IInputInterface.h"

/** The DualShock's two motors as libpad's padSetActDirect takes them: the small one on or off, the large one's speed.
 */
struct FDualShockMotors
{
	/** 0 off, 1 on (act_align[0]). */
	uint8 Small = 0;
	/** 0 stopped, else its speed up to 255 (act_align[1]). */
	uint8 Large = 0;

	[[nodiscard]] bool operator==(const FDualShockMotors& Other) const
	{
		return Small == Other.Small && Large == Other.Large;
	}
	[[nodiscard]] bool operator!=(const FDualShockMotors& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * UE's four force feedback channels on the DualShock's two motors (Leon; Docs/PLANS/ps2-shipping.md N24), as UE's
 * XInputInterface folds them on a pad's two: the large channels (left or right, the stronger) drive the large motor,
 * the small ones the small motor. The small motor has no speed: it runs from SmallOnThreshold up. The large one does
 * not turn below LargeMinimumSpeed, so any strength above 0 starts there and 1 is 255.
 *
 * FDualShockActuators decides when the pad is told: when the motors change, once the pad's actuators are aligned
 * (FDualShockConnection), and again after a reconnection (a pad plugged back is still); a pad that goes away forgets
 * what it was sent, and the input interface drops the game's request for it, so it does not start again by itself.
 */
struct FDualShockForceFeedback
{
	static constexpr float SmallOnThreshold = 0.5f;
	static constexpr uint8 LargeMinimumSpeed = 0x40;

	[[nodiscard]] static FDualShockMotors ToMotors(const FForceFeedbackValues& Values)
	{
		FDualShockMotors Motors;
		const float Small = FMath::Max(Values.LeftSmall, Values.RightSmall);
		const float Large = FMath::Clamp(FMath::Max(Values.LeftLarge, Values.RightLarge), 0.0f, 1.0f);
		Motors.Small = Small >= SmallOnThreshold ? 1 : 0;
		if (Large > 0.0f)
		{
			const float Speed = float(LargeMinimumSpeed) + Large * float(255 - LargeMinimumSpeed);
			Motors.Large = uint8(FMath::Clamp(FMath::RoundToInt(Speed), int32(LargeMinimumSpeed), 255));
		}
		return Motors;
	}
};

/** When a pad's motors are sent (see FDualShockForceFeedback). */
class FDualShockActuators
{
public:
	/**
	 * One frame: bReady when the pad is connected with its actuators aligned. True, with OutMotors, when the pad must
	 * be sent them now.
	 */
	[[nodiscard]] bool Update(bool bReady, const FForceFeedbackValues& Values, FDualShockMotors& OutMotors)
	{
		if (!bReady)
		{
			bSent = false;
			return false;
		}
		const FDualShockMotors Motors = FDualShockForceFeedback::ToMotors(Values);
		if (bSent && Motors == Last)
		{
			return false;
		}
		bSent = true;
		Last = Motors;
		OutMotors = Motors;
		return true;
	}

	/** What the pad was last sent (all 0 before). */
	[[nodiscard]] const FDualShockMotors& GetLastSent() const
	{
		return Last;
	}

private:
	bool bSent = false;
	FDualShockMotors Last;
};
