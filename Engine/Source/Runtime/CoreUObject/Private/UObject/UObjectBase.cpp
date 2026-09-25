#include "UObject/UObjectBase.h"

#include "HAL/PlatformProperties.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"

#include <new>

DEFINE_LOG_CATEGORY_STATIC(LogUObjectBase, Log, All);

namespace
{
	bool GObjectSystemInitialized = false;

	/** A class object built before the object system started, waiting for its package and name. */
	struct FPendingRegistrant
	{
		UObjectBase* Object;
		const TCHAR* PackageName;
		const TCHAR* Name;
	};

	TArray<FPendingRegistrant>& GetPendingRegistrants()
	{
		static TArray<FPendingRegistrant> PendingRegistrants;
		return PendingRegistrants;
	}

	/** RegisterCompiledInInfo tables waiting for ProcessNewlyLoadedUObjects (UE: the deferred registration lists). */
	struct FPendingClass
	{
		const TCHAR* PackageName;
		const FClassRegisterCompiledInInfo* Info;
	};
	struct FPendingStruct
	{
		const TCHAR* PackageName;
		const FStructRegisterCompiledInInfo* Info;
	};
	struct FPendingEnum
	{
		const TCHAR* PackageName;
		const FEnumRegisterCompiledInInfo* Info;
	};

	TArray<FPendingClass>& GetPendingClasses()
	{
		static TArray<FPendingClass> Pending;
		return Pending;
	}

	TArray<FPendingStruct>& GetPendingStructs()
	{
		static TArray<FPendingStruct> Pending;
		return Pending;
	}

	TArray<FPendingEnum>& GetPendingEnums()
	{
		static TArray<FPendingEnum> Pending;
		return Pending;
	}

	/** Heap allocated while constructing compiled-in types (FUObjectReflectionStats::ConstructionHeapBytes). */
	SIZE_T GReflectionConstructionBytes = 0;
} // namespace

UObjectBase::UObjectBase()
{
	FUObjectThreadContext& Context = FUObjectThreadContext::Get();
	checkf(Context.PendingConstructions.Num() > 0 && Context.PendingConstructions.Last().Memory == this,
		"UObjects are created with NewObject / StaticConstructObject_Internal, not with new or on the stack");
	const FUObjectThreadContext::FPendingConstruction Pending = Context.PendingConstructions.Pop(false);
	ObjectFlags = Pending.Flags;
	InternalIndex = INDEX_NONE;
	ClassPrivate = Pending.Class;
	OuterPrivate = Pending.Outer;
	AddObject(Pending.Name, EInternalObjectFlags::None);
}

UObjectBase::UObjectBase(EObjectFlags InFlags)
	: ObjectFlags(InFlags)
	, InternalIndex(INDEX_NONE)
	, ClassPrivate(nullptr)
	, NamePrivate()
	, OuterPrivate(nullptr)
{
}

UObjectBase::~UObjectBase()
{
	// The garbage collector destroys objects after BeginDestroy renamed them to NAME_None (out of the name hash); the
	// slot is freed here, which makes weak pointers to the object stale (UE).
	if (InternalIndex != INDEX_NONE && GUObjectArray.IsInitialized())
	{
		UnhashObject(this);
		GUObjectArray.FreeUObjectIndex(this);
	}
}

void UObjectBase::AddObject(FName Name, EInternalObjectFlags InSetInternalFlags)
{
	NamePrivate = Name;
	EInternalObjectFlags InternalFlagsToSet = InSetInternalFlags;
	if (ObjectFlags & RF_MarkAsRootSet)
	{
		InternalFlagsToSet |= EInternalObjectFlags::RootSet;
		ObjectFlags &= ~RF_MarkAsRootSet;
	}
	if (ObjectFlags & RF_MarkAsNative)
	{
		InternalFlagsToSet |= EInternalObjectFlags::Native;
		ObjectFlags &= ~RF_MarkAsNative;
	}
	GUObjectArray.AllocateUObjectIndex(this);
	if (InternalFlagsToSet != EInternalObjectFlags::None)
	{
		GUObjectArray.IndexToObject(InternalIndex)->SetFlags(InternalFlagsToSet);
	}
	HashObject(this);
}

