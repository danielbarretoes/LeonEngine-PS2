#include "Commandlets/CookCommandlet.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"

// LeonCook program: the cook commandlet as a standalone executable.
int main(int ArgC, char* ArgV[])
{
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));
	return UCookCommandlet::Main(ArgC, ArgV);
}
