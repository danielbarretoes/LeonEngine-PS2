#include "UObject/WeakObjectPtr.h"

#include "UObject/Object.h"
#include "UObject/UObjectArray.h"

void FWeakObjectPtr::operator=(const UObject* Object)
{
	if (Object && GUObjectArray.IsValid(Object))
	{
		ObjectIndex = int32(Object->GetUniqueID());
		ObjectSerialNumber = GUObjectArray.AllocateSerialNumber(ObjectIndex);
	}
	else
	{
		Reset();
	}
}

const FUObjectItem* FWeakObjectPtr::Internal_GetObjectItem() const
{
	if (ObjectSerialNumber == 0)
	{
		return nullptr;
	}
	const FUObjectItem* Item = GUObjectArray.IndexToObject(ObjectIndex);
	if (!Item || !Item->Object || Item->SerialNumber != ObjectSerialNumber)
	{
		return nullptr;
	}
	return Item;
}

UObject* FWeakObjectPtr::Get() const
{
	return Get(false);
}

UObject* FWeakObjectPtr::Get(bool bEvenIfPendingKill) const
{
	const FUObjectItem* Item = Internal_GetObjectItem();
	// An object the garbage collector found unreachable is gone even before its memory is freed (UE).
	if (!Item || Item->HasAnyFlags(EInternalObjectFlags::Unreachable) ||
		(!bEvenIfPendingKill && Item->HasAnyFlags(EInternalObjectFlags::PendingKill)))
	{
		return nullptr;
	}
	return (UObject*)Item->Object;
}

bool FWeakObjectPtr::IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest) const
{
	(void)bThreadsafeTest;
	return Get(bEvenIfPendingKill) != nullptr;
}

bool FWeakObjectPtr::IsStale(bool bIncludingIfPendingKill, bool bThreadsafeTest) const
{
	(void)bThreadsafeTest;
	return ObjectSerialNumber != 0 && !IsValid(!bIncludingIfPendingKill);
}
