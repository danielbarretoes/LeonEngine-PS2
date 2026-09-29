#include "Factories/PhysicalMaterialFactoryNew.h"

#include "LeonEdLog.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"

UPhysicalMaterialFactoryNew::UPhysicalMaterialFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPhysicalMaterial::StaticClass();
	bCreateNew = 1;
}

UObject* UPhysicalMaterialFactoryNew::FactoryCreateNew(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context)
{
	(void)Context;
	EPhysicalSurface Surface = SurfaceType_Default;
	if (!UPhysicsSettings::Get()->FindSurfaceType(SurfaceType, Surface))
	{
		UE_LOG(LogLeonEd, Error,
			"PhysicalMaterialFactoryNew: %s: '%s' is no surface type ([/Script/Engine.PhysicsSettings] "
			"PhysicalSurfaces names them)",
			*InName.ToString(), *SurfaceType);
		return nullptr;
	}
	UClass* Class = InClass != nullptr && InClass->IsChildOf(UPhysicalMaterial::StaticClass())
		? InClass
		: UPhysicalMaterial::StaticClass();
	UPhysicalMaterial* Material = Cast<UPhysicalMaterial>(CreateOrOverwriteAsset(Class, InParent, InName, Flags));
	if (Material != nullptr)
	{
		Material->SurfaceType = Surface;
	}
	return Material;
}
