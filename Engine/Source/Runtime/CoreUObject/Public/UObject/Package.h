#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

/**
 * The outermost object of every object: a /Script/<Module> package of compiled-in types, the transient package, or
 * (P11) a .lasset / .lmap package loaded from disk (UE: UPackage).
 */
class COREUOBJECT_API UPackage : public UObject
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(
		UPackage, UObject, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UPackage, COREUOBJECT_API)

public:
	explicit UPackage(const FObjectInitializer& ObjectInitializer);

	FORCEINLINE void SetPackageFlagsTo(uint32 NewFlags)
	{
		PackageFlagsPrivate = NewFlags;
	}

	FORCEINLINE void SetPackageFlags(uint32 NewFlags)
	{
		PackageFlagsPrivate |= NewFlags;
	}

	FORCEINLINE void ClearPackageFlags(uint32 NewFlags)
	{
		PackageFlagsPrivate &= ~NewFlags;
	}

	FORCEINLINE bool HasAnyPackageFlags(uint32 FlagsToCheck) const
	{
		return (PackageFlagsPrivate & FlagsToCheck) != 0;
	}

	FORCEINLINE bool HasAllPackageFlags(uint32 FlagsToCheck) const
	{
		return (PackageFlagsPrivate & FlagsToCheck) == FlagsToCheck;
	}

	/** EPackageFlags bits (UE). */
	FORCEINLINE uint32 GetPackageFlags() const
	{
		return PackageFlagsPrivate;
	}

	FORCEINLINE const FGuid& GetGuid() const
	{
		return Guid;
	}

	FORCEINLINE void SetGuid(const FGuid& NewGuid)
	{
		Guid = NewGuid;
	}

private:
	uint32 PackageFlagsPrivate;
	/** The package's GUID (P11: saved in the package summary). */
	FGuid Guid;
};
