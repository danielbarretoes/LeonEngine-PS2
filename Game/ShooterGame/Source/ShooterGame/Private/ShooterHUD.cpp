#include "ShooterHUD.h"

#include "CanvasTypes.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"

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
	DrawCrosshair();
	const AShooterPlayerController* Controller = Cast<AShooterPlayerController>(PlayerOwner);
	if (Controller != nullptr && Controller->IsScoreboardShown())
	{
		DrawScoreboard();
	}
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
