#include "UObject/LinkerLoad.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace
{
	/** Package bytes registered with FLinkerLoad::RegisterInMemoryPackage, by long package name. */
	TMap<FName, TArray<uint8>>& GetInMemoryPackages()
	{
		static TMap<FName, TArray<uint8>> Packages;
		return Packages;
	}

	/** The state of the running load (UE: FUObjectSerializeContext; one thread). */
	struct FLoadContext
	{
		/** BeginLoad calls without their EndLoad. */
		int32 LoadDepth = 0;
		/**
		 * The linkers of the packages loaded since the outermost BeginLoad, in the order their LoadAllObjects finished:
		 * a package another one imports finishes first.
		 */
		TArray<FLinkerLoad*> Linkers;
	};

	FLoadContext& GetLoadContext()
	{
		static FLoadContext Context;
		return Context;
	}
} // namespace

// Load context

void BeginLoad()
{
	++GetLoadContext().LoadDepth;
}

void EndLoad()
{
	FLoadContext& Context = GetLoadContext();
	check(Context.LoadDepth > 0);
	if (Context.LoadDepth > 1)
	{
		--Context.LoadDepth;
		return;
	}

	// The outermost load: every package it pulled in is serialized. PostLoad package by package in the order they
	// finished loading (imported packages first), each in export order (UE sorts by linker and offset too). A PostLoad
	// that loads another package nests in this load (the depth stays 1): its linker joins the list and is handled here.
	for (int32 LinkerIndex = 0; LinkerIndex < Context.Linkers.Num(); ++LinkerIndex)
	{
		FLinkerLoad* Linker = Context.Linkers[LinkerIndex];
		for (int32 ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex)
		{
			if (UObject* Object = Linker->ExportMap[ExportIndex].Object)
			{
				Object->ConditionalPostLoad();
			}
		}
	}

	TArray<FLinkerLoad*> Linkers = MoveTemp(Context.Linkers);
	Context.Linkers.Reset();
	for (FLinkerLoad* Linker : Linkers)
	{
		if (UPackage* Package = Linker->LinkerRoot)
		{
			Package->MarkAsFullyLoaded();
		}
		Linker->Detach();
		delete Linker;
	}
	Context.LoadDepth = 0;
	// Soft pointers that did not find their object look again (UE).
	FSoftObjectPath::InvalidateTag();
}

bool IsLoading()
{
	return GetLoadContext().LoadDepth > 0;
}

// FLinkerLoad

FLinkerLoad::FLinkerLoad(UPackage* InParent, const TCHAR* InFilename, uint32 InLoadFlags)
	: FLinker(ELinkerType::Load, InParent, InFilename)
	, LoadFlags(InLoadFlags)
{
	SetIsLoading(true);
	SetIsPersistent(true);
}

FLinkerLoad::~FLinkerLoad()
{
	if (LinkerRoot && LinkerRoot->LinkerLoad == this)
	{
		LinkerRoot->LinkerLoad = nullptr;
	}
}

FLinkerLoad* FLinkerLoad::CreateLinker(UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, Filename, FILEREAD_Silent))
	{
		UE_LOG(LogLinker, Error, TEXT("Cannot read the package file %s"), Filename);
		return nullptr;
	}
	return CreateLinkerFromBytes(Parent, Filename, LoadFlags, MoveTemp(Bytes));
}

FLinkerLoad* FLinkerLoad::CreateLinkerFromMemory(
	UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags, const TArray<uint8>& InPackageData)
{
	TArray<uint8> Bytes = InPackageData;
	return CreateLinkerFromBytes(Parent, Filename, LoadFlags, MoveTemp(Bytes));
}

