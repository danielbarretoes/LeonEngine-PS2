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

UObject* FWeakObjectPtr::Get() const
{
	if (ObjectSerialNumber == 0)
	{
		return nullptr;
	}
	const FUObjectItem* Item = GUObjectArray.IndexToObject(ObjectIndex);
	if (!Item || !Item->Object || Item->SerialNumber != ObjectSerialNumber ||
		Item->HasAnyFlags(EInternalObjectFlags::Unreachable | EInternalObjectFlags::PendingKill))
	{
		return nullptr;
	}
	return (UObject*)Item->Object;
}

bool FWeakObjectPtr::IsValid() const
{
	return Get() != nullptr;
}

bool FWeakObjectPtr::IsStale() const
{
	return ObjectSerialNumber != 0 && !IsValid();
}
