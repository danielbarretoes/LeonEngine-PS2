#pragma once

// FProperty and every property type, the container helpers and TFieldIterator (UE: UObject/UnrealType.h, which UE
// splits with EnumProperty.h and TextProperty.h).

#include "Containers/Map.h"
#include "Containers/ScriptArray.h"
#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <new>
#include <type_traits>

class FOutputDevice;

/** Port flags of ExportTextItem / ImportText (UE: EPropertyPortFlags; the subset Leon uses). */
enum EPropertyPortFlags
{
	PPF_None = 0x00000000,
	/** Export for a config file (P10). */
	PPF_ConfigOnly = 0x00000400,
	/** Export names and strings without quotes where possible. */
	PPF_Delimited = 0x00000002,
};

/**
 * A reflected member of a struct, class or function (UE 4.25+: FProperty). It knows the member's offset, size and
 * flags and how to initialize, copy, compare, destroy and convert the value to / from text. ContainerPtr arguments
 * point at the owner (the object or struct instance); Value / Data arguments point at the member itself.
 */
class COREUOBJECT_API FProperty : public FField
{
	DECLARE_FIELD(FProperty, FField, CASTCLASS_FProperty)

public:
	/** Number of elements: 1, or N for a C array member. */
	int32 ArrayDim;
	/** Size of one element. */
	int32 ElementSize;
	EPropertyFlags PropertyFlags;

	/** Next property of the owner's PropertyLink chain (all properties, supers included). */
	FProperty* PropertyLinkNext;
	/** Next property of the owner's RefLink chain (object references). */
	FProperty* NextRef;
	/** Next property of the owner's DestructorLink chain. */
	FProperty* DestructorLinkNext;
	/** Next property of the owner's PostConstructLink chain. */
	FProperty* PostConstructLinkNext;

	FProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	/** Adds itself to InOwner (the struct's ChildProperties, or the container property's inner). */
	FProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);

	// Identity.

	/** The C++ type ("int32", "TArray<FName>", "UObject*") (UE: GetCPPType). */
	virtual FString GetCPPType() const = 0;

	/** True when Other has the same property class and C++ type (UE). */
	virtual bool SameType(const FProperty* Other) const;

	// Layout.

	FORCEINLINE int32 GetSize() const
	{
		return ArrayDim * ElementSize;
	}

	FORCEINLINE int32 GetOffset_ForInternal() const
	{
		return Offset_Internal;
	}

	FORCEINLINE int32 GetOffset_ForUFunction() const
	{
		return Offset_Internal;
	}

	FORCEINLINE int32 GetOffset_ForDebug() const
	{
		return Offset_Internal;
	}

	FORCEINLINE void SetOffset_Internal(int32 NewOffset)
	{
		Offset_Internal = NewOffset;
	}

	/** Alignment of one element (UE). */
	virtual int32 GetMinAlignment() const
	{
		return 1;
	}

	/** True when the whole member fits in a container of ContainerSize bytes. */
	FORCEINLINE bool IsInContainer(int32 ContainerSize) const
	{
		return Offset_Internal + GetSize() <= ContainerSize;
	}

	/** The address of element ArrayIndex of this member inside the owner at ContainerPtr (UE). */
	template <typename ValueType>
	FORCEINLINE ValueType* ContainerPtrToValuePtr(void* ContainerPtr, int32 ArrayIndex = 0) const
	{
		checkSlow(ContainerPtr && ArrayIndex >= 0 && ArrayIndex < ArrayDim);
		return (ValueType*)((uint8*)ContainerPtr + Offset_Internal + ElementSize * ArrayIndex);
	}

	template <typename ValueType>
	FORCEINLINE const ValueType* ContainerPtrToValuePtr(const void* ContainerPtr, int32 ArrayIndex = 0) const
	{
		return ContainerPtrToValuePtr<ValueType>(const_cast<void*>(ContainerPtr), ArrayIndex);
	}

	// Flags.

	FORCEINLINE bool HasAnyPropertyFlags(uint64 FlagsToCheck) const
	{
		return (PropertyFlags & FlagsToCheck) != 0;
	}

	FORCEINLINE bool HasAllPropertyFlags(uint64 FlagsToCheck) const
	{
		return (PropertyFlags & FlagsToCheck) == FlagsToCheck;
	}

	FORCEINLINE EPropertyFlags GetPropertyFlags() const
	{
		return PropertyFlags;
	}

	FORCEINLINE void SetPropertyFlags(EPropertyFlags NewFlags)
	{
		PropertyFlags |= NewFlags;
	}

	FORCEINLINE void ClearPropertyFlags(EPropertyFlags NewFlags)
	{
		PropertyFlags &= ~NewFlags;
	}

	/** True for an object reference property or a container / struct holding one (P10: RefLink). */
	virtual bool ContainsObjectReference() const
	{
		return false;
	}

	// Values. These handle the whole member (ArrayDim elements) unless named Single.

	/** Constructs the member in raw memory (UE). */
	FORCEINLINE void InitializeValue(void* Dest) const
	{
		if (PropertyFlags & CPF_ZeroConstructor)
		{
			FMemory::Memzero(Dest, ElementSize * ArrayDim);
		}
		else
		{
			InitializeValueInternal(Dest);
		}
	}

	FORCEINLINE void InitializeValue_InContainer(void* Dest) const
	{
		InitializeValue(ContainerPtrToValuePtr<void>(Dest));
	}

	/** Destroys the member (UE). */
	FORCEINLINE void DestroyValue(void* Dest) const
	{
		if (!(PropertyFlags & CPF_NoDestructor))
		{
			DestroyValueInternal(Dest);
		}
	}

	FORCEINLINE void DestroyValue_InContainer(void* Dest) const
	{
		DestroyValue(ContainerPtrToValuePtr<void>(Dest));
	}

	/** Resets one element to its default value (UE). */
	FORCEINLINE void ClearValue(void* Data) const
	{
		if (HasAllPropertyFlags(CPF_NoDestructor | CPF_ZeroConstructor))
		{
			FMemory::Memzero(Data, ElementSize);
		}
		else
		{
			ClearValueInternal(Data);
		}
	}

	FORCEINLINE void ClearValue_InContainer(void* Data, int32 ArrayIndex = 0) const
	{
		ClearValue(ContainerPtrToValuePtr<void>(Data, ArrayIndex));
	}

	/** Copies one element (UE). */
	FORCEINLINE void CopySingleValue(void* Dest, void const* Src) const
	{
		if (Dest != Src)
		{
			if (PropertyFlags & CPF_IsPlainOldData)
			{
				FMemory::Memcpy(Dest, Src, ElementSize);
			}
			else
			{
				CopyValuesInternal(Dest, Src, 1);
			}
		}
	}

	/** Copies every element (UE). */
	FORCEINLINE void CopyCompleteValue(void* Dest, void const* Src) const
	{
		if (Dest != Src)
		{
			if (PropertyFlags & CPF_IsPlainOldData)
			{
				FMemory::Memcpy(Dest, Src, ElementSize * ArrayDim);
			}
			else
			{
				CopyValuesInternal(Dest, Src, ArrayDim);
			}
		}
	}

	FORCEINLINE void CopyCompleteValue_InContainer(void* Dest, void const* Src) const
	{
		CopyCompleteValue(ContainerPtrToValuePtr<void>(Dest), ContainerPtrToValuePtr<void>(Src));
	}

	FORCEINLINE void CopySingleValue_InContainer(void* Dest, void const* Src, int32 ArrayIndex = 0) const
	{
		CopySingleValue(ContainerPtrToValuePtr<void>(Dest, ArrayIndex), ContainerPtrToValuePtr<void>(Src, ArrayIndex));
	}

	/** Copies every element into a script (exec thunk) local; bool writes a uint32 there (UE). */
	virtual void CopyCompleteValueToScriptVM(void* Dest, void const* Src) const
	{
		CopyCompleteValue(Dest, Src);
	}

	/** True when two elements are equal; a null B compares against the default value (UE). */
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const = 0;

	FORCEINLINE bool Identical_InContainer(
		const void* A, const void* B, int32 ArrayIndex = 0, uint32 PortFlags = 0) const
	{
		return Identical(ContainerPtrToValuePtr<void>(A, ArrayIndex),
			B ? ContainerPtrToValuePtr<void>(B, ArrayIndex) : nullptr, PortFlags);
	}

	/** GetTypeHash of one element; only valid with CPF_HasGetValueTypeHash (UE). */
	FORCEINLINE uint32 GetValueTypeHash(const void* Src) const
	{
		checkf(PropertyFlags & CPF_HasGetValueTypeHash, "Property %s has no GetValueTypeHash", *GetName());
		checkf(ArrayDim == 1, "GetValueTypeHash of a C array property %s", *GetName());
		return GetValueTypeHashInternal(Src);
	}

	// Text.

	/** Appends one element as text: numbers as C++ writes them, strings and names quoted, objects by path (UE). */
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const = 0;

	/**
	 * Parses one element from Buffer into Data; returns the text after it, or nullptr on an error, which is also
	 * written to ErrorText (UE: ImportText).
	 */
	const TCHAR* ImportText(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText = nullptr) const;

	/** ExportTextItem of element ArrayIndex of the member in a container (UE: ExportText_InContainer). */
	void ExportText_InContainer(int32 Index, FString& ValueStr, const void* Data, const void* Delta, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const;

	// Linking.

	/** Sets up the computed flags and sizes; the offset comes from the generated code (UE). */
	FORCEINLINE void LinkWithoutChangingOffset(FArchive& Ar)
	{
		LinkInternal(Ar);
	}

protected:
	friend class UStruct;

	virtual void LinkInternal(FArchive& Ar);
	virtual const TCHAR* ImportText_Internal(
		const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const = 0;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const;
	virtual uint32 GetValueTypeHashInternal(const void* Src) const;
	virtual void ClearValueInternal(void* Data) const;
	virtual void DestroyValueInternal(void* Dest) const;
	virtual void InitializeValueInternal(void* Dest) const;

private:
	/** Adds this property to its owner (UStruct::AddCppProperty or the container's AddCppProperty). */
	void Init();

	int32 Offset_Internal;
};

/** The type-level operations of a property's C++ type (UE: TPropertyTypeFundamentals). */
template <typename InTCppType>
class TPropertyTypeFundamentals
{
public:
	typedef InTCppType TCppType;

	enum
	{
		CPPSize = sizeof(TCppType),
		CPPAlignment = alignof(TCppType)
	};

	static FORCEINLINE TCppType const* GetPropertyValuePtr(void const* A)
	{
		return (TCppType const*)A;
	}

	static FORCEINLINE TCppType* GetPropertyValuePtr(void* A)
	{
		return (TCppType*)A;
	}

	static FORCEINLINE TCppType const& GetPropertyValue(void const* A)
	{
		return *GetPropertyValuePtr(A);
	}

	static FORCEINLINE TCppType GetDefaultPropertyValue()
	{
		return TCppType();
	}

	/** The value at B, or the default value when B is null (UE). */
	static FORCEINLINE TCppType GetOptionalPropertyValue(void const* B)
	{
		return B ? GetPropertyValue(B) : GetDefaultPropertyValue();
	}

	static FORCEINLINE void SetPropertyValue(void* A, TCppType const& Value)
	{
		*GetPropertyValuePtr(A) = Value;
	}

	static FORCEINLINE TCppType* InitializePropertyValue(void* A)
	{
		return ::new (A) TCppType();
	}

	static FORCEINLINE void DestroyPropertyValue(void* A)
	{
		GetPropertyValuePtr(A)->~TCppType();
	}

	/** CPF_ flags that follow from the C++ type (UE: GetComputedFlagsPropertyFlags). */
	static constexpr EPropertyFlags GetComputedFlagsPropertyFlags()
	{
		return (TIsPODType<TCppType>::Value ? CPF_IsPlainOldData : CPF_None) |
			(std::is_trivially_destructible_v<TCppType> ? CPF_NoDestructor : CPF_None) |
			(TIsZeroConstructType<TCppType>::Value ? CPF_ZeroConstructor : CPF_None) |
			(UE::CoreUObject::Private::THasGetTypeHash<TCppType>::value ? CPF_HasGetValueTypeHash : CPF_None);
	}
};

/** A property whose value is one C++ type (UE: TProperty). */
template <typename InTCppType, class TInPropertyBaseClass>
class TProperty
	: public TInPropertyBaseClass
	, public TPropertyTypeFundamentals<InTCppType>
{
public:
	typedef InTCppType TCppType;
	typedef TInPropertyBaseClass Super;
	typedef TPropertyTypeFundamentals<InTCppType> TTypeFundamentals;

	TProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
		: Super(InOwner, InName, InObjectFlags)
	{
		SetElementSize();
	}

	TProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
		: Super(InOwner, InName, InObjectFlags, InOffset, InFlags | TTypeFundamentals::GetComputedFlagsPropertyFlags())
	{
		SetElementSize();
	}

	virtual int32 GetMinAlignment() const override
	{
		return TTypeFundamentals::CPPAlignment;
	}

	/** The value of element ArrayIndex of the member in a container (UE). */
	FORCEINLINE TCppType GetPropertyValue_InContainer(void const* A, int32 ArrayIndex = 0) const
	{
		return TTypeFundamentals::GetPropertyValue(this->template ContainerPtrToValuePtr<void>(A, ArrayIndex));
	}

	FORCEINLINE void SetPropertyValue_InContainer(void* A, TCppType const& Value, int32 ArrayIndex = 0) const
	{
		TTypeFundamentals::SetPropertyValue(this->template ContainerPtrToValuePtr<void>(A, ArrayIndex), Value);
	}

protected:
	virtual void LinkInternal(FArchive& Ar) override
	{
		(void)Ar;
		SetElementSize();
		this->PropertyFlags |= TTypeFundamentals::GetComputedFlagsPropertyFlags();
	}

	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			TTypeFundamentals::GetPropertyValuePtr(Dest)[Index] = TTypeFundamentals::GetPropertyValuePtr(Src)[Index];
		}
	}

	virtual void ClearValueInternal(void* Data) const override
	{
		TTypeFundamentals::SetPropertyValue(Data, TTypeFundamentals::GetDefaultPropertyValue());
	}

	virtual void InitializeValueInternal(void* Dest) const override
	{
		for (int32 Index = 0; Index < this->ArrayDim; ++Index)
		{
			TTypeFundamentals::InitializePropertyValue((uint8*)Dest + Index * this->ElementSize);
		}
	}

	virtual void DestroyValueInternal(void* Dest) const override
	{
		for (int32 Index = 0; Index < this->ArrayDim; ++Index)
		{
			TTypeFundamentals::DestroyPropertyValue((uint8*)Dest + Index * this->ElementSize);
		}
	}

	virtual uint32 GetValueTypeHashInternal(const void* Src) const override
	{
		if constexpr (UE::CoreUObject::Private::THasGetTypeHash<TCppType>::value)
		{
			return GetTypeHash(TTypeFundamentals::GetPropertyValue(Src));
		}
		else
		{
			return Super::GetValueTypeHashInternal(Src);
		}
	}

	FORCEINLINE void SetElementSize()
	{
		this->ElementSize = TTypeFundamentals::CPPSize;
	}
};

