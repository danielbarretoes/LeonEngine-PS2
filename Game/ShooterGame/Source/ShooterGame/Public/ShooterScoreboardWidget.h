#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "ShooterTypes.h"
#include "ShooterScoreboardWidget.generated.h"

class AShooterPlayerController;
class UBorder;
class UTableView;
class UTextBlock;
class UVerticalBox;

/**
 * The scoreboard (CS 1.6's, while Tab or Select is held; ps2-polish P6), a tree of UMG widgets the HUD owns: a panel in
 * the middle of the screen with the map's name, then each team's section, the counter-terrorists' first: a heading
 * (the team's name and players in its colour, its score in the HUD's number font) over a UTableView of its players,
 * one row each: the name, the status (DEAD, and BOMB on the bomb's carrier for a terrorist or a spectator watching),
 * the score (the kills, CS's frags), the deaths and BOT or the latency. The rows are sorted by score, most first, ties
 * by fewer deaths (the order they were added), and the local player's row is highlighted; a dead player's row is
 * dimmed. The players without a team (a bot match's spectator) are listed under the tables.
 *
 * NativeTick shows the panel while the owner's scoreboard is shown and fills the tables again only when what they show
 * changed (a key of the scores and each player's team, kills, deaths, life and the bomb), so a held Tab costs a key's
 * comparison a frame.
 */
UCLASS()
class SHOOTERGAME_API UShooterScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UShooterScoreboardWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void NativeOnInitialized() override;
	void NativeTick(float DeltaTime) override;

	/** The tables' columns. */
	static const FName NameColumn;
	static const FName StatusColumn;
	static const FName ScoreColumn;
	static const FName DeathsColumn;
	static const FName LatencyColumn;

	/** The panel (collapsed while the scoreboard is not shown). */
	[[nodiscard]] UBorder* GetPanel() const
	{
		return Panel;
	}
	/** A team's table (CT or T; null for None). */
	[[nodiscard]] UTableView* GetTeamTable(EShooterTeam Team) const;
	/** A team's score as its heading shows it. */
	[[nodiscard]] UTextBlock* GetTeamScoreText(EShooterTeam Team) const;
	/** The line listing the spectators (empty without any). */
	[[nodiscard]] UTextBlock* GetSpectatorsText() const
	{
		return SpectatorsText;
	}
	/** How many times the tables were filled. */
	[[nodiscard]] int32 GetNumRefreshes() const
	{
		return NumRefreshes;
	}

private:
	/** Fills the headings and the tables from the game, as Viewer sees them. */
	void Refresh(const AShooterPlayerController& Viewer);
	/** A team's heading: its name, its players and its score, right-aligned. */
	void AddTeamHeading(UVerticalBox& Box, EShooterTeam Team, UTextBlock*& OutName, UTextBlock*& OutScore);
	/** A team's table with its columns, sorted by score. */
	UTableView* AddTeamTable(UVerticalBox& Box);

	/** What the board showed last (the scores, then each player's), and this frame's. */
	TArray<int64> ShownKey;
	TArray<int64> NewKey;
	int32 NumRefreshes = 0;

	UPROPERTY()
	UBorder* Panel = nullptr;
	UPROPERTY()
	UTextBlock* TitleText = nullptr;
	UPROPERTY()
	UTextBlock* CTNameText = nullptr;
	UPROPERTY()
	UTextBlock* CTScoreText = nullptr;
	UPROPERTY()
	UTableView* CTTable = nullptr;
	UPROPERTY()
	UTextBlock* TNameText = nullptr;
	UPROPERTY()
	UTextBlock* TScoreText = nullptr;
	UPROPERTY()
	UTableView* TTable = nullptr;
	UPROPERTY()
	UTextBlock* SpectatorsText = nullptr;
};
