#include "UObject/SavePackage.h"

#include "HAL/PlatformProperties.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/BulkData.h"
#include "UObject/Class.h"
#include "UObject/LinkerLoad.h"
#include "UObject/LinkerSave.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectThreadContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavePackage, Log, All);

namespace
{
	/** What one save collects before it writes (UE: the tagging passes of SavePackage). */
	struct FSaveContext
	{
		UPackage* Package = nullptr;
		/** PKG_FilterEditorOnly: editor-only properties and objects are left out. */
		bool bFilterEditorOnly = false;
		TArray<UObject*> Exports;
		TSet<UObject*> ExportSet;
		/** Exports whose references are not collected yet. */
		TArray<UObject*> PendingExports;
		TArray<UObject*> Imports;
		TSet<UObject*> ImportSet;
		/** The name table: number 0 names, one per entry. */
		TArray<FName> Names;
		TSet<FNameEntryId> NameEntries;
		TArray<FName> SoftPackageReferences;
	};

	/**
	 * Transient objects are never saved: flagged, pending kill, inside a transient outer, or of a transient class.
	 * Native objects (classes, structs, enums of /Script packages, which are flagged transient) can be referenced (UE).
	 */
	bool IsTransientForSave(const UObject* Object)
	{
		if (Object->HasAnyInternalFlags(EInternalObjectFlags::Native))
		{
			return false;
		}
		for (const UObject* Current = Object; Current; Current = Current->GetOuter())
		{
			if (Current->HasAnyFlags(RF_Transient) || Current->IsPendingKill())
			{
				return true;
			}
		}
		return Object->GetClass()->HasAnyClassFlags(CLASS_Transient);
	}

	/**
	 * An object only the editor needs (UE: IsEditorOnlyObject): it or one of its outers says so (UObject::IsEditorOnly,
	 * the assets' import data). A package that filters editor-only data leaves it out and saves references to it as
	 * null.
	 */
	bool IsEditorOnlyForSave(const FSaveContext& Context, const UObject* Object)
	{
		if (!Context.bFilterEditorOnly)
		{
			return false;
		}
		for (const UObject* Current = Object; Current && Current != Context.Package; Current = Current->GetOuter())
		{
			if (Current->IsEditorOnly())
			{
				return true;
			}
		}
		return false;
	}

	void AddName(FSaveContext& Context, FName Name)
	{
		const FNameEntryId Entry = Name.GetComparisonIndex();
		if (!Context.NameEntries.Contains(Entry))
		{
			Context.NameEntries.Add(Entry);
			Context.Names.Add(FName(Entry, Entry, NAME_NO_NUMBER_INTERNAL));
		}
	}

	/**
	 * Makes Object an export when it belongs to the package and can be saved, with its outers up to the package and
	 * its inner objects (default subobjects included).
	 */
	void MarkExport(FSaveContext& Context, UObject* Object)
	{
		if (!Object || Object == Context.Package || Context.ExportSet.Contains(Object) ||
			!Object->IsIn(Context.Package) || Object->HasAnyFlags(RF_ClassDefaultObject) ||
			IsTransientForSave(Object) || IsEditorOnlyForSave(Context, Object))
		{
			return;
		}
		Context.ExportSet.Add(Object);
		Context.Exports.Add(Object);
		Context.PendingExports.Add(Object);
		MarkExport(Context, Object->GetOuter());
		TArray<UObject*> InnerObjects;
		GetObjectsWithOuter(Object, InnerObjects, /*bIncludeNestedObjects =*/false);
		for (UObject* Inner : InnerObjects)
		{
			MarkExport(Context, Inner);
		}
	}

	/** Makes Object, its outers and its class names imports (UE: FArchiveSaveTagImports). */
	void AddImport(FSaveContext& Context, UObject* Object)
	{
		if (!Object || Context.ImportSet.Contains(Object))
		{
			return;
		}
		Context.ImportSet.Add(Object);
		Context.Imports.Add(Object);
		AddName(Context, Object->GetFName());
		AddName(Context, Object->GetClass()->GetOutermost()->GetFName());
		AddName(Context, Object->GetClass()->GetFName());
		AddImport(Context, Object->GetOuter());
	}

