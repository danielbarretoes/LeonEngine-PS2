#include "UObject/UObjectHash.h"

#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectArray.h"

namespace
{
	/**
	 * Objects by the hash of their name (UE: FUObjectHashTables::Hash). A lookup walks the objects that share the
	 * name's hash and filters them by name and outer. Grows with the object count instead of reserving for the
	 * capacity.
	 */
	TMultiMap<uint32, UObjectBase*>& GetNameHash()
	{
		static TMultiMap<uint32, UObjectBase*> NameHash;
		return NameHash;
	}

	bool MatchesClass(const UObject* Object, const UClass* ObjectClass, bool bExactClass)
	{
		if (!ObjectClass)
		{
			return true;
		}
		return bExactClass ? Object->GetClass() == ObjectClass : Object->IsA(ObjectClass);
	}
} // namespace

void HashObject(UObjectBase* Object)
{
	const FName Name = Object->GetFName();
	if (!Name.IsNone())
	{
		GetNameHash().Add(GetTypeHash(Name), Object);
	}
}

void UnhashObject(UObjectBase* Object)
{
	const FName Name = Object->GetFName();
	if (!Name.IsNone())
	{
		GetNameHash().RemoveSingle(GetTypeHash(Name), Object);
	}
}

UObject* StaticFindObjectFastInternal(const UClass* ObjectClass, const UObject* InOuter, FName InName, bool bExactClass,
	bool bAnyPackage, EObjectFlags ExclusiveFlags, EInternalObjectFlags ExclusiveInternalFlags)
{
	if (InName.IsNone())
	{
		return nullptr;
	}
	ExclusiveInternalFlags |= EInternalObjectFlags::Unreachable;
	for (TMultiMap<uint32, UObjectBase*>::TConstKeyIterator It =
			 GetNameHash().CreateConstKeyIterator(GetTypeHash(InName));
		It; ++It)
	{
		UObject* Object = (UObject*)It.Value();
		if (Object->GetFName() != InName || (!bAnyPackage && Object->GetOuter() != InOuter))
		{
			continue;
		}
		if (Object->HasAnyFlags(ExclusiveFlags) || Object->HasAnyInternalFlags(ExclusiveInternalFlags))
		{
			continue;
		}
		if (MatchesClass(Object, ObjectClass, bExactClass))
		{
			return Object;
		}
	}
	return nullptr;
}

void GetObjectsWithOuter(const UObjectBase* Outer, TArray<UObject*>& Results, bool bIncludeNestedObjects,
	EObjectFlags ExclusionFlags, EInternalObjectFlags ExclusionInternalFlags)
{
	ExclusionInternalFlags |= EInternalObjectFlags::Unreachable;
	for (int32 Index = 0; Index < GUObjectArray.GetObjectArrayNum(); ++Index)
	{
		FUObjectItem* Item = GUObjectArray.IndexToObject(Index);
		UObject* Object = Item ? (UObject*)Item->Object : nullptr;
		if (!Object || Item->HasAnyFlags(ExclusionInternalFlags) || Object->HasAnyFlags(ExclusionFlags))
		{
			continue;
		}
		const bool bDirect = Object->GetOuter() == Outer;
		if (bDirect || (bIncludeNestedObjects && Object->IsIn((const UObject*)Outer)))
		{
			Results.Add(Object);
		}
	}
}

void GetObjectsOfClass(const UClass* ClassToLookFor, TArray<UObject*>& Results, bool bIncludeDerivedClasses,
	EObjectFlags ExcludeFlags, EInternalObjectFlags ExclusionInternalFlags)
{
	ExclusionInternalFlags |= EInternalObjectFlags::Unreachable;
	for (int32 Index = 0; Index < GUObjectArray.GetObjectArrayNum(); ++Index)
	{
		FUObjectItem* Item = GUObjectArray.IndexToObject(Index);
		UObject* Object = Item ? (UObject*)Item->Object : nullptr;
		if (!Object || !Object->GetClass() || Item->HasAnyFlags(ExclusionInternalFlags) ||
			Object->HasAnyFlags(ExcludeFlags))
		{
			continue;
		}
		if (bIncludeDerivedClasses ? Object->IsA(ClassToLookFor) : Object->GetClass() == ClassToLookFor)
		{
			Results.Add(Object);
		}
	}
}

void GetDerivedClasses(const UClass* ClassToLookFor, TArray<UClass*>& Results, bool bRecursive)
{
	TArray<UObject*> Classes;
	GetObjectsOfClass(UClass::StaticClass(), Classes);
	for (UObject* Object : Classes)
	{
		UClass* Class = (UClass*)Object;
		if (Class == ClassToLookFor)
		{
			continue;
		}
		if (bRecursive ? Class->IsChildOf(ClassToLookFor) : Class->GetSuperClass() == ClassToLookFor)
		{
			Results.Add(Class);
		}
	}
}