/** A TProperty whose Identical is operator== (UE: TProperty_WithEqualityAndSerializer). */
template <typename InTCppType, class TInPropertyBaseClass>
class TProperty_WithEqualityAndSerializer : public TProperty<InTCppType, TInPropertyBaseClass>
{
public:
	typedef TProperty<InTCppType, TInPropertyBaseClass> Super;
	typedef InTCppType TCppType;
	typedef typename Super::TTypeFundamentals TTypeFundamentals;

	TProperty_WithEqualityAndSerializer(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
		: Super(InOwner, InName, InObjectFlags)
	{
	}

	TProperty_WithEqualityAndSerializer(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
		: Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
	{
	}

	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override
	{
		(void)PortFlags;
		return TTypeFundamentals::GetPropertyValue(A) == TTypeFundamentals::GetOptionalPropertyValue(B);
	}
};

class UEnum;

/** Base of the numeric properties (UE: FNumericProperty). */
class COREUOBJECT_API FNumericProperty : public FProperty
{
	DECLARE_FIELD(FNumericProperty, FProperty, CASTCLASS_FNumericProperty)

public:
	FNumericProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FNumericProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);

	virtual bool IsFloatingPoint() const;
	virtual bool IsInteger() const;

	/** The enum of an FByteProperty declared as TEnumAsByte, else nullptr (UE). */
	virtual UEnum* GetIntPropertyEnum() const;

	FORCEINLINE bool IsEnum() const
	{
		return GetIntPropertyEnum() != nullptr;
	}

	virtual void SetIntPropertyValue(void* Data, uint64 Value) const;
	virtual void SetIntPropertyValue(void* Data, int64 Value) const;
	virtual void SetFloatingPointPropertyValue(void* Data, double Value) const;
	virtual void SetNumericPropertyValueFromString(void* Data, TCHAR const* Value) const;
	virtual int64 GetSignedIntPropertyValue(void const* Data) const;
	virtual uint64 GetUnsignedIntPropertyValue(void const* Data) const;
	virtual double GetFloatingPointPropertyValue(void const* Data) const;
	virtual FString GetNumericPropertyValueToString(void const* Data) const;

protected:
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
};

namespace UE::CoreUObject::Private
{
	/** Parses a number as C++ writes it (Leon helper of the numeric ImportText). */
	COREUOBJECT_API const TCHAR* ParseNumber(const TCHAR* Buffer, bool bFloatingPoint, bool bSigned, int64& OutInteger,
		uint64& OutUnsigned, double& OutFloat);

	/** Writes a number the way ExportTextItem does. */
	COREUOBJECT_API void AppendNumber(FString& Out, int64 Value);
	COREUOBJECT_API void AppendUnsignedNumber(FString& Out, uint64 Value);
	COREUOBJECT_API void AppendFloat(FString& Out, double Value, bool bIsFloat);
} // namespace UE::CoreUObject::Private

/** A numeric property of C++ type InTCppType (UE: TProperty_Numeric). */
template <typename InTCppType>
class TProperty_Numeric : public TProperty_WithEqualityAndSerializer<InTCppType, FNumericProperty>
{
public:
	typedef TProperty_WithEqualityAndSerializer<InTCppType, FNumericProperty> Super;
	typedef InTCppType TCppType;
	typedef typename Super::TTypeFundamentals TTypeFundamentals;

