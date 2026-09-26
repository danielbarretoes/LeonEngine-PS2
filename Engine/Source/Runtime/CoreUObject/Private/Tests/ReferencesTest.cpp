#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/ConfigExecTestTypes.h"
#include "Tests/GarbageCollectionTestTypes.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUObjectDelegateTest, "System.CoreUObject.Delegates.UObjectBindings",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUObjectDelegateTest::RunTest(const FString& Parameters)
{
	DECLARE_DELEGATE_OneParam(FOnValue, int32);
	DECLARE_DELEGATE_RetVal(int32, FGetValue);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnValueMulticast, int32);

	UGCTestObject* Target = NewObject<UGCTestObject>();
	UGCTestObject* Kept = NewObject<UGCTestObject>();
	Kept->AddToRoot();

	FOnValue Single;
	Single.BindUObject(Target, &UGCTestObject::OnValue);
	TestTrue(TEXT("BindUObject"), Single.IsBound() && Single.IsBoundToObject(Target));
	Single.Execute(3);
	TestEqual(TEXT("Execute"), Target->Received, 3);
	FGetValue Getter = FGetValue::CreateUObject((const UGCTestObject*)Target, &UGCTestObject::GetReceived);
	TestEqual(TEXT("Const member with a return value"), Getter.Execute(), 3);

	FOnValueMulticast Multicast;
	Multicast.AddUObject(Target, &UGCTestObject::OnValue);
	Multicast.AddUObject(Kept, &UGCTestObject::OnValue);
	Multicast.Broadcast(2);
	TestTrue(TEXT("AddUObject"), Target->Received == 5 && Kept->Received == 2);

	// The binding does not keep the object alive, and goes inert once it is collected.
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("Collected object: not bound"), Single.IsBound());
	TestFalse(TEXT("ExecuteIfBound skips it"), Single.ExecuteIfBound(1));
	TestFalse(TEXT("Getter not bound"), Getter.IsBound());
	Multicast.Broadcast(4);
	TestEqual(TEXT("Broadcast skips the collected object"), Kept->Received, 6);
	TestTrue(TEXT("Still bound to the live object"), Multicast.IsBound() && Multicast.IsBoundToObject(Kept));

	// A pending-kill object is skipped at once.
	UGCTestObject* Doomed = NewObject<UGCTestObject>();
	FOnValue ToDoomed = FOnValue::CreateUObject(Doomed, &UGCTestObject::OnValue);
	Doomed->MarkPendingKill();
	TestFalse(TEXT("Pending-kill object: not bound"), ToDoomed.IsBound());

	TestEqual(TEXT("RemoveAll(object)"), Multicast.RemoveAll(Kept), 1);
	TestFalse(TEXT("Nothing left"), Multicast.IsBound());
	Kept->RemoveFromRoot();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoftObjectPathTest, "System.CoreUObject.SoftObject.Path",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSoftObjectPathTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Asset(TEXT("/Game/Maps/Arena.Arena"));
	TestEqual(TEXT("ToString"), Asset.ToString(), TEXT("/Game/Maps/Arena.Arena"));
	TestEqual(TEXT("Package"), Asset.GetLongPackageName(), TEXT("/Game/Maps/Arena"));
	TestEqual(TEXT("Asset name"), Asset.GetAssetName(), TEXT("Arena"));
	TestTrue(TEXT("IsAsset"), Asset.IsAsset() && !Asset.IsSubobject() && Asset.IsValid());

	const FSoftObjectPath Subobject(TEXT("/Game/Maps/Arena.Arena:PersistentLevel.Door"));
	TestEqual(TEXT("Asset path"), Subobject.GetAssetPathString(), TEXT("/Game/Maps/Arena.Arena"));
	TestEqual(TEXT("Subobject path"), Subobject.GetSubPathString(), TEXT("PersistentLevel.Door"));
	TestEqual(TEXT("Round trip"), Subobject.ToString(), TEXT("/Game/Maps/Arena.Arena:PersistentLevel.Door"));
	TestTrue(TEXT("IsSubobject"), Subobject.IsSubobject());

	TestTrue(TEXT("Exported reference form"),
		FSoftObjectPath(TEXT("StaticMesh'/Game/Meshes/Crate.Crate'")) ==
			FSoftObjectPath(TEXT("/Game/Meshes/Crate.Crate")));
	TestTrue(TEXT("None"), FSoftObjectPath(TEXT("None")).IsNull() && FSoftObjectPath().ToString().IsEmpty());
	TestTrue(TEXT("Equality and hash"),
		Asset == FSoftObjectPath(TEXT("/Game/Maps/Arena.Arena")) &&
			GetTypeHash(Asset) == GetTypeHash(FSoftObjectPath(Asset)));

	// Objects in memory resolve, and TryLoad finds them without loading (the package tests load from disk).
	UPackage* Package = CreatePackage(TEXT("/Game/LeonSoftPathTest"));
	UGCTestObject* Object = NewObject<UGCTestObject>(Package, TEXT("SoftTarget"));
	const FSoftObjectPath ObjectPath(Object);
	TestEqual(TEXT("Path of an object"), ObjectPath.ToString(), TEXT("/Game/LeonSoftPathTest.SoftTarget"));
	TestTrue(TEXT("ResolveObject"), ObjectPath.ResolveObject() == Object && ObjectPath.TryLoad() == Object);
	TestNull(TEXT("Missing object"), FSoftObjectPath(TEXT("/Game/LeonSoftPathTest.Missing")).ResolveObject());

	const FSoftClassPath ClassPath(UGCTestObject::StaticClass());
	TestEqual(TEXT("Class path"), ClassPath.ToString(), TEXT("/Script/CoreUObject.GCTestObject"));
	TestTrue(TEXT("ResolveClass"), ClassPath.ResolveClass() == UGCTestObject::StaticClass());
	TestTrue(TEXT("TryLoadClass<T>"),
		ClassPath.TryLoadClass<UObject>() == UGCTestObject::StaticClass() &&
			ClassPath.TryLoadClass<UExecTestObject>() == nullptr);

	// Soft pointers: pending until the object exists, then valid; they do not keep it alive.
	TSoftObjectPtr<UGCTestObject> Soft(FSoftObjectPath(TEXT("/Game/LeonSoftPathTest.Later")));
	TestTrue(TEXT("Pending"), Soft.IsPending() && !Soft.IsValid() && !Soft.IsNull());
	UGCTestObject* Later = NewObject<UGCTestObject>(Package, TEXT("Later"));
	TestTrue(TEXT("Found once created"), Soft.Get() == Later && Soft.LoadSynchronous() == Later);
	TestEqual(TEXT("Soft asset name"), Soft.GetAssetName(), TEXT("Later"));
	const TSoftObjectPtr<UExecTestObject> WrongType{FSoftObjectPath(Later)};
	TestNull(TEXT("Wrong type reads null"), WrongType.Get());
	TSoftClassPtr<UObject> SoftClass(UGCTestObject::StaticClass());
	TestTrue(TEXT("TSoftClassPtr"), SoftClass.Get() == UGCTestObject::StaticClass());
	TestNull(
		TEXT("TSoftClassPtr of an unrelated base"), TSoftClassPtr<UExecTestObject>(UGCTestObject::StaticClass()).Get());

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue(TEXT("Soft pointer does not keep its object"), Soft.IsPending() && Soft.Get() == nullptr);

	// Text form of a reflected FSoftObjectPath / FSoftClassPath: the path itself.
	UConfigTestObject* Holder = NewObject<UConfigTestObject>();
	FProperty* SoftPathProperty = UConfigTestObject::StaticClass()->FindPropertyByName(TEXT("SoftPath"));
	FProperty* SoftClassProperty = UConfigTestObject::StaticClass()->FindPropertyByName(TEXT("SoftClass"));
	TestTrue(TEXT("Reflected as structs"),
		CastField<FStructProperty>(SoftPathProperty) && CastField<FStructProperty>(SoftClassProperty) &&
			(CastField<FStructProperty>(SoftPathProperty)->Struct->StructFlags & STRUCT_ImportTextItemNative));
	TestNotNull(TEXT("ImportText"),
		SoftPathProperty->ImportText(
			TEXT("/Game/Maps/Arena.Arena"), SoftPathProperty->ContainerPtrToValuePtr<void>(Holder), PPF_None, Holder));
	TestTrue(TEXT("Imported path"), Holder->SoftPath == Asset);
	FString Exported;
	SoftPathProperty->ExportText_InContainer(0, Exported, Holder, nullptr, nullptr, PPF_None);
	TestEqual(TEXT("ExportText"), Exported, TEXT("/Game/Maps/Arena.Arena"));
	Exported.Empty();
	SoftPathProperty->ExportText_InContainer(0, Exported, Holder, nullptr, nullptr, PPF_Delimited);
	TestEqual(TEXT("Delimited ExportText"), Exported, TEXT("\"/Game/Maps/Arena.Arena\""));
	TestNotNull(TEXT("ImportText of an exported class reference"),
		SoftClassProperty->ImportText(TEXT("Class'/Script/CoreUObject.GCTestObject'"),
			SoftClassProperty->ContainerPtrToValuePtr<void>(Holder), PPF_None, Holder));
	TestTrue(TEXT("Imported class path"), Holder->SoftClass.ResolveClass() == UGCTestObject::StaticClass());
	TestNotNull(TEXT("Generic struct form still works"),
		SoftPathProperty->ImportText(TEXT("(AssetPathName=\"/Game/A.A\",SubPathString=\"B\")"),
			SoftPathProperty->ContainerPtrToValuePtr<void>(Holder), PPF_None, Holder));
	TestEqual(TEXT("Generic struct form"), Holder->SoftPath.ToString(), TEXT("/Game/A.A:B"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubclassOfTest, "System.CoreUObject.SoftObject.SubclassOf",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSubclassOfTest::RunTest(const FString& Parameters)
{
	TSubclassOf<UGCTestObject> Subclass = UGCTestObject::StaticClass();
	TestTrue(TEXT("TSubclassOf of its own class"), Subclass.Get() == UGCTestObject::StaticClass());
	TestTrue(TEXT("Default object"), Subclass.GetDefaultObject() == GetDefault<UGCTestObject>());
	TSubclassOf<UObject> Base = Subclass;
	TestTrue(TEXT("Converts to a base"), Base.Get() == UGCTestObject::StaticClass());
	TSubclassOf<UGCTestObject> Wrong = UExecTestObject::StaticClass();
	TestNull(TEXT("A class that is not a T reads null"), Wrong.Get());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
