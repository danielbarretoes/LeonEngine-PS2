#include "GameFramework/Info.h"

AInfo::AInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidden = true;
	// Information actors do not tick in the world (UE: AInfo's PrimaryActorTick.bCanEverTick is false); whoever
	// owns one ticks it when needed (the game mode ticks its game state).
	bCanEverTick = false;
}