FLinkerLoad* FLinkerLoad::CreateLinkerFromBytes(
	UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags, TArray<uint8>&& InPackageData)
{
	checkf(!Parent || !Parent->LinkerLoad, "Package %s is already being loaded", *Parent->GetName());
	FLinkerLoad* Linker = new FLinkerLoad(Parent, Filename, LoadFlags);
	Linker->PackageData = MoveTemp(InPackageData);
	if (!Linker->ReadTables())
	{
		delete Linker;
		return nullptr;
	}
	Linker->PackageData.Shrink();
	if (Parent)
	{
		Parent->LinkerLoad = Linker;
		Parent->SetPackageFlagsTo(Linker->Summary.GetPackageFlags());
		Parent->SetGuid(Linker->Summary.Guid);
		Parent->FileName = FName(Filename);
#if !WITH_EDITORONLY_DATA
		// This build has no editor-only properties: their tags are unknown names and are skipped (D14: this platform
		// loads cooked packages, saved with PKG_FilterEditorOnly).
		if (!Parent->HasAnyPackageFlags(uint32(PKG_FilterEditorOnly)))
		{
			UE_LOG(LogLinker, Log,
				TEXT("%s was saved with editor-only data; this build has none, so those properties are skipped"),
				*Parent->GetName());
		}
#endif
	}
	return Linker;
}

FLinkerLoad* FLinkerLoad::FindExistingLinkerForPackage(const UPackage* Package)
{
	return Package ? Package->LinkerLoad : nullptr;
}

