#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBase.h"

class UClass;
class UObject;
class UPackage;

/** Delimiter between an object and its subobject in a path name: /Game/Map.Map:PersistentLevel (UE). */
#define SUBOBJECT_DELIMITER TEXT(":")

/**
 * Flag, name and outer-chain helpers built on UObjectBase (UE: UObjectBaseUtility). Nothing here is virtual.
 */
class COREUOBJECT_API UObjectBaseUtility : public UObjectBase
{
public:
	UObjectBaseUtility() = default;

	explicit UObjectBaseUtility(EObjectFlags InFlags)
		: UObjectBase(InFlags)
	{
	}

	// Flags.

	FORCEINLINE void SetFlags(EObjectFlags NewFlags)
	{
		SetFlagsTo(GetFlags() | NewFlags);
	}

	FORCEINLINE void ClearFlags(EObjectFlags NewFlags)
	{
		SetFlagsTo(GetFlags() & ~NewFlags);
	}

	FORCEINLINE bool HasAnyFlags(EObjectFlags FlagsToCheck) const
	{
		return (GetFlags() & FlagsToCheck) != 0;
	}

	FORCEINLINE bool HasAllFlags(EObjectFlags FlagsToCheck) const
	{
		return (GetFlags() & FlagsToCheck) == FlagsToCheck;
	}

	FORCEINLINE EObjectFlags GetMaskedFlags(EObjectFlags Mask = RF_AllFlags) const
	{
		return GetFlags() & Mask;
	}

	/** Flags kept in the object's GUObjectArray item. */
	EInternalObjectFlags GetInternalFlags() const;
	bool HasAnyInternalFlags(EInternalObjectFlags FlagsToCheck) const;
	void SetInternalFlags(EInternalObjectFlags FlagsToSet) const;
	void ClearInternalFlags(EInternalObjectFlags FlagsToClear) const;

	/**
	 * Marks the object for destruction: weak pointers stop resolving it at once, and the next garbage collection
	 * clears the strong references to it and destroys it even if it is still referenced (UE 4.27). A rooted object
	 * cannot be marked.
	 */
	void MarkPendingKill();
	void ClearPendingKill();
	bool IsPendingKill() const;

	/** True for an object the running (or last, not yet purged) collection found unreachable (UE). */
	bool IsUnreachable() const;

	/** IsPendingKill() || IsUnreachable() (UE). */
	bool IsPendingKillOrUnreachable() const;

	/** Keeps the object alive regardless of references, until RemoveFromRoot (UE: the root set). */
	void AddToRoot();
	void RemoveFromRoot();
	bool IsRooted() const;

	/** True for a class default object, an archetype or anything inside one (UE: IsTemplate). */
	bool IsTemplate(EObjectFlags TemplateTypes = RF_ArchetypeObject | RF_ClassDefaultObject) const;

	// Names.

	/** The object's name, with its number: "Enemy_2" (UE). */
	FString GetName() const;
	void GetName(FString& ResultString) const;

	/**
	 * The name of the object and its outers down to (not including) StopOuter: "/Game/Maps/Arena.Arena:Level.Enemy_2".
	 * '.' separates package-level names and inner objects; ':' separates an object whose outer is not a package from
	 * the object right below the package (UE).
	 */
	FString GetPathName(const UObject* StopOuter = nullptr) const;
	void GetPathName(const UObject* StopOuter, FString& ResultString) const;

	/** "<ClassName> <PathName>": "Class /Script/CoreUObject.Object" (UE). */
	FString GetFullName(const UObject* StopOuter = nullptr) const;

	// Outers.

	/** The top of the outer chain: the package the object lives in (UE). */
	UPackage* GetOutermost() const;
	UPackage* GetPackage() const
	{
		return GetOutermost();
	}

	/** The nearest outer of the given class, or nullptr. */
	UObject* GetTypedOuter(UClass* Target) const;

	template <typename T>
	T* GetTypedOuter() const
	{
		return (T*)GetTypedOuter(T::StaticClass());
	}

	/** True when SomeOuter is somewhere in this object's outer chain. */
	bool IsIn(const UObject* SomeOuter) const;

	// Class.

	/** True when the object's class is SomeBase or derives from it (UE: IsA). */
	bool IsA(const UClass* SomeBase) const;

	template <class T>
	bool IsA() const
	{
		return IsA(T::StaticClass());
	}
};
