#include "ShooterHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasTypes.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
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

	/** The HUD's font: the engine's small font (DejaVu Sans Condensed, 14 pixels). */
	const UFont* HUDFont()
	{
		return UEngine::GetSmallFont();
	}

	/** A line of the HUD font, pixels. */
	float HUDLineHeight()
	{
		const UFont* Font = HUDFont();
		return Font != nullptr ? Font->GetLineHeight() : 0.0f;
	}

	/** The HUD's layout on the 640 x 448 canvas, in lines of the HUD font (HUDLineHeight). */
	constexpr float EdgeMargin = 12.0f;
	float LineStep()
	{
		return HUDLineHeight() + 4.0f;
	}

	/** The buy menu's panel's top-left corner on the canvas (the refusal is drawn there too), below the radar. */
	constexpr float BuyMenuTop = 112.0f;

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

	FColor GetTeamColor(EShooterTeam Team)
	{
		return Team == EShooterTeam::T ? TColor : Team == EShooterTeam::CT ? CTColor : FColor::White;
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

	/** A kept line centred on X. */
	void DrawCentredText(FCanvas& Canvas, const FShooterHUDText& Line, float X, float Y, const FLinearColor& Color)
	{
		Canvas.DrawText(HUDFont(), Line.Text, X - (Line.Width * 0.5f), Y, Color);
	}

	/** A text centred on X (measured now: the lines that change with the round's events). */
	void DrawCentredText(FCanvas& Canvas, const FString& Text, float X, float Y, const FLinearColor& Color)
	{
		float Width = 0.0f;
		float Height = 0.0f;
		FCanvas::MeasureText(HUDFont(), Text, Width, Height);
		Canvas.DrawText(HUDFont(), Text, X - (Width * 0.5f), Y, Color);
	}

} // namespace

void FShooterHUDText::Set(const FString& InText)
{
	Text = InText;
	FCanvas::MeasureText(HUDFont(), Text, Width, Height);
	bFormatted = true;
}

