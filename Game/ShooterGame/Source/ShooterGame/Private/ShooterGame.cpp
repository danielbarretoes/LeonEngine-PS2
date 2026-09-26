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
