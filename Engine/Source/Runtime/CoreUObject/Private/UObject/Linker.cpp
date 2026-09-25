#include "UObject/Linker.h"

#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY(LogLinker);

FLinker::FLinker(ELinkerType::Type InType, UPackage* InRoot, const TCHAR* InFilename)
	: LinkerRoot(InRoot)
	, Filename(InFilename ? InFilename : TEXT(""))
	, LinkerType(InType)
{
}

FLinker::~FLinker()
{
}

FObjectImport& FLinker::Imp(FPackageIndex Index)
{
	return ImportMap[Index.ToImport()];
}

const FObjectImport& FLinker::Imp(FPackageIndex Index) const
{
	return ImportMap[Index.ToImport()];
}

FObjectExport& FLinker::Exp(FPackageIndex Index)
{
	return ExportMap[Index.ToExport()];
}

const FObjectExport& FLinker::Exp(FPackageIndex Index) const
{
	return ExportMap[Index.ToExport()];
}

FObjectResource& FLinker::ImpExp(FPackageIndex Index)
{
	check(!Index.IsNull());
	if (Index.IsImport())
	{
		return Imp(Index);
	}
	return Exp(Index);
}

FName FLinker::GetExportClassName(int32 ExportIndex)
{
	const FPackageIndex ClassIndex = ExportMap[ExportIndex].ClassIndex;
	if (ClassIndex.IsNull())
	{
		return NAME_Class;
	}
	return ImpExp(ClassIndex).ObjectName;
}

FString FLinker::GetImportPathName(int32 ImportIndex)
{
	// Outers first: "/Script/CoreUObject" + "." + "Class".
	FString Result;
	for (FPackageIndex Index = FPackageIndex::FromImport(ImportIndex); !Index.IsNull(); Index = Imp(Index).OuterIndex)
	{
		const FObjectImport& Import = Imp(Index);
		FString Part = Import.ObjectName.ToString();
		if (Result.Len() > 0)
		{
			// ':' follows a top-level object (one directly in its package), '.' anything else (UE: GetPathName). An
			// import without an outer is a package.
			const bool bIsTopLevel = !Import.OuterIndex.IsNull() && Imp(Import.OuterIndex).OuterIndex.IsNull();
			Part += bIsTopLevel ? SUBOBJECT_DELIMITER : TEXT(".");
		}
		Result = Part + Result;
	}
	return Result;
}

FString FLinker::GetExportPathName(int32 ExportIndex)
{
	FString Result;
	for (FPackageIndex Index = FPackageIndex::FromExport(ExportIndex); !Index.IsNull(); Index = Exp(Index).OuterIndex)
	{
		const FObjectExport& Export = Exp(Index);
		FString Part = Export.ObjectName.ToString();
		if (Result.Len() > 0)
		{
			// An export without an outer is directly in the package: a top-level object.
			Part += Export.OuterIndex.IsNull() ? SUBOBJECT_DELIMITER : TEXT(".");
		}
		Result = Part + Result;
	}
	return GetRootPathName() + TEXT(".") + Result;
}

FString FLinker::GetExportFullName(int32 ExportIndex)
{
	return GetExportClassName(ExportIndex).ToString() + TEXT(" ") + GetExportPathName(ExportIndex);
}

FString FLinker::GetRootPathName() const
{
	return LinkerRoot ? LinkerRoot->GetName() : Filename;
}

void ResetLoaders(UObject* InOuter)
{
	if (IsLoading())
	{
		// The running load releases its linkers when it ends (EndLoad).
		UE_LOG(LogLinker, Warning, TEXT("ResetLoaders during a load: the linkers are released when the load ends"));
		return;
	}
	if (InOuter)
	{
		if (FLinkerLoad* Linker = FLinkerLoad::FindExistingLinkerForPackage(InOuter->GetOutermost()))
		{
			Linker->Detach();
			delete Linker;
		}
		return;
	}
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		if (FLinkerLoad* Linker = It->LinkerLoad)
		{
			Linker->Detach();
			delete Linker;
		}
	}
}