	TProperty_Numeric(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
		: Super(InOwner, InName, InObjectFlags)
	{
	}

	TProperty_Numeric(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
		: Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
	{
	}

	virtual bool IsFloatingPoint() const override
	{
		return std::is_floating_point_v<TCppType>;
	}

	virtual bool IsInteger() const override
	{
		return std::is_integral_v<TCppType>;
	}

	virtual void SetIntPropertyValue(void* Data, uint64 Value) const override
	{
		check(IsInteger());
		TTypeFundamentals::SetPropertyValue(Data, (TCppType)Value);
	}

	virtual void SetIntPropertyValue(void* Data, int64 Value) const override
	{
		check(IsInteger());
		TTypeFundamentals::SetPropertyValue(Data, (TCppType)Value);
	}

	virtual void SetFloatingPointPropertyValue(void* Data, double Value) const override
	{
		check(IsFloatingPoint());
		TTypeFundamentals::SetPropertyValue(Data, (TCppType)Value);
	}

	virtual int64 GetSignedIntPropertyValue(void const* Data) const override
	{
		check(IsInteger());
		return (int64)TTypeFundamentals::GetPropertyValue(Data);
	}

	virtual uint64 GetUnsignedIntPropertyValue(void const* Data) const override
	{
		check(IsInteger());
		return (uint64)TTypeFundamentals::GetPropertyValue(Data);
	}

	virtual double GetFloatingPointPropertyValue(void const* Data) const override
	{
		check(IsFloatingPoint());
		return (double)TTypeFundamentals::GetPropertyValue(Data);
	}

	virtual FString GetNumericPropertyValueToString(void const* Data) const override
	{
		FString Result;
		AppendValue(Result, TTypeFundamentals::GetPropertyValue(Data));
		return Result;
	}

	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override
	{
		(void)DefaultValue;
		(void)Parent;
		(void)PortFlags;
		(void)ExportRootScope;
		AppendValue(ValueStr, TTypeFundamentals::GetPropertyValue(PropertyValue));
	}

protected:
	static void AppendValue(FString& Out, TCppType Value)
	{
		if constexpr (std::is_floating_point_v<TCppType>)
		{
			UE::CoreUObject::Private::AppendFloat(Out, (double)Value, sizeof(TCppType) == sizeof(float));
		}
		else if constexpr (std::is_signed_v<TCppType>)
		{
			UE::CoreUObject::Private::AppendNumber(Out, (int64)Value);
		}
		else
		{
			UE::CoreUObject::Private::AppendUnsignedNumber(Out, (uint64)Value);
		}
	}
};

/** int8 (UE: FInt8Property). */
class COREUOBJECT_API FInt8Property : public TProperty_Numeric<int8>
{
	DECLARE_FIELD(FInt8Property, TProperty_Numeric<int8>, CASTCLASS_FInt8Property)

public:
	FInt8Property(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FInt8Property(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual FString GetCPPType() const override;
};

/** int16 (UE: FInt16Property). */
class COREUOBJECT_API FInt16Property : public TProperty_Numeric<int16>
{
	DECLARE_FIELD(FInt16Property, TProperty_Numeric<int16>, CASTCLASS_FInt16Property)

public:
	FInt16Property(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FInt16Property(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual FString GetCPPType() const override;
};

/** int32, and C++ int (UE: FIntProperty). */
class COREUOBJECT_API FIntProperty : public TProperty_Numeric<int32>
{
	DECLARE_FIELD(FIntProperty, TProperty_Numeric<int32>, CASTCLASS_FIntProperty)

public:
	FIntProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FIntProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual FString GetCPPType() const override;
};

/** int64 (UE: FInt64Property). */
class COREUOBJECT_API FInt64Property : public TProperty_Numeric<int64>
{
	DECLARE_FIELD(FInt64Property, TProperty_Numeric<int64>, CASTCLASS_FInt64Property)

public:
	FInt64Property(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FInt64Property(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual FString GetCPPType() const override;
};

/** uint16 (UE: FUInt16Property). */
class COREUOBJECT_API FUInt16Property : public TProperty_Numeric<uint16>
{
	DECLARE_FIELD(FUInt16Property, TProperty_Numeric<uint16>, CASTCLASS_FUInt16Property)

public:
	FUInt16Property(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FUInt16Property(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual FString GetCPPType() const override;
};

/** uint32, and C++ unsigned int (UE: FUInt32Property). */
class COREUOBJECT_API FUInt32Property : public TProperty_Numeric<uint32>
{
	DECLARE_FIELD(FUInt32Property, TProperty_Numeric<uint32>, CASTCLASS_FUInt32Property)

public:
	FUInt32Property(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FUInt32Property(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual FString GetCPPType() const override;
};

/** uint64 (UE: FUInt64Property). */
class COREUOBJECT_API FUInt64Property : public TProperty_Numeric<uint64>
{
	DECLARE_FIELD(FUInt64Property, TProperty_Numeric<uint64>, CASTCLASS_FUInt64Property)

public:
	FUInt64Property(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FUInt64Property(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual FString GetCPPType() const override;
};

/** float (UE: FFloatProperty). */
class COREUOBJECT_API FFloatProperty : public TProperty_Numeric<float>
{
	DECLARE_FIELD(FFloatProperty, TProperty_Numeric<float>, CASTCLASS_FFloatProperty)

public:
	FFloatProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FFloatProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual FString GetCPPType() const override;
};

/** double (UE: FDoubleProperty). */
class COREUOBJECT_API FDoubleProperty : public TProperty_Numeric<double>
{
	DECLARE_FIELD(FDoubleProperty, TProperty_Numeric<double>, CASTCLASS_FDoubleProperty)

public:
	FDoubleProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FDoubleProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual FString GetCPPType() const override;
};

/** C++ int and unsigned int are int32 / uint32 on every Leon platform (UE: the unsized property types). */
typedef FIntProperty FUnsizedIntProperty;
typedef FUInt32Property FUnsizedUIntProperty;
static_assert(sizeof(int) == sizeof(int32) && sizeof(unsigned int) == sizeof(uint32), "int must be 32 bits");

/** uint8, optionally holding a TEnumAsByte enum (UE: FByteProperty). */
class COREUOBJECT_API FByteProperty : public TProperty_Numeric<uint8>
{
	DECLARE_FIELD(FByteProperty, TProperty_Numeric<uint8>, CASTCLASS_FByteProperty)

public:
	/** The enum of a TEnumAsByte member, or nullptr. */
	UEnum* Enum;

	FByteProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FByteProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UEnum* InEnum = nullptr);

	virtual FString GetCPPType() const override;
	virtual UEnum* GetIntPropertyEnum() const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
};

/**
 * A bool, as a native bool or one bit of a bitfield (UE: FBoolProperty). ByteOffset / ByteMask address the bit
 * inside the member's storage; FieldMask is 0xff for a native bool.
 */
class COREUOBJECT_API FBoolProperty : public FProperty
{
	DECLARE_FIELD(FBoolProperty, FProperty, CASTCLASS_FBoolProperty)

public:
	FBoolProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	/** InBitMask is the mask of the bit in its storage, InElementSize the storage size (sizeof(bool) when native). */
	FBoolProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, uint32 InBitMask, uint32 InElementSize, bool bIsNativeBool);

	/** Sets the storage size and bit (UE: SetBoolSize). */
	void SetBoolSize(const uint32 InSize, const bool bIsNativeBool = false, const uint32 InBitMask = 0);

	FORCEINLINE bool IsNativeBool() const
	{
		return FieldMask == 0xff;
	}

	FORCEINLINE uint8 GetFieldSize() const
	{
		return FieldSize;
	}

	FORCEINLINE uint8 GetByteOffset() const
	{
		return ByteOffset;
	}

	FORCEINLINE uint8 GetFieldMask() const
	{
		return FieldMask;
	}

	FORCEINLINE uint8 GetByteMask() const
	{
		return ByteMask;
	}

	/** The bool at A (the member's address) (UE). */
	FORCEINLINE bool GetPropertyValue(void const* A) const
	{
		const uint8* ByteValue = (const uint8*)A + ByteOffset;
		return !!(*ByteValue & FieldMask);
	}

	FORCEINLINE bool GetPropertyValue_InContainer(void const* A, int32 ArrayIndex = 0) const
	{
		return GetPropertyValue(ContainerPtrToValuePtr<void>(A, ArrayIndex));
	}

	/** Sets the bool at A, leaving the other bits of its storage alone (UE). */
	FORCEINLINE void SetPropertyValue(void* A, bool Value) const
	{
		uint8* ByteValue = (uint8*)A + ByteOffset;
		*ByteValue = ((*ByteValue) & ~FieldMask) | (Value ? ByteMask : 0);
	}

	FORCEINLINE void SetPropertyValue_InContainer(void* A, bool Value, int32 ArrayIndex = 0) const
	{
		SetPropertyValue(ContainerPtrToValuePtr<void>(A, ArrayIndex), Value);
	}

	virtual FString GetCPPType() const override;
	virtual int32 GetMinAlignment() const override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;
	virtual void CopyCompleteValueToScriptVM(void* Dest, void const* Src) const override;

protected:
	virtual void LinkInternal(FArchive& Ar) override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override;
	virtual void ClearValueInternal(void* Data) const override;
	virtual void InitializeValueInternal(void* Dest) const override;
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override;

private:
	/** Size of the bool's storage. */
	uint8 FieldSize;
	/** Byte of the storage that holds the bit. */
	uint8 ByteOffset;
	/** The bit in that byte. */
	uint8 ByteMask;
	/** The bits to test: ByteMask, or 0xff for a native bool. */
	uint8 FieldMask;
};

typedef TProperty_WithEqualityAndSerializer<FString, FProperty> FStrProperty_Super;

/** FString (UE: FStrProperty). */
class COREUOBJECT_API FStrProperty : public FStrProperty_Super
{
	DECLARE_FIELD(FStrProperty, FStrProperty_Super, CASTCLASS_FStrProperty)

public:
	FStrProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FStrProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);

	virtual FString GetCPPType() const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
};

typedef TProperty_WithEqualityAndSerializer<FName, FProperty> FNameProperty_Super;

/** FName (UE: FNameProperty). */
class COREUOBJECT_API FNameProperty : public FNameProperty_Super
{
	DECLARE_FIELD(FNameProperty, FNameProperty_Super, CASTCLASS_FNameProperty)

public:
	FNameProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FNameProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);

	virtual FString GetCPPType() const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
};

typedef TProperty<FText, FProperty> FTextProperty_Super;

/** FText (UE: FTextProperty). Leon's FText is a display string without localization data. */
class COREUOBJECT_API FTextProperty : public FTextProperty_Super
{
	DECLARE_FIELD(FTextProperty, FTextProperty_Super, CASTCLASS_FTextProperty)

public:
	FTextProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FTextProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);

	virtual FString GetCPPType() const override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override;
};

/** An enum class member: the enum plus an inner numeric property for the underlying type (UE: FEnumProperty). */
class COREUOBJECT_API FEnumProperty : public FProperty
{
	DECLARE_FIELD(FEnumProperty, FProperty, CASTCLASS_FEnumProperty)

public:
	FEnumProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FEnumProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UEnum* InEnum);
	virtual ~FEnumProperty() override;

	FORCEINLINE FNumericProperty* GetUnderlyingProperty() const
	{
		return UnderlyingProp;
	}

	FORCEINLINE UEnum* GetEnum() const
	{
		return Enum;
	}

	virtual void AddCppProperty(FProperty* Inner) override;
	virtual FString GetCPPType() const override;
	virtual int32 GetMinAlignment() const override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual void LinkInternal(FArchive& Ar) override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override;

private:
	FNumericProperty* UnderlyingProp;
	UEnum* Enum;
};

/** Base of the properties that reference a UObject (UE: FObjectPropertyBase). */
class COREUOBJECT_API FObjectPropertyBase : public FProperty
{
	DECLARE_FIELD(FObjectPropertyBase, FProperty, CASTCLASS_FObjectPropertyBase)

public:
	/** The class (or base class) of the referenced objects. */
	UClass* PropertyClass;

	FObjectPropertyBase(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FObjectPropertyBase(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UClass* InClass = nullptr);

	/** The object the value at PropertyValueAddress references (resolving soft / weak references) (UE). */
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const = 0;
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const = 0;

	FORCEINLINE UObject* GetObjectPropertyValue_InContainer(const void* ContainerAddress, int32 ArrayIndex = 0) const
	{
		return GetObjectPropertyValue(ContainerPtrToValuePtr<void>(ContainerAddress, ArrayIndex));
	}

	FORCEINLINE void SetObjectPropertyValue_InContainer(
		void* ContainerAddress, UObject* Value, int32 ArrayIndex = 0) const
	{
		SetObjectPropertyValue(ContainerPtrToValuePtr<void>(ContainerAddress, ArrayIndex), Value);
	}

	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;
	virtual bool SameType(const FProperty* Other) const override;

	/** Finds the object an ImportText path names ("None", a path, or a name in the owner's package) (UE). */
	static UObject* FindImportedObject(const FProperty* Property, UObject* OwnerObject, UClass* ObjectClass,
		UClass* RequiredMetaClass, const TCHAR* Text);

protected:
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;

	/** Rejects an object that is not a PropertyClass (and, for class properties, not a child of the meta class). */
	virtual bool AllowObjectTypeReference(UObject* Object) const;
};

/** An object property storing InTCppType (UE: TFObjectPropertyBase). */
template <typename InTCppType>
class TFObjectPropertyBase : public TProperty<InTCppType, FObjectPropertyBase>
{
public:
	typedef TProperty<InTCppType, FObjectPropertyBase> Super;
	typedef InTCppType TCppType;

	TFObjectPropertyBase(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
		: Super(InOwner, InName, InObjectFlags)
	{
	}

	TFObjectPropertyBase(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UClass* InClass)
		: Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
	{
		this->PropertyClass = InClass;
	}

	virtual bool ContainsObjectReference() const override
	{
		return true;
	}
};

/** A UObject* member (UE: FObjectProperty). */
class COREUOBJECT_API FObjectProperty : public TFObjectPropertyBase<UObject*>
{
	DECLARE_FIELD(FObjectProperty, TFObjectPropertyBase<UObject*>, CASTCLASS_FObjectProperty)

public:
	FObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UClass* InClass);

	virtual FString GetCPPType() const override;
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const override;
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const override;
};

/** A TSubclassOf<T> / UClass* member (UE: FClassProperty). */
class COREUOBJECT_API FClassProperty : public FObjectProperty
{
	DECLARE_FIELD(FClassProperty, FObjectProperty, CASTCLASS_FClassProperty)

public:
	/** The base class of the classes the member may hold (T of TSubclassOf<T>). */
	UClass* MetaClass;

	FClassProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FClassProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UClass* InMetaClass, UClass* InClassType);

	virtual FString GetCPPType() const override;
	virtual bool SameType(const FProperty* Other) const override;

protected:
	virtual bool AllowObjectTypeReference(UObject* Object) const override;
};

/** A TWeakObjectPtr<T> member (UE: FWeakObjectProperty). */
class COREUOBJECT_API FWeakObjectProperty : public TFObjectPropertyBase<FWeakObjectPtr>
{
	DECLARE_FIELD(FWeakObjectProperty, TFObjectPropertyBase<FWeakObjectPtr>, CASTCLASS_FWeakObjectProperty)

public:
	FWeakObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FWeakObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UClass* InClass);

	virtual FString GetCPPType() const override;
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const override;
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const override;
};

/** A TSoftObjectPtr<T> member (UE: FSoftObjectProperty). */
class COREUOBJECT_API FSoftObjectProperty : public TFObjectPropertyBase<FSoftObjectPtr>
{
	DECLARE_FIELD(FSoftObjectProperty, TFObjectPropertyBase<FSoftObjectPtr>, CASTCLASS_FSoftObjectProperty)

public:
	FSoftObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FSoftObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UClass* InClass);

	virtual FString GetCPPType() const override;
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const override;
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
};

/** A TSoftClassPtr<T> member (UE: FSoftClassProperty). */
class COREUOBJECT_API FSoftClassProperty : public FSoftObjectProperty
{
	DECLARE_FIELD(FSoftClassProperty, FSoftObjectProperty, CASTCLASS_FSoftClassProperty)

public:
	UClass* MetaClass;

	FSoftClassProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FSoftClassProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UClass* InMetaClass);

	virtual FString GetCPPType() const override;
	virtual bool SameType(const FProperty* Other) const override;
};

/** A USTRUCT member (UE: FStructProperty). */
class COREUOBJECT_API FStructProperty : public FProperty
{
	DECLARE_FIELD(FStructProperty, FProperty, CASTCLASS_FStructProperty)

public:
	UScriptStruct* Struct;

	FStructProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FStructProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, UScriptStruct* InStruct);

	virtual FString GetCPPType() const override;
	virtual int32 GetMinAlignment() const override;
	virtual bool ContainsObjectReference() const override;
	virtual bool SameType(const FProperty* Other) const override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual void LinkInternal(FArchive& Ar) override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override;
	virtual void ClearValueInternal(void* Data) const override;
	virtual void DestroyValueInternal(void* Dest) const override;
	virtual void InitializeValueInternal(void* Dest) const override;
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override;
};

/** A TArray member: an FScriptArray plus the Inner element property (UE: FArrayProperty). */
class COREUOBJECT_API FArrayProperty : public FProperty
{
	DECLARE_FIELD(FArrayProperty, FProperty, CASTCLASS_FArrayProperty)

public:
	FProperty* Inner;
	EArrayPropertyFlags ArrayFlags;

	FArrayProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FArrayProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, EArrayPropertyFlags InArrayPropertyFlags = EArrayPropertyFlags::None);
	virtual ~FArrayProperty() override;

	virtual void AddCppProperty(FProperty* Property) override;
	virtual FString GetCPPType() const override;
	virtual int32 GetMinAlignment() const override;
	virtual bool ContainsObjectReference() const override;
	virtual bool SameType(const FProperty* Other) const override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual void LinkInternal(FArchive& Ar) override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override;
	virtual void ClearValueInternal(void* Data) const override;
	virtual void DestroyValueInternal(void* Dest) const override;
	virtual void InitializeValueInternal(void* Dest) const override;
};

/** A TSet member: an FScriptSet plus the ElementProp property (UE: FSetProperty). */
class COREUOBJECT_API FSetProperty : public FProperty
{
	DECLARE_FIELD(FSetProperty, FProperty, CASTCLASS_FSetProperty)

public:
	FProperty* ElementProp;
	FScriptSetLayout SetLayout;

	FSetProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FSetProperty(
		FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags);
	virtual ~FSetProperty() override;

	virtual void AddCppProperty(FProperty* Property) override;
	virtual FString GetCPPType() const override;
	virtual int32 GetMinAlignment() const override;
	virtual bool ContainsObjectReference() const override;
	virtual bool SameType(const FProperty* Other) const override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual void LinkInternal(FArchive& Ar) override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override;
	virtual void ClearValueInternal(void* Data) const override;
	virtual void DestroyValueInternal(void* Dest) const override;
	virtual void InitializeValueInternal(void* Dest) const override;
};

/** A TMap member: an FScriptMap plus KeyProp and ValueProp (UE: FMapProperty). */
class COREUOBJECT_API FMapProperty : public FProperty
{
	DECLARE_FIELD(FMapProperty, FProperty, CASTCLASS_FMapProperty)

public:
	FProperty* KeyProp;
	FProperty* ValueProp;
	FScriptMapLayout MapLayout;
	EMapPropertyFlags MapFlags;

	FMapProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FMapProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
		EPropertyFlags InFlags, EMapPropertyFlags InMapFlags = EMapPropertyFlags::None);
	virtual ~FMapProperty() override;

	/** The first property added is the key, the second the value (the generated order, read backwards). */
	virtual void AddCppProperty(FProperty* Property) override;
	virtual FString GetCPPType() const override;
	virtual int32 GetMinAlignment() const override;
	virtual bool ContainsObjectReference() const override;
	virtual bool SameType(const FProperty* Other) const override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags = 0) const override;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent,
		int32 PortFlags, UObject* ExportRootScope = nullptr) const override;

protected:
	virtual void LinkInternal(FArchive& Ar) override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject,
		FOutputDevice* ErrorText) const override;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override;
	virtual void ClearValueInternal(void* Data) const override;
	virtual void DestroyValueInternal(void* Dest) const override;
	virtual void InitializeValueInternal(void* Dest) const override;
};

