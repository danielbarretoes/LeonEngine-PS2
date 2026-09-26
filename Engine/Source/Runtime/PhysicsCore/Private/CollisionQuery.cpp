#include "CollisionQuery.h"

const FCollisionResponseParams FCollisionResponseParams::DefaultResponseParam(ECR_Block);

FCollisionResponseParams::FCollisionResponseParams()
	: CollisionResponse(FCollisionResponseContainer::GetDefaultResponseContainer())
{
}

bool FCollisionQueryParams::IsIgnored(SIZE_T ComponentID, SIZE_T ActorID) const
{
	if (ComponentID == IgnoreComponentID || IgnoreComponents.Contains(ComponentID))
	{
		return true;
	}
	return ActorID != NoComponentID && IgnoreActors.Contains(ActorID);
}
