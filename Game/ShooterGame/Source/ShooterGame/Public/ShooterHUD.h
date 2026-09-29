#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ShooterHUD.generated.h"

class AShooterCharacter;
class AShooterGameState;
class AShooterPlayerController;
class UBorder;
class UTextBlock;
class UVerticalBox;

/**
 * The values a line of HUD text is made of (up to four integers: a count, a whole second, an enum, a pointer or a
 * name's index), so the line is formatted again only when one of them changes.
 */
struct FShooterHUDTextKey
{
	int64 Values[4] = {0, 0, 0, 0};

	bool operator==(const FShooterHUDTextKey& Other) const
	{
		return Values[0] == Other.Values[0] && Values[1] == Other.Values[1] && Values[2] == Other.Values[2] &&
			Values[3] == Other.Values[3];
	}
};

/**
 * A line of HUD text kept between frames (ps2-shipping N20: FString::Printf and the text's measure cost the EE every
 * frame): Update formats it, and measures it for the HUD font, only when its key changed.
 */
struct FShooterHUDText
{
	FString Text;
	float Width = 0.0f;
	float Height = 0.0f;

	/** Formats Text with Format() and measures it when Key differs from the last one (or the first time): true then. */
	template <typename FormatType>
	bool Update(const FShooterHUDTextKey& Key, FormatType&& Format)
	{
		if (bFormatted && Key == LastKey)
		{
			return false;
		}
		Set(Format());
		LastKey = Key;
		return true;
	}

	/** Takes InText and measures it (a line whose key the caller keeps). */
	void Set(const FString& InText);

private:
	FShooterHUDTextKey LastKey;
	bool bFormatted = false;
};

