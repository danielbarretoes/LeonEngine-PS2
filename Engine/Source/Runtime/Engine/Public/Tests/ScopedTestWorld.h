#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

	#include "Engine/World.h"
	#include "UObject/UObjectGlobals.h"

/**
 * A game world for the length of a scope, for automation tests (UE tests call UWorld::CreateWorld and DestroyWorld the
 * same way). The world is created in the root set; at the end of the scope it ends play on every actor, is destroyed
 * and the garbage is collected (a world teardown is a safe point, plan decision D11), so the test leaves no objects
 * behind. Objects the test created with NewObject and still uses after the scope must be declared before it or be held
 * through TStrongObjectPtr.
 *
 *     FScopedTestWorld TestWorld;
 *     UWorld& World = *TestWorld;
 *     ACharacter* Character = World.SpawnActor<ACharacter>();
 */
class FScopedTestWorld
{
public:
	FScopedTestWorld()
		: World(UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld =*/false))
	{
	}

	~FScopedTestWorld()
	{
		World->DestroyWorld(/*bInformEngineOfWorld =*/false);
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	FScopedTestWorld(const FScopedTestWorld&) = delete;
	FScopedTestWorld& operator=(const FScopedTestWorld&) = delete;
	FScopedTestWorld(FScopedTestWorld&&) = delete;
	FScopedTestWorld& operator=(FScopedTestWorld&&) = delete;

	[[nodiscard]] UWorld& operator*() const
	{
		return *World;
	}
	[[nodiscard]] UWorld* operator->() const
	{
		return World;
	}
	[[nodiscard]] UWorld* Get() const
	{
		return World;
	}

private:
	UWorld* World;
};

#endif // WITH_DEV_AUTOMATION_TESTS
