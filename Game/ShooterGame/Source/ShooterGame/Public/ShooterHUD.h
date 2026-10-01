#pragma once

#include "Blueprint/UserWidget.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UObject/SoftObjectPath.h"
#include "ShooterHUD.generated.h"

class AShooterCharacter;
class AShooterGameState;
struct FWorldOverviewSettings;
class AShooterPlayerController;
class UBorder;
class UFont;
class UShooterScoreboardWidget;
class UTableView;
class UTextBlock;
class UTexture2D;
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
 * frame): Update formats it, and measures it in its font, only when its key changed.
 */
struct FShooterHUDText
{
	FString Text;
	float Width = 0.0f;
	float Height = 0.0f;
	/** The font it is measured and drawn in (null: the HUD's small font). */
	const UFont* Font = nullptr;

	/** Formats Text with Format() and measures it in InFont when Key differs from the last one (or the first time). */
	template <typename FormatType>
	bool Update(const FShooterHUDTextKey& Key, FormatType&& Format, const UFont* InFont = nullptr)
	{
		if (bFormatted && Key == LastKey && InFont == Font)
		{
			return false;
		}
		Set(Format(), InFont);
		LastKey = Key;
		return true;
	}

	/** Takes InText and measures it in InFont (a line whose key the caller keeps). */
	void Set(const FString& InText, const UFont* InFont = nullptr);

private:
	FShooterHUDTextKey LastKey;
	bool bFormatted = false;
};

/**
 * The icons of the HUD's atlas (/Game/UI/T_HUDIcons, made by SourceArt/UI/make_hud_icons.py): the kill feed's weapons,
 * the bomb, the world and the headshot (16 pixels high), and the status icons (24 x 24). AShooterHUD::GetIconRect has
 * their rectangles, the script's table.
 */
enum class EShooterHUDIcon : uint8
{
	AK47,
	M4A1,
	AWP,
	MP5,
	Glock,
	USP,
	Deagle,
	Knife,
	HEGrenade,
	Flashbang,
	SmokeGrenade,
	C4,
	World,
	Headshot,
	Health,
	Armor,
	ArmorHelmet,
	BuyZone,
	Bomb,
	Defuser,
	Clock,
	Num,
};

