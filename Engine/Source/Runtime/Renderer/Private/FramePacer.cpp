#include "FramePacer.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"

double FFramePacer::NextFrameSeconds(double LastFrame, double Now, int32 SyncInterval)
{
	const double Target = LastFrame + (double(SyncInterval) * FieldSeconds);
	// More than a field late: the frame goes now, and the next ones count from it.
	return Now > Target + FieldSeconds ? Now : Target;
}

void FFramePacer::Wait(int32 SyncInterval)
{
	const double Now = FPlatformTime::Seconds();
	if (SyncInterval <= 0 || FApp::IsBenchmarking() || LastFrameSeconds <= 0.0)
	{
		LastFrameSeconds = Now;
		return;
	}
	const double Target = NextFrameSeconds(LastFrameSeconds, Now, SyncInterval);
	// Sleep most of the wait, then spin the last millisecond (a sleep can overshoot).
	constexpr double SpinSeconds = 0.001;
	const double Remaining = Target - Now;
	if (Remaining > SpinSeconds)
	{
		FPlatformProcess::Sleep(float(Remaining - SpinSeconds));
	}
	while (FPlatformTime::Seconds() < Target)
	{
	}
	LastFrameSeconds = Target;
}
