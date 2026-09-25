#include "Misc/CommandLine.h"

#include "Misc/Parse.h"

namespace
{
	struct FCommandLineStorage
	{
		FString Current;
		FString Original;
		bool bInitialized = false;
	};

	FCommandLineStorage& GetStorage()
	{
		static FCommandLineStorage Storage;
		return Storage;
	}
} // namespace

bool FCommandLine::IsInitialized()
{
	return GetStorage().bInitialized;
}

const TCHAR* FCommandLine::Get()
{
	return *GetStorage().Current;
}

const TCHAR* FCommandLine::GetOriginal()
{
	return *GetStorage().Original;
}

bool FCommandLine::Set(const TCHAR* NewCommandLine)
{
	FCommandLineStorage& Storage = GetStorage();
	if (!Storage.bInitialized)
	{
		Storage.Original = NewCommandLine;
	}
	Storage.Current = NewCommandLine;
	Storage.bInitialized = true;
	return true;
}

void FCommandLine::Append(const TCHAR* AppendString)
{
	GetStorage().Current += AppendString;
}

FString FCommandLine::BuildFromArgV(const TCHAR* Prefix, int32 ArgC, TCHAR* ArgV[], const TCHAR* Suffix)
{
	FString Result;
	if (Prefix != nullptr)
	{
		Result = Prefix;
	}

	for (int32 Index = 1; Index < ArgC; Index++)
	{
		FString Argument(ArgV[Index]);
		if (Argument.Contains(" "))
		{
			FString ArgName;
			FString ArgValue;
			if (Argument.Split("=", &ArgName, &ArgValue))
			{
				Argument = FString::Printf("%s=\"%s\"", *ArgName, *ArgValue);
			}
			else
			{
				Argument = FString::Printf("\"%s\"", *Argument);
			}
		}

		if (Result.Len() > 0)
		{
			Result += " ";
		}
		Result += Argument;
	}

	if (Suffix != nullptr && *Suffix)
	{
		if (Result.Len() > 0)
		{
			Result += " ";
		}
		Result += Suffix;
	}

	return Result;
}

void FCommandLine::Parse(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	FString NextToken;
	while (FParse::Token(CmdLine, NextToken, false))
	{
		if (NextToken[0] == '-')
		{
			Switches.Add(NextToken.Mid(1));
		}
		else
		{
			Tokens.Add(NextToken);
		}
	}
}
