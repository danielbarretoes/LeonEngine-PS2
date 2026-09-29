#include "Weapons/ShooterSmokeCloud.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "ShooterGameMode.h"

namespace
{

	/** The golden angle, radians: the puffs turn by it around the cloud's axis, so none hides another. */
	constexpr float PuffGoldenAngle = 2.39996323f;

	/** How far out from the axis and how high the puffs sit, fractions of the radius. */
	constexpr float PuffSpread = 0.55f;
	constexpr float PuffHeightSpread = 0.35f;

	AShooterGameMode* GetShooterGameMode(const UWorld* World)
	{
		return World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	}

} // namespace

AShooterSmokeCloud::AShooterSmokeCloud(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector AShooterSmokeCloud::GetCloudCenter() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, CenterHeight);
}

float AShooterSmokeCloud::GetAge() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() - SpawnTime : 0.0f;
}

bool AShooterSmokeCloud::IsThick() const
{
	return GetAge() < Duration - (FadeOutTime * 0.5f);
}

bool AShooterSmokeCloud::BlocksLine(const FVector& Start, const FVector& End) const
{
	if (!IsThick() || IsPendingKillPending())
	{
		return false;
	}
	// The segment's point nearest the centre is inside the sphere.
	const FVector Center = GetCloudCenter();
	const FVector Along = End - Start;
	const float LengthSquared = Along.SizeSquared();
	const float T = LengthSquared > 0.0f ? FMath::Clamp(((Center - Start) | Along) / LengthSquared, 0.0f, 1.0f) : 0.0f;
	return FVector::DistSquared(Start + (Along * T), Center) <= FMath::Square(Radius);
}

void AShooterSmokeCloud::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	SpawnTime = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	if (AShooterGameMode* GameMode = GetShooterGameMode(World))
	{
		GameMode->RegisterSmokeCloud(this);
	}
	// The puffs: on a golden-angle spiral around the axis, alternately low and high, the same every time.
	const FVector Center = GetCloudCenter();
	const int32 Count = FMath::Max(1, NumPuffs);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Out =
			Radius * PuffSpread * FMath::Sqrt((static_cast<float>(Index) + 0.5f) / static_cast<float>(Count));
		const float Angle = static_cast<float>(Index) * PuffGoldenAngle;
		const float Up = Radius * PuffHeightSpread * ((Index % 2) == 0 ? -0.5f : 0.5f);
		const FVector Place = Center + FVector(Out * FMath::Cos(Angle), Out * FMath::Sin(Angle), Up);
		PuffSerials.Add(
			UGameplayStatics::SpawnEffectSprite(this, Place, PuffSize, PuffColor, Duration, FadeInTime, FadeOutTime));
	}
	SetLifeSpan(Duration);
}

void AShooterSmokeCloud::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (AShooterGameMode* GameMode = GetShooterGameMode(World))
	{
		GameMode->UnregisterSmokeCloud(this);
	}
	if (World != nullptr)
	{
		for (const uint32 Serial : PuffSerials)
		{
			World->EffectSprites.RemoveSprite(Serial);
		}
	}
	PuffSerials.Reset();
	Super::EndPlay(EndPlayReason);
}