	/** An object an export references: an import when it lives in another package and is not transient. */
	void AddReference(FSaveContext& Context, UObject* Object)
	{
		if (!Object || Context.ExportSet.Contains(Object) || Object == Context.Package ||
			Object->IsIn(Context.Package) || IsTransientForSave(Object) || IsEditorOnlyForSave(Context, Object))
		{
			// An export, or something saved as null: an object of the package that is not saved, a transient one, an
			// editor-only one in a filtered package.
			return;
		}
		AddImport(Context, Object);
	}

	/**
	 * Writes nothing and tracks the position, so tagged properties can patch their sizes as in the real save (UE: the
	 * FArchiveSaveTag* archives).
	 */
	class FArchiveSaveCounter : public FArchive
	{
	public:
		using FArchive::operator<<;

		FArchiveSaveCounter(FSaveContext& InContext, bool bFilterEditorOnly)
			: Context(InContext)
		{
			SetIsSaving(true);
			SetIsPersistent(true);
			SetFilterEditorOnly(bFilterEditorOnly);
		}

		virtual void Serialize(void* V, int64 Length) override
		{
			(void)V;
			Pos += Length;
			End = FMath::Max(End, Pos);
		}

		virtual int64 Tell() override
		{
			return Pos;
		}

		virtual int64 TotalSize() override
		{
			return End;
		}

		virtual void Seek(int64 InPos) override
		{
			Pos = InPos;
		}

		/** Starts over for the next export. */
		void Restart()
		{
			Pos = 0;
			End = 0;
		}

	protected:
		/** Advances like the linker's FName / FPackageIndex. */
		void Skip(int64 Length)
		{
			Serialize(nullptr, Length);
		}

		FSaveContext& Context;

	private:
		int64 Pos = 0;
		int64 End = 0;
	};

	/** The first pass: the objects of the package the exports reference become exports too (UE). */
	class FArchiveSaveTagExports final : public FArchiveSaveCounter
	{
	public:
		using FArchiveSaveCounter::FArchiveSaveCounter;
		using FArchive::operator<<;

		virtual FArchive& operator<<(FName& Name) override
		{
			(void)Name;
			Skip(8);
			return *this;
		}

		virtual FArchive& operator<<(UObject*& Object) override
		{
			MarkExport(Context, Object);
			Skip(4);
			return *this;
		}

		virtual FString GetArchiveName() const override
		{
			return FString(TEXT("FArchiveSaveTagExports"));
		}
	};

	/** The second pass: the names and imports the exports serialize (UE). */
	class FArchiveSaveTagImports final : public FArchiveSaveCounter
	{
	public:
		using FArchiveSaveCounter::FArchiveSaveCounter;
		using FArchive::operator<<;

		virtual FArchive& operator<<(FName& Name) override
		{
			AddName(Context, Name);
			Skip(8);
			return *this;
		}

		virtual FArchive& operator<<(UObject*& Object) override
		{
			AddReference(Context, Object);
			Skip(4);
			return *this;
		}

		virtual FString GetArchiveName() const override
		{
			return FString(TEXT("FArchiveSaveTagImports"));
		}
	};

	/** Sorts by a string key, ignoring case first, deterministically (D13). */
	template <typename ElementType, typename KeyFunctionType>
	void SortByString(TArray<ElementType>& Elements, KeyFunctionType KeyFunction)
	{
		TArray<TPair<FString, ElementType>> Keyed;
		Keyed.Reserve(Elements.Num());
		for (const ElementType& Element : Elements)
		{
			Keyed.Emplace(KeyFunction(Element), Element);
		}
		Keyed.Sort(
			[](const TPair<FString, ElementType>& A, const TPair<FString, ElementType>& B)
			{
				const int32 IgnoringCase = A.Key.Compare(B.Key, ESearchCase::IgnoreCase);
				return IgnoringCase != 0 ? IgnoringCase < 0 : A.Key.Compare(B.Key, ESearchCase::CaseSensitive) < 0;
			});
		for (int32 Index = 0; Index < Keyed.Num(); ++Index)
		{
			Elements[Index] = Keyed[Index].Value;
		}
	}