AShooterHUD::AShooterHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AShooterHUD::BeginPlay()
{
	Super::BeginPlay();
	(void)AddWidget<UShooterBuyMenuWidget>();
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
	DrawRoundInfo();
	DrawKillFeed();
	DrawRadio();
	DrawMessages();
	DrawBuyRefusal();
	DrawPickupNotice();
	DrawRadioMenu();
	DrawProgress();
	DrawSpectatorInfo();
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	// The scoreboard covers the radar's corner.
	if (Controller != nullptr && Controller->IsScoreboardShown())
	{
		NumRadarPrimitives = 0;
		DrawScoreboard();
	}
	else
	{
		DrawRadar();
	}
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

void AShooterHUD::DrawRadar()
{
	NumRadarPrimitives = 0;
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
		Canvas->DrawText(HUDFont(), RadarSiteLabels[Index], CenterX + Offset.X - LetterHalfWidth,
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
	DrawCentredText(*Canvas, SpectatorText, CenterX, Y, MessageColor);
}

void AShooterHUD::DrawStatus()
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	const AShooterPlayerState* State =
		PlayerOwner != nullptr ? PlayerOwner->GetPlayerState<AShooterPlayerState>() : nullptr;
	const float Width = static_cast<float>(Canvas->GetSizeX());
	const float Bottom = static_cast<float>(Canvas->GetSizeY()) - EdgeMargin - HUDLineHeight();
	if (State != nullptr && GetShooterGameState() != nullptr)
	{
		const int32 Money = State->GetMoney();
		UpdateText(MoneyText, MakeKey(Money), [Money]() { return FString::Printf(TEXT("$ %d"), Money); });
		Canvas->DrawText(HUDFont(), MoneyText.Text, EdgeMargin, Bottom - LineStep(), StatusColor);
	}
	if (Pawn == nullptr)
	{
		return;
	}
	const int32 Health = FMath::CeilToInt(Pawn->GetHealth());
	const bool bArmor = Pawn->GetArmor() > 0.0f;
	const int32 Armor = bArmor ? FMath::CeilToInt(Pawn->GetArmor()) : -1;
	const bool bHelmet = bArmor && Pawn->HasHelmet();
	UpdateText(StatusText, MakeKey(Health, Armor, bHelmet ? 1 : 0),
		[Health, bArmor, Armor, bHelmet]()
		{
			FString Status = FString::Printf(TEXT("+ %d"), Health);
			if (bArmor)
			{
				Status += FString::Printf(TEXT("   [] %d%s"), Armor, bHelmet ? TEXT(" H") : TEXT(""));
			}
			return Status;
		});
	Canvas->DrawText(HUDFont(), StatusText.Text, EdgeMargin, Bottom, StatusColor);
	const AShooterWeapon* Weapon = Pawn->GetWeapon();
	const int32 Clip = Weapon != nullptr ? Weapon->GetCurrentAmmoInClip() : 0;
	const int32 Reserve = Weapon != nullptr ? Weapon->GetCurrentAmmo() : 0;
	const AShooterWeapon_Instant* InstantWeapon = Cast<AShooterWeapon_Instant>(Weapon);
	const int32 WeaponMode = Pawn->IsBombDrawn() ? 3
		: InstantWeapon == nullptr               ? 0
		: InstantWeapon->IsSilenced()            ? 1
		: InstantWeapon->IsBurstMode()           ? 2
												 : 0;
	UpdateText(WeaponText, MakeKey(ToKey(Weapon), Clip, Reserve, WeaponMode),
		[Weapon, Clip, Reserve, WeaponMode]()
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
			const TCHAR* Mode = WeaponMode == 1 ? TEXT(" (silenced)") : WeaponMode == 2 ? TEXT(" (burst)") : TEXT("");
			return Weapon->AmmoPerClip > 1 || Weapon->MaxAmmo > 0
				? FString::Printf(TEXT("%s%s  %d | %d"), *Weapon->WeaponName, Mode, Clip, Reserve)
				: Weapon->WeaponName;
		});
	const bool bBomb = Pawn->GetCarriedBomb() != nullptr;
	const bool bKit = Pawn->HasDefuseKit();
	UpdateText(ItemsText, MakeKey(bBomb ? 1 : 0, bKit ? 1 : 0),
		[bBomb, bKit]()
		{
			FString Items;
			if (bBomb)
			{
				Items += TEXT("C4 ");
			}
			if (bKit)
			{
				Items += TEXT("KIT ");
			}
			return Items;
		});
	if (!WeaponText.Text.IsEmpty())
	{
		Canvas->DrawText(HUDFont(), WeaponText.Text, Width - WeaponText.Width - EdgeMargin, Bottom, StatusColor);
	}
	if (!ItemsText.Text.IsEmpty())
	{
		Canvas->DrawText(
			HUDFont(), ItemsText.Text, Width - ItemsText.Width - EdgeMargin, Bottom - LineStep(), BombColor);
	}
}

void AShooterHUD::DrawRoundInfo()
{
	const AShooterGameState* State = GetShooterGameState();
	if (State == nullptr || State->GetRoundState() == EShooterRoundState::Warmup)
	{
		return;
	}
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float Now = GetWorldTime();
	// The clock: C4 once planted, else the phase's seconds (formatted once a second).
	const bool bPlanted = State->GetBombState() == EShooterBombState::Planted;
	const bool bTimed =
		State->GetRoundState() == EShooterRoundState::Freeze || State->GetRoundState() == EShooterRoundState::Live;
	const int32 Seconds = bTimed ? GetClockSeconds(State->GetPhaseTimeRemaining(Now)) : 0;
	UpdateText(ClockText,
		MakeKey(bPlanted ? 1
				: bTimed ? 2
						 : 0,
			bPlanted ? 0 : Seconds),
		[bPlanted, bTimed, Seconds]()
		{ return bPlanted ? FString(TEXT("C4"))
			  : bTimed    ? FormatClock(Seconds)
						  : FString(); });
	if (!ClockText.Text.IsEmpty())
	{
		DrawCentredText(*Canvas, ClockText, CenterX, EdgeMargin, bPlanted ? BombColor : StatusColor);
	}
	// The scores either side of the clock, CT's ending and T's starting a clock's width away from the centre.
	const int32 ScoreCT = State->GetTeamScore(EShooterTeam::CT);
	const int32 ScoreT = State->GetTeamScore(EShooterTeam::T);
	const int32 RoundNumber = State->GetRoundNumber();
	UpdateText(CTScoreText, MakeKey(ScoreCT), [ScoreCT]() { return FString::Printf(TEXT("CT %d"), ScoreCT); });
	UpdateText(TScoreText, MakeKey(ScoreT), [ScoreT]() { return FString::Printf(TEXT("%d T"), ScoreT); });
	UpdateText(
		RoundText, MakeKey(RoundNumber), [RoundNumber]() { return FString::Printf(TEXT("Round %d"), RoundNumber); });
	constexpr float ScoreGap = 40.0f;
	Canvas->DrawText(HUDFont(), CTScoreText.Text, CenterX - ScoreGap - CTScoreText.Width, EdgeMargin, CTColor);
	Canvas->DrawText(HUDFont(), TScoreText.Text, CenterX + ScoreGap, EdgeMargin, TColor);
	DrawCentredText(*Canvas, RoundText, CenterX, EdgeMargin + LineStep(), FLinearColor(0.7f, 0.7f, 0.7f));
}

