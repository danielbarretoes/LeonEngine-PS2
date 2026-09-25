#include "Commandlets/ValidateAssetsCommandlet.h"

#include "AssetImportUtils.h"
#include "Commandlets/ResavePackagesCommandlet.h"
#include "LeonEdLog.h"
#include "Misc/PackageName.h"
#include "Templates/UniquePtr.h"
#include "UObject/GarbageCollection.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"

UValidateAssetsCommandlet::UValidateAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HelpDescription = TEXT("Loads packages and checks that their imports resolve and their classes are valid");
	HelpUsage = TEXT("-run=ValidateAssets [-package=<LongPackageName>[,...]] [-packagefolder=<LongPackagePath>]");
	LogToConsole = 1;
}

namespace
{

	/** The object path of export Index: the package, then the outer exports (a tables-only linker has no package). */
	FString GetExportObjectPath(const FLinkerLoad& Tables, const FString& PackageName, int32 Index)
	{
		FString Path = Tables.ExportMap[Index].ObjectName.ToString();
		FPackageIndex Outer = Tables.ExportMap[Index].OuterIndex;
		while (Outer.IsExport())
		{
			const FObjectExport& OuterExport = Tables.ExportMap[Outer.ToExport()];
			// ':' follows a top-level object (UE's SUBOBJECT_DELIMITER), '.' anything else.
			Path = OuterExport.ObjectName.ToString() +
				(OuterExport.OuterIndex.IsNull() ? SUBOBJECT_DELIMITER : TEXT(".")) + Path;
			Outer = OuterExport.OuterIndex;
		}
		return PackageName + TEXT(".") + Path;
	}

} // namespace

int32 UValidateAssetsCommandlet::ValidatePackage(const FString& PackageName)
{
	FString Filename;
	if (!FPackageName::DoesPackageExist(PackageName, nullptr, &Filename) || Filename.IsEmpty())
	{
		UE_LOG(LogLeonEd, Error, "ValidateAssets: %s has no file", *PackageName);
		return 1;
	}
	// The tables first, without loading: a package that cannot be read is one problem.
	const TUniquePtr<FLinkerLoad> Tables(FLinkerLoad::CreateLinker(nullptr, *Filename, LOAD_None));
	if (!Tables)
	{
		UE_LOG(LogLeonEd, Error, "ValidateAssets: %s is not a readable package", *Filename);
		return 1;
	}
	UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
	if (Package == nullptr)
	{
		UE_LOG(LogLeonEd, Error, "ValidateAssets: %s does not load", *PackageName);
		return 1;
	}

	int32 Problems = 0;
	for (int32 Index = 0; Index < Tables->ImportMap.Num(); ++Index)
	{
		const FObjectImport& Import = Tables->ImportMap[Index];
		const FString Path = Tables->GetImportPathName(Index);
		bool bResolved = false;
		if (Import.OuterIndex.IsNull())
		{
			// A package: compiled in (/Script), in memory or on disk.
			bResolved = FindPackage(nullptr, *Path) != nullptr ||
				(!FPackageName::IsScriptPackage(Path) && FPackageName::DoesPackageExist(Path));
		}
		else
		{
			bResolved = StaticFindObject(UObject::StaticClass(), nullptr, *Path) != nullptr;
		}
		if (!bResolved)
		{
			UE_LOG(LogLeonEd, Error, "ValidateAssets: %s imports %s, which does not resolve", *PackageName, *Path);
			++Problems;
		}
	}
	for (int32 Index = 0; Index < Tables->ExportMap.Num(); ++Index)
	{
		const FString Path = GetExportObjectPath(*Tables, PackageName, Index);
		const UObject* Object = StaticFindObject(UObject::StaticClass(), nullptr, *Path);
		if (Object == nullptr)
		{
			UE_LOG(LogLeonEd, Error, "ValidateAssets: %s's export %s was not made (class %s)", *PackageName, *Path,
				*Tables->GetExportClassName(Index).ToString());
			++Problems;
		}
		else if (Object->GetClass()->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogLeonEd, Error, "ValidateAssets: %s's export %s is of the abstract class %s", *PackageName, *Path,
				*Object->GetClass()->GetName());
			++Problems;
		}
	}
	return Problems;
}

int32 UValidateAssetsCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamsMap;
	ParseCommandLine(*Params, Tokens, Switches, ParamsMap);
	TArray<FString> Packages;
	UResavePackagesCommandlet::GatherPackages(ParamsMap, Packages);

	int32 Problems = 0;
	int32 Invalid = 0;
	for (const FString& PackageName : Packages)
	{
		const int32 PackageProblems = ValidatePackage(PackageName);
		Problems += PackageProblems;
		Invalid += PackageProblems > 0 ? 1 : 0;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	UE_LOG(LogLeonEd, Display, "ValidateAssets: %d packages, %d valid, %d problem(s)", Packages.Num(),
		Packages.Num() - Invalid, Problems);
	return Problems == 0 ? 0 : 1;
}
