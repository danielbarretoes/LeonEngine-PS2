#pragma once

#include "CoreTypes.h"
#include "Templates/TypeHash.h"

/** Identifies a delegate binding (UE: FDelegateHandle); Remove() on multicast delegates takes it. */
class CORE_API FDelegateHandle
{
public:
	enum EGenerateNewHandleType
	{
		GenerateNewHandle
	};

	FDelegateHandle()
		: ID(0)
	{
	}

	explicit FDelegateHandle(EGenerateNewHandleType)
		: ID(GenerateNewID())
	{
	}

	bool IsValid() const
	{
		return ID != 0;
	}

	void Reset()
	{
		ID = 0;
	}

	friend bool operator==(const FDelegateHandle& Lhs, const FDelegateHandle& Rhs)
	{
		return Lhs.ID == Rhs.ID;
	}

	friend bool operator!=(const FDelegateHandle& Lhs, const FDelegateHandle& Rhs)
	{
		return Lhs.ID != Rhs.ID;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FDelegateHandle& Key)
	{
		return GetTypeHash(Key.ID);
	}

private:
	static uint64 GenerateNewID();

	uint64 ID;
};

/** Base interface of a delegate binding (UE: IDelegateInstance). */
class IDelegateInstance
{
public:
	virtual ~IDelegateInstance() = default;

	/** False when the bound object is gone (weak / shared-pointer bindings). */
	virtual bool IsSafeToExecute() const = 0;

	/** True when bound to a member function of InUserObject. */
	virtual bool HasSameObject(const void* InUserObject) const = 0;

	/** The bound object, or nullptr for static / lambda bindings. */
	virtual const void* GetObjectForTimerManager() const = 0;

	virtual FDelegateHandle GetHandle() const = 0;
};
