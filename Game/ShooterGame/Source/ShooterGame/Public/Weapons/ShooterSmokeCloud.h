#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterSmokeCloud.generated.h"

/**
 * A smoke grenade's cloud (Counter-Strike 1.6's smoke): a sphere of Radius centred CenterHeight above where the grenade
 * went off, for Duration seconds (CS 1.6: about 18). While it is thick (IsThick: until half of its last FadeOutTime
 * seconds are gone) a line through it is blocked for the bots' sight (BlocksLine, through
 * AShooterGameMode::IsSightBlockedBySmoke and UShooterPawnSensingComponent). It is drawn cheaply for the GS: NumPuffs
 * soft grey puffs of PuffSize cm facing the camera (the world's effect sprites, two alpha-blended triangles each with
 * the effects' mask), spread over the sphere, thickening in FadeInTime and thinning out in FadeOutTime. The game mode
 * keeps the clouds; the round's clean-up removes them, and a cloud takes its puffs with it.
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterSmokeCloud : public AActor
{
	GENERATED_BODY()

public:
	AShooterSmokeCloud(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The cloud's sphere, cm (CS: about 128 units), and its centre's height over the grenade. */
	UPROPERTY(Config)
	float Radius = 325.0f;

	UPROPERTY(Config)
	float CenterHeight = 150.0f;

	/** Seconds the cloud lasts, thickens and thins out. */
	UPROPERTY(Config)
	float Duration = 18.0f;

	UPROPERTY(Config)
	float FadeInTime = 1.0f;

	UPROPERTY(Config)
	float FadeOutTime = 3.0f;

	/** The puffs drawn: how many, their side (cm) and colour (A: the thickest opacity). */
	UPROPERTY(Config)
	int32 NumPuffs = 6;

	UPROPERTY(Config)
	float PuffSize = 440.0f;

	UPROPERTY(Config)
	FLinearColor PuffColor = FLinearColor(0.62f, 0.62f, 0.6f, 0.9f);

	/** The sphere's centre in the world. */
	[[nodiscard]] FVector GetCloudCenter() const;
	/** Seconds since the cloud appeared. */
	[[nodiscard]] float GetAge() const;
	/** Thick enough to hide what is behind it (see the class comment). */
	[[nodiscard]] bool IsThick() const;
	/** The segment [Start, End] crosses the thick cloud's sphere. */
	[[nodiscard]] bool BlocksLine(const FVector& Start, const FVector& End) const;
	/** The world's effect sprites the cloud shows (their serials). */
	[[nodiscard]] int32 GetNumPuffs() const
	{
		return PuffSerials.Num();
	}

	/** Joins the game mode's clouds, adds its puffs and lives Duration seconds. */
	void BeginPlay() override;
	/** Leaves the game mode's clouds and takes its puffs away. */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	float SpawnTime = 0.0f;
	/** The puffs' sprites in the world's pool. */
	TArray<uint32, TInlineAllocator<8>> PuffSerials;
};
