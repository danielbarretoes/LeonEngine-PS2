#include "Engine/GameInstance.h"

#include "Engine/World.h"

void FWorldContext::SetCurrentWorld(UWorld* World)
{
	ThisCurrentWorld = World;
	if (World != nullptr)
	{
		World->OwningGameInstance = OwningGameInstance;
	}
}

void UGameInstance::Init()
{
}

void UGameInstance::Shutdown()
{
}

void UGameInstance::InitializeStandalone(FName InPackageName, UPackage* InWorldPackage)
{
	WorldContext.WorldType = EWorldType::Game;
	WorldContext.OwningGameInstance = this;
	WorldContext.ContextHandle = FName(TEXT("Context_0"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, InPackageName, InWorldPackage);
	WorldContext.SetCurrentWorld(World);
}

void UGameInstance::DestroyWorldContextWorld()
{
	UWorld* World = WorldContext.World();
	if (World == nullptr)
	{
		return;
	}
	WorldContext.SetCurrentWorld(nullptr);
	World->DestroyWorld(true);
}
