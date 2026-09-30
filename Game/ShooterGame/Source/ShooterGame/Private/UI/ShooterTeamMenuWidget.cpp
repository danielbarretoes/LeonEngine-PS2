#include "UI/ShooterTeamMenuWidget.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerController.h"

namespace
{
	/** CS 1.6's team colours on the team lines. */
	const FLinearColor CTColor(0.55f, 0.72f, 1.0f, 1.0f);
	const FLinearColor TColor(1.0f, 0.55f, 0.35f, 1.0f);
} // namespace

UShooterTeamMenuWidget::UShooterTeamMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UShooterTeamMenuWidget::BuildPages()
{
	SetTitle(TEXT("Choose a team"));
	SetHint(TEXT("Cross / Enter: join    Circle / Esc: back"));
	int32 MainPage = 0;
	UVerticalBox* Page = AddPage(MainPage);
	const TPair<EShooterTeamChoice, const TCHAR*> Lines[] = {{EShooterTeamChoice::CT, TEXT("Counter-Terrorists")},
		{EShooterTeamChoice::T, TEXT("Terrorists")}, {EShooterTeamChoice::Auto, TEXT("Auto-select")},
		{EShooterTeamChoice::Spectate, TEXT("Spectate")}};
	for (const TPair<EShooterTeamChoice, const TCHAR*>& Line : Lines)
	{
		UShooterMenuButton* Button = AddMenuButton(*Page, Line.Value);
		const EShooterTeamChoice Choice = Line.Key;
		Button->OnClicked.AddLambda([this, Choice]() { Choose(Choice); });
		ChoiceButtons.Add(Button);
	}
	ChoiceButtons[0]->GetLabel()->SetColorAndOpacity(CTColor);
	ChoiceButtons[1]->GetLabel()->SetColorAndOpacity(TColor);
	CancelButton = AddMenuButton(*Page, TEXT("Cancel"));
	CancelButton->OnClicked.AddLambda([this]() { OnBack(); });
	SetInitialChoice(true);
}

void UShooterTeamMenuWidget::SetInitialChoice(bool bInitial)
{
	bInitialChoice = bInitial;
	CancelButton->SetVisibility(bInitial ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

UShooterMenuButton* UShooterTeamMenuWidget::GetChoiceButton(EShooterTeamChoice Choice) const
{
	const int32 Index = static_cast<int32>(Choice);
	return ChoiceButtons.IsValidIndex(Index) ? ChoiceButtons[Index] : nullptr;
}

void UShooterTeamMenuWidget::NativeTick(float /*DeltaTime*/)
{
	const AShooterPlayerController* Player = GetShooterPlayerController();
	const UWorld* World = Player != nullptr ? Player->GetWorld() : nullptr;
	const AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		return;
	}
	const int32 NumCT = GameMode->GetTeamSize(EShooterTeam::CT);
	const int32 NumT = GameMode->GetTeamSize(EShooterTeam::T);
	if (NumCT != ShownCT)
	{
		ShownCT = NumCT;
		ChoiceButtons[0]->GetLabel()->SetText(
			FText::FromString(FString::Printf(TEXT("Counter-Terrorists  (%d)"), NumCT)));
	}
	if (NumT != ShownT)
	{
		ShownT = NumT;
		ChoiceButtons[1]->GetLabel()->SetText(FText::FromString(FString::Printf(TEXT("Terrorists  (%d)"), NumT)));
	}
}

void UShooterTeamMenuWidget::Choose(EShooterTeamChoice Choice)
{
	PlayMenuSoundStatic(true);
	if (AShooterPlayerController* Player = GetShooterPlayerController())
	{
		Player->JoinTeam(GetShooterTeamChoiceName(Choice));
	}
}

void UShooterTeamMenuWidget::OnBack()
{
	AShooterPlayerController* Player = GetShooterPlayerController();
	if (Player == nullptr)
	{
		return;
	}
	if (bInitialChoice)
	{
		// The first choice has nothing to go back to: the pause menu (resume, or the main menu).
		Player->ShowPauseMenu(true);
	}
	else
	{
		Player->ShowTeamMenu(false);
		Player->ShowPauseMenu(true);
	}
}
