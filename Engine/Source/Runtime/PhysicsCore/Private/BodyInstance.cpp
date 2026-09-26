#include "BodyInstance.h"

void FBodyInstance::SetDefaultCollision(EBodyType InType)
{
	CollisionResponses = FCollisionResponseContainer::GetDefaultResponseContainer();
	if (InType == EBodyType::Static)
	{
		ObjectType = ECC_WorldStatic;
		(void)CollisionResponses.SetResponse(ECC_WorldDynamic, ECR_Ignore);
	}
	else
	{
		ObjectType = ECC_WorldDynamic;
		(void)CollisionResponses.SetResponse(ECC_WorldStatic, ECR_Ignore);
	}
}
