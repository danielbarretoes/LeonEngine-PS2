#pragma once

#include "CoreMinimal.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/Object.h"
#include "PhysicsSettings.generated.h"

/** A surface type the config names (UE: FPhysicalSurfaceName): `+PhysicalSurfaces=(Type=SurfaceType1,Name=Concrete)`.
 */
USTRUCT()
struct ENGINE_API FPhysicalSurfaceName
{
	GENERATED_BODY()

	/** The surface type (UE: Type), SurfaceType1 to SurfaceType62. */
	UPROPERTY()
	TEnumAsByte<EPhysicalSurface> Type = SurfaceType1;

	/** Its name (UE: Name), e.g. Concrete. */
	UPROPERTY()
	FName Name;
};

/**
 * The project's physics settings (UE: UPhysicsSettings), read from [/Script/Engine.PhysicsSettings] of the Engine
 * config. Leon reads only the names of the game's surface types (UE's gravity, solver and friction settings have no
 * counterpart in Leon's physics).
 */
UCLASS(Config = Engine, DefaultConfig)
class ENGINE_API UPhysicsSettings : public UObject
{
	GENERATED_BODY()

public:
	UPhysicsSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The named surface types (UE: PhysicalSurfaces). */
	UPROPERTY(Config)
	TArray<FPhysicalSurfaceName> PhysicalSurfaces;

	/** The settings (UE: Get): the class default object. */
	[[nodiscard]] static UPhysicsSettings* Get();

	/**
	 * A surface type's name: Default for SurfaceType_Default, the config's for a named one, else None (Leon; UE sets
	 * the names as the enum's display names, LoadSurfaceType).
	 */
	[[nodiscard]] FName GetSurfaceName(EPhysicalSurface Surface) const;

	/**
	 * The surface type of a name: a configured name, Default, or an enumerator (SurfaceType4); false when it is none
	 * of them.
	 */
	[[nodiscard]] bool FindSurfaceType(const FString& Name, EPhysicalSurface& OutSurface) const;
};
