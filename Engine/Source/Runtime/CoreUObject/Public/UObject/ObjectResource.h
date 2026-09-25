#pragma once

// The import and export tables of a package (UE: UObject/ObjectResource.h).

#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "UObject/ObjectMacros.h"

class UClass;
class UObject;

/**
 * A reference to an entry of a linker's tables (UE: FPackageIndex): 0 is null, a positive value N is the export N - 1,
 * a negative value -N is the import N - 1. Serialized as its int32; object references inside a package are written as
 * one of these.
 */
class FPackageIndex
{
public:
	FORCEINLINE FPackageIndex()
		: Index(0)
	{
	}

	FORCEINLINE bool IsImport() const
	{
		return Index < 0;
	}

	FORCEINLINE bool IsExport() const
	{
		return Index > 0;
	}

	FORCEINLINE bool IsNull() const
	{
		return Index == 0;
	}

	/** The import table index; only for an import. */
	FORCEINLINE int32 ToImport() const
	{
		check(IsImport());
		return -Index - 1;
	}

	/** The export table index; only for an export. */
	FORCEINLINE int32 ToExport() const
	{
		check(IsExport());
		return Index - 1;
	}

	/** The raw value, for logs (UE). */
	FORCEINLINE int32 ForDebugging() const
	{
		return Index;
	}

	FORCEINLINE static FPackageIndex FromImport(int32 ImportIndex)
	{
		check(ImportIndex >= 0);
		return FPackageIndex(-ImportIndex - 1);
	}

	FORCEINLINE static FPackageIndex FromExport(int32 ExportIndex)
	{
		check(ExportIndex >= 0);
		return FPackageIndex(ExportIndex + 1);
	}

	FORCEINLINE bool operator==(const FPackageIndex& Other) const
	{
		return Index == Other.Index;
	}

	FORCEINLINE bool operator!=(const FPackageIndex& Other) const
	{
		return Index != Other.Index;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FPackageIndex& In)
	{
		return uint32(In.Index);
	}

	friend FArchive& operator<<(FArchive& Ar, FPackageIndex& Value)
	{
		Ar << Value.Index;
		return Ar;
	}

private:
	explicit FPackageIndex(int32 InIndex)
		: Index(InIndex)
	{
	}

	int32 Index;
};

/** What imports and exports share: the name and the outer (UE: FObjectResource). */
struct FObjectResource
{
	/** The object's own name, without its outers. */
	FName ObjectName;

	/**
	 * The entry of the object's outer. Exports: null for an object directly in the package, else the outer's export.
	 * Imports: null for a package, else the outer's import.
	 */
	FPackageIndex OuterIndex;
};

/**
 * An object saved in this package (UE: FObjectExport). Its data, tagged properties then the native Serialize tail,
 * is SerialSize bytes at SerialOffset.
 */
struct COREUOBJECT_API FObjectExport : public FObjectResource
{
	/** The object's class: an import of a /Script/<Module> class. */
	FPackageIndex ClassIndex;

	/** The super struct of a struct export (UE: blueprint classes); always null in Leon. */
	FPackageIndex SuperIndex;

	/** The object's flags, masked with RF_Load. */
	EObjectFlags ObjectFlags = RF_NoFlags;

	/** Size of the object's data. */
	int64 SerialSize = 0;

	/** Position of the object's data in the file. */
	int64 SerialOffset = 0;

	/** UE: an export whose outer lives in another package; always false in Leon. */
	bool bForcedExport = false;

	/** UE: skipped on dedicated clients / servers; always false in Leon (no networking). */
	bool bNotForClient = false;
	bool bNotForServer = false;

	/** The object is an asset (UObject::IsAsset): public, not transient, directly in the package. */
	bool bIsAsset = false;

	/** The package flags of a package export (UE); always 0 in Leon. */
	uint32 PackageFlags = 0;

	// Not serialized.

	/** The object once created (loading) or the object saved (saving). */
	UObject* Object = nullptr;

	/** True once creating the object failed (missing class or outer); it stays null. */
	bool bExportLoadFailed = false;

	/**
	 * ClassIndex, SuperIndex, OuterIndex, ObjectName, ObjectFlags (uint32), SerialSize, SerialOffset (int64), the three
	 * bools, PackageFlags, bIsAsset: a fixed size (60 bytes) once names are table indices.
	 */
	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FObjectExport& Export);
};

/** An object of another package an export references (UE: FObjectImport). */
struct COREUOBJECT_API FObjectImport : public FObjectResource
{
	/** The package of the object's class ("/Script/CoreUObject"). */
	FName ClassPackage;

	/** The name of the object's class ("Class", "Package", "PackageTestObject"). */
	FName ClassName;

	// Not serialized.

	/** The object once resolved (loading) or the object referenced (saving). */
	UObject* XObject = nullptr;

	/** True once resolving the import failed; it stays null. */
	bool bImportFailed = false;

	/** ClassPackage, ClassName, OuterIndex, ObjectName (UE's order). */
	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FObjectImport& Import);
};