/**
 * Element access to an array member through its property (UE: FScriptArrayHelper). Elements are constructed and
 * destroyed with the Inner property.
 */
class COREUOBJECT_API FScriptArrayHelper
{
public:
	FScriptArrayHelper(const FArrayProperty* InProperty, const void* InArray);

	/** A helper for an array of InInnerProperty elements that has no FArrayProperty (UE). */
	static FScriptArrayHelper CreateHelperFormInnerProperty(const FProperty* InInnerProperty, const void* InArray);

	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Index >= 0 && Index < Num();
	}

	FORCEINLINE int32 Num() const
	{
		return HeapArray->Num();
	}

	/** The element at Index; nullptr for an empty array (UE). */
	FORCEINLINE uint8* GetRawPtr(int32 Index = 0)
	{
		if (!Num())
		{
			checkSlow(!Index);
			return nullptr;
		}
		checkSlow(IsValidIndex(Index));
		return (uint8*)HeapArray->GetData() + Index * ElementSize;
	}

	/** Destroys every element, keeping room for Slack (UE). */
	void EmptyValues(int32 Slack = 0);

	/** Destroys every element and adds Count default ones (UE). */
	void EmptyAndAddValues(int32 Count);

	/** Adds one default element; returns its index (UE). */
	int32 AddValue();

	/** Adds Count default elements; returns the index of the first (UE). */
	int32 AddValues(int32 Count);

	/** Inserts Count default elements at Index (UE). */
	void InsertValues(int32 Index, int32 Count = 1);

	/** Grows with default elements or shrinks to NewNum (UE). */
	void Resize(int32 NewNum);

	/** Destroys and removes Count elements at Index (UE). */
	void RemoveValues(int32 Index, int32 Count = 1);

	void SwapValues(int32 A, int32 B);