/**
 * ShooterGame's HUD (UE ShooterGame: AShooterHUD), Counter-Strike 1.6's layout on the 640 x 448 frame, its text in the
 * engine's DejaVu Sans Condensed (the numbers in its bold face, NumberFontName) with a one-pixel drop shadow over the
 * world, its icons from one atlas (IconsTextureName, tinted as they are drawn):
 * - the crosshair at the centre, its gap growing with the weapon's spread (the dynamic crosshair), and the hit marker
 *   (four diagonal ticks, red on a kill) for HitMarkerDuration after a confirmed hit; the AWP's scope (a square view
 *   between black side bars, thin black cross lines) instead of the crosshair while zoomed;
 * - bottom left the health (a cross, red at 25 or less) and the armor (a vest, with a helmet when it has one); bottom
 *   right the ammunition (clip | reserve), the weapon's name above it (its DisplayName, "(silenced)" or "(burst)"
 *   after it; C4 while the bomb is drawn), and above them the money with the buy zone's cart while the player may buy;
 *   above the money for PickupNoticeDuration what the player last picked up ("Picked up AK-47", CS's pickup notice);
 * - on the left, halfway down, the bomb for its carrier (blinking inside a bomb site, CS's) and the defuse kit;
 * - top centre the round's clock with a stopwatch (the freeze, then the round's time; the bomb and C4 once planted)
 *   between the teams' scores, the round's number under it; top right the kill feed (the last kills for
 *   KillFeedDuration: the killer, the weapon's icon, the headshot's, the victim, on a dark band);
 * - bottom left above the health the radio (CS's chat area): the viewer's team's last messages for
 *   RadioMessageDuration, "<sender> (RADIO): <message>", the sender in the team's colour; the radio menu (Z, X, C)
 *   where the buy menu sits while it is open;
 * - the centre's messages (the warmup, the round's result, the match's end) and a bar while planting or defusing;
 * - the scoreboard while Tab is held: UShooterScoreboardWidget, a UMG widget the HUD owns;
 * - under the radar a compact frame readout ("30 fps  33.3 ms", the player's option bShowFrameStats, on by default);
 * - top left the radar (CS 1.6's): a square around the view's position that turns with its yaw (ahead is up), over
 *   the map's overview when it has one (its world settings' OverviewSettings, ps2-polish P7: the overview turned with
 *   the view and clipped to the square and to the overview's own edges, as textured triangles), the living teammates
 *   as dots, for the terrorists the bomb's carrier (a bigger red dot) or the bomb on the floor or
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
 * shows change (the money, the health, the clock's second, the score, the kill feed).
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterHUD : public AHUD
{
	GENERATED_BODY()

public:
	AShooterHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Loads the icons and the fonts, and adds the buy menu and the scoreboard widgets. */
	void BeginPlay() override;
	/** Draws the HUD into Canvas (UE: DrawHUD); the widgets paint after it. */
	void DrawHUD() override;

	/** The icons' atlas (a UTexture2D: SourceArt/UI/make_hud_icons.py's). */
	UPROPERTY(Config)
	FSoftObjectPath IconsTextureName;

	/** The font of the HUD's numbers (the health, the armor, the ammunition, the money, the clock, the scores). */
	UPROPERTY(Config)
	FSoftObjectPath NumberFontName;

	/** The font of headings (the scoreboard's and the buy menu's). */
	UPROPERTY(Config)
	FSoftObjectPath BoldFontName;

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

	/** Seconds the pickup notice ("Picked up AK-47") shows after a pickup. */
	UPROPERTY(Config)
	float PickupNoticeDuration = 2.0f;

	/** Seconds a kill stays in the kill feed. */
	UPROPERTY(Config)
	float KillFeedDuration = 6.0f;

	/** The status text's and icons' colour (CS's amber). */
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

	/** An icon's rectangle in the atlas, texels (the script's table). */
	static void GetIconRect(EShooterHUDIcon Icon, float& OutU, float& OutV, float& OutUL, float& OutVL);
	/** The kill feed's icon of a weapon's buy name (the feed's WeaponName): the world's skull for anything else. */
	[[nodiscard]] static EShooterHUDIcon GetKillFeedIcon(const FString& WeaponName);
	/** Icon of the loaded atlas (no texture before BeginPlay or without the atlas). */
	[[nodiscard]] FCanvasIcon MakeHUDIcon(EShooterHUDIcon Icon) const;
	/** The icons' atlas, once loaded. */
	[[nodiscard]] const UTexture2D* GetIconsTexture() const
	{
		return IconsTexture;
	}
	/** The HUD's fonts: text (the engine's small font), numbers and headings (their fallbacks: the engine's). */
	[[nodiscard]] static const UFont* GetTextFont();
	[[nodiscard]] const UFont* GetNumberFont() const;
	[[nodiscard]] const UFont* GetBoldFont() const;

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
	/**
	 * The radar's overview (ps2-polish P7): the triangles that draw the part of the map's overview a radar square
	 * shows, the square at (Left, Top) Size pixels on a side, centred on Origin, turned with the view's Yaw (degrees),
	 * Range cm from its centre to its edge. The square's corners take the overview's UVs (north up at yaw 0), and the
	 * square is clipped where the overview ends (Sutherland-Hodgman against its UV square, then a fan). Returns the
	 * triangles' count: 0 off the overview or without one, 2 inside it, at most 6.
	 */
	static int32 MakeRadarOverviewTriangles(const FWorldOverviewSettings& Overview, const FVector& Origin, float Yaw,
		float Range, float Left, float Top, float Size, TArray<FCanvasUVTri, TInlineAllocator<8>>& OutTriangles);
	/** Whether the last frame's radar drew the map's overview. */
	[[nodiscard]] bool WasRadarOverviewDrawn() const
	{
		return bRadarOverviewDrawn;
	}
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
	/** The pickup notice drawn in the last frame, or empty. */
	[[nodiscard]] FString GetPickupNoticeText() const
	{
		return bPickupNoticeShown ? PickupText.Text : FString();
	}
	/** The weapon's name drawn in the last frame ("AK-47", "USP (silenced)", "C4" with the bomb drawn). */
	[[nodiscard]] const FString& GetWeaponText() const
	{
		return WeaponText.Text;
	}
	/** The ammunition drawn in the last frame ("30 | 90"; empty for the knife and the bomb). */
	[[nodiscard]] const FString& GetAmmoText() const
	{
		return AmmoText.Text;
	}
	/** The kill feed's lines drawn in the last frame (the newest last). */
	[[nodiscard]] int32 GetNumKillFeedLines() const
	{
		return NumKillFeedLinesDrawn;
	}
	/** The frame readout drawn in the last frame ("30 fps  33.3 ms"), or empty (off, or not measured yet). */
	[[nodiscard]] FString GetFrameStatsText() const
	{
		return bFrameStatsShown ? FrameStatsText.Text : FString();
	}
	/** Seconds the frame readout averages over (it is formatted again that often). */
	UPROPERTY(Config)
	float FrameStatsRefreshSeconds = 0.5f;
	/** The top of the menus' place (the buy menu, the radio menu, a refusal): under the radar and the frame readout. */
	[[nodiscard]] float GetMenuTop() const;
	/** The scoreboard widget (null before BeginPlay). */
	[[nodiscard]] UShooterScoreboardWidget* GetScoreboardWidget() const
	{
		return ScoreboardWidget;
	}

	/** The pawn the HUD shows (the owner's), or null. */
	[[nodiscard]] AShooterCharacter* GetViewedPawn() const;
	/** The world's game state, as the game's class, or null. */
	[[nodiscard]] AShooterGameState* GetShooterGameState() const;
	/**
	 * The crosshair's gap now on a view ViewHeight pixels high: CrosshairGap plus the drawn weapon's spread on screen,
	 * pixels (CS's dynamic crosshair: tighter crouched, wider moving and in the air).
	 */
	[[nodiscard]] float GetCrosshairGap(float ViewHeight) const;
	/** The owner as the game's controller. */
	[[nodiscard]] AShooterPlayerController* GetShooterPlayerController() const;

	/** How many lines of text the HUD has formatted (the text cache's test). */
	[[nodiscard]] int32 GetNumTextFormats() const
	{
		return NumTextFormats;
	}