/**
 * ShooterGame's HUD (UE ShooterGame: AShooterHUD), Counter-Strike's layout:
 * - the crosshair at the centre, its gap growing with the weapon's spread (the dynamic crosshair), and the hit marker
 *   (four diagonal ticks, red on a kill) for HitMarkerDuration after a confirmed hit; the AWP's scope (a square view
 *   between black side bars, thin black cross lines) instead of the crosshair while zoomed;
 * - bottom left the health, the armor (H: a helmet) and the money, bottom right the weapon with its clip and reserve,
 *   the bomb (C4) and the defuse kit when carried;
 * - top centre the round's clock (the freeze, then the round's time; the bomb once planted) and the score; top right
 *   the kill feed (the last kills, for KillFeedDuration);
 * - bottom left above the money the radio (CS's chat area): the viewer's team's last messages for
 *   RadioMessageDuration, "<sender> (RADIO): <message>", the sender in the team's colour; the radio menu (Z, X, C)
 *   where the buy menu sits while it is open;
 * - the centre's messages (the warmup, the round's result, the match's end) and a bar while planting or defusing;
 * - the scoreboard while Tab is held (each team's players: kills, deaths, money, dead ones marked);
 * - top left the radar (CS 1.6's): a square around the view's position that turns with its yaw (ahead is up), the
 *   living teammates as dots, for the terrorists the bomb's carrier (a bigger red dot) or the bomb on the floor or
 *   planted, and the bomb sites' letters; what lies beyond RadarRange sits on the edge. The team is the player's, or
 *   (a bot match's spectator) the watched player's. At most about 20 filled rectangles, lines and letters a frame,
 *   into the frame's canvas (no allocation);
 * - the damage direction indicator: for DamageIndicatorDuration after the player is hurt, an arc around the centre
 *   toward where the damage came from (ahead is up), shrinking and darkening as it fades;
 * - a flashbang's white (AShooterCharacter::GetFlashAlpha): one full-screen alpha-blended tile under the rest of the
 *   HUD, held and then faded;
 * - while spectating, at the bottom, who is watched ("Killed by" the killer during the death cam, "Spectating" a
 *   teammate with its health, or the free look);
 * - the buy menu (UShooterBuyMenuWidget, a UMG widget the HUD owns) while it is open, and where it would be, for
 *   BuyRefusalDuration, why it did not open or closed by itself (AShooterPlayerController::CanOpenBuyMenu).
 *
 * Its text is kept between frames (FShooterHUDText): a line is formatted and measured again only when the values it
 * shows change (the money, the health, the clock's second, the score, the kill feed, the scoreboard's players).
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterHUD : public AHUD
{
	GENERATED_BODY()

public:
	AShooterHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Adds the buy menu widget. */
	void BeginPlay() override;
	/** Draws the HUD into Canvas (UE: DrawHUD); the widgets paint after it. */
	void DrawHUD() override;

	/** The crosshair's colour (CS's default green). */
	UPROPERTY(Config)
	FLinearColor CrosshairColor = FLinearColor(0.0f, 1.0f, 0.0f);

	/** The gap between the centre and each arm with no spread, pixels. */
	UPROPERTY(Config)
	float CrosshairGap = 4.0f;

	/** The length of each arm, pixels. */
	UPROPERTY(Config)
	float CrosshairLength = 7.0f;

	/** The thickness of the arms, pixels. */
	UPROPERTY(Config)
	float CrosshairThickness = 2.0f;

	/** How much the weapon's spread opens the crosshair: the spread's pixels at the screen times this. */
	UPROPERTY(Config)
	float CrosshairSpreadScale = 1.0f;

	/** Seconds the hit marker shows after a hit. */
	UPROPERTY(Config)
	float HitMarkerDuration = 0.25f;

	/** Seconds the buy menu's refusal ("You cannot buy now: ...") shows after the menu was refused or closed. */
	UPROPERTY(Config)
	float BuyRefusalDuration = 2.0f;

	/** Seconds a kill stays in the kill feed. */
	UPROPERTY(Config)
	float KillFeedDuration = 6.0f;

	/** The status text's colour (CS's amber). */
	UPROPERTY(Config)
	FLinearColor StatusColor = FLinearColor(1.0f, 0.75f, 0.2f);

	/** The radar's side, pixels (0: no radar). */
	UPROPERTY(Config)
	float RadarSize = 88.0f;

	/** The distance from the radar's centre to its edge, cm. */
	UPROPERTY(Config)
	float RadarRange = 2500.0f;

	/** Seconds the damage direction indicator shows after a hit. */
	UPROPERTY(Config)
	float DamageIndicatorDuration = 1.0f;

	/** Seconds a radio message stays on the HUD. */
	UPROPERTY(Config)
	float RadioMessageDuration = 6.0f;

	/** The indicator's distance from the centre, pixels, and its arc when fresh, degrees. */
	UPROPERTY(Config)
	float DamageIndicatorRadius = 64.0f;

	UPROPERTY(Config)
	float DamageIndicatorArc = 50.0f;

	/**
	 * Where Location shows on a radar centred on Origin that turns with the view's Yaw (degrees): the offset from the
	 * radar's centre in pixels (X right, Y down: ahead is up), Range cm to HalfSize pixels. A location beyond the
	 * square sits on its edge, in its direction.
	 */
	[[nodiscard]] static FVector2D ProjectToRadar(
		const FVector& Origin, float Yaw, const FVector& Location, float Range, float HalfSize);
	/**
	 * The direction of Source seen from a view at ViewLocation turned to ViewYaw, degrees in (-180, 180]: 0 ahead, 90
	 * to the right, -90 to the left, 180 behind (the damage indicator's angle; on the screen 0 is up).
	 */
	[[nodiscard]] static float GetDamageIndicatorAngle(
		const FVector& ViewLocation, float ViewYaw, const FVector& Source);
	/** The radar's rectangles, lines and letters drawn in the last frame. */
	[[nodiscard]] int32 GetNumRadarPrimitives() const
	{
		return NumRadarPrimitives;
	}
	/** How white the last frame's flashbang tile was (0: none drawn). */
	[[nodiscard]] float GetFlashOverlayAlpha() const
	{
		return FlashOverlayAlpha;
	}
	/** The damage indicator's lines drawn in the last frame (0 when it did not show). */
	[[nodiscard]] int32 GetNumDamageIndicatorLines() const
	{
		return NumDamageIndicatorLines;
	}
	/** The radio lines drawn in the last frame (the viewer's team's, the oldest first). */
	[[nodiscard]] int32 GetNumRadioLines() const
	{
		return DrawnRadioLines.Num();
	}
	/** A radio line of the last frame: "<sender> (RADIO): <message>". */
	[[nodiscard]] FString GetRadioLineText(int32 Index) const;
	/** The colour its sender was drawn in (the team's). */
	[[nodiscard]] FLinearColor GetRadioLineColor(int32 Index) const;
	/** The radio menu's lines drawn in the last frame (its title and messages; 0 when closed). */
	[[nodiscard]] int32 GetNumRadioMenuLines() const
	{
		return NumRadioMenuLines;
	}
	/** The spectator's line of the last frame (empty while playing). */
	[[nodiscard]] const FString& GetSpectatorText() const
	{
		return SpectatorText.Text;
	}

	/** The pawn the HUD shows (the owner's), or null. */
	[[nodiscard]] AShooterCharacter* GetViewedPawn() const;
	/** The world's game state, as the game's class, or null. */
	[[nodiscard]] AShooterGameState* GetShooterGameState() const;
	/** The crosshair's gap now: CrosshairGap plus the drawn weapon's spread on screen, pixels. */
	[[nodiscard]] float GetCrosshairGap() const;
	/** The owner as the game's controller. */
	[[nodiscard]] AShooterPlayerController* GetShooterPlayerController() const;

	/** How many lines of text the HUD has formatted (the text cache's test). */
	[[nodiscard]] int32 GetNumTextFormats() const
	{
		return NumTextFormats;
	}

