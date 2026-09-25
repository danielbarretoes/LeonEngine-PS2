#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/Stack.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogObj, Log, All);

UObject::UObject()
{
}

UObject::UObject(const FObjectInitializer& ObjectInitializer)
{
	checkf(!ObjectInitializer.GetObj() || ObjectInitializer.GetObj() == this,
		"UObject %s constructed with the initializer of another object", *GetName());
}

UObject::UObject(EStaticConstructor, EObjectFlags InFlags)
	: UObjectBaseUtility(InFlags | RF_MarkAsNative | RF_MarkAsRootSet)
{
}

UObject::UObject(FVTableHelper& Helper)
	: UObjectBaseUtility(RF_NoFlags)
{
	(void)Helper;
}

UObject* UObject::CreateDefaultSubobject(
	FName SubobjectFName, UClass* ReturnType, UClass* ClassToCreateByDefault, bool bIsRequired, bool bIsTransient)
{
	FObjectInitializer* CurrentInitializer = FUObjectThreadContext::Get().TopInitializer();
	if (!CurrentInitializer)
	{
		UE_LOG(LogObj, Fatal,
			TEXT("CreateDefaultSubobject(%s): no object is being constructed; call it from %s's "
				 "constructor"),
			*SubobjectFName.ToString(), *GetName());
	}
	if (CurrentInitializer->GetObj() != this)
	{
		UE_LOG(LogObj, Fatal, TEXT("CreateDefaultSubobject(%s) called on %s while %s is being constructed"),
			*SubobjectFName.ToString(), *GetName(), *CurrentInitializer->GetObj()->GetName());
	}
	return CurrentInitializer->CreateDefaultSubobject(
		this, SubobjectFName, ReturnType, ClassToCreateByDefault, bIsRequired, bIsTransient);
}

void UObject::PostInitProperties()
{
}

void UObject::PostLoad()
{
}

void UObject::BeginDestroy()
{
	// Out of the name hash: the name can be reused and lookups no longer find the object (UE).
	LowLevelRename(NAME_None);
	// Tells ConditionalBeginDestroy the call reached UObject (every override calls Super::BeginDestroy).
	SetFlags(RF_BeginDestroyed);
}

bool UObject::IsReadyForFinishDestroy()
{
	return true;
}

void UObject::FinishDestroy()
{
	// Tells ConditionalFinishDestroy the call reached UObject. Native members are destroyed by the C++ destructor.
	SetFlags(RF_FinishDestroyed);
}

bool UObject::ConditionalBeginDestroy()
{
	if (HasAnyFlags(RF_BeginDestroyed))
	{
		return false;
	}
	BeginDestroy();
	if (!HasAnyFlags(RF_BeginDestroyed))
	{
		UE_LOG(LogObj, Fatal, TEXT("%s (%s) failed to route BeginDestroy (an override must call Super::BeginDestroy)"),
			*GetClass()->GetName(), *GetName());
	}
	return true;
}

bool UObject::ConditionalFinishDestroy()
{
	if (HasAnyFlags(RF_FinishDestroyed))
	{
		return false;
	}
	checkf(HasAnyFlags(RF_BeginDestroyed), "FinishDestroy before BeginDestroy on %s", *GetFullName());
	FinishDestroy();
	if (!HasAnyFlags(RF_FinishDestroyed))
	{
		UE_LOG(LogObj, Fatal,
			TEXT("%s (%s) failed to route FinishDestroy (an override must call Super::FinishDestroy)"),
			*GetClass()->GetName(), *GetName());
	}
	return true;
}

void UObject::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	(void)InThis;
	(void)Collector;
}

void UObject::Serialize(FArchive& Ar)
{
	(void)Ar;
}

UObject* UObject::GetArchetype() const
{
	UClass* Class = GetClass();
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UClass* SuperClass = Class->GetSuperClass();
		return SuperClass ? SuperClass->GetDefaultObject(false) : nullptr;
	}
	return Class->GetDefaultObject(false);
}

bool UObject::IsDefaultSubobject() const
{
	return HasAnyFlags(RF_DefaultSubObject);
}

void UObject::GetDefaultSubobjects(TArray<UObject*>& OutDefaultSubobjects) const
{
	OutDefaultSubobjects.Reset();
	TArray<UObject*> Inner;
	GetObjectsWithOuter(this, Inner, /*bIncludeNestedObjects =*/false);
	for (UObject* Object : Inner)
	{
		if (Object->IsDefaultSubobject())
		{
			OutDefaultSubobjects.Add(Object);
		}
	}
}

UFunction* UObject::FindFunction(FName InName) const
{
	return GetClass()->FindFunctionByName(InName);
}

UFunction* UObject::FindFunctionChecked(FName InName) const
{
	UFunction* Result = FindFunction(InName);
	if (!Result)
	{
		UE_LOG(LogObj, Fatal, TEXT("Failed to find function %s in %s"), *InName.ToString(), *GetFullName());
	}
	return Result;
}

