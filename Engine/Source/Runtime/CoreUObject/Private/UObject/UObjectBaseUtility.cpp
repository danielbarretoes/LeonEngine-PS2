#include "UObject/UObjectBaseUtility.h"

#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectArray.h"

EInternalObjectFlags UObjectBaseUtility::GetInternalFlags() const
{
	const FUObjectItem* Item = GUObjectArray.IndexToObject(int32(GetUniqueID()));
	return Item ? Item->GetFlags() : EInternalObjectFlags::None;
}

bool UObjectBaseUtility::HasAnyInternalFlags(EInternalObjectFlags FlagsToCheck) const
{
	const FUObjectItem* Item = GUObjectArray.IndexToObject(int32(GetUniqueID()));
	return Item && Item->HasAnyFlags(FlagsToCheck);
}

void UObjectBaseUtility::SetInternalFlags(EInternalObjectFlags FlagsToSet) const
{
	FUObjectItem* Item = GUObjectArray.IndexToObject(int32(GetUniqueID()));
	check(Item);
	Item->SetFlags(FlagsToSet);
}

void UObjectBaseUtility::ClearInternalFlags(EInternalObjectFlags FlagsToClear) const
{
	FUObjectItem* Item = GUObjectArray.IndexToObject(int32(GetUniqueID()));
	check(Item);
	Item->ClearFlags(FlagsToClear);
}

void UObjectBaseUtility::MarkPendingKill()
{
	checkf(!IsRooted(), "Cannot mark the rooted object %s pending kill", *GetFullName());
	SetInternalFlags(EInternalObjectFlags::PendingKill);
}

void UObjectBaseUtility::ClearPendingKill()
{
	ClearInternalFlags(EInternalObjectFlags::PendingKill);
}

bool UObjectBaseUtility::IsPendingKill() const
{
	return HasAnyInternalFlags(EInternalObjectFlags::PendingKill);
}

void UObjectBaseUtility::AddToRoot()
{
	SetInternalFlags(EInternalObjectFlags::RootSet);
}

void UObjectBaseUtility::RemoveFromRoot()
{
	ClearInternalFlags(EInternalObjectFlags::RootSet);
}

bool UObjectBaseUtility::IsRooted() const
{
	return HasAnyInternalFlags(EInternalObjectFlags::RootSet);
}

bool UObjectBaseUtility::IsTemplate(EObjectFlags TemplateTypes) const
{
	for (const UObjectBaseUtility* TestOuter = this; TestOuter; TestOuter = TestOuter->GetOuter())
	{
		if (TestOuter->HasAnyFlags(TemplateTypes))
		{
			return true;
		}
	}
	return false;
}

FString UObjectBaseUtility::GetName() const
{
	return GetFName().ToString();
}

void UObjectBaseUtility::GetName(FString& ResultString) const
{
	GetFName().ToString(ResultString);
}

FString UObjectBaseUtility::GetPathName(const UObject* StopOuter) const
{
	FString Result;
	GetPathName(StopOuter, Result);
	return Result;
}

void UObjectBaseUtility::GetPathName(const UObject* StopOuter, FString& ResultString) const
{
	if (this == (const UObjectBaseUtility*)StopOuter)
	{
		ResultString += TEXT("None");
		return;
	}
	UObject* ObjOuter = GetOuter();
	if (ObjOuter && ObjOuter != StopOuter)
	{
		ObjOuter->GetPathName(StopOuter, ResultString);
		// ':' marks an object whose outer is not a package but whose outer's outer is (UE: SUBOBJECT_DELIMITER).
		UObject* OuterOuter = ObjOuter->GetOuter();
		if (ObjOuter->GetClass() != UPackage::StaticClass() && OuterOuter &&
			OuterOuter->GetClass() == UPackage::StaticClass())
		{
			ResultString += SUBOBJECT_DELIMITER;
		}
		else
		{
			ResultString += TEXT(".");
		}
	}
	GetFName().AppendString(ResultString);
}

FString UObjectBaseUtility::GetFullName(const UObject* StopOuter) const
{
	FString Result;
	if (GetClass())
	{
		Result = GetClass()->GetName();
		Result += TEXT(" ");
		GetPathName(StopOuter, Result);
	}
	else
	{
		Result = TEXT("None");
	}
	return Result;
}

UPackage* UObjectBaseUtility::GetOutermost() const
{
	UObject* Top = (UObject*)this;
	while (UObject* Outer = Top->GetOuter())
	{
		Top = Outer;
	}
	return (UPackage*)Top;
}

UObject* UObjectBaseUtility::GetTypedOuter(UClass* Target) const
{
	for (UObject* NextOuter = GetOuter(); NextOuter; NextOuter = NextOuter->GetOuter())
	{
		if (NextOuter->IsA(Target))
		{
			return NextOuter;
		}
	}
	return nullptr;
}

bool UObjectBaseUtility::IsIn(const UObject* SomeOuter) const
{
	for (UObject* It = GetOuter(); It; It = It->GetOuter())
	{
		if (It == SomeOuter)
		{
			return true;
		}
	}
	return SomeOuter == nullptr;
}

bool UObjectBaseUtility::IsA(const UClass* SomeBase) const
{
	const UClass* ThisClass = GetClass();
	return SomeBase && ThisClass && ThisClass->IsChildOf(SomeBase);
}
