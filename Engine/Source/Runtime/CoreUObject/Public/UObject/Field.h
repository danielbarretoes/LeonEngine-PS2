#pragma once

// FField: the lightweight, non-UObject base of properties (UE 4.25+: UObject/Field.h).

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include <new>
#include <type_traits>

class FArchive;
class FField;
class FProperty;
class UClass;
class UField;
class UObject;
class UStruct;

/**
 * The owner of an FField: a UObject (the struct, class or function that declares a property) or another FField (the
 * array / set / map / enum property an inner property belongs to) (UE: FFieldVariant).
 */
class COREUOBJECT_API FFieldVariant
{
public:
	FFieldVariant()
		: bIsUObject(false)
	{
		Container.Field = nullptr;
	}

	FFieldVariant(const FField* InField)
		: bIsUObject(false)
	{
		Container.Field = const_cast<FField*>(InField);
	}

	FFieldVariant(const UObject* InObject)
		: bIsUObject(true)
	{
		Container.Object = const_cast<UObject*>(InObject);
	}

	FORCEINLINE bool IsUObject() const
	{
		return bIsUObject;
	}

	FORCEINLINE bool IsValid() const
	{
		return bIsUObject ? Container.Object != nullptr : Container.Field != nullptr;
	}

	FORCEINLINE UObject* ToUObject() const
	{
		return bIsUObject ? Container.Object : nullptr;
	}

	FORCEINLINE FField* ToField() const
	{
		return bIsUObject ? nullptr : Container.Field;
	}

	FORCEINLINE bool operator==(const FFieldVariant& Other) const
	{
		return bIsUObject == Other.bIsUObject && Container.Field == Other.Container.Field;
	}

	FORCEINLINE bool operator!=(const FFieldVariant& Other) const
	{
		return !(*this == Other);
	}

private:
	union FFieldObjectUnion
	{
		FField* Field;
		UObject* Object;
	} Container;

	bool bIsUObject;
};

/**
 * The class of an FField type: its name, cast id and super class (UE: FFieldClass). One static instance per property
 * type, returned by <Type>::StaticClass().
 */
class COREUOBJECT_API FFieldClass
{
public:
	typedef FField* (*FConstructFunction)(const FFieldVariant&, const FName&, EObjectFlags);

	/** InCPPName is the C++ name ("FIntProperty"); the class name drops the F ("IntProperty"). */
	FFieldClass(const TCHAR* InCPPName, uint64 InId, uint64 InCastFlags, FFieldClass* InSuperClass,
		FConstructFunction InConstructFn);

	FFieldClass(const FFieldClass&) = delete;
	FFieldClass& operator=(const FFieldClass&) = delete;

	FORCEINLINE FName GetFName() const
	{
		return Name;
	}

	FString GetName() const
	{
		return Name.ToString();
	}

	/** The cast bit of this class (CASTCLASS_F...Property). */
	FORCEINLINE uint64 GetId() const
	{
		return Id;
	}

	/** Its own id plus its supers' ids. */
	FORCEINLINE uint64 GetCastFlags() const
	{
		return CastFlags;
	}

	FORCEINLINE bool HasAnyCastFlags(const uint64 InCastFlags) const
	{
		return !!(CastFlags & InCastFlags);
	}

	FORCEINLINE bool HasAllCastFlags(const uint64 InCastFlags) const
	{
		return (CastFlags & InCastFlags) == InCastFlags;
	}

	/** True when this class is InClass or derives from it. */
	FORCEINLINE bool IsChildOf(const FFieldClass* InClass) const
	{
		const uint64 OtherClassId = InClass->GetId();
		return OtherClassId ? !!(CastFlags & OtherClassId) : false;
	}

	FORCEINLINE FFieldClass* GetSuperClass() const
	{
		return SuperClass;
	}

	/** Creates a field of this class (UE: FFieldClass::Construct). */
	FField* Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InFlags = RF_NoFlags) const
	{
		return ConstructFn(InOwner, InName, InFlags);
	}

private:
	FName Name;
	uint64 Id;
	uint64 CastFlags;
	FFieldClass* SuperClass;
	FConstructFunction ConstructFn;
};

/**
 * Declares the boilerplate of an FField subclass (UE: DECLARE_FIELD): Super / ThisClass, StaticClass(), the cast
 * flags and the constructor the field class uses. IMPLEMENT_FIELD defines StaticClass() and Construct().
 */
#define DECLARE_FIELD(TClass, TSuperClass, TStaticFlags)                                                               \
private:                                                                                                               \
	TClass& operator=(TClass&&);                                                                                       \
	TClass& operator=(const TClass&);                                                                                  \
                                                                                                                       \
public:                                                                                                                \
	typedef TSuperClass Super;                                                                                         \
	typedef TClass ThisClass;                                                                                          \
	static FFieldClass* StaticClass();                                                                                 \
	static FField* Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InObjectFlags);           \
	static constexpr uint64 StaticClassCastFlagsPrivate()                                                              \
	{                                                                                                                  \
		return uint64(TStaticFlags);                                                                                   \
	}                                                                                                                  \
	static constexpr uint64 StaticClassCastFlags()                                                                     \
	{                                                                                                                  \
		return uint64(TStaticFlags) | Super::StaticClassCastFlags();                                                   \
	}

namespace UE::CoreUObject::Private
{
	/** Creates a field of type T, or nothing for an abstract field type. */
	template <typename T>
	FField* ConstructField(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InObjectFlags)
	{
		if constexpr (std::is_abstract_v<T>)
		{
			(void)InOwner;
			(void)InName;
			(void)InObjectFlags;
			return nullptr;
		}
		else
		{
			return new T(InOwner, InName, InObjectFlags);
		}
	}
} // namespace UE::CoreUObject::Private