private:
	/** The health and the armor, bottom left. */
	void DrawStatus();
	/** The ammunition, the weapon, the money and the buy zone, bottom right. */
	void DrawWeaponAndMoney();
	/** The bomb and the defuse kit, on the left halfway down. */
	void DrawStatusIcons();
	/** The round's clock and the score. */
	void DrawRoundInfo();
	/** The last kills. */
	void DrawKillFeed();
	/** The team's last radio messages, bottom left above the health (CS's chat area). */
	void DrawRadio();
	/** The radio menu while it is open (Z, X, C), where the buy menu sits. */
	void DrawRadioMenu();
	/** The warmup, the round's result, the match's end, the bomb. */
	void DrawMessages();
	/** Why the buy menu did not open or closed by itself, where the menu sits, for BuyRefusalDuration. */
	void DrawBuyRefusal();
	/** What the player last picked up, bottom right above the money, for PickupNoticeDuration. */
	void DrawPickupNotice();
	/** A bar under the crosshair while planting or defusing. */
	void DrawProgress();
	/** The ticks of a confirmed hit. */
	void DrawHitMarker();
	/** The sniper's scope: true when drawn (the crosshair is not). */
	bool DrawScope();
	/** The crosshair: four arms around the centre. */
	void DrawCrosshair();
	/** The radar, top left. */
	void DrawRadar();
	/** The arc toward the last damage's source. */
	void DrawDamageIndicator();
	/** The flashbang's white over the view. */
	void DrawFlash();
	/** Who the spectator watches. */
	void DrawSpectatorInfo();
	/** The frame's rate and time under the radar (the player's option, bShowFrameStats). */
	void DrawFrameStats();
	/** Text over the world: a one-pixel drop shadow under it. */
	void DrawHUDText(const UFont* Font, const FString& Text, float X, float Y, const FLinearColor& Color,
		ETextJustify Justify = ETextJustify::Left);
	/** A kept line over the world, justified at X. */
	void DrawHUDText(const FShooterHUDText& Line, float X, float Y, const FLinearColor& Color,
		ETextJustify Justify = ETextJustify::Left);
	/** An icon of the atlas at X, Y, tinted by Color (nothing without the atlas). */
	void DrawHUDIcon(EShooterHUDIcon Icon, float X, float Y, const FLinearColor& Color);
	/** The bottom rows' top: the numbers' line above the bottom margin. */
	[[nodiscard]] float GetBottomRowY() const;
	[[nodiscard]] float GetWorldTime() const;
	/** Line.Update, counted in NumTextFormats. */
	template <typename FormatType>
	void UpdateText(
		FShooterHUDText& Line, const FShooterHUDTextKey& Key, FormatType&& Format, const UFont* InFont = nullptr)
	{
		NumTextFormats += Line.Update(Key, Forward<FormatType>(Format), InFont) ? 1 : 0;
	}

	/** One kill feed line: the names, measured, and the icons. */
	struct FKillFeedLine
	{
		FShooterHUDText Killer;
		FShooterHUDText Victim;
		EShooterHUDIcon Icon = EShooterHUDIcon::World;
		bool bHeadshot = false;
	};

	/** The icons' atlas and the fonts, loaded at BeginPlay. */
	UPROPERTY(Transient)
	UTexture2D* IconsTexture = nullptr;
	UPROPERTY(Transient)
	UFont* NumberFont = nullptr;
	UPROPERTY(Transient)
	UFont* BoldFont = nullptr;
	UPROPERTY(Transient)
	UShooterScoreboardWidget* ScoreboardWidget = nullptr;

	/** The lines of text kept between frames (FShooterHUDText). */
	FShooterHUDText MoneyText;
	FShooterHUDText HealthText;
	FShooterHUDText ArmorText;
	FShooterHUDText WeaponText;
	FShooterHUDText AmmoText;
	FShooterHUDText ClockText;
	FShooterHUDText CTScoreText;
	FShooterHUDText TScoreText;
	FShooterHUDText RoundText;
	FShooterHUDText MessageText;
	FShooterHUDText SpectatorText;
	FShooterHUDText PickupText;
	bool bPickupNoticeShown = false;
	/** The frame readout, and the frames and seconds it is averaging. */
	FShooterHUDText FrameStatsText;
	bool bFrameStatsShown = false;
	int32 FrameStatsFrames = 0;
	double FrameStatsSeconds = 0.0;
	/** The bomb sites' letters on the radar (the game mode's sites, made once). */
	TArray<FString> RadarSiteLabels;
	int32 NumRadarPrimitives = 0;
	bool bRadarOverviewDrawn = false;
	int32 NumDamageIndicatorLines = 0;
	float FlashOverlayAlpha = 0.0f;
	/** The kill feed's lines, formatted when the game state's feed changes (its serial). */
	TArray<FKillFeedLine> KillFeedLines;
	int32 KillFeedSerial = -1;
	int32 NumKillFeedLinesDrawn = 0;

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
	int32 NumTextFormats = 0;
};

