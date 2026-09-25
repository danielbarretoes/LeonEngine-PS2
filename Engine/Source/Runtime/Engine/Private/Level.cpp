#include "Engine/Level.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

void ULevel::PostLoad()
{
	Super::PostLoad();
	OwningWorld = Cast<UWorld>(GetOuter());
	Actors.RemoveAll([](const AActor* Actor) { return Actor == nullptr; });
}
