#pragma once

// The reflection types: UField, UStruct, UScriptStruct, UClass, UEnum and UFunction (UE: UObject/Class.h).

#include "CoreMinimal.h"
#include "Templates/AlignmentTemplates.h"
#include "UObject/Field.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Script.h"
#include "UObject/UObjectGlobals.h"

#include <new>
#include <type_traits>

class FArchive;
class FProperty;
struct FFrame;

/**
 * Base of the reflection objects that are UObjects: structs, classes, functions and enums (UE: UField). Functions of
 * a class form a list through Next.
 */
class COREUOBJECT_API UField : public UObject
{
	DECLARE_CASTED_CLASS_INTRINSIC(UField, UObject, CLASS_Abstract, TEXT("/Script/CoreUObject"), CASTCLASS_UField)

public:
	/** The next field of the owner (UStruct::Children). */
	UField* Next;

	UField(EStaticConstructor, EObjectFlags InFlags);
	explicit UField(const FObjectInitializer& ObjectInitializer);

	/** Takes a property constructed with this field as its owner (UStruct links it into ChildProperties). */
	virtual void AddCppProperty(FProperty* Property);

	/** Resolves native pointers once the field is built (UFunction finds its exec thunk) (UE). */
	virtual void Bind();

	/** The class this field belongs to (itself for a class). */
	UClass* GetOwnerClass() const;

	/** The struct this field belongs to (itself for a struct). */
	UStruct* GetOwnerStruct() const;
};

/** Which fields TFieldIterator visits (UE: EFieldIteratorFlags). */
namespace EFieldIteratorFlags
{
	enum SuperClassFlags
	{
		ExcludeSuper = 0,
		IncludeSuper
	};

	enum DeprecatedPropertyFlags
	{
		ExcludeDeprecated = 0,
		IncludeDeprecated
	};
} // namespace EFieldIteratorFlags

/**
 * A reflected layout: the properties (ChildProperties) and functions (Children) of a struct, class or function, with
 * the size and alignment of an instance (UE: UStruct).
 */
class COREUOBJECT_API UStruct : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC(
		UStruct, UField, CLASS_MatchedSerializers, TEXT("/Script/CoreUObject"), CASTCLASS_UStruct)

public:
	/** UFunctions of a class (UFields). */
	UField* Children;

	/** Properties declared by this struct itself (not its supers), in declaration order. */
	FField* ChildProperties;

	/** sizeof of an instance, as C++ sees it. */
	int32 PropertiesSize;

	/** alignof of an instance. */
	int32 MinAlignment;

	/** Every property, this struct's first, then its supers' (set by Link). */
	FProperty* PropertyLink;

	/** The properties that hold object references, supers included (for the P10 garbage collector). */
	FProperty* RefLink;

	/** The properties that need a destructor call, supers included. */
	FProperty* DestructorLink;

	/**
	 * The properties an instance copies from its class default object after its constructor: for native classes, the
	 * config properties (UE: PostConstructLink).
	 */
	FProperty* PostConstructLink;

	UStruct(EStaticConstructor, int32 InSize, int32 InAlignment, EObjectFlags InFlags);
	explicit UStruct(const FObjectInitializer& ObjectInitializer, UStruct* InSuperStruct = nullptr,
		SIZE_T ParamsSize = 0, SIZE_T Alignment = 0);
	virtual ~UStruct() override;

	/** Builds the property chains and the computed flags (UE: Link). Leon links compiled-in layouts only. */
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties);

	/** Link without an archive (UE: StaticLink). */
	void StaticLink(bool bRelinkExistingProperties = false);

	/** Constructs ArrayDim instances at Dest (memory of GetStructureSize() * ArrayDim bytes) (UE). */
	virtual void InitializeStruct(void* Dest, int32 ArrayDim = 1) const;

	/** Destroys ArrayDim instances at Dest (UE). */
	virtual void DestroyStruct(void* Dest, int32 ArrayDim = 1) const;

	virtual void SetSuperStruct(UStruct* NewSuperStruct);

	virtual void AddCppProperty(FProperty* Property) override;

	FORCEINLINE UStruct* GetSuperStruct() const
	{
		return SuperStruct;
	}

	/** The struct this one inherits its layout from (UE). */
	FORCEINLINE UStruct* GetInheritanceSuper() const
	{
		return SuperStruct;
	}

	FORCEINLINE int32 GetPropertiesSize() const
	{
		return PropertiesSize;
	}

	FORCEINLINE int32 GetMinAlignment() const
	{
		return MinAlignment;
	}

	/** The size of an instance rounded up to its alignment (UE). */
	FORCEINLINE int32 GetStructureSize() const
	{
		return Align(PropertiesSize, MinAlignment);
	}

	FORCEINLINE void SetPropertiesSize(int32 NewSize)
	{
		PropertiesSize = NewSize;
	}

	/** True when this struct is SomeBase or derives from it (UE). */
	bool IsChildOf(const UStruct* SomeBase) const;

	template <class T>
	bool IsChildOf() const
	{
		return IsChildOf(T::StaticClass());
	}

	/** The property named InName, supers included, or nullptr (UE). */
	FProperty* FindPropertyByName(FName InName) const;

