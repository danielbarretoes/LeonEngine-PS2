#pragma once

// Object creation and lookup, FObjectInitializer and the tables of the generated reflection code
// (UE: UObject/UObjectGlobals.h).

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Script.h"

class FOutputDevice;
class UClass;
class UEnum;
class UFunction;
class UObject;
class UPackage;
class UScriptStruct;
struct FClassFunctionLinkInfo;
struct FCppClassTypeInfoStatic;

/** Search every package instead of one outer (UE: ANY_PACKAGE). */
#define ANY_PACKAGE ((UPackage*)-1)

/** Argument of the vtable helper constructors (UE: FVTableHelper). */
struct COREUOBJECT_API FVTableHelper
{
	FVTableHelper();
};

/**
 * Handed to every UObject constructor while the object system constructs an object (UE: FObjectInitializer). It knows
 * the object and its archetype, creates default subobjects, and when it goes out of scope finishes the construction:
 * property initialization from the defaults, then PostInitProperties.
 */
class COREUOBJECT_API FObjectInitializer
{
public:
	/** An initializer that constructs nothing (UE: a placeholder for code that calls constructors directly). */
	FObjectInitializer();

	/** Starts constructing InObj (memory from StaticAllocateObject); becomes the current initializer. */
	FObjectInitializer(UObject* InObj, UObject* InObjectArchetype, bool bInCopyTransientsFromClassDefaults = false,
		bool bInShouldInitializeProps = true);

	/** Finishes the construction (UE: PostConstructInit) and stops being the current initializer. */
	~FObjectInitializer();

	FObjectInitializer(const FObjectInitializer&) = delete;
	FObjectInitializer& operator=(const FObjectInitializer&) = delete;

	/** The object being constructed. */
	FORCEINLINE UObject* GetObj() const
	{
		return Obj;
	}

	/** The object its properties are initialized from (a class default object or a template). */
	FORCEINLINE UObject* GetArchetype() const
	{
		return ObjectArchetype;
	}

	/** The class of the object being constructed. */
	UClass* GetClass() const;

	/** The initializer of the object under construction (UE); fatal when nothing is being constructed. */
	static FObjectInitializer& Get();

	/**
	 * Creates a default subobject of Outer, which must be the object under construction (UE). The class is
	 * ClassToCreateByDefault unless a derived constructor overrode it with SetDefaultSubobjectClass; returns nullptr
	 * when DoNotCreateDefaultSubobject was used on an optional subobject.
	 */
	UObject* CreateDefaultSubobject(UObject* Outer, FName SubobjectFName, UClass* ReturnType,
		UClass* ClassToCreateByDefault, bool bIsRequired, bool bIsTransient = false) const;

	template <class TReturnType>
	TReturnType* CreateDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(
			CreateDefaultSubobject(Outer, SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/true, bTransient));
	}

	template <class TReturnType, class TClassToCreateByDefault>
	TReturnType* CreateDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const
	{
		return static_cast<TReturnType*>(CreateDefaultSubobject(Outer, SubobjectName, TReturnType::StaticClass(),
			TClassToCreateByDefault::StaticClass(), /*bIsRequired =*/true, bTransient));
	}

	/** A subobject a derived class may skip with DoNotCreateDefaultSubobject (UE). */
	template <class TReturnType>
	TReturnType* CreateOptionalDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(
			CreateDefaultSubobject(Outer, SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/false, bTransient));
	}

	/** A subobject that only exists with WITH_EDITORONLY_DATA (UE). */
	template <class TReturnType>
	TReturnType* CreateEditorOnlyDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const
	{
#if WITH_EDITORONLY_DATA
		return CreateDefaultSubobject<TReturnType>(Outer, SubobjectName, bTransient);
#else
		(void)Outer;
		(void)SubobjectName;
		(void)bTransient;
		return nullptr;
#endif
	}

	/**
	 * Makes a base-class CreateDefaultSubobject(SubobjectName) create Class instead: used in a derived constructor as
	 * Super(ObjectInitializer.SetDefaultSubobjectClass<UMyComponent>(TEXT("Mesh"))) (UE).
	 */
	const FObjectInitializer& SetDefaultSubobjectClass(FName SubobjectName, UClass* Class) const;

	template <class T>
	const FObjectInitializer& SetDefaultSubobjectClass(FName SubobjectName) const
	{
		return SetDefaultSubobjectClass(SubobjectName, T::StaticClass());
	}

	/** Makes a base-class CreateOptionalDefaultSubobject(SubobjectName) return nullptr (UE). */
	const FObjectInitializer& DoNotCreateDefaultSubobject(FName SubobjectName) const;

	/** True while Outer's constructor runs (used to reject NewObject without a name there, as UE does). */
	static bool IsInConstructor(const UObject* Outer);

