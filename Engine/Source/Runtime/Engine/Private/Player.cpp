#include "Engine/Player.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

APlayerController* UPlayer::GetPlayerController(const UWorld* InWorld) const
{
	if (PlayerController == nullptr || (InWorld != nullptr && PlayerController->GetWorld() != InWorld))
	{
		return nullptr;
	}
	return PlayerController;
}

bool UPlayer::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	APlayerController* PC = PlayerController;
	if (PC == nullptr || PC->IsPendingKillPending())
	{
		return false;
	}
	// The chain of UE's UPlayer::Exec: each object answers with its Exec UFUNCTIONs.
	APawn* Pawn = PC->GetPawn();
	if (PC->ProcessConsoleExec(Cmd, Ar, Pawn))
	{
		return true;
	}
	if (Pawn != nullptr && Pawn->ProcessConsoleExec(Cmd, Ar, Pawn))
	{
		return true;
	}
	UWorld* World = PC->GetWorld() != nullptr ? PC->GetWorld() : InWorld;
	if (World != nullptr)
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode();
			GameMode != nullptr && GameMode->ProcessConsoleExec(Cmd, Ar, Pawn))
		{
			return true;
		}
		if (AGameStateBase* GameState = World->GetGameState();
			GameState != nullptr && GameState->ProcessConsoleExec(Cmd, Ar, Pawn))
		{
			return true;
		}
		// Leon: the world settings answer too (UE reaches them through the world's Exec).
		if (AWorldSettings* WorldSettings = World->GetWorldSettings();
			WorldSettings != nullptr && WorldSettings->ProcessConsoleExec(Cmd, Ar, Pawn))
		{
			return true;
		}
	}
	return false;
}

FString UPlayer::ConsoleCommand(const FString& Cmd, bool bWriteToLog)
{
	// Several commands may be chained with '|' (UE).
	TArray<FString> Commands;
	Cmd.ParseIntoArray(Commands, TEXT("|"), true);
	FString Output;
	for (const FString& Command : Commands)
	{
		const FString Trimmed = Command.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			continue;
		}
		if (bWriteToLog)
		{
			UE_LOG(LogTemp, Log, TEXT("Cmd: %s"), *Trimmed);
		}
		if (!Exec(PlayerController != nullptr ? PlayerController->GetWorld() : nullptr, *Trimmed, *GLog))
		{
			Output += FString::Printf(TEXT("Command not recognized: %s\n"), *Trimmed);
		}
	}
	if (bWriteToLog && !Output.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("%s"), *Output.TrimEnd());
	}
	return Output;
}
