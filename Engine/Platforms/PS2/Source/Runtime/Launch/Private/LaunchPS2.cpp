#include "LaunchEngineLoop.h"

// PS2 entry point (UE: Launch<Platform>.cpp in the platform extension).
int main(int ArgC, char* ArgV[])
{
	return GuardedMain(ArgC, ArgV);
}
