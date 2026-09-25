#pragma once

// The header of a package file (UE: UObject/PackageFileSummary.h).

#include "CoreMinimal.h"
#include "Misc/EngineVersion.h"
#include "Misc/Guid.h"
#include "Serialization/Archive.h"

/** The first four bytes of every package, "LEON" (UE: PACKAGE_FILE_TAG, 0x9E2A83C1). */
#define PACKAGE_FILE_TAG 0x4E4F454C

/** The tag as a big-endian (byte-swapped) package would read (UE: PACKAGE_FILE_TAG_SWAPPED). Leon rejects those. */
#define PACKAGE_FILE_TAG_SWAPPED 0x4C454F4E

/**
 * The fixed part at the start of a `.lasset` / `.lmap` file: the format version, the package flags, where the tables
 * and the bulk data are, the package GUID and the engine that saved it (UE: FPackageFileSummary, trimmed). Saving is
 * deterministic (plan decision D13): nothing here depends on the time or the machine.
 */
struct COREUOBJECT_API FPackageFileSummary
{
	/** PACKAGE_FILE_TAG. */
	int32 Tag = PACKAGE_FILE_TAG;

	/** The package format version, an ELeonPackageVersion (UE 5: FileVersionUE; UE 4.27: FileVersionUE4). */
	int32 FileVersionUE = 0;

	/** The licensee version: always 0 (UE 5: FileVersionLicenseeUE). */
	int32 FileVersionLicenseeUE = 0;

	/** Size of the summary and the tables: the export data starts here. */
	int32 TotalHeaderSize = 0;

	/** The package's EPackageFlags (PKG_ContainsMap, PKG_Cooked, PKG_FilterEditorOnly, ...). */
	uint32 PackageFlags = 0;

	/** The name table: NameCount strings, sorted and without duplicates (D13). */
	int32 NameCount = 0;
	int32 NameOffset = 0;

	/** The export table: ExportCount FObjectExport. */
	int32 ExportCount = 0;
	int32 ExportOffset = 0;

	/** The import table: ImportCount FObjectImport. */
	int32 ImportCount = 0;
	int32 ImportOffset = 0;

	/** The packages the exports reference softly (FSoftObjectPath): SoftPackageReferencesCount names, sorted. */
	int32 SoftPackageReferencesCount = 0;
	int32 SoftPackageReferencesOffset = 0;

	/** The package GUID: FGuid::NewDeterministicGuid of the long package name (MD5), not a random one (D13). */
	FGuid Guid;

	/** The engine that saved the package (Build.version). */
	FEngineVersion SavedByEngineVersion;

	/** The platform a cooked package (PKG_Cooked) was cooked for; empty otherwise (Leon). */
	FString CookedPlatform;

	/** Where the bulk data payloads start: after the export data, at the end of the file (D13). */
	int64 BulkDataStartOffset = 0;

	FORCEINLINE int32 GetFileVersionUE() const
	{
		return FileVersionUE;
	}

	FORCEINLINE int32 GetFileVersionLicenseeUE() const
	{
		return FileVersionLicenseeUE;
	}

	FORCEINLINE uint32 GetPackageFlags() const
	{
		return PackageFlags;
	}

	FORCEINLINE void SetPackageFlags(uint32 InPackageFlags)
	{
		PackageFlags = InPackageFlags;
	}

	/**
	 * Every field in declaration order. Loading stops after a wrong Tag (a byte-swapped or foreign file) and sets the
	 * archive's error.
	 */
	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FPackageFileSummary& Sum);
};
