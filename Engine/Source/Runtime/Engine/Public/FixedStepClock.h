#pragma once

#include "CoreMinimal.h"

/**
 * The game's clock ([ps2-shipping](Docs/PLANS/ps2-shipping.md) D4; Leon, UE has no fixed step): the world advances in
 * steps of exactly 1 / StepsPerSecond seconds, as many as the real time since the last frame holds, and the render
 * draws between the last two steps.
 *
 * - The real time comes in integer microseconds (FPlatformTime::Cycles64) and is kept in units of 1 / (1 000 000 x
 *   StepsPerSecond) s, so a step is exactly 1 000 000 units: no float sums, no drift, and a run plays the same steps
 *   whatever its frame rate or its frames' lengths.
 * - What is left of the time after the whole steps carries over to the next frame; GetAlpha is that remainder as a
 *   fraction of a step, the render's interpolation weight between the previous and the current step.
 * - Spiral-of-death guard: a frame runs at most MaxStepsPerFrame steps. When a frame took longer (a load, a debugger,
 *   a machine too slow for the game) the whole steps beyond that are dropped and counted (GetNumDroppedSteps): the
 *   game slows down for that frame instead of running ever more steps to catch up.
 * - PAL renders at 25 fps and NTSC at 30 (the display's vertical blank paces the frames); the step stays 1/30 s, so a
 *   PAL frame runs one step or two (1.2 on average) and the interpolation hides it.
 */
class ENGINE_API FFixedStepClock
{
public:
	/** The game's step rate (D4: 30 Hz). */
	static constexpr uint32 DefaultStepsPerSecond = 30;
	/** The most steps one frame runs (133 ms of game at 30 Hz). */
	static constexpr int32 DefaultMaxStepsPerFrame = 4;
	/** A step in the accumulator's units. */
	static constexpr uint64 UnitsPerStep = 1000000;

	explicit FFixedStepClock(
		uint32 InStepsPerSecond = DefaultStepsPerSecond, int32 InMaxStepsPerFrame = DefaultMaxStepsPerFrame);

	/**
	 * Adds a frame's real time and returns how many steps to run now (0 to MaxStepsPerFrame); the rest of the time
	 * carries over.
	 */
	int32 Advance(uint64 ElapsedMicroseconds);

	/** Starts again: no time accumulated (a map loaded: its load is no game time). */
	void Reset()
	{
		Accumulator = 0;
	}

	/** One step's length in seconds: the world's delta time. */
	[[nodiscard]] float GetStepSeconds() const
	{
		return 1.0f / static_cast<float>(StepsPerSecond);
	}
	/** One step's length in microseconds, rounded down. */
	[[nodiscard]] uint64 GetStepMicroseconds() const
	{
		return 1000000ull / StepsPerSecond;
	}
	[[nodiscard]] uint32 GetStepsPerSecond() const
	{
		return StepsPerSecond;
	}
	[[nodiscard]] int32 GetMaxStepsPerFrame() const
	{
		return MaxStepsPerFrame;
	}

	/** How far the time is into the next step, in [0, 1): the render's interpolation weight. */
	[[nodiscard]] float GetAlpha() const
	{
		return static_cast<float>(Accumulator) / static_cast<float>(UnitsPerStep);
	}
	/** Microseconds of real time until the next step is due (a paced headless run sleeps them). */
	[[nodiscard]] uint64 GetMicrosecondsToNextStep() const;

	/** The steps run so far, and those dropped by the guard. */
	[[nodiscard]] uint64 GetNumSteps() const
	{
		return NumSteps;
	}
	[[nodiscard]] uint64 GetNumDroppedSteps() const
	{
		return NumDroppedSteps;
	}

private:
	/** The time not yet stepped, in units (1 000 000 a step). */
	uint64 Accumulator = 0;
	uint64 NumSteps = 0;
	uint64 NumDroppedSteps = 0;
	uint32 StepsPerSecond = DefaultStepsPerSecond;
	int32 MaxStepsPerFrame = DefaultMaxStepsPerFrame;
};
