#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "PhysicalMaterialFactoryNew.generated.h"

/**
 * Makes a physical material (UE: UPhysicalMaterialFactoryNew, the editor's New Physical Material) from a description:
 * an ImportList.ini section (UImportAssetsCommandlet, Docs/TOOLS.md) whose keys are these settings:
 *
 *     [PM_Wood]
 *     Type=PhysicalMaterial
 *     Dest=/Game/PhysicalMaterials
 *     SurfaceType=Wood
 *
 * SurfaceType is a name the Engine config gives a surface type (`[/Script/Engine.PhysicsSettings]
 * +PhysicalSurfaces=(Type=SurfaceType4,Name=Wood)`), an enumerator (SurfaceType4) or Default.
 */
UCLASS()
class LEONED_API UPhysicalMaterialFactoryNew : public UFactory
{
	GENERATED_BODY()

public:
	UPhysicalMaterialFactoryNew(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The surface type (UPhysicalMaterial::SurfaceType), by its configured name or its enumerator. */
	UPROPERTY()
	FString SurfaceType = TEXT("Default");

	UObject* FactoryCreateNew(
		UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context) override;
};
