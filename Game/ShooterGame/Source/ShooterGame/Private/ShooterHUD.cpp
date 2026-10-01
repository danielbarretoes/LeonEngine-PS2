#include "ShooterHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasTypes.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TableView.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/App.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "ShooterScoreboardWidget.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_AWP.h"
#include "Weapons/ShooterWeapon_Instant.h"

namespace
{

	const FColor CTColor(110, 160, 255);
	const FColor TColor(255, 190, 90);
	const FLinearColor MessageColor(1.0f, 1.0f, 1.0f);
	const FLinearColor BombColor(1.0f, 0.25f, 0.2f);
	const FLinearColor RefusalColor(1.0f, 0.3f, 0.2f);
	const FLinearColor LowHealthColor(1.0f, 0.2f, 0.15f);
	const FLinearColor MoneyColor(0.55f, 0.9f, 0.35f);
	const FLinearColor BuyZoneColor(0.35f, 0.9f, 0.3f);
	const FLinearColor BombIconColor(0.3f, 0.9f, 0.3f);
	const FLinearColor KillIconColor(0.95f, 0.9f, 0.8f);
	const FLinearColor HeadshotIconColor(1.0f, 0.35f, 0.25f);
	const FLinearColor KillFeedBandColor(0.0f, 0.0f, 0.0f, 0.4f);
	/** The drop shadow of the text and the icons over the world, a pixel down and right: readable on a bright sky. */
	const FLinearColor TextShadowColor(0.0f, 0.0f, 0.0f, 1.0f);
	const FLinearColor IconShadowColor(0.0f, 0.0f, 0.0f, 0.85f);
	const FLinearColor FrameStatsColor(0.85f, 0.85f, 0.85f);

	/** The HUD's layout on the 640 x 448 canvas, pixels. */
	constexpr float EdgeMargin = 12.0f;
	/** The status icons' side, and the space between an icon and its number. */
	constexpr float StatusIconSize = 24.0f;
	constexpr float IconGap = 4.0f;
	/** The armor's column from the left margin (after the health's three digits). */
	constexpr float ArmorColumn = 96.0f;
	/** The health at or under which it shows red. */
	constexpr int32 LowHealth = 25;
	/** How far from the centre the scores sit, either side of the clock. */
	constexpr float ScoreGap = 64.0f;

	/** The buy menu's panel's left edge on the canvas (the refusal and the radio menu are drawn there too). */
	constexpr float BuyMenuLeft = 24.0f;

	/** The radar's colours, its view cone's arms (pixels) and the bomb sites it shows. */
	const FLinearColor RadarBorderColor(0.45f, 0.45f, 0.45f);
	const FLinearColor RadarBackgroundColor(0.04f, 0.07f, 0.04f);
	const FLinearColor RadarConeColor(0.3f, 0.45f, 0.3f);
	const FLinearColor RadarSiteColor(0.85f, 0.85f, 0.85f);
	constexpr float RadarConeLength = 12.0f;
	constexpr float RadarDotSize = 4.0f;
	constexpr float RadarBombDotSize = 5.0f;
	constexpr int32 MaxRadarSites = 2;

	/** The damage indicator's segments and thickness (pixels). */
	constexpr int32 DamageIndicatorSegments = 4;
	constexpr float DamageIndicatorThickness = 4.0f;

	/** The atlas's rectangles (SourceArt/UI/make_hud_icons.py's ICONS), in EShooterHUDIcon's order, texels. */
	struct FIconRect
	{
		float U;
		float V;
		float UL;
		float VL;
	};
	constexpr FIconRect IconRects[] = {
		{0.0f, 0.0f, 48.0f, 16.0f}, // AK47
		{48.0f, 0.0f, 48.0f, 16.0f}, // M4A1
		{96.0f, 0.0f, 48.0f, 16.0f}, // AWP
		{144.0f, 0.0f, 48.0f, 16.0f}, // MP5
		{192.0f, 0.0f, 32.0f, 16.0f}, // Glock
		{224.0f, 0.0f, 32.0f, 16.0f}, // USP
		{0.0f, 16.0f, 32.0f, 16.0f}, // Deagle
		{32.0f, 16.0f, 40.0f, 16.0f}, // Knife
		{72.0f, 16.0f, 16.0f, 16.0f}, // HEGrenade
		{88.0f, 16.0f, 16.0f, 16.0f}, // Flashbang
		{104.0f, 16.0f, 16.0f, 16.0f}, // SmokeGrenade
		{120.0f, 16.0f, 24.0f, 16.0f}, // C4
		{144.0f, 16.0f, 16.0f, 16.0f}, // World
		{160.0f, 16.0f, 16.0f, 16.0f}, // Headshot
		{0.0f, 32.0f, 24.0f, 24.0f}, // Health
		{24.0f, 32.0f, 24.0f, 24.0f}, // Armor
		{48.0f, 32.0f, 24.0f, 24.0f}, // ArmorHelmet
		{72.0f, 32.0f, 24.0f, 24.0f}, // BuyZone
		{96.0f, 32.0f, 24.0f, 24.0f}, // Bomb
		{120.0f, 32.0f, 24.0f, 24.0f}, // Defuser
		{144.0f, 32.0f, 24.0f, 24.0f}, // Clock
	};
	static_assert(UE_ARRAY_COUNT(IconRects) == static_cast<int32>(EShooterHUDIcon::Num), "An icon a rectangle");

	/** The kill feed's weapons by their buy names. */
	struct FKillIconName
	{
		const TCHAR* WeaponName;
		EShooterHUDIcon Icon;
	};
	constexpr FKillIconName KillIconNames[] = {
		{TEXT("ak47"), EShooterHUDIcon::AK47},
		{TEXT("m4a1"), EShooterHUDIcon::M4A1},
		{TEXT("awp"), EShooterHUDIcon::AWP},
		{TEXT("mp5"), EShooterHUDIcon::MP5},
		{TEXT("glock"), EShooterHUDIcon::Glock},
		{TEXT("usp"), EShooterHUDIcon::USP},
		{TEXT("deagle"), EShooterHUDIcon::Deagle},
		{TEXT("knife"), EShooterHUDIcon::Knife},
		{TEXT("hegrenade"), EShooterHUDIcon::HEGrenade},
		{TEXT("flashbang"), EShooterHUDIcon::Flashbang},
		{TEXT("smokegrenade"), EShooterHUDIcon::SmokeGrenade},
		{TEXT("c4"), EShooterHUDIcon::C4},
	};

	FColor GetTeamColor(EShooterTeam Team)
	{
		return Team == EShooterTeam::T ? TColor : Team == EShooterTeam::CT ? CTColor : FColor::White;
	}

	/** A byte colour as the canvas takes it (FCanvas::DrawText's FColor overload: no sRGB curve). */
	FLinearColor ToLinear(const FColor& Color)
	{
		return FLinearColor(float(Color.R) / 255.0f, float(Color.G) / 255.0f, float(Color.B) / 255.0f, 1.0f);
	}

	/** A font's line, pixels (0 without it). */
	float LineHeightOf(const UFont* Font)
	{
		return Font != nullptr ? Font->GetLineHeight() : 0.0f;
	}

	/** A line of the HUD's text font, and the step between two. */
	float HUDLineHeight()
	{
		return LineHeightOf(AShooterHUD::GetTextFont());
	}
	float LineStep()
	{
		return HUDLineHeight() + 4.0f;
	}

	/** The whole seconds a clock shows (it rounds up). */
	int32 GetClockSeconds(float Seconds)
	{
		return FMath::CeilToInt(FMath::Max(0.0f, Seconds));
	}

	/** m:ss. */
	FString FormatClock(int32 WholeSeconds)
	{
		return FString::Printf(TEXT("%d:%02d"), WholeSeconds / 60, WholeSeconds % 60);
	}

	/** A line's key from the values it shows. */
	FShooterHUDTextKey MakeKey(int64 A, int64 B = 0, int64 C = 0, int64 D = 0)
	{
		FShooterHUDTextKey Key;
		Key.Values[0] = A;
		Key.Values[1] = B;
		Key.Values[2] = C;
		Key.Values[3] = D;
		return Key;
	}

	/** An object's or a name's identity in a key. */
	int64 ToKey(const void* Object)
	{
		return static_cast<int64>(reinterpret_cast<UPTRINT>(Object));
	}
	int64 ToKey(FName Name)
	{
		return (static_cast<int64>(Name.GetComparisonIndex().ToUnstableInt()) << 32) | Name.GetNumber();
	}

	/**
	 * Location on the floor plane seen from Origin turned to Yaw (degrees): how far ahead along the yaw and how far to
	 * the right (90 degrees clockwise from it: +Y at yaw 0, as UE's), cm.
	 */
	void ToViewFrame(const FVector& Origin, float Yaw, const FVector& Location, float& OutAhead, float& OutRight)
	{
		const float YawRadians = FMath::DegreesToRadians(Yaw);
		const float Cos = FMath::Cos(YawRadians);
		const float Sin = FMath::Sin(YawRadians);
		const float DeltaX = Location.X - Origin.X;
		const float DeltaY = Location.Y - Origin.Y;
		OutAhead = (DeltaX * Cos) + (DeltaY * Sin);
		OutRight = (DeltaY * Cos) - (DeltaX * Sin);
	}