void AShooterHUD::DrawKillFeed()
{
	const AShooterGameState* State = GetShooterGameState();
	if (State == nullptr)
	{
		return;
	}
	const float Width = static_cast<float>(Canvas->GetSizeX());
	const float Now = GetWorldTime();
	// Below the engine's stats (top right) when they show.
	float Y = EdgeMargin;
	if (GEngine != nullptr && GEngine->IsHudStatsVisible())
	{
		Y = FMath::Max(Y, GEngine->GetDebugOverlay().GetRightTextBottom() + 4.0f);
	}
	// The lines are formatted and measured when the feed changes (a kill), not every frame.
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
			Line.Middle.Set(
				FString::Printf(TEXT(" [%s%s] "), *Entry.WeaponName, Entry.bHeadshot ? TEXT(" HS") : TEXT("")));
			Line.Victim.Set(Entry.VictimName);
			NumTextFormats += 3;
		}
	}
	for (int32 Index = 0; Index < Feed.Num(); ++Index)
	{
		const FShooterKillFeedEntry& Entry = Feed[Index];
		if (Now - Entry.Time > KillFeedDuration)
		{
			continue;
		}
		const FKillFeedLine& Line = KillFeedLines[Index];
		float X = Width - EdgeMargin - Line.Killer.Width - Line.Middle.Width - Line.Victim.Width;
		if (!Line.Killer.Text.IsEmpty())
		{
			Canvas->DrawText(HUDFont(), Line.Killer.Text, X, Y, GetTeamColor(Entry.KillerTeam));
		}
		X += Line.Killer.Width;
		Canvas->DrawText(HUDFont(), Line.Middle.Text, X, Y, FColor::White);
		X += Line.Middle.Width;
		Canvas->DrawText(HUDFont(), Line.Victim.Text, X, Y, GetTeamColor(Entry.VictimTeam));
		Y += Line.Victim.Height + 4.0f;
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
	// Above the money, the newest at the bottom (CS's chat area).
	const float Bottom = static_cast<float>(Canvas->GetSizeY()) - EdgeMargin - HUDLineHeight();
	float Y = Bottom - (LineStep() * static_cast<float>(DrawnRadioLines.Num() + 1));
	for (const int32 Index : DrawnRadioLines)
	{
		const FRadioLine& Line = RadioLines[Index];
		Canvas->DrawText(HUDFont(), Line.Sender.Text, EdgeMargin, Y, Line.Color);
		Canvas->DrawText(HUDFont(), Line.Message.Text, EdgeMargin + Line.Sender.Width, Y, MessageColor);
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
		RadioMenuTitle.Set(GetRadioMenuTitle(Menu));
		RadioMenuLines.SetNum(Messages.Num());
		for (int32 Index = 0; Index < Messages.Num(); ++Index)
		{
			RadioMenuLines[Index].Set(FString::Printf(TEXT("%d. %s"), Index + 1, GetRadioMessageText(Messages[Index])));
		}
		NumTextFormats += 1 + Messages.Num();
	}
	const float X = EdgeMargin * 2.0f;
	float Y = BuyMenuTop;
	Canvas->DrawText(HUDFont(), RadioMenuTitle.Text, X, Y, StatusColor);
	++NumRadioMenuLines;
	for (const FShooterHUDText& Line : RadioMenuLines)
	{
		Y += LineStep();
		Canvas->DrawText(HUDFont(), Line.Text, X, Y, MessageColor);
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
	UpdateText(MessageText, Key,
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
		});
	if (!MessageText.Text.IsEmpty())
	{
		DrawCentredText(*Canvas, MessageText, CenterX, Y, Color);
	}
}

