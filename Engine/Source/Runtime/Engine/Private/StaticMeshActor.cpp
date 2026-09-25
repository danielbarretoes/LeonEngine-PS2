#include "Engine/StaticMeshActor.h"

AStaticMeshActor::AStaticMeshActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(AActor::DefaultSceneRootName))
{
	// Placed geometry does not tick (UE: PrimaryActorTick.bCanEverTick = false).
	bCanEverTick = false;
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent0"));
	RootComponent = StaticMeshComponent;
}
