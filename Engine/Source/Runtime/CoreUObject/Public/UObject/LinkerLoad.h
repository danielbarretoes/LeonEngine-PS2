#pragma once

// The package loader (UE: UObject/LinkerLoad.h).

#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "UObject/Linker.h"

class FMemStackBase;
class UClass;
class UObject;
class UPackage;

/**
 * Loads one package file (UE: FLinkerLoad). It reads the whole file into memory, then the summary and the tables;
 * LoadAllObjects creates every export with StaticConstructObject (its class and outer first, so the class defaults
 * and the default subobjects exist) and serializes its tagged properties and native data from the file. Object
 * references read as FPackageIndex resolve to the linker's exports or to imports, which are found in memory or load
 * their package first. Loading is synchronous; UObject::PostLoad runs once every export of the outermost LoadPackage
 * call (dependencies included) is serialized. The linker is then detached and deleted (Leon: all data, bulk data
 * included, is loaded eagerly, so nothing needs it afterwards).
 *
 * The file's bytes live in the load arena (GetLoadArena; Docs/PLANS/ps2-shipping.md N17), not on the heap one by one:
 * LoadPackage marks the arena before it creates the linker and pops it once the exports are serialized
 * (ReleasePackageData), so a package's bytes go before its importer's, and a tables-only linker gives its bytes back
 * before CreateLinker returns. When the outermost load ends, the arena returns its chunks to GMalloc: what a map's
 * load read is freed as a block.
 */
class COREUOBJECT_API FLinkerLoad
	: public FLinker
	, public FArchive
{
public:
	using FArchive::operator<<;

	/** ELoadFlags of the load. */
	uint32 LoadFlags;

	/**
	 * Reads the package file Filename (through IFileManager) and its tables and attaches the linker to Parent: its
	 * flags and GUID come from the summary. Returns nullptr, with an error logged, when the file cannot be read or is
	 * not a valid package (UE: CreateLinker). A null Parent reads the tables only (the cooker's dependency walk and the
	 * tests; UE: FPackageReader).
	 */
	static FLinkerLoad* CreateLinker(UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags);

	/** CreateLinker over package bytes already in memory (Leon: the in-memory packages below). */
	static FLinkerLoad* CreateLinkerFromMemory(
		UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags, const TArray<uint8>& PackageData);

	/**
	 * The packages a package file's imports name, /Script ones left out, from its bytes (Leon: what an asynchronous
	 * load reads next; UE's FAsyncPackage reads them from the summary too). False, logged, for bytes that are no
	 * package.
	 */
	static bool GetImportedPackageNames(
		const TCHAR* Filename, const uint8* Bytes, int64 Size, TArray<FName>& OutPackageNames);

	/** The linker loading Package right now, or nullptr (UE: FindExistingLinkerForPackage). */
	static FLinkerLoad* FindExistingLinkerForPackage(const UPackage* Package);

	virtual ~FLinkerLoad() override;

	/** Creates and serializes every export (UE: LoadAllObjects). */
	void LoadAllObjects();

	/**
	 * The object of export Index, created on first use: its class (an import) and outer are resolved, an object of
	 * that name already in the outer (a default subobject the outer's constructor made) is reused, otherwise it is
	 * constructed with RF_NeedLoad | RF_NeedPostLoad. Null when the class or outer is missing (warned) (UE:
	 * CreateExport).
	 */
	UObject* CreateExport(int32 Index);

	/**
	 * The object of import Index, resolved on first use: a package import is the package, loaded first unless it is a
	 * /Script package or already loaded; another import is found in its outer. A missing import is a warning and
	 * resolves to null (UE: CreateImport).
	 */
	UObject* CreateImport(int32 Index);

	/** Serializes an export's object from the file if it still has RF_NeedLoad (UE: Preload). */
	void Preload(UObject* Object);

	/** Forgets the objects and releases the package (UE: Detach). */
	void Detach();

	/**
	 * Forgets the package's bytes once every export is serialized (Leon): the load arena's mark that holds them pops
	 * next. The tables stay.
	 */
	void ReleasePackageData();

	/**
	 * The arena of the package bytes being loaded (Leon): 64 KB chunks of GMalloc, charged to ELLMTag::LoadMapMisc,
	 * given back when the outermost load ends.
	 */
	static FMemStackBase& GetLoadArena();

	// FArchive
	virtual void Serialize(void* V, int64 Length) override;
	virtual int64 Tell() override;
	virtual int64 TotalSize() override;
	virtual void Seek(int64 InPos) override;
	virtual FArchive& operator<<(FName& Name) override;
	virtual FArchive& operator<<(UObject*& Object) override;
	virtual FLinker* GetLinker() override;
	virtual FString GetArchiveName() const override;

	// In-memory packages (Leon).

	/**
	 * Registers package bytes under a long package name: LoadPackage and FPackageName::DoesPackageExist use them
	 * instead of a file. For the tests, and for TestPAL on the PS2, whose platform file is read-only.
	 */
	static void RegisterInMemoryPackage(const FString& LongPackageName, const TArray<uint8>& PackageData);

	/** Forgets bytes registered with RegisterInMemoryPackage; false when none were. */
	static bool UnregisterInMemoryPackage(const FString& LongPackageName);

	/** The registered bytes of a package, or nullptr. */
	static const TArray<uint8>* FindInMemoryPackage(const FString& LongPackageName);

private:
	FLinkerLoad(UPackage* InParent, const TCHAR* InFilename, uint32 InLoadFlags);

	/** CreateLinker once the bytes are in the load arena. */
	static FLinkerLoad* CreateLinkerFromBytes(
		UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags, const uint8* Bytes, int64 Size);

	/** Reads the file into the load arena and creates the linker; nullptr (logged) when it cannot be read. */
	static FLinkerLoad* CreateLinkerFromFile(UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags);

	/** Reads the summary and the tables from PackageData; false (logged) when the data is not a valid package. */
	bool ReadTables();

	/** True when Index is null or names an entry of the tables. */
	bool IsValidIndex(FPackageIndex Index) const;

	/** The object export Index is created in: the package, or another export's object. */
	UObject* GetExportOuter(int32 Index);

	/** The object of a table index: null, an export (CreateExport) or an import (CreateImport). */
	UObject* IndexToObject(FPackageIndex Index);

	/** Serializes export Index into its object. */
	void PreloadExport(int32 Index);

	/** The class an import's ClassPackage / ClassName name, or nullptr. */
	static UClass* FindImportClass(const FObjectImport& Import);

	/** The file's bytes in the load arena (nullptr once released). */
	const uint8* PackageBytes = nullptr;
	int64 PackageSize = 0;
	int64 Pos = 0;
	/** The export of each created object, for Preload. */
	TMap<UObject*, int32> ObjectToExportIndex;
};

/**
 * Starts a load: the PostLoad calls and the linker releases wait until the matching outermost EndLoad (UE: BeginLoad).
 * LoadPackage calls both.
 */
COREUOBJECT_API void BeginLoad();

/**
 * Ends a load. The outermost EndLoad calls ConditionalPostLoad on every object loaded since the outermost BeginLoad,
 * package by package in the order their loads finished (a package's imports first), each in export order; then it
 * marks the packages fully loaded and deletes their linkers (UE: EndLoad).
 */
COREUOBJECT_API void EndLoad();

/** True between the outermost BeginLoad and EndLoad (UE: IsLoading). */
COREUOBJECT_API bool IsLoading();