/**
 * The buy menu (CS's), a tree of UMG widgets: a bordered vertical box at the top left, under the radar: a heading with
 * the page's name and the money, why buying is refused (outside a buy zone, after the buy time), the page's lines
 * (AShooterPlayerController::GetBuyMenuEntries) in a UTableView (their number key, the category or the item's display
 * name, its price; a category's line in amber with '>', an item grey when it cannot be bought now; the pad's line
 * highlighted) and the last buy's result. NativeTick refreshes it from the game, and collapses it while the owner's
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
	/** The page's lines: a row an entry of GetBuyMenuEntries, in their order (columns Key, Item, Price). */
	[[nodiscard]] UTableView* GetItemTable() const
	{
		return ItemTable;
	}
	/** How many lines of text the menu has set (a line is set only when what it shows changes). */
	[[nodiscard]] int32 GetNumTextFormats() const
	{
		return NumTextFormats;
	}

	/** The colours of an item that can and cannot be bought now, and of a category. */
	static const FLinearColor AffordableColor;
	static const FLinearColor UnaffordableColor;
	static const FLinearColor CategoryColor;

private:
	/** Adds a text line to the box. */
	UTextBlock* AddLine(UVerticalBox& Box, float TopPadding);
	/** Sets Block's text to Text when it differs from Shown (what Block shows), counted in NumTextFormats. */
	void SetLineText(UTextBlock& Block, FString& Shown, const FString& Text);

	/** What the lines show: set again only when it changes (FShooterHUDText; the refusal and the last buy as text). */
	FShooterHUDText TitleLine;
	FShooterHUDText MoneyLine;
	FString ShownRefusal;
	TArray<FShooterHUDText> ItemLines;
	FString ShownLastBuy;
	int32 NumTextFormats = 0;

	UPROPERTY()
	UBorder* Panel = nullptr;
	UPROPERTY()
	UTextBlock* TitleText = nullptr;
	UPROPERTY()
	UTextBlock* MoneyText = nullptr;
	UPROPERTY()
	UTextBlock* RefusalText = nullptr;
	UPROPERTY()
	UTableView* ItemTable = nullptr;
	UPROPERTY()
	UTextBlock* LastBuyText = nullptr;
};
