#include "Logging/LogSuppressionInterface.h"

#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/CString.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

namespace
{
	bool ParseVerbosity(const FString& VerbosityName, ELogVerbosity::Type& OutVerbosity)
	{
		if (VerbosityName.Equals("Off", ESearchCase::IgnoreCase))
		{
			OutVerbosity = ELogVerbosity::NoLogging;
			return true;
		}
		OutVerbosity = ParseLogVerbosityFromString(*VerbosityName);
		return OutVerbosity != ELogVerbosity::NoLogging;
	}
} // namespace

FLogSuppressionInterface& FLogSuppressionInterface::Get()
{
	static FLogSuppressionInterface Singleton;
	return Singleton;
}

bool FLogSuppressionInterface::SetCategoryVerbosity(const FString& CategoryName, const FString& VerbosityName)
{
	ELogVerbosity::Type Verbosity;
	if (!ParseVerbosity(VerbosityName, Verbosity))
	{
		return false;
	}

	if (CategoryName.Equals("Global", ESearchCase::IgnoreCase))
	{
		FLogCategoryBase::ForEachCategory(
			[Verbosity](FLogCategoryBase& Category) { Category.SetVerbosity(Verbosity); });
		return true;
	}

	FLogCategoryBase* Category = FLogCategoryBase::FindCategory(FName(*CategoryName));
	if (Category == nullptr)
	{
		return false;
	}
	Category->SetVerbosity(Verbosity);
	return true;
}

void FLogSuppressionInterface::ApplyLogCommands(const FString& Commands)
{
	TArray<FString> Entries;
	Commands.ParseIntoArray(Entries, ",", true);
	for (const FString& Entry : Entries)
	{
		TArray<FString> Parts;
		Entry.ParseIntoArrayWS(Parts);
		if (Parts.Num() == 2)
		{
			SetCategoryVerbosity(Parts[0], Parts[1]);
		}
	}
}

void FLogSuppressionInterface::ProcessConfigAndCommandLine()
{
	if (GConfig != nullptr)
	{
		TArray<FString> Lines;
		GConfig->GetSection("Core.Log", Lines, GEngineIni);
		for (const FString& Line : Lines)
		{
			FString CategoryName;
			FString VerbosityName;
			if (Line.Split("=", &CategoryName, &VerbosityName))
			{
				SetCategoryVerbosity(CategoryName.TrimStartAndEnd(), VerbosityName.TrimStartAndEnd());
			}
		}
	}

	FString LogCommands;
	if (FParse::Value(FCommandLine::Get(), "LogCmds=", LogCommands, false))
	{
		ApplyLogCommands(LogCommands);
	}
}
