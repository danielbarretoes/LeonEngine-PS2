#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "LaunchEngineLoop.h"
#include "Misc/FileHelper.h"
#include "PS2ErrorScreen.h"

namespace
{

	/**
	 * The arguments of <ELF folder>/LeonCommandLine.txt (UE: UECommandLine.txt on consoles and mobile), split on
	 * whitespace outside double quotes (the quotes are dropped). PCSX2 and a disc boot pass argv[0] only, so this is
	 * how a PS2 game gets -nullrhi -botmatch and the like; no file, no arguments.
	 */
	TArray<FString> ReadCommandLineFile()
	{
		TArray<FString> Arguments;
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *(FString(FPlatformProcess::BaseDir()) + "LeonCommandLine.txt")))
		{
			return Arguments;
		}
		FString Current;
		bool bQuoted = false;
		bool bInArgument = false;
		for (int32 Index = 0; Index < Contents.Len(); ++Index)
		{
			const TCHAR Char = Contents[Index];
			if (Char == '"')
			{
				bQuoted = !bQuoted;
				bInArgument = true;
			}
			else if (!bQuoted && (Char == ' ' || Char == '\t' || Char == '\r' || Char == '\n'))
			{
				if (bInArgument)
				{
					Arguments.Add(MoveTemp(Current));
					Current.Reset();
					bInArgument = false;
				}
			}
			else
			{
				Current.AppendChar(Char);
				bInArgument = true;
			}
		}
		if (bInArgument)
		{
			Arguments.Add(MoveTemp(Current));
		}
		return Arguments;
	}

} // namespace

// PS2 entry point (UE: Launch<Platform>.cpp in the platform extension): argv, then LeonCommandLine.txt's arguments. A
// game that stops with an error shows it on the TV (FPS2ErrorScreen) instead of a black screen.
int main(int ArgC, char* ArgV[])
{
	FPS2ErrorScreen::Install();
	// The IOP first, before a file is opened (a reboot closes them); -NoIopReset keeps a debugger's host: (ps2link).
	bool bResetIop = true;
	for (int32 Index = 1; Index < ArgC; ++Index)
	{
		bResetIop = bResetIop && FCString::Stricmp(ArgV[Index], "-NoIopReset") != 0;
	}
	FPlatformMisc::InitializeIop(bResetIop);
	FPlatformProcess::SetArgV0(ArgC > 0 ? ArgV[0] : nullptr);
	const TArray<FString> FileArguments = ReadCommandLineFile();
	TArray<char*> Arguments;
	for (int32 Index = 0; Index < ArgC; ++Index)
	{
		Arguments.Add(ArgV[Index]);
	}
	for (const FString& Argument : FileArguments)
	{
		Arguments.Add(const_cast<char*>(*Argument));
	}
	Arguments.Add(nullptr);
	const int32 ExitCode = GuardedMain(Arguments.Num() - 1, Arguments.GetData());
	if (ExitCode != 0)
	{
		FPS2ErrorScreen::Show(ExitCode);
	}
	return ExitCode;
}
