#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "Tests/HierarchyTestTypes.h"
#include "Tests/OrderTestChild.h"
#include "Tests/ReflectionTestTypes.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassHierarchyTest, "System.CoreUObject.Class.Hierarchy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FClassHierarchyTest::RunTest(const FString& Parameters)
{
	UClass* Base = UHierarchyTestBase::StaticClass();
	UClass* Child = UHierarchyTestChild::StaticClass();
	UClass* GrandChild = UHierarchyTestGrandChild::StaticClass();

	TestTrue(TEXT("Super classes"),
		GrandChild->GetSuperClass() == Child && Child->GetSuperClass() == Base &&
			Base->GetSuperClass() == UObject::StaticClass() && UObject::StaticClass()->GetSuperClass() == nullptr);
	TestTrue(TEXT("IsChildOf"),
		GrandChild->IsChildOf(Base) && GrandChild->IsChildOf(GrandChild) && GrandChild->IsChildOf<UObject>() &&
			!Base->IsChildOf(Child));
	TestEqual(TEXT("Class name"), Child->GetName(), TEXT("HierarchyTestChild"));
	TestTrue(TEXT("Class package"), Child->GetOuter() == FindObject<UPackage>(nullptr, TEXT("/Script/CoreUObject")));
	TestTrue(TEXT("Abstract flag"), Base->HasAnyClassFlags(CLASS_Abstract) && !Child->HasAnyClassFlags(CLASS_Abstract));
	TestTrue(TEXT("Generated classes are native, constructed and not intrinsic"),
		Child->HasAllClassFlags(CLASS_Native | CLASS_Constructed) && !Child->HasAnyClassFlags(CLASS_Intrinsic));
	TestTrue(TEXT("Class of a class"), Child->GetClass() == UClass::StaticClass());
	TestTrue(TEXT("Class cast flags"),
		UClass::StaticClass()->HasAnyCastFlag(CASTCLASS_UClass) &&
			UClass::StaticClass()->HasAnyCastFlag(CASTCLASS_UStruct) &&
			UFunction::StaticClass()->HasAnyCastFlag(CASTCLASS_UStruct) && !Child->HasAnyCastFlag(CASTCLASS_UStruct));

	UHierarchyTestGrandChild* Object = NewObject<UHierarchyTestGrandChild>();
	TestTrue(TEXT("IsA"),
		Object->IsA(Base) && Object->IsA<UHierarchyTestChild>() && Object->IsA<UObject>() && !Object->IsA<UPackage>());
	TestEqual(TEXT("Virtual call through the base"), ((UHierarchyTestBase*)Object)->GetKind(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassCastTest, "System.CoreUObject.Class.Cast",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FClassCastTest::RunTest(const FString& Parameters)
{
	UHierarchyTestChild* Child = NewObject<UHierarchyTestChild>();
	UObject* AsObject = Child;
	const UObject* AsConstObject = Child;

	TestTrue(TEXT("Cast to its class"), Cast<UHierarchyTestChild>(AsObject) == Child);
	TestTrue(TEXT("Cast to a base"), Cast<UHierarchyTestBase>(AsObject) == Child);
	TestNull(TEXT("Cast to a derived class"), Cast<UHierarchyTestGrandChild>(AsObject));
	TestNull(TEXT("Cast to an unrelated class"), Cast<UPackage>(AsObject));
	TestNull(TEXT("Cast of nullptr"), Cast<UHierarchyTestChild>((UObject*)nullptr));
	TestTrue(TEXT("Cast keeps const"), Cast<UHierarchyTestChild>(AsConstObject) == Child);
	TestTrue(TEXT("CastChecked"), CastChecked<UHierarchyTestBase>(AsObject) == Child);
	TestNull(TEXT("CastChecked allowing null"),
		CastChecked<UHierarchyTestChild>((UObject*)nullptr, ECastCheckedType::NullAllowed));
	TestTrue(TEXT("ExactCast"), ExactCast<UHierarchyTestChild>(AsObject) == Child);
	TestNull(TEXT("ExactCast to a base"), ExactCast<UHierarchyTestBase>(AsObject));

	// Classes with a cast bit take the fast path.
	UObject* ClassObject = UHierarchyTestChild::StaticClass();
	TestTrue(TEXT("Cast to UClass"), Cast<UClass>(ClassObject) == ClassObject);
	TestTrue(TEXT("Cast to UStruct"), Cast<UStruct>(ClassObject) == ClassObject);
	TestNull(TEXT("Cast to UFunction"), Cast<UFunction>(ClassObject));
	TestNull(TEXT("Cast of an object to UClass"), Cast<UClass>(AsObject));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassSubclassOfTest, "System.CoreUObject.Class.SubclassOf",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FClassSubclassOfTest::RunTest(const FString& Parameters)
{
	TSubclassOf<UHierarchyTestBase> Subclass = UHierarchyTestGrandChild::StaticClass();
	TestTrue(TEXT("Holds a subclass"), Subclass.Get() == UHierarchyTestGrandChild::StaticClass());
	TestTrue(TEXT("Default object"), Subclass.GetDefaultObject() == GetDefault<UHierarchyTestGrandChild>());
	TestEqual(TEXT("Default object is typed"), Subclass.GetDefaultObject()->GetKind(), 2);

	TSubclassOf<UHierarchyTestChild> Narrow = UHierarchyTestGrandChild::StaticClass();
	TSubclassOf<UHierarchyTestBase> Widened = Narrow;
	TestTrue(TEXT("Converts to a wider TSubclassOf"), Widened.Get() == UHierarchyTestGrandChild::StaticClass());

	TSubclassOf<UHierarchyTestChild> Wrong = UPackage::StaticClass();
	TestNull(TEXT("Reads as null when not a subclass"), Wrong.Get());
	TSubclassOf<UHierarchyTestChild> Empty;
	TestNull(TEXT("Empty"), Empty.Get());
	TestTrue(TEXT("Same size as UClass*"), sizeof(TSubclassOf<UObject>) == sizeof(UClass*));

	UObject* Instance = NewObject<UObject>(GetTransientPackage(), *Subclass, NAME_None);
	TestTrue(TEXT("NewObject from a TSubclassOf"), Instance->GetClass() == UHierarchyTestGrandChild::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassDefaultObjectTest, "System.CoreUObject.Class.DefaultObject",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FClassDefaultObjectTest::RunTest(const FString& Parameters)
{
	UClass* Class = UReflectionTestObject::StaticClass();
	const UReflectionTestObject* Defaults = GetDefault<UReflectionTestObject>();
	if (!TestNotNull(TEXT("Default object"), Defaults))
	{
		return false;
	}
	TestTrue(TEXT("CDO flags"), Defaults->HasAllFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Public));
	TestEqual(TEXT("CDO name"), Defaults->GetName(), TEXT("Default__ReflectionTestObject"));
	TestTrue(TEXT("CDO outer"), Defaults->GetOuter() == Class->GetOuter());
	TestTrue(TEXT("Class default object"), Class->GetDefaultObject(false) == Defaults);
	TestEqual(TEXT("CDO member default"), Defaults->IntValue, 42);
	TestEqual(TEXT("CDO constructor value"), Defaults->FixedArray[2], 4.0f);
	TestTrue(TEXT("CDO is a template"), Defaults->IsTemplate());

	UReflectionTestObject* Instance = NewObject<UReflectionTestObject>();
	TestFalse(TEXT("Instance is not a template"), Instance->IsTemplate());
	TestTrue(TEXT("Archetype of an instance"), Instance->GetArchetype() == Defaults);
	TestEqual(TEXT("Member default reaches the instance"), Instance->IntValue, 42);
	TestEqual(TEXT("Constructor value reaches the instance"), Instance->StructValue.Id, 5);
	TestTrue(TEXT("Constructor bitfield reaches the instance"), Instance->bFlagA == 1 && Instance->bFlagB == 0);

	// Config properties are copied from the class defaults after the constructor (UE: PostConstructLink).
	UReflectionTestObject* MutableDefaults = GetMutableDefault<UReflectionTestObject>();
	const int32 OldConfigValue = MutableDefaults->ConfigValue;
	const int32 OldIntValue = MutableDefaults->IntValue;
	MutableDefaults->ConfigValue = 1234;
	MutableDefaults->IntValue = 4321;
	UReflectionTestObject* AfterChange = NewObject<UReflectionTestObject>();
	TestEqual(TEXT("Config property comes from the defaults"), AfterChange->ConfigValue, 1234);
	TestEqual(TEXT("Other properties come from the constructor"), AfterChange->IntValue, 42);
	MutableDefaults->ConfigValue = OldConfigValue;
	MutableDefaults->IntValue = OldIntValue;

	// A template overrides the defaults: every property is copied from it.
	UReflectionTestObject* Template = NewObject<UReflectionTestObject>();
	Template->IntValue = 77;
	Template->StringValue = TEXT("FromTemplate");
	Template->IntArray = {9, 8};
	UReflectionTestObject* FromTemplate =
		NewObject<UReflectionTestObject>(GetTransientPackage(), NAME_None, RF_NoFlags, Template);
	TestEqual(TEXT("Template int"), FromTemplate->IntValue, 77);
	TestEqual(TEXT("Template string"), FromTemplate->StringValue, TEXT("FromTemplate"));
	TestEqual(TEXT("Template array"), FromTemplate->IntArray.Num(), 2);

	// The abstract base has a default object, and the defaults of the super class come first.
	const UHierarchyTestBase* BaseDefaults = GetDefault<UHierarchyTestBase>();
	TestTrue(TEXT("Abstract class default object"), BaseDefaults != nullptr && BaseDefaults->BaseValue == 100);
	TestTrue(TEXT("Intrinsic class default objects"),
		UClass::StaticClass()->GetDefaultObject(false) != nullptr &&
			UObject::StaticClass()->GetDefaultObject(false) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassDefaultSubobjectTest, "System.CoreUObject.Class.DefaultSubobjects",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FClassDefaultSubobjectTest::RunTest(const FString& Parameters)
{
	UHierarchyTestChild* Child = NewObject<UHierarchyTestChild>();
	if (!TestNotNull(TEXT("Subobject created"), Child->Component))
	{
		return false;
	}
	TestTrue(TEXT("Subobject outer"), Child->Component->GetOuter() == Child);
	TestEqual(TEXT("Subobject name"), Child->Component->GetName(), TEXT("Component"));
	TestTrue(TEXT("Subobject flag"), Child->Component->IsDefaultSubobject());
	TestTrue(TEXT("Subobject class"), Child->Component->GetClass() == UHierarchyTestComponent::StaticClass());
	TestEqual(TEXT("Subobject defaults"), Child->Component->Power, 5);
	TestEqual(TEXT("Base constructor ran"), Child->BaseValue, 100);
	TestEqual(TEXT("Child constructor ran"), Child->ChildValue, 200);

	// Each instance builds its own subobject (D12); the default object has its own too.
	UHierarchyTestChild* Other = NewObject<UHierarchyTestChild>();
	const UHierarchyTestChild* Defaults = GetDefault<UHierarchyTestChild>();
	TestTrue(TEXT("Per-instance subobjects"), Other->Component != Child->Component);
	TestTrue(TEXT("Default object subobject"), Defaults->Component && Defaults->Component != Child->Component);
	TestTrue(TEXT("Default object subobject is an archetype"),
		Defaults->Component->HasAllFlags(RF_ArchetypeObject | RF_DefaultSubObject));

	TArray<UObject*> Subobjects;
	Child->GetDefaultSubobjects(Subobjects);
	TestTrue(TEXT("GetDefaultSubobjects"), Subobjects.Num() == 1 && Subobjects[0] == Child->Component);

	// A template's subobject reference becomes the new object's own subobject.
	UHierarchyTestChild* FromTemplate =
		NewObject<UHierarchyTestChild>(GetTransientPackage(), NAME_None, RF_NoFlags, Child);
	TestTrue(TEXT("Template subobject remapped"),
		FromTemplate->Component && FromTemplate->Component->GetOuter() == FromTemplate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassSubobjectOverrideTest, "System.CoreUObject.Class.SubobjectOverrides",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FClassSubobjectOverrideTest::RunTest(const FString& Parameters)
{
	UHierarchyTestGrandChild* GrandChild = NewObject<UHierarchyTestGrandChild>();
	TestTrue(TEXT("SetDefaultSubobjectClass"),
		GrandChild->Component && GrandChild->Component->GetClass() == UHierarchyTestSpecialComponent::StaticClass());
	TestEqual(TEXT("Overridden subobject defaults"), GrandChild->Component->Power, 50);
	TestEqual(TEXT("Overridden subobject virtual"), GrandChild->Component->GetKind(), 11);
	TestEqual(TEXT("Grand child constructor ran"), GrandChild->GrandChildValue, 300.0f);

	UHierarchyTestOptionalOwner* Owner = NewObject<UHierarchyTestOptionalOwner>();
	TestNotNull(TEXT("Optional subobject"), Owner->Optional);
	UHierarchyTestNoOptional* NoOptional = NewObject<UHierarchyTestNoOptional>();
	TestNull(TEXT("DoNotCreateDefaultSubobject"), NoOptional->Optional);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassRegistrationOrderTest, "System.CoreUObject.Class.RegistrationOrder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FClassRegistrationOrderTest::RunTest(const FString& Parameters)
{
	// OrderTestChild.h sorts before OrderTestParent.h, so the child class is recorded (and its reflection constructed)
	// first; its DependentSingletons build the parent, and the parent's default object is created first.
	UClass* Parent = UOrderTestParent::StaticClass();
	UClass* Child = UOrderTestChild::StaticClass();
	TestTrue(TEXT("Both classes constructed"),
		Parent->HasAnyClassFlags(CLASS_Constructed) && Child->HasAnyClassFlags(CLASS_Constructed));
	TestTrue(TEXT("Super class"), Child->GetSuperClass() == Parent);
	TestTrue(TEXT("Inherited property"), Child->FindPropertyByName(TEXT("ParentValue")) != nullptr);
	TestTrue(TEXT("Child property after the parent's"), Child->PropertiesSize >= Parent->PropertiesSize);

	const UOrderTestParent* ParentDefaults = GetDefault<UOrderTestParent>();
	const UOrderTestChild* ChildDefaults = GetDefault<UOrderTestChild>();
	TestTrue(TEXT("Default objects exist"), ParentDefaults && ChildDefaults);
	TestTrue(TEXT("Parent default object first"),
		ParentDefaults->DefaultObjectSequence > 0 &&
			ParentDefaults->DefaultObjectSequence < ChildDefaults->DefaultObjectSequence);
	TestEqual(TEXT("Child defaults"), ChildDefaults->ParentValue + ChildDefaults->ChildValue, 33);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassIntrinsicTest, "System.CoreUObject.Class.Intrinsic",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FClassIntrinsicTest::RunTest(const FString& Parameters)
{
	UPackage* CoreUObjectPackage = FindObject<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
	if (!TestNotNull(TEXT("/Script/CoreUObject"), CoreUObjectPackage))
	{
		return false;
	}
	TestTrue(TEXT("Compiled-in package"), CoreUObjectPackage->HasAnyPackageFlags(PKG_CompiledIn));
	UClass* const Intrinsics[] = {UObject::StaticClass(), UField::StaticClass(), UStruct::StaticClass(),
		UClass::StaticClass(), UScriptStruct::StaticClass(), UEnum::StaticClass(), UFunction::StaticClass(),
		UPackage::StaticClass()};
	for (UClass* Class : Intrinsics)
	{
		TestTrue(*FString::Printf(TEXT("%s is intrinsic and registered"), *Class->GetName()),
			Class->HasAllClassFlags(CLASS_Intrinsic | CLASS_Constructed) && Class->GetOuter() == CoreUObjectPackage &&
				Class->GetClass() == UClass::StaticClass());
	}
	TestTrue(TEXT("UObject is abstract"), UObject::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestTrue(TEXT("UStruct derives from UField"), UStruct::StaticClass()->GetSuperClass() == UField::StaticClass());
	TestTrue(TEXT("UClass by path"),
		FindObject<UClass>(nullptr, TEXT("/Script/CoreUObject.Class")) == UClass::StaticClass());
	TestTrue(
		TEXT("Transient package"), FindObject<UPackage>(nullptr, TEXT("/Engine/Transient")) == GetTransientPackage());

	const FUObjectReflectionStats Stats = GetUObjectReflectionStats();
	TestTrue(TEXT("Reflection stats"),
		Stats.NumClasses >= 8 && Stats.NumStructs > 0 && Stats.NumEnums > 0 && Stats.NumFunctions > 0 &&
			Stats.NumProperties > 0 && Stats.NumPackages > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
