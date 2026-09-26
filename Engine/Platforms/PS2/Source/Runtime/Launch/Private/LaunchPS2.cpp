#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformProcess.h"
#include "LaunchEngineLoop.h"
#include "Misc/FileHelper.h"

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

// PS2 entry point (UE: Launch<Platform>.cpp in the platform extension): argv, then LeonCommandLine.txt's arguments.
int main(int ArgC, char* ArgV[])
{
	FPlatformProcess::SetArgV0(ArgV[0]);
	const TArray<FString> FileArguments = ReadCommandLineFile();
	if (FileArguments.Num() == 0)
	{
		return GuardedMain(ArgC, ArgV);
	}
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
	return GuardedMain(Arguments.Num() - 1, Arguments.GetData());
}