	/** Reports a save failure: an error (a warning with SAVE_NoError), also on Error when given. */
	FSavePackageResultStruct ReportSaveError(FOutputDevice* Error, uint32 SaveFlags, const FString& Message)
	{
		if (SaveFlags & SAVE_NoError)
		{
			UE_LOG(LogSavePackage, Warning, TEXT("%s"), *Message);
		}
		else
		{
			UE_LOG(LogSavePackage, Error, TEXT("%s"), *Message);
		}
		if (Error)
		{
			Error->Log(ELogVerbosity::Warning, Message);
		}
		return FSavePackageResultStruct(ESavePackageResult::Error);
	}

	/** Collects the exports, imports and names of InOuter and writes the package into OutBytes. */
	FSavePackageResultStruct SavePackageToBytes(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags,
		const TCHAR* Filename, FOutputDevice* Error, uint32 SaveFlags, const TCHAR* CookedPlatformName,
		TArray<uint8>& OutBytes)
	{
		if (!InOuter)
		{
			return ReportSaveError(Error, SaveFlags, TEXT("SavePackage: no package"));
		}
		const FString PackageName = InOuter->GetName();
		if (InOuter->HasAnyPackageFlags(PKG_CompiledIn))
		{
			return ReportSaveError(
				Error, SaveFlags, FString::Printf(TEXT("SavePackage: %s is compiled in"), *PackageName));
		}
		if (InOuter->LinkerLoad)
		{
			return ReportSaveError(
				Error, SaveFlags, FString::Printf(TEXT("SavePackage: %s is being loaded"), *PackageName));
		}
		if (Base && !Base->IsIn(InOuter))
		{
			return ReportSaveError(Error, SaveFlags,
				FString::Printf(TEXT("SavePackage: %s is not in %s"), *Base->GetFullName(), *PackageName));
		}

		// A build without editor-only data has no editor-only properties: its packages hold none (D14).
		bool bFilterEditorOnly = InOuter->HasAnyPackageFlags(uint32(PKG_FilterEditorOnly));
#if !WITH_EDITORONLY_DATA
		bFilterEditorOnly = true;
#endif
		const bool bIsMap = Filename &&
			FPaths::GetExtension(Filename, true)
				.Equals(FPackageName::GetMapPackageExtension(), ESearchCase::IgnoreCase);

		// Exports: Base, the package's objects with TopLevelFlags, and what they reference inside the package.
		FSaveContext Context;
		Context.Package = InOuter;
		Context.bFilterEditorOnly = bFilterEditorOnly;
		MarkExport(Context, Base);
		if (TopLevelFlags != RF_NoFlags)
		{
			TArray<UObject*> ObjectsInPackage;
			GetObjectsWithOuter(InOuter, ObjectsInPackage, /*bIncludeNestedObjects =*/true);
			for (UObject* Object : ObjectsInPackage)
			{
				if (Object->HasAnyFlags(TopLevelFlags))
				{
					MarkExport(Context, Object);
				}
			}
		}
		{
			FArchiveSaveTagExports TagExports(Context, bFilterEditorOnly);
			while (Context.PendingExports.Num() > 0)
			{
				UObject* Object = Context.PendingExports.Pop(false);
				TagExports.Restart();
				Object->Serialize(TagExports);
			}
		}
		SortByString(Context.Exports, [](UObject* Object) { return Object->GetPathName(); });

		// Imports, names and soft package references.
		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		TArray<FName>* PreviousCollector = ThreadContext.SoftPackageReferenceCollector;
		ThreadContext.SoftPackageReferenceCollector = &Context.SoftPackageReferences;
		{
			FArchiveSaveTagImports TagImports(Context, bFilterEditorOnly);
			for (UObject* Export : Context.Exports)
			{
				AddName(Context, Export->GetFName());
				AddImport(Context, Export->GetClass());
				TagImports.Restart();
				Export->Serialize(TagImports);
			}
		}
		ThreadContext.SoftPackageReferenceCollector = PreviousCollector;
		Context.SoftPackageReferences.Remove(InOuter->GetFName());
		SortByString(Context.SoftPackageReferences, [](FName Name) { return Name.ToString(); });
		for (FName SoftPackage : Context.SoftPackageReferences)
		{
			AddName(Context, SoftPackage);
		}
		SortByString(Context.Imports, [](UObject* Object) { return Object->GetPathName(); });
		AddName(Context, NAME_None);
		SortByString(Context.Names, [](FName Name) { return Name.GetPlainNameString(); });

		// The tables.
		FLinkerSave Linker(InOuter, Filename ? Filename : *PackageName, bFilterEditorOnly);
		for (int32 Index = 0; Index < Context.Names.Num(); ++Index)
		{
			Linker.NameIndices.Add(Context.Names[Index].GetComparisonIndex(), Index);
		}
		Linker.NameMap = Context.Names;
		for (int32 Index = 0; Index < Context.Imports.Num(); ++Index)
		{
			Linker.ObjectIndicesMap.Add(Context.Imports[Index], FPackageIndex::FromImport(Index));
		}
		for (int32 Index = 0; Index < Context.Exports.Num(); ++Index)
		{
			Linker.ObjectIndicesMap.Add(Context.Exports[Index], FPackageIndex::FromExport(Index));
		}
		for (UObject* Object : Context.Imports)
		{
			FObjectImport& Import = Linker.ImportMap.AddDefaulted_GetRef();
			Import.ClassPackage = Object->GetClass()->GetOutermost()->GetFName();
			Import.ClassName = Object->GetClass()->GetFName();
			Import.OuterIndex = Linker.MapObject(Object->GetOuter());
			Import.ObjectName = Object->GetFName();
			Import.XObject = Object;
		}
		for (UObject* Object : Context.Exports)
		{
			FObjectExport& Export = Linker.ExportMap.AddDefaulted_GetRef();
			Export.ClassIndex = Linker.MapObject(Object->GetClass());
			Export.OuterIndex = Object->GetOuter() == InOuter ? FPackageIndex() : Linker.MapObject(Object->GetOuter());
			Export.ObjectName = Object->GetFName();
			Export.ObjectFlags = Object->GetMaskedFlags(RF_Load);
			Export.bIsAsset = Object->IsAsset();
			Export.Object = Object;
		}
		Linker.SoftPackageReferenceList = Context.SoftPackageReferences;

		// The summary: only what the objects decide, nothing about the time or the machine (D13).
		FPackageFileSummary& Summary = Linker.Summary;
		Summary.Tag = PACKAGE_FILE_TAG;
		Summary.FileVersionUE = VER_LEON_LATEST;
		Summary.FileVersionLicenseeUE = VER_LEON_LATEST_LICENSEE;
		Summary.PackageFlags = (InOuter->GetPackageFlags() & ~uint32(PKG_NewlyCreated)) |
			(bFilterEditorOnly ? uint32(PKG_FilterEditorOnly) : 0u) | (bIsMap ? uint32(PKG_ContainsMap) : 0u);
		Summary.Guid = ((SaveFlags & SAVE_KeepGUID) && InOuter->GetGuid().IsValid())
			? InOuter->GetGuid()
			: FGuid::NewDeterministicGuid(PackageName);
		Summary.SavedByEngineVersion = FEngineVersion::Current();
		// A cooked package records its target platform (the cook's), else the platform that saved it.
		Summary.CookedPlatform = FString();
		if (InOuter->HasAnyPackageFlags(PKG_Cooked))
		{
			Summary.CookedPlatform = CookedPlatformName != nullptr && *CookedPlatformName != 0
				? FString(CookedPlatformName)
				: FString(FPlatformProperties::PlatformName());
		}
		Summary.NameCount = Linker.NameMap.Num();
		Summary.ImportCount = Linker.ImportMap.Num();
		Summary.ExportCount = Linker.ExportMap.Num();
		Summary.SoftPackageReferencesCount = Linker.SoftPackageReferenceList.Num();

		Linker << Summary;
		Summary.NameOffset = int32(Linker.Tell());
		for (const FName& Name : Linker.NameMap)
		{
			FString NameString = Name.GetPlainNameString();
			Linker << NameString;
		}
		Summary.ImportOffset = int32(Linker.Tell());
		for (FObjectImport& Import : Linker.ImportMap)
		{
			Linker << Import;
		}
		Summary.ExportOffset = int32(Linker.Tell());
		for (FObjectExport& Export : Linker.ExportMap)
		{
			Linker << Export;
		}
		Summary.SoftPackageReferencesOffset = int32(Linker.Tell());
		for (FName& SoftPackage : Linker.SoftPackageReferenceList)
		{
			Linker << SoftPackage;
		}
		Summary.TotalHeaderSize = int32(Linker.Tell());

		// The export data: tagged properties and the native tail of each object.
		for (FObjectExport& Export : Linker.ExportMap)
		{
			Export.SerialOffset = Linker.Tell();
			Export.Object->Serialize(Linker);
			Export.SerialSize = Linker.Tell() - Export.SerialOffset;
		}

		// The bulk data payloads, after every export (D13); their offsets are relative to BulkDataStartOffset.
		Summary.BulkDataStartOffset = Linker.Tell();
		for (const FLinkerSave::FBulkDataStorageInfo& Info : Linker.BulkDataToAppend)
		{
			int64 OffsetInFile = Linker.Tell() - Summary.BulkDataStartOffset;
			const int64 Size = Info.BulkData->GetBulkDataSize();
			const void* Payload = Info.BulkData->LockReadOnly();
			Linker.Serialize(const_cast<void*>(Payload), Size);
			Info.BulkData->Unlock();
			const int64 PayloadEnd = Linker.Tell();
			Linker.Seek(Info.BulkDataOffsetPos);
			Linker << OffsetInFile;
			Linker.Seek(PayloadEnd);
		}

		// The tag again: a loader detects a truncated file (UE).
		uint32 EndTag = PACKAGE_FILE_TAG;
		Linker << EndTag;
		const int64 FileSize = Linker.Tell();

		// The summary and the export table now that the offsets are known; both have a fixed size.
		Linker.Seek(0);
		Linker << Summary;
		Linker.Seek(Summary.ExportOffset);
		for (FObjectExport& Export : Linker.ExportMap)
		{
			Linker << Export;
		}
		Linker.Seek(FileSize);
		if (Linker.IsError())
		{
			return ReportSaveError(
				Error, SaveFlags, FString::Printf(TEXT("SavePackage: %s could not be serialized"), *PackageName));
		}

		OutBytes = MoveTemp(Linker.GetBytes());
		InOuter->SetGuid(Summary.Guid);
		InOuter->ClearPackageFlags(PKG_NewlyCreated);
		return FSavePackageResultStruct(ESavePackageResult::Success, FileSize);
	}
} // namespace

