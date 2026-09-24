#include "CoreGlobals.h"
#include "LaunchEngineLoop.h"

int32 GuardedMain(int32 ArgC, char* ArgV[])
{
	const int32 ErrorLevel = GEngineLoop.PreInit(ArgC, ArgV);
	if (ErrorLevel != 0 || IsEngineExitRequested())
	{
		GEngineLoop.Exit();
		return ErrorLevel;
	}

	GEngineLoop.Init();
	while (!IsEngineExitRequested())
	{
		GEngineLoop.Tick();
	}
	GEngineLoop.Exit();
	return GEngineLoop.GetExitCode();
}
