#include "LaunchEngineLoop.h"

// Win64 entry point (UE: LaunchWindows.cpp WinMain -> GuardedMain). Console subsystem for now.
int main(int ArgC, char* ArgV[])
{
	return GuardedMain(ArgC, ArgV);
}
