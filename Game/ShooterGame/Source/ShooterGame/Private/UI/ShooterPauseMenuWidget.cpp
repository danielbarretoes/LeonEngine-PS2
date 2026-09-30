#include "UI/ShooterPauseMenuWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPlayerController.h"

UShooterPauseMenuWidget::UShooterPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UShooterPauseMenuWidget::BuildPages()
{
	SetTitle(TEXT("Paused"));
	SetHint(TEXT("Cross / Enter: select    Start / Esc: resume"));
	Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
	int32 MainPage = 0;
	UVerticalBox* Page = AddPage(MainPage);
	StatusText = AddTextLine(*Page, FString(), 14, HintColor);
	ResumeButton = AddMenuButton(*Page, TEXT("Resume"));
	ResumeButton->OnClicked.AddUObject(this, &UShooterPauseMenuWidget::Resume);
	ChangeTeamButton = AddMenuButton(*Page, TEXT("Change team"));
	ChangeTeamButton->OnClicked.AddLambda(
		[this]()
		{
			if (AShooterPlayerController* Player = GetShooterPlayerController())
			{
				Player->ChooseTeam();
			}
		});
	UShooterMenuButton* Options = AddMenuButton(*Page, TEXT("Options"));
	Options->OnClicked.AddLambda([this]() { ShowOptionsPage(); });
	QuitButton = AddMenuButton(*Page, TEXT("Quit to main menu"));
	QuitButton->OnClicked.AddLambda(
		[this]()
		{
			PlayMenuSoundStatic(true);
			if (AShooterPlayerController* Player = GetShooterPlayerController())
			{
				Player->ReturnToMainMenu();
			}
		});
	BuildOptionsPage();
}

void UShooterPauseMenuWidget::NativeTick(float /*DeltaTime*/)
{
	const AShooterPlayerController* Player = GetShooterPlayerController();
	const UWorld* World = Player != nullptr ? Player->GetWorld() : nullptr;
	const AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	const AShooterGameState* State = GameMode != nullptr ? GameMode->GetShooterGameState() : nullptr;
	if (State == nullptr)
	{
		return;
	}
	const FString Status = FString::Printf(TEXT("%s    CT %d - %d T    round %d, the first to %d wins"),
		*FPackageName::GetShortName(World->GetMapName()), State->GetTeamScore(EShooterTeam::CT),
		State->GetTeamScore(EShooterTeam::T), FMath::Max(1, State->GetRoundNumber()), GameMode->GetRoundsToWin());
	if (Status != ShownStatus)
	{
		ShownStatus = Status;
		StatusText->SetText(FText::FromString(Status));
	}
}

FReply UShooterPauseMenuWidget::HandleMenuKey(const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Gamepad_Special_Right && !InKeyEvent.IsRepeat())
	{
		PlayMenuSoundStatic(false);
		Resume();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UShooterPauseMenuWidget::OnBack()
{
	if (GetPage() == OptionsPageIndex)
	{
		LeaveOptionsPage();
	}
	else
	{
		Resume();
	}
}

void UShooterPauseMenuWidget::Resume()
{
	// The options are saved when leaving their page, also by resuming from it.
	if (GetPage() == OptionsPageIndex)
	{
		LeaveOptionsPage();
	}
	if (AShooterPlayerController* Player = GetShooterPlayerController())
	{
		Player->ShowPauseMenu(false);
	}
}
