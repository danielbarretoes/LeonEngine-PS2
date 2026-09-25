#include "UObject/Field.h"

#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogField, Log, All);

// FFieldClass

FFieldClass::FFieldClass(const TCHAR* InCPPName, uint64 InId, uint64 InCastFlags, FFieldClass* InSuperClass,
	FConstructFunction InConstructFn)
	: Name(InCPPName[0] == 'F' ? InCPPName + 1 : InCPPName)
	, Id(InId)
	, CastFlags(InCastFlags)
	, SuperClass(InSuperClass)
	, ConstructFn(InConstructFn)
{
}

// FField

FField::FField(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Next(nullptr)
	, ClassPrivate(FField::StaticClass())
	, Owner(InOwner)
	, NamePrivate(InName)
	, FlagsPrivate(InObjectFlags)
{
}

FField::~FField()
{
}

FFieldClass* FField::StaticClass()
{
	static FFieldClass StaticFieldClass(TEXT("FField"), FField::StaticClassCastFlagsPrivate(),
		FField::StaticClassCastFlags(), nullptr, &FField::Construct);
	return &StaticFieldClass;
}

FField* FField::Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InObjectFlags)
{
	return new FField(InOwner, InName, InObjectFlags);
}

void* FField::operator new(size_t Size)
{
	return FMemory::Malloc(SIZE_T(Size));
}

void FField::operator delete(void* Memory)
{
	FMemory::Free(Memory);
}

UObject* FField::GetOwnerUObject() const
{
	FFieldVariant TempOuter = Owner;
	while (!TempOuter.IsUObject())
	{
		FField* OwnerField = TempOuter.ToField();
		if (!OwnerField)
		{
			return nullptr;
		}
		TempOuter = OwnerField->Owner;
	}
	return TempOuter.ToUObject();
}

UClass* FField::GetOwnerClass() const
{
	return Cast<UClass>(GetOwnerUObject());
}

UStruct* FField::GetOwnerStruct() const
{
	return Cast<UStruct>(GetOwnerUObject());
}

FString FField::GetFullName() const
{
	return GetClass()->GetName() + TEXT(" ") + GetPathName();
}

FString FField::GetPathName(const UObject* StopOuter) const
{
	FString Result;
	if (FField* OwnerField = Owner.ToField())
	{
		Result = OwnerField->GetPathName(StopOuter);
		Result += TEXT(".");
	}
	else if (UObject* OwnerObject = Owner.ToUObject())
	{
		Result = OwnerObject->GetPathName(StopOuter);
		Result += SUBOBJECT_DELIMITER;
	}
	GetFName().AppendString(Result);
	return Result;
}

void FField::AddCppProperty(FProperty* Property)
{
	UE_LOG(LogField, Fatal, TEXT("Field %s cannot own the property %s"), *GetName(), *Property->GetName());
}
