#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReflectionTestTypes.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	template <typename PropertyType>
	PropertyType* FindContainerProperty(const TCHAR* Name)
	{
		return CastField<PropertyType>(UReflectionTestObject::StaticClass()->FindPropertyByName(FName(Name)));
	}

	/** Raw memory for one value of a property, constructed and destroyed through it. */
	struct FPropertyValue
	{
		explicit FPropertyValue(const FProperty* InProperty)
			: Property(InProperty)
			, Memory((uint8*)FMemory::Malloc(SIZE_T(InProperty->GetSize()), uint32(InProperty->GetMinAlignment())))
		{
			FMemory::Memset(Memory, 0xCD, SIZE_T(InProperty->GetSize()));
			Property->InitializeValue(Memory);
		}

		~FPropertyValue()
		{
			Property->DestroyValue(Memory);
			FMemory::Free(Memory);
		}

		FPropertyValue(const FPropertyValue&) = delete;
		FPropertyValue& operator=(const FPropertyValue&) = delete;

		const FProperty* Property;
		uint8* Memory;
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContainerArrayTest, "System.CoreUObject.Container.Array",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FContainerArrayTest::RunTest(const FString& Parameters)
{
	FArrayProperty* StringArray = FindContainerProperty<FArrayProperty>(TEXT("StringArray"));
	if (!TestNotNull(TEXT("Array property"), StringArray))
	{
		return false;
	}
	TestTrue(TEXT("Script array layout"),
		sizeof(FScriptArray) == sizeof(TArray<FString>) && StringArray->ElementSize == int32(sizeof(TArray<FString>)));

	// A value built through the property is a real TArray.
	FPropertyValue Value(StringArray);
	TArray<FString>& Native = *(TArray<FString>*)Value.Memory;
	TestEqual(TEXT("InitializeValue: empty"), Native.Num(), 0);
	FScriptArrayHelper Helper(StringArray, Value.Memory);
	const int32 First = Helper.AddValues(3);
	TestTrue(TEXT("AddValues"), First == 0 && Native.Num() == 3 && Native[2].IsEmpty());
	Native[0] = TEXT("Zero");
	Native[1] = TEXT("One");
	Native[2] = TEXT("Two");
	Native.Add(TEXT("Three"));
	TestEqual(TEXT("Native growth seen by the helper"), Helper.Num(), 4);
	TestEqual(TEXT("Element through the helper"), *(FString*)Helper.GetRawPtr(3), TEXT("Three"));
	Helper.RemoveValues(1, 2);
	TestTrue(TEXT("RemoveValues"), Native.Num() == 2 && Native[0] == TEXT("Zero") && Native[1] == TEXT("Three"));
	Helper.InsertValues(1);
	TestTrue(TEXT("InsertValues"), Native.Num() == 3 && Native[1].IsEmpty() && Native[2] == TEXT("Three"));
	Helper.Resize(1);
	TestTrue(TEXT("Resize"), Native.Num() == 1);

	// Copy and compare.
	FPropertyValue Copy(StringArray);
	StringArray->CopyCompleteValue(Copy.Memory, Value.Memory);
	TArray<FString>& NativeCopy = *(TArray<FString>*)Copy.Memory;
	TestTrue(TEXT("CopyCompleteValue"), NativeCopy.Num() == 1 && NativeCopy[0] == TEXT("Zero"));
	TestTrue(TEXT("Identical"), StringArray->Identical(Value.Memory, Copy.Memory));
	NativeCopy.Add(TEXT("More"));
	TestFalse(TEXT("Not identical"), StringArray->Identical(Value.Memory, Copy.Memory));
	StringArray->ClearValue(Copy.Memory);
	TestEqual(TEXT("ClearValue"), NativeCopy.Num(), 0);

	// POD elements and arrays of structs, inside an object.
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	UReflectionTestObject* Other = NewObject<UReflectionTestObject>();
	Object->StructArray.Add({4, TEXT("Four")});
	FArrayProperty* StructArray = FindContainerProperty<FArrayProperty>(TEXT("StructArray"));
	StructArray->CopyCompleteValue_InContainer(Other, Object);
	TestTrue(TEXT("Struct array copy"), Other->StructArray.Num() == 1 && Other->StructArray[0].Label == TEXT("Four"));
	FArrayProperty* IntArray = FindContainerProperty<FArrayProperty>(TEXT("IntArray"));
	Object->IntArray = {5, 6, 7, 8};
	IntArray->CopyCompleteValue_InContainer(Other, Object);
	TestTrue(TEXT("POD array copy"), Other->IntArray.Num() == 4 && Other->IntArray[3] == 8);
	FString Text;
	IntArray->ExportText_InContainer(0, Text, Other, nullptr, nullptr, PPF_None);
	TestEqual(TEXT("Export"), Text, TEXT("(5,6,7,8)"));
	TestTrue(TEXT("Import"),
		IntArray->ImportText(TEXT("(1, 2)"), IntArray->ContainerPtrToValuePtr<void>(Other), PPF_None, Other) !=
				nullptr &&
			Other->IntArray.Num() == 2 && Other->IntArray[1] == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContainerSetTest, "System.CoreUObject.Container.Set",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FContainerSetTest::RunTest(const FString& Parameters)
{
	FSetProperty* NameSet = FindContainerProperty<FSetProperty>(TEXT("NameSet"));
	if (!TestNotNull(TEXT("Set property"), NameSet))
	{
		return false;
	}
	TestTrue(TEXT("Script set layout"),
		sizeof(FScriptSet) == sizeof(TSet<FName>) && NameSet->SetLayout.Size == int32(sizeof(TSetElement<FName>)));

	FPropertyValue Value(NameSet);
	TSet<FName>& Native = *(TSet<FName>*)Value.Memory;
	TestEqual(TEXT("InitializeValue: empty"), Native.Num(), 0);
	FScriptSetHelper Helper(NameSet, Value.Memory);
	for (const TCHAR* Name : {TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F")})
	{
		const FName Element(Name);
		Helper.AddElement(&Element);
	}
	const FName Duplicate(TEXT("C"));
	Helper.AddElement(&Duplicate);
	TestEqual(TEXT("AddElement ignores duplicates"), Native.Num(), 6);
	TestTrue(TEXT("Native lookup of reflected adds"),
		Native.Contains(TEXT("A")) && Native.Contains(TEXT("F")) && !Native.Contains(TEXT("G")));
	Native.Add(TEXT("G"));
	const FName G(TEXT("G"));
	TestTrue(TEXT("Reflected lookup of native adds"), Helper.FindElementIndex(&G) != INDEX_NONE);
	const FName B(TEXT("B"));
	TestTrue(TEXT("RemoveElement"), Helper.RemoveElement(&B) && !Native.Contains(TEXT("B")) && Native.Num() == 6);
	Native.Remove(TEXT("D"));
	const FName D(TEXT("D"));
	TestEqual(TEXT("Native remove seen by the helper"), Helper.FindElementIndex(&D), int32(INDEX_NONE));

	FPropertyValue Copy(NameSet);
	NameSet->CopyCompleteValue(Copy.Memory, Value.Memory);
	TSet<FName>& NativeCopy = *(TSet<FName>*)Copy.Memory;
	TestTrue(TEXT("CopyCompleteValue"),
		NativeCopy.Num() == Native.Num() && NativeCopy.Contains(TEXT("G")) && NativeCopy.Contains(TEXT("A")));
	TestTrue(TEXT("Identical"), NameSet->Identical(Value.Memory, Copy.Memory));
	NativeCopy.Remove(TEXT("A"));
	NativeCopy.Add(TEXT("Z"));
	TestFalse(TEXT("Not identical"), NameSet->Identical(Value.Memory, Copy.Memory));
	NameSet->ClearValue(Copy.Memory);
	TestEqual(TEXT("ClearValue"), NativeCopy.Num(), 0);
	NativeCopy.Add(TEXT("AfterClear"));
	TestTrue(TEXT("Usable after ClearValue"), NativeCopy.Contains(TEXT("AfterClear")));

	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	TestTrue(TEXT("Import"),
		NameSet->ImportText(TEXT("(X, Y, X)"), NameSet->ContainerPtrToValuePtr<void>(Object), PPF_None, Object) !=
				nullptr &&
			Object->NameSet.Num() == 2 && Object->NameSet.Contains(TEXT("Y")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContainerMapTest, "System.CoreUObject.Container.Map",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FContainerMapTest::RunTest(const FString& Parameters)
{
	FMapProperty* StringToStruct = FindContainerProperty<FMapProperty>(TEXT("StringToStruct"));
	FMapProperty* NameToInt = FindContainerProperty<FMapProperty>(TEXT("NameToInt"));
	if (!TestTrue(TEXT("Map properties"), StringToStruct && NameToInt))
	{
		return false;
	}
	typedef TPair<FString, FReflectionTestInner> FPairType;
	FPairType Pair;
	const int32 ValueOffset = int32((const uint8*)&Pair.Value - (const uint8*)&Pair);
	TestTrue(TEXT("Script map layout"),
		sizeof(FScriptMap) == sizeof(TMap<FString, FReflectionTestInner>) &&
			StringToStruct->MapLayout.ValueOffset == ValueOffset &&
			StringToStruct->MapLayout.SetLayout.Size == int32(sizeof(TSetElement<FPairType>)));

	FPropertyValue Value(StringToStruct);
	TMap<FString, FReflectionTestInner>& Native = *(TMap<FString, FReflectionTestInner>*)Value.Memory;
	FScriptMapHelper Helper(StringToStruct, Value.Memory);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FString Key = FString::Printf(TEXT("Key%d"), Index);
		FReflectionTestInner Inner;
		Inner.Id = Index;
		Inner.Label = Key;
		Helper.AddPair(&Key, &Inner);
	}
	TestEqual(TEXT("AddPair"), Native.Num(), 6);
	TestTrue(TEXT("Native lookup of reflected adds"), Native.Contains(TEXT("Key3")) && Native[TEXT("Key3")].Id == 3);
	TestTrue(TEXT("Keys compare like the native map (case-insensitive)"), Native.Contains(TEXT("KEY5")));
	FReflectionTestInner Replacement;
	Replacement.Id = 30;
	const FString Key3(TEXT("Key3"));
	Helper.AddPair(&Key3, &Replacement);
	TestTrue(TEXT("AddPair replaces"), Native.Num() == 6 && Native[TEXT("Key3")].Id == 30);
	Native.Add(TEXT("Native"), FReflectionTestInner());
	const FString NativeKey(TEXT("Native"));
	TestTrue(TEXT("Reflected lookup of native adds"), Helper.FindValueFromHash(&NativeKey) != nullptr);
	const FString NewKey(TEXT("New"));
	FReflectionTestInner* Added = (FReflectionTestInner*)Helper.FindOrAdd(&NewKey);
	TestTrue(TEXT("FindOrAdd"), Added && Added->Id == 0 && Native.Contains(TEXT("New")));
	TestTrue(TEXT("RemovePair"), Helper.RemovePair(&Key3) && !Native.Contains(TEXT("Key3")));

	FPropertyValue Copy(StringToStruct);
	StringToStruct->CopyCompleteValue(Copy.Memory, Value.Memory);
	TMap<FString, FReflectionTestInner>& NativeCopy = *(TMap<FString, FReflectionTestInner>*)Copy.Memory;
	TestTrue(
		TEXT("CopyCompleteValue"), NativeCopy.Num() == Native.Num() && NativeCopy[TEXT("Key4")].Label == TEXT("Key4"));
	TestTrue(TEXT("Identical"), StringToStruct->Identical(Value.Memory, Copy.Memory));
	NativeCopy[TEXT("Key4")].Id = 44;
	TestFalse(TEXT("Values compared"), StringToStruct->Identical(Value.Memory, Copy.Memory));
	StringToStruct->DestroyValue(Copy.Memory);
	StringToStruct->InitializeValue(Copy.Memory);
	TestEqual(TEXT("Destroy then initialize"), NativeCopy.Num(), 0);

	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	Object->NameToInt.Add(TEXT("One"), 1);
	Object->NameToInt.Add(TEXT("Two"), 2);
	FString Text;
	NameToInt->ExportText_InContainer(0, Text, Object, nullptr, nullptr, PPF_None);
	// Names inside a container are delimited (quoted), as UE exports them.
	TestTrue(TEXT("Export"), Text == TEXT("((\"One\",1),(\"Two\",2))") || Text == TEXT("((\"Two\",2),(\"One\",1))"));
	UReflectionTestObject* Other = NewObject<UReflectionTestObject>();
	TestTrue(TEXT("Import"),
		NameToInt->ImportText(*Text, NameToInt->ContainerPtrToValuePtr<void>(Other), PPF_None, Other) != nullptr &&
			Other->NameToInt.Num() == 2 && Other->NameToInt[TEXT("Two")] == 2);
	NameToInt->CopyCompleteValue_InContainer(Other, Object);
	TestTrue(TEXT("Copy in an object"), NameToInt->Identical_InContainer(Object, Other));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
