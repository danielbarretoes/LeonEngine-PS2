#include "PhysicalMaterials/PhysicalMaterial.h"

UPhysicalMaterial::UPhysicalMaterial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

EPhysicalSurface UPhysicalMaterial::DetermineSurfaceType(const UPhysicalMaterial* PhysicalMaterial)
{
	return PhysicalMaterial != nullptr ? PhysicalMaterial->SurfaceType.GetValue() : SurfaceType_Default;
}