bool FLinkerLoad::ReadTables()
{
	Pos = 0;
	*this << Summary;
	if (Summary.Tag != PACKAGE_FILE_TAG)
	{
		if (Summary.Tag == PACKAGE_FILE_TAG_SWAPPED)
		{
			UE_LOG(LogLinker, Error, TEXT("%s is a big-endian package; Leon only reads little-endian packages"),
				*Filename);
		}
		else
		{
			UE_LOG(LogLinker, Error, TEXT("%s is not a package (tag 0x%08X)"), *Filename, uint32(Summary.Tag));
		}
		return false;
	}
	// Saving ends the file with the tag again: a shorter file was cut (UE).
	const int64 FileSize = PackageData.Num();
	uint32 EndTag = 0;
	if (!IsError() && FileSize >= int64(sizeof(EndTag)))
	{
		const int64 SummaryEnd = Pos;
		Seek(FileSize - int64(sizeof(EndTag)));
		*this << EndTag;
		Seek(SummaryEnd);
	}
	if (IsError() || EndTag != PACKAGE_FILE_TAG)
	{
		UE_LOG(LogLinker, Error, TEXT("%s is truncated: the package does not end with its tag"), *Filename);
		return false;
	}
	if (Summary.GetFileVersionUE() < VER_LEON_OLDEST_LOADABLE_PACKAGE)
	{
		UE_LOG(LogLinker, Error, TEXT("%s: package version %d is older than the oldest this engine loads (%d)"),
			*Filename, Summary.GetFileVersionUE(), int32(VER_LEON_OLDEST_LOADABLE_PACKAGE));
		return false;
	}
	if (Summary.GetFileVersionUE() > VER_LEON_LATEST || Summary.GetFileVersionLicenseeUE() > VER_LEON_LATEST_LICENSEE)
	{
		UE_LOG(LogLinker, Error,
			TEXT("%s: package version %d (licensee %d) is newer than this engine's (%d): it was saved by engine %s"),
			*Filename, Summary.GetFileVersionUE(), Summary.GetFileVersionLicenseeUE(), int32(VER_LEON_LATEST),
			*Summary.SavedByEngineVersion.ToString());
		return false;
	}
	SetUEVer(Summary.GetFileVersionUE());
	SetLicenseeUEVer(Summary.GetFileVersionLicenseeUE());
	SetFilterEditorOnly((Summary.GetPackageFlags() & uint32(PKG_FilterEditorOnly)) != 0);

	// Every table entry takes at least one int32, so a count the bytes after its offset cannot hold is corrupt (and is
	// refused before anything reserves room for it).
	auto IsValidTable = [FileSize](int32 Count, int32 Offset)
	{
		constexpr int64 MinEntryBytes = sizeof(int32);
		return Count >= 0 && Offset >= 0 && Offset <= FileSize &&
			(Count == 0 || (Offset < FileSize && int64(Count) <= (int64(FileSize) - Offset) / MinEntryBytes));
	};
	if (!IsValidTable(Summary.NameCount, Summary.NameOffset) ||
		!IsValidTable(Summary.ImportCount, Summary.ImportOffset) ||
		!IsValidTable(Summary.ExportCount, Summary.ExportOffset) ||
		!IsValidTable(Summary.SoftPackageReferencesCount, Summary.SoftPackageReferencesOffset) ||
		Summary.TotalHeaderSize < 0 || Summary.TotalHeaderSize > FileSize || Summary.BulkDataStartOffset < 0 ||
		Summary.BulkDataStartOffset > FileSize)
	{
		UE_LOG(LogLinker, Error, TEXT("%s: the package summary has invalid tables (offsets or counts)"), *Filename);
		return false;
	}

	// Names first: the other tables refer to them.
	Seek(Summary.NameOffset);
	NameMap.Reserve(Summary.NameCount);
	for (int32 Index = 0; Index < Summary.NameCount && !IsError(); ++Index)
	{
		FString NameString;
		*this << NameString;
		// The table holds plain strings: "Actor_7" stays a string with no number suffix.
		NameMap.Add(FName(*NameString, NAME_NO_NUMBER_INTERNAL));
	}

	Seek(Summary.ImportOffset);
	ImportMap.Reserve(Summary.ImportCount);
	for (int32 Index = 0; Index < Summary.ImportCount && !IsError(); ++Index)
	{
		*this << ImportMap.AddDefaulted_GetRef();
	}

	Seek(Summary.ExportOffset);
	ExportMap.Reserve(Summary.ExportCount);
	for (int32 Index = 0; Index < Summary.ExportCount && !IsError(); ++Index)
	{
		*this << ExportMap.AddDefaulted_GetRef();
	}

	Seek(Summary.SoftPackageReferencesOffset);
	SoftPackageReferenceList.Reserve(Summary.SoftPackageReferencesCount);
	for (int32 Index = 0; Index < Summary.SoftPackageReferencesCount && !IsError(); ++Index)
	{
		*this << SoftPackageReferenceList.AddDefaulted_GetRef();
	}

	if (IsError())
	{
		UE_LOG(LogLinker, Error, TEXT("%s: the package tables are corrupt"), *Filename);
		return false;
	}
	for (const FObjectImport& Import : ImportMap)
	{
		if (!IsValidIndex(Import.OuterIndex) || Import.OuterIndex.IsExport())
		{
			UE_LOG(
				LogLinker, Error, TEXT("%s: import %s has an invalid outer"), *Filename, *Import.ObjectName.ToString());
			return false;
		}
	}
	for (const FObjectExport& Export : ExportMap)
	{
		if (!IsValidIndex(Export.ClassIndex) || !IsValidIndex(Export.OuterIndex) || Export.SerialSize < 0 ||
			Export.SerialOffset < Summary.TotalHeaderSize || Export.SerialOffset + Export.SerialSize > FileSize)
		{
			UE_LOG(LogLinker, Error, TEXT("%s: export %s is corrupt"), *Filename, *Export.ObjectName.ToString());
			return false;
		}
	}

	Seek(Summary.TotalHeaderSize);
	return !IsError();
}

bool FLinkerLoad::IsValidIndex(FPackageIndex Index) const
{
	if (Index.IsImport())
	{
		return Index.ToImport() < ImportMap.Num();
	}
	if (Index.IsExport())
	{
		return Index.ToExport() < ExportMap.Num();
	}
	return true;
}

void FLinkerLoad::LoadAllObjects()
{
	checkf(LinkerRoot, "LoadAllObjects on a linker that only reads the tables (%s)", *Filename);
	for (int32 Index = 0; Index < ExportMap.Num(); ++Index)
	{
		if (CreateExport(Index))
		{
			PreloadExport(Index);
		}
	}
	// The outermost EndLoad post-loads the objects and releases the linker.
	if (::IsLoading())
	{
		GetLoadContext().Linkers.AddUnique(this);
	}
}