void UObject::ProcessEvent(UFunction* Function, void* Parms)
{
	checkf(Function, "ProcessEvent on %s without a function", *GetName());
	checkf(Function->HasAnyFunctionFlags(FUNC_Native) && Function->GetNativeFunc(),
		"ProcessEvent: %s has no native implementation (Leon has no script VM)", *Function->GetName());
	checkf(Function->HasAnyFunctionFlags(FUNC_Static) || IsA(Function->GetOwnerClass()),
		"ProcessEvent: %s is not a function of %s", *Function->GetName(), *GetFullName());
	checkf(Parms || Function->ParmsSize == 0, "ProcessEvent: %s needs a parameter block", *Function->GetName());

	FFrame NewStack(this, Function, Parms, nullptr, Function->ChildProperties);
	uint8* ReturnValueAddress =
		Function->ReturnValueOffset != MAX_uint16 ? (uint8*)Parms + Function->ReturnValueOffset : nullptr;
	Function->Invoke(this, NewStack, ReturnValueAddress);
}

// Config (UE: Obj.cpp)

namespace
{
	/**
	 * The section Object's config members of BaseClass live in: the class path ("/Script/Engine.GameMapsSettings"),
	 * or "<Name> <Class>" for a PerObjectConfig object (UE: the ClassSection of LoadConfig / SaveConfig).
	 */
	FString GetConfigSection(UObject* Object, const UClass* BaseClass, bool bPerObject)
	{
		if (!bPerObject)
		{
			return BaseClass->GetPathName();
		}
		UPackage* Outermost = Object->GetOutermost();
		const FString PathName =
			Outermost == GetTransientPackage() ? Object->GetName() : Object->GetPathName((UObject*)Outermost);
		FString Section = PathName + TEXT(" ") + Object->GetClass()->GetName();
		Object->OverridePerObjectConfigSection(Section);
		return Section;
	}

	/** "Key[Index]" (UE: the key of a C array member or of one TArray element). */
	FString GetIndexedKey(const FString& Key, int32 Index)
	{
		return FString::Printf(TEXT("%s[%d]"), *Key, Index);
	}
} // namespace

FString GetConfigFilename(UObject* SourceObject)
{
	checkf(SourceObject, "GetConfigFilename without an object");
	return SourceObject->GetClass()->GetConfigName();
}

bool UsesPerObjectConfig(UObject* SourceObject)
{
	checkf(SourceObject, "UsesPerObjectConfig without an object");
	return SourceObject->GetClass()->HasAnyClassFlags(CLASS_PerObjectConfig);
}

