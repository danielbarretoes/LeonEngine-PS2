#include "LaunchEngineLoop.h"

// Linux entry point (UE: LaunchLinux.cpp).
int main(int ArgC, char* ArgV[])
{
	return GuardedMain(ArgC, ArgV);
}
