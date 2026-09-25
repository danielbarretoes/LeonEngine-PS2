#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "MaterialFactoryNew.generated.h"

/**
 * Makes a new UMaterial (UE: UMaterialFactoryNew): the default parameters, which draw like an unset material slot.
 * Materials are authored as assets (they have no source file): the commandlets and the tests make them with it and set
 * their properties.
 */
UCLASS()
class LEONED_API UMaterialFactoryNew : public UFactory
{
	GENERATED_BODY()

public:
	UMaterialFactoryNew(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UObject* FactoryCreateNew(
		UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context) override;
};