void UObjectBase::LowLevelRename(FName NewName, UObject* NewOuter)
{
	UnhashObject(this);
	NamePrivate = NewName;
	if (NewOuter)
	{
		OuterPrivate = NewOuter;
	}
	HashObject(this);
}

void UObjectBase::Register(const TCHAR* PackageName, const TCHAR* Name)
{
	if (GObjectSystemInitialized)
	{
		DeferredRegister(UClass::StaticClass(), PackageName, Name);
	}
	else
	{
		GetPendingRegistrants().Add({this, PackageName, Name});
	}
}

void UObjectBase::DeferredRegister(UClass* UClassStaticClass, const TCHAR* PackageName, const TCHAR* Name)
{
	check(GObjectSystemInitialized);
	checkf(InternalIndex == INDEX_NONE, "%s is registered twice", Name);
	// The package first: creating it may build UPackage's class, which may register this class recursively.
	UPackage* Package = CreatePackage(PackageName);
	if (InternalIndex != INDEX_NONE)
	{
		return;
	}
	OuterPrivate = Package;
	ClassPrivate = UClassStaticClass;
	AddObject(FName(Name), EInternalObjectFlags::Native);
}

bool UObjectBase::IsValidLowLevel() const
{
	return ClassPrivate && GUObjectArray.IsValid(this);
}

bool UObjectBase::IsValidLowLevelFast(bool bRecursive) const
{
	if (!IsValidLowLevel())
	{
		return false;
	}
	if (bRecursive)
	{
		const UObjectBase* ClassBase = (const UObjectBase*)ClassPrivate;
		if (!ClassBase->IsValidLowLevelFast(false))
		{
			return false;
		}
		if (OuterPrivate && !((const UObjectBase*)OuterPrivate)->IsValidLowLevelFast(false))
		{
			return false;
		}
	}
	return true;
}

void UObjectProcessRegistrants()
{
	TArray<FPendingRegistrant>& Pending = GetPendingRegistrants();
	while (Pending.Num() > 0)
	{
		UObjectForceRegistration(Pending[0].Object);
	}
}

void UObjectForceRegistration(UObjectBase* Object)
{
	TArray<FPendingRegistrant>& Pending = GetPendingRegistrants();
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		if (Pending[Index].Object == Object)
		{
			const FPendingRegistrant Registrant = Pending[Index];
			Pending.RemoveAt(Index);
			Object->DeferredRegister(UClass::StaticClass(), Registrant.PackageName, Registrant.Name);
			return;
		}
	}
}

bool UObjectInitialized()
{
	return GObjectSystemInitialized;
}

void UObjectBaseInit()
{
	if (GObjectSystemInitialized)
	{
		return;
	}
	GUObjectArray.AllocateObjectPool(FPlatformProperties::MaxObjectsInGame);
	GObjectSystemInitialized = true;
	// The intrinsic classes and the first packages count as reflection data too (the object array does not).
	const SIZE_T HeapBefore = FMemory::GetUsage().CurrentBytes;

	// Classes whose StaticClass() ran before the object system started (UE: UObjectProcessRegistrants).
	UObjectProcessRegistrants();

	// CoreUObject's own classes are recorded like generated ones; ProcessNewlyLoadedUObjects constructs them.
	UObjectRegisterIntrinsicClasses();

	UPackage* TransientPackage = GetTransientPackage();
	const SIZE_T HeapAfter = FMemory::GetUsage().CurrentBytes;
	if (HeapAfter > HeapBefore)
	{
		GReflectionConstructionBytes += HeapAfter - HeapBefore;
	}
	UE_LOG(LogUObjectBase, Log, TEXT("Object system started: %d object slots, transient package %s"),
		GUObjectArray.GetObjectArrayCapacity(), *TransientPackage->GetName());
}

