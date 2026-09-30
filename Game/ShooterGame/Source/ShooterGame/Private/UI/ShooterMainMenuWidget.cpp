#include "UI/ShooterMainMenuWidget.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "CoreGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "ShooterGame.h"
#include "ShooterPersistentUser.h"
#include "ShooterPlayerController.h"

namespace
{
	/** Steps Index by Direction within Num, wrapping. */
	int32 StepIndex(int32 Index, int32 Direction, int32 Num)
	{
		return Num > 0 ? (((Index + Direction) % Num) + Num) % Num : 0;
	}
} // namespace

UShooterMainMenuWidget::UShooterMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UShooterMainMenuWidget::BuildPages()
{
	SetTitle(TEXT("ShooterGame"));
	SetHint(TEXT("Up / down: choose    Left / right: change    Cross / Enter: select"));
	int32 MainPage = 0;
	UVerticalBox* Page = AddPage(MainPage);
	MapButton = AddMenuButton(*Page, TEXT("Map"), true);
	MapButton->OnValueStep.AddUObject(this, &UShooterMainMenuWidget::StepMap);
	DifficultyButton = AddMenuButton(*Page, TEXT("Bot difficulty"), true);
	DifficultyButton->OnValueStep.AddUObject(this, &UShooterMainMenuWidget::StepDifficulty);
	RoundsButton = AddMenuButton(*Page, TEXT("Rounds to win"), true);
	RoundsButton->OnValueStep.AddUObject(this, &UShooterMainMenuWidget::StepRoundsToWin);
	BotsButton = AddMenuButton(*Page, TEXT("Bots"), true);
	BotsButton->OnValueStep.AddUObject(this, &UShooterMainMenuWidget::StepBots);
	UShooterMenuButton* Options = AddMenuButton(*Page, TEXT("Options"));
	Options->OnClicked.AddLambda([this]() { ShowOptionsPage(); });
	StartButton = AddMenuButton(*Page, TEXT("Start"));
	StartButton->OnClicked.AddUObject(this, &UShooterMainMenuWidget::StartMatch);
#if PLATFORM_WINDOWS
	// A console game is switched off, not quit (UE ShooterGame's menu hides Quit on consoles).
	UShooterMenuButton* Quit = AddMenuButton(*Page, TEXT("Quit"));
	Quit->OnClicked.AddUObject(this, &UShooterMainMenuWidget::QuitGame);
#endif
	BuildOptionsPage();
}

void UShooterMainMenuWidget::SetShown(bool bShown)
{
	if (bShown)
	{
		AShooterPlayerController* Player = GetShooterPlayerController();
		Settings = Player != nullptr ? Player->GetPersistentUser()->GetMatchSettings() : FShooterMatchSettings();
		if (!MapNames.Contains(Settings.MapName))
		{
			Settings.MapName = MapNames.Num() > 0 ? MapNames[0] : FString();
		}
		Refresh();
	}
	Super::SetShown(bShown);
}

void UShooterMainMenuWidget::OnBack()
{
	if (GetPage() == OptionsPageIndex)
	{
		LeaveOptionsPage();
	}
}

FString UShooterMainMenuWidget::GetMapDisplayName(const FString& MapName)
{
	return MapName.IsEmpty() ? FString(TEXT("-")) : FPackageName::GetShortName(MapName);
}

void UShooterMainMenuWidget::Refresh()
{
	MapButton->SetValueText(GetMapDisplayName(Settings.MapName));
	DifficultyButton->SetValueText(GetBotDifficultyName(Settings.BotDifficulty));
	RoundsButton->SetValueText(FString::Printf(
		TEXT("%d (best of %d)"), Settings.RoundsToWin, FShooterMatchSettings::GetMaxRounds(Settings.RoundsToWin)));
	BotsButton->SetValueText(FString::Printf(TEXT("%d"), Settings.NumBots));
}

void UShooterMainMenuWidget::StepMap(int32 Direction)
{
	const int32 Index = MapNames.IndexOfByKey(Settings.MapName);
	Settings.MapName =
		MapNames.Num() > 0 ? MapNames[StepIndex(FMath::Max(Index, 0), Direction, MapNames.Num())] : FString();
	Refresh();
}

void UShooterMainMenuWidget::StepDifficulty(int32 Direction)
{
	constexpr int32 NumDifficulties = static_cast<int32>(EShooterBotDifficulty::Expert) + 1;
	Settings.BotDifficulty = static_cast<EShooterBotDifficulty>(
		StepIndex(static_cast<int32>(Settings.BotDifficulty), Direction, NumDifficulties));
	Refresh();
}

void UShooterMainMenuWidget::StepRoundsToWin(int32 Direction)
{
	const TArrayView<const int32> Choices = FShooterMatchSettings::GetRoundsToWinChoices();
	const int32 Index = Choices.IndexOfByKey(Settings.RoundsToWin);
	Settings.RoundsToWin = Choices[StepIndex(FMath::Max(Index, 0), Direction, Choices.Num())];
	Refresh();
}

void UShooterMainMenuWidget::StepBots(int32 Direction)
{
	constexpr int32 NumChoices = FShooterMatchSettings::MaxBots - FShooterMatchSettings::MinBots + 1;
	Settings.NumBots = FShooterMatchSettings::MinBots +
		StepIndex(Settings.NumBots - FShooterMatchSettings::MinBots, Direction, NumChoices);
	Refresh();
}

void UShooterMainMenuWidget::StartMatch()
{
	if (Settings.MapName.IsEmpty())
	{
		UE_LOG(LogShooter, Warning, TEXT("Main menu: no map to start (MapNames is empty)"));
		return;
	}
	PlayMenuSoundStatic(true);
	if (AShooterPlayerController* Player = GetShooterPlayerController())
	{
		Player->GetPersistentUser()->SetMatchSettings(Settings);
		Player->SavePersistentUser();
	}
	const FString Options = Settings.GetURLOptions();
	UE_LOG(LogShooter, Display, TEXT("Main menu: starting %s?%s"), *Settings.MapName, *Options);
	UGameplayStatics::OpenLevel(this, FName(*Settings.MapName), true, Options);
}

void UShooterMainMenuWidget::QuitGame()
{
	RequestEngineExit("Main menu: Quit");
}