/** Defines StaticClass() and Construct() of an FField subclass (UE: IMPLEMENT_FIELD). */
#define IMPLEMENT_FIELD(TClass)                                                                                        \
	FField* TClass::Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InObjectFlags)           \
	{                                                                                                                  \
		return UE::CoreUObject::Private::ConstructField<TClass>(InOwner, InName, InObjectFlags);                       \
	}                                                                                                                  \
	FFieldClass* TClass::StaticClass()                                                                                 \
	{                                                                                                                  \
		static FFieldClass StaticFieldClass(TEXT(#TClass), TClass::StaticClassCastFlagsPrivate(),                      \
			TClass::StaticClassCastFlags(), TClass::Super::StaticClass(), &TClass::Construct);                         \
		return &StaticFieldClass;                                                                                      \
	}

/**
 * Base of the reflection data that is not a UObject: properties (UE 4.25+: FField). Fields of a struct form a
 * singly linked list (Next) owned by the struct (UStruct::ChildProperties).
 */
class COREUOBJECT_API FField
{
public:
	typedef FField Super;
	typedef FField ThisClass;
	typedef FField BaseFieldClass;
	typedef FFieldClass FieldTypeClass;

	/** The next field of the owner. */
	FField* Next;

	FField(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	virtual ~FField();

	FField(const FField&) = delete;
	FField& operator=(const FField&) = delete;

	static FFieldClass* StaticClass();
	static FField* Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InObjectFlags);
	static constexpr uint64 StaticClassCastFlagsPrivate()
	{
		return uint64(CASTCLASS_UField);
	}
	static constexpr uint64 StaticClassCastFlags()
	{
		return uint64(CASTCLASS_UField);
	}

	FORCEINLINE FFieldClass* GetClass() const
	{
		return ClassPrivate;
	}

	FORCEINLINE uint64 GetCastFlags() const
	{
		return GetClass()->GetCastFlags();
	}

	FORCEINLINE bool IsA(const FFieldClass* FieldType) const
	{
		return !!(GetCastFlags() & FieldType->GetId());
	}

	template <typename T>
	FORCEINLINE bool IsA() const
	{
		return !!(GetCastFlags() & T::StaticClassCastFlagsPrivate());
	}

	FORCEINLINE bool HasAnyCastFlags(const uint64 InCastFlags) const
	{
		return !!(GetCastFlags() & InCastFlags);
	}

	FORCEINLINE FName GetFName() const
	{
		return NamePrivate;
	}

	FString GetName() const
	{
		return NamePrivate.ToString();
	}

	FORCEINLINE EObjectFlags GetFlags() const
	{
		return FlagsPrivate;
	}

	FORCEINLINE bool HasAnyFlags(const EObjectFlags FlagsToCheck) const
	{
		return (FlagsPrivate & FlagsToCheck) != 0;
	}

	FORCEINLINE FFieldVariant GetOwnerVariant() const
	{
		return Owner;
	}

	/** The UObject that ultimately owns this field (through container properties), or nullptr. */
	UObject* GetOwnerUObject() const;

	/** The class that owns this field, or nullptr (for a struct's or function's field). */
	UClass* GetOwnerClass() const;

	/** The struct (class, script struct or function) that owns this field. */
	UStruct* GetOwnerStruct() const;

	/** "<FieldClassName> <PathName>" (UE). */
	FString GetFullName() const;

	/** The owner's path name, then ':' and the field name ("/Script/Engine.Actor:RootComponent") (UE). */
	FString GetPathName(const UObject* StopOuter = nullptr) const;

	/** Called by a field created with this field as its owner (containers take their inner properties here). */
	virtual void AddCppProperty(FProperty* Property);

	/** Global operator new, but through FMemory so the fields count in the reflection budget. */
	static void* operator new(size_t Size);
	static void operator delete(void* Memory);

protected:
	FFieldClass* ClassPrivate;
	FFieldVariant Owner;
	FName NamePrivate;
	EObjectFlags FlagsPrivate;

	/** Sets the field class; each constructor sets its own, so the most derived one wins. */
	FORCEINLINE void SetFieldClass(FFieldClass* InClass)
	{
		ClassPrivate = InClass;
	}
};

/** Casts an FField to FieldType, or nullptr when it is not one (UE: CastField). */
template <typename FieldType>
FORCEINLINE FieldType* CastField(FField* Src)
{
	return Src && Src->HasAnyCastFlags(FieldType::StaticClassCastFlagsPrivate()) ? static_cast<FieldType*>(Src)
																				 : nullptr;
}

template <typename FieldType>
FORCEINLINE const FieldType* CastField(const FField* Src)
{
	return Src && Src->HasAnyCastFlags(FieldType::StaticClassCastFlagsPrivate()) ? static_cast<const FieldType*>(Src)
																				 : nullptr;
}

/** CastField that must succeed (UE: CastFieldChecked). */
template <typename FieldType>
FORCEINLINE FieldType* CastFieldChecked(FField* Src)
{
	FieldType* Result = CastField<FieldType>(Src);
	checkf(Result, "CastFieldChecked failed: %s is not a %s", Src ? *Src->GetName() : "nullptr",
		*FieldType::StaticClass()->GetName());
	return Result;
}

template <typename FieldType>
FORCEINLINE const FieldType* CastFieldChecked(const FField* Src)
{
	return CastFieldChecked<FieldType>(const_cast<FField*>(Src));
}

/** The field when its class is exactly FieldType (UE: ExactCastField). */
template <typename FieldType>
FORCEINLINE FieldType* ExactCastField(FField* Src)
{
	return Src && Src->GetClass() == FieldType::StaticClass() ? static_cast<FieldType*>(Src) : nullptr;
}
