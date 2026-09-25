#include "Serialization/BulkData.h"

#include "UObject/LinkerLoad.h"
#include "UObject/LinkerSave.h"
#include "UObject/Object.h"

DEFINE_LOG_CATEGORY_STATIC(LogSerialization, Log, All);

FByteBulkData::FByteBulkData(const FByteBulkData& Other)
{
	*this = Other;
}

FByteBulkData& FByteBulkData::operator=(const FByteBulkData& Other)
{
	if (this != &Other)
	{
		check(!IsLocked() && !Other.IsLocked());
		RemoveBulkData();
		BulkDataFlags = Other.BulkDataFlags;
		BulkDataOffsetInFile = Other.BulkDataOffsetInFile;
		if (Other.ElementCount > 0)
		{
			Data = FMemory::Malloc(SIZE_T(Other.ElementCount));
			FMemory::Memcpy(Data, Other.Data, SIZE_T(Other.ElementCount));
			ElementCount = Other.ElementCount;
		}
	}
	return *this;
}

FByteBulkData::~FByteBulkData()
{
	checkf(!IsLocked(), "Bulk data destroyed while locked");
	RemoveBulkData();
}

void* FByteBulkData::Lock(uint32 LockFlags)
{
	checkf(!IsLocked(), "Bulk data is already locked");
	checkf(LockFlags == LOCK_READ_ONLY || LockFlags == LOCK_READ_WRITE, "Lock with LOCK_READ_ONLY or LOCK_READ_WRITE");
	LockStatus = LockFlags;
	return Data;
}

const void* FByteBulkData::LockReadOnly() const
{
	checkf(!IsLocked(), "Bulk data is already locked");
	LockStatus = LOCK_READ_ONLY;
	return Data;
}

void FByteBulkData::Unlock() const
{
	checkf(IsLocked(), "Bulk data is not locked");
	LockStatus = LOCK_NONE;
}

void* FByteBulkData::Realloc(int64 InElementCount)
{
	checkf(LockStatus == LOCK_READ_WRITE, "Realloc needs the bulk data locked with LOCK_READ_WRITE");
	checkf(InElementCount >= 0, "Negative bulk data size");
	if (InElementCount == 0)
	{
		FMemory::Free(Data);
		Data = nullptr;
	}
	else
	{
		Data = FMemory::Realloc(Data, SIZE_T(InElementCount));
	}
	ElementCount = InElementCount;
	return Data;
}

void FByteBulkData::GetCopy(void** Dest, bool bDiscardInternalCopy)
{
	check(Dest && !IsLocked());
	if (ElementCount == 0)
	{
		return;
	}
	if (!*Dest)
	{
		*Dest = FMemory::Malloc(SIZE_T(ElementCount));
	}
	FMemory::Memcpy(*Dest, Data, SIZE_T(ElementCount));
	if (bDiscardInternalCopy)
	{
		RemoveBulkData();
	}
}

void FByteBulkData::RemoveBulkData()
{
	check(!IsLocked());
	FMemory::Free(Data);
	Data = nullptr;
	ElementCount = 0;
}

void FByteBulkData::Serialize(FArchive& Ar, UObject* Owner, int32 Index)
{
	(void)Index;
	check(!IsLocked());
	FLinker* Linker = Ar.GetLinker();

	if (Ar.IsSaving())
	{
		// In a package the payload goes to the end of the file unless forced inline; elsewhere it is inline.
		const bool bEndOfFile =
			Linker && Linker->GetType() == ELinkerType::Save && !(BulkDataFlags & BULKDATA_ForceInlinePayload);
		uint32 SavedFlags = (BulkDataFlags & BULKDATA_ForceInlinePayload) | BULKDATA_Size64Bit;
		if (ElementCount == 0)
		{
			SavedFlags |= BULKDATA_Unused;
		}
		else if (bEndOfFile)
		{
			SavedFlags |= BULKDATA_PayloadAtEndOfFile;
		}
		int64 SavedCount = ElementCount;
		int64 SizeOnDisk = ElementCount;
		int64 OffsetInFile = INDEX_NONE;
		Ar << SavedFlags << SavedCount << SizeOnDisk;
		const int64 OffsetPos = Ar.Tell();
		Ar << OffsetInFile;
		if (SavedFlags & BULKDATA_PayloadAtEndOfFile)
		{
			// UPackage::Save writes the payload after the exports and patches the offset.
			FLinkerSave::FBulkDataStorageInfo& Info =
				static_cast<FLinkerSave*>(Linker)->BulkDataToAppend.AddDefaulted_GetRef();
			Info.BulkDataOffsetPos = OffsetPos;
			Info.BulkData = this;
		}
		else if (ElementCount > 0)
		{
			Ar.Serialize(Data, ElementCount);
		}
		return;
	}

	if (!Ar.IsLoading())
	{
		return;
	}
	uint32 SavedFlags = 0;
	int64 SavedCount = 0;
	int64 SizeOnDisk = 0;
	int64 OffsetInFile = INDEX_NONE;
	Ar << SavedFlags << SavedCount << SizeOnDisk << OffsetInFile;
	RemoveBulkData();
	BulkDataFlags = SavedFlags & BULKDATA_ForceInlinePayload;
	BulkDataOffsetInFile = INDEX_NONE;
	if (Ar.IsError() || SavedCount < 0 || SizeOnDisk != SavedCount)
	{
		Ar.SetCriticalError();
		return;
	}
	if ((SavedFlags & BULKDATA_Unused) || SavedCount == 0)
	{
		return;
	}

	const int64 Remaining = Ar.TotalSize() != INDEX_NONE ? Ar.TotalSize() - Ar.Tell() : SavedCount;
	if (SavedFlags & BULKDATA_PayloadAtEndOfFile)
	{
		FLinkerLoad* LinkerLoad =
			(Linker && Linker->GetType() == ELinkerType::Load) ? static_cast<FLinkerLoad*>(Linker) : nullptr;
		if (!LinkerLoad)
		{
			UE_LOG(LogSerialization, Error,
				TEXT("%s: bulk data of %s is at the end of a package file, but this is not a package"),
				*Ar.GetArchiveName(), Owner ? *Owner->GetFullName() : TEXT("an object"));
			Ar.SetCriticalError();
			return;
		}
		// Eager: read the payload now and come back (Leon has no lazy bulk data loading).
		const int64 PayloadOffset = LinkerLoad->Summary.BulkDataStartOffset + OffsetInFile;
		if (OffsetInFile < 0 || PayloadOffset + SavedCount > Ar.TotalSize())
		{
			Ar.SetCriticalError();
			return;
		}
		const int64 ReturnPos = Ar.Tell();
		Data = FMemory::Malloc(SIZE_T(SavedCount));
		ElementCount = SavedCount;
		Ar.Seek(PayloadOffset);
		Ar.Serialize(Data, SavedCount);
		Ar.Seek(ReturnPos);
		BulkDataOffsetInFile = OffsetInFile;
		return;
	}
	if (SavedCount > Remaining)
	{
		Ar.SetCriticalError();
		return;
	}
	Data = FMemory::Malloc(SIZE_T(SavedCount));
	ElementCount = SavedCount;
	Ar.Serialize(Data, SavedCount);
}