private:
	UStruct* SuperStruct;
};

/** Default traits of a struct's C++ operations; specialize TStructOpsTypeTraits to change them (UE). */
template <class CPPSTRUCT>
struct TStructOpsTypeTraitsBase2
{
	enum
	{
		/** memset(0) builds a valid instance. */
		WithZeroConstructor = false,
		/** The destructor does nothing. */
		WithNoDestructor = false,
		/** Copy with operator=. */
		WithCopy = !TIsPODType<CPPSTRUCT>::Value,
		/** Identical uses operator==. */
		WithIdenticalViaEquality = false,
		/** Identical uses bool Identical(const CPPSTRUCT* Other, uint32 PortFlags) const. */
		WithIdentical = false,
	};
};

template <class CPPSTRUCT>
struct TStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<CPPSTRUCT>
{
};

namespace UE::CoreUObject::Private
{
	/** True when GetTypeHash(const T&) resolves. */
	template <typename T, typename = void>
	struct THasGetTypeHash : std::false_type
	{
	};

	template <typename T>
	struct THasGetTypeHash<T, std::void_t<decltype(GetTypeHash(*(const T*)nullptr))>> : std::true_type
	{
	};
} // namespace UE::CoreUObject::Private

/** A reflected C++ struct (USTRUCT) with its C++ construct / destruct / copy / compare operations (UE: UScriptStruct).
 */
class COREUOBJECT_API UScriptStruct : public UStruct
{
	DECLARE_CASTED_CLASS_INTRINSIC(UScriptStruct, UStruct, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UScriptStruct)

public:
	/** The C++ operations of a struct type, erased (UE: UScriptStruct::ICppStructOps). */
	struct COREUOBJECT_API ICppStructOps
	{
		ICppStructOps(int32 InSize, int32 InAlignment)
			: Size(InSize)
			, Alignment(InAlignment)
		{
		}
		virtual ~ICppStructOps() = default;

		virtual bool HasZeroConstructor() = 0;
		/** Default-constructs one instance at Dest (value-initialized: `new (Dest) T()`). */
		virtual void Construct(void* Dest) = 0;
		virtual bool HasDestructor() = 0;
		virtual void Destruct(void* Dest) = 0;
		virtual bool IsPlainOldData() = 0;
		virtual bool HasCopy() = 0;
		/** Copies ArrayDim instances with operator=; false when the type has no copy. */
		virtual bool Copy(void* Dest, void const* Src, int32 ArrayDim) = 0;
		virtual bool HasIdentical() = 0;
		/** Compares with the type's own comparison; false when it has none. */
		virtual bool Identical(const void* A, const void* B, uint32 PortFlags, bool& bOutResult) = 0;
		virtual bool HasGetTypeHash() = 0;
		virtual uint32 GetStructTypeHash(const void* Src) = 0;

