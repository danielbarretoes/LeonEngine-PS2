#include "FixedStepClock.h"

FFixedStepClock::FFixedStepClock(uint32 InStepsPerSecond, int32 InMaxStepsPerFrame)
	: StepsPerSecond(FMath::Max<uint32>(1, InStepsPerSecond))
	, MaxStepsPerFrame(FMath::Max(1, InMaxStepsPerFrame))
{
}

int32 FFixedStepClock::Advance(uint64 ElapsedMicroseconds)
{
	// A second is StepsPerSecond x 1 000 000 units: the microseconds times the rate, exactly.
	Accumulator += ElapsedMicroseconds * StepsPerSecond;
	const uint64 DueSteps = Accumulator / UnitsPerStep;
	Accumulator -= DueSteps * UnitsPerStep;
	const uint64 Steps = FMath::Min<uint64>(DueSteps, static_cast<uint64>(MaxStepsPerFrame));
	NumDroppedSteps += DueSteps - Steps;
	NumSteps += Steps;
	return static_cast<int32>(Steps);
}

uint64 FFixedStepClock::GetMicrosecondsToNextStep() const
{
	const uint64 UnitsLeft = UnitsPerStep - Accumulator;
	return (UnitsLeft + StepsPerSecond - 1) / StepsPerSecond;
}
