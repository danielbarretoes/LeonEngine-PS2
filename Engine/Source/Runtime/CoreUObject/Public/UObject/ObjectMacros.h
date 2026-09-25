#pragma once

// Reflection macros, object / class / property flags and the class declaration boilerplate (UE:
// UObject/ObjectMacros.h). The macro half is the contract with LeonHeaderTool
// (Engine/Source/Programs/LeonHeaderTool/README.md).

#include "CoreMinimal.h"
#include "UObject/Script.h"

class FObjectInitializer;
class UClass;
class UEnum;
class UFunction;
class UObject;
class UPackage;
class UScriptStruct;
struct FVTableHelper;

/** C++ sees 1, LeonHeaderTool parses with 0: '#if !CPP' blocks hold declarations only the tool reads (UE: CPP). */
#ifndef CPP
	#define CPP 1
#endif

/** API macro of a generated class declared without one (UE: NO_API). */
#define NO_API

/** Placement-new tag for constructing UObjects in memory the object system allocated (UE: EInternal). */
enum EInternal
{
	EC_InternalUseOnlyConstructor
};

/** Tag of the constructors used to build intrinsic UClasses before the object system runs (UE: EStaticConstructor). */
enum EStaticConstructor
{
	EC_StaticConstructor
};

/**
 * Flags describing an object instance (UE: EObjectFlags). Stored in UObjectBase::ObjectFlags; the values match
 * UE 4.27, so generated code can combine them (RF_Public|RF_Transient|RF_MarkAsNative).
 */
enum EObjectFlags
{
	RF_NoFlags = 0x00000000,
	/** Visible outside its package. */
	RF_Public = 0x00000001,
	/** Kept even when unreferenced (P10 garbage collection). */
	RF_Standalone = 0x00000002,
	/** Native object: its memory is not tracked by the loader. */
	RF_MarkAsNative = 0x00000004,
	RF_Transactional = 0x00000008,
	/** The class default object (CDO). */
	RF_ClassDefaultObject = 0x00000010,
	/** A template for other objects: CDOs and their default subobjects. */
	RF_ArchetypeObject = 0x00000020,
	/** Never saved. */
	RF_Transient = 0x00000040,
	RF_MarkAsRootSet = 0x00000080,
	RF_TagGarbageTemp = 0x00000100,
	/** Constructed; FObjectInitializer has not finished initializing it. */
	RF_NeedInitialization = 0x00000200,
	RF_NeedLoad = 0x00000400,
	RF_KeepForCooker = 0x00000800,
	RF_NeedPostLoad = 0x00001000,
	RF_NeedPostLoadSubobjects = 0x00002000,
	RF_NewerVersionExists = 0x00004000,
	/** BeginDestroy has been called (P10). */
	RF_BeginDestroyed = 0x00008000,
	/** FinishDestroy has been called (P10). */
	RF_FinishDestroyed = 0x00010000,
	RF_BeingRegenerated = 0x00020000,
	/** Created by CreateDefaultSubobject. */
	RF_DefaultSubObject = 0x00040000,
	RF_WasLoaded = 0x00080000,
	RF_TextExportTransient = 0x00100000,
	RF_LoadCompleted = 0x00200000,
	RF_InheritableComponentTemplate = 0x00400000,
	RF_DuplicateTransient = 0x00800000,
	RF_StrongRefOnFrame = 0x01000000,
	RF_NonPIEDuplicateTransient = 0x02000000,
	RF_Dynamic = 0x04000000,
	RF_WillBeLoaded = 0x08000000,
};
ENUM_CLASS_FLAGS(EObjectFlags)

/** Flags a default subobject inherits from its outer (UE: RF_PropagateToSubObjects). */
#define RF_PropagateToSubObjects ((EObjectFlags)(RF_Public | RF_ArchetypeObject | RF_Transactional | RF_Transient))
/** Every flag (UE: RF_AllFlags). */
#define RF_AllFlags ((EObjectFlags)0x0fffffff)

