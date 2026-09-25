#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SavePackage.h"

class FLinkerLoad;
class FOutputDevice;

/**
 * The outermost object of every object: a /Script/<Module> package of compiled-in types, the transient package, or a
 * `.lasset` / `.lmap` package loaded from disk with LoadPackage and written with SavePackage (UE: UPackage).
 */
class COREUOBJECT_API UPackage : public UObject
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(
		UPackage, UObject, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UPackage, COREUOBJECT_API)

public:
	explicit UPackage(const FObjectInitializer& ObjectInitializer);

	/** The linker loading this package while LoadPackage runs; nullptr otherwise (UE: LinkerLoad). */
	FLinkerLoad* LinkerLoad;

	/** The file the package was loaded from, NAME_None for a package that was not loaded (UE: FileName). */
	FName FileName;

	virtual void BeginDestroy() override;

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

	/** The package's GUID: saved in the package summary (derived from the name, D13) and restored by a load (UE). */
	FORCEINLINE const FGuid& GetGuid() const
	{
		return Guid;
	}

	FORCEINLINE void SetGuid(const FGuid& NewGuid)
	{
		Guid = NewGuid;
	}

	/** True for a package with PKG_ContainsMap: a map, saved as ".lmap" (UE: ContainsMap). */
	FORCEINLINE bool ContainsMap() const
	{
		return HasAnyPackageFlags(PKG_ContainsMap);
	}

	/**
	 * True once LoadPackage finished loading the package, and for a package that has no file (created in memory): there
	 * is nothing left to load (UE: IsFullyLoaded).
	 */
	bool IsFullyLoaded() const;

	/** Records that the package needs no further loading (UE: MarkAsFullyLoaded). */
	FORCEINLINE void MarkAsFullyLoaded()
	{
		bHasBeenFullyLoaded = true;
	}

	/**
	 * Saves InOuter to Filename (UE: UPackage::Save, trimmed). The exports are Base, the objects of the package with
	 * any of TopLevelFlags (RF_Public, RF_Standalone), and, recursively, their outers and inner objects (default
	 * subobjects included) and every object of the package they reference; transient and pending-kill objects are not
	 * saved, and references to them are saved as null. References to objects of other packages become imports
	 * (classes: "/Script/<Module>" imports). Each export is saved as tagged properties that differ from its archetype
	 * (the class default object, or for a default subobject the subobject of its outer's archetype), ended by
	 * NAME_None, followed by its native UObject::Serialize data; bulk data goes at the end of the file. The output only
	 * depends on the objects (D13): names, imports and exports are sorted, the GUID is derived from the package name
	 * and there are no timestamps. A package with PKG_FilterEditorOnly (and every package on a platform without
	 * editor-only data) drops the editor-only properties. A ".lmap" file sets PKG_ContainsMap. Errors are logged
	 * (warnings with SAVE_NoError) and also sent to Error when given. UE's Conform, bForceByteSwapping,
	 * bWarnOfLongFilename, TargetPlatform, FinalTimeStamp, bSlowTask, DiffMap and SavePackageContext parameters are
	 * not supported.
	 */
	static FSavePackageResultStruct Save(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags,
		const TCHAR* Filename, FOutputDevice* Error = nullptr, uint32 SaveFlags = SAVE_None);

	/** Save, returning whether it succeeded (UE: SavePackage). */
	static bool SavePackage(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename,
		FOutputDevice* Error = nullptr, uint32 SaveFlags = SAVE_None);

	/**
	 * Save into OutPackageData instead of a file (Leon; the same bytes Save writes). Register them with
	 * FLinkerLoad::RegisterInMemoryPackage to load them back.
	 */
	static FSavePackageResultStruct SaveToMemory(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags,
		TArray<uint8>& OutPackageData, FOutputDevice* Error = nullptr, uint32 SaveFlags = SAVE_None);

private:
	uint32 PackageFlagsPrivate;
	/** The package's GUID: from the package summary once loaded or saved. */
	FGuid Guid;
	/** Set by MarkAsFullyLoaded, or by IsFullyLoaded for a package without a file. */
	mutable bool bHasBeenFullyLoaded;
};
