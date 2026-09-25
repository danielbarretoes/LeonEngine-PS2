#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "Serialization/MemoryWriter.h"

/** A memory writer that owns its bytes: it is the TArray<uint8> it writes to (UE: FBufferArchive). */
class FBufferArchive
	: public FMemoryWriter
	, public TArray<uint8>
{
public:
	explicit FBufferArchive(bool bIsPersistent = false)
		: FMemoryWriter(static_cast<TArray<uint8>&>(*this), bIsPersistent)
	{
	}

	virtual FString GetArchiveName() const override
	{
		return FString("FBufferArchive");
	}
};