/** Flags kept in the object's GUObjectArray item rather than in the object (UE: EInternalObjectFlags). */
enum class EInternalObjectFlags : int32
{
	None = 0,
	ReachableInCluster = 1 << 23,
	ClusterRoot = 1 << 24,
	/** A compiled-in (native) object: a class, struct, enum, function or package created by the registration. */
	Native = 1 << 25,
	Async = 1 << 26,
	AsyncLoading = 1 << 27,
	/** Found unreachable by the garbage collector (P10). */
	Unreachable = 1 << 28,
	/** Marked for destruction (P10). */
	PendingKill = 1 << 29,
	/** Never collected (P10: AddToRoot). */
	RootSet = 1 << 30,
	GarbageCollectionKeepFlags = Native | Async | AsyncLoading,
	AllFlags = ReachableInCluster | ClusterRoot | Native | Async | AsyncLoading | Unreachable | PendingKill | RootSet,
};
ENUM_CLASS_FLAGS(EInternalObjectFlags)

/** Flags describing a class (UE: EClassFlags, 4.27 values). */
enum EClassFlags
{
	CLASS_None = 0x00000000u,
	CLASS_Abstract = 0x00000001u,
	CLASS_DefaultConfig = 0x00000002u,
	CLASS_Config = 0x00000004u,
	CLASS_Transient = 0x00000008u,
	CLASS_Parsed = 0x00000010u,
	CLASS_MatchedSerializers = 0x00000020u,
	CLASS_ProjectUserConfig = 0x00000040u,
	CLASS_Native = 0x00000080u,
	CLASS_NoExport = 0x00000100u,
	CLASS_NotPlaceable = 0x00000200u,
	CLASS_PerObjectConfig = 0x00000400u,
	CLASS_ReplicationDataIsSetUp = 0x00000800u,
	CLASS_EditInlineNew = 0x00001000u,
	CLASS_CollapseCategories = 0x00002000u,
	CLASS_Interface = 0x00004000u,
	CLASS_CustomConstructor = 0x00008000u,
	CLASS_Const = 0x00010000u,
	CLASS_LayoutChanging = 0x00020000u,
	CLASS_CompiledFromBlueprint = 0x00040000u,
	CLASS_MinimalAPI = 0x00080000u,
	CLASS_RequiredAPI = 0x00100000u,
	CLASS_DefaultToInstanced = 0x00200000u,
	CLASS_TokenStreamAssembled = 0x00400000u,
	CLASS_HasInstancedReference = 0x00800000u,
	CLASS_Hidden = 0x01000000u,
	CLASS_Deprecated = 0x02000000u,
	CLASS_HideDropDown = 0x04000000u,
	CLASS_GlobalUserConfig = 0x08000000u,
	/** Declared by hand in C++, without generated code (UObject, UClass, ...). */
	CLASS_Intrinsic = 0x10000000u,
	/** Its reflection data (properties, functions) has been constructed. */
	CLASS_Constructed = 0x20000000u,
	CLASS_ConfigDoNotCheckDefaults = 0x40000000u,
	CLASS_NewerVersionExists = 0x80000000u,
};
ENUM_CLASS_FLAGS(EClassFlags)

/** Flags a class inherits from its super class (UE: CLASS_Inherit). */
#define CLASS_Inherit                                                                                                  \
	((EClassFlags)(CLASS_Transient | CLASS_DefaultConfig | CLASS_Config | CLASS_PerObjectConfig |                      \
		CLASS_ConfigDoNotCheckDefaults | CLASS_NotPlaceable | CLASS_Const | CLASS_HasInstancedReference |              \
		CLASS_Deprecated | CLASS_DefaultToInstanced | CLASS_GlobalUserConfig | CLASS_ProjectUserConfig))

/** The class flags of a compiled-in class declaration (UE: COMPILED_IN_FLAGS). */
#define COMPILED_IN_FLAGS(TStaticFlags) (TStaticFlags | CLASS_Intrinsic)

/**
 * One bit per engine class for fast Cast / IsA checks, and the ids of the FField classes (UE: EClassCastFlags, 4.27
 * values). A class's ClassCastFlags hold its own bit and all its supers' bits.
 */