void RegisterCompiledInInfo(const TCHAR* PackageName, const FClassRegisterCompiledInInfo* ClassInfo,
	SIZE_T NumClassInfo, const FStructRegisterCompiledInInfo* StructInfo, SIZE_T NumStructInfo,
	const FEnumRegisterCompiledInInfo* EnumInfo, SIZE_T NumEnumInfo)
{
	for (SIZE_T Index = 0; Index < NumClassInfo; ++Index)
	{
		GetPendingClasses().Add({PackageName, &ClassInfo[Index]});
	}
	for (SIZE_T Index = 0; Index < NumStructInfo; ++Index)
	{
		GetPendingStructs().Add({PackageName, &StructInfo[Index]});
	}
	for (SIZE_T Index = 0; Index < NumEnumInfo; ++Index)
	{
		GetPendingEnums().Add({PackageName, &EnumInfo[Index]});
	}
}

void ProcessNewlyLoadedUObjects(const TCHAR* ModuleName, bool bCanProcessNewlyLoadedObjects)
{
	static bool bProcessing = false;
	if (!GObjectSystemInitialized || !bCanProcessNewlyLoadedObjects || bProcessing)
	{
		return;
	}
	bProcessing = true;
	const SIZE_T HeapBefore = FMemory::GetUsage().CurrentBytes;

	int32 NumClasses = 0;
	int32 NumStructs = 0;
	int32 NumEnums = 0;
	while (GetPendingClasses().Num() > 0 || GetPendingStructs().Num() > 0 || GetPendingEnums().Num() > 0)
	{
		const TArray<FPendingClass> Classes = MoveTemp(GetPendingClasses());
		const TArray<FPendingStruct> Structs = MoveTemp(GetPendingStructs());
		const TArray<FPendingEnum> Enums = MoveTemp(GetPendingEnums());
		GetPendingClasses().Reset();
		GetPendingStructs().Reset();
		GetPendingEnums().Reset();

		// The /Script/<Module> packages.
		for (const FPendingClass& Pending : Classes)
		{
			CreatePackage(Pending.PackageName)->SetPackageFlags(PKG_CompiledIn);
		}
		for (const FPendingStruct& Pending : Structs)
		{
			CreatePackage(Pending.PackageName)->SetPackageFlags(PKG_CompiledIn);
		}
		for (const FPendingEnum& Pending : Enums)
		{
			CreatePackage(Pending.PackageName)->SetPackageFlags(PKG_CompiledIn);
		}

		// The class objects (UE: UClassRegisterAllCompiledInClasses), then their registration.
		for (const FPendingClass& Pending : Classes)
		{
			Pending.Info->InnerRegister();
		}
		UObjectProcessRegistrants();

		// Reflection data: enums, structs, then classes. A class recorded before its super constructs the super first
		// (its DependentSingletons); each Z_Construct_* is idempotent.
		for (const FPendingEnum& Pending : Enums)
		{
			Pending.Info->OuterRegister();
		}
		for (const FPendingStruct& Pending : Structs)
		{
			Pending.Info->OuterRegister();
		}
		for (const FPendingClass& Pending : Classes)
		{
			Pending.Info->OuterRegister();
		}

		// Class default objects: GetDefaultObject builds the super class's first (UE:
		// UObjectLoadAllCompiledInDefaultProperties).
		for (const FPendingClass& Pending : Classes)
		{
			Pending.Info->OuterRegister()->GetDefaultObject();
		}

		NumClasses += Classes.Num();
		NumStructs += Structs.Num();
		NumEnums += Enums.Num();
	}

	const SIZE_T HeapAfter = FMemory::GetUsage().CurrentBytes;
	if (HeapAfter > HeapBefore)
	{
		GReflectionConstructionBytes += HeapAfter - HeapBefore;
	}
	if (NumClasses + NumStructs + NumEnums > 0)
	{
		UE_LOG(LogUObjectBase, Verbose, TEXT("%s: constructed %d classes, %d structs, %d enums"),
			ModuleName ? ModuleName : TEXT("(startup)"), NumClasses, NumStructs, NumEnums);
	}
	bProcessing = false;
}