private:
	/** A SetDefaultSubobjectClass / DoNotCreateDefaultSubobject entry. */
	struct FOverride
	{
		FName Name;
		UClass* Class = nullptr;
		bool bDoNotCreate = false;
	};

	/** Property initialization from the archetype, PostInitProperties and clearing RF_NeedInitialization. */
	void PostConstructInit();

	/** Copies the properties the object takes from its archetype (UE: InitProperties). */
	static void InitProperties(
		UObject* Obj, UClass* DefaultsClass, UObject* DefaultData, bool bCopyTransientsFromClassDefaults);

	UObject* Obj = nullptr;
	UObject* ObjectArchetype = nullptr;
	bool bCopyTransientsFromClassDefaults = false;
	bool bShouldInitializePropsFromArchetype = false;
	/** True for the initializers pushed on the construction stack (not the placeholder). */
	bool bIsConstructing = false;
	mutable TArray<FOverride> Overrides;
};

/** Everything StaticConstructObject_Internal needs (UE: FStaticConstructObjectParameters). */
struct COREUOBJECT_API FStaticConstructObjectParameters
{
	/** The class of the object; must not be abstract. */
	const UClass* Class;
	/** Defaults to the transient package. */
	UObject* Outer = nullptr;
	/** NAME_None makes a unique name: "<ClassName>_<N>". */
	FName Name;
	EObjectFlags SetFlags = RF_NoFlags;
	/** Properties are copied from it (all of them); nullptr uses the class default object (config properties). */
	UObject* Template = nullptr;
	bool bCopyTransientsFromClassDefaults = false;

	explicit FStaticConstructObjectParameters(const UClass* InClass);
};

/** Creates and constructs an object; the implementation of NewObject (UE). */
COREUOBJECT_API UObject* StaticConstructObject_Internal(const FStaticConstructObjectParameters& Params);

/**
 * Allocates the memory of an object and records its class, outer, name and flags for the constructor (UE:
 * StaticAllocateObject). An existing object with the same name and outer is a fatal error (UE may replace it).
 */
COREUOBJECT_API UObject* StaticAllocateObject(const UClass* Class, UObject* InOuter, FName Name, EObjectFlags SetFlags);

/** Checks that NewObject's class argument derives from the requested C++ type. */
COREUOBJECT_API void CheckIsClassChildOf_Internal(const UClass* Parent, const UClass* Child);

/**
 * Creates an object of Class (a T or derived class) in Outer (UE: NewObject). Name NAME_None makes a unique name;
 * Template, when given, is copied into the new object instead of the class defaults.
 */
template <class T>
T* NewObject(UObject* Outer, const UClass* Class, FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags,
	UObject* Template = nullptr, bool bCopyTransientsFromClassDefaults = false)
{
	checkf(Name != NAME_None || !FObjectInitializer::IsInConstructor(Outer),
		"NewObject with an empty name cannot create default subobjects inside a constructor: use "
		"CreateDefaultSubobject");
#if DO_CHECK
	CheckIsClassChildOf_Internal(T::StaticClass(), Class);
#endif
	FStaticConstructObjectParameters Params(Class);
	Params.Outer = Outer;
	Params.Name = Name;
	Params.SetFlags = Flags;
	Params.Template = Template;
	Params.bCopyTransientsFromClassDefaults = bCopyTransientsFromClassDefaults;
	return static_cast<T*>(StaticConstructObject_Internal(Params));
}