private:
	FScriptArrayHelper(const FProperty* InInnerProperty, const void* InArray, int32 InElementSize);

	void ConstructItems(int32 Index, int32 Count);
	void DestructItems(int32 Index, int32 Count);

	const FProperty* InnerProperty;
	FScriptArray* HeapArray;
	int32 ElementSize;
};

/** Element access to a set member through its property (UE: FScriptSetHelper). */
class COREUOBJECT_API FScriptSetHelper
{
public:
	FScriptSetHelper(const FSetProperty* InProperty, const void* InSet);

	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Set->IsValidIndex(Index);
	}

	FORCEINLINE int32 Num() const
	{
		return Set->Num();
	}

	/** One past the largest index; iterate [0, GetMaxIndex()) skipping invalid indices (UE). */
	FORCEINLINE int32 GetMaxIndex() const
	{
		return Set->GetMaxIndex();
	}

	/** The element at Index (a valid index) (UE). */
	FORCEINLINE uint8* GetElementPtr(int32 Index)
	{
		return (uint8*)Set->GetData(Index, SetLayout);
	}

	/** Destroys every element, keeping room for Slack (UE). */
	void EmptyElements(int32 Slack = 0);

	/** Adds a default element; the set is not valid until Rehash (UE). */
	int32 AddDefaultValue_Invalid_NeedsRehash();

	/** Rebuilds the hash after AddDefaultValue_Invalid_NeedsRehash (UE). */
	void Rehash();

	/** Destroys and removes the element at Index (UE). */
	void RemoveAt(int32 Index, int32 Count = 1);

	/** The index of the element equal to ElementToFind, or INDEX_NONE (UE). */
	int32 FindElementIndex(const void* ElementToFind) const;

	/** Adds a copy of ElementToAdd unless an equal element exists (UE: AddElement). */
	void AddElement(const void* ElementToAdd);

	/** Removes the element equal to ElementToRemove; returns whether it was there (UE). */
	bool RemoveElement(const void* ElementToRemove);