void UObject::LoadConfig(
	UClass* ConfigClass, const TCHAR* InFilename, uint32 PropagationFlags, FProperty* PropertyToLoad)
{
	if (!ConfigClass)
	{
		ConfigClass = GetClass();
	}
	if (!GetClass()->HasAnyClassFlags(CLASS_Config) || !ConfigClass->HasAnyClassFlags(CLASS_Config))
	{
		return;
	}
	if (!GConfig)
	{
		UE_LOG(LogObj, Verbose, TEXT("LoadConfig(%s): no config system"), *GetPathName());
		return;
	}

	// The parent classes' sections first; the class's own section overrides them (UE: LCPF_ReadParentSections).
	UClass* ParentClass = ConfigClass->GetSuperClass();
	if ((PropagationFlags & UE4::LCPF_ReadParentSections) && ParentClass && ParentClass->HasAnyClassFlags(CLASS_Config))
	{
		LoadConfig(ParentClass, InFilename, UE4::LCPF_ReadParentSections, PropertyToLoad);
	}

	const FString Filename = InFilename ? FString(InFilename) : GetConfigFilename(this);
	const bool bPerObject = UsesPerObjectConfig(this);
	for (FProperty* Property = ConfigClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Config) || (PropertyToLoad && Property != PropertyToLoad))
		{
			continue;
		}
		// A GlobalConfig member always lives in the section (and, unless a file is given, the file) of the class that
		// declares it; the others in ConfigClass's (UE).
		const bool bGlobalConfig = Property->HasAnyPropertyFlags(CPF_GlobalConfig);
		UClass* OwnerClass = Property->GetOwnerClass();
		UClass* BaseClass = bGlobalConfig ? OwnerClass : ConfigClass;
		const FString Section = GetConfigSection(this, BaseClass, bPerObject);
		const FString PropFileName = (bGlobalConfig && !InFilename) ? OwnerClass->GetConfigName() : Filename;
		const FString Key = Property->GetName();
		constexpr int32 PortFlags = PPF_None;

		FArrayProperty* Array = CastField<FArrayProperty>(Property);
		if (!Array)
		{
			for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
			{
				const FString IndexedKey = Property->ArrayDim != 1 ? GetIndexedKey(Key, Index) : Key;
				FString Value;
				if (GConfig->GetString(*Section, *IndexedKey, Value, PropFileName) &&
					!Property->ImportText(
						*Value, Property->ContainerPtrToValuePtr<uint8>(this, Index), PortFlags, this))
				{
					UE_LOG(LogObj, Warning, TEXT("LoadConfig (%s): import failed for %s in: %s"), *GetPathName(),
						*IndexedKey, *Value);
				}
			}
			continue;
		}

		const FConfigFile* File = GConfig->FindConfigFile(PropFileName);
		const FConfigSection* Sec = File ? File->Find(Section) : nullptr;
		if (!Sec)
		{
			continue;
		}
		// The values the layers left for the key ("+Key=", "Key=", after "-Key=" and "!Key" edits), in order (UE).
		TArray<FConfigValue> List;
		const FName KeyName(*Key, FNAME_Find);
		if (!KeyName.IsNone())
		{
			Sec->MultiFind(KeyName, List, true);
		}
		FScriptArrayHelper ArrayHelper(Array, Property->ContainerPtrToValuePtr<void>(this));
		if (List.Num() > 0)
		{
			ArrayHelper.EmptyAndAddValues(List.Num());
			for (int32 Index = 0; Index < List.Num(); ++Index)
			{
				if (!Array->Inner->ImportText(*List[Index].GetValue(), ArrayHelper.GetRawPtr(Index), PortFlags, this))
				{
					UE_LOG(LogObj, Warning, TEXT("LoadConfig (%s): import failed for %s[%d] in: %s"), *GetPathName(),
						*Key, Index, *List[Index].GetValue());
				}
			}
			continue;
		}
		// "Key[N]=" sets element N, growing the array; the other elements keep their value (UE).
		int32 Index = 0;
		const FConfigValue* ElementValue = nullptr;
		do
		{
			const FName IndexedName(*GetIndexedKey(Key, Index), FNAME_Find);
			if (IndexedName.IsNone())
			{
				break;
			}
			ElementValue = Sec->Find(IndexedName);
			if (ElementValue)
			{
				if (Index >= ArrayHelper.Num())
				{
					ArrayHelper.Resize(Index + 1);
				}
				Array->Inner->ImportText(*ElementValue->GetValue(), ArrayHelper.GetRawPtr(Index), PortFlags, this);
			}
			++Index;
		} while (ElementValue || Index < ArrayHelper.Num());
	}

	// The child classes' defaults (UE: LCPF_PropagateToChildDefaultObjects): each direct child, and so on down.
	if (PropagationFlags & UE4::LCPF_PropagateToChildDefaultObjects)
	{
		TArray<UClass*> ChildClasses;
		GetDerivedClasses(ConfigClass, ChildClasses, /*bRecursive =*/false);
		for (UClass* ChildClass : ChildClasses)
		{
			ChildClass->GetDefaultObject()->LoadConfig(ChildClass, nullptr,
				UE4::LCPF_PropagateToChildDefaultObjects | (PropagationFlags & UE4::LCPF_PersistentFlags),
				PropertyToLoad);
		}
	}

	if (PropagationFlags & UE4::LCPF_ReloadingConfigData)
	{
		PostReloadConfig(PropertyToLoad);
	}

	// The instances of a class default object that reloads (UE: LCPF_PropagateToInstances). Each reads its class's
	// sections, parents first, as its class default object did.
	if ((PropagationFlags & UE4::LCPF_PropagateToInstances) && HasAnyFlags(RF_ClassDefaultObject))
	{
		TArray<UObject*> Instances;
		GetObjectsOfClass(GetClass(), Instances, /*bIncludeDerivedClasses =*/true, RF_ClassDefaultObject);
		for (UObject* Instance : Instances)
		{
			if (!Instance->IsTemplate() && !Instance->IsPendingKillOrUnreachable())
			{
				Instance->LoadConfig(nullptr, InFilename,
					UE4::LCPF_ReadParentSections | (PropagationFlags & UE4::LCPF_PersistentFlags), PropertyToLoad);
			}
		}
	}
}

void UObject::ReloadConfig(
	UClass* ConfigClass, const TCHAR* InFilename, uint32 PropagationFlags, FProperty* PropertyToLoad)
{
	LoadConfig(ConfigClass, InFilename,
		PropagationFlags | UE4::LCPF_ReloadingConfigData | UE4::LCPF_PropagateToInstances |
			(HasAnyFlags(RF_ClassDefaultObject) ? UE4::LCPF_ReadParentSections : UE4::LCPF_None),
		PropertyToLoad);
}

void UObject::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	(void)PropertyThatWasLoaded;
}

void UObject::OverridePerObjectConfigSection(FString& SectionName)
{
	(void)SectionName;
}

