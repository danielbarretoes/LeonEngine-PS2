#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
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
	SetFlags(RF_BeginDestroyed);
}

bool UObject::IsReadyForFinishDestroy()
{
	return true;
}

void UObject::FinishDestroy()
{
	SetFlags(RF_FinishDestroyed);
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