		FORCEINLINE int32 GetSize() const
		{
			return Size;
		}

		FORCEINLINE int32 GetAlignment() const
		{
			return Alignment;
		}

	private:
		const int32 Size;
		const int32 Alignment;
	};

	/** ICppStructOps of CPPSTRUCT, driven by TStructOpsTypeTraits<CPPSTRUCT> (UE: UScriptStruct::TCppStructOps). */
	template <class CPPSTRUCT>
	struct TCppStructOps : public ICppStructOps
	{
		typedef TStructOpsTypeTraits<CPPSTRUCT> TTraits;

		TCppStructOps()
			: ICppStructOps(sizeof(CPPSTRUCT), alignof(CPPSTRUCT))
		{
		}

		virtual bool HasZeroConstructor() override
		{
			return TTraits::WithZeroConstructor;
		}

		virtual void Construct(void* Dest) override
		{
			check(!TTraits::WithZeroConstructor);
			::new (Dest) CPPSTRUCT();
		}

		virtual bool HasDestructor() override
		{
			return !(TTraits::WithNoDestructor || std::is_trivially_destructible_v<CPPSTRUCT>);
		}

		virtual void Destruct(void* Dest) override
		{
			((CPPSTRUCT*)Dest)->~CPPSTRUCT();
		}

		virtual bool IsPlainOldData() override
		{
			return TIsPODType<CPPSTRUCT>::Value;
		}

		virtual bool HasCopy() override
		{
			return TTraits::WithCopy;
		}

		virtual bool Copy(void* Dest, void const* Src, int32 ArrayDim) override
		{
			if constexpr (std::is_copy_assignable_v<CPPSTRUCT>)
			{
				CPPSTRUCT* TypedDest = (CPPSTRUCT*)Dest;
				const CPPSTRUCT* TypedSrc = (const CPPSTRUCT*)Src;
				for (; ArrayDim; --ArrayDim)
				{
					*TypedDest++ = *TypedSrc++;
				}
				return true;
			}
			else
			{
				(void)Dest;
				(void)Src;
				(void)ArrayDim;
				return false;
			}
		}

		virtual bool HasIdentical() override
		{
			return TTraits::WithIdenticalViaEquality || TTraits::WithIdentical;
		}

		virtual bool Identical(const void* A, const void* B, uint32 PortFlags, bool& bOutResult) override
		{
			if constexpr (TTraits::WithIdenticalViaEquality)
			{
				(void)PortFlags;
				bOutResult = (*(const CPPSTRUCT*)A == *(const CPPSTRUCT*)B);
				return true;
			}
			else if constexpr (TTraits::WithIdentical)
			{
				bOutResult = ((const CPPSTRUCT*)A)->Identical((const CPPSTRUCT*)B, PortFlags);
				return true;
			}
			else
			{
				(void)A;
				(void)B;
				(void)PortFlags;
				bOutResult = false;
				return false;
			}
		}

		virtual bool HasGetTypeHash() override
		{
			return UE::CoreUObject::Private::THasGetTypeHash<CPPSTRUCT>::value;
		}

		virtual uint32 GetStructTypeHash(const void* Src) override
		{
			if constexpr (UE::CoreUObject::Private::THasGetTypeHash<CPPSTRUCT>::value)
			{
				return GetTypeHash(*(const CPPSTRUCT*)Src);
			}
			else
			{
				// Only called when HasGetTypeHash() is true.
				(void)Src;
				return 0;
			}
		}
	};

	EStructFlags StructFlags;