private:
	const FProperty* ElementProp;
	FScriptSet* Set;
	FScriptSetLayout SetLayout;
};

/** Pair access to a map member through its property (UE: FScriptMapHelper). */
class COREUOBJECT_API FScriptMapHelper
{
public:
	FScriptMapHelper(const FMapProperty* InProperty, const void* InMap);

	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Map->IsValidIndex(Index);
	}

	FORCEINLINE int32 Num() const
	{
		return Map->Num();
	}

	FORCEINLINE int32 GetMaxIndex() const
	{
		return Map->GetMaxIndex();
	}

	/** The pair at Index: the key at offset 0 (UE). */
	FORCEINLINE uint8* GetPairPtr(int32 Index)
	{
		return (uint8*)Map->GetData(Index, MapLayout);
	}

	FORCEINLINE uint8* GetKeyPtr(int32 Index)
	{
		return GetPairPtr(Index);
	}

	FORCEINLINE uint8* GetValuePtr(int32 Index)
	{
		return GetPairPtr(Index) + MapLayout.ValueOffset;
	}

	/** Destroys every pair, keeping room for Slack (UE). */
	void EmptyValues(int32 Slack = 0);

	/** Adds a default pair; the map is not valid until Rehash (UE). */
	int32 AddDefaultValue_Invalid_NeedsRehash();

	void Rehash();

	/** Destroys and removes the pair at Index (UE). */
	void RemoveAt(int32 Index, int32 Count = 1);

	/** The index of the pair with this key, or INDEX_NONE (UE). */
	int32 FindMapIndexWithKey(const void* KeyPtr) const;

	/** The value of the pair with this key, or nullptr (UE). */
	uint8* FindValueFromHash(const void* KeyPtr);

	/** Adds or replaces the pair KeyPtr -> ValuePtr with copies (UE: AddPair). */
	void AddPair(const void* KeyPtr, const void* ValuePtr);

	/** The value of KeyPtr's pair, added with a default value when missing (UE). */
	void* FindOrAdd(const void* KeyPtr);

	/** Removes the pair with this key; returns whether it was there (UE). */
	bool RemovePair(const void* KeyPtr);