private:
	/** The health, the armor, the money and the weapon's ammunition. */
	void DrawStatus();
	/** The round's clock and the score. */
	void DrawRoundInfo();
	/** The last kills. */
	void DrawKillFeed();
	/** The team's last radio messages, bottom left above the money (CS's chat area). */
	void DrawRadio();
	/** The radio menu while it is open (Z, X, C), where the buy menu sits. */
	void DrawRadioMenu();
	/** The warmup, the round's result, the match's end, the bomb. */
	void DrawMessages();
	/** Why the buy menu did not open or closed by itself, where the menu sits, for BuyRefusalDuration. */
	void DrawBuyRefusal();
	/** A bar under the crosshair while planting or defusing. */
	void DrawProgress();
	/** The ticks of a confirmed hit. */
	void DrawHitMarker();
	/** The sniper's scope: true when drawn (the crosshair is not). */
	bool DrawScope();
	/** The crosshair: four arms around the centre. */
	void DrawCrosshair();
	/** The players by team (Tab held). */
	void DrawScoreboard();
	/** The radar, top left. */
	void DrawRadar();
	/** The arc toward the last damage's source. */
	void DrawDamageIndicator();
	/** The flashbang's white over the view. */
	void DrawFlash();
	/** Who the spectator watches. */
	void DrawSpectatorInfo();
	[[nodiscard]] float GetWorldTime() const;
	/** Line.Update, counted in NumTextFormats. */
	template <typename FormatType>
	void UpdateText(FShooterHUDText& Line, const FShooterHUDTextKey& Key, FormatType&& Format)
	{
		NumTextFormats += Line.Update(Key, Forward<FormatType>(Format)) ? 1 : 0;
	}

	/** One kill feed line: its three parts, measured. */
	struct FKillFeedLine
	{
		FShooterHUDText Killer;
		FShooterHUDText Middle;
		FShooterHUDText Victim;
	};

	/** The lines of text kept between frames (FShooterHUDText). */
	FShooterHUDText MoneyText;
	FShooterHUDText StatusText;
	FShooterHUDText WeaponText;
	FShooterHUDText ItemsText;
	FShooterHUDText ClockText;
	FShooterHUDText CTScoreText;
	FShooterHUDText TScoreText;
	FShooterHUDText RoundText;
	FShooterHUDText MessageText;
	FShooterHUDText ScoreboardCTText;
	FShooterHUDText ScoreboardTText;
	FShooterHUDText SpectatorText;
	/** The bomb sites' letters on the radar (the game mode's sites, made once). */
	TArray<FString> RadarSiteLabels;
	int32 NumRadarPrimitives = 0;
	int32 NumDamageIndicatorLines = 0;
	float FlashOverlayAlpha = 0.0f;
	/** The kill feed's lines, formatted when the game state's feed changes (its serial). */
	TArray<FKillFeedLine> KillFeedLines;
	int32 KillFeedSerial = -1;

	/** One radio line: the sender's part (in the team's colour) and the message. */
	struct FRadioLine
	{
		FShooterHUDText Sender;
		FShooterHUDText Message;
		FLinearColor Color = FLinearColor::White;
	};
	/** The radio log's lines, formatted when the game state's log changes (its serial), and the ones drawn. */
	TArray<FRadioLine> RadioLines;
	int32 RadioSerial = -1;
	TArray<int32, TInlineAllocator<8>> DrawnRadioLines;
	/** The radio menu's title and messages, formatted when another menu opens. */
	FShooterHUDText RadioMenuTitle;
	TArray<FShooterHUDText> RadioMenuLines;
	int32 RadioMenuShown = 0;
	int32 NumRadioMenuLines = 0;
	/** What the scoreboard showed (each player's state, team, kills, deaths, money and life), and this frame's. */
	TArray<int64> ScoreboardKey;
	TArray<int64> NewScoreboardKey;
	int32 NumTextFormats = 0;
};

