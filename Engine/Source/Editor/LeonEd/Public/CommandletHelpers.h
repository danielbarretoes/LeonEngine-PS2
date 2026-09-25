#pragma once

#include "CoreMinimal.h"

class UClass;

/** How LeonCook finds and describes commandlets (Leon; UE does this in FEngineLoop::PreInit). */
namespace CommandletHelpers
{
	/**
	 * The concrete UCommandlet class `-run=<Name>` names: U<Name> or U<Name>Commandlet, found through reflection
	 * (`ImportAssets` is UImportAssetsCommandlet); null when none.
	 */
	[[nodiscard]] LEONED_API UClass* FindCommandletClass(const FString& Name);

	/** Every concrete commandlet class, sorted by name. */
	LEONED_API void GetCommandletClasses(TArray<UClass*>& OutClasses);
} // namespace CommandletHelpers
