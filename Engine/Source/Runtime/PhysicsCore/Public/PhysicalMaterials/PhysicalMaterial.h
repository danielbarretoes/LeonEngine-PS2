#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PhysicalMaterial.generated.h"

/**
 * What a surface is made of (UE: EPhysicalSurface, PhysicsCore's Chaos/ChaosEngineInterface.h there): the default and
 * 62 types a game names in the Engine config (`[/Script/Engine.PhysicsSettings] +PhysicalSurfaces=(Type=SurfaceType1,
 * Name=Concrete)`, UPhysicsSettings), the way the game channels are named. A physical material has one; the game
 * decides what it means (the sound of a step, a bullet's penetration).
 */
UENUM()
enum EPhysicalSurface
{
	SurfaceType_Default UMETA(DisplayName = "Default"),
	SurfaceType1,
	SurfaceType2,
	SurfaceType3,
	SurfaceType4,
	SurfaceType5,
	SurfaceType6,
	SurfaceType7,
	SurfaceType8,
	SurfaceType9,
	SurfaceType10,
	SurfaceType11,
	SurfaceType12,
	SurfaceType13,
	SurfaceType14,
	SurfaceType15,
	SurfaceType16,
	SurfaceType17,
	SurfaceType18,
	SurfaceType19,
	SurfaceType20,
	SurfaceType21,
	SurfaceType22,
	SurfaceType23,
	SurfaceType24,
	SurfaceType25,
	SurfaceType26,
	SurfaceType27,
	SurfaceType28,
	SurfaceType29,
	SurfaceType30,
	SurfaceType31,
	SurfaceType32,
	SurfaceType33,
	SurfaceType34,
	SurfaceType35,
	SurfaceType36,
	SurfaceType37,
	SurfaceType38,
	SurfaceType39,
	SurfaceType40,
	SurfaceType41,
	SurfaceType42,
	SurfaceType43,
	SurfaceType44,
	SurfaceType45,
	SurfaceType46,
	SurfaceType47,
	SurfaceType48,
	SurfaceType49,
	SurfaceType50,
	SurfaceType51,
	SurfaceType52,
	SurfaceType53,
	SurfaceType54,
	SurfaceType55,
	SurfaceType56,
	SurfaceType57,
	SurfaceType58,
	SurfaceType59,
	SurfaceType60,
	SurfaceType61,
	SurfaceType62,
	SurfaceType_Max UMETA(Hidden),
};

/**
 * A physical material asset (UE: UPhysicalMaterial, `PM_`): what a surface is made of, for the game's queries. A
 * material names it (UMaterial::PhysMaterial), and a trace that asks for it (FCollisionQueryParams::
 * bReturnPhysicalMaterial) reports the one of the surface it hit (FHitResult::PhysMaterial). Leon's physics has no
 * friction or restitution to take from it: it carries the surface type only.
 */
UCLASS()
class PHYSICSCORE_API UPhysicalMaterial : public UObject
{
	GENERATED_BODY()

public:
	UPhysicalMaterial(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The surface it is (UE: SurfaceType), named in the Engine config. */
	UPROPERTY()
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	/**
	 * The surface of a physical material, SurfaceType_Default for none (UE: DetermineSurfaceType, which falls back to
	 * the engine's default physical material, whose surface is the default).
	 */
	[[nodiscard]] static EPhysicalSurface DetermineSurfaceType(const UPhysicalMaterial* PhysicalMaterial);
};
