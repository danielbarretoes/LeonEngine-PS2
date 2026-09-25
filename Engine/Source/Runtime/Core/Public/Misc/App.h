#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

/** Build configuration of the running binary (UE: EBuildConfiguration). */
enum class EBuildConfiguration : uint8
{
	Unknown,
	Debug,
	DebugGame,
	Development,
	Shipping,
	Test
};

CORE_API const TCHAR* LexToString(EBuildConfiguration Configuration);

/** Facts about the running application (UE: FApp). */
class CORE_API FApp
{
public:
	/** Name of the loaded project; empty for engine programs without one (UE: GetProjectName). */
	static const TCHAR* GetProjectName();
	static void SetProjectName(const TCHAR* InProjectName);
	static bool HasProjectName();

	/** Name of the build target: the executable's name (UE: GetName). */
	static const TCHAR* GetName();

	static EBuildConfiguration GetBuildConfiguration();

	/** Unique id of this run, created on first use (UE: GetSessionId). */
	static FGuid GetSessionId();

	/**
	 * No one is watching: -unattended (UE: IsUnattended), or one of Leon's scripted captures (-Screenshot=,
	 * -ExitAfterFrames=). An unattended game ignores the OS input (UGameViewportClient::SetIgnoreInput), so a capture
	 * does not depend on the mouse or the keyboard.
	 */
	static bool IsUnattended();

	/** Whether a renderer may ever be created: false with -nullrhi (UE: CanEverRender). */
	static bool CanEverRender();

	/** Frame delta time in seconds (UE: GetDeltaTime / SetDeltaTime; double like UE). */
	static double GetDeltaTime()
	{
		return DeltaTime;
	}
	static void SetDeltaTime(double Seconds)
	{
		DeltaTime = Seconds;
	}

	/** Game time in seconds (UE: GetCurrentTime / SetCurrentTime). */
	static double GetCurrentTime()
	{
		return CurrentTime;
	}
	static void SetCurrentTime(double Seconds)
	{
		CurrentTime = Seconds;
	}

private:
	static double DeltaTime;
	static double CurrentTime;
};
