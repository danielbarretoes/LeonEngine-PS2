#include "Components/LightComponentBase.h"

ULightComponentBase::ULightComponentBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadows = true;
}
