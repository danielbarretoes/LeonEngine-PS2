#include "CoreMinimal.h"
#include "HAL/PlatformProperties.h"
#include "Misc/AutomationTest.h"
#include "Tests/HierarchyTestTypes.h"
#include "Tests/ReflectionTestTypes.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectNewObjectNamesTest, "System.CoreUObject.Object.NewObjectAndNames",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FObjectNewObjectNamesTest::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Temp/ObjectTest_Names"));
	UReflectionTestObject* First = NewObject<UReflectionTestObject>(Package);
	UReflectionTestObject* Second = NewObject<UReflectionTestObject>(Package);
	if (!TestNotNull(TEXT("NewObject"), First) || !TestNotNull(TEXT("NewObject"), Second))
	{
		return false;
	}
	TestTrue(TEXT("Unique names"), First->GetFName() != Second->GetFName());
	TestTrue(TEXT("Name from the class name"), First->GetName().StartsWith(TEXT("ReflectionTestObject_")));
	TestTrue(TEXT("Class"), First->GetClass() == UReflectionTestObject::StaticClass());
	TestTrue(TEXT("Outer"), First->GetOuter() == Package);
	TestTrue(TEXT("Registered in GUObjectArray"), GUObjectArray.IsValid(First));
	TestFalse(TEXT("Initialization finished"), First->HasAnyFlags(RF_NeedInitialization));

	UReflectionTestObject* Named = NewObject<UReflectionTestObject>(Package, TEXT("Named"), RF_Public);
	TestEqual(TEXT("Explicit name"), Named->GetName(), TEXT("Named"));
	TestTrue(TEXT("Flags set"), Named->HasAnyFlags(RF_Public));

	const FName Unique = MakeUniqueObjectName(Package, UReflectionTestObject::StaticClass(), FName(TEXT("Named")));
	TestTrue(TEXT("MakeUniqueObjectName keeps the base"), Unique.GetPlainNameString() == TEXT("Named"));
	TestTrue(TEXT("MakeUniqueObjectName is free"), StaticFindObjectFast(nullptr, Package, Unique) == nullptr);

	UReflectionTestObject* InTransient = NewObject<UReflectionTestObject>();
	TestTrue(TEXT("Default outer is the transient package"), InTransient->GetOuter() == GetTransientPackage());
	TestTrue(TEXT("Transient package flag"), GetTransientPackage()->HasAnyFlags(RF_Transient));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPathNameTest, "System.CoreUObject.Object.OuterChainAndPathNames",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FObjectPathNameTest::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Game/PathTest"));
	UReflectionTestObject* Outer = NewObject<UReflectionTestObject>(Package, TEXT("Outer"));
	UReflectionTestObject* Inner = NewObject<UReflectionTestObject>(Outer, TEXT("Inner"));
	UReflectionTestObject* Innermost = NewObject<UReflectionTestObject>(Inner, TEXT("Innermost"));

	TestEqual(TEXT("Package path"), Package->GetPathName(), TEXT("/Game/PathTest"));
	TestEqual(TEXT("Object in a package"), Outer->GetPathName(), TEXT("/Game/PathTest.Outer"));
	TestEqual(TEXT("Subobject uses ':'"), Inner->GetPathName(), TEXT("/Game/PathTest.Outer:Inner"));
	TestEqual(TEXT("Deeper objects use '.'"), Innermost->GetPathName(), TEXT("/Game/PathTest.Outer:Inner.Innermost"));
	TestEqual(TEXT("Path relative to a stop outer"), Innermost->GetPathName(Outer), TEXT("Inner.Innermost"));
	TestEqual(TEXT("Full name"), Outer->GetFullName(), TEXT("ReflectionTestObject /Game/PathTest.Outer"));
	TestTrue(TEXT("Outermost"), Innermost->GetOutermost() == Package);
	TestTrue(TEXT("GetPackage"), Innermost->GetPackage() == Package);
	TestTrue(TEXT("IsIn"), Innermost->IsIn(Outer) && Innermost->IsIn(Package) && !Outer->IsIn(Inner));
	TestTrue(TEXT("GetTypedOuter"), Innermost->GetTypedOuter<UPackage>() == Package);
	TestTrue(TEXT("GetTypedOuter of a class"), Innermost->GetTypedOuter<UReflectionTestObject>() == Inner);
	TestEqual(TEXT("Class full name"), UObject::StaticClass()->GetFullName(), TEXT("Class /Script/CoreUObject.Object"));

	TArray<UObject*> Children;
	GetObjectsWithOuter(Outer, Children, false);
	TestEqual(TEXT("Direct inner objects"), Children.Num(), 1);
	Children.Reset();
	GetObjectsWithOuter(Outer, Children, true);
	TestEqual(TEXT("Nested inner objects"), Children.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectFindObjectTest, "System.CoreUObject.Object.FindObject",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FObjectFindObjectTest::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Game/FindTest"));
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>(Package, TEXT("Target"));
	UReflectionTestObject* Sub = NewObject<UReflectionTestObject>(Object, TEXT("Sub"));

	TestTrue(TEXT("By outer and name"), FindObject<UReflectionTestObject>(Package, TEXT("Target")) == Object);
	TestTrue(TEXT("By path"), FindObject<UReflectionTestObject>(nullptr, TEXT("/Game/FindTest.Target")) == Object);
	TestTrue(TEXT("Subobject by path"),
		FindObject<UReflectionTestObject>(nullptr, TEXT("/Game/FindTest.Target:Sub")) == Sub);
	TestTrue(TEXT("In any package"), FindObject<UReflectionTestObject>(ANY_PACKAGE, TEXT("Target")) == Object);
	TestTrue(TEXT("Class filter"), FindObject<UPackage>(Package, TEXT("Target")) == nullptr);
	TestTrue(TEXT("Missing name"), FindObject<UObject>(Package, TEXT("NoSuchObject")) == nullptr);
	TestTrue(TEXT("None"), StaticFindObject(nullptr, Package, TEXT("None")) == nullptr);
	TestTrue(TEXT("Package"), FindObject<UPackage>(nullptr, TEXT("/Game/FindTest")) == Package);
	TestTrue(TEXT("CreatePackage finds it"), CreatePackage(TEXT("/Game/FindTest")) == Package);
	TestTrue(TEXT("Script class by path"),
		FindObject<UClass>(nullptr, TEXT("/Script/CoreUObject.ReflectionTestObject")) ==
			UReflectionTestObject::StaticClass());
	TestTrue(TEXT("Case-insensitive names"), FindObject<UReflectionTestObject>(Package, TEXT("target")) == Object);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectArrayTest, "System.CoreUObject.Object.ObjectArray",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FObjectArrayTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Capacity is the platform constant"), GUObjectArray.GetObjectArrayCapacity(),
		FPlatformProperties::MaxObjectsInGame);
	TestTrue(TEXT("Capacity is a known platform value"),
		FPlatformProperties::MaxObjectsInGame == 8192 || FPlatformProperties::MaxObjectsInGame == 131072);
	TestEqual(TEXT("Slot size"), int32(sizeof(FUObjectItem)), int32(sizeof(void*) == 4 ? 12 : 16));
	TestTrue(TEXT("Bytes"),
		GUObjectArray.GetAllocatedSize() == SIZE_T(FPlatformProperties::MaxObjectsInGame) * sizeof(FUObjectItem));

	const int32 Before = GUObjectArray.GetObjectArrayNumMinusAvailable();
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>();
	TestEqual(TEXT("One more object"), GUObjectArray.GetObjectArrayNumMinusAvailable(), Before + 1);
	FUObjectItem* Item = GUObjectArray.ObjectToObjectItem(Object);
	TestTrue(TEXT("Item points at the object"), Item && Item->Object == Object);
	TestEqual(TEXT("Index"), GUObjectArray.ObjectToIndex(Object), int32(Object->GetUniqueID()));

	Object->AddToRoot();
	TestTrue(TEXT("Rooted"), Object->IsRooted());
	Object->RemoveFromRoot();
	TestFalse(TEXT("Not rooted"), Object->IsRooted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectIteratorTest, "System.CoreUObject.Object.Iterator",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FObjectIteratorTest::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Temp/IteratorTest"));
	UHierarchyTestChild* Child = NewObject<UHierarchyTestChild>(Package);
	UHierarchyTestGrandChild* GrandChild = NewObject<UHierarchyTestGrandChild>(Package);

	bool bFoundChild = false;
	bool bFoundGrandChild = false;
	bool bFoundDefaultObject = false;
	for (UHierarchyTestChild* Object : TObjectRange<UHierarchyTestChild>())
	{
		bFoundChild |= Object == Child;
		bFoundGrandChild |= Object == GrandChild;
		bFoundDefaultObject |= Object->HasAnyFlags(RF_ClassDefaultObject);
	}
	TestTrue(TEXT("Finds the class"), bFoundChild);
	TestTrue(TEXT("Finds derived classes"), bFoundGrandChild);
	TestFalse(TEXT("Skips class default objects"), bFoundDefaultObject);

	bool bExactOnlyChild = true;
	for (TObjectIterator<UHierarchyTestChild> It(RF_ClassDefaultObject, false); It; ++It)
	{
		bExactOnlyChild &= It->GetClass() == UHierarchyTestChild::StaticClass();
	}
	TestTrue(TEXT("Exact class iteration"), bExactOnlyChild);

	TArray<UObject*> Objects;
	GetObjectsOfClass(UHierarchyTestGrandChild::StaticClass(), Objects);
	TestTrue(TEXT("GetObjectsOfClass"), Objects.Contains(GrandChild) && !Objects.Contains(Child));

	TArray<UClass*> Derived;
	GetDerivedClasses(UHierarchyTestBase::StaticClass(), Derived);
	TestTrue(TEXT("GetDerivedClasses"),
		Derived.Contains(UHierarchyTestChild::StaticClass()) &&
			Derived.Contains(UHierarchyTestGrandChild::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectWeakAndSoftTest, "System.CoreUObject.Object.WeakAndSoftPointers",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FObjectWeakAndSoftTest::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Game/SoftTest"));
	UReflectionTestObject* Object = NewObject<UReflectionTestObject>(Package, TEXT("Asset"));

	TWeakObjectPtr<UReflectionTestObject> Weak(Object);
	TestTrue(TEXT("Weak resolves"), Weak.Get() == Object && Weak.IsValid() && !Weak.IsStale());
	TWeakObjectPtr<UReflectionTestObject> Empty;
	TestTrue(TEXT("Empty weak"), Empty.Get() == nullptr && !Empty.IsValid());

	TSoftObjectPtr<UReflectionTestObject> Soft(Object);
	TestEqual(TEXT("Soft path"), Soft.ToString(), TEXT("/Game/SoftTest.Asset"));
	TestTrue(TEXT("Soft resolves"), Soft.Get() == Object);
	TSoftObjectPtr<UReflectionTestObject> ByPath(FSoftObjectPath(TEXT("/Game/SoftTest.Asset")));
	TestTrue(TEXT("Soft path resolves an object in memory"), ByPath.Get() == Object);
	TSoftObjectPtr<UReflectionTestObject> Missing(FSoftObjectPath(TEXT("/Game/SoftTest.Missing")));
	TestTrue(TEXT("Pending soft reference"), Missing.Get() == nullptr && Missing.IsPending());
	TestTrue(TEXT("Soft class"),
		TSoftClassPtr<UObject>(UReflectionTestObject::StaticClass()).Get() == UReflectionTestObject::StaticClass());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
