#pragma once

// The package saver (UE: UObject/LinkerSave.h).

#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "UObject/Linker.h"

class FByteBulkData;
class UObject;
class UPackage;

/**
 * Writes one package into memory (UE: FLinkerSave). UPackage::Save fills its tables, then serializes each export
 * through it: FName becomes a name table index plus its number, UObject* the FPackageIndex of its export or import
 * (null for anything not in the tables: transient objects), and FByteBulkData payloads are queued for the end of the
 * file. The caller writes the bytes to the file.
 */
class COREUOBJECT_API FLinkerSave
	: public FLinker
	, public FArchive
{
public:
	using FArchive::operator<<;

	FLinkerSave(UPackage* InParent, const TCHAR* InFilename, bool bInFilterEditorOnly);
	virtual ~FLinkerSave() override;

	/** The table index of every export and import (UE: ObjectIndicesMap). */
	TMap<UObject*, FPackageIndex> ObjectIndicesMap;

	/** The name table index of every name entry (UE: NameIndices). */
	TMap<FNameEntryId, int32> NameIndices;

	/** A bulk data payload waiting for the end of the file (UE: FLinkerSave::FBulkDataStorageInfo). */
	struct FBulkDataStorageInfo
	{
		/** Where the payload's offset (relative to BulkDataStartOffset) is to be written. */
		int64 BulkDataOffsetPos = 0;
		/** The payload. */
		const FByteBulkData* BulkData = nullptr;
	};

	/** The end-of-file payloads, in the order the exports serialized them (UE: BulkDataToAppend). */
	TArray<FBulkDataStorageInfo> BulkDataToAppend;

	/** The export or import of Object, null when it has none (UE: MapObject). */
	FPackageIndex MapObject(const UObject* Object) const;

	/** The bytes written so far. */
	FORCEINLINE const TArray<uint8>& GetBytes() const
	{
		return Bytes;
	}

	FORCEINLINE TArray<uint8>& GetBytes()
	{
		return Bytes;
	}

	// FArchive
	virtual void Serialize(void* V, int64 Length) override;
	virtual int64 Tell() override;
	virtual int64 TotalSize() override;
	virtual void Seek(int64 InPos) override;
	virtual FArchive& operator<<(FName& Name) override;
	virtual FArchive& operator<<(UObject*& Object) override;
	virtual FLinker* GetLinker() override;
	virtual FString GetArchiveName() const override;

private:
	TArray<uint8> Bytes;
	int64 Pos = 0;
};