UObject* FLinkerLoad::GetExportOuter(int32 Index)
{
	const FPackageIndex OuterIndex = ExportMap[Index].OuterIndex;
	if (OuterIndex.IsNull())
	{
		return LinkerRoot;
	}
	// Leon saves exports with exports as outers; an import outer (UE's forced exports, never saved here) has none.
	return OuterIndex.IsExport() ? CreateExport(OuterIndex.ToExport()) : nullptr;
}

UObject* FLinkerLoad::CreateExport(int32 Index)
{
	check(LinkerRoot);
	FObjectExport& Export = ExportMap[Index];
	if (Export.Object || Export.bExportLoadFailed)
	{
		return Export.Object;
	}
	// Set first: an export that refers to itself through its class or outer (a corrupt file) fails instead of
	// recursing.
	Export.bExportLoadFailed = true;

	UClass* Class = Cast<UClass>(IndexToObject(Export.ClassIndex));
	if (!Class)
	{
		UE_LOG(LogLinker, Warning, TEXT("%s: the class of %s is missing (%s); the object is not loaded"), *Filename,
			*GetExportPathName(Index),
			Export.ClassIndex.IsNull() ? TEXT("None") : *ImpExp(Export.ClassIndex).ObjectName.ToString());
		return nullptr;
	}
	if (Class->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogLinker, Warning, TEXT("%s: the class %s of %s is abstract; the object is not loaded"), *Filename,
			*Class->GetName(), *GetExportPathName(Index));
		return nullptr;
	}
	UObject* Outer = GetExportOuter(Index);
	if (!Outer)
	{
		UE_LOG(LogLinker, Warning, TEXT("%s: the outer of %s is missing; the object is not loaded"), *Filename,
			*GetExportPathName(Index));
		return nullptr;
	}

	const EObjectFlags LoadedFlags = (Export.ObjectFlags & RF_Load) | RF_NeedLoad | RF_NeedPostLoad | RF_WasLoaded;
	UObject* Object = StaticFindObjectFastInternal(nullptr, Outer, Export.ObjectName);
	if (Object)
	{
		// A default subobject its outer's constructor built (D12), or an object of the package that was already in
		// memory: loaded in place.
		if (!Object->IsA(Class) || Object->IsPendingKillOrUnreachable())
		{
			UE_LOG(LogLinker, Warning,
				TEXT("%s: %s already exists as a %s, which is not the saved class %s; the object is not loaded"),
				*Filename, *Object->GetPathName(), *Object->GetClass()->GetName(), *Class->GetName());
			return nullptr;
		}
		Object->SetFlags(LoadedFlags);
	}
	else
	{
		FStaticConstructObjectParameters Params(Class);
		Params.Outer = Outer;
		Params.Name = Export.ObjectName;
		Params.SetFlags = LoadedFlags;
		Object = StaticConstructObject_Internal(Params);
	}
	Export.bExportLoadFailed = false;
	Export.Object = Object;
	ObjectToExportIndex.Add(Object, Index);
	return Object;
}

UClass* FLinkerLoad::FindImportClass(const FObjectImport& Import)
{
	UObject* ClassPackage = StaticFindObjectFastInternal(UPackage::StaticClass(), nullptr, Import.ClassPackage);
	if (!ClassPackage)
	{
		return nullptr;
	}
	return (UClass*)StaticFindObjectFastInternal(UClass::StaticClass(), ClassPackage, Import.ClassName);
}

