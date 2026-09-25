#pragma once

// What the package loader and saver share (UE: UObject/Linker.h).

#include "CoreMinimal.h"
#include "UObject/ObjectResource.h"
#include "UObject/PackageFileSummary.h"

class UPackage;

DECLARE_LOG_CATEGORY_EXTERN(LogLinker, Log, All);

/** Which way a linker goes (UE: ELinkerType). */
namespace ELinkerType
{
	enum Type
	{
		None,
		Load,
		Save
	};
} // namespace ELinkerType

/**
 * The tables of one package file: summary, names, imports, exports and soft package references (UE: FLinker with
 * FLinkerTables). FLinkerLoad reads them, FLinkerSave writes them; both are also the FArchive the export data goes
 * through, turning FName into name table indices and UObject* into FPackageIndex.
 */
class COREUOBJECT_API FLinker
{
public:
	FLinker(ELinkerType::Type InType, UPackage* InRoot, const TCHAR* InFilename);
	virtual ~FLinker();

	FLinker(const FLinker&) = delete;
	FLinker& operator=(const FLinker&) = delete;

	FORCEINLINE ELinkerType::Type GetType() const
	{
		return LinkerType;
	}

	/** The import at Index (UE: Imp). */
	FObjectImport& Imp(FPackageIndex Index);
	const FObjectImport& Imp(FPackageIndex Index) const;

	/** The export at Index (UE: Exp). */
	FObjectExport& Exp(FPackageIndex Index);
	const FObjectExport& Exp(FPackageIndex Index) const;

	/** The import or export at Index (UE: ImpExp). */
	FObjectResource& ImpExp(FPackageIndex Index);

	/** The class name of export ExportIndex, from its class import (UE: GetExportClassName). */
	FName GetExportClassName(int32 ExportIndex);

	/** "/Script/CoreUObject.Class" style path of import ImportIndex (UE: GetImportPathName). */
	FString GetImportPathName(int32 ImportIndex);

	/** The path of export ExportIndex inside the linker's package: "/Game/Pkg.Asset:Sub" (UE: GetExportPathName). */
	FString GetExportPathName(int32 ExportIndex);

	/** "<ClassName> <PathName>" of export ExportIndex (UE: GetExportFullName). */
	FString GetExportFullName(int32 ExportIndex);

	/** The package the linker loads into or saves; nullptr for a linker that only reads the tables. */
	UPackage* LinkerRoot;

	FPackageFileSummary Summary;

	/** The name table: the strings of every FName in the package, number 0. */
	TArray<FName> NameMap;

	TArray<FObjectImport> ImportMap;
	TArray<FObjectExport> ExportMap;

	/** The packages the exports reference through soft object paths (UE: SoftPackageReferenceList). */
	TArray<FName> SoftPackageReferenceList;

	/** The package file (or the name the in-memory package was registered under). */
	FString Filename;

protected:
	/** The package path of the linker's root ("/Game/Pkg"), or the summary-less file name. */
	FString GetRootPathName() const;

	ELinkerType::Type LinkerType;
};

/**
 * Detaches the loaders of InOuter's package, or of every package with a null InOuter (UE: ResetLoaders). Leon's loads
 * release their linker when the package is loaded, so this only matters while a load is running.
 */
COREUOBJECT_API void ResetLoaders(UObject* InOuter);