	/** A corner of the radar's overview polygon: where it is on the screen and on the overview. */
	struct FRadarCorner
	{
		FVector2D Screen;
		FVector2D UV;
	};
	using FRadarPolygon = TArray<FRadarCorner, TInlineAllocator<12>>;

	/**
	 * Clips Polygon to one side of the overview's UV square (Sutherland-Hodgman): Axis 0 is U, 1 is V; the kept side
	 * is >= 0 when bKeepAbove, else <= 1. A crossing's screen position and UV move together, both affine in the other.
	 */
	void ClipRadarPolygon(FRadarPolygon& Polygon, int32 Axis, bool bKeepAbove, FRadarPolygon& Scratch)
	{
		Scratch.Reset();
		const float Edge = bKeepAbove ? 0.0f : 1.0f;
		auto Keeps = [Axis, bKeepAbove, Edge](const FRadarCorner& Corner)
		{
			const float Value = Axis == 0 ? Corner.UV.X : Corner.UV.Y;
			return bKeepAbove ? Value >= Edge : Value <= Edge;
		};
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FRadarCorner& A = Polygon[Index];
			const FRadarCorner& B = Polygon[(Index + 1) % Polygon.Num()];
			const bool bKeepA = Keeps(A);
			if (bKeepA)
			{
				Scratch.Add(A);
			}
			if (bKeepA != Keeps(B))
			{
				const float ValueA = Axis == 0 ? A.UV.X : A.UV.Y;
				const float ValueB = Axis == 0 ? B.UV.X : B.UV.Y;
				const float Alpha = (Edge - ValueA) / (ValueB - ValueA);
				FRadarCorner Crossing{A.Screen + ((B.Screen - A.Screen) * Alpha), A.UV + ((B.UV - A.UV) * Alpha)};
				// Exactly on the edge (no sampling past the texture by a rounding).
				(Axis == 0 ? Crossing.UV.X : Crossing.UV.Y) = Edge;
				Scratch.Add(Crossing);
			}
		}
		Polygon = Scratch;
	}

	/** The labels drawn every frame, made once (a frame's HUD allocates nothing). */
	const FString& CTLabel()
	{
		static const FString Label(TEXT("CT"));
		return Label;
	}
	const FString& TLabel()
	{
		static const FString Label(TEXT("T"));
		return Label;
	}

} // namespace

void FShooterHUDText::Set(const FString& InText, const UFont* InFont)
{
	Text = InText;
	Font = InFont;
	FCanvas::MeasureText(InFont != nullptr ? InFont : AShooterHUD::GetTextFont(), Text, Width, Height);
	bFormatted = true;
}

AShooterHUD::AShooterHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AShooterHUD::BeginPlay()
{
	Super::BeginPlay();
	IconsTexture = LoadShooterAsset<UTexture2D>(IconsTextureName);
	NumberFont = LoadShooterAsset<UFont>(NumberFontName);
	BoldFont = LoadShooterAsset<UFont>(BoldFontName);
	(void)AddWidget<UShooterBuyMenuWidget>();
	ScoreboardWidget = AddWidget<UShooterScoreboardWidget>();
}

float AShooterHUD::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0f;
}

AShooterPlayerController* AShooterHUD::GetShooterPlayerController() const
{
	return Cast<AShooterPlayerController>(PlayerOwner);
}

AShooterCharacter* AShooterHUD::GetViewedPawn() const
{
	return PlayerOwner != nullptr ? Cast<AShooterCharacter>(PlayerOwner->GetPawn()) : nullptr;
}

AShooterGameState* AShooterHUD::GetShooterGameState() const
{
	const UWorld* World = GetWorld();
	const AGameModeBase* GameMode = World != nullptr ? World->GetAuthGameMode() : nullptr;
	return GameMode != nullptr ? GameMode->GetGameState<AShooterGameState>() : nullptr;
}

const UFont* AShooterHUD::GetTextFont()
{
	return UEngine::GetSmallFont();
}

const UFont* AShooterHUD::GetNumberFont() const
{
	return NumberFont != nullptr ? NumberFont : UEngine::GetMediumFont();
}

const UFont* AShooterHUD::GetBoldFont() const
{
	return BoldFont != nullptr ? BoldFont : UEngine::GetSmallFont();
}

void AShooterHUD::GetIconRect(EShooterHUDIcon Icon, float& OutU, float& OutV, float& OutUL, float& OutVL)
{
	const int32 Index = FMath::Clamp(static_cast<int32>(Icon), 0, static_cast<int32>(EShooterHUDIcon::Num) - 1);
	const FIconRect& Rect = IconRects[Index];
	OutU = Rect.U;
	OutV = Rect.V;
	OutUL = Rect.UL;
	OutVL = Rect.VL;
}

EShooterHUDIcon AShooterHUD::GetKillFeedIcon(const FString& WeaponName)
{
	for (const FKillIconName& Entry : KillIconNames)
	{
		if (WeaponName.Equals(Entry.WeaponName, ESearchCase::IgnoreCase))
		{
			return Entry.Icon;
		}
	}
	return EShooterHUDIcon::World;
}

FCanvasIcon AShooterHUD::MakeHUDIcon(EShooterHUDIcon Icon) const
{
	float U = 0.0f;
	float V = 0.0f;
	float UL = 0.0f;
	float VL = 0.0f;
	GetIconRect(Icon, U, V, UL, VL);
	return FCanvas::MakeIcon(IconsTexture, U, V, UL, VL);
}

void AShooterHUD::DrawHUDIcon(EShooterHUDIcon Icon, float X, float Y, const FLinearColor& Color)
{
	if (IconsTexture != nullptr)
	{
		// Its shadow first, as the text's: the icon stays readable on a bright sky.
		const FCanvasIcon CanvasIcon = MakeHUDIcon(Icon);
		const float Left = FMath::RoundToFloat(X);
		const float Top = FMath::RoundToFloat(Y);
		Canvas->DrawIcon(CanvasIcon, Left + 1.0f, Top + 1.0f, 1.0f, IconShadowColor);
		Canvas->DrawIcon(CanvasIcon, Left, Top, 1.0f, Color);
	}
}

float AShooterHUD::GetMenuTop() const
{
	// Under the radar and the frame readout's line.
	return EdgeMargin + FMath::Max(0.0f, RadarSize) + 8.0f + HUDLineHeight() + 8.0f;
}

void AShooterHUD::DrawFrameStats()
{
	// The frames' real time (FApp's), averaged over FrameStatsRefreshSeconds and formatted then.
	FrameStatsSeconds += FApp::GetDeltaTime();
	++FrameStatsFrames;
	if (FrameStatsSeconds >= double(FrameStatsRefreshSeconds) && FrameStatsFrames > 0)
	{
		const double Milliseconds = (FrameStatsSeconds * 1000.0) / double(FrameStatsFrames);
		const double Fps = double(FrameStatsFrames) / FrameStatsSeconds;
		FrameStatsText.Set(FString::Printf(TEXT("%.0f fps  %.1f ms"), Fps, Milliseconds));
		++NumTextFormats;
		FrameStatsFrames = 0;
		FrameStatsSeconds = 0.0;
	}
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	bFrameStatsShown = Controller != nullptr && Controller->IsFrameStatsShown() && !FrameStatsText.Text.IsEmpty();
	if (bFrameStatsShown)
	{
		DrawHUDText(FrameStatsText, EdgeMargin, EdgeMargin + FMath::Max(0.0f, RadarSize) + 6.0f, FrameStatsColor);
	}
}

void AShooterHUD::DrawHUDText(
	const UFont* Font, const FString& Text, float X, float Y, const FLinearColor& Color, ETextJustify Justify)
{
	Canvas->DrawText(Font, Text, X, Y, Color, Justify, FVector2D(1.0f, 1.0f), TextShadowColor);
}

void AShooterHUD::DrawHUDText(
	const FShooterHUDText& Line, float X, float Y, const FLinearColor& Color, ETextJustify Justify)
{
	if (!Line.Text.IsEmpty())
	{
		DrawHUDText(Line.Font != nullptr ? Line.Font : GetTextFont(), Line.Text, X, Y, Color, Justify);
	}
}

float AShooterHUD::GetBottomRowY() const
{
	return static_cast<float>(Canvas->GetSizeY()) - EdgeMargin - LineHeightOf(GetNumberFont());
}

void AShooterHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}
	// A flashbang's white under the rest (the HUD stays readable, as CS's).
	DrawFlash();
	// A spectator (dead until the next round) has no weapon to aim: no crosshair (CS).
	const bool bSpectating = PlayerOwner != nullptr && PlayerOwner->IsInState(NAME_Spectating);
	if (!DrawScope() && !bSpectating)
	{
		DrawCrosshair();
	}
	DrawHitMarker();
	DrawDamageIndicator();
	DrawStatus();
	DrawWeaponAndMoney();
	DrawStatusIcons();
	DrawRoundInfo();
	DrawKillFeed();
	DrawRadio();
	DrawMessages();
	DrawBuyRefusal();
	DrawPickupNotice();
	DrawRadioMenu();
	DrawProgress();
	DrawSpectatorInfo();
	DrawRadar();
	DrawFrameStats();
}

