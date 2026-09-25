#include "UObject/UObjectArray.h"

#include "UObject/UObjectBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectArray, Log, All);

FUObjectArray GUObjectArray;

FUObjectArray::FUObjectArray()
	: Objects(nullptr)
	, MaxObjects(0)
	, ObjLastNonGCIndex(0)
	, MasterSerialNumber(0)
{
}

FUObjectArray::~FUObjectArray()
{
	// The slots are released with the process: objects may still be referenced by other static objects while the
	// program exits (objects are only destroyed by the P10 garbage collector).
}

void FUObjectArray::AllocateObjectPool(int32 InMaxUObjects)
{
	checkf(Objects == nullptr, "The UObject array is already allocated");
	checkf(InMaxUObjects > 0, "Invalid UObject array capacity %d", InMaxUObjects);
	MaxObjects = InMaxUObjects;
	Objects = (FUObjectItem*)FMemory::Malloc(SIZE_T(MaxObjects) * sizeof(FUObjectItem));
	FMemory::Memzero(Objects, SIZE_T(MaxObjects) * sizeof(FUObjectItem));
	UE_LOG(LogUObjectArray, Log, TEXT("UObject array: %d objects, %d bytes"), MaxObjects,
		int32(SIZE_T(MaxObjects) * sizeof(FUObjectItem)));
}

void FUObjectArray::AllocateUObjectIndex(UObjectBase* Object)
{
	check(Objects != nullptr);
	check(Object->InternalIndex == INDEX_NONE);
	int32 Index;
	if (ObjAvailableList.Num() > 0)
	{
		Index = ObjAvailableList.Pop(false);
	}
	else
	{
		if (ObjLastNonGCIndex >= MaxObjects)
		{
			UE_LOG(LogUObjectArray, Fatal,
				TEXT("Maximum number of UObjects (%d) exceeded (MaxObjectsInGame); the array holds %d bytes"),
				MaxObjects, int32(GetAllocatedSize()));
		}
		Index = ObjLastNonGCIndex++;
	}
	FUObjectItem& Item = Objects[Index];
	checkf(Item.Object == nullptr, "UObject array slot %d is in use", Index);
	Item.Object = Object;
	Item.Flags = 0;
	Object->InternalIndex = Index;
}

void FUObjectArray::FreeUObjectIndex(UObjectBase* Object)
{
	const int32 Index = Object->InternalIndex;
	FUObjectItem* Item = IndexToObject(Index);
	check(Item && Item->Object == Object);
	Item->Object = nullptr;
	Item->Flags = 0;
	// A new serial number is allocated for the next object that gets a weak pointer, so stale weak pointers fail.
	Item->SerialNumber = 0;
	ObjAvailableList.Add(Index);
	Object->InternalIndex = INDEX_NONE;
}

FUObjectItem* FUObjectArray::ObjectToObjectItem(const UObjectBase* Object)
{
	return Object ? IndexToObject(Object->InternalIndex) : nullptr;
}

int32 FUObjectArray::ObjectToIndex(const UObjectBase* Object) const
{
	return Object ? Object->InternalIndex : INDEX_NONE;
}

bool FUObjectArray::IsValid(const UObjectBase* Object) const
{
	const FUObjectItem* Item = Object ? IndexToObject(Object->InternalIndex) : nullptr;
	return Item && Item->Object == Object;
}

int32 FUObjectArray::AllocateSerialNumber(int32 Index)
{
	FUObjectItem* Item = IndexToObject(Index);
	checkf(Item && Item->Object, "No object in slot %d", Index);
	if (Item->SerialNumber == 0)
	{
		Item->SerialNumber = ++MasterSerialNumber;
	}
	return Item->SerialNumber;
}