void GetPrivateStaticClassBody(const TCHAR* PackageName, const TCHAR* Name, UClass*& ReturnClass,
	void (*RegisterNativeFunc)(), uint32 InSize, uint32 InAlignment, EClassFlags InClassFlags,
	EClassCastFlags InClassCastFlags, const TCHAR* InConfigName, void (*InClassConstructor)(const FObjectInitializer&),
	UObject* (*InClassVTableHelperCtorCaller)(FVTableHelper&),
	void (*InClassAddReferencedObjects)(UObject*, FReferenceCollector&), UClass* (*InSuperClassFn)())
{
	// ReturnClass is set before anything else runs: building the super class or registering may need this class.
	void* Memory = FMemory::Malloc(sizeof(UClass), alignof(UClass));
	ReturnClass =
		::new (Memory) UClass(EC_StaticConstructor, FName(Name), InSize, InAlignment, InClassFlags, InClassCastFlags,
			InConfigName, EObjectFlags(RF_Public | RF_Standalone | RF_Transient | RF_MarkAsNative | RF_MarkAsRootSet),
			InClassConstructor, InClassVTableHelperCtorCaller, InClassAddReferencedObjects);

	UClass* SuperClass = InSuperClassFn();
	if (SuperClass != ReturnClass)
	{
		ReturnClass->SetSuperStruct(SuperClass);
		ReturnClass->ClassCastFlags |= SuperClass->ClassCastFlags;
		ReturnClass->ClassFlags |= (SuperClass->ClassFlags & CLASS_Inherit);
	}
	((UObjectBase*)ReturnClass)->Register(PackageName, Name);

	// The class's exec thunks (UFunction::Bind finds them by name).
	RegisterNativeFunc();
}

FUObjectReflectionStats GetUObjectReflectionStats()
{
	FUObjectReflectionStats Stats;
	Stats.ConstructionHeapBytes = GReflectionConstructionBytes;
	const auto CountProperties = [](const UStruct* Struct)
	{
		int32 Count = 0;
		for (FField* Field = Struct->ChildProperties; Field; Field = Field->Next)
		{
			++Count;
			if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Field))
			{
				Count += ArrayProperty->Inner ? 1 : 0;
			}
			else if (FSetProperty* SetProperty = CastField<FSetProperty>(Field))
			{
				Count += SetProperty->ElementProp ? 1 : 0;
			}
			else if (FMapProperty* MapProperty = CastField<FMapProperty>(Field))
			{
				Count += (MapProperty->KeyProp ? 1 : 0) + (MapProperty->ValueProp ? 1 : 0);
			}
			else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Field))
			{
				Count += EnumProperty->GetUnderlyingProperty() ? 1 : 0;
			}
		}
		return Count;
	};
	for (FObjectIterator It(UObject::StaticClass(), false, RF_ClassDefaultObject); It; ++It)
	{
		UObject* Object = *It;
		if (UFunction* Function = Cast<UFunction>(Object))
		{
			++Stats.NumFunctions;
			Stats.NumProperties += CountProperties(Function);
		}
		else if (UClass* Class = Cast<UClass>(Object))
		{
			++Stats.NumClasses;
			Stats.NumProperties += CountProperties(Class);
		}
		else if (UScriptStruct* Struct = Cast<UScriptStruct>(Object))
		{
			++Stats.NumStructs;
			Stats.NumProperties += CountProperties(Struct);
		}
		else if (Cast<UEnum>(Object))
		{
			++Stats.NumEnums;
		}
		else if (Cast<UPackage>(Object))
		{
			++Stats.NumPackages;
		}
	}
	return Stats;
}