FVector2D AShooterHUD::ProjectToRadar(
	const FVector& Origin, float Yaw, const FVector& Location, float Range, float HalfSize)
{
	float Ahead = 0.0f;
	float Right = 0.0f;
	ToViewFrame(Origin, Yaw, Location, Ahead, Right);
	const float Scale = Range > 0.0f ? HalfSize / Range : 0.0f;
	FVector2D Offset(Right * Scale, -Ahead * Scale);
	// Beyond the square: on its edge, in the same direction.
	const float Largest = FMath::Max(FMath::Abs(Offset.X), FMath::Abs(Offset.Y));
	if (Largest > HalfSize && Largest > 0.0f)
	{
		Offset *= HalfSize / Largest;
	}
	return Offset;
}

float AShooterHUD::GetDamageIndicatorAngle(const FVector& ViewLocation, float ViewYaw, const FVector& Source)
{
	float Ahead = 0.0f;
	float Right = 0.0f;
	ToViewFrame(ViewLocation, ViewYaw, Source, Ahead, Right);
	if (FMath::Abs(Ahead) < KINDA_SMALL_NUMBER && FMath::Abs(Right) < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	// Clockwise from ahead.
	return FMath::UnwindDegrees(FMath::RadiansToDegrees(FMath::Atan2(Right, Ahead)));
}

int32 AShooterHUD::MakeRadarOverviewTriangles(const FWorldOverviewSettings& Overview, const FVector& Origin, float Yaw,
	float Range, float Left, float Top, float Size, TArray<FCanvasUVTri, TInlineAllocator<8>>& OutTriangles)
{
	OutTriangles.Reset();
	if (!Overview.IsValid() || Size <= 0.0f || Range <= 0.0f)
	{
		return 0;
	}
	// The square's corners clockwise from the top left, each at the world point it shows: ahead is up, the right right.
	const float Half = Size * 0.5f;
	const float CmPerPixel = Range / Half;
	const float YawRadians = FMath::DegreesToRadians(Yaw);
	const float Cos = FMath::Cos(YawRadians);
	const float Sin = FMath::Sin(YawRadians);
	FRadarPolygon Polygon;
	for (const FVector2D& Offset :
		{FVector2D(-Half, -Half), FVector2D(Half, -Half), FVector2D(Half, Half), FVector2D(-Half, Half)})
	{
		const float Ahead = -Offset.Y * CmPerPixel;
		const float Right = Offset.X * CmPerPixel;
		const FVector World(Origin.X + (Ahead * Cos) - (Right * Sin), Origin.Y + (Ahead * Sin) + (Right * Cos), 0.0f);
		Polygon.Add({FVector2D(Left + Half + Offset.X, Top + Half + Offset.Y), Overview.GetUV(World)});
	}
	FRadarPolygon Scratch;
	for (int32 Axis = 0; Axis < 2 && Polygon.Num() >= 3; ++Axis)
	{
		ClipRadarPolygon(Polygon, Axis, true, Scratch);
		if (Polygon.Num() >= 3)
		{
			ClipRadarPolygon(Polygon, Axis, false, Scratch);
		}
	}
	for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
	{
		FCanvasUVTri& Triangle = OutTriangles.AddDefaulted_GetRef();
		Triangle.V0_Pos = Polygon[0].Screen;
		Triangle.V0_UV = Polygon[0].UV;
		Triangle.V1_Pos = Polygon[Index].Screen;
		Triangle.V1_UV = Polygon[Index].UV;
		Triangle.V2_Pos = Polygon[Index + 1].Screen;
		Triangle.V2_UV = Polygon[Index + 1].UV;
	}
	return OutTriangles.Num();
}

void AShooterHUD::DrawRadar()
{
	NumRadarPrimitives = 0;
	bRadarOverviewDrawn = false;
	const UWorld* World = GetWorld();
	const AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	const AShooterGameState* State = GetShooterGameState();
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	const APlayerCameraManager* Camera = Controller != nullptr ? Controller->PlayerCameraManager : nullptr;
	if (GameMode == nullptr || State == nullptr || Camera == nullptr || !Camera->HasCameraCache() || RadarSize <= 0.0f)
	{
		return;
	}
	// Whose radar: the player's team, else (a spectator without one) the watched player's; the one at the centre.
	const AShooterCharacter* Watched = Controller->GetViewedPlayer();
	const AShooterCharacter* Centre = GetViewedPawn() != nullptr ? GetViewedPawn() : Watched;
	const AShooterPlayerState* OwnState = Controller->GetPlayerState<AShooterPlayerState>();
	EShooterTeam Team = OwnState != nullptr ? OwnState->GetTeam() : EShooterTeam::None;
	if (Team == EShooterTeam::None && Watched != nullptr)
	{
		Team = Watched->GetTeam();
	}
	const FVector Origin = Camera->GetCameraLocation();
	const float Yaw = Camera->GetCameraRotation().Yaw;
	const float HalfSize = RadarSize * 0.5f;
	const float CenterX = EdgeMargin + HalfSize;
	const float CenterY = EdgeMargin + HalfSize;
	auto DrawDot = [this, &Origin, Yaw, HalfSize, CenterX, CenterY](
					   const FVector& Location, float Size, const FLinearColor& Color)
	{
		const FVector2D Offset = ProjectToRadar(Origin, Yaw, Location, RadarRange, HalfSize - (Size * 0.5f));
		Canvas->DrawTile(CenterX + Offset.X - (Size * 0.5f), CenterY + Offset.Y - (Size * 0.5f), Size, Size, Color);
		++NumRadarPrimitives;
	};

	// The frame, then the view's cone (90 degrees, ahead is up).
	Canvas->DrawTile(EdgeMargin - 1.0f, EdgeMargin - 1.0f, RadarSize + 2.0f, RadarSize + 2.0f, RadarBorderColor);
	Canvas->DrawTile(EdgeMargin, EdgeMargin, RadarSize, RadarSize, RadarBackgroundColor);
	// The map's overview under everything else (ps2-polish P7), turned with the view.
	if (const AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		TArray<FCanvasUVTri, TInlineAllocator<8>> Triangles;
		if (MakeRadarOverviewTriangles(WorldSettings->OverviewSettings, Origin, Yaw, RadarRange, EdgeMargin, EdgeMargin,
				RadarSize, Triangles) > 0)
		{
			FCanvasTriangleItem Overview(TArrayView<const FCanvasUVTri>(Triangles.GetData(), Triangles.Num()),
				WorldSettings->OverviewSettings.Texture);
			Canvas->DrawItem(Overview);
			bRadarOverviewDrawn = true;
		}
	}
	const float ConeArm = RadarConeLength * UE_INV_SQRT_2;
	Canvas->DrawLine(CenterX, CenterY, CenterX - ConeArm, CenterY - ConeArm, RadarConeColor, 1.0f);
	Canvas->DrawLine(CenterX, CenterY, CenterX + ConeArm, CenterY - ConeArm, RadarConeColor, 1.0f);
	NumRadarPrimitives += 4;

	// The bomb sites' letters (made once: the sites do not change during a match).
	const TArray<FName>& Sites = GameMode->GetBombSiteNames();
	if (RadarSiteLabels.Num() != Sites.Num())
	{
		RadarSiteLabels.Reset();
		for (const FName& Site : Sites)
		{
			RadarSiteLabels.Add(Site.ToString());
		}
	}
	for (int32 Index = 0; Index < FMath::Min(Sites.Num(), MaxRadarSites); ++Index)
	{
		FVector SiteLocation;
		if (!GameMode->GetBombSiteLocation(Sites[Index], SiteLocation))
		{
			continue;
		}
		constexpr float LetterHalfWidth = 3.0f;
		const FVector2D Offset =
			ProjectToRadar(Origin, Yaw, SiteLocation, RadarRange, HalfSize - HUDLineHeight() * 0.5f);
		Canvas->DrawText(GetTextFont(), RadarSiteLabels[Index], CenterX + Offset.X - LetterHalfWidth,
			CenterY + Offset.Y - (HUDLineHeight() * 0.5f), RadarSiteColor);
		++NumRadarPrimitives;
	}

	// The living teammates (the terrorists see the bomb's carrier in the bomb's colour), then the bomb for them.
	if (Team != EShooterTeam::None)
	{
		const FLinearColor TeamColor(GetTeamColor(Team));
		for (const AShooterCharacter* Pawn : GameMode->GetPawns())
		{
			if (Pawn == Centre || Pawn->IsPendingKillPending() || !Pawn->IsAlive() || Pawn->GetTeam() != Team)
			{
				continue;
			}
			const bool bCarrier = Team == EShooterTeam::T && Pawn->GetCarriedBomb() != nullptr;
			DrawDot(
				Pawn->GetActorLocation(), bCarrier ? RadarBombDotSize : RadarDotSize, bCarrier ? BombColor : TeamColor);
		}
	}
	const AShooterBomb* Bomb = GameMode->GetBomb();
	const EShooterBombState BombState = State->GetBombState();
	if (Team == EShooterTeam::T && Bomb != nullptr &&
		(BombState == EShooterBombState::Dropped || BombState == EShooterBombState::Planted))
	{
		DrawDot(Bomb->GetActorLocation(), RadarBombDotSize, BombColor);
	}
	// The view itself, at the centre.
	Canvas->DrawTile(CenterX - 1.5f, CenterY - 1.5f, 3.0f, 3.0f, FLinearColor::White);
	++NumRadarPrimitives;
}

void AShooterHUD::DrawFlash()
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	FlashOverlayAlpha = Pawn != nullptr ? Pawn->GetFlashAlpha() : 0.0f;
	if (FlashOverlayAlpha <= 0.0f)
	{
		return;
	}
	// One full-screen tile: a sprite for the GS (N15), alpha blended.
	Canvas->DrawTile(0.0f, 0.0f, static_cast<float>(Canvas->GetSizeX()), static_cast<float>(Canvas->GetSizeY()),
		FLinearColor(1.0f, 1.0f, 1.0f, FlashOverlayAlpha));
}