UObject* FLinkerLoad::CreateImport(int32 Index)
{
	FObjectImport& Import = ImportMap[Index];
	if (Import.XObject || Import.bImportFailed)
	{
		return Import.XObject;
	}
	Import.bImportFailed = true;

	if (Import.OuterIndex.IsNull())
	{
		// A package: compiled-in (/Script), loaded, being loaded (a circular reference), or loaded now.
		const FString PackageName = Import.ObjectName.ToString();
		UPackage* Package = FindPackage(nullptr, *PackageName);
		if (!FPackageName::IsScriptPackage(PackageName) &&
			(!Package || (!Package->LinkerLoad && !Package->IsFullyLoaded())))
		{
			Package = LoadPackage(nullptr, *PackageName, LOAD_NoWarn | (LoadFlags & LOAD_Quiet));
		}
		if (!Package)
		{
			UE_LOG(LogLinker, Warning, TEXT("%s: missing import: package %s cannot be found; its objects are null"),
				*Filename, *PackageName);
			return nullptr;
		}
		Import.XObject = Package;
		Import.bImportFailed = false;
		return Package;
	}

	UObject* Outer = IndexToObject(Import.OuterIndex);
	if (!Outer)
	{
		// The outer's import already reported what is missing.
		return nullptr;
	}
	UClass* ImportClass = FindImportClass(Import);
	if (!ImportClass)
	{
		UE_LOG(LogLinker, Warning,
			TEXT("%s: missing import: the class %s.%s of %s cannot be found; the reference is null"), *Filename,
			*Import.ClassPackage.ToString(), *Import.ClassName.ToString(), *GetImportPathName(Index));
		return nullptr;
	}

	UObject* Object = nullptr;
	if (FLinkerLoad* OtherLinker = FindExistingLinkerForPackage(Outer->GetOutermost()))
	{
		// The package is being loaded (a circular reference): its export, created now and serialized by its own load.
		const int32* OuterExport = OtherLinker->ObjectToExportIndex.Find(Outer);
		const FPackageIndex OuterIndex = Outer == OtherLinker->LinkerRoot
			? FPackageIndex()
			: FPackageIndex::FromExport(OuterExport ? *OuterExport : 0);
		if (Outer == OtherLinker->LinkerRoot || OuterExport)
		{
			for (int32 ExportIndex = 0; ExportIndex < OtherLinker->ExportMap.Num(); ++ExportIndex)
			{
				const FObjectExport& Export = OtherLinker->ExportMap[ExportIndex];
				if (Export.ObjectName == Import.ObjectName && Export.OuterIndex == OuterIndex)
				{
					Object = OtherLinker->CreateExport(ExportIndex);
					break;
				}
			}
		}
	}
	if (!Object)
	{
		Object = StaticFindObjectFastInternal(nullptr, Outer, Import.ObjectName);
	}
	if (!Object || Object->IsPendingKillOrUnreachable())
	{
		UE_LOG(LogLinker, Warning, TEXT("%s: missing import: %s %s cannot be found; the reference is null"), *Filename,
			*Import.ClassName.ToString(), *GetImportPathName(Index));
		return nullptr;
	}
	if (!Object->IsA(ImportClass))
	{
		UE_LOG(LogLinker, Warning, TEXT("%s: import %s is a %s, not the saved class %s; the reference is null"),
			*Filename, *GetImportPathName(Index), *Object->GetClass()->GetName(), *ImportClass->GetName());
		return nullptr;
	}
	Import.XObject = Object;
	Import.bImportFailed = false;
	return Object;
}

UObject* FLinkerLoad::IndexToObject(FPackageIndex Index)
{
	if (Index.IsNull())
	{
		return nullptr;
	}
	if (!IsValidIndex(Index))
	{
		UE_LOG(LogLinker, Error, TEXT("%s: object reference %d is out of the tables"), *Filename, Index.ForDebugging());
		SetCriticalError();
		return nullptr;
	}
	return Index.IsExport() ? CreateExport(Index.ToExport()) : CreateImport(Index.ToImport());
}

void FLinkerLoad::Preload(UObject* Object)
{
	if (const int32* Index = ObjectToExportIndex.Find(Object))
	{
		PreloadExport(*Index);
	}
}

