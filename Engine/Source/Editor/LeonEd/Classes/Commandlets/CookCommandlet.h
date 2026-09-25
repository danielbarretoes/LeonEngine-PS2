#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "CookCommandlet.generated.h"

/**
 * Cooks content (UE: UCookCommandlet, `-run=Cook`), moved from the Developer Cooker module into LeonEd. Before P16 the
 * cook is minimal: `LeonCook [<Project>.lproj] -run=Cook [-TargetPlatform=<Name>]` loads every package under the mount
 * points (or `-package=` / `-packagefolder=`) and saves it with PKG_FilterEditorOnly | PKG_Cooked, which drops the
 * editor-only data (the assets' import data), into `<Project>/Saved/Cooked/<Platform>/<Engine|Project>/Content/...`
 * (UE's layout). The platform (default Win64) only names the folder: P16 brings the dependency closure (imports and
 * soft references), the target platforms (Win64 identity, PS2), `DirectoriesToAlwaysCook`, the staging of config and
 * shaders, and `.lpak` files. Returns 0 when every package cooked.
 */
UCLASS()
class LEONED_API UCookCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCookCommandlet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 Main(const FString& Params) override;

	/** The cooked file of a package for Platform (above); empty for a package under no mount point. */
	static FString GetCookedFilename(const FString& PackageName, const FString& Platform);
};