void AShooterHUD::DrawDamageIndicator()
{
	NumDamageIndicatorLines = 0;
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	const APlayerCameraManager* Camera = Controller != nullptr ? Controller->PlayerCameraManager : nullptr;
	if (Camera == nullptr || !Camera->HasCameraCache() || Controller->GetLastDamageTime() < 0.0f ||
		DamageIndicatorDuration <= 0.0f)
	{
		return;
	}
	const float Elapsed = GetWorldTime() - Controller->GetLastDamageTime();
	if (Elapsed < 0.0f || Elapsed >= DamageIndicatorDuration)
	{
		return;
	}
	// Fresh: the whole arc, bright red; it narrows and darkens to nothing within the duration.
	const float Fade = 1.0f - (Elapsed / DamageIndicatorDuration);
	const float Angle = GetDamageIndicatorAngle(
		Camera->GetCameraLocation(), Camera->GetCameraRotation().Yaw, Controller->GetLastDamageSourceLocation());
	const float HalfArc = DamageIndicatorArc * 0.5f * (0.4f + (0.6f * Fade));
	const FLinearColor Color(0.3f + (0.7f * Fade), 0.05f * Fade, 0.05f * Fade);
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float CenterY = static_cast<float>(Canvas->GetSizeY()) * 0.5f;
	auto PointAt = [this, CenterX, CenterY](float Degrees)
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		return FVector2D(CenterX + (DamageIndicatorRadius * FMath::Sin(Radians)),
			CenterY - (DamageIndicatorRadius * FMath::Cos(Radians)));
	};
	FVector2D Previous = PointAt(Angle - HalfArc);
	for (int32 Segment = 1; Segment <= DamageIndicatorSegments; ++Segment)
	{
		const FVector2D Next =
			PointAt(Angle - HalfArc + (2.0f * HalfArc * static_cast<float>(Segment) / DamageIndicatorSegments));
		Canvas->DrawLine(Previous.X, Previous.Y, Next.X, Next.Y, Color, DamageIndicatorThickness);
		Previous = Next;
		++NumDamageIndicatorLines;
	}
}

void AShooterHUD::DrawSpectatorInfo()
{
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	const EShooterSpectatorMode Mode =
		Controller != nullptr ? Controller->GetSpectatorMode() : EShooterSpectatorMode::None;
	const AShooterCharacter* Watched = Mode == EShooterSpectatorMode::Player ? Controller->GetViewedPlayer() : nullptr;
	const int32 Health = Watched != nullptr ? FMath::CeilToInt(Watched->GetHealth()) : 0;
	const FString* KillerName =
		Mode == EShooterSpectatorMode::DeathCam ? &Controller->GetDeathCamKillerName() : nullptr;
	UpdateText(SpectatorText,
		MakeKey(static_cast<int64>(Mode), ToKey(Watched), Health, KillerName != nullptr ? KillerName->Len() : 0),
		[Mode, Watched, Health, KillerName]()
		{
			switch (Mode)
			{
				case EShooterSpectatorMode::DeathCam:
					return KillerName != nullptr && !KillerName->IsEmpty()
						? FString::Printf(TEXT("Killed by %s"), **KillerName)
						: FString(TEXT("You died"));
				case EShooterSpectatorMode::Player:
				{
					const AController* WatchedController = Watched != nullptr ? Watched->GetController() : nullptr;
					const APlayerState* WatchedState =
						WatchedController != nullptr ? WatchedController->GetPlayerState<APlayerState>() : nullptr;
					return FString::Printf(TEXT("Spectating %s  + %d"),
						WatchedState != nullptr ? *WatchedState->GetPlayerName() : TEXT("?"), Health);
				}
				case EShooterSpectatorMode::FreeLook:
					return FString(TEXT("Free look"));
				case EShooterSpectatorMode::None:
					break;
			}
			return FString();
		});
	if (SpectatorText.Text.IsEmpty())
	{
		return;
	}
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float Y = static_cast<float>(Canvas->GetSizeY()) - EdgeMargin - HUDLineHeight();
	DrawHUDText(SpectatorText, CenterX, Y, MessageColor, ETextJustify::Center);
}

void AShooterHUD::DrawStatus()
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	if (Pawn == nullptr)
	{
		return;
	}
	const UFont* Numbers = GetNumberFont();
	const float Row = GetBottomRowY();
	const float IconY = Row + FMath::RoundToFloat((LineHeightOf(Numbers) - StatusIconSize) * 0.5f);
	// The health: a cross and the number, red when low (CS).
	const int32 Health = FMath::CeilToInt(Pawn->GetHealth());
	UpdateText(HealthText, MakeKey(Health), [Health]() { return FString::FromInt(Health); }, Numbers);
	const FLinearColor& HealthColor = Health <= LowHealth ? LowHealthColor : StatusColor;
	DrawHUDIcon(EShooterHUDIcon::Health, EdgeMargin, IconY, HealthColor);
	DrawHUDText(HealthText, EdgeMargin + StatusIconSize + IconGap, Row, HealthColor);
	// The armor: a vest, with a helmet when it has one, and its points (0 without any, as CS shows it).
	const int32 Armor = FMath::CeilToInt(FMath::Max(0.0f, Pawn->GetArmor()));
	const bool bHelmet = Armor > 0 && Pawn->HasHelmet();
	UpdateText(ArmorText, MakeKey(Armor), [Armor]() { return FString::FromInt(Armor); }, Numbers);
	const float ArmorX = EdgeMargin + ArmorColumn;
	DrawHUDIcon(bHelmet ? EShooterHUDIcon::ArmorHelmet : EShooterHUDIcon::Armor, ArmorX, IconY, StatusColor);
	DrawHUDText(ArmorText, ArmorX + StatusIconSize + IconGap, Row, StatusColor);
}

void AShooterHUD::DrawWeaponAndMoney()
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	const AShooterPlayerState* State =
		PlayerOwner != nullptr ? PlayerOwner->GetPlayerState<AShooterPlayerState>() : nullptr;
	const UFont* Numbers = GetNumberFont();
	const float NumberLine = LineHeightOf(Numbers);
	const float Right = static_cast<float>(Canvas->GetSizeX()) - EdgeMargin;
	// From the bottom up: the ammunition, the weapon's name, the money.
	float Top = GetBottomRowY() + NumberLine;
	if (Pawn != nullptr)
	{
		const AShooterWeapon* Weapon = Pawn->GetWeapon();
		const int32 Clip = Weapon != nullptr ? Weapon->GetCurrentAmmoInClip() : 0;
		const int32 Reserve = Weapon != nullptr ? Weapon->GetCurrentAmmo() : 0;
		const AShooterWeapon_Instant* InstantWeapon = Cast<AShooterWeapon_Instant>(Weapon);
		const int32 WeaponMode = Pawn->IsBombDrawn() ? 3
			: InstantWeapon == nullptr               ? 0
			: InstantWeapon->IsSilenced()            ? 1
			: InstantWeapon->IsBurstMode()           ? 2
													 : 0;
		const bool bCounted = WeaponMode != 3 && Weapon != nullptr && (Weapon->AmmoPerClip > 1 || Weapon->MaxAmmo > 0);
		UpdateText(
			AmmoText, MakeKey(ToKey(Weapon), Clip, Reserve, bCounted ? 1 : 0), [bCounted, Clip, Reserve]()
			{ return bCounted ? FString::Printf(TEXT("%d | %d"), Clip, Reserve) : FString(); }, Numbers);
		UpdateText(WeaponText, MakeKey(ToKey(Weapon), WeaponMode),
			[Weapon, WeaponMode]()
			{
				// The C4 drawn (CS's slot 5): no weapon in hand.
				if (WeaponMode == 3)
				{
					return FString(TEXT("C4"));
				}
				if (Weapon == nullptr)
				{
					return FString();
				}
				// The silencer and the burst mode show after the name (CS: the USP-S, the Glock's burst).
				const TCHAR* Mode = WeaponMode == 1 ? TEXT(" (silenced)")
					: WeaponMode == 2               ? TEXT(" (burst)")
													: TEXT("");
				const FString& Name = Weapon->DisplayName.IsEmpty() ? Weapon->WeaponName : Weapon->DisplayName;
				return Name + Mode;
			});
		if (!AmmoText.Text.IsEmpty())
		{
			Top -= NumberLine;
			DrawHUDText(AmmoText, Right, Top, StatusColor, ETextJustify::Right);
		}
		if (!WeaponText.Text.IsEmpty())
		{
			Top -= AmmoText.Text.IsEmpty() ? NumberLine : WeaponText.Height;
			DrawHUDText(WeaponText, Right, AmmoText.Text.IsEmpty() ? Top + NumberLine - WeaponText.Height : Top,
				StatusColor, ETextJustify::Right);
		}
	}
	if (State == nullptr || GetShooterGameState() == nullptr)
	{
		return;
	}
	// The money, and the buy zone's cart while the player may buy (CS's buy icon).
	const int32 Money = State->GetMoney();
	UpdateText(MoneyText, MakeKey(Money), [Money]() { return FString::Printf(TEXT("$ %d"), Money); }, Numbers);
	Top -= NumberLine + 2.0f;
	DrawHUDText(MoneyText, Right, Top, MoneyColor, ETextJustify::Right);
	const UWorld* World = GetWorld();
	const AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	if (Pawn != nullptr && GameMode != nullptr && Pawn->IsAlive() && GameMode->CanBuy(*Pawn))
	{
		DrawHUDIcon(EShooterHUDIcon::BuyZone, Right - MoneyText.Width - IconGap - StatusIconSize,
			Top + FMath::RoundToFloat((NumberLine - StatusIconSize) * 0.5f), BuyZoneColor);
	}
}