enum EClassCastFlags : uint64
{
	CASTCLASS_None = 0x0000000000000000,
	CASTCLASS_UField = 0x0000000000000001,
	CASTCLASS_FInt8Property = 0x0000000000000002,
	CASTCLASS_UEnum = 0x0000000000000004,
	CASTCLASS_UStruct = 0x0000000000000008,
	CASTCLASS_UScriptStruct = 0x0000000000000010,
	CASTCLASS_UClass = 0x0000000000000020,
	CASTCLASS_FByteProperty = 0x0000000000000040,
	CASTCLASS_FIntProperty = 0x0000000000000080,
	CASTCLASS_FFloatProperty = 0x0000000000000100,
	CASTCLASS_FUInt64Property = 0x0000000000000200,
	CASTCLASS_FClassProperty = 0x0000000000000400,
	CASTCLASS_FUInt32Property = 0x0000000000000800,
	CASTCLASS_FInterfaceProperty = 0x0000000000001000,
	CASTCLASS_FNameProperty = 0x0000000000002000,
	CASTCLASS_FStrProperty = 0x0000000000004000,
	CASTCLASS_FProperty = 0x0000000000008000,
	CASTCLASS_FObjectProperty = 0x0000000000010000,
	CASTCLASS_FBoolProperty = 0x0000000000020000,
	CASTCLASS_FUInt16Property = 0x0000000000040000,
	CASTCLASS_UFunction = 0x0000000000080000,
	CASTCLASS_FStructProperty = 0x0000000000100000,
	CASTCLASS_FArrayProperty = 0x0000000000200000,
	CASTCLASS_FInt64Property = 0x0000000000400000,
	CASTCLASS_FDelegateProperty = 0x0000000000800000,
	CASTCLASS_FNumericProperty = 0x0000000001000000,
	CASTCLASS_FMulticastDelegateProperty = 0x0000000002000000,
	CASTCLASS_FObjectPropertyBase = 0x0000000004000000,
	CASTCLASS_FWeakObjectProperty = 0x0000000008000000,
	CASTCLASS_FLazyObjectProperty = 0x0000000010000000,
	CASTCLASS_FSoftObjectProperty = 0x0000000020000000,
	CASTCLASS_FTextProperty = 0x0000000040000000,
	CASTCLASS_FInt16Property = 0x0000000080000000,
	CASTCLASS_FDoubleProperty = 0x0000000100000000,
	CASTCLASS_FSoftClassProperty = 0x0000000200000000,
	CASTCLASS_UPackage = 0x0000000400000000,
	CASTCLASS_ULevel = 0x0000000800000000,
	CASTCLASS_AActor = 0x0000001000000000,
	CASTCLASS_APlayerController = 0x0000002000000000,
	CASTCLASS_APawn = 0x0000004000000000,
	CASTCLASS_USceneComponent = 0x0000008000000000,
	CASTCLASS_UPrimitiveComponent = 0x0000010000000000,
	CASTCLASS_USkinnedMeshComponent = 0x0000020000000000,
	CASTCLASS_USkeletalMeshComponent = 0x0000040000000000,
	CASTCLASS_UBlueprint = 0x0000080000000000,
	CASTCLASS_UDelegateFunction = 0x0000100000000000,
	CASTCLASS_UStaticMeshComponent = 0x0000200000000000,
	CASTCLASS_FMapProperty = 0x0000400000000000,
	CASTCLASS_FSetProperty = 0x0000800000000000,
	CASTCLASS_FEnumProperty = 0x0001000000000000,
	CASTCLASS_USparseDelegateFunction = 0x0002000000000000,
	CASTCLASS_FMulticastInlineDelegateProperty = 0x0004000000000000,
	CASTCLASS_FMulticastSparseDelegateProperty = 0x0008000000000000,
	CASTCLASS_FFieldPathProperty = 0x0010000000000000,
};
ENUM_CLASS_FLAGS(EClassCastFlags)