	UScriptStruct(EStaticConstructor, int32 InSize, int32 InAlignment, EObjectFlags InFlags);
	explicit UScriptStruct(const FObjectInitializer& ObjectInitializer, UScriptStruct* InSuperStruct = nullptr,
		ICppStructOps* InCppStructOps = nullptr, EStructFlags InStructFlags = STRUCT_NoFlags, SIZE_T ExplicitSize = 0,
		SIZE_T ExplicitAlignment = 0);
	virtual ~UScriptStruct() override;

	/** Takes ownership of the struct's C++ operations and derives the computed flags from them. */
	void SetCppStructOps(ICppStructOps* InCppStructOps);

	FORCEINLINE ICppStructOps* GetCppStructOps() const
	{
		return CppStructOps;
	}

	/** Copies ArrayDim instances (C++ copy when available, else property by property) (UE). */
	void CopyScriptStruct(void* Dest, void const* Src, int32 ArrayDim = 1) const;

	/** Resets ArrayDim instances to their default state (UE). */
	void ClearScriptStruct(void* Dest, int32 ArrayDim = 1) const;

	/** Compares two instances (C++ comparison when the traits provide one, else property by property) (UE). */
	bool CompareScriptStruct(const void* A, const void* B, uint32 PortFlags) const;

	/** GetTypeHash of an instance; only for structs whose C++ type has one (UE). */
	uint32 GetStructTypeHash(const void* Src) const;

	/** "F" + the struct name: the C++ type name (UE). */
	FString GetStructCPPName() const;

	virtual void InitializeStruct(void* Dest, int32 ArrayDim = 1) const override;
	virtual void DestroyStruct(void* Dest, int32 ArrayDim = 1) const override;
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;

private:
	ICppStructOps* CppStructOps;
};

/** The UScriptStruct of a noexport Core type or of a USTRUCT (UE: TBaseStructure). */
template <class T>
struct TBaseStructure
{
	static UScriptStruct* Get()
	{
		return StaticStruct<T>();
	}
};

/** A native function of a class, by name, filled by StaticRegisterNatives<Class> (UE: FNativeFunctionLookup). */
struct FNativeFunctionLookup
{
	FName Name;
	FNativeFuncPtr Pointer;

	FNativeFunctionLookup(FName InName, FNativeFuncPtr InPointer)
		: Name(InName)
		, Pointer(InPointer)
	{
	}
};

/** One UFUNCTION of a class for ConstructUClass (UE: FClassFunctionLinkInfo). */
struct FClassFunctionLinkInfo
{
	UFunction* (*CreateFuncPtr)();
	const char* FuncNameUTF8;
};

/** C++ facts about a class the generated code records (UE: FCppClassTypeInfoStatic). */
struct FCppClassTypeInfoStatic
{
	bool bIsAbstract;
};

/** C++ traits of a class (UE: TCppClassTypeTraits). */
template <class CPPCLASS>
struct TCppClassTypeTraits
{
	enum
	{
		IsAbstract = std::is_abstract_v<CPPCLASS>
	};
};

/** A native function name and its exec thunk (UE: FNameNativePtrPair). */
struct FNameNativePtrPair
{
	const char* NameUTF8;
	FNativeFuncPtr Pointer;
};

/** Registers the exec thunks of a class (UE: FNativeFunctionRegistrar). */
struct COREUOBJECT_API FNativeFunctionRegistrar
{
	static void RegisterFunction(UClass* Class, const ANSICHAR* InName, FNativeFuncPtr InPointer);
	static void RegisterFunctions(UClass* Class, const FNameNativePtrPair* InArray, int32 NumFunctions);
};

/**
 * A reflected C++ class (UE: UClass): its constructor, flags, default object and functions. Compiled-in classes get
 * their UClass from IMPLEMENT_CLASS and their reflection data from ConstructUClass.
 */
class COREUOBJECT_API UClass : public UStruct
{
	DECLARE_CASTED_CLASS_INTRINSIC(UClass, UStruct, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UClass)

public:
	typedef void (*ClassConstructorType)(const FObjectInitializer&);
	typedef UObject* (*ClassVTableHelperCtorCallerType)(FVTableHelper& Helper);
	typedef UClass* (*StaticClassFunctionType)();

