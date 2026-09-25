// Constructors and UFUNCTION bodies of the reflected test fixtures.

#include "Tests/HierarchyTestTypes.h"
#include "Tests/OrderTestChild.h"
#include "Tests/OrderTestParent.h"
#include "Tests/ReflectionTestTypes.h"

// UReflectionTestObject

UReflectionTestObject::UReflectionTestObject()
{
	bFlagA = 1;
	bFlagB = 0;
	bFlagC = 1;
	TextValue = FText::FromString(TEXT("Text"));
	StructValue.Id = 5;
	StructValue.Label = TEXT("Inner");
	FixedArray[0] = 1.0f;
	FixedArray[1] = 2.0f;
	FixedArray[2] = 4.0f;
	IntArray = {1, 2, 3};
}

int32 UReflectionTestObject::AddNumbers(int32 A, int32 B) const
{
	return A + B;
}

FString UReflectionTestObject::Describe(const FString& Prefix, float Scale, bool bLoud) const
{
	FString Result = Prefix;
	Result += FString::Printf(TEXT(" %d"), int32(Scale * 10.0f));
	if (bLoud)
	{
		Result += TEXT("!");
	}
	return Result;
}

FReflectionTestInner UReflectionTestObject::MakeInner(int32 Id, FName Label) const
{
	FReflectionTestInner Result;
	Result.Id = Id;
	Result.Label = Label.ToString();
	return Result;
}

int32 UReflectionTestObject::SumArray(const TArray<int32>& Values) const
{
	int32 Sum = 0;
	for (const int32 Value : Values)
	{
		Sum += Value;
	}
	return Sum;
}

int64 UReflectionTestObject::Twice(int64 Value)
{
	return Value * 2;
}

void UReflectionTestObject::SetMode(EReflectionTestMode NewMode)
{
	Mode = NewMode;
}

bool UReflectionTestObject::IsSameObject(UObject* Other) const
{
	return Other == this;
}

void UReflectionTestObject::Reset()
{
	++ResetCount;
}

// Hierarchy fixtures

UHierarchyTestSpecialComponent::UHierarchyTestSpecialComponent()
{
	Power = 50;
}

UHierarchyTestBase::UHierarchyTestBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BaseValue = 100;
}

UHierarchyTestChild::UHierarchyTestChild(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Component = CreateDefaultSubobject<UHierarchyTestComponent>(TEXT("Component"));
	ChildValue = 200;
}

UHierarchyTestGrandChild::UHierarchyTestGrandChild(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UHierarchyTestSpecialComponent>(TEXT("Component")))
{
	GrandChildValue = 300.0f;
}

UHierarchyTestOptionalOwner::UHierarchyTestOptionalOwner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Optional = ObjectInitializer.CreateOptionalDefaultSubobject<UHierarchyTestComponent>(this, TEXT("Optional"));
}

UHierarchyTestNoOptional::UHierarchyTestNoOptional(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("Optional")))
{
}

// Registration-order fixtures

UOrderTestParent::UOrderTestParent()
{
	if (HasAnyFlags(RF_ClassDefaultObject) && GetClass() == UOrderTestParent::StaticClass())
	{
		DefaultObjectSequence = FOrderTestSequence::Next();
	}
}

UOrderTestChild::UOrderTestChild()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefaultObjectSequence = FOrderTestSequence::Next();
	}
}
