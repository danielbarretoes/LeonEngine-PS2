#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "ValidateAssetsCommandlet.generated.h"

/**
 * Checks that packages load cleanly (Leon; UE's data validation runs in the editor): `LeonCook [<Project>.lproj]
 * -run=ValidateAssets` reads each package's tables and loads it, then checks that every import resolves (its package
 * exists, the object is in it) and that every export was made with a valid, non-abstract class. `-package=` /
 * `-packagefolder=` choose the packages, as for ResavePackages; by default every package under the mount points.
 * Returns 0 when every package is valid, 1 otherwise (each problem is logged as an error).
 */
UCLASS()
class LEONED_API UValidateAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UValidateAssetsCommandlet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 Main(const FString& Params) override;

	/** Validates one package (a long package name); the number of problems found. */
	static int32 ValidatePackage(const FString& PackageName);
};
