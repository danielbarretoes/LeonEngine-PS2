#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "ResavePackagesCommandlet.generated.h"

/**
 * Loads packages and saves them again in the current format (UE: UResavePackagesCommandlet): after a change to a
 * class's properties or to the package version, `LeonCook [<Project>.lproj] -run=ResavePackages` brings the content up
 * to date. `-package=<LongPackageName>[,...]` or `-packagefolder=<LongPackagePath>` choose the packages; by default
 * every package under the mount points (`/Engine`, and `/Game` with a project). Returns 0 when every package saved.
 */
UCLASS()
class LEONED_API UResavePackagesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UResavePackagesCommandlet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 Main(const FString& Params) override;

	/**
	 * The packages a content-wide commandlet processes: `-package=<Name>[,...]`, else every package under
	 * `-packagefolder=<Path>`, else every package under the mount points (Leon: shared by ResavePackages,
	 * ValidateAssets and Cook).
	 */
	static void GatherPackages(const TMap<FString, FString>& ParamsMap, TArray<FString>& OutPackageNames);
};