void AShooterHUD::DrawBuyRefusal()
{
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	if (Controller == nullptr || Controller->IsBuyMenuOpen() || Controller->GetBuyRefusalTime() < 0.0f ||
		GetWorldTime() - Controller->GetBuyRefusalTime() > BuyRefusalDuration)
	{
		return;
	}
	Canvas->DrawText(HUDFont(), Controller->GetLastBuyMessage(), EdgeMargin * 2.0f, BuyMenuTop, RefusalColor);
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
	const float Width = static_cast<float>(Canvas->GetSizeX());
	const float Bottom = static_cast<float>(Canvas->GetSizeY()) - EdgeMargin - HUDLineHeight();
	Canvas->DrawText(
		HUDFont(), PickupText.Text, Width - PickupText.Width - EdgeMargin, Bottom - (2.0f * LineStep()), StatusColor);
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
	DrawCentredText(*Canvas, Label, CenterX, Y - LineStep(), MessageColor);
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

void AShooterHUD::DrawScoreboard()
{
	const UWorld* World = GetWorld();
	const AGameModeBase* GameMode = World != nullptr ? World->GetAuthGameMode() : nullptr;
	if (GameMode == nullptr)
	{
		return;
	}
	const AShooterGameState* ShooterGameState = GetShooterGameState();
	const int32 ScoreCT = ShooterGameState != nullptr ? ShooterGameState->GetTeamScore(EShooterTeam::CT) : 0;
	const int32 ScoreT = ShooterGameState != nullptr ? ShooterGameState->GetTeamScore(EShooterTeam::T) : 0;
	// What the board shows (the scores, then each player's state, team, kills, deaths, money and life): the board is
	// formatted again only when it changed.
	NewScoreboardKey.Reset();
	NewScoreboardKey.Add(ScoreCT);
	NewScoreboardKey.Add(ScoreT);
	for (const APlayerState* State : GameMode->GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
		if (ShooterState == nullptr || ShooterState->GetTeam() == EShooterTeam::None)
		{
			continue; // a spectator (the bot match's player) plays for no team
		}
		const AController* Controller = Cast<AController>(ShooterState->GetOwner());
		const AShooterCharacter* Pawn =
			Controller != nullptr ? Cast<AShooterCharacter>(Controller->GetPawn()) : nullptr;
		NewScoreboardKey.Add(ToKey(ShooterState));
		NewScoreboardKey.Add(static_cast<int64>(ShooterState->GetTeam()));
		NewScoreboardKey.Add(ShooterState->GetKills());
		NewScoreboardKey.Add(ShooterState->GetDeaths());
		NewScoreboardKey.Add(ShooterState->GetMoney());
		NewScoreboardKey.Add(Pawn == nullptr || !Pawn->IsAlive() ? 1 : 0);
	}
	if (NewScoreboardKey != ScoreboardKey || ScoreboardCTText.Text.IsEmpty())
	{
		Swap(ScoreboardKey, NewScoreboardKey);
		FString CTLines;
		FString TLines;
		for (const APlayerState* State : GameMode->GetGameState().GetPlayerArray())
		{
			const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
			if (ShooterState == nullptr || ShooterState->GetTeam() == EShooterTeam::None)
			{
				continue;
			}
			const AController* Controller = Cast<AController>(ShooterState->GetOwner());
			const AShooterCharacter* Pawn =
				Controller != nullptr ? Cast<AShooterCharacter>(Controller->GetPawn()) : nullptr;
			const bool bDead = Pawn == nullptr || !Pawn->IsAlive();
			FString& Lines = ShooterState->GetTeam() == EShooterTeam::T ? TLines : CTLines;
			Lines += FString::Printf(TEXT("%-14s %3d %3d  $%-5d%s\n"), *ShooterState->GetPlayerName(),
				ShooterState->GetKills(), ShooterState->GetDeaths(), ShooterState->GetMoney(),
				bDead ? TEXT(" dead") : TEXT(""));
		}
		ScoreboardCTText.Set(
			FString::Printf(TEXT("Counter-Terrorists  %d\nName            K   D  Money\n"), ScoreCT) + CTLines);
		ScoreboardTText.Set(FString::Printf(TEXT("Terrorists  %d\nName            K   D  Money\n"), ScoreT) + TLines);
		NumTextFormats += 2;
	}
	// A box around the longer team's lines (the two headers and one line per player).
	const float Width = static_cast<float>(Canvas->GetSizeX());
	constexpr float Top = 80.0f;
	constexpr float Padding = 12.0f;
	Canvas->DrawTile(Width * 0.1f, Top, Width * 0.8f,
		FMath::Max(ScoreboardCTText.Height, ScoreboardTText.Height) + (2.0f * Padding),
		FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Canvas->DrawText(HUDFont(), ScoreboardCTText.Text, Width * 0.12f, Top + Padding, CTColor);
	Canvas->DrawText(HUDFont(), ScoreboardTText.Text, Width * 0.54f, Top + Padding, TColor);
}

// The buy menu

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
	// Canvas > Border (the panel) > VerticalBox > the lines.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f));
	Panel->SetPadding(FMargin(12.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetPosition(FVector2D(EdgeMargin * 2.0f, BuyMenuTop));
	PanelSlot->SetAutoSize(true);
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Box);
	constexpr float LineGap = 4.0f;
	MoneyText = AddLine(*Box, 0.0f);
	MoneyText->SetColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.2f));
	RefusalText = AddLine(*Box, LineGap);
	RefusalText->SetColorAndOpacity(RefusalColor);
	for (int32 Index = 0; Index < AShooterPlayerController::MaxBuyMenuEntries; ++Index)
	{
		ItemTexts.Add(AddLine(*Box, LineGap));
	}
	ItemLines.SetNum(ItemTexts.Num());
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
	if (MoneyLine.Update(MakeKey(Money, Category),
			[Money, Category]()
			{
				return FString::Printf(
					TEXT("%s   $ %d"), AShooterPlayerController::GetBuyMenuCategoryLabel(Category), Money);
			}))
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
	// The page's lines: the categories, or the category's items with their prices.
	TArray<FShooterBuyMenuEntry, TInlineAllocator<AShooterPlayerController::MaxBuyMenuEntries>> Entries;
	Controller->GetBuyMenuEntries(Entries);
	for (int32 Index = 0; Index < ItemTexts.Num(); ++Index)
	{
		UTextBlock& Line = *ItemTexts[Index];
		const bool bShown = Entries.IsValidIndex(Index);
		const ESlateVisibility LineVisibility = bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (Line.GetVisibility() != LineVisibility)
		{
			Line.SetVisibility(LineVisibility);
		}
		if (!bShown)
		{
			continue;
		}
		const FShooterBuyMenuEntry& Entry = Entries[Index];
		const int32 Price = Entry.Item != nullptr && GameMode != nullptr && Pawn != nullptr
			? GameMode->GetPrice(*Pawn, Entry.Item)
			: -1;
		const bool bAffordable = bCanBuy && (Entry.Item == nullptr || (Price >= 0 && Money >= Price));
		// The pad's highlighted line is marked.
		const bool bSelected = Index == Controller->GetBuyMenuSelection();
		const bool bChanged = ItemLines[Index].Update(
			MakeKey(Price, (bSelected ? 1 : 0) | (bAffordable ? 2 : 0), ToKey(Entry.Label), Entry.Category),
			[&Entry, Index, Price, bSelected]()
			{
				const TCHAR* Marker = bSelected ? TEXT(">") : TEXT(" ");
				if (Entry.Item == nullptr)
				{
					return FString::Printf(TEXT("%s%d  %s >"), Marker, Index + 1, Entry.Label);
				}
				return Price >= 0 ? FString::Printf(TEXT("%s%d  %-14s $%d"), Marker, Index + 1, Entry.Label, Price)
								  : FString::Printf(TEXT("%s%d  %-14s  -"), Marker, Index + 1, Entry.Label);
			});
		if (bChanged)
		{
			Line.SetText(FText::FromString(ItemLines[Index].Text));
			Line.SetColorAndOpacity(bAffordable ? FLinearColor(1.0f, 1.0f, 1.0f) : FLinearColor(0.45f, 0.45f, 0.45f));
			++NumTextFormats;
		}
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
