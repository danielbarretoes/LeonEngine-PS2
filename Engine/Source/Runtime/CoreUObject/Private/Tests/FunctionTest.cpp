#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReflectionTestTypes.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFunctionLookupTest, "System.CoreUObject.Function.Lookup",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFunctionLookupTest::RunTest(const FString& Parameters)
{
	UClass* Class = UReflectionTestObject::StaticClass();
	UFunction* AddNumbers = Class->FindFunctionByName(TEXT("AddNumbers"));
	if (!TestNotNull(TEXT("FindFunctionByName"), AddNumbers))
	{
		return false;
	}
	TestTrue(TEXT("Function outer"), AddNumbers->GetOuter() == Class && AddNumbers->GetOwnerClass() == Class);
	TestTrue(TEXT("Native with a thunk"), AddNumbers->HasAnyFunctionFlags(FUNC_Native) && AddNumbers->GetNativeFunc());
	TestTrue(TEXT("Const and final"), AddNumbers->HasAllFunctionFlags(FUNC_Const | FUNC_Final | FUNC_Public));
	TestEqual(TEXT("NumParms"), int32(AddNumbers->NumParms), 3);
	TestEqual(TEXT("ParmsSize"), int32(AddNumbers->ParmsSize), 12);
	TestEqual(TEXT("ReturnValueOffset"), int32(AddNumbers->ReturnValueOffset), 8);
	FProperty* ReturnProperty = AddNumbers->GetReturnProperty();
	TestTrue(TEXT("Return property"),
		ReturnProperty && ReturnProperty->GetFName() == TEXT("ReturnValue") &&
			ReturnProperty->HasAllPropertyFlags(CPF_Parm | CPF_OutParm | CPF_ReturnParm));

	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	TestTrue(
		TEXT("UObject::FindFunction"), Object->FindFunction(TEXT("Reset")) == Class->FindFunctionByName(TEXT("Reset")));
	TestNull(TEXT("Missing function"), Object->FindFunction(TEXT("NoSuchFunction")));
	UFunction* Twice = Class->FindFunctionByName(TEXT("Twice"));
	TestTrue(TEXT("Static function"), Twice && Twice->HasAnyFunctionFlags(FUNC_Static));
	UFunction* SumArray = Class->FindFunctionByName(TEXT("SumArray"));
	TestTrue(TEXT("By-reference parameter"), SumArray && SumArray->HasAnyFunctionFlags(FUNC_HasOutParms));

	TArray<FString> Names;
	for (TFieldIterator<UFunction> It(Class); It; ++It)
	{
		Names.Add(It->GetName());
	}
	TestTrue(TEXT("Functions in declaration order"),
		Names.Num() == 8 && Names[0] == TEXT("AddNumbers") && Names[7] == TEXT("Reset"));
	TArray<FString> ParameterNames;
	for (TFieldIterator<FProperty> It(Class->FindFunctionByName(TEXT("Describe"))); It; ++It)
	{
		ParameterNames.Add(It->GetName());
	}
	TestTrue(TEXT("Parameters in order"),
		ParameterNames.Num() == 4 && ParameterNames[0] == TEXT("Prefix") && ParameterNames[2] == TEXT("bLoud") &&
			ParameterNames[3] == TEXT("ReturnValue"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFunctionProcessEventTest, "System.CoreUObject.Function.ProcessEvent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFunctionProcessEventTest::RunTest(const FString& Parameters)
{
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	UClass* Class = UReflectionTestObject::StaticClass();

	// A parameter block laid out like the generated _Parms struct.
	struct FAddNumbersParms
	{
		int32 A;
		int32 B;
		int32 ReturnValue;
	};
	FAddNumbersParms AddParms = {19, 23, 0};
	Object->ProcessEvent(Class->FindFunctionByName(TEXT("AddNumbers")), &AddParms);
	TestEqual(TEXT("Return value"), AddParms.ReturnValue, 42);

	// A block built through the reflection data: every parameter initialized by its property.
	UFunction* Describe = Class->FindFunctionByName(TEXT("Describe"));
	uint8* Parms = (uint8*)FMemory::Malloc(Describe->ParmsSize, uint32(Describe->GetMinAlignment()));
	Describe->InitializeStruct(Parms);
	for (TFieldIterator<FProperty> It(Describe); It; ++It)
	{
		FProperty* Property = *It;
		if (Property->GetFName() == TEXT("Prefix"))
		{
			*Property->ContainerPtrToValuePtr<FString>(Parms) = TEXT("Score");
		}
		else if (FFloatProperty* Scale = CastField<FFloatProperty>(Property))
		{
			Scale->SetPropertyValue_InContainer(Parms, 1.5f);
		}
		else if (FBoolProperty* Loud = CastField<FBoolProperty>(Property))
		{
			Loud->SetPropertyValue_InContainer(Parms, true);
		}
	}
	Object->ProcessEvent(Describe, Parms);
	TestEqual(TEXT("String parameters and return value"),
		*Describe->GetReturnProperty()->ContainerPtrToValuePtr<FString>(Parms), TEXT("Score 15!"));
	Describe->DestroyStruct(Parms);
	FMemory::Free(Parms);

	// Struct return value.
	UFunction* MakeInner = Class->FindFunctionByName(TEXT("MakeInner"));
	uint8* InnerParms = (uint8*)FMemory::Malloc(MakeInner->ParmsSize, uint32(MakeInner->GetMinAlignment()));
	MakeInner->InitializeStruct(InnerParms);
	*CastField<FIntProperty>(MakeInner->FindPropertyByName(TEXT("Id")))->ContainerPtrToValuePtr<int32>(InnerParms) = 12;
	*MakeInner->FindPropertyByName(TEXT("Label"))->ContainerPtrToValuePtr<FName>(InnerParms) = TEXT("Twelve");
	Object->ProcessEvent(MakeInner, InnerParms);
	const FReflectionTestInner& Result =
		*MakeInner->GetReturnProperty()->ContainerPtrToValuePtr<FReflectionTestInner>(InnerParms);
	TestTrue(TEXT("Struct return value"), Result.Id == 12 && Result.Label == TEXT("Twelve"));
	MakeInner->DestroyStruct(InnerParms);
	FMemory::Free(InnerParms);

	// A by-reference array parameter is read in place.
	struct FSumArrayParms
	{
		TArray<int32> Values;
		int32 ReturnValue;
	};
	FSumArrayParms SumParms;
	SumParms.Values = {1, 2, 3, 4};
	SumParms.ReturnValue = 0;
	Object->ProcessEvent(Class->FindFunctionByName(TEXT("SumArray")), &SumParms);
	TestEqual(TEXT("Reference parameter"), SumParms.ReturnValue, 10);

	// Enum, object and void functions, and a static function.
	struct FSetModeParms
	{
		EReflectionTestMode NewMode;
	};
	FSetModeParms ModeParms = {EReflectionTestMode::Third};
	Object->ProcessEvent(Class->FindFunctionByName(TEXT("SetMode")), &ModeParms);
	TestTrue(TEXT("Enum parameter"), Object->Mode == EReflectionTestMode::Third);
	struct FIsSameParms
	{
		UObject* Other;
		bool ReturnValue;
	};
	FIsSameParms SameParms = {Object, false};
	Object->ProcessEvent(Class->FindFunctionByName(TEXT("IsSameObject")), &SameParms);
	TestTrue(TEXT("Object parameter and bool return value"), SameParms.ReturnValue);
	Object->ProcessEvent(Class->FindFunctionByName(TEXT("Reset")), nullptr);
	TestEqual(TEXT("Void function"), Object->ResetCount, 1);
	struct FTwiceParms
	{
		int64 Value;
		int64 ReturnValue;
	};
	FTwiceParms TwiceParms = {int64(1) << 33, 0};
	Object->ProcessEvent(Class->FindFunctionByName(TEXT("Twice")), &TwiceParms);
	TestTrue(TEXT("Static function"), TwiceParms.ReturnValue == (int64(1) << 34));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