void AShooterHUD::DrawStatusIcons()
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	if (Pawn == nullptr || !Pawn->IsAlive())
	{
		return;
	}
	float Y = FMath::RoundToFloat(static_cast<float>(Canvas->GetSizeY()) * 0.45f);
	if (Pawn->GetCarriedBomb() != nullptr)
	{
		// CS: the C4's icon blinks red while its carrier stands in a bomb site.
		const bool bInSite = Pawn->GetBombSiteHere() != NAME_None;
		const bool bBlink = bInSite && FMath::FloorToInt(GetWorldTime() * 2.0f) % 2 == 0;
		DrawHUDIcon(EShooterHUDIcon::Bomb, EdgeMargin, Y, bBlink ? BombColor : BombIconColor);
		Y += StatusIconSize + IconGap;
	}
	if (Pawn->HasDefuseKit())
	{
		DrawHUDIcon(EShooterHUDIcon::Defuser, EdgeMargin, Y, BombIconColor);
	}
}

void AShooterHUD::DrawRoundInfo()
{
	const AShooterGameState* State = GetShooterGameState();
	if (State == nullptr || State->GetRoundState() == EShooterRoundState::Warmup)
	{
		return;
	}
	const UFont* Numbers = GetNumberFont();
	const float NumberLine = LineHeightOf(Numbers);
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float Now = GetWorldTime();
	// The clock: C4 once planted, else the phase's seconds (formatted once a second).
	const bool bPlanted = State->GetBombState() == EShooterBombState::Planted;
	const bool bTimed =
		State->GetRoundState() == EShooterRoundState::Freeze || State->GetRoundState() == EShooterRoundState::Live;
	const int32 Seconds = bTimed ? GetClockSeconds(State->GetPhaseTimeRemaining(Now)) : 0;
	UpdateText(
		ClockText,
		MakeKey(bPlanted ? 1
				: bTimed ? 2
						 : 0,
			bPlanted ? 0 : Seconds),
		[bPlanted, bTimed, Seconds]()
		{ return bPlanted ? FString(TEXT("C4"))
			  : bTimed    ? FormatClock(Seconds)
						  : FString(); }, Numbers);
	if (!ClockText.Text.IsEmpty())
	{
		const FLinearColor& ClockColor = bPlanted ? BombColor : StatusColor;
		DrawHUDText(ClockText, CenterX, EdgeMargin, ClockColor, ETextJustify::Center);
		DrawHUDIcon(bPlanted ? EShooterHUDIcon::Bomb : EShooterHUDIcon::Clock,
			CenterX - (ClockText.Width * 0.5f) - IconGap - StatusIconSize,
			EdgeMargin + FMath::RoundToFloat((NumberLine - StatusIconSize) * 0.5f), ClockColor);
	}
	// The scores either side of the clock, each team's name beside its own (CT's ends, T's starts, ScoreGap away).
	const int32 ScoreCT = State->GetTeamScore(EShooterTeam::CT);
	const int32 ScoreT = State->GetTeamScore(EShooterTeam::T);
	const int32 RoundNumber = State->GetRoundNumber();
	UpdateText(CTScoreText, MakeKey(ScoreCT), [ScoreCT]() { return FString::FromInt(ScoreCT); }, Numbers);
	UpdateText(TScoreText, MakeKey(ScoreT), [ScoreT]() { return FString::FromInt(ScoreT); }, Numbers);
	UpdateText(
		RoundText, MakeKey(RoundNumber), [RoundNumber]() { return FString::Printf(TEXT("Round %d"), RoundNumber); });
	const FLinearColor CTLinear = ToLinear(CTColor);
	const FLinearColor TLinear = ToLinear(TColor);
	const UFont* Bold = GetBoldFont();
	// A dark band under the block, so the teams' colours read on a bright sky too.
	constexpr float LabelRoom = 24.0f;
	const float HalfBand = ScoreGap + FMath::Max(CTScoreText.Width, TScoreText.Width) + IconGap + LabelRoom;
	Canvas->DrawTile(
		CenterX - HalfBand, EdgeMargin - 3.0f, 2.0f * HalfBand, NumberLine + HUDLineHeight() + 4.0f, KillFeedBandColor);
	// The names sit on the numbers' baseline: their lines' bottoms level.
	const float LabelY = EdgeMargin + NumberLine - LineHeightOf(Bold) - 2.0f;
	DrawHUDText(CTScoreText, CenterX - ScoreGap, EdgeMargin, CTLinear, ETextJustify::Right);
	DrawHUDText(
		Bold, CTLabel(), CenterX - ScoreGap - CTScoreText.Width - IconGap, LabelY, CTLinear, ETextJustify::Right);
	DrawHUDText(TScoreText, CenterX + ScoreGap, EdgeMargin, TLinear);
	DrawHUDText(Bold, TLabel(), CenterX + ScoreGap + TScoreText.Width + IconGap, LabelY, TLinear);
	DrawHUDText(RoundText, CenterX, EdgeMargin + NumberLine, FLinearColor(0.85f, 0.85f, 0.85f), ETextJustify::Center);
}

void AShooterHUD::DrawKillFeed()
{
	NumKillFeedLinesDrawn = 0;
	const AShooterGameState* State = GetShooterGameState();
	if (State == nullptr)
	{
		return;
	}
	const float Right = static_cast<float>(Canvas->GetSizeX()) - EdgeMargin;
	const float Now = GetWorldTime();
	// Below the engine's stats (top right) when they show.
	float Y = EdgeMargin;
	if (GEngine != nullptr && GEngine->IsHudStatsVisible())
	{
		Y = FMath::Max(Y, GEngine->GetDebugOverlay().GetRightTextBottom() + 4.0f);
	}
	// The names are formatted and measured when the feed changes (a kill), not every frame.
	const TArray<FShooterKillFeedEntry>& Feed = State->GetKillFeed();
	if (State->GetKillFeedSerial() != KillFeedSerial || KillFeedLines.Num() != Feed.Num())
	{
		KillFeedSerial = State->GetKillFeedSerial();
		KillFeedLines.SetNum(Feed.Num());
		for (int32 Index = 0; Index < Feed.Num(); ++Index)
		{
			const FShooterKillFeedEntry& Entry = Feed[Index];
			FKillFeedLine& Line = KillFeedLines[Index];
			Line.Killer.Set(Entry.KillerName);
			Line.Victim.Set(Entry.VictimName);
			Line.Icon = GetKillFeedIcon(Entry.WeaponName);
			Line.bHeadshot = Entry.bHeadshot;
			NumTextFormats += 2;
		}
	}
	// A line: the killer, the weapon's icon (and the headshot's), the victim, on a dark band (CS's d_ sprites).
	constexpr float Space = 5.0f;
	constexpr float BandPadding = 2.0f;
	const float TextLine = HUDLineHeight();
	for (int32 Index = 0; Index < Feed.Num(); ++Index)
	{
		const FShooterKillFeedEntry& Entry = Feed[Index];
		if (Now - Entry.Time > KillFeedDuration)
		{
			continue;
		}
		const FKillFeedLine& Line = KillFeedLines[Index];
		float IconU = 0.0f;
		float IconV = 0.0f;
		float IconWidth = 0.0f;
		float IconHeight = 0.0f;
		GetIconRect(Line.Icon, IconU, IconV, IconWidth, IconHeight);
		float HeadshotWidth = 0.0f;
		float HeadshotHeight = 0.0f;
		if (Line.bHeadshot)
		{
			GetIconRect(EShooterHUDIcon::Headshot, IconU, IconV, HeadshotWidth, HeadshotHeight);
			HeadshotWidth += 2.0f;
		}
		const float KillerPart = Line.Killer.Text.IsEmpty() ? 0.0f : Line.Killer.Width + Space;
		const float Width = KillerPart + IconWidth + HeadshotWidth + Space + Line.Victim.Width;
		const float Height = FMath::Max(TextLine, IconHeight);
		float X = Right - Width;
		Canvas->DrawTile(X - BandPadding - 2.0f, Y - BandPadding, Width + (2.0f * BandPadding) + 4.0f,
			Height + (2.0f * BandPadding), KillFeedBandColor);
		const float TextY = Y + FMath::RoundToFloat((Height - TextLine) * 0.5f);
		const float IconY = Y + FMath::RoundToFloat((Height - IconHeight) * 0.5f);
		if (!Line.Killer.Text.IsEmpty())
		{
			DrawHUDText(Line.Killer, X, TextY, ToLinear(GetTeamColor(Entry.KillerTeam)));
		}
		X += KillerPart;
		DrawHUDIcon(Line.Icon, X, IconY, KillIconColor);
		X += IconWidth;
		if (Line.bHeadshot)
		{
			DrawHUDIcon(EShooterHUDIcon::Headshot, X + 2.0f, IconY, HeadshotIconColor);
			X += HeadshotWidth;
		}
		X += Space;
		DrawHUDText(Line.Victim, X, TextY, ToLinear(GetTeamColor(Entry.VictimTeam)));
		Y += Height + (2.0f * BandPadding) + 2.0f;
		++NumKillFeedLinesDrawn;
	}
}

