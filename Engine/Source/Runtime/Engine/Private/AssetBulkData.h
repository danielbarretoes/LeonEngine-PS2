#pragma once

#include "CoreMinimal.h"
#include "Serialization/BulkData.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

/**
 * Saves or loads the CPU arrays of an asset as one bulk data payload (Leon: the meshes and clips keep their data in
 * arrays and use bulk data only in packages). While saving, SerializePayload writes the arrays into BulkData, a member
 * of the asset (the package saver writes the payload after the exports, so it must outlive Serialize); while loading
 * it reads them back from the payload, which is then freed. False when the payload is damaged (the archive gets an
 * error).
 */
template <typename PayloadFunctionType>
bool SerializeBulkPayload(FArchive& Ar, UObject* Owner, FByteBulkData& BulkData, PayloadFunctionType&& SerializePayload)
{
	if (Ar.IsSaving())
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, /*bIsPersistent =*/true);
		SerializePayload(Writer);
		(void)BulkData.Lock(LOCK_READ_WRITE);
		void* Data = BulkData.Realloc(Bytes.Num());
		if (Bytes.Num() > 0)
		{
			FMemory::Memcpy(Data, Bytes.GetData(), static_cast<SIZE_T>(Bytes.Num()));
		}
		BulkData.Unlock();
	}
	BulkData.Serialize(Ar, Owner);
	if (!Ar.IsLoading())
	{
		return true;
	}
	if (Ar.IsError())
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.AddUninitialized(static_cast<int32>(BulkData.GetBulkDataSize()));
	if (Bytes.Num() > 0)
	{
		FMemory::Memcpy(Bytes.GetData(), BulkData.LockReadOnly(), static_cast<SIZE_T>(Bytes.Num()));
		BulkData.Unlock();
	}
	BulkData.RemoveBulkData();
	FMemoryReader Reader(Bytes, /*bIsPersistent =*/true);
	SerializePayload(Reader);
	if (Reader.IsError() || Reader.Tell() != Bytes.Num())
	{
		Ar.SetError();
		return false;
	}
	return true;
}
