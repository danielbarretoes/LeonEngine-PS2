#include "ShooterPlayerState.h"

AShooterPlayerState::AShooterPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AShooterPlayerState::SetMoney(int32 NewMoney, int32 MaxMoney)
{
	Money = FMath::Clamp(NewMoney, 0, FMath::Max(0, MaxMoney));
}

int32 AShooterPlayerState::AddMoney(int32 Amount, int32 MaxMoney)
{
	const int32 Before = Money;
	SetMoney(Money + Amount, MaxMoney);
	return Money - Before;
}

void AShooterPlayerState::ScoreKill(int32 Points)
{
	NumKills += Points;
	AddScore(static_cast<float>(Points));
}

void AShooterPlayerState::ScoreDeath()
{
	++NumDeaths;
}

void AShooterPlayerState::ResetStats()
{
	NumKills = 0;
	NumDeaths = 0;
	SetScore(0.0f);
}
