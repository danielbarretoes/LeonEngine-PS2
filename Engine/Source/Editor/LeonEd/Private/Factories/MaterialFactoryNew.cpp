#include "Factories/MaterialFactoryNew.h"

#include "Materials/Material.h"

UMaterialFactoryNew::UMaterialFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMaterial::StaticClass();
	bCreateNew = 1;
}

UObject* UMaterialFactoryNew::FactoryCreateNew(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context)
{
	(void)Context;
	UClass* Class =
		InClass != nullptr && InClass->IsChildOf(UMaterial::StaticClass()) ? InClass : UMaterial::StaticClass();
	return CreateOrOverwriteAsset(Class, InParent, InName, Flags);
}