private:
	const FProperty* KeyProp;
	const FProperty* ValueProp;
	FScriptMap* Map;
	FScriptMapLayout MapLayout;
};

namespace UE::CoreUObject::Private
{
	template <typename FieldType>
	FORCEINLINE bool IsFieldOfType(UField* Field)
	{
		return Field->IsA(FieldType::StaticClass());
	}

	template <typename FieldType>
	FORCEINLINE bool IsFieldOfType(FField* Field)
	{
		return Field->IsA<FieldType>();
	}

	template <typename FieldType>
	FORCEINLINE UField* GetFirstField(const UStruct* Struct, UField*)
	{
		return Struct->Children;
	}

	template <typename FieldType>
	FORCEINLINE FField* GetFirstField(const UStruct* Struct, FField*)
	{
		return Struct->ChildProperties;
	}
} // namespace UE::CoreUObject::Private

/**
 * Iterates the fields of a struct: its FProperties (FieldType derived from FField) or its UFunctions (derived from
 * UField), this struct's first in declaration order, then its supers' with IncludeSuper (UE: TFieldIterator).
 */
template <class FieldType>
class TFieldIterator
{
	typedef std::conditional_t<std::is_base_of_v<UField, FieldType>, UField, FField> BaseFieldClass;

public:
	explicit TFieldIterator(const UStruct* InStruct,
		EFieldIteratorFlags::SuperClassFlags InSuperClassFlags = EFieldIteratorFlags::IncludeSuper,
		EFieldIteratorFlags::DeprecatedPropertyFlags InDeprecatedFieldFlags = EFieldIteratorFlags::IncludeDeprecated)
		: Struct(InStruct)
		, Field(InStruct ? FirstField(InStruct) : nullptr)
		, bIncludeSuper(InSuperClassFlags == EFieldIteratorFlags::IncludeSuper)
		, bIncludeDeprecated(InDeprecatedFieldFlags == EFieldIteratorFlags::IncludeDeprecated)
	{
		IterateToNext();
	}

	FORCEINLINE explicit operator bool() const
	{
		return Field != nullptr;
	}

	FORCEINLINE bool operator!() const
	{
		return !(bool)*this;
	}

	FORCEINLINE void operator++()
	{
		checkSlow(Field);
		Field = Field->Next;
		IterateToNext();
	}

	FORCEINLINE FieldType* operator*() const
	{
		return (FieldType*)Field;
	}

	FORCEINLINE FieldType* operator->() const
	{
		return (FieldType*)Field;
	}

	/** The struct that declares the current field (UE). */
	FORCEINLINE const UStruct* GetStruct() const
	{
		return Struct;
	}

private:
	static BaseFieldClass* FirstField(const UStruct* InStruct)
	{
		return UE::CoreUObject::Private::GetFirstField<FieldType>(InStruct, (BaseFieldClass*)nullptr);
	}

	bool IsDeprecated(BaseFieldClass* InField) const
	{
		if constexpr (std::is_base_of_v<FProperty, FieldType>)
		{
			return ((FProperty*)InField)->HasAnyPropertyFlags(CPF_Deprecated);
		}
		else
		{
			(void)InField;
			return false;
		}
	}

	void IterateToNext()
	{
		BaseFieldClass* CurrentField = Field;
		const UStruct* CurrentStruct = Struct;
		while (CurrentStruct)
		{
			for (; CurrentField; CurrentField = CurrentField->Next)
			{
				if (UE::CoreUObject::Private::IsFieldOfType<FieldType>(CurrentField) &&
					(bIncludeDeprecated || !IsDeprecated(CurrentField)))
				{
					Struct = CurrentStruct;
					Field = CurrentField;
					return;
				}
			}
			CurrentStruct = bIncludeSuper ? CurrentStruct->GetInheritanceSuper() : nullptr;
			if (CurrentStruct)
			{
				CurrentField = FirstField(CurrentStruct);
			}
		}
		Struct = nullptr;
		Field = nullptr;
	}

	const UStruct* Struct;
	BaseFieldClass* Field;
	bool bIncludeSuper;
	bool bIncludeDeprecated;
};

/** Range-for adapter of TFieldIterator (UE: TFieldRange). */
template <class FieldType>
class TFieldRange
{
public:
	explicit TFieldRange(const UStruct* InStruct,
		EFieldIteratorFlags::SuperClassFlags InSuperClassFlags = EFieldIteratorFlags::IncludeSuper)
		: Begin(InStruct, InSuperClassFlags)
	{
	}

	class FIterator
	{
	public:
		explicit FIterator(const TFieldIterator<FieldType>& InIterator)
			: Iterator(InIterator)
		{
		}

		FORCEINLINE FieldType* operator*() const
		{
			return *Iterator;
		}

		FORCEINLINE void operator++()
		{
			++Iterator;
		}

		/** Only compares with the end iterator. */
		FORCEINLINE bool operator!=(const FIterator&) const
		{
			return (bool)Iterator;
		}

	private:
		TFieldIterator<FieldType> Iterator;
	};

	FIterator begin() const
	{
		return FIterator(Begin);
	}

	FIterator end() const
	{
		return FIterator(Begin);
	}

private:
	TFieldIterator<FieldType> Begin;
};
