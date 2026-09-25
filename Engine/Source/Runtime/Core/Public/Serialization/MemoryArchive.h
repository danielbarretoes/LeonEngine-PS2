#pragma once

#include "CoreTypes.h"
#include "Serialization/Archive.h"

/** Base for archives over memory: a byte offset that Seek / Tell move (UE: FMemoryArchive). */
class CORE_API FMemoryArchive : public FArchive
{
public:
	virtual FString GetArchiveName() const override
	{
		return FString("FMemoryArchive");
	}

	virtual void Seek(int64 InPos) override
	{
		Offset = InPos;
	}

	virtual int64 Tell() override
	{
		return Offset;
	}

protected:
	FMemoryArchive() = default;

	int64 Offset = 0;
};
