#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReflectionTestTypes.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include <cstddef>

#if WITH_DEV_AUTOMATION_TESTS

	// The tests compare reflected offsets with offsetof on a UObject class, as the generated code does.
	#if defined(__GNUC__) || defined(__clang__)
		#pragma GCC diagnostic ignored "-Winvalid-offsetof"
	#endif

namespace
{
	FProperty* FindTestProperty(const TCHAR* Name)
	{
		return UReflectionTestObject::StaticClass()->FindPropertyByName(FName(Name));
	}

	FString ExportProperty(const UObject* Object, const TCHAR* Name, int32 Index = 0)
	{
		FString Result;
		FProperty* Property = FindTestProperty(Name);
		Property->ExportText_InContainer(Index, Result, Object, nullptr, nullptr, PPF_None);
		return Result;
	}

	bool ImportProperty(UObject* Object, const TCHAR* Name, const TCHAR* Text)
	{
		FProperty* Property = FindTestProperty(Name);
		return Property->ImportText(Text, Property->ContainerPtrToValuePtr<void>(Object), PPF_None, Object) != nullptr;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropertyIterationTest, "System.CoreUObject.Property.IterationOrder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPropertyIterationTest::RunTest(const FString& Parameters)
{
	TArray<FString> Expected = {TEXT("Int8Value"), TEXT("Int16Value"), TEXT("IntValue"), TEXT("Int64Value"),
		TEXT("ByteValue"), TEXT("UInt16Value"), TEXT("UInt32Value"), TEXT("UInt64Value"), TEXT("FloatValue"),
		TEXT("DoubleValue"), TEXT("bNativeBool"), TEXT("bFlagA"), TEXT("bFlagB"), TEXT("bFlagC"), TEXT("StringValue"),
		TEXT("NameValue"), TEXT("TextValue"), TEXT("Mode"), TEXT("Legacy"), TEXT("StructValue"), TEXT("Location"),
		TEXT("Transform"), TEXT("ObjectRef"), TEXT("ClassRef"), TEXT("WeakRef"), TEXT("SoftRef"), TEXT("IntArray"),
		TEXT("StringArray"), TEXT("StructArray"), TEXT("NameSet"), TEXT("NameToInt"), TEXT("StringToStruct"),
		TEXT("FixedArray"), TEXT("ConfigValue"), TEXT("TransientValue")};
	#if WITH_EDITORONLY_DATA
	Expected.Add(TEXT("EditorNote"));
	Expected.Add(TEXT("EditorCount"));
	#endif
	Expected.Add(TEXT("AfterEditorOnly"));

	TArray<FString> Actual;
	for (TFieldIterator<FProperty> It(UReflectionTestObject::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It;
		++It)
	{
		Actual.Add(It->GetName());
	}
	TestEqual(TEXT("Property count"), Actual.Num(), Expected.Num());
	for (int32 Index = 0; Index < FMath::Min(Actual.Num(), Expected.Num()); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Property %d"), Index), Actual[Index], Expected[Index]);
	}

	// Derived first, then the supers' (UE), through the PropertyLink chain too.
	TArray<FString> Hierarchy;
	for (TFieldIterator<FProperty> It(UReflectionTestObject::StaticClass()); It; ++It)
	{
		Hierarchy.Add(It->GetName());
	}
	TestEqual(TEXT("UObject has no properties"), Hierarchy.Num(), Expected.Num());
	int32 LinkCount = 0;
	for (FProperty* Property = UReflectionTestObject::StaticClass()->PropertyLink; Property;
		Property = Property->PropertyLinkNext)
	{
		++LinkCount;
	}
	TestEqual(TEXT("PropertyLink"), LinkCount, Expected.Num());

	TArray<FString> StructNames;
	for (TFieldIterator<FProperty> It(FReflectionTestStruct::StaticStruct()); It; ++It)
	{
		StructNames.Add(It->GetName());
	}
	TestTrue(TEXT("Struct: own properties, then the base's"),
		StructNames.Num() == 4 && StructNames[0] == TEXT("Weight") && StructNames[1] == TEXT("Tags") &&
			StructNames[2] == TEXT("Id") && StructNames[3] == TEXT("Label"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropertyTypesTest, "System.CoreUObject.Property.TypesAndOffsets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPropertyTypesTest::RunTest(const FString& Parameters)
{
	struct FExpectedProperty
	{
		const TCHAR* Name;
		FFieldClass* Class;
		SIZE_T Offset;
		int32 ElementSize;
	};
	const FExpectedProperty Expected[] = {
		{TEXT("Int8Value"), FInt8Property::StaticClass(), offsetof(UReflectionTestObject, Int8Value), 1},
		{TEXT("Int16Value"), FInt16Property::StaticClass(), offsetof(UReflectionTestObject, Int16Value), 2},
		{TEXT("IntValue"), FIntProperty::StaticClass(), offsetof(UReflectionTestObject, IntValue), 4},
		{TEXT("Int64Value"), FInt64Property::StaticClass(), offsetof(UReflectionTestObject, Int64Value), 8},
		{TEXT("ByteValue"), FByteProperty::StaticClass(), offsetof(UReflectionTestObject, ByteValue), 1},
		{TEXT("UInt16Value"), FUInt16Property::StaticClass(), offsetof(UReflectionTestObject, UInt16Value), 2},
		{TEXT("UInt32Value"), FUInt32Property::StaticClass(), offsetof(UReflectionTestObject, UInt32Value), 4},
		{TEXT("UInt64Value"), FUInt64Property::StaticClass(), offsetof(UReflectionTestObject, UInt64Value), 8},
		{TEXT("FloatValue"), FFloatProperty::StaticClass(), offsetof(UReflectionTestObject, FloatValue), 4},
		{TEXT("DoubleValue"), FDoubleProperty::StaticClass(), offsetof(UReflectionTestObject, DoubleValue), 8},
		{TEXT("bNativeBool"), FBoolProperty::StaticClass(), offsetof(UReflectionTestObject, bNativeBool), 1},
		{TEXT("StringValue"), FStrProperty::StaticClass(), offsetof(UReflectionTestObject, StringValue),
			int32(sizeof(FString))},
		{TEXT("NameValue"), FNameProperty::StaticClass(), offsetof(UReflectionTestObject, NameValue),
			int32(sizeof(FName))},
		{TEXT("TextValue"), FTextProperty::StaticClass(), offsetof(UReflectionTestObject, TextValue),
			int32(sizeof(FText))},
		{TEXT("Mode"), FEnumProperty::StaticClass(), offsetof(UReflectionTestObject, Mode), 1},
		{TEXT("Legacy"), FByteProperty::StaticClass(), offsetof(UReflectionTestObject, Legacy), 1},
		{TEXT("StructValue"), FStructProperty::StaticClass(), offsetof(UReflectionTestObject, StructValue),
			int32(sizeof(FReflectionTestStruct))},
		{TEXT("Location"), FStructProperty::StaticClass(), offsetof(UReflectionTestObject, Location),
			int32(sizeof(FVector))},
		{TEXT("Transform"), FStructProperty::StaticClass(), offsetof(UReflectionTestObject, Transform),
			int32(sizeof(FTransform))},
		{TEXT("ObjectRef"), FObjectProperty::StaticClass(), offsetof(UReflectionTestObject, ObjectRef),
			int32(sizeof(UObject*))},
		{TEXT("ClassRef"), FClassProperty::StaticClass(), offsetof(UReflectionTestObject, ClassRef),
			int32(sizeof(UClass*))},
		{TEXT("WeakRef"), FWeakObjectProperty::StaticClass(), offsetof(UReflectionTestObject, WeakRef),
			int32(sizeof(FWeakObjectPtr))},
		{TEXT("SoftRef"), FSoftObjectProperty::StaticClass(), offsetof(UReflectionTestObject, SoftRef),
			int32(sizeof(FSoftObjectPtr))},
		{TEXT("IntArray"), FArrayProperty::StaticClass(), offsetof(UReflectionTestObject, IntArray),
			int32(sizeof(TArray<int32>))},
		{TEXT("NameSet"), FSetProperty::StaticClass(), offsetof(UReflectionTestObject, NameSet),
			int32(sizeof(TSet<FName>))},
		{TEXT("NameToInt"), FMapProperty::StaticClass(), offsetof(UReflectionTestObject, NameToInt),
			int32(sizeof(TMap<FName, int32>))},
		{TEXT("FixedArray"), FFloatProperty::StaticClass(), offsetof(UReflectionTestObject, FixedArray), 4},
		{TEXT("AfterEditorOnly"), FIntProperty::StaticClass(), offsetof(UReflectionTestObject, AfterEditorOnly), 4},
	};
	for (const FExpectedProperty& Entry : Expected)
	{
		FProperty* Property = FindTestProperty(Entry.Name);
		if (!TestNotNull(Entry.Name, Property))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s class"), Entry.Name), Property->GetClass() == Entry.Class);
		TestEqual(
			*FString::Printf(TEXT("%s offset"), Entry.Name), Property->GetOffset_ForInternal(), int32(Entry.Offset));
		TestEqual(*FString::Printf(TEXT("%s element size"), Entry.Name), Property->ElementSize, Entry.ElementSize);
		TestTrue(*FString::Printf(TEXT("%s owner"), Entry.Name),
			Property->GetOwnerClass() == UReflectionTestObject::StaticClass());
	}

	// Type details.
	FEnumProperty* Mode = CastField<FEnumProperty>(FindTestProperty(TEXT("Mode")));
	TestTrue(TEXT("Enum property"),
		Mode && Mode->GetEnum() == StaticEnum<EReflectionTestMode>() &&
			CastField<FByteProperty>(Mode->GetUnderlyingProperty()) != nullptr);
	FByteProperty* Legacy = CastField<FByteProperty>(FindTestProperty(TEXT("Legacy")));
	TestTrue(TEXT("TEnumAsByte"), Legacy && Legacy->Enum == StaticEnum<EReflectionTestLegacy>());
	TestTrue(TEXT("Plain byte"), CastField<FByteProperty>(FindTestProperty(TEXT("ByteValue")))->Enum == nullptr);
	FClassProperty* ClassRef = CastField<FClassProperty>(FindTestProperty(TEXT("ClassRef")));
	TestTrue(TEXT("Class property"),
		ClassRef && ClassRef->MetaClass == UObject::StaticClass() && ClassRef->PropertyClass == UClass::StaticClass() &&
			ClassRef->HasAnyPropertyFlags(CPF_UObjectWrapper) == false);
	FStructProperty* StructValue = CastField<FStructProperty>(FindTestProperty(TEXT("StructValue")));
	TestTrue(TEXT("Struct property"), StructValue && StructValue->Struct == FReflectionTestStruct::StaticStruct());
	FArrayProperty* IntArray = CastField<FArrayProperty>(FindTestProperty(TEXT("IntArray")));
	TestTrue(TEXT("Array inner"), IntArray && CastField<FIntProperty>(IntArray->Inner) != nullptr);
	FMapProperty* StringToStruct = CastField<FMapProperty>(FindTestProperty(TEXT("StringToStruct")));
	TestTrue(TEXT("Map key and value"),
		StringToStruct && CastField<FStrProperty>(StringToStruct->KeyProp) &&
			CastField<FStructProperty>(StringToStruct->ValueProp));
	FSetProperty* NameSet = CastField<FSetProperty>(FindTestProperty(TEXT("NameSet")));
	TestTrue(TEXT("Set element"), NameSet && CastField<FNameProperty>(NameSet->ElementProp) != nullptr);
	TestEqual(TEXT("C++ type"), IntArray->GetCPPType(), TEXT("TArray<int32>"));
	TestEqual(TEXT("C++ map type"), StringToStruct->GetCPPType(), TEXT("TMap<FString, FReflectionTestInner>"));

	// Casts between field classes.
	FProperty* IntValue = FindTestProperty(TEXT("IntValue"));
	TestTrue(TEXT("CastField to a base"), CastField<FNumericProperty>(IntValue) != nullptr);
	TestNull(TEXT("CastField to another type"), CastField<FFloatProperty>(IntValue));
	TestTrue(TEXT("Field class hierarchy"),
		FIntProperty::StaticClass()->IsChildOf(FNumericProperty::StaticClass()) &&
			FIntProperty::StaticClass()->IsChildOf(FProperty::StaticClass()) &&
			!FIntProperty::StaticClass()->IsChildOf(FFloatProperty::StaticClass()));
	TestEqual(TEXT("Field class name"), FIntProperty::StaticClass()->GetName(), TEXT("IntProperty"));

	// Flags from the specifiers and the computed ones.
	TestTrue(TEXT("Config"), FindTestProperty(TEXT("ConfigValue"))->HasAnyPropertyFlags(CPF_Config));
	TestTrue(TEXT("Transient"), FindTestProperty(TEXT("TransientValue"))->HasAnyPropertyFlags(CPF_Transient));
	TestTrue(TEXT("POD int"),
		IntValue->HasAllPropertyFlags(
			CPF_IsPlainOldData | CPF_ZeroConstructor | CPF_NoDestructor | CPF_HasGetValueTypeHash));
	TestFalse(TEXT("String needs a destructor"),
		FindTestProperty(TEXT("StringValue"))->HasAnyPropertyFlags(CPF_NoDestructor));
	TestTrue(TEXT("Object references"),
		FindTestProperty(TEXT("ObjectRef"))->ContainsObjectReference() &&
			FindTestProperty(TEXT("StructArray"))->ContainsObjectReference() == false);

	// Fixed arrays.
	FProperty* FixedArray = FindTestProperty(TEXT("FixedArray"));
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	TestEqual(TEXT("Fixed array dimension"), FixedArray->ArrayDim, 3);
	TestEqual(TEXT("Fixed array element"), *FixedArray->ContainerPtrToValuePtr<float>(Object, 2), 4.0f);
	TestEqual(TEXT("Value through the property"),
		CastField<FIntProperty>(IntValue)->GetPropertyValue_InContainer(Object), 42);
	CastField<FIntProperty>(IntValue)->SetPropertyValue_InContainer(Object, 43);
	TestEqual(TEXT("Set through the property"), Object->IntValue, 43);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropertyBoolTest, "System.CoreUObject.Property.BoolBitfields",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPropertyBoolTest::RunTest(const FString& Parameters)
{
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	FBoolProperty* NativeBool = CastField<FBoolProperty>(FindTestProperty(TEXT("bNativeBool")));
	FBoolProperty* FlagA = CastField<FBoolProperty>(FindTestProperty(TEXT("bFlagA")));
	FBoolProperty* FlagB = CastField<FBoolProperty>(FindTestProperty(TEXT("bFlagB")));
	FBoolProperty* FlagC = CastField<FBoolProperty>(FindTestProperty(TEXT("bFlagC")));
	if (!TestTrue(TEXT("Bool properties"), NativeBool && FlagA && FlagB && FlagC))
	{
		return false;
	}
	TestTrue(TEXT("Native bool"), NativeBool->IsNativeBool() && NativeBool->GetFieldMask() == 0xff);
	TestFalse(TEXT("Bitfield"), FlagA->IsNativeBool());
	TestTrue(TEXT("Different bits of one byte"),
		FlagA->GetOffset_ForInternal() == FlagB->GetOffset_ForInternal() &&
			FlagA->GetFieldMask() != FlagB->GetFieldMask());
	TestEqual(TEXT("uint32 bitfield storage"), int32(FlagC->GetFieldSize()), 4);
	TestFalse(TEXT("Bitfields are not POD"), FlagA->HasAnyPropertyFlags(CPF_IsPlainOldData));

	TestTrue(TEXT("Read bits"),
		FlagA->GetPropertyValue_InContainer(Object) && !FlagB->GetPropertyValue_InContainer(Object) &&
			FlagC->GetPropertyValue_InContainer(Object) && NativeBool->GetPropertyValue_InContainer(Object));
	FlagB->SetPropertyValue_InContainer(Object, true);
	FlagA->SetPropertyValue_InContainer(Object, false);
	TestTrue(TEXT("Write bits"), Object->bFlagA == 0 && Object->bFlagB == 1 && Object->bFlagC == 1);
	FlagC->SetPropertyValue_InContainer(Object, false);
	TestTrue(TEXT("uint32 bitfield"), Object->bFlagC == 0 && Object->bFlagB == 1);
	NativeBool->SetPropertyValue_InContainer(Object, false);
	TestFalse(TEXT("Native write"), Object->bNativeBool);

	UReflectionTestObject* Copy = NewObject<UReflectionTestObject>();
	FlagB->CopyCompleteValue_InContainer(Copy, Object);
	TestTrue(TEXT("Copy one bit"), Copy->bFlagB == 1 && Copy->bFlagA == 1);
	TestTrue(
		TEXT("Identical"), FlagB->Identical_InContainer(Copy, Object) && !FlagA->Identical_InContainer(Copy, Object));
	FlagA->ClearValue_InContainer(Copy);
	TestTrue(TEXT("Clear one bit"), Copy->bFlagA == 0 && Copy->bFlagB == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropertyTextTest, "System.CoreUObject.Property.Text",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPropertyTextTest::RunTest(const FString& Parameters)
{
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	TestEqual(TEXT("Export int"), ExportProperty(Object, TEXT("IntValue")), TEXT("42"));
	TestEqual(TEXT("Export int64"), ExportProperty(Object, TEXT("Int64Value")), TEXT("1099511627776"));
	TestEqual(TEXT("Export uint64"), ExportProperty(Object, TEXT("UInt64Value")), TEXT("9223372036854775808"));
	TestEqual(TEXT("Export string"), ExportProperty(Object, TEXT("StringValue")), TEXT("\"Hello\""));
	TestEqual(TEXT("Export name"), ExportProperty(Object, TEXT("NameValue")), TEXT("Leon"));
	TestEqual(TEXT("Export bool"), ExportProperty(Object, TEXT("bFlagA")), TEXT("True"));
	TestEqual(TEXT("Export enum"), ExportProperty(Object, TEXT("Mode")), TEXT("Second"));
	TestEqual(TEXT("Export TEnumAsByte"), ExportProperty(Object, TEXT("Legacy")), TEXT("RTL_Beta"));
	TestEqual(TEXT("Export null object"), ExportProperty(Object, TEXT("ObjectRef")), TEXT("None"));
	TestEqual(TEXT("Export array"), ExportProperty(Object, TEXT("IntArray")), TEXT("(1,2,3)"));

	TestTrue(TEXT("Import int"), ImportProperty(Object, TEXT("IntValue"), TEXT("-7")) && Object->IntValue == -7);
	TestTrue(
		TEXT("Import hex"), ImportProperty(Object, TEXT("UInt32Value"), TEXT("0xFF")) && Object->UInt32Value == 255u);
	TestTrue(TEXT("Import uint64"),
		ImportProperty(Object, TEXT("UInt64Value"), TEXT("18446744073709551615")) &&
			Object->UInt64Value == 18446744073709551615ull);
	TestTrue(
		TEXT("Import float"), ImportProperty(Object, TEXT("FloatValue"), TEXT("0.75")) && Object->FloatValue == 0.75f);
	TestTrue(TEXT("Import string"),
		ImportProperty(Object, TEXT("StringValue"), TEXT("\"Quoted \\\"text\\\"\"")) &&
			Object->StringValue == TEXT("Quoted \"text\""));
	TestTrue(TEXT("Import name"),
		ImportProperty(Object, TEXT("NameValue"), TEXT("Other")) && Object->NameValue == FName(TEXT("Other")));
	TestTrue(TEXT("Import text"),
		ImportProperty(Object, TEXT("TextValue"), TEXT("\"Shown\"")) && Object->TextValue.ToString() == TEXT("Shown"));
	TestTrue(TEXT("Import bool"), ImportProperty(Object, TEXT("bFlagB"), TEXT("true")) && Object->bFlagB == 1);
	TestTrue(TEXT("Import enum"),
		ImportProperty(Object, TEXT("Mode"), TEXT("Third")) && Object->Mode == EReflectionTestMode::Third);
	TestTrue(TEXT("Import TEnumAsByte"),
		ImportProperty(Object, TEXT("Legacy"), TEXT("RTL_Gamma")) && Object->Legacy == RTL_Gamma);
	TestTrue(TEXT("Import object path"),
		ImportProperty(Object, TEXT("ObjectRef"), *Object->GetPathName()) && Object->ObjectRef == Object);
	TestEqual(TEXT("Export object"), ExportProperty(Object, TEXT("ObjectRef")),
		FString(TEXT("ReflectionTestObject'")) + Object->GetPathName() + TEXT("'"));
	TestTrue(TEXT("Import class"),
		ImportProperty(Object, TEXT("ClassRef"), TEXT("/Script/CoreUObject.Package")) &&
			Object->ClassRef.Get() == UPackage::StaticClass());

	TestTrue(TEXT("Import struct"),
		ImportProperty(Object, TEXT("StructValue"), TEXT("(Id=9, Label=\"Nine\", Weight=0.5)")) &&
			Object->StructValue.Id == 9 && Object->StructValue.Label == TEXT("Nine") &&
			Object->StructValue.Weight == 0.5f);
	const FString StructText = ExportProperty(Object, TEXT("StructValue"));
	UReflectionTestObject* Other = NewObject<UReflectionTestObject>();
	TestTrue(TEXT("Struct round trip"),
		ImportProperty(Other, TEXT("StructValue"), *StructText) && Other->StructValue.Id == 9 &&
			Other->StructValue.Weight == 0.5f);

	// Rejected input (reported as warnings).
	TestFalse(TEXT("Bad number"), ImportProperty(Object, TEXT("IntValue"), TEXT("abc")));
	TestFalse(TEXT("Bad enum name"), ImportProperty(Object, TEXT("Mode"), TEXT("Fourth")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropertyEditorOnlyTest, "System.CoreUObject.Property.EditorOnlyData",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPropertyEditorOnlyTest::RunTest(const FString& Parameters)
{
	FProperty* EditorNote = FindTestProperty(TEXT("EditorNote"));
	FProperty* After = FindTestProperty(TEXT("AfterEditorOnly"));
	TestNotNull(TEXT("Properties after the editor-only block"), After);
	#if WITH_EDITORONLY_DATA
	if (!TestNotNull(TEXT("Editor-only property"), EditorNote))
	{
		return false;
	}
	TestTrue(TEXT("CPF_EditorOnly"),
		EditorNote->HasAnyPropertyFlags(CPF_EditorOnly) &&
			FindTestProperty(TEXT("EditorCount"))->HasAnyPropertyFlags(CPF_EditorOnly));
	TestFalse(TEXT("Other properties are not editor-only"), After->HasAnyPropertyFlags(CPF_EditorOnly));
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	TestEqual(TEXT("Editor-only default"), ExportProperty(Object, TEXT("EditorNote")), TEXT("\"Editor\""));
	#else
	TestNull(TEXT("Stripped without WITH_EDITORONLY_DATA"), EditorNote);
	#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropertyNoExportTest, "System.CoreUObject.Property.NoExportStruct",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPropertyNoExportTest::RunTest(const FString& Parameters)
{
	UScriptStruct* VectorStruct = StaticStruct<FVector>();
	if (!TestNotNull(TEXT("FVector is reflected"), VectorStruct))
	{
		return false;
	}
	TestTrue(TEXT("TBaseStructure"), TBaseStructure<FVector>::Get() == VectorStruct);
	TestTrue(TEXT("By path"), FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Vector")) == VectorStruct);
	TestEqual(TEXT("Name"), VectorStruct->GetName(), TEXT("Vector"));
	TestEqual(TEXT("C++ name"), VectorStruct->GetStructCPPName(), TEXT("FVector"));
	TestTrue(TEXT("NoExport flags"),
		(VectorStruct->StructFlags & STRUCT_NoExport) && (VectorStruct->StructFlags & STRUCT_Immutable) &&
			(VectorStruct->StructFlags & STRUCT_Native));
	TestEqual(TEXT("Size"), VectorStruct->GetStructureSize(), int32(sizeof(FVector)));
	TestEqual(TEXT("X offset"), VectorStruct->FindPropertyByName(TEXT("X"))->GetOffset_ForInternal(),
		int32(offsetof(FVector, X)));
	TestEqual(TEXT("Z offset"), VectorStruct->FindPropertyByName(TEXT("Z"))->GetOffset_ForInternal(),
		int32(offsetof(FVector, Z)));

	UScriptStruct* PlaneStruct = StaticStruct<FPlane>();
	TestTrue(TEXT("FPlane derives from FVector"),
		PlaneStruct->GetSuperStruct() == VectorStruct && PlaneStruct->FindPropertyByName(TEXT("X")) != nullptr);
	UScriptStruct* TransformStruct = StaticStruct<FTransform>();
	FStructProperty* Rotation = CastField<FStructProperty>(TransformStruct->FindPropertyByName(TEXT("Rotation")));
	TestTrue(TEXT("FTransform members"),
		Rotation && Rotation->Struct == StaticStruct<FQuat>() &&
			TransformStruct->GetStructureSize() == int32(sizeof(FTransform)));
	UScriptStruct* const Others[] = {StaticStruct<FVector2D>(), StaticStruct<FVector4>(), StaticStruct<FRotator>(),
		StaticStruct<FColor>(), StaticStruct<FLinearColor>(), StaticStruct<FGuid>(), StaticStruct<FIntPoint>(),
		StaticStruct<FIntVector>(), StaticStruct<FBox>()};
	for (UScriptStruct* Struct : Others)
	{
		TestTrue(TEXT("Core struct reflected"), Struct && Struct->GetStructureSize() > 0 && Struct->PropertyLink);
	}

	// A UPROPERTY() FVector of a class uses the Core struct's reflection.
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	FStructProperty* Location = CastField<FStructProperty>(FindTestProperty(TEXT("Location")));
	TestTrue(TEXT("FVector property"), Location && Location->Struct == VectorStruct);
	TestTrue(TEXT("POD struct"), Location->HasAnyPropertyFlags(CPF_IsPlainOldData));
	FVector* Value = Location->ContainerPtrToValuePtr<FVector>(Object);
	TestTrue(TEXT("Value"), Value->X == 1.0f && Value->Y == 2.0f && Value->Z == 3.0f);
	TestTrue(TEXT("Import FVector"),
		ImportProperty(Object, TEXT("Location"), TEXT("(X=10,Y=20,Z=30)")) &&
			Object->Location == FVector(10.0f, 20.0f, 30.0f));
	UReflectionTestObject* Other = NewObject<UReflectionTestObject>();
	TestFalse(TEXT("Compare FVector"), Location->Identical_InContainer(Object, Other));
	Location->CopyCompleteValue_InContainer(Other, Object);
	TestTrue(
		TEXT("Copy FVector"), Other->Location == Object->Location && Location->Identical_InContainer(Object, Other));

	FStructProperty* TransformProperty = CastField<FStructProperty>(FindTestProperty(TEXT("Transform")));
	FTransform Moved(FVector(5.0f, 6.0f, 7.0f));
	TransformProperty->CopyCompleteValue(TransformProperty->ContainerPtrToValuePtr<void>(Object), &Moved);
	TestTrue(TEXT("Copy FTransform"), Object->Transform.GetTranslation() == FVector(5.0f, 6.0f, 7.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
