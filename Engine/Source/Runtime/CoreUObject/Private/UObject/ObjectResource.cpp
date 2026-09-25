#include "UObject/ObjectResource.h"

FArchive& operator<<(FArchive& Ar, FObjectExport& Export)
{
	Ar << Export.ClassIndex;
	Ar << Export.SuperIndex;
	Ar << Export.OuterIndex;
	Ar << Export.ObjectName;

	uint32 Flags = uint32(Export.ObjectFlags & RF_Load);
	Ar << Flags;
	if (Ar.IsLoading())
	{
		Export.ObjectFlags = EObjectFlags(Flags & uint32(RF_Load));
	}

	Ar << Export.SerialSize;
	Ar << Export.SerialOffset;
	Ar << Export.bForcedExport;
	Ar << Export.bNotForClient;
	Ar << Export.bNotForServer;
	Ar << Export.PackageFlags;
	Ar << Export.bIsAsset;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FObjectImport& Import)
{
	Ar << Import.ClassPackage;
	Ar << Import.ClassName;
	Ar << Import.OuterIndex;
	Ar << Import.ObjectName;
	return Ar;
}