/**
 * The buy menu (CS's), a tree of UMG widgets: a bordered vertical box at the top-left with the page's name and the
 * money, why buying is refused (outside a buy zone, after the buy time), the lines of the page
 * (AShooterPlayerController::GetBuyMenuEntries) on their number keys, a category with '>', an item with its price
 * (grey when it cannot be bought now; '>' before the number marks the pad's highlighted line; the lines a page does not
 * use collapsed) and the last buy's result. NativeTick refreshes it from the game, and collapses it while the owner's
 * menu is closed.
 */
UCLASS()
class SHOOTERGAME_API UShooterBuyMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UShooterBuyMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void NativeOnInitialized() override;
	void NativeTick(float DeltaTime) override;

	/** The panel (collapsed while the menu is closed). */
	[[nodiscard]] UBorder* GetPanel() const
	{
		return Panel;
	}
	/** The line of the entry at Index of the page (GetBuyMenuEntries), or null. */
	[[nodiscard]] UTextBlock* GetItemText(int32 Index) const
	{
		return ItemTexts.IsValidIndex(Index) ? ItemTexts[Index] : nullptr;
	}
	/** How many lines of text the menu has set (a line is set only when what it shows changes). */
	[[nodiscard]] int32 GetNumTextFormats() const
	{
		return NumTextFormats;
	}

private:
	/** Adds a text line to the box. */
	UTextBlock* AddLine(UVerticalBox& Box, float TopPadding);
	/** Sets Block's text to Text when it differs from Shown (what Block shows), counted in NumTextFormats. */
	void SetLineText(UTextBlock& Block, FString& Shown, const FString& Text);

	/** What the lines show: set again only when it changes (FShooterHUDText; the refusal and the last buy as text). */
	FShooterHUDText MoneyLine;
	FString ShownRefusal;
	TArray<FShooterHUDText> ItemLines;
	FString ShownLastBuy;
	int32 NumTextFormats = 0;

	UPROPERTY()
	UBorder* Panel = nullptr;
	UPROPERTY()
	UTextBlock* MoneyText = nullptr;
	UPROPERTY()
	UTextBlock* RefusalText = nullptr;
	UPROPERTY()
	TArray<UTextBlock*> ItemTexts;
	UPROPERTY()
	UTextBlock* LastBuyText = nullptr;
};