COREUOBJECT_API UPackage* GetTransientPackage();

/** A T in Outer (the transient package by default) with a unique name (UE). */
template <class T>
T* NewObject(UObject* Outer = (UObject*)GetTransientPackage())
{
	checkf(!FObjectInitializer::IsInConstructor(Outer),
		"NewObject with an empty name cannot create default subobjects inside a constructor: use "
		"CreateDefaultSubobject");
	FStaticConstructObjectParameters Params(T::StaticClass());
	Params.Outer = Outer;
	return static_cast<T*>(StaticConstructObject_Internal(Params));
}

/** A T named Name in Outer (UE). */
template <class T>
T* NewObject(UObject* Outer, FName Name, EObjectFlags Flags = RF_NoFlags, UObject* Template = nullptr,
	bool bCopyTransientsFromClassDefaults = false)
{
	checkf(Name != NAME_None || !FObjectInitializer::IsInConstructor(Outer),
		"NewObject with an empty name cannot create default subobjects inside a constructor: use "
		"CreateDefaultSubobject");
	FStaticConstructObjectParameters Params(T::StaticClass());
	Params.Outer = Outer;
	Params.Name = Name;
	Params.SetFlags = Flags;
	Params.Template = Template;
	Params.bCopyTransientsFromClassDefaults = bCopyTransientsFromClassDefaults;
	return static_cast<T*>(StaticConstructObject_Internal(Params));
}

/**
 * Finds an object by name (UE: StaticFindObject). InOuter scopes the search (ANY_PACKAGE: every package); Name may be
 * a path ("/Script/CoreUObject.Object", "/Game/Map.Map:Level.Actor"), resolved from its package. ObjectClass (or a
 * derived class, unless bExactClass) filters the result; nullptr accepts any class.
 */
COREUOBJECT_API UObject* StaticFindObject(
	UClass* ObjectClass, UObject* InOuter, const TCHAR* Name, bool bExactClass = false);

/** Finds an object by exact name in one outer, or in every package with bAnyPackage (UE: StaticFindObjectFast). */
COREUOBJECT_API UObject* StaticFindObjectFast(UClass* ObjectClass, UObject* InOuter, FName InName,
	bool bExactClass = false, bool bAnyPackage = false, EObjectFlags ExclusiveFlags = RF_NoFlags,
	EInternalObjectFlags ExclusiveInternalFlags = EInternalObjectFlags::None);

template <class T>
T* FindObject(UObject* Outer, const TCHAR* Name, bool bExactClass = false)
{
	return (T*)StaticFindObject(T::StaticClass(), Outer, Name, bExactClass);
}

template <class T>
T* FindObjectChecked(UObject* Outer, const TCHAR* Name, bool bExactClass = false)
{
	UObject* Result = StaticFindObject(T::StaticClass(), Outer, Name, bExactClass);
	checkf(Result, "Failed to find object '%s'", Name);
	return (T*)Result;
}

template <class T>
T* FindObjectFast(UObject* Outer, FName Name, bool bExactClass = false, bool bAnyPackage = false)
{
	return (T*)StaticFindObjectFast(T::StaticClass(), Outer, Name, bExactClass, bAnyPackage);
}

/**
 * A name no object of Outer uses: "<BaseName or class name>_<N>", N from the class's counter (UE:
 * MakeUniqueObjectName; UE 4.27 counts per outer, Leon per class).
 */
COREUOBJECT_API FName MakeUniqueObjectName(UObject* Outer, const UClass* Class, FName InBaseName = NAME_None);

/** The package named PackageName ("/Game/Maps/Arena"), created when it does not exist (UE: CreatePackage). */
COREUOBJECT_API UPackage* CreatePackage(const TCHAR* PackageName);

/** The class default object of T, which holds the defaults of every T (UE: GetDefault). */
template <class T>
inline const T* GetDefault()
{
	return (const T*)T::StaticClass()->GetDefaultObject();
}

/** GetDefault, writable (UE: GetMutableDefault). */
template <class T>
inline T* GetMutableDefault()
{
	return (T*)T::StaticClass()->GetDefaultObject();
}

