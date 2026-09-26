#pragma once

#include "CoreMinimal.h"
#include "Weapons/ShooterWeapon_Instant.h"
#include "ShooterWeapon_Sniper.generated.h"

/**
 * An AWP-like sniper rifle (CS: the AWP): one heavy shot every 1.45 s, a scope and poor accuracy without it.
 *
 * - The secondary button (Targeting, the right mouse button) steps through the zoom levels: ZoomFOVs (the first-person
 *   camera's vertical field of view: CS's 40 and 10 degrees wide at 4:3) and back to no zoom. The HUD draws the scope
 *   (AShooterHUD) and hides the view model while zoomed.
 * - Unscoped, UnscopedSpread is added to the spread; moving adds MovingSpread as for every hitscan weapon (the CS
 *   penalty); scoped, the carrier walks at ScopedSpeedModifier of the running speed.
 * - A shot leaves the scope for the bolt (TimeBetweenShots), which returns to the zoom it had (CS's resume zoom).
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_Sniper : public AShooterWeapon_Instant
{
	GENERATED_BODY()

public:
	AShooterWeapon_Sniper(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The zoom levels' vertical fields of view, degrees (`+ZoomFOVs=` in the config). */
	UPROPERTY(Config)
	TArray<float> ZoomFOVs;

	/** Added to the spread without the scope, degrees. */
	UPROPERTY(Config)
	float UnscopedSpread = 6.0f;

	/** The carrier's speed scoped, a fraction of the running speed (CS: 150 units a second). */
	UPROPERTY(Config)
	float ScopedSpeedModifier = 0.6f;

	/** The next zoom level, or none after the last (the secondary button). */
	void StartSecondaryFire() override;

	/** 0: no zoom, 1..ZoomFOVs.Num(): a zoom level. */
	void SetZoomLevel(int32 NewZoomLevel);
	[[nodiscard]] int32 GetZoomLevel() const
	{
		return ZoomLevel;
	}
	[[nodiscard]] bool IsZoomed() const
	{
		return ZoomLevel > 0;
	}

	[[nodiscard]] float GetCurrentSpread() const override;
	[[nodiscard]] float GetSpeedModifier() const override;

	void OnUnEquip() override;
	void Tick(float DeltaSeconds) override;

protected:
	/** The bolt leaves the scope; it returns after TimeBetweenShots. */
	void OnShotFired() override;

private:
	int32 ZoomLevel = 0;
	/** The zoom to return to after the bolt, and when. */
	int32 ResumeZoomLevel = 0;
	float ResumeZoomTime = 0.0f;
};
