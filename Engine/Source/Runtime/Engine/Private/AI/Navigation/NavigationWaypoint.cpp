#include "AI/Navigation/NavigationWaypoint.h"

ANavigationWaypoint::ANavigationWaypoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A graph point does not tick.
	bCanEverTick = false;
}
