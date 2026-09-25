#include "Commandlets/Commandlet.h"

#include "EngineLogs.h"
#include "Misc/Parse.h"

UCommandlet::UCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsServer = 1;
	IsClient = 1;
	IsEditor = 1;
	LogToConsole = 0;
	ShowErrorCount = 1;
}

int32 UCommandlet::Main(const FString& Params)
{
	(void)Params;
	UE_LOG(LogEngine, Warning, "%s does not implement Main", *GetClass()->GetName());
	return 0;
}

void UCommandlet::ParseCommandLine(
	const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches, TMap<FString, FString>& Params)
{
	FString NextToken;
	while (CmdLine != nullptr && FParse::Token(CmdLine, NextToken, false))
	{
		if (NextToken.StartsWith(TEXT("-")))
		{
			FString Switch = NextToken.Mid(1);
			int32 EqualsIndex = INDEX_NONE;
			if (Switch.FindChar('=', EqualsIndex))
			{
				const FString Key = Switch.Left(EqualsIndex);
				Params.Add(Key, Switch.Mid(EqualsIndex + 1).TrimQuotes());
				Switch = Key;
			}
			Switches.Add(Switch);
		}
		else
		{
			Tokens.Add(NextToken);
		}
	}
}

void UCommandlet::ParseCommandLine(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	TMap<FString, FString> Params;
	ParseCommandLine(CmdLine, Tokens, Switches, Params);
}
