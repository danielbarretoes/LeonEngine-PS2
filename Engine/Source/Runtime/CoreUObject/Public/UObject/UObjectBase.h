#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class FReferenceCollector;
class UClass;
class UEnum;
class UObject;
class UPackage;
class UScriptStruct;

/**
 * The lowest level of every UObject: flags, index in GUObjectArray, class, name and outer (UE: UObjectBase). Objects
 * are created by the object system (NewObject / StaticConstructObject_Internal), never with plain new.
 */
class COREUOBJECT_API UObjectBase
{
	friend class UObjectBaseUtility;
	friend class FUObjectArray;
	friend COREUOBJECT_API void UObjectForceRegistration(UObjectBase* Object);
	friend COREUOBJECT_API void GetPrivateStaticClassBody(const TCHAR*, const TCHAR*, UClass*&, void (*)(), uint32,
		uint32, EClassFlags, EClassCastFlags, const TCHAR*, void (*)(const FObjectInitializer&),
		UObject* (*)(FVTableHelper&), void (*)(UObject*, FReferenceCollector&), UClass* (*)());

protected:
	/**
	 * Constructor the class constructors run. The object system has already allocated the memory and recorded the
	 * class, name, outer and flags of the object being constructed; this constructor takes them and registers the
	 * object in GUObjectArray and the name hash. (UE sets the fields before the constructor runs and leaves them
	 * untouched here; Leon passes them through the construction context instead, so no member is read before it is
	 * initialized.)
	 */
	UObjectBase();

	/** Constructor of the intrinsic UClasses built before the object system runs; registered later. */
	explicit UObjectBase(EObjectFlags InFlags);

public:
	virtual ~UObjectBase();

	UObjectBase(const UObjectBase&) = delete;
	UObjectBase& operator=(const UObjectBase&) = delete;

	/** True when the object is in GUObjectArray with a class (a basic sanity check). */
	bool IsValidLowLevel() const;

	/** Like IsValidLowLevel, plus the object's class and outer look valid. */
	bool IsValidLowLevelFast(bool bRecursive = true) const;

	/** The object's index in GUObjectArray, unique among live objects. */
	FORCEINLINE uint32 GetUniqueID() const
	{
		return (uint32)InternalIndex;
	}

	FORCEINLINE UClass* GetClass() const
	{
		return ClassPrivate;
	}

	FORCEINLINE UObject* GetOuter() const
	{
		return OuterPrivate;
	}

	FORCEINLINE FName GetFName() const
	{
		return NamePrivate;
	}

	FORCEINLINE EObjectFlags GetFlags() const
	{
		return ObjectFlags;
	}

protected:
	FORCEINLINE void SetFlagsTo(EObjectFlags NewFlags)
	{
		ObjectFlags = NewFlags;
	}

	/**
	 * Registers an intrinsic class built by GetPrivateStaticClassBody: now when the object system is up, else when it
	 * starts (UE: UObjectBase::Register).
	 */
	void Register(const TCHAR* PackageName, const TCHAR* Name);

	/** Gives the class object its package, UClass and name, and adds it to GUObjectArray (UE). */
	virtual void DeferredRegister(UClass* UClassStaticClass, const TCHAR* PackageName, const TCHAR* Name);

	/**
	 * Changes the name (and optionally the outer) and rehashes the object; no uniqueness check. UObject::BeginDestroy
	 * renames the object to NAME_None, which takes it out of the name hash (UE).
	 */
	void LowLevelRename(FName NewName, UObject* NewOuter = nullptr);

private:
	/**
	 * Takes a slot in GUObjectArray and enters the name hash. RF_MarkAsRootSet and RF_MarkAsNative become the
	 * RootSet and Native internal flags (UE).
	 */
	void AddObject(FName Name, EInternalObjectFlags InSetInternalFlags);

	EObjectFlags ObjectFlags;
	/** Index in GUObjectArray; INDEX_NONE until registered. */
	int32 InternalIndex;
	UClass* ClassPrivate;
	FName NamePrivate;
	UObject* OuterPrivate;
};

