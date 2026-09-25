// Constructors and native code of the package test fixtures.

#include "Tests/PackageTestTypes.h"

// UPackageTestObject

UPackageTestObject::UPackageTestObject()
{
	bBitA = 0;
	bBitB = 0;
	bBitC = 0;
}

void UPackageTestObject::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	// The native tail, after the tagged properties (UE: a class's own Serialize after Super::Serialize).
	Ar << NativeValue;
	BulkData.Serialize(Ar, this);
}

void UPackageTestObject::PostLoad()
{
	Super::PostLoad();
	++NumPostLoads;
	GetPostLoadLog().Add(GetPathName());
	const UPackageTestObject* Ref = Cast<UPackageTestObject>(ObjectRef);
	PostLoadSeenRefValue = Ref ? Ref->IntValue : INDEX_NONE;
}

TArray<FString>& UPackageTestObject::GetPostLoadLog()
{
	static TArray<FString> Log;
	return Log;
}

// UPackageTestOwner

UPackageTestOwner::UPackageTestOwner()
{
	// Built by every instance's constructor, then the loaded values are applied (D12).
	Component = CreateDefaultSubobject<UPackageTestSubobject>(TEXT("Component"));
	Component->Value = 5;
	Component->Label = TEXT("FromConstructor");
}
