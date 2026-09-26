#include "ShooterHUD.h"

#include "CanvasTypes.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "ShooterCharacter.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_Sniper.h"

AShooterHUD::AShooterHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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
	const AShooterPlayerController* Controller = Cast<AShooterPlayerController>(PlayerOwner);
	if (Controller != nullptr && Controller->IsScoreboardShown())
	{
		DrawScoreboard();
	}
}

void AShooterHUD::DrawStatus()
{
	const AShooterCharacter* Pawn = PlayerOwner != nullptr ? Cast<AShooterCharacter>(PlayerOwner->GetPawn()) : nullptr;
	if (Pawn == nullptr)
	{
		return;
	}
	const float Width = static_cast<float>(Canvas->GetSizeX());
	const float Bottom = static_cast<float>(Canvas->GetSizeY()) - 40.0f;
	FString Status = FString::Printf(TEXT("+ %d"), FMath::CeilToInt(Pawn->GetHealth()));
	if (Pawn->GetArmor() > 0.0f)
	{
		Status += FString::Printf(
			TEXT("   [] %d%s"), FMath::CeilToInt(Pawn->GetArmor()), Pawn->HasHelmet() ? TEXT(" H") : TEXT(""));
	}
	Canvas->DrawText(Status, 24.0f, Bottom, StatusColor);
	if (const AShooterWeapon* Weapon = Pawn->GetWeapon())
	{
		const FString Ammo = Weapon->AmmoPerClip > 1 || Weapon->MaxAmmo > 0
			? FString::Printf(
				  TEXT("%s  %d | %d"), *Weapon->WeaponName, Weapon->GetCurrentAmmoInClip(), Weapon->GetCurrentAmmo())
			: Weapon->WeaponName;
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		FCanvas::MeasureText(Ammo, HudFontScale, TextWidth, TextHeight);
		Canvas->DrawText(Ammo, Width - TextWidth - 24.0f, Bottom, StatusColor);
	}
}

void AShooterHUD::DrawHitMarker()
{
	const AShooterPlayerController* Controller = Cast<AShooterPlayerController>(PlayerOwner);
	const UWorld* World = GetWorld();
	if (Controller == nullptr || World == nullptr || Controller->GetLastHitTime() < 0.0f ||
		World->GetTimeSeconds() - Controller->GetLastHitTime() > HitMarkerDuration)
	{
		return;
	}
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float CenterY = static_cast<float>(Canvas->GetSizeY()) * 0.5f;
	const FLinearColor Color = Controller->WasLastHitKill() ? FLinearColor(1.0f, 0.15f, 0.1f) : FLinearColor::White;
	const float Inner = CrosshairGap + 3.0f;
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
	const AShooterCharacter* Pawn = PlayerOwner != nullptr ? Cast<AShooterCharacter>(PlayerOwner->GetPawn()) : nullptr;
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

void AShooterHUD::DrawCrosshair()
{
	const float CenterX = static_cast<float>(Canvas->GetSizeX()) * 0.5f;
	const float CenterY = static_cast<float>(Canvas->GetSizeY()) * 0.5f;
	const float Inner = CrosshairGap;
	const float Outer = CrosshairGap + CrosshairLength;
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
	FString CTNames;
	FString TNames;
	for (const APlayerState* State : GameMode->GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
		if (ShooterState == nullptr)
		{
			continue;
		}
		FString& Names = ShooterState->GetTeam() == EShooterTeam::T ? TNames : CTNames;
		Names += ShooterState->GetPlayerName() + TEXT("\n");
	}
	const float Width = static_cast<float>(Canvas->GetSizeX());
	const FColor CTColor(110, 160, 255);
	const FColor TColor(255, 190, 90);
	Canvas->DrawText(FString(TEXT("Counter-Terrorists\n")) + CTNames, Width * 0.3f, 120.0f, CTColor);
	Canvas->DrawText(FString(TEXT("Terrorists\n")) + TNames, Width * 0.6f, 120.0f, TColor);
}
