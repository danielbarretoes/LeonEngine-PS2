#pragma once

// Object lookup by name and outer, and object enumeration by outer or class (UE: UObject/UObjectHash.h).

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class UClass;
class UObject;
class UObjectBase;

/** Adds an object to the name hash (UE: HashObject). */
COREUOBJECT_API void HashObject(UObjectBase* Object);

/** Removes an object from the name hash (UE: UnhashObject; LowLevelRename and the destructor use it). */
COREUOBJECT_API void UnhashObject(UObjectBase* Object);

/**
 * The object named InName in InOuter (any outer with bAnyPackage), of class ObjectClass or a subclass (exactly with
 * bExactClass; any class when ObjectClass is null), skipping objects with ExclusiveFlags (UE:
 * StaticFindObjectFastInternal).
 */
COREUOBJECT_API UObject* StaticFindObjectFastInternal(const UClass* ObjectClass, const UObject* InOuter, FName InName,
	bool bExactClass = false, bool bAnyPackage = false, EObjectFlags ExclusiveFlags = RF_NoFlags,
	EInternalObjectFlags ExclusiveInternalFlags = EInternalObjectFlags::None);

/** The objects whose outer is Outer (and, with bIncludeNestedObjects, their inner objects) (UE). */
COREUOBJECT_API void GetObjectsWithOuter(const UObjectBase* Outer, TArray<UObject*>& Results,
	bool bIncludeNestedObjects = true, EObjectFlags ExclusionFlags = RF_NoFlags,
	EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None);

/**
 * The objects of ClassToLookFor (and its subclasses with bIncludeDerivedClasses) (UE). Leon walks GUObjectArray
 * instead of keeping a per-class hash: fine for the object counts of a game, and it costs no memory.
 */
COREUOBJECT_API void GetObjectsOfClass(const UClass* ClassToLookFor, TArray<UObject*>& Results,
	bool bIncludeDerivedClasses = true, EObjectFlags ExcludeFlags = RF_ClassDefaultObject,
	EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None);

/** The classes that derive from ClassToLookFor (recursively with bRecursive) (UE). */
COREUOBJECT_API void GetDerivedClasses(const UClass* ClassToLookFor, TArray<UClass*>& Results, bool bRecursive = true);