/**
 * The reflection tables LeonHeaderTool generates and the functions that turn them into UClass / UScriptStruct / UEnum /
 * UFunction / UPackage objects (UE 4.27: UE4CodeGen_Private). The field order of each params struct is the initializer
 * order of the generated code (LeonHeaderTool/README.md).
 */
namespace UE4CodeGen_Private
{
	/** The FProperty class of a params entry, plus flags (UE 4.27 values). */
	enum class EPropertyGenFlags : uint8
	{
		None = 0x00,

		// The first 6 bits are the property type.
		Byte = 0x00,
		Int8 = 0x01,
		Int16 = 0x02,
		Int = 0x03,
		Int64 = 0x04,
		UInt16 = 0x05,
		UInt32 = 0x06,
		UInt64 = 0x07,
		UnsizedInt = 0x08,
		UnsizedUInt = 0x09,
		Float = 0x0A,
		Double = 0x0B,
		Bool = 0x0C,
		SoftClass = 0x0D,
		WeakObject = 0x0E,
		LazyObject = 0x0F,
		SoftObject = 0x10,
		Class = 0x11,
		Object = 0x12,
		Interface = 0x13,
		Name = 0x14,
		Str = 0x15,
		Array = 0x16,
		Map = 0x17,
		Set = 0x18,
		Struct = 0x19,
		Delegate = 0x1A,
		InlineMulticastDelegate = 0x1B,
		SparseMulticastDelegate = 0x1C,
		Text = 0x1D,
		Enum = 0x1E,
		FieldPath = 0x1F,

		// Property-specific flags.
		NativeBool = 0x20,
	};
	ENUM_CLASS_FLAGS(EPropertyGenFlags)

	/** Masks out the flags of an EPropertyGenFlags, leaving the type. */
	constexpr EPropertyGenFlags PropertyTypeMask = (EPropertyGenFlags)0x1F;

