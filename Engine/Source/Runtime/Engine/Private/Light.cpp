#include "Engine/Light.h"

ALight::ALight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(AActor::DefaultSceneRootName))
{
	bCanEverTick = false;
}