FSavePackageResultStruct UPackage::Save(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags,
	const TCHAR* Filename, FOutputDevice* Error, uint32 SaveFlags, const TCHAR* CookedPlatformName)
{
	if (!Filename || !*Filename)
	{
		return ReportSaveError(Error, SaveFlags, TEXT("SavePackage: no file name"));
	}
	TArray<uint8> Bytes;
	FSavePackageResultStruct Result =
		SavePackageToBytes(InOuter, Base, TopLevelFlags, Filename, Error, SaveFlags, CookedPlatformName, Bytes);
	if (!Result.IsSuccessful())
	{
		return Result;
	}
	if (!FFileHelper::SaveArrayToFile(Bytes, Filename))
	{
		return ReportSaveError(Error, SaveFlags,
			FString::Printf(TEXT("SavePackage: cannot write %s to %s"), *InOuter->GetName(), Filename));
	}
	UE_LOG(LogSavePackage, Verbose, TEXT("Saved %s to %s (%d bytes)"), *InOuter->GetName(), Filename, Bytes.Num());
	return Result;
}

bool UPackage::SavePackage(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename,
	FOutputDevice* Error, uint32 SaveFlags, const TCHAR* CookedPlatformName)
{
	return Save(InOuter, Base, TopLevelFlags, Filename, Error, SaveFlags, CookedPlatformName).IsSuccessful();
}

FSavePackageResultStruct UPackage::SaveToMemory(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags,
	TArray<uint8>& OutPackageData, FOutputDevice* Error, uint32 SaveFlags, const TCHAR* CookedPlatformName)
{
	return SavePackageToBytes(
		InOuter, Base, TopLevelFlags, nullptr, Error, SaveFlags, CookedPlatformName, OutPackageData);
}