	/** Constructs an instance in the memory of FObjectInitializer::GetObj() (InternalConstructor<T>). */
	ClassConstructorType ClassConstructor;
	ClassVTableHelperCtorCallerType ClassVTableHelperCtorCaller;

	/** Counter of MakeUniqueObjectName. */
	mutable int32 ClassUnique;

	EClassFlags ClassFlags;

	/** The class's own cast bit plus its supers' (Cast<T> fast path). */
	EClassCastFlags ClassCastFlags;

	/** The config file of the class ("Engine", "Game") (P10: LoadConfig). */
	FName ClassConfigName;

	/** The exec thunks of the class, by function name. */
	TArray<FNativeFunctionLookup> NativeFunctionLookupTable;

	/** The class default object (CDO); created by GetDefaultObject. */
	UObject* ClassDefaultObject;

	UClass(EStaticConstructor, FName InName, uint32 InSize, uint32 InAlignment, EClassFlags InClassFlags,
		EClassCastFlags InClassCastFlags, const TCHAR* InClassConfigName, EObjectFlags InFlags,
		ClassConstructorType InClassConstructor, ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller);
	explicit UClass(const FObjectInitializer& ObjectInitializer);

	/** The class default object, created (super class's first) when bCreateIfNeeded (UE). */
	UObject* GetDefaultObject(bool bCreateIfNeeded = true) const
	{
		if (ClassDefaultObject == nullptr && bCreateIfNeeded)
		{
			const_cast<UClass*>(this)->CreateDefaultObject();
		}
		return ClassDefaultObject;
	}

	template <class T>
	T* GetDefaultObject() const
	{
		UObject* Ret = GetDefaultObject();
		checkf(Ret->IsA(T::StaticClass()), "The default object of %s is not a %s", *GetName(),
			*T::StaticClass()->GetName());
		return (T*)Ret;
	}

	/** "Default__" + the class name (UE). */
	FName GetDefaultObjectName() const;

	FORCEINLINE UClass* GetSuperClass() const
	{
		return (UClass*)GetSuperStruct();
	}

	/** The UFunction named InName, in this class or (IncludeSuper) its supers (UE). */
	UFunction* FindFunctionByName(
		FName InName, EIncludeSuperFlag::Type IncludeSuper = EIncludeSuperFlag::IncludeSuper) const;

	void AddFunctionToFunctionMap(UFunction* Function, FName FuncName);

	/** Records an exec thunk; UFunction::Bind finds it by name (UE). */
	void AddNativeFunction(const ANSICHAR* InName, FNativeFuncPtr InPointer);

	/** Creates the class's UFunctions and adds them to Children and the function map (UE). */
	void CreateLinkAndAddChildFunctionsToMap(const FClassFunctionLinkInfo* Functions, uint32 NumFunctions);

	FORCEINLINE bool HasAnyClassFlags(EClassFlags FlagsToCheck) const
	{
		return (ClassFlags & FlagsToCheck) != 0;
	}

	FORCEINLINE bool HasAllClassFlags(EClassFlags FlagsToCheck) const
	{
		return (ClassFlags & FlagsToCheck) == FlagsToCheck;
	}

	FORCEINLINE EClassFlags GetClassFlags() const
	{
		return ClassFlags;
	}

	FORCEINLINE bool HasAnyCastFlag(EClassCastFlags FlagToCheck) const
	{
		return (ClassCastFlags & FlagToCheck) != 0;
	}

	FORCEINLINE bool HasAllCastFlags(EClassCastFlags FlagsToCheck) const
	{
		return (ClassCastFlags & FlagsToCheck) == FlagsToCheck;
	}

