#include "UObject/PackageFileSummary.h"

#include "UObject/Linker.h"

FArchive& operator<<(FArchive& Ar, FPackageFileSummary& Sum)
{
	Ar << Sum.Tag;
	if (Ar.IsLoading() && Sum.Tag != PACKAGE_FILE_TAG)
	{
		// A big-endian package (PACKAGE_FILE_TAG_SWAPPED) or not a package at all: nothing else can be read.
		Ar.SetError();
		return Ar;
	}
	Ar << Sum.FileVersionUE;
	Ar << Sum.FileVersionLicenseeUE;
	Ar << Sum.TotalHeaderSize;
	Ar << Sum.PackageFlags;
	Ar << Sum.NameCount << Sum.NameOffset;
	Ar << Sum.ExportCount << Sum.ExportOffset;
	Ar << Sum.ImportCount << Sum.ImportOffset;
	Ar << Sum.SoftPackageReferencesCount << Sum.SoftPackageReferencesOffset;
	Ar << Sum.Guid;
	Ar << Sum.SavedByEngineVersion;
	Ar << Sum.CookedPlatform;
	Ar << Sum.BulkDataStartOffset;
	return Ar;
}