/** Flags describing a property (UE: EPropertyFlags, 4.27 values). */
enum EPropertyFlags : uint64
{
	CPF_None = 0,
	CPF_Edit = 0x0000000000000001,
	/** A const function parameter. */
	CPF_ConstParm = 0x0000000000000002,
	CPF_BlueprintVisible = 0x0000000000000004,
	CPF_ExportObject = 0x0000000000000008,
	CPF_BlueprintReadOnly = 0x0000000000000010,
	CPF_Net = 0x0000000000000020,
	CPF_EditFixedSize = 0x0000000000000040,
	/** A function parameter. */
	CPF_Parm = 0x0000000000000080,
	/** An out (or by-reference) function parameter. */
	CPF_OutParm = 0x0000000000000100,
	/** memset(0) is a valid initial value (computed at link time). */
	CPF_ZeroConstructor = 0x0000000000000200,
	/** A function's return value. */
	CPF_ReturnParm = 0x0000000000000400,
	CPF_DisableEditOnTemplate = 0x0000000000000800,
	CPF_Transient = 0x0000000000002000,
	/** Loaded from / saved to the config (P10). */
	CPF_Config = 0x0000000000004000,
	CPF_DisableEditOnInstance = 0x0000000000010000,
	CPF_EditConst = 0x0000000000020000,
	CPF_GlobalConfig = 0x0000000000040000,
	CPF_InstancedReference = 0x0000000000080000,
	CPF_DuplicateTransient = 0x0000000000200000,
	CPF_SaveGame = 0x0000000001000000,
	CPF_NoClear = 0x0000000002000000,
	/** A by-reference function parameter (const T&). */
	CPF_ReferenceParm = 0x0000000008000000,
	CPF_BlueprintAssignable = 0x0000000010000000,
	CPF_Deprecated = 0x0000000020000000,
	/** Copyable with memcpy and comparable with memcmp (computed at link time). */
	CPF_IsPlainOldData = 0x0000000040000000,
	CPF_RepSkip = 0x0000000080000000,
	CPF_RepNotify = 0x0000000100000000,
	CPF_Interp = 0x0000000200000000,
	CPF_NonTransactional = 0x0000000400000000,
	/** Declared inside #if WITH_EDITORONLY_DATA. */
	CPF_EditorOnly = 0x0000000800000000,
	/** Needs no destructor call (computed at link time). */
	CPF_NoDestructor = 0x0000001000000000,
	CPF_AutoWeak = 0x0000004000000000,
	CPF_ContainsInstancedReference = 0x0000008000000000,
	CPF_AssetRegistrySearchable = 0x0000010000000000,
	CPF_SimpleDisplay = 0x0000020000000000,
	CPF_AdvancedDisplay = 0x0000040000000000,
	CPF_Protected = 0x0000080000000000,
	CPF_BlueprintCallable = 0x0000100000000000,
	CPF_BlueprintAuthorityOnly = 0x0000200000000000,
	CPF_TextExportTransient = 0x0000400000000000,
	CPF_NonPIEDuplicateTransient = 0x0000800000000000,
	CPF_ExposeOnSpawn = 0x0001000000000000,
	CPF_PersistentInstance = 0x0002000000000000,
	/** A TSubclassOf / soft / weak wrapper rather than a raw pointer. */
	CPF_UObjectWrapper = 0x0004000000000000,
	/** GetValueTypeHash works (computed at link time); required for set elements and map keys. */
	CPF_HasGetValueTypeHash = 0x0008000000000000,
	CPF_NativeAccessSpecifierPublic = 0x0010000000000000,
	CPF_NativeAccessSpecifierProtected = 0x0020000000000000,
	CPF_NativeAccessSpecifierPrivate = 0x0040000000000000,
	CPF_SkipSerialization = 0x0080000000000000,
};
ENUM_CLASS_FLAGS(EPropertyFlags)

/** Parameter flags of a function's parameters (UE: CPF_ParmFlags). */
#define CPF_ParmFlags ((EPropertyFlags)(CPF_Parm | CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm | CPF_ConstParm))
/** Flags FProperty computes when it links (UE: CPF_ComputedFlags). */
#define CPF_ComputedFlags                                                                                              \
	((EPropertyFlags)(CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor | CPF_HasGetValueTypeHash))