	/** The C++ facts of the class (abstract or not), from the generated code; nullptr for intrinsic classes. */
	FORCEINLINE const FCppClassTypeInfoStatic* GetCppTypeInfoStatic() const
	{
		return CppTypeInfoStatic;
	}

	FORCEINLINE void SetCppTypeInfoStatic(const FCppClassTypeInfoStatic* InCppTypeInfoStatic)
	{
		CppTypeInfoStatic = InCppTypeInfoStatic;
	}

	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	virtual void SetSuperStruct(UStruct* NewSuperStruct) override;

protected:
	/** Creates the class default object: named Default__<Class>, in the class's package (UE). */
	virtual UObject* CreateDefaultObject();

private:
	TMap<FName, UFunction*> FuncMap;
	const FCppClassTypeInfoStatic* CppTypeInfoStatic;
};

/** How UEnum::GetIndexByName matches names (UE: EGetByNameFlags). */
enum class EGetByNameFlags
{
	None = 0,
	/** Report an error when the name is not found. */
	ErrorIfNotFound = 0x01,
	/** Compare case-sensitively (FName comparison ignores case otherwise). */
	CaseSensitive = 0x02,
};
ENUM_CLASS_FLAGS(EGetByNameFlags)

/**
 * A reflected enum (UENUM) (UE: UEnum). Names are stored as C++ spells them: "EMyEnum::Value" for an enum class,
 * "Value" for a regular enum, plus the <Prefix>_MAX entry UE appends.
 */
class COREUOBJECT_API UEnum : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC(UEnum, UField, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UEnum)

public:
	/** How the enum is declared in C++ (UE: UEnum::ECppForm). */
	enum class ECppForm
	{
		/** enum EName { Value }: names are "Value". */
		Regular,
		/** namespace EName { enum Type { Value } }: names are "EName::Value". */
		Namespaced,
		/** enum class EName { Value }: names are "EName::Value". */
		EnumClass
	};

	/** The C++ type name ("EMyEnum" or "TEnumAsByte<EMyEnum>"). */
	FString CppType;

	explicit UEnum(const FObjectInitializer& ObjectInitializer);

	/**
	 * Sets the names and values. With bAddMaxKeyIfMissing a "<Prefix>_MAX" entry (value: the largest value + 1) is
	 * appended when the names have none, as UE does (UE: SetEnums).
	 */
	virtual bool SetEnums(TArray<TPair<FName, int64>>& InNames, ECppForm InCppForm,
		EEnumFlags InFlags = EEnumFlags::None, bool bAddMaxKeyIfMissing = true);

	/** Number of names, _MAX included (UE). */
	FORCEINLINE int32 NumEnums() const
	{
		return Names.Num();
	}

	FName GetNameByIndex(int32 Index) const;
	int64 GetValueByIndex(int32 Index) const;

	/** The name of Value, or NAME_None (UE). */
	FName GetNameByValue(int64 InValue) const;

	/** The value of Name ("EMyEnum::Value", or "Value" for enum classes too), or INDEX_NONE (UE). */
	int64 GetValueByName(FName InName, EGetByNameFlags Flags = EGetByNameFlags::None) const;

	/** The index of Name, or INDEX_NONE (UE). */
	int32 GetIndexByName(FName InName, EGetByNameFlags Flags = EGetByNameFlags::None) const;

	/** The index of Value, or INDEX_NONE (UE). */
	int32 GetIndexByValue(int64 InValue) const;

	/** The name at Index without the "EMyEnum::" scope ("Value") (UE). */
	FString GetNameStringByIndex(int32 InIndex) const;

	/** The short name of Value, or an empty string (UE). */
	FString GetNameStringByValue(int64 InValue) const;

	/** The value of a short or full name string, or INDEX_NONE (UE). */
	int64 GetValueByNameString(const FString& SearchString, EGetByNameFlags Flags = EGetByNameFlags::None) const;

	/** The display text of an entry: the generated display-name function's, else the short name (UE). */
	FText GetDisplayNameTextByIndex(int32 InIndex) const;

	/** The largest value, the _MAX entry excluded when present... (UE: the largest value of all entries). */
	int64 GetMaxEnumValue() const;

	bool IsValidEnumValue(int64 InValue) const;
	bool IsValidEnumName(FName InName) const;

	FORCEINLINE ECppForm GetCppForm() const
	{
		return CppForm;
	}

	FORCEINLINE bool HasAnyEnumFlags(EEnumFlags InFlags) const
	{
		return EnumHasAnyFlags(EnumFlags, InFlags);
	}

	/** The longest common prefix of the names up to its last '_', or the enum name (UE). */
	FString GenerateEnumPrefix() const;

	/** "EMyEnum::Name" for scoped forms, "Name" for regular enums (UE). */
	FString GenerateFullEnumName(const TCHAR* InEnumName) const;

	/** The generated display-name function (nullptr in Leon: no metadata). */
	FText (*EnumDisplayNameFn)(int32);

