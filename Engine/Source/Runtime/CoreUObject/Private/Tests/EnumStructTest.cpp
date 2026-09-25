#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReflectionTestTypes.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnumNamesTest, "System.CoreUObject.Enum.NamesAndValues",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEnumNamesTest::RunTest(const FString& Parameters)
{
	UEnum* Enum = StaticEnum<EReflectionTestMode>();
	if (!TestNotNull(TEXT("StaticEnum"), Enum))
	{
		return false;
	}
	TestEqual(TEXT("Name"), Enum->GetName(), TEXT("EReflectionTestMode"));
	TestTrue(TEXT("Enum class form"), Enum->GetCppForm() == UEnum::ECppForm::EnumClass);
	TestEqual(TEXT("NumEnums includes _MAX"), Enum->NumEnums(), 4);
	TestEqual(TEXT("Full name"), Enum->GetNameByIndex(1).ToString(), TEXT("EReflectionTestMode::Second"));
	TestEqual(TEXT("Short name"), Enum->GetNameStringByIndex(1), TEXT("Second"));
	TestEqual(TEXT("Explicit value"), Enum->GetValueByIndex(2), int64(10));
	TestEqual(
		TEXT("_MAX entry"), Enum->GetNameByIndex(3).ToString(), TEXT("EReflectionTestMode::EReflectionTestMode_MAX"));
	TestEqual(TEXT("_MAX value"), Enum->GetValueByIndex(3), int64(11));
	TestEqual(TEXT("GetNameByValue"), Enum->GetNameByValue(10).ToString(), TEXT("EReflectionTestMode::Third"));
	TestEqual(TEXT("GetNameStringByValue"), Enum->GetNameStringByValue(0), TEXT("First"));
	TestEqual(TEXT("GetValueByName (full)"), Enum->GetValueByName(TEXT("EReflectionTestMode::Third")), int64(10));
	TestEqual(TEXT("GetValueByName (short)"), Enum->GetValueByName(TEXT("Second")), int64(1));
	TestEqual(TEXT("GetValueByNameString"), Enum->GetValueByNameString(TEXT("First")), int64(0));
	TestEqual(TEXT("Missing name"), Enum->GetValueByName(TEXT("Missing")), int64(INDEX_NONE));
	TestEqual(TEXT("GetIndexByValue"), Enum->GetIndexByValue(10), 2);
	TestTrue(TEXT("IsValidEnumValue"), Enum->IsValidEnumValue(1) && !Enum->IsValidEnumValue(5));
	TestEqual(TEXT("Display name"), Enum->GetDisplayNameTextByIndex(0).ToString(), TEXT("First"));
	TestEqual(TEXT("C++ type"), Enum->CppType, TEXT("EReflectionTestMode"));
	TestTrue(TEXT("Enum by path"), FindObject<UEnum>(nullptr, TEXT("/Script/CoreUObject.EReflectionTestMode")) == Enum);

	UEnum* Legacy = StaticEnum<EReflectionTestLegacy>();
	if (!TestNotNull(TEXT("Regular enum"), Legacy))
	{
		return false;
	}
	TestTrue(TEXT("Regular form"), Legacy->GetCppForm() == UEnum::ECppForm::Regular);
	TestEqual(TEXT("Regular names"), Legacy->GetNameStringByIndex(2), TEXT("RTL_Gamma"));
	TestEqual(TEXT("Regular _MAX uses the common prefix"), Legacy->GetNameByIndex(3).ToString(), TEXT("RTL_MAX"));
	TestEqual(TEXT("Regular _MAX value"), Legacy->GetValueByName(TEXT("RTL_MAX")), int64(3));
	TestEqual(TEXT("Regular lookup"), Legacy->GetValueByName(TEXT("RTL_Beta")), int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStructOpsTest, "System.CoreUObject.Struct.Ops",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStructOpsTest::RunTest(const FString& Parameters)
{
	UScriptStruct* Inner = FReflectionTestInner::StaticStruct();
	UScriptStruct* Derived = FReflectionTestStruct::StaticStruct();
	if (!TestTrue(TEXT("Structs"), Inner && Derived))
	{
		return false;
	}
	TestEqual(TEXT("Struct name"), Inner->GetName(), TEXT("ReflectionTestInner"));
	TestTrue(TEXT("Super struct"), Derived->GetSuperStruct() == Inner && Derived->IsChildOf(Inner));
	TestTrue(TEXT("StaticStruct<>"), StaticStruct<FReflectionTestInner>() == Inner);
	TestEqual(TEXT("Size"), Derived->GetStructureSize(), int32(sizeof(FReflectionTestStruct)));
	TestEqual(TEXT("Alignment"), Derived->GetMinAlignment(), int32(alignof(FReflectionTestStruct)));
	UScriptStruct::ICppStructOps* Ops = Inner->GetCppStructOps();
	TestTrue(TEXT("C++ operations"),
		Ops && Ops->HasDestructor() && Ops->HasCopy() && Ops->HasIdentical() && Ops->HasGetTypeHash() &&
			!Ops->IsPlainOldData());
	TestTrue(TEXT("Computed flags"),
		(Inner->StructFlags & STRUCT_IdenticalNative) && (Inner->StructFlags & STRUCT_CopyNative) &&
			!(Inner->StructFlags & STRUCT_NoDestructor));
	TestFalse(TEXT("Derived compares property by property"),
		Derived->GetCppStructOps() && Derived->GetCppStructOps()->HasIdentical());

	// Construct, copy, compare and destroy through the reflection data.
	uint8* A = (uint8*)FMemory::Malloc(Derived->GetStructureSize(), Derived->GetMinAlignment());
	uint8* B = (uint8*)FMemory::Malloc(Derived->GetStructureSize(), Derived->GetMinAlignment());
	Derived->InitializeStruct(A);
	Derived->InitializeStruct(B);
	FReflectionTestStruct& TypedA = *(FReflectionTestStruct*)A;
	FReflectionTestStruct& TypedB = *(FReflectionTestStruct*)B;
	TestTrue(
		TEXT("Constructed with the C++ defaults"), TypedA.Weight == 1.5f && TypedA.Id == 0 && TypedA.Tags.Num() == 0);
	TypedA.Id = 3;
	TypedA.Label = TEXT("Three");
	TypedA.Tags.Add(TEXT("Tag"));
	TestFalse(TEXT("Different"), Derived->CompareScriptStruct(A, B, 0));
	Derived->CopyScriptStruct(B, A);
	TestTrue(TEXT("Copied"), TypedB.Id == 3 && TypedB.Label == TEXT("Three") && TypedB.Tags.Num() == 1);
	TestTrue(TEXT("Identical after copy"), Derived->CompareScriptStruct(A, B, 0));
	TypedB.Tags[0] = TEXT("Other");
	TestFalse(TEXT("Property-wise comparison sees the array"), Derived->CompareScriptStruct(A, B, 0));
	TestTrue(TEXT("Equality operator of the base"), Inner->CompareScriptStruct(A, B, 0));
	TestEqual(TEXT("Hash"), Inner->GetStructTypeHash(A), GetTypeHash(TypedA.Id));
	Derived->ClearScriptStruct(A);
	TestTrue(TEXT("Cleared"), TypedA.Id == 0 && TypedA.Tags.Num() == 0 && TypedA.Weight == 1.5f);
	Derived->DestroyStruct(A);
	Derived->DestroyStruct(B);
	FMemory::Free(A);
	FMemory::Free(B);

	// Structs as properties.
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	FStructProperty* StructValue =
		CastField<FStructProperty>(UReflectionTestObject::StaticClass()->FindPropertyByName(TEXT("StructValue")));
	TestTrue(TEXT("Struct property default"),
		StructValue->Identical_InContainer(Object, GetDefault<UReflectionTestObject>()));
	Object->StructValue.Weight = 9.0f;
	TestFalse(TEXT("Struct property changed"),
		StructValue->Identical_InContainer(Object, GetDefault<UReflectionTestObject>()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