void AShooterHUD::DrawRadio()
{
	DrawnRadioLines.Reset();
	const AShooterGameState* State = GetShooterGameState();
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	if (State == nullptr || Controller == nullptr)
	{
		return;
	}
	// The lines are formatted when the log changes (a message), not every frame.
	const TArray<FShooterRadioEntry>& Log = State->GetRadioLog();
	if (State->GetRadioSerial() != RadioSerial || RadioLines.Num() != Log.Num())
	{
		RadioSerial = State->GetRadioSerial();
		RadioLines.SetNum(Log.Num());
		for (int32 Index = 0; Index < Log.Num(); ++Index)
		{
			const FShooterRadioEntry& Entry = Log[Index];
			FRadioLine& Line = RadioLines[Index];
			Line.Sender.Set(FString::Printf(TEXT("%s (RADIO): "), *Entry.SenderName));
			Line.Message.Set(GetRadioMessageText(Entry.Message));
			Line.Color = FLinearColor(GetTeamColor(Entry.Team));
			NumTextFormats += 2;
		}
	}
	// Whose radio: the player's team, else (a spectator without one) the watched player's; everyone's without either.
	const AShooterPlayerState* OwnState = Controller->GetPlayerState<AShooterPlayerState>();
	EShooterTeam Team = OwnState != nullptr ? OwnState->GetTeam() : EShooterTeam::None;
	const AShooterCharacter* Watched = Controller->GetViewedPlayer();
	if (Team == EShooterTeam::None && Watched != nullptr)
	{
		Team = Watched->GetTeam();
	}
	const float Now = GetWorldTime();
	for (int32 Index = 0; Index < Log.Num(); ++Index)
	{
		if (Now - Log[Index].Time <= RadioMessageDuration && (Team == EShooterTeam::None || Log[Index].Team == Team))
		{
			DrawnRadioLines.Add(Index);
		}
	}
	// Above the health, the newest at the bottom (CS's chat area).
	float Y = GetBottomRowY() - 8.0f - (LineStep() * static_cast<float>(DrawnRadioLines.Num()));
	for (const int32 Index : DrawnRadioLines)
	{
		const FRadioLine& Line = RadioLines[Index];
		DrawHUDText(Line.Sender, EdgeMargin, Y, Line.Color);
		DrawHUDText(Line.Message, EdgeMargin + Line.Sender.Width, Y, MessageColor);
		Y += LineStep();
	}
}

FString AShooterHUD::GetRadioLineText(int32 Index) const
{
	if (!DrawnRadioLines.IsValidIndex(Index) || !RadioLines.IsValidIndex(DrawnRadioLines[Index]))
	{
		return FString();
	}
	const FRadioLine& Line = RadioLines[DrawnRadioLines[Index]];
	return Line.Sender.Text + Line.Message.Text;
}

FLinearColor AShooterHUD::GetRadioLineColor(int32 Index) const
{
	return DrawnRadioLines.IsValidIndex(Index) && RadioLines.IsValidIndex(DrawnRadioLines[Index])
		? RadioLines[DrawnRadioLines[Index]].Color
		: FLinearColor::White;
}

