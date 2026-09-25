#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Commandlet.generated.h"

/**
 * A command-line tool that runs inside the engine instead of the game (UE: UCommandlet): LeonCook's `-run=<Name>`
 * finds the `U<Name>Commandlet` class, makes one and calls Main with the rest of the command line. The base only has
 * the interface; the editor module (LeonEd) brings the commandlets that import, resave, validate and cook content.
 */
UCLASS(Abstract, Transient)
class ENGINE_API UCommandlet : public UObject
{
	GENERATED_BODY()

public:
	UCommandlet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** One line about what the commandlet does (UE: HelpDescription). */
	UPROPERTY()
	FString HelpDescription;

	/** Its usage line (UE: HelpUsage). */
	UPROPERTY()
	FString HelpUsage;

	/** The engine it runs in acts as a server (UE: IsServer; UE's bit names are kept). */
	UPROPERTY()
	uint8 IsServer : 1;
	/** ... as a client (UE: IsClient). */
	UPROPERTY()
	uint8 IsClient : 1;
	/** ... as an editor (UE: IsEditor). */
	UPROPERTY()
	uint8 IsEditor : 1;

	/** The log also goes to the console (UE: LogToConsole). */
	UPROPERTY()
	uint8 LogToConsole : 1;

	/** The error and warning counts are shown when it ends (UE: ShowErrorCount). */
	UPROPERTY()
	uint8 ShowErrorCount : 1;

	/**
	 * Runs the commandlet (UE: Main). Params is the command line after `-run=<Name>`. Returns the process exit code: 0
	 * on success.
	 */
	virtual int32 Main(const FString& Params);

	/**
	 * Splits a command line into its tokens (words that are not switches), its switches (words after `-`,
	 * without it) and the switches' values (`-Name=Value`, quotes removed) (UE: ParseCommandLine).
	 */
	static void ParseCommandLine(
		const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches, TMap<FString, FString>& Params);
	static void ParseCommandLine(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches);
};
