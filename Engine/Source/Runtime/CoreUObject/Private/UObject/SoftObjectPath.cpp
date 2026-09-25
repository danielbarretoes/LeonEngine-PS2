#include "UObject/SoftObjectPath.h"

#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"

FSoftObjectPath::FSoftObjectPath(const UObject* Object)
{
	if (Object)
	{
		SetPath(Object->GetPathName());
	}
}

FString FSoftObjectPath::ToString() const
{
	if (IsNull())
	{
		return FString();
	}
	FString Result = AssetPathName.ToString();
	if (SubPathString.Len() > 0)
	{
		Result += SUBOBJECT_DELIMITER;
		Result += SubPathString;
	}
	return Result;
}

void FSoftObjectPath::SetPath(const FString& Path)
{
	if (Path.Len() == 0 || Path == TEXT("None"))
	{
		Reset();
		return;
	}
	// "Package.Asset:Sub.Path": the asset path is up to the first ':' (UE).
	int32 ColonIndex = INDEX_NONE;
	if (Path.FindChar(':', ColonIndex))
	{
		AssetPathName = FName(*Path.Left(ColonIndex));
		SubPathString = Path.RightChop(ColonIndex + 1);
	}
	else
	{
		AssetPathName = FName(*Path);
		SubPathString.Empty();
	}
}

UObject* FSoftObjectPath::ResolveObject() const
{
	if (IsNull())
	{
		return nullptr;
	}
	return StaticFindObject(nullptr, nullptr, *ToString());
}

void FSoftObjectPtr::operator=(const UObject* Object)
{
	WeakPtr = Object;
	ObjectID = FSoftObjectPath(Object);
}

UObject* FSoftObjectPtr::Get() const
{
	if (UObject* Object = WeakPtr.Get())
	{
		return Object;
	}
	UObject* Object = ObjectID.ResolveObject();
	if (Object)
	{
		WeakPtr = Object;
	}
	return Object;
}