protected:
	TArray<TPair<FName, int64>> Names;
	ECppForm CppForm;
	EEnumFlags EnumFlags;
};

/**
 * A reflected function (UFUNCTION) (UE: UFunction). Its properties are the parameters, in order, then ReturnValue; the
 * parameter block is laid out like the generated <Class>_event<Func>_Parms struct. Always native in Leon.
 */
class COREUOBJECT_API UFunction : public UStruct
{
	DECLARE_CASTED_CLASS_INTRINSIC(UFunction, UStruct, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UFunction)

public:
	EFunctionFlags FunctionFlags;

	/** Number of parameters, the return value included. */
	uint8 NumParms;

	/** Size of the parameter block. */
	uint16 ParmsSize;

	/** Offset of the return value in the parameter block, or MAX_uint16 without one. */
	uint16 ReturnValueOffset;

	explicit UFunction(const FObjectInitializer& ObjectInitializer, UFunction* InSuperFunction = nullptr,
		EFunctionFlags InFunctionFlags = FUNC_None, SIZE_T ParamsSize = 0);

	FORCEINLINE FNativeFuncPtr GetNativeFunc() const
	{
		return Func;
	}

	FORCEINLINE void SetNativeFunc(FNativeFuncPtr InFunc)
	{
		Func = InFunc;
	}

	/** Calls the exec thunk (UE). */
	void Invoke(UObject* Obj, FFrame& Stack, RESULT_DECL);

	/** The ReturnValue parameter, or nullptr (UE). */
	FProperty* GetReturnProperty() const;

	FORCEINLINE UFunction* GetSuperFunction() const
	{
		return (UFunction*)GetSuperStruct();
	}

	FORCEINLINE bool HasAnyFunctionFlags(EFunctionFlags FlagsToCheck) const
	{
		return (FunctionFlags & FlagsToCheck) != 0;
	}

	FORCEINLINE bool HasAllFunctionFlags(EFunctionFlags FlagsToCheck) const
	{
		return (FunctionFlags & FlagsToCheck) == FlagsToCheck;
	}

	/** Finds the exec thunk in the owner class's NativeFunctionLookupTable (UE). */
	virtual void Bind() override;

	/** Links the parameters and computes NumParms, ParmsSize and ReturnValueOffset (UE). */
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;

private:
	FNativeFuncPtr Func;
};

/** The UScriptStruct of a generated struct, constructed on first use (UE: GetStaticStruct). */
COREUOBJECT_API UScriptStruct* GetStaticStruct(
	UScriptStruct* (*InRegister)(), UObject* StructOuter, const TCHAR* StructName, SIZE_T Size, uint32 Crc);

/** The UEnum of a generated enum, constructed on first use (UE: GetStaticEnum). */
COREUOBJECT_API UEnum* GetStaticEnum(UEnum* (*InRegister)(), UObject* EnumOuter, const TCHAR* EnumName);