void AShooterHUD::DrawRadioMenu()
{
	NumRadioMenuLines = 0;
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	const int32 Menu = Controller != nullptr ? Controller->GetRadioMenu() : 0;
	if (Menu == 0)
	{
		return;
	}
	// Formatted when another menu opens.
	const TArrayView<const EShooterRadioMessage> Messages = GetRadioMenuMessages(Menu);
	if (Menu != RadioMenuShown)
	{
		RadioMenuShown = Menu;
		RadioMenuTitle.Set(GetRadioMenuTitle(Menu), GetBoldFont());
		RadioMenuLines.SetNum(Messages.Num());
		for (int32 Index = 0; Index < Messages.Num(); ++Index)
		{
			RadioMenuLines[Index].Set(FString::Printf(TEXT("%d. %s"), Index + 1, GetRadioMessageText(Messages[Index])));
		}
		NumTextFormats += 1 + Messages.Num();
	}
	// On a dark panel where the buy menu sits.
	constexpr float Padding = 8.0f;
	float Width = RadioMenuTitle.Width;
	for (const FShooterHUDText& Line : RadioMenuLines)
	{
		Width = FMath::Max(Width, Line.Width);
	}
	const float BuyMenuTop = GetMenuTop();
	Canvas->DrawTile(BuyMenuLeft, BuyMenuTop, Width + (2.0f * Padding),
		(LineStep() * static_cast<float>(RadioMenuLines.Num() + 1)) + (2.0f * Padding) - 4.0f,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	const float X = BuyMenuLeft + Padding;
	float Y = BuyMenuTop + Padding;
	DrawHUDText(RadioMenuTitle, X, Y, StatusColor);
	++NumRadioMenuLines;
	for (const FShooterHUDText& Line : RadioMenuLines)
	{
		Y += LineStep();
		DrawHUDText(Line, X, Y, MessageColor);
		++NumRadioMenuLines;
	}
}

void AShooterHUD::DrawMessages()
{
	const AShooterGameState* State = GetShooterGameState();
	if (State == nullptr)
	{
		return;
	}
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float Y = static_cast<float>(Canvas->GetSizeY()) * 0.3f;
	// What the message is made of (its key), then the message only when that changed.
	const EShooterRoundState RoundState = State->GetRoundState();
	const EShooterRoundEndReason Reason = State->GetLastRoundEndReason();
	const EShooterTeam Winner = State->GetMatchWinner();
	const int32 ScoreCT = State->GetTeamScore(EShooterTeam::CT);
	const int32 ScoreT = State->GetTeamScore(EShooterTeam::T);
	const EShooterBombState BombState = State->GetBombState();
	const FName Site = State->GetBombSite();
	// Only the terrorists are told of the dropped bomb (CS: their radar shows it).
	const AShooterCharacter* Viewed = GetViewedPawn();
	const bool bToldOfDrop = Viewed != nullptr && Viewed->GetTeam() == EShooterTeam::T;
	// The second half's first freeze says the teams switched sides.
	const bool bSidesSwitched = RoundState == EShooterRoundState::Freeze && State->IsSecondHalf() &&
		State->GetRoundNumber() == State->GetHalftimeRound() + 1;
	FShooterHUDTextKey Key = MakeKey(static_cast<int64>(RoundState));
	FLinearColor Color = MessageColor;
	switch (RoundState)
	{
		case EShooterRoundState::Warmup:
			break;
		case EShooterRoundState::RoundEnd:
			Key = MakeKey(static_cast<int64>(RoundState), static_cast<int64>(Reason));
			Color = FLinearColor(GetTeamColor(GetRoundEndWinner(Reason)));
			break;
		case EShooterRoundState::MatchEnd:
			Key = MakeKey(static_cast<int64>(RoundState), static_cast<int64>(Winner), ScoreCT, ScoreT);
			break;
		case EShooterRoundState::Freeze:
		case EShooterRoundState::Live:
			Key = MakeKey(static_cast<int64>(RoundState), static_cast<int64>(BombState),
				BombState == EShooterBombState::Planted ? ToKey(Site) : 0,
				(BombState == EShooterBombState::Dropped && bToldOfDrop ? 1 : 0) | (bSidesSwitched ? 2 : 0));
			Color = BombState == EShooterBombState::Planted || BombState == EShooterBombState::Dropped ? BombColor
																									   : MessageColor;
			break;
	}
	UpdateText(
		MessageText, Key,
		[RoundState, Reason, Winner, ScoreCT, ScoreT, BombState, Site, bToldOfDrop, bSidesSwitched]()
		{
			switch (RoundState)
			{
				case EShooterRoundState::Warmup:
					return FString(TEXT("Waiting for players on both teams"));
				case EShooterRoundState::RoundEnd:
					return FString(GetRoundEndMessage(Reason));
				case EShooterRoundState::MatchEnd:
					return FString::Printf(TEXT("%s  %d - %d"),
						Winner == EShooterTeam::CT      ? TEXT("Counter-Terrorists win the match")
							: Winner == EShooterTeam::T ? TEXT("Terrorists win the match")
														: TEXT("The match is a draw"),
						ScoreCT, ScoreT);
				case EShooterRoundState::Freeze:
				case EShooterRoundState::Live:
					if (BombState == EShooterBombState::Planted)
					{
						return FString::Printf(TEXT("The bomb has been planted at %s"), *Site.ToString());
					}
					if (BombState == EShooterBombState::Dropped && bToldOfDrop)
					{
						return FString(TEXT("The bomb has been dropped"));
					}
					if (bSidesSwitched)
					{
						return FString(TEXT("Halftime: the teams have switched sides"));
					}
					break;
			}
			return FString();
		},
		GetBoldFont());
	DrawHUDText(MessageText, CenterX, Y, Color, ETextJustify::Center);
}

void AShooterHUD::DrawBuyRefusal()
{
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	if (Controller == nullptr || Controller->IsBuyMenuOpen() || Controller->GetBuyRefusalTime() < 0.0f ||
		GetWorldTime() - Controller->GetBuyRefusalTime() > BuyRefusalDuration)
	{
		return;
	}
	DrawHUDText(GetTextFont(), Controller->GetLastBuyMessage(), BuyMenuLeft, GetMenuTop(), RefusalColor);
}

void AShooterHUD::DrawPickupNotice()
{
	bPickupNoticeShown = false;
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	const AShooterCharacter* Pawn = GetViewedPawn();
	if (Controller == nullptr || Pawn == nullptr || !Pawn->IsAlive() || Controller->GetPickupTime() < 0.0f ||
		GetWorldTime() - Controller->GetPickupTime() > PickupNoticeDuration)
	{
		return;
	}
	const FString& Message = Controller->GetPickupMessage();
	// Formatted once per pickup (its time is the key).
	UpdateText(PickupText, MakeKey(static_cast<int64>(Controller->GetPickupTime() * 1000.0f)),
		[&Message]() { return Message; });
	// Above the money: the ammunition, the weapon's name and the money under it.
	const float Right = static_cast<float>(Canvas->GetSizeX()) - EdgeMargin;
	const float Y = GetBottomRowY() - (2.0f * LineHeightOf(GetNumberFont())) - HUDLineHeight() - 8.0f;
	DrawHUDText(PickupText, Right, Y, StatusColor, ETextJustify::Right);
	bPickupNoticeShown = true;
}

void AShooterHUD::DrawProgress()
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	if (Pawn == nullptr || (!Pawn->IsPlanting() && !Pawn->IsDefusing()))
	{
		return;
	}
	const float Now = GetWorldTime();
	float Start = 0.0f;
	float End = 0.0f;
	FString Label;
	if (Pawn->IsPlanting() && Pawn->GetCarriedBomb() != nullptr)
	{
		End = Pawn->GetPlantEndTime();
		Start = End - Pawn->GetCarriedBomb()->PlantDuration;
		Label = TEXT("Planting the bomb");
	}
	else if (const AShooterGameMode* GameMode =
				 GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AShooterGameMode>() : nullptr)
	{
		const AShooterBomb* Bomb = GameMode->GetBomb();
		if (Bomb == nullptr)
		{
			return;
		}
		End = Bomb->GetDefuseEndTime();
		Start = End - (Pawn->HasDefuseKit() ? Bomb->DefuseKitDuration : Bomb->DefuseDuration);
		Label = TEXT("Defusing the bomb");
	}
	const float Fraction = End > Start ? FMath::Clamp((Now - Start) / (End - Start), 0.0f, 1.0f) : 0.0f;
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float Y = static_cast<float>(Canvas->GetSizeY()) * 0.62f;
	constexpr float BarWidth = 300.0f;
	constexpr float BarHeight = 12.0f;
	DrawHUDText(GetTextFont(), Label, CenterX, Y - LineStep(), MessageColor, ETextJustify::Center);
	Canvas->DrawTile(CenterX - (BarWidth * 0.5f), Y, BarWidth, BarHeight, FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
	Canvas->DrawTile(CenterX - (BarWidth * 0.5f), Y, BarWidth * Fraction, BarHeight, StatusColor);
}

void AShooterHUD::DrawHitMarker()
{
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	const float Now = GetWorldTime();
	if (Controller == nullptr || Controller->GetLastHitTime() < 0.0f ||
		Now - Controller->GetLastHitTime() > HitMarkerDuration)
	{
		return;
	}
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float CenterY = static_cast<float>(Canvas->GetSizeY()) * 0.5f;
	const FLinearColor Color = Controller->WasLastHitKill() ? FLinearColor(1.0f, 0.15f, 0.1f) : FLinearColor::White;
	const float Inner = GetCrosshairGap(static_cast<float>(Canvas->GetSizeY())) + 3.0f;
	const float Outer = Inner + (Controller->WasLastHitHeadshot() ? 9.0f : 6.0f);
	for (const float SignX : {-1.0f, 1.0f})
	{
		for (const float SignY : {-1.0f, 1.0f})
		{
			Canvas->DrawLine(CenterX + (SignX * Inner), CenterY + (SignY * Inner), CenterX + (SignX * Outer),
				CenterY + (SignY * Outer), Color, CrosshairThickness);
		}
	}
}

bool AShooterHUD::DrawScope()
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	const AShooterWeapon_AWP* Sniper = Pawn != nullptr ? Cast<AShooterWeapon_AWP>(Pawn->GetWeapon()) : nullptr;
	if (Sniper == nullptr || !Sniper->IsZoomed())
	{
		return false;
	}
	// A square view the height of the screen, black bars on the sides, thin black cross lines (CS's scope).
	const float Width = static_cast<float>(Canvas->GetSizeX());
	const float Height = static_cast<float>(Canvas->GetSizeY());
	const float Side = FMath::Max(0.0f, (Width - Height) * 0.5f);
	const FLinearColor Black(0.0f, 0.0f, 0.0f, 1.0f);
	Canvas->DrawTile(0.0f, 0.0f, Side, Height, Black);
	Canvas->DrawTile(Width - Side, 0.0f, Side, Height, Black);
	Canvas->DrawLine(Side, Height * 0.5f, Width - Side, Height * 0.5f, Black, 1.0f);
	Canvas->DrawLine(Width * 0.5f, 0.0f, Width * 0.5f, Height, Black, 1.0f);
	return true;
}

float AShooterHUD::GetCrosshairGap(float ViewHeight) const
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	const AShooterWeapon_Instant* Weapon = Pawn != nullptr ? Cast<AShooterWeapon_Instant>(Pawn->GetWeapon()) : nullptr;
	if (Weapon == nullptr || ViewHeight <= 0.0f)
	{
		return CrosshairGap;
	}
	// The spread's half angle on the screen: the view's vertical field of view spans its height.
	const float FieldOfView = FMath::Max(1.0f, Pawn->GetFirstPersonCameraComponent()->FieldOfView());
	const float HalfHeight = ViewHeight * 0.5f;
	const float SpreadPixels = HalfHeight * FMath::Tan(FMath::DegreesToRadians(Weapon->GetCurrentSpread())) /
		FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	// CS's ACCURACY_DUCK: crouched on the floor, the crosshair's own gap closes with the weapon's crouched spread too,
	// so it visibly tightens even where the spread is a pixel or two.
	const float BaseGap =
		Pawn->bIsCrouched && Pawn->IsMovingOnGround() ? CrosshairGap * Weapon->CrouchingSpreadMod : CrosshairGap;
	return BaseGap + (SpreadPixels * CrosshairSpreadScale);
}

void AShooterHUD::DrawCrosshair()
{
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float CenterY = static_cast<float>(Canvas->GetSizeY()) * 0.5f;
	const float Inner = GetCrosshairGap(static_cast<float>(Canvas->GetSizeY()));
	const float Outer = Inner + CrosshairLength;
	Canvas->DrawLine(CenterX - Outer, CenterY, CenterX - Inner, CenterY, CrosshairColor, CrosshairThickness);
	Canvas->DrawLine(CenterX + Inner, CenterY, CenterX + Outer, CenterY, CrosshairColor, CrosshairThickness);
	Canvas->DrawLine(CenterX, CenterY - Outer, CenterX, CenterY - Inner, CrosshairColor, CrosshairThickness);
	Canvas->DrawLine(CenterX, CenterY + Inner, CenterX, CenterY + Outer, CrosshairColor, CrosshairThickness);
}