/** Flags describing a package (UE: EPackageFlags; the subset Leon uses). */
enum EPackageFlags
{
	PKG_None = 0x00000000,
	PKG_NewlyCreated = 0x00000001,
	/** A /Script/<Module> package of compiled-in types. */
	PKG_CompiledIn = 0x00000010,
	PKG_EditorOnly = 0x00000040,
	PKG_Cooked = 0x00000200,
	PKG_ContainsNoAsset = 0x00000400,
	PKG_ContainsMap = 0x00020000,
	PKG_ContainsScript = 0x00200000,
	PKG_FilterEditorOnly = 0x80000000,
};
ENUM_CLASS_FLAGS(EPackageFlags)

/** Flags describing a struct (UE: EStructFlags, 4.27 values). */
enum EStructFlags
{
	STRUCT_NoFlags = 0x00000000,
	STRUCT_Native = 0x00000001,
	/** Identical uses the C++ operator== (TStructOpsTypeTraits::WithIdenticalViaEquality). */
	STRUCT_IdenticalNative = 0x00000002,
	STRUCT_HasInstancedReference = 0x00000004,
	/** Reflected from a NoExport declaration (the C++ type is defined elsewhere). */
	STRUCT_NoExport = 0x00000008,
	STRUCT_Atomic = 0x00000010,
	STRUCT_Immutable = 0x00000020,
	STRUCT_AddStructReferencedObjects = 0x00000040,
	STRUCT_RequiredAPI = 0x00000200,
	STRUCT_NetSerializeNative = 0x00000400,
	STRUCT_SerializeNative = 0x00000800,
	STRUCT_CopyNative = 0x00001000,
	STRUCT_IsPlainOldData = 0x00002000,
	STRUCT_NoDestructor = 0x00004000,
	STRUCT_ZeroConstructor = 0x00008000,
	STRUCT_ExportTextItemNative = 0x00010000,
	STRUCT_ImportTextItemNative = 0x00020000,
	STRUCT_PostSerializeNative = 0x00040000,
	STRUCT_SerializeFromMismatchedTag = 0x00080000,
	STRUCT_NetDeltaSerializeNative = 0x00100000,
	STRUCT_PostScriptConstruct = 0x00200000,
	STRUCT_NetSharedSerialization = 0x00400000,
	STRUCT_Trashed = 0x00800000,

	/** Flags a struct inherits from its super struct. */
	STRUCT_Inherit = STRUCT_HasInstancedReference | STRUCT_Atomic,
	/** Flags UScriptStruct computes from its C++ operations. */
	STRUCT_ComputedFlags = STRUCT_NetDeltaSerializeNative | STRUCT_NetSerializeNative | STRUCT_SerializeNative |
		STRUCT_PostSerializeNative | STRUCT_CopyNative | STRUCT_IsPlainOldData | STRUCT_NoDestructor |
		STRUCT_ZeroConstructor | STRUCT_IdenticalNative | STRUCT_AddStructReferencedObjects |
		STRUCT_ExportTextItemNative | STRUCT_ImportTextItemNative | STRUCT_SerializeFromMismatchedTag |
		STRUCT_PostScriptConstruct | STRUCT_NetSharedSerialization,
};
ENUM_CLASS_FLAGS(EStructFlags)

/** Flags describing an enum (UE: EEnumFlags). */
enum class EEnumFlags
{
	None = 0,
	/** UENUM(Flags): a bit-flag enum. */
	Flags = 0x00000001,
	NewerVersionExists = 0x00000002,
};
ENUM_CLASS_FLAGS(EEnumFlags)

/** Flags of an FArrayProperty (UE: EArrayPropertyFlags). */
enum class EArrayPropertyFlags
{
	None,
	UsesMemoryImageAllocator
};

/** Flags of an FMapProperty (UE: EMapPropertyFlags). */
enum class EMapPropertyFlags
{
	None,
	UsesMemoryImageAllocator
};

/** Search flag of FindFunctionByName (UE: EIncludeSuperFlag). */
namespace EIncludeSuperFlag
{
	enum Type
	{
		ExcludeSuper,
		IncludeSuper
	};
} // namespace EIncludeSuperFlag

// Reflection markup. LeonHeaderTool reads these; the C++ compiler sees the generated macros or nothing.

/** Pastes CURRENT_FILE_ID, the line and a suffix after expanding them (UE: BODY_MACRO_COMBINE). */
#define BODY_MACRO_COMBINE_INNER(A, B, C, D) A##B##C##D
#define BODY_MACRO_COMBINE(A, B, C, D) BODY_MACRO_COMBINE_INNER(A, B, C, D)

