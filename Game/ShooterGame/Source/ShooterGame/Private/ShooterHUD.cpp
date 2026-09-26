#include "ShooterHUD.h"

#include "Camera/CameraComponent.h"
#include "CanvasTypes.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_Instant.h"
#include "Weapons/ShooterWeapon_Sniper.h"

namespace
{

	const FColor CTColor(110, 160, 255);
	const FColor TColor(255, 190, 90);
	const FLinearColor MessageColor(1.0f, 1.0f, 1.0f);
	const FLinearColor BombColor(1.0f, 0.25f, 0.2f);

	FColor GetTeamColor(EShooterTeam Team)
	{
		return Team == EShooterTeam::T ? TColor : Team == EShooterTeam::CT ? CTColor : FColor::White;
	}

	/** m:ss. */
	FString FormatClock(float Seconds)
	{
		const int32 Whole = FMath::CeilToInt(FMath::Max(0.0f, Seconds));
		return FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60);
	}

	/** Text centred on X. */
	void DrawCentredText(FCanvas& Canvas, const FString& Text, float X, float Y, const FLinearColor& Color)
	{
		float Width = 0.0f;
		float Height = 0.0f;
		FCanvas::MeasureText(Text, HudFontScale, Width, Height);
		Canvas.DrawText(Text, X - (Width * 0.5f), Y, Color);
	}

} // namespace

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
	if (!DrawScope())
	{
		DrawCrosshair();
	}
	DrawHitMarker();
	DrawStatus();
	DrawRoundInfo();
	DrawKillFeed();
	DrawMessages();
	DrawProgress();
	const AShooterPlayerController* Controller = GetShooterPlayerController();
	if (Controller != nullptr && Controller->IsScoreboardShown())
	{
		DrawScoreboard();
	}
}

