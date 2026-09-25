#include "GameFramework/PlayerStart.h"

APlayerStart::APlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(AActor::DefaultSceneRootName))
{
	bCanEverTick = false;
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	// UE's player start capsule (radius 40 cm, half height 92 cm).
	CapsuleComponent->InitCapsuleSize(40.0f, 92.0f);
	RootComponent = CapsuleComponent;
}