	/** The fields every property params struct starts with. */
	struct FPropertyParamsBase
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
	};

	/** The base fields plus the offset in the owner. */
	struct FPropertyParamsBaseWithOffset
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		int32 Offset;
	};

	struct FGenericPropertyParams
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		int32 Offset;
	};

	struct FBytePropertyParams
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		int32 Offset;
		UEnum* (*EnumFunc)();
	};

	struct FBoolPropertyParams
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		/** sizeof(bool), or of the bitfield's storage type. */
		uint32 ElementSize;
		/** sizeof(owner); 0 for a container element. */
		SIZE_T SizeOfOuter;
		/** Sets the bit in a zeroed owner, which locates it; nullptr for a container element. */
		void (*SetBitFunc)(void* Obj);
	};

	struct FObjectPropertyParams
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		int32 Offset;
		UClass* (*ClassFunc)();
	};

	struct FClassPropertyParams
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		int32 Offset;
		UClass* (*MetaClassFunc)();
		UClass* (*ClassFunc)();
	};

	struct FSoftClassPropertyParams
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		int32 Offset;
		UClass* (*MetaClassFunc)();
	};

	struct FStructPropertyParams
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		int32 Offset;
		UScriptStruct* (*ScriptStructFunc)();
	};

	struct FArrayPropertyParams
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		int32 Offset;
		EArrayPropertyFlags ArrayFlags;
	};

	struct FMapPropertyParams
	{
		const char* NameUTF8;
		const char* RepNotifyFuncUTF8;
		EPropertyFlags PropertyFlags;
		EPropertyGenFlags Flags;
		EObjectFlags ObjectFlags;
		int32 ArrayDim;
		int32 Offset;
		EMapPropertyFlags MapFlags;
	};

	typedef FGenericPropertyParams FInt8PropertyParams;
	typedef FGenericPropertyParams FInt16PropertyParams;
	typedef FGenericPropertyParams FIntPropertyParams;
	typedef FGenericPropertyParams FInt64PropertyParams;
	typedef FGenericPropertyParams FUInt16PropertyParams;
	typedef FGenericPropertyParams FUInt32PropertyParams;
	typedef FGenericPropertyParams FUInt64PropertyParams;
	typedef FGenericPropertyParams FUnsizedIntPropertyParams;
	typedef FGenericPropertyParams FUnsizedUIntPropertyParams;
	typedef FGenericPropertyParams FFloatPropertyParams;
	typedef FGenericPropertyParams FDoublePropertyParams;
	typedef FGenericPropertyParams FNamePropertyParams;
	typedef FGenericPropertyParams FStrPropertyParams;
	typedef FGenericPropertyParams FSetPropertyParams;
	typedef FGenericPropertyParams FTextPropertyParams;
	typedef FBytePropertyParams FEnumPropertyParams;
	typedef FObjectPropertyParams FWeakObjectPropertyParams;
	typedef FObjectPropertyParams FLazyObjectPropertyParams;
	typedef FObjectPropertyParams FSoftObjectPropertyParams;

	struct FEnumeratorParam
	{
		const char* NameUTF8;
		int64 Value;
	};

	struct FEnumParams
	{
		UObject* (*OuterFunc)();
		FText (*DisplayNameFunc)(int32);
		const char* NameUTF8;
		const char* CppTypeUTF8;
		const FEnumeratorParam* EnumeratorParams;
		int32 NumEnumerators;
		EObjectFlags ObjectFlags;
		EEnumFlags EnumFlags;
		uint8 CppForm;
	};

	struct FStructParams
	{
		UObject* (*OuterFunc)();
		UScriptStruct* (*SuperFunc)();
		void* (*StructOpsFunc)();
		const char* NameUTF8;
		SIZE_T SizeOf;
		SIZE_T AlignOf;
		const FPropertyParamsBase* const* PropertyArray;
		int32 NumProperties;
		EObjectFlags ObjectFlags;
		uint32 StructFlags;
	};

	struct FFunctionParams
	{
		UObject* (*OuterFunc)();
		UFunction* (*SuperFunc)();
		const char* NameUTF8;
		const char* OwningClassName;
		const char* DelegateName;
		SIZE_T StructureSize;
		const FPropertyParamsBase* const* PropertyArray;
		int32 NumProperties;
		EObjectFlags ObjectFlags;
		EFunctionFlags FunctionFlags;
		uint16 RPCId;
		uint16 RPCResponseId;
	};

	struct FImplementedInterfaceParams
	{
		UClass* (*ClassFunc)();
		int32 Offset;
		bool bImplementedByK2;
	};

	struct FPackageParams
	{
		const char* NameUTF8;
		UObject* (*const* SingletonFuncArray)();
		int32 NumSingletons;
		uint32 PackageFlags;
		uint32 BodyCRC;
		uint32 DeclarationsCRC;
	};

	struct FClassParams
	{
		UClass* (*ClassNoRegisterFunc)();
		/** nullptr: inherit the super class's config name. */
		const char* ClassConfigNameUTF8;
		const FCppClassTypeInfoStatic* CppClassInfo;
		UObject* (*const* DependencySingletonFuncArray)();
		const FClassFunctionLinkInfo* FunctionLinkArray;
		const FPropertyParamsBase* const* PropertyArray;
		/** Always nullptr (no interfaces). */
		const FImplementedInterfaceParams* ImplementedInterfaceArray;
		int32 NumDependencySingletons;
		int32 NumFunctions;
		int32 NumProperties;
		int32 NumImplementedInterfaces;
		uint32 ClassFlags;
	};

	COREUOBJECT_API void ConstructUFunction(UFunction*& OutFunction, const FFunctionParams& Params);
	COREUOBJECT_API void ConstructUEnum(UEnum*& OutEnum, const FEnumParams& Params);
	COREUOBJECT_API void ConstructUScriptStruct(UScriptStruct*& OutStruct, const FStructParams& Params);
	COREUOBJECT_API void ConstructUPackage(UPackage*& OutPackage, const FPackageParams& Params);
	COREUOBJECT_API void ConstructUClass(UClass*& OutClass, const FClassParams& Params);
} // namespace UE4CodeGen_Private