/** Class body: the generated declarations (constructors, StaticClass, exec thunks) (UE: GENERATED_BODY). */
#define GENERATED_BODY(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY)
/** Legacy class body: public access and a declared-only FObjectInitializer constructor (UE: GENERATED_BODY_LEGACY). */
#define GENERATED_BODY_LEGACY(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY_LEGACY)
#define GENERATED_UCLASS_BODY(...) GENERATED_BODY_LEGACY()
#define GENERATED_USTRUCT_BODY(...) GENERATED_BODY()

#define UCLASS(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID, _, __LINE__, _PROLOG)
#define USTRUCT(...)
#define UENUM(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UMETA(...)

/** Serialization helper of a generated class (UE: DECLARE_SERIALIZER; FArchive << UObject* arrives with P11). */
#define DECLARE_SERIALIZER(TClass)

/**
 * The reflection boilerplate of a class (UE: DECLARE_CLASS): Super / ThisClass, StaticClass(), the static flags and
 * the placement operators the object system constructs with. IMPLEMENT_CLASS defines GetPrivateStaticClass().
 */
#define DECLARE_CLASS(TClass, TSuperClass, TStaticFlags, TStaticCastFlags, TPackage, TRequiredAPI)                     \
private:                                                                                                               \
	TClass& operator=(TClass&&);                                                                                       \
	TClass& operator=(const TClass&);                                                                                  \
	TRequiredAPI static UClass* GetPrivateStaticClass();                                                               \
                                                                                                                       \
public:                                                                                                                \
	/** Bitwise union of EClassFlags of this class. */                                                                 \
	enum                                                                                                               \
	{                                                                                                                  \
		StaticClassFlags = (TStaticFlags)                                                                              \
	};                                                                                                                 \
	/** The reflected base class. */                                                                                   \
	typedef TSuperClass Super;                                                                                         \
	typedef TClass ThisClass;                                                                                          \
	/** The UClass describing this class. */                                                                           \
	inline static UClass* StaticClass()                                                                                \
	{                                                                                                                  \
		return GetPrivateStaticClass();                                                                                \
	}                                                                                                                  \
	/** The /Script/<Module> package of this class. */                                                                 \
	inline static const TCHAR* StaticPackage()                                                                         \
	{                                                                                                                  \
		return TPackage;                                                                                               \
	}                                                                                                                  \
	static constexpr EClassCastFlags StaticClassCastFlags()                                                            \
	{                                                                                                                  \
		return TStaticCastFlags;                                                                                       \
	}                                                                                                                  \
	/** For internal use only: the object system constructs objects in memory it allocated. */                         \
	inline void* operator new(const size_t InSize, EInternal* InMem)                                                   \
	{                                                                                                                  \
		(void)InSize;                                                                                                  \
		return (void*)InMem;                                                                                           \
	}                                                                                                                  \
	inline void operator delete(void* InMem, EInternal*)                                                               \
	{                                                                                                                  \
		(void)InMem;                                                                                                   \
	}                                                                                                                  \
	inline void operator delete(void* InMem)                                                                           \
	{                                                                                                                  \
		::operator delete(InMem);                                                                                      \
	}

/** The class constructor the object system calls, when the class only has a default constructor. */
#define DEFINE_DEFAULT_CONSTRUCTOR_CALL(TClass)                                                                        \
	static void __DefaultConstructor(const FObjectInitializer& X)                                                      \
	{                                                                                                                  \
		new ((EInternal*)X.GetObj()) TClass;                                                                           \
	}

/** The class constructor the object system calls, when the class takes an FObjectInitializer. */
#define DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(TClass)                                                     \
	static void __DefaultConstructor(const FObjectInitializer& X)                                                      \
	{                                                                                                                  \
		new ((EInternal*)X.GetObj()) TClass(X);                                                                        \
	}

/** Declares the vtable helper constructor (UE: hot reload builds throw-away objects to read vtables). */
#define DECLARE_VTABLE_PTR_HELPER_CTOR(API, TClass) API TClass(FVTableHelper& Helper)

