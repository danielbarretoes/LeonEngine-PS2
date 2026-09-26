#include "ShooterGame.h"

#include "Modules/ModuleManager.h"
#include "ShooterTypes.h"

// The game's classes are UObjects the engine finds by reflection (plan decision D18): GlobalDefaultGameMode names
// /Script/ShooterGame.ShooterGameMode, which names the pawn, the controllers, the player state and the HUD. The module
// itself does nothing at startup.
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ShooterGame, "ShooterGame")

DEFINE_LOG_CATEGORY(LogShooter);

FName GetShooterTeamTag(EShooterTeam Team)
{
	switch (Team)
	{
		case EShooterTeam::CT:
			return FName(TEXT("CT"));
		case EShooterTeam::T:
			return FName(TEXT("T"));
		case EShooterTeam::None:
			break;
	}
	return NAME_None;
}

EShooterTeam ParseShooterTeam(const FString& Text)
{
	if (Text.Equals(TEXT("CT"), ESearchCase::IgnoreCase))
	{
		return EShooterTeam::CT;
	}
	if (Text.Equals(TEXT("T"), ESearchCase::IgnoreCase))
	{
		return EShooterTeam::T;
	}
	return EShooterTeam::None;
}

const TCHAR* GetShooterTeamName(EShooterTeam Team)
{
	switch (Team)
	{
		case EShooterTeam::CT:
			return TEXT("CT");
		case EShooterTeam::T:
			return TEXT("T");
		case EShooterTeam::None:
			break;
	}
	return TEXT("None");
}

EShooterTeam GetOpposingTeam(EShooterTeam Team)
{
	switch (Team)
	{
		case EShooterTeam::CT:
			return EShooterTeam::T;
		case EShooterTeam::T:
			return EShooterTeam::CT;
		case EShooterTeam::None:
			break;
	}
	return EShooterTeam::None;
}

const TCHAR* GetRoundEndMessage(EShooterRoundEndReason Reason)
{
	switch (Reason)
	{
		case EShooterRoundEndReason::TargetBombed:
			return TEXT("Target Successfully Bombed!");
		case EShooterRoundEndReason::BombDefused:
			return TEXT("The bomb has been defused!");
		case EShooterRoundEndReason::CTsEliminated:
			return TEXT("Terrorists Win!");
		case EShooterRoundEndReason::TerroristsEliminated:
			return TEXT("Counter-Terrorists Win!");
		case EShooterRoundEndReason::TargetSaved:
			return TEXT("Target has been saved!");
		case EShooterRoundEndReason::Draw:
			return TEXT("Round Draw!");
		case EShooterRoundEndReason::None:
			break;
	}
	return TEXT("");
}

EShooterTeam GetRoundEndWinner(EShooterRoundEndReason Reason)
{
	switch (Reason)
	{
		case EShooterRoundEndReason::TargetBombed:
		case EShooterRoundEndReason::CTsEliminated:
			return EShooterTeam::T;
		case EShooterRoundEndReason::BombDefused:
		case EShooterRoundEndReason::TerroristsEliminated:
		case EShooterRoundEndReason::TargetSaved:
			return EShooterTeam::CT;
		case EShooterRoundEndReason::Draw:
		case EShooterRoundEndReason::None:
			break;
	}
	return EShooterTeam::None;
}
