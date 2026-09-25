#include "Engine/BlockingVolume.h"

ABlockingVolume::ABlockingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}