/** One compiled-in class of a RegisterCompiledInInfo call (UE 5: FClassRegisterCompiledInInfo). */
struct FClassRegisterCompiledInInfo
{
	/** Z_Construct_UClass_<Class>: builds the reflection data (properties, functions) and returns the class. */
	UClass* (*OuterRegister)();
	/** <Class>::StaticClass: the class object without its reflection data. */
	UClass* (*InnerRegister)();
	const TCHAR* Name;
	SIZE_T Size;
};

/** One compiled-in struct of a RegisterCompiledInInfo call (UE 5: FStructRegisterCompiledInInfo). */
struct FStructRegisterCompiledInInfo
{
	UScriptStruct* (*OuterRegister)();
	const TCHAR* Name;
	SIZE_T Size;
};

/** One compiled-in enum of a RegisterCompiledInInfo call (UE 5: FEnumRegisterCompiledInInfo). */
struct FEnumRegisterCompiledInInfo
{
	UEnum* (*OuterRegister)();
	const TCHAR* Name;
};

/**
 * Records a module's compiled-in types (called by the generated RegisterReflection_<Module>, which FModuleManager calls
 * right after creating the module). Nothing is constructed here: ProcessNewlyLoadedUObjects does it, so a class may be
 * recorded before its super class. A null table comes with a zero count.
 */
COREUOBJECT_API void RegisterCompiledInInfo(const TCHAR* PackageName, const FClassRegisterCompiledInInfo* ClassInfo,
	SIZE_T NumClassInfo, const FStructRegisterCompiledInInfo* StructInfo, SIZE_T NumStructInfo,
	const FEnumRegisterCompiledInInfo* EnumInfo, SIZE_T NumEnumInfo);

/**
 * Constructs everything recorded since the last call: the /Script/<Module> packages, then enums, structs and classes
 * (supers first, through each class's DependentSingletons), then the class default objects, parents first (UE:
 * ProcessNewlyLoadedUObjects). Bound to FModuleManager::OnProcessLoadedObjectsCallback; does nothing before the object
 * system starts.
 */
COREUOBJECT_API void ProcessNewlyLoadedUObjects(
	const TCHAR* ModuleName = nullptr, bool bCanProcessNewlyLoadedObjects = true);

/** Starts the object system: GUObjectArray, the intrinsic classes and the transient package (UE: UObjectBaseInit). */
COREUOBJECT_API void UObjectBaseInit();

/** True once UObjectBaseInit has run (UE: UObjectInitialized). */
COREUOBJECT_API bool UObjectInitialized();

/** Registers a class object whose registration was deferred until the object system started (UE). */
COREUOBJECT_API void UObjectForceRegistration(UObjectBase* Object);

/**
 * Builds a compiled-in class's UClass (called once from IMPLEMENT_CLASS's GetPrivateStaticClass). ReturnClass is set
 * before the super class is resolved, so the class can be referenced while its bootstrap recurses (UE).
 */
COREUOBJECT_API void GetPrivateStaticClassBody(const TCHAR* PackageName, const TCHAR* Name, UClass*& ReturnClass,
	void (*RegisterNativeFunc)(), uint32 InSize, uint32 InAlignment, EClassFlags InClassFlags,
	EClassCastFlags InClassCastFlags, const TCHAR* InConfigName, void (*InClassConstructor)(const FObjectInitializer&),
	UObject* (*InClassVTableHelperCtorCaller)(FVTableHelper&),
	void (*InClassAddReferencedObjects)(UObject*, FReferenceCollector&), UClass* (*InSuperClassFn)());

/** Memory figures of the reflection system, for platform budgets (Leon). */
struct FUObjectReflectionStats
{
	/** Heap bytes (GMalloc) allocated while constructing the compiled-in types and their default objects. */
	SIZE_T ConstructionHeapBytes = 0;
	int32 NumClasses = 0;
	int32 NumStructs = 0;
	int32 NumEnums = 0;
	int32 NumFunctions = 0;
	/** Properties of every struct, class and function, container inners included. */
	int32 NumProperties = 0;
	int32 NumPackages = 0;
};

/** Counts the reflected types and returns the heap used to construct them (Leon). */
COREUOBJECT_API FUObjectReflectionStats GetUObjectReflectionStats();