/** Defines the vtable helper constructor; it only forwards to the super class. */
#define DEFINE_VTABLE_PTR_HELPER_CTOR(TClass)                                                                          \
	TClass::TClass(FVTableHelper& Helper)                                                                              \
		: Super(Helper)                                                                                                \
	{                                                                                                                  \
	}

/**
 * The vtable helper caller stored in UClass::ClassVTableHelperCtorCaller. Leon has no hot reload, so nothing calls it
 * and it builds nothing.
 */
#define DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(TClass)                                                                   \
	static UObject* __VTableCtorCaller(FVTableHelper& Helper)                                                          \
	{                                                                                                                  \
		(void)Helper;                                                                                                  \
		return nullptr;                                                                                                \
	}

/** Intrinsic class declaration: DECLARE_CLASS plus the pieces LeonHeaderTool would generate (UE). */
#define DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(                                                                       \
	TClass, TSuperClass, TStaticFlags, TPackage, TStaticCastFlags, TRequiredAPI)                                       \
	DECLARE_CLASS(TClass, TSuperClass, TStaticFlags | CLASS_Intrinsic, TStaticCastFlags, TPackage, TRequiredAPI)       \
	static void StaticRegisterNatives##TClass()                                                                        \
	{                                                                                                                  \
	}                                                                                                                  \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(TClass)                                                         \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(TClass)                                                                       \
	TClass(FVTableHelper& Helper)                                                                                      \
		: Super(Helper)                                                                                                \
	{                                                                                                                  \
	}

#define DECLARE_CASTED_CLASS_INTRINSIC(TClass, TSuperClass, TStaticFlags, TPackage, TStaticCastFlags)                  \
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(TClass, TSuperClass, TStaticFlags, TPackage, TStaticCastFlags, NO_API)

#define DECLARE_CLASS_INTRINSIC(TClass, TSuperClass, TStaticFlags, TPackage)                                           \
	DECLARE_CASTED_CLASS_INTRINSIC(TClass, TSuperClass, TStaticFlags, TPackage, CASTCLASS_None)

/** Calls a class's __DefaultConstructor (UE: InternalConstructor, stored in UClass::ClassConstructor). */
template <class T>
void InternalConstructor(const FObjectInitializer& X)
{
	T::__DefaultConstructor(X);
}

/** Calls a class's __VTableCtorCaller (UE: InternalVTableHelperCtorCaller). */
template <class T>
UObject* InternalVTableHelperCtorCaller(FVTableHelper& Helper)
{
	return T::__VTableCtorCaller(Helper);
}

/**
 * Defines GetPrivateStaticClass(): the class's UClass, built on first use (UE: IMPLEMENT_CLASS). The CRC is always 0
 * (no hot reload).
 */
#define IMPLEMENT_CLASS(TClass, TClassCrc)                                                                             \
	UClass* TClass::GetPrivateStaticClass()                                                                            \
	{                                                                                                                  \
		static UClass* PrivateStaticClass = nullptr;                                                                   \
		if (!PrivateStaticClass)                                                                                       \
		{                                                                                                              \
			(void)(TClassCrc);                                                                                         \
			GetPrivateStaticClassBody(StaticPackage(), TEXT(#TClass) + 1, PrivateStaticClass,                          \
				StaticRegisterNatives##TClass, sizeof(TClass), alignof(TClass), (EClassFlags)TClass::StaticClassFlags, \
				TClass::StaticClassCastFlags(), TClass::StaticConfigName(),                                            \
				(UClass::ClassConstructorType)InternalConstructor<TClass>,                                             \
				(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,                       \
				&TClass::Super::StaticClass);                                                                          \
		}                                                                                                              \
		return PrivateStaticClass;                                                                                     \
	}

/** The UClass / UScriptStruct / UEnum of a C++ type, specialized by the generated code (UE). */
template <typename ClassType>
UClass* StaticClass();

template <typename StructType>
UScriptStruct* StaticStruct();

template <typename EnumType>
UEnum* StaticEnum();

/** The API macro of every CoreUObject declaration (LeonBuildTool defines it empty; kept for the UE shape). */
#ifndef COREUOBJECT_API
	#define COREUOBJECT_API
#endif
