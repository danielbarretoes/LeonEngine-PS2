#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"

/**
 * Run-time log verbosity from config and the command line (UE: FLogSuppressionInterface):
 *   [Core.Log] in the Engine config: "LogTemp=Verbose", "Global=Warning" (every category);
 *   -LogCmds="LogTemp Verbose, LogConfig Off" on the command line (applied after the config).
 */
class CORE_API FLogSuppressionInterface
{
public:
	static FLogSuppressionInterface& Get();

	/** Applies [Core.Log] from GEngineIni, then -LogCmds (UE: ProcessConfigAndCommandLine). */
	void ProcessConfigAndCommandLine();

	/** Applies "Category Verbosity" pairs separated by commas; "Off" silences a category. */
	void ApplyLogCommands(const FString& Commands);

	/** Sets one category ("Global" for all); false for an unknown category or verbosity. */
	bool SetCategoryVerbosity(const FString& CategoryName, const FString& VerbosityName);
};