FString UObject::GetDefaultConfigFilename() const
{
	return FPaths::ProjectConfigDir() + TEXT("Default") + GetClass()->ClassConfigName.ToString() + TEXT(".ini");
}

void UObject::SaveConfig(uint64 Flags, const TCHAR* InFilename, FConfigCacheIni* Config, bool bAllowCopyToDefaultObject)
{
	if (!GetClass()->HasAnyClassFlags(CLASS_Config))
	{
		return;
	}
#if !PLATFORM_DESKTOP
	// Consoles have no writable user layer (plan decision D8).
	(void)Flags;
	(void)InFilename;
	(void)Config;
	(void)bAllowCopyToDefaultObject;
	UE_LOG(LogObj, Log, TEXT("SaveConfig(%s): config files are read-only on this platform"), *GetPathName());
#else
	if (!Config)
	{
		UE_LOG(LogObj, Warning, TEXT("SaveConfig(%s): no config system"), *GetPathName());
		return;
	}
	const FString Filename = InFilename ? FString(InFilename) : GetConfigFilename(this);
	const bool bPerObject = UsesPerObjectConfig(this);
	UObject* CDO = GetClass()->GetDefaultObject();
	// Only an instance saved to GConfig updates its class defaults (UE).
	const bool bCopyValues = bAllowCopyToDefaultObject && this != CDO && Config == GConfig;
	constexpr int32 PortFlags = PPF_ConfigOnly;
	TArray<FString> SavedFiles;
	SavedFiles.Add(Filename);

	for (FProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Config) || (uint64(Property->PropertyFlags) & Flags) != Flags)
		{
			continue;
		}
		const bool bGlobalConfig = Property->HasAnyPropertyFlags(CPF_GlobalConfig);
		UClass* BaseClass = bGlobalConfig ? Property->GetOwnerClass() : GetClass();
		const FString Section = GetConfigSection(this, BaseClass, bPerObject);
		const FString PropFileName =
			(bGlobalConfig && !InFilename) ? Property->GetOwnerClass()->GetConfigName() : Filename;
		SavedFiles.AddUnique(PropFileName);
		const FString Key = Property->GetName();

		if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			// Every element as a "+Key=" value; the saved user layer clears what the lower layers had (UE).
			FScriptArrayHelper ArrayHelper(Array, Property->ContainerPtrToValuePtr<void>(this));
			TArray<FString> Values;
			for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
			{
				FString Buffer;
				Array->Inner->ExportTextItem(
					Buffer, ArrayHelper.GetRawPtr(Index), ArrayHelper.GetRawPtr(Index), this, PortFlags);
				Values.Add(MoveTemp(Buffer));
			}
			Config->SetArray(*Section, *Key, Values, PropFileName);
		}
		else
		{
			for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
			{
				const FString IndexedKey = Property->ArrayDim != 1 ? GetIndexedKey(Key, Index) : Key;
				FString Value;
				Property->ExportText_InContainer(Index, Value, this, this, this, PortFlags);
				Config->SetString(*Section, *IndexedKey, *Value, PropFileName);
			}
		}

		if (bCopyValues)
		{
			Property->CopyCompleteValue(
				Property->ContainerPtrToValuePtr<void>(CDO), Property->ContainerPtrToValuePtr<void>(this));
		}
	}

	// Writes the user layer of each file touched (UE: Config->Flush(false, Filename)).
	for (const FString& SavedFile : SavedFiles)
	{
		Config->Flush(false, SavedFile);
	}
#endif
}

// Console commands (UE: Obj.cpp)

bool UObject::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	return CallFunctionByNameWithArguments(Cmd, Ar, Executor);
}

namespace UE::CoreUObject::Private
{
	void CastCheckedFailed(const UObject* Src, const UClass* ToClass)
	{
		if (Src)
		{
			UE_LOG(LogObj, Fatal, TEXT("Cast of %s to %s failed"), *Src->GetFullName(), *ToClass->GetName());
		}
		else
		{
			UE_LOG(LogObj, Fatal, TEXT("Cast of nullptr to %s failed"), *ToClass->GetName());
		}
	}
} // namespace UE::CoreUObject::Private

// UObject is intrinsic: its UClass and its (empty) reflection data are written by hand (UE: the UObject entries of
// CoreUObject's generated code).
IMPLEMENT_CLASS(UObject, 0)

COREUOBJECT_API UClass* Z_Construct_UClass_UObject_NoRegister()
{
	return UObject::StaticClass();
}

COREUOBJECT_API UClass* Z_Construct_UClass_UObject()
{
	static UClass* Class = nullptr;
	if (!Class)
	{
		Class = UObject::StaticClass();
		UObjectForceRegistration(Class);
		Class->ClassFlags |= CLASS_Constructed;
		Class->StaticLink();
	}
	return Class;
}