void AShooterHUD::DrawStatus()
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	const AShooterPlayerState* State =
		PlayerOwner != nullptr ? PlayerOwner->GetPlayerState<AShooterPlayerState>() : nullptr;
	const float Width = static_cast<float>(Canvas->GetSizeX());
	const float Bottom = static_cast<float>(Canvas->GetSizeY()) - 40.0f;
	if (State != nullptr && GetShooterGameState() != nullptr)
	{
		Canvas->DrawText(FString::Printf(TEXT("$ %d"), State->GetMoney()), 24.0f, Bottom - 36.0f, StatusColor);
	}
	if (Pawn == nullptr)
	{
		return;
	}
	FString Status = FString::Printf(TEXT("+ %d"), FMath::CeilToInt(Pawn->GetHealth()));
	if (Pawn->GetArmor() > 0.0f)
	{
		Status += FString::Printf(
			TEXT("   [] %d%s"), FMath::CeilToInt(Pawn->GetArmor()), Pawn->HasHelmet() ? TEXT(" H") : TEXT(""));
	}
	Canvas->DrawText(Status, 24.0f, Bottom, StatusColor);
	FString Right;
	if (const AShooterWeapon* Weapon = Pawn->GetWeapon())
	{
		Right = Weapon->AmmoPerClip > 1 || Weapon->MaxAmmo > 0
			? FString::Printf(
				  TEXT("%s  %d | %d"), *Weapon->WeaponName, Weapon->GetCurrentAmmoInClip(), Weapon->GetCurrentAmmo())
			: Weapon->WeaponName;
	}
	FString Items;
	if (Pawn->GetCarriedBomb() != nullptr)
	{
		Items += TEXT("C4 ");
	}
	if (Pawn->HasDefuseKit())
	{
		Items += TEXT("KIT ");
	}
	auto DrawRight = [this, Width](const FString& Line, float Y, const FLinearColor& Color)
	{
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		FCanvas::MeasureText(Line, HudFontScale, TextWidth, TextHeight);
		Canvas->DrawText(Line, Width - TextWidth - 24.0f, Y, Color);
	};
	if (!Right.IsEmpty())
	{
		DrawRight(Right, Bottom, StatusColor);
	}
	if (!Items.IsEmpty())
	{
		DrawRight(Items, Bottom - 36.0f, BombColor);
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
	FString Clock;
	FLinearColor ClockColor = StatusColor;
	if (State->GetBombState() == EShooterBombState::Planted)
	{
		Clock = TEXT("C4");
		ClockColor = BombColor;
	}
	else if (State->GetRoundState() == EShooterRoundState::Freeze || State->GetRoundState() == EShooterRoundState::Live)
	{
		Clock = FormatClock(State->GetPhaseTimeRemaining(Now));
	}
	if (!Clock.IsEmpty())
	{
		DrawCentredText(*Canvas, Clock, CenterX, 16.0f, ClockColor);
	}
	Canvas->DrawText(
		FString::Printf(TEXT("CT %d"), State->GetTeamScore(EShooterTeam::CT)), CenterX - 150.0f, 16.0f, CTColor);
	Canvas->DrawText(
		FString::Printf(TEXT("%d T"), State->GetTeamScore(EShooterTeam::T)), CenterX + 90.0f, 16.0f, TColor);
	DrawCentredText(*Canvas, FString::Printf(TEXT("Round %d"), State->GetRoundNumber()), CenterX, 44.0f,
		FLinearColor(0.7f, 0.7f, 0.7f));
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
	float Y = 16.0f;
	for (const FShooterKillFeedEntry& Entry : State->GetKillFeed())
	{
		if (Now - Entry.Time > KillFeedDuration)
		{
			continue;
		}
		const FString Middle =
			FString::Printf(TEXT(" [%s%s] "), *Entry.WeaponName, Entry.bHeadshot ? TEXT(" HS") : TEXT(""));
		float KillerWidth = 0.0f;
		float MiddleWidth = 0.0f;
		float VictimWidth = 0.0f;
		float Height = 0.0f;
		FCanvas::MeasureText(Entry.KillerName, HudFontScale, KillerWidth, Height);
		FCanvas::MeasureText(Middle, HudFontScale, MiddleWidth, Height);
		FCanvas::MeasureText(Entry.VictimName, HudFontScale, VictimWidth, Height);
		float X = Width - 24.0f - KillerWidth - MiddleWidth - VictimWidth;
		if (!Entry.KillerName.IsEmpty())
		{
			Canvas->DrawText(Entry.KillerName, X, Y, GetTeamColor(Entry.KillerTeam));
		}
		X += KillerWidth;
		Canvas->DrawText(Middle, X, Y, FColor::White);
		X += MiddleWidth;
		Canvas->DrawText(Entry.VictimName, X, Y, GetTeamColor(Entry.VictimTeam));
		Y += Height + 4.0f;
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
	FString Message;
	FLinearColor Color = MessageColor;
	switch (State->GetRoundState())
	{
		case EShooterRoundState::Warmup:
			Message = TEXT("Waiting for players on both teams");
			break;
		case EShooterRoundState::RoundEnd:
			Message = GetRoundEndMessage(State->GetLastRoundEndReason());
			Color = FLinearColor(GetTeamColor(GetRoundEndWinner(State->GetLastRoundEndReason())));
			break;
		case EShooterRoundState::MatchEnd:
			Message = FString::Printf(TEXT("%s  %d - %d"),
				State->GetMatchWinner() == EShooterTeam::CT      ? TEXT("Counter-Terrorists win the match")
					: State->GetMatchWinner() == EShooterTeam::T ? TEXT("Terrorists win the match")
																 : TEXT("The match is a draw"),
				State->GetTeamScore(EShooterTeam::CT), State->GetTeamScore(EShooterTeam::T));
			break;
		case EShooterRoundState::Freeze:
		case EShooterRoundState::Live:
			if (State->GetBombState() == EShooterBombState::Planted)
			{
				Message = FString::Printf(TEXT("The bomb has been planted at %s"), *State->GetBombSite().ToString());
				Color = BombColor;
			}
			break;
	}
	if (!Message.IsEmpty())
	{
		DrawCentredText(*Canvas, Message, CenterX, Y, Color);
	}
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
	DrawCentredText(*Canvas, Label, CenterX, Y - 32.0f, MessageColor);
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
	const float Inner = GetCrosshairGap() + 3.0f;
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
	const AShooterWeapon_Sniper* Sniper = Pawn != nullptr ? Cast<AShooterWeapon_Sniper>(Pawn->GetWeapon()) : nullptr;
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

float AShooterHUD::GetCrosshairGap() const
{
	const AShooterCharacter* Pawn = GetViewedPawn();
	const AShooterWeapon_Instant* Weapon = Pawn != nullptr ? Cast<AShooterWeapon_Instant>(Pawn->GetWeapon()) : nullptr;
	if (Weapon == nullptr || Canvas == nullptr)
	{
		return CrosshairGap;
	}
	// The spread's half angle on the screen: the view's vertical field of view spans the canvas's height.
	const float FieldOfView = FMath::Max(1.0f, Pawn->GetFirstPersonCameraComponent()->FieldOfView());
	const float HalfHeight = static_cast<float>(Canvas->GetSizeY()) * 0.5f;
	const float SpreadPixels = HalfHeight * FMath::Tan(FMath::DegreesToRadians(Weapon->GetCurrentSpread())) /
		FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	return CrosshairGap + (SpreadPixels * CrosshairSpreadScale);
}

void AShooterHUD::DrawCrosshair()
{
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float CenterY = static_cast<float>(Canvas->GetSizeY()) * 0.5f;
	const float Inner = GetCrosshairGap();
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
	FString CTLines;
	FString TLines;
	for (const APlayerState* State : GameMode->GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
		if (ShooterState == nullptr)
		{
			continue;
		}
		const AController* Controller = Cast<AController>(ShooterState->GetOwner());
		const AShooterCharacter* Pawn =
			Controller != nullptr ? Cast<AShooterCharacter>(Controller->GetPawn()) : nullptr;
		const bool bDead = Pawn == nullptr || !Pawn->IsAlive();
		FString& Lines = ShooterState->GetTeam() == EShooterTeam::T ? TLines : CTLines;
		Lines +=
			FString::Printf(TEXT("%-14s %3d %3d  $%-5d%s\n"), *ShooterState->GetPlayerName(), ShooterState->GetKills(),
				ShooterState->GetDeaths(), ShooterState->GetMoney(), bDead ? TEXT(" dead") : TEXT(""));
	}
	const float Width = static_cast<float>(Canvas->GetSizeX());
	const AShooterGameState* ShooterGameState = GetShooterGameState();
	const int32 ScoreCT = ShooterGameState != nullptr ? ShooterGameState->GetTeamScore(EShooterTeam::CT) : 0;
	const int32 ScoreT = ShooterGameState != nullptr ? ShooterGameState->GetTeamScore(EShooterTeam::T) : 0;
	Canvas->DrawTile(Width * 0.1f, 100.0f, Width * 0.8f, 340.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Canvas->DrawText(FString::Printf(TEXT("Counter-Terrorists  %d\nName            K   D  Money\n"), ScoreCT) + CTLines,
		Width * 0.12f, 120.0f, CTColor);
	Canvas->DrawText(FString::Printf(TEXT("Terrorists  %d\nName            K   D  Money\n"), ScoreT) + TLines,
		Width * 0.54f, 120.0f, TColor);
}

// The buy menu

UShooterBuyMenuWidget::UShooterBuyMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UShooterBuyMenuWidget::NativePaint(FPaintContext& Ctx)
{
	const AShooterHUD* HUD = Cast<AShooterHUD>(GetOwningHUD());
	const AShooterPlayerController* Controller = HUD != nullptr ? HUD->GetShooterPlayerController() : nullptr;
	if (Controller == nullptr || !Controller->IsBuyMenuOpen())
	{
		return;
	}
	const UWorld* World = HUD->GetWorld();
	const AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	const AShooterCharacter* Pawn = HUD->GetViewedPawn();
	const AShooterPlayerState* State = Controller->GetPlayerState<AShooterPlayerState>();
	const float X = 60.0f;
	float Y = 120.0f;
	const float LineHeight = HudLineHeight + 6.0f;
	const int32 NumItems = AShooterPlayerController::GetBuyMenuItems().Num();
	Ctx.DrawRect(X - 20.0f, Y - 20.0f, 420.0f, (LineHeight * static_cast<float>(NumItems + 4)) + 30.0f,
		FLinearColor(0.0f, 0.0f, 0.0f));
	FString Refusal;
	const bool bCanBuy = GameMode != nullptr && Pawn != nullptr && GameMode->CanBuy(*Pawn, &Refusal);
	Ctx.DrawText(FString::Printf(TEXT("Buy   $ %d"), State != nullptr ? State->GetMoney() : 0), X, Y,
		FLinearColor(1.0f, 0.75f, 0.2f));
	Y += LineHeight;
	if (!bCanBuy)
	{
		Ctx.DrawText(
			Refusal.IsEmpty() ? FString(TEXT("You cannot buy now")) : Refusal, X, Y, FLinearColor(1.0f, 0.3f, 0.2f));
	}
	Y += LineHeight;
	for (int32 Index = 0; Index < NumItems; ++Index)
	{
		const FString& Item = AShooterPlayerController::GetBuyMenuItems()[Index];
		const int32 Price = GameMode != nullptr && Pawn != nullptr ? GameMode->GetPrice(*Pawn, Item) : -1;
		const bool bAffordable = bCanBuy && Price >= 0 && State != nullptr && State->GetMoney() >= Price;
		const FString Line = Price >= 0 ? FString::Printf(TEXT("%d  %-10s $%d"), Index + 1, *Item, Price)
										: FString::Printf(TEXT("%d  %-10s  -"), Index + 1, *Item);
		Ctx.DrawText(Line, X, Y, bAffordable ? FLinearColor(1.0f, 1.0f, 1.0f) : FLinearColor(0.45f, 0.45f, 0.45f));
		Y += LineHeight;
	}
	if (!Controller->GetLastBuyMessage().IsEmpty())
	{
		Ctx.DrawText(Controller->GetLastBuyMessage(), X, Y + 6.0f, FLinearColor(0.7f, 0.9f, 0.7f));
	}
}