void FLinkerLoad::PreloadExport(int32 Index)
{
	FObjectExport& Export = ExportMap[Index];
	UObject* Object = Export.Object;
	if (!Object || !Object->HasAnyFlags(RF_NeedLoad))
	{
		return;
	}
	const int64 SavedPos = Pos;
	Seek(Export.SerialOffset);
	Object->ClearFlags(RF_NeedLoad);
	Object->Serialize(*this);
	const int64 Consumed = Pos - Export.SerialOffset;
	if (IsError())
	{
		UE_LOG(LogLinker, Error, TEXT("%s: the data of %s is corrupt"), *Filename, *GetExportFullName(Index));
		ClearError();
	}
	else if (Consumed != Export.SerialSize)
	{
		UE_LOG(LogLinker, Error,
			TEXT("%s: %s read %lld bytes of its %lld (its class's Serialize does not match the saved data)"), *Filename,
			*GetExportFullName(Index), (long long)Consumed, (long long)Export.SerialSize);
	}
	Object->SetFlags(RF_LoadCompleted);
	Seek(SavedPos);
}

void FLinkerLoad::Detach()
{
	for (FObjectExport& Export : ExportMap)
	{
		Export.Object = nullptr;
	}
	for (FObjectImport& Import : ImportMap)
	{
		Import.XObject = nullptr;
	}
	ObjectToExportIndex.Empty();
	if (LinkerRoot && LinkerRoot->LinkerLoad == this)
	{
		LinkerRoot->LinkerLoad = nullptr;
	}
	LinkerRoot = nullptr;
	PackageData.Empty();
	Pos = 0;
}

// FArchive

void FLinkerLoad::Serialize(void* V, int64 Length)
{
	if (Length <= 0)
	{
		return;
	}
	if (IsError() || Pos < 0 || Pos + Length > PackageData.Num())
	{
		FMemory::Memzero(V, SIZE_T(Length));
		SetError();
		return;
	}
	FMemory::Memcpy(V, PackageData.GetData() + Pos, SIZE_T(Length));
	Pos += Length;
}

int64 FLinkerLoad::Tell()
{
	return Pos;
}

int64 FLinkerLoad::TotalSize()
{
	return PackageData.Num();
}

void FLinkerLoad::Seek(int64 InPos)
{
	Pos = InPos;
}

FArchive& FLinkerLoad::operator<<(FName& Name)
{
	int32 NameIndex = 0;
	int32 Number = 0;
	*this << NameIndex << Number;
	if (IsError())
	{
		Name = NAME_None;
	}
	else if (NameIndex < 0 || NameIndex >= NameMap.Num())
	{
		UE_LOG(LogLinker, Error, TEXT("%s: name index %d is out of the name table (%d names)"), *Filename, NameIndex,
			NameMap.Num());
		SetCriticalError();
		Name = NAME_None;
	}
	else
	{
		Name = FName(NameMap[NameIndex], Number);
	}
	return *this;
}

FArchive& FLinkerLoad::operator<<(UObject*& Object)
{
	FPackageIndex Index;
	*this << Index;
	Object = IsError() ? nullptr : IndexToObject(Index);
	return *this;
}

FLinker* FLinkerLoad::GetLinker()
{
	return this;
}

FString FLinkerLoad::GetArchiveName() const
{
	return Filename;
}

// In-memory packages

void FLinkerLoad::RegisterInMemoryPackage(const FString& LongPackageName, const TArray<uint8>& InPackageData)
{
	GetInMemoryPackages().Add(FName(*LongPackageName), InPackageData);
}

bool FLinkerLoad::UnregisterInMemoryPackage(const FString& LongPackageName)
{
	return GetInMemoryPackages().Remove(FName(*LongPackageName)) > 0;
}

const TArray<uint8>* FLinkerLoad::FindInMemoryPackage(const FString& LongPackageName)
{
	const FName Name(*LongPackageName, FNAME_Find);
	return Name.IsNone() ? nullptr : GetInMemoryPackages().Find(Name);
}
