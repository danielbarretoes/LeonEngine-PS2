#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DamageType.generated.h"

/**
 * What kind of damage a hit does (UE: UDamageType). A class, never instanced: the damage events carry the class
 * (FDamageEvent::DamageTypeClass) and the receivers read its class default object. A game makes a subclass per kind
 * of damage (a bullet, an explosion, a fall) and sets the defaults in its constructor.
 *
 * Leon keeps UE's gameplay fields; the destructible ones (DestructibleImpulse, DestructibleDamageSpreadScale) wait for
 * destructible meshes, and the impulse fields for simulated bodies that take impulses.
 */
UCLASS()
class ENGINE_API UDamageType : public UObject
{
	GENERATED_BODY()

public:
	UDamageType(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The damage comes from the world (a fall, a pain volume), not from another player (UE: bCausedByWorld). */
	UPROPERTY()
	uint8 bCausedByWorld : 1;

	/** The impulse is scaled by the mass of the body it pushes (UE: bScaleMomentumByMass). */
	UPROPERTY()
	uint8 bScaleMomentumByMass : 1;

	/** A radial impulse is a velocity change, ignoring the mass (UE: bRadialDamageVelChange). */
	UPROPERTY()
	uint8 bRadialDamageVelChange : 1;

	/** The impulse a hit gives a simulated body, along the shot (UE: DamageImpulse). */
	UPROPERTY()
	float DamageImpulse = 800.0f;

	/** The impulse a destructible takes (UE: DestructibleImpulse). */
	UPROPERTY()
	float DestructibleImpulse = 800.0f;

	/** How far a destructible's damage spreads (UE: DestructibleDamageSpreadScale). */
	UPROPERTY()
	float DestructibleDamageSpreadScale = 1.0f;

	/** The exponent of a radial falloff between the inner and outer radius (UE: DamageFalloff). */
	UPROPERTY()
	float DamageFalloff = 1.0f;
};
