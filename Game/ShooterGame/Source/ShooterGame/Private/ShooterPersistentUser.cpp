#include "ShooterPersistentUser.h"

#include "Kismet/GameplayStatics.h"
#include "ShooterGame.h"

UShooterPersistentUser::UShooterPersistentUser(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UShooterPersistentUser* UShooterPersistentUser::LoadPersistentUser(int32 UserIndex)
{
	UShooterPersistentUser* User = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(GetSlotName(), UserIndex))
	{
		User = Cast<UShooterPersistentUser>(UGameplayStatics::LoadGameFromSlot(GetSlotName(), UserIndex));
		if (User == nullptr)
		{
			UE_LOG(LogShooter, Warning, TEXT("The settings could not be loaded (%s): the defaults are used"),
				LexToString(UGameplayStatics::GetLastSaveGameResult()));
		}
		else
		{
			UE_LOG(LogShooter, Log, TEXT("Settings loaded: sensitivity %.2f, Y axis %s, volume %.2f, crouch %s"),
				static_cast<double>(User->AimSensitivity), User->bInvertedYAxis ? TEXT("inverted") : TEXT("normal"),
				static_cast<double>(User->SoundVolume), User->bToggleCrouch ? TEXT("toggles") : TEXT("held"));
		}
	}
	if (User == nullptr)
	{
		User = Cast<UShooterPersistentUser>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
	}
	return User;
}

FShooterMatchSettings UShooterPersistentUser::GetMatchSettings() const
{
	FShooterMatchSettings Settings;
	Settings.MapName = MatchMapName;
	Settings.BotDifficulty = BotDifficulty;
	Settings.RoundsToWin = FShooterMatchSettings::GetRoundsToWinChoices().Contains(RoundsToWin)
		? RoundsToWin
		: FShooterMatchSettings::GetRoundsToWinChoices()[0];
	Settings.NumBots = FMath::Clamp(NumBots, FShooterMatchSettings::MinBots, FShooterMatchSettings::MaxBots);
	return Settings;
}

void UShooterPersistentUser::SetMatchSettings(const FShooterMatchSettings& Settings)
{
	MatchMapName = Settings.MapName;
	BotDifficulty = Settings.BotDifficulty;
	RoundsToWin = Settings.RoundsToWin;
	NumBots = Settings.NumBots;
}

bool UShooterPersistentUser::SaveToSlot(int32 UserIndex)
{
	const bool bSaved = UGameplayStatics::SaveGameToSlot(this, GetSlotName(), UserIndex);
	if (!bSaved)
	{
		UE_LOG(LogShooter, Warning, TEXT("The settings were not saved: %s"),
			LexToString(UGameplayStatics::GetLastSaveGameResult()));
	}
	return bSaved;
}