// The buy menu

const FLinearColor UShooterBuyMenuWidget::AffordableColor(1.0f, 1.0f, 1.0f);
const FLinearColor UShooterBuyMenuWidget::UnaffordableColor(0.45f, 0.45f, 0.45f);
const FLinearColor UShooterBuyMenuWidget::CategoryColor(1.0f, 0.75f, 0.2f);

namespace
{
	/** The buy menu table's columns. */
	const FName BuyKeyColumn(TEXT("Key"));
	const FName BuyItemColumn(TEXT("Item"));
	const FName BuyPriceColumn(TEXT("Price"));
} // namespace

UShooterBuyMenuWidget::UShooterBuyMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UTextBlock* UShooterBuyMenuWidget::AddLine(UVerticalBox& Box, float TopPadding)
{
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	Box.AddChildToVerticalBox(Line)->SetPadding(FMargin(0.0f, TopPadding, 0.0f, 0.0f));
	return Line;
}

void UShooterBuyMenuWidget::NativeOnInitialized()
{
	const AShooterHUD* HUD = Cast<AShooterHUD>(GetOwningHUD());
	const FSlateFontInfo BoldFont(const_cast<UFont*>(HUD != nullptr ? HUD->GetBoldFont() : nullptr), 14);
	// Canvas > Border (the panel) > VerticalBox > the heading (a horizontal box: the page, the money), the refusal,
	// the page's table and the last buy.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f));
	Panel->SetPadding(FMargin(8.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetPosition(FVector2D(BuyMenuLeft, HUD != nullptr ? HUD->GetMenuTop() : 136.0f));
	PanelSlot->SetAutoSize(true);
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Box);
	constexpr float LineGap = 4.0f;
	UHorizontalBox* Heading = WidgetTree->ConstructWidget<UHorizontalBox>();
	Box->AddChildToVerticalBox(Heading);
	TitleText = WidgetTree->ConstructWidget<UTextBlock>();
	TitleText->SetFont(BoldFont);
	TitleText->SetColorAndOpacity(CategoryColor);
	UHorizontalBoxSlot* TitleSlot = Heading->AddChildToHorizontalBox(TitleText);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MoneyText = WidgetTree->ConstructWidget<UTextBlock>();
	MoneyText->SetFont(BoldFont);
	MoneyText->SetColorAndOpacity(FLinearColor(0.55f, 0.9f, 0.35f));
	UHorizontalBoxSlot* MoneySlot = Heading->AddChildToHorizontalBox(MoneyText);
	MoneySlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
	MoneySlot->SetHorizontalAlignment(HAlign_Right);
	RefusalText = AddLine(*Box, LineGap);
	RefusalText->SetColorAndOpacity(RefusalColor);
	RefusalText->SetVisibility(ESlateVisibility::Collapsed);
	// The page: the number key, the name, the price (right-aligned); no header, no row bands (the highlight is the
	// pad's line).
	ItemTable = WidgetTree->ConstructWidget<UTableView>();
	ItemTable->AddColumn(FTableViewColumn(BuyKeyColumn, FText::FromString(TEXT("#")), 20.0f, HAlign_Right));
	ItemTable->AddColumn(FTableViewColumn(BuyItemColumn, FText::FromString(TEXT("Item")), 0.0f, HAlign_Left));
	ItemTable->AddColumn(FTableViewColumn(BuyPriceColumn, FText::FromString(TEXT("Price")), 56.0f, HAlign_Right));
	ItemTable->SetShowHeader(false);
	ItemTable->RowBackgroundColor = FLinearColor::Transparent;
	ItemTable->AlternateRowBackgroundColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.04f);
	ItemTable->HighlightBackgroundColor = FLinearColor(0.55f, 0.42f, 0.1f, 0.55f);
	ItemTable->CellPadding = FMargin(4.0f, 2.0f);
	Box->AddChildToVerticalBox(ItemTable)->SetPadding(FMargin(0.0f, LineGap, 0.0f, 0.0f));
	LastBuyText = AddLine(*Box, LineGap * 2.0f);
	LastBuyText->SetColorAndOpacity(FLinearColor(0.7f, 0.9f, 0.7f));
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UShooterBuyMenuWidget::NativeTick(float /*DeltaTime*/)
{
	const AShooterHUD* HUD = Cast<AShooterHUD>(GetOwningHUD());
	const AShooterPlayerController* Controller = HUD != nullptr ? HUD->GetShooterPlayerController() : nullptr;
	const bool bOpen = Controller != nullptr && Controller->IsBuyMenuOpen();
	Panel->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bOpen)
	{
		return;
	}
	const UWorld* World = HUD->GetWorld();
	const AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	const AShooterCharacter* Pawn = HUD->GetViewedPawn();
	const AShooterPlayerState* State = Controller->GetPlayerState<AShooterPlayerState>();
	// Each line is set again only when what it shows changed (the menu is open for seconds at a time).
	const int32 Money = State != nullptr ? State->GetMoney() : 0;
	const int32 Category = Controller->GetBuyMenuCategory();
	if (TitleLine.Update(MakeKey(Category),
			[Category]() { return FString(AShooterPlayerController::GetBuyMenuCategoryLabel(Category)); }))
	{
		TitleText->SetText(FText::FromString(TitleLine.Text));
		++NumTextFormats;
	}
	if (MoneyLine.Update(MakeKey(Money), [Money]() { return FString::Printf(TEXT("$ %d"), Money); }))
	{
		MoneyText->SetText(FText::FromString(MoneyLine.Text));
		++NumTextFormats;
	}
	FString Refusal;
	const bool bCanBuy = GameMode != nullptr && Pawn != nullptr && GameMode->CanBuy(*Pawn, &Refusal);
	if (bCanBuy)
	{
		Refusal.Reset();
	}
	else if (Refusal.IsEmpty())
	{
		Refusal = TEXT("You cannot buy now");
	}
	SetLineText(*RefusalText, ShownRefusal, Refusal);
	const ESlateVisibility RefusalVisibility =
		Refusal.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	if (RefusalText->GetVisibility() != RefusalVisibility)
	{
		RefusalText->SetVisibility(RefusalVisibility);
	}
	// The page's lines: the categories, or the category's items with their prices.
	TArray<FShooterBuyMenuEntry, TInlineAllocator<AShooterPlayerController::MaxBuyMenuEntries>> Entries;
	Controller->GetBuyMenuEntries(Entries);
	if (ItemTable->GetNumRows() != Entries.Num())
	{
		// Another page: its rows, their lines formatted again.
		ItemTable->ClearRows();
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			(void)ItemTable->AddRow();
		}
		ItemLines.Reset();
	}
	ItemLines.SetNum(Entries.Num());
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FShooterBuyMenuEntry& Entry = Entries[Index];
		const int32 Price = Entry.Item != nullptr && GameMode != nullptr && Pawn != nullptr
			? GameMode->GetPrice(*Pawn, Entry.Item)
			: -1;
		const bool bAffordable = bCanBuy && (Entry.Item == nullptr || (Price >= 0 && Money >= Price));
		const bool bChanged = ItemLines[Index].Update(
			MakeKey(Price, bAffordable ? 1 : 0, ToKey(Entry.Label), Entry.Category),
			[&Entry]()
			{
				// A category's name, an item's display name (CS's: "AK-47", "Kevlar + Helmet").
				return Entry.Item == nullptr ? FString(Entry.Label) : AShooterWeapon::GetItemDisplayName(Entry.Label);
			});
		if (bChanged)
		{
			ItemTable->SetCellText(Index, 0, FString::FromInt(Index + 1));
			ItemTable->SetCellText(Index, 1, ItemLines[Index].Text);
			ItemTable->SetCellText(Index, 2,
				Entry.Item == nullptr ? FString(TEXT(">"))
					: Price >= 0      ? FString::Printf(TEXT("$%d"), Price)
									  : FString(TEXT("-")));
			ItemTable->SetRowColor(Index,
				Entry.Item == nullptr ? CategoryColor
					: bAffordable     ? AffordableColor
									  : UnaffordableColor);
			++NumTextFormats;
		}
	}
	// The pad's line.
	const int32 Selection = Controller->GetBuyMenuSelection();
	if (ItemTable->GetHighlightedRow() != Selection)
	{
		ItemTable->SetHighlightedRow(Entries.IsValidIndex(Selection) ? Selection : INDEX_NONE);
	}
	SetLineText(*LastBuyText, ShownLastBuy, Controller->GetLastBuyMessage());
}

void UShooterBuyMenuWidget::SetLineText(UTextBlock& Block, FString& Shown, const FString& Text)
{
	if (!Shown.Equals(Text, ESearchCase::CaseSensitive))
	{
		Shown = Text;
		Block.SetText(FText::FromString(Text));
		++NumTextFormats;
	}
}
