#include "LaunchEngineLoop.h"

// Win64 entry point (UE: LaunchWindows.cpp WinMain -> GuardedMain), a console subsystem program (the log on stdout).
int main(int ArgC, char* ArgV[])
{
	return GuardedMain(ArgC, ArgV);
}
