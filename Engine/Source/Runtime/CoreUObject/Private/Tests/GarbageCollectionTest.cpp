#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Tests/GarbageCollectionTestTypes.h"
#include "UObject/GCObject.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectArray.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A collection with the engine's keep flags and a full purge, as the engine's safe points run it. */
	void Collect()
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	/** An FGCObject holding one object, as a subsystem would. */
	class FGCTestHolder : public FGCObject
	{
	public:
		UObject* Object = nullptr;

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObject(Object);
		}

		virtual FString GetReferencerName() const override
		{
			return TEXT("FGCTestHolder");
		}
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionUnreferencedTest,
	"System.CoreUObject.GarbageCollection.Unreferenced",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionUnreferencedTest::RunTest(const FString& Parameters)
{
	Collect();
	const int32 NumBefore = GetNumGCObjects();
	TWeakObjectPtr<UGCTestObject> Weak = NewObject<UGCTestObject>();
	TWeakObjectPtr<UGCTestObject> Other = NewObject<UGCTestObject>();
	TestEqual(TEXT("Two new objects"), GetNumGCObjects(), NumBefore + 2);
	TestTrue(TEXT("Alive before the collection"), Weak.IsValid() && Other.IsValid());

	Collect();
	TestFalse(TEXT("Unreferenced object collected"), Weak.IsValid());
	TestFalse(TEXT("Second unreferenced object collected"), Other.IsValid());
	TestEqual(TEXT("Object count back"), GetNumGCObjects(), NumBefore);
	const FGarbageCollectionStats& Stats = GetLastGarbageCollectionStats();
	TestEqual(TEXT("Stats: collected"), Stats.NumObjectsCollected, 2);
	TestEqual(TEXT("Stats: before / after"), Stats.NumObjectsBefore - Stats.NumObjectsAfter, 2);
	TestFalse(TEXT("Not collecting afterwards"), IsGarbageCollecting());
	TestFalse(TEXT("Nothing pending"), IsIncrementalPurgePending());

	// Class default objects, classes and the compiled-in packages are never collected.
	TestNotNull(TEXT("Class default object kept"), GetDefault<UGCTestObject>());
	TestTrue(TEXT("CDO still valid"), GetDefault<UGCTestObject>()->IsValidLowLevel());
	TestTrue(TEXT("Class kept"), UGCTestObject::StaticClass()->IsValidLowLevel());
	UPackage* ScriptPackage = FindObject<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
	TestTrue(TEXT("Compiled-in package kept"), ScriptPackage && ScriptPackage->HasAnyPackageFlags(PKG_CompiledIn));
	TestTrue(TEXT("Transient package kept"), GetTransientPackage()->IsValidLowLevel());
	TestTrue(TEXT("Reflected struct kept"), FGCTestStruct::StaticStruct()->IsValidLowLevel());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionReferencesTest, "System.CoreUObject.GarbageCollection.References",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionReferencesTest::RunTest(const FString& Parameters)
{
	UGCTestObject* Owner = NewObject<UGCTestObject>();
	Owner->AddToRoot();

	// One object per way of referencing it.
	TArray<TWeakObjectPtr<UObject>> Kept;
	const auto Make = [&Kept]()
	{
		UObject* Object = NewObject<UGCTestObject>();
		Kept.Add(Object);
		return Object;
	};
	Owner->Ref = Make();
	Owner->RefArray.Add(Make());
	Owner->RefMap.Add(TEXT("Value"), Make());
	Owner->KeyMap.Add(Make(), 7);
	Owner->RefSet.Add(Make());
	Owner->Struct.Object = Make();
	Owner->Struct.Objects.Add(Make());
	FGCTestStruct Element;
	Element.Object = Make();
	Owner->StructArray.Add(Element);
	Owner->FixedRefs[1] = Make();
	Owner->NativeRef = Make();
	UGCTestObject* Chained = NewObject<UGCTestObject>();
	((UGCTestObject*)Owner->Ref)->Ref = Chained;
	Kept.Add(Chained);

	// Not kept: weak, soft and unreported references.
	TWeakObjectPtr<UObject> WeakOnly = NewObject<UGCTestObject>();
	Owner->WeakRef = WeakOnly.Get();
	TWeakObjectPtr<UObject> SoftOnly = NewObject<UGCTestObject>();
	Owner->SoftRef = SoftOnly.Get();
	TWeakObjectPtr<UObject> Unreported = NewObject<UGCTestObject>();
	Owner->UnreportedRef = Unreported.Get();

	Collect();
	static const TCHAR* const Names[] = {TEXT("UPROPERTY object"), TEXT("array element"), TEXT("map value"),
		TEXT("map key"), TEXT("set element"), TEXT("struct member"), TEXT("array in a struct"),
		TEXT("struct in an array"), TEXT("C array element"), TEXT("AddReferencedObjects"), TEXT("reference chain")};
	for (int32 Index = 0; Index < Kept.Num(); ++Index)
	{
		TestTrue(Names[Index], Kept[Index].IsValid());
	}
	TestFalse(TEXT("Weak reference does not keep"), WeakOnly.IsValid());
	TestTrue(TEXT("Weak member reads null"), Owner->WeakRef.Get() == nullptr && Owner->WeakRef.IsStale());
	TestFalse(TEXT("Soft reference does not keep"), SoftOnly.IsValid());
	TestTrue(TEXT("Soft member keeps its path"), Owner->SoftRef.IsPending() && !Owner->SoftRef.IsNull());
	TestFalse(TEXT("Unreported pointer does not keep"), Unreported.IsValid());
	TestTrue(TEXT("Values survive"), Owner->KeyMap.Num() == 1 && Owner->RefSet.Num() == 1);

	Owner->RemoveFromRoot();
	Collect();
	for (int32 Index = 0; Index < Kept.Num(); ++Index)
	{
		TestFalse(TEXT("Collected with the owner"), Kept[Index].IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionOuterTest, "System.CoreUObject.GarbageCollection.Outer",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionOuterTest::RunTest(const FString& Parameters)
{
	UGCTestObject* Parent = NewObject<UGCTestObject>();
	UGCTestObject* Child = NewObject<UGCTestObject>(Parent);
	UGCTestObject* GrandChild = NewObject<UGCTestObject>(Child);
	TWeakObjectPtr<UGCTestObject> WeakParent = Parent;
	TWeakObjectPtr<UGCTestObject> WeakChild = Child;
	TWeakObjectPtr<UGCTestObject> WeakGrandChild = GrandChild;

	// A reachable object keeps its outers alive, not the other way round (UE).
	GrandChild->AddToRoot();
	Collect();
	TestTrue(TEXT("Outer chain kept by a rooted inner object"),
		WeakParent.IsValid() && WeakChild.IsValid() && WeakGrandChild.IsValid());
	GrandChild->RemoveFromRoot();

	Parent->AddToRoot();
	Collect();
	TestTrue(TEXT("Rooted outer kept"), WeakParent.IsValid());
	TestFalse(TEXT("Its unreferenced inner objects are collected"), WeakChild.IsValid() || WeakGrandChild.IsValid());
	Parent->RemoveFromRoot();
	Collect();
	TestFalse(TEXT("Outer collected once unrooted"), WeakParent.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionRootsTest, "System.CoreUObject.GarbageCollection.Roots",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionRootsTest::RunTest(const FString& Parameters)
{
	UGCTestObject* Rooted = NewObject<UGCTestObject>();
	Rooted->AddToRoot();
	TWeakObjectPtr<UGCTestObject> WeakRooted = Rooted;
	TestTrue(TEXT("IsRooted"), Rooted->IsRooted());

	// RF_Standalone keeps an object only in a collection that passes it in KeepFlags (UE: the editor does).
	TWeakObjectPtr<UGCTestObject> Standalone =
		NewObject<UGCTestObject>(GetTransientPackage(), NAME_None, RF_Standalone);
	CollectGarbage(RF_Standalone);
	TestTrue(TEXT("Root set kept"), WeakRooted.IsValid());
	TestTrue(TEXT("KeepFlags kept"), Standalone.IsValid());
	Collect();
	TestTrue(TEXT("Root set kept again"), WeakRooted.IsValid());
	TestFalse(TEXT("Standalone collected without the keep flag"), Standalone.IsValid());

	Rooted->RemoveFromRoot();
	TestFalse(TEXT("RemoveFromRoot"), Rooted->IsRooted());
	Collect();
	TestFalse(TEXT("Collected once unrooted"), WeakRooted.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionGCObjectTest, "System.CoreUObject.GarbageCollection.GCObject",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionGCObjectTest::RunTest(const FString& Parameters)
{
	TWeakObjectPtr<UGCTestObject> Weak;
	FGCObject* HolderAddress = nullptr;
	{
		FGCTestHolder Holder;
		HolderAddress = &Holder;
		TestTrue(TEXT("Registered"), FGCObject::GetRegisteredGCObjects().Contains(&Holder));
		Holder.Object = NewObject<UGCTestObject>();
		Weak = (UGCTestObject*)Holder.Object;
		Collect();
		TestTrue(TEXT("FGCObject reference keeps the object"), Weak.IsValid());
		TestTrue(TEXT("The reference is unchanged"), Holder.Object == Weak.Get());

		// A copy registers too.
		FGCTestHolder Copy(Holder);
		TestTrue(TEXT("Copy registered"), FGCObject::GetRegisteredGCObjects().Contains(&Copy));
		Holder.Object = nullptr;
		Collect();
		TestTrue(TEXT("Copy keeps the object"), Weak.IsValid());
	}
	TestFalse(TEXT("Unregistered"), FGCObject::GetRegisteredGCObjects().Contains(HolderAddress));
	Collect();
	TestFalse(TEXT("Collected once no FGCObject reports it"), Weak.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionStrongObjectPtrTest,
	"System.CoreUObject.GarbageCollection.StrongObjectPtr",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionStrongObjectPtrTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGCTestObject> Strong(NewObject<UGCTestObject>());
	TWeakObjectPtr<UGCTestObject> Weak = Strong.Get();
	TestTrue(TEXT("Valid"), Strong.IsValid() && Strong.Get() == Weak.Get());
	Collect();
	TestTrue(TEXT("Kept"), Weak.IsValid());

	TStrongObjectPtr<UGCTestObject> Copy = Strong;
	Strong.Reset();
	TestFalse(TEXT("Reset"), Strong.IsValid());
	Collect();
	TestTrue(TEXT("The copy keeps it"), Weak.IsValid() && Copy.Get() == Weak.Get());

	TStrongObjectPtr<UGCTestObject> Moved = MoveTemp(Copy);
	Collect();
	TestTrue(TEXT("The moved pointer keeps it"), Weak.IsValid() && Moved.Get() == Weak.Get());
	Moved.Reset();
	Collect();
	TestFalse(TEXT("Collected once no pointer holds it"), Weak.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionWeakTest, "System.CoreUObject.GarbageCollection.WeakPointers",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionWeakTest::RunTest(const FString& Parameters)
{
	UGCTestObject* Object = NewObject<UGCTestObject>();
	TWeakObjectPtr<UGCTestObject> Weak = Object;
	TWeakObjectPtr<UObject> AsBase = Weak;
	TestTrue(TEXT("Valid"), Weak.IsValid() && !Weak.IsStale() && Weak == Object && AsBase == Weak);
	TestTrue(TEXT("Hash of equal pointers"), GetTypeHash(Weak) == GetTypeHash(TWeakObjectPtr<UGCTestObject>(Object)));
	TWeakObjectPtr<UGCTestObject> Null;
	TestTrue(TEXT("Explicitly null"), Null.IsExplicitlyNull() && !Null.IsStale() && Null == nullptr);

	Collect();
	TestNull(TEXT("Stale weak pointer reads null"), Weak.Get());
	TestTrue(TEXT("IsStale"), Weak.IsStale() && !Weak.IsValid() && !Weak.IsExplicitlyNull());
	TestTrue(TEXT("Invalid pointers compare equal"), Weak == Null);
	Weak.Reset();
	TestTrue(TEXT("Reset"), Weak.IsExplicitlyNull() && !Weak.IsStale());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionPendingKillTest, "System.CoreUObject.GarbageCollection.PendingKill",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionPendingKillTest::RunTest(const FString& Parameters)
{
	UGCTestObject* Owner = NewObject<UGCTestObject>();
	Owner->AddToRoot();
	UGCTestObject* Victim = NewObject<UGCTestObject>();
	UGCTestObject* Survivor = NewObject<UGCTestObject>();
	Owner->Ref = Victim;
	Owner->RefArray = {Victim, Survivor};
	Owner->RefMap.Add(TEXT("Victim"), Victim);
	Owner->KeyMap.Add(Victim, 1);
	Owner->KeyMap.Add(Survivor, 2);
	Owner->RefSet.Add(Victim);
	Owner->RefSet.Add(Survivor);
	Owner->Struct.Object = Victim;
	Owner->NativeRef = Victim;
	TStrongObjectPtr<UGCTestObject> Strong(Victim);
	TWeakObjectPtr<UGCTestObject> WeakVictim = Victim;
	TWeakObjectPtr<UGCTestObject> WeakSurvivor = Survivor;

	Victim->MarkPendingKill();
	TestTrue(TEXT("IsPendingKill"), Victim->IsPendingKill() && !IsValid(Victim));
	TestNull(TEXT("A weak pointer stops resolving at once"), WeakVictim.Get());
	TestTrue(TEXT("Unless asked for pending-kill objects"), WeakVictim.Get(true) == Victim);

	Collect();
	TestFalse(TEXT("Pending-kill object collected although referenced"), WeakVictim.IsValid(true));
	TestTrue(TEXT("Other objects kept"), WeakSurvivor.IsValid());
	TestNull(TEXT("UPROPERTY cleared"), Owner->Ref);
	TestTrue(TEXT("Array element cleared"),
		Owner->RefArray.Num() == 2 && Owner->RefArray[0] == nullptr && Owner->RefArray[1] == Survivor);
	TestTrue(TEXT("Map value cleared"), Owner->RefMap.Num() == 1 && Owner->RefMap[TEXT("Victim")] == nullptr);
	TestTrue(TEXT("Map pair removed with its key"),
		Owner->KeyMap.Num() == 1 && Owner->KeyMap.Contains(Survivor) && Owner->KeyMap[Survivor] == 2);
	TestTrue(TEXT("Set element removed"), Owner->RefSet.Num() == 1 && Owner->RefSet.Contains(Survivor));
	TestNull(TEXT("Struct member cleared"), Owner->Struct.Object);
	TestNull(TEXT("AddReferencedObjects reference cleared"), Owner->NativeRef);
	TestNull(TEXT("TStrongObjectPtr cleared"), Strong.Get());
	TestTrue(TEXT("References cleared counted"), GetLastGarbageCollectionStats().NumReferencesCleared >= 8);

	Owner->RemoveFromRoot();
	Collect();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionDestroyOrderTest,
	"System.CoreUObject.GarbageCollection.DestroyOrder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionDestroyOrderTest::RunTest(const FString& Parameters)
{
	Collect();
	TArray<FString>& Log = UGCTestDestroyTracker::GetEventLog();
	Log.Reset();
	UGCTestDestroyTracker* First = NewObject<UGCTestDestroyTracker>();
	First->Id = 1;
	UGCTestDestroyTracker* Second = NewObject<UGCTestDestroyTracker>();
	Second->Id = 2;
	const FName SecondName = Second->GetFName();

	// Every BeginDestroy, then every FinishDestroy, then the destructors (UE); within a step, GUObjectArray order.
	Collect();
	const auto IsStep = [&Log](int32 Index, const TCHAR* Step)
	{
		return Log.IsValidIndex(Index) && Log[Index].StartsWith(Step) &&
			(Log[Index].EndsWith(TEXT(" 1")) || Log[Index].EndsWith(TEXT(" 2")));
	};
	TestTrue(TEXT("BeginDestroy, FinishDestroy, destructor order"),
		Log.Num() == 6 && IsStep(0, TEXT("Begin")) && IsStep(1, TEXT("Begin")) && IsStep(2, TEXT("Finish")) &&
			IsStep(3, TEXT("Finish")) && IsStep(4, TEXT("Destroy")) && IsStep(5, TEXT("Destroy")) && Log[0] != Log[1] &&
			Log[4] != Log[5]);
	TestNull(
		TEXT("A destroyed object's name is free"), StaticFindObjectFast(nullptr, GetTransientPackage(), SecondName));

	// An object not ready for FinishDestroy waits: an incremental purge polls it, the next full purge finishes it.
	Log.Reset();
	UGCTestDestroyTracker* Slow = NewObject<UGCTestDestroyTracker>();
	Slow->Id = 3;
	Slow->NotReadyCount = 2;
	TWeakObjectPtr<UGCTestDestroyTracker> WeakSlow = Slow;
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, /*bPerformFullPurge =*/false);
	TestTrue(TEXT("Purge pending"), IsIncrementalPurgePending());
	TestFalse(TEXT("Unreachable reads as gone"), WeakSlow.IsValid());
	TestTrue(TEXT("BeginDestroy ran"), Log.Num() == 1 && Log[0] == TEXT("Begin 3"));
	IncrementalPurgeGarbage(true, 1.0f);
	TestTrue(TEXT("Not ready yet"), IsIncrementalPurgePending() && Log.Last() == TEXT("NotReady 3"));
	IncrementalPurgeGarbage(false);
	TestFalse(TEXT("Purge done"), IsIncrementalPurgePending());
	const TArray<FString> ExpectedSlow = {
		TEXT("Begin 3"), TEXT("NotReady 3"), TEXT("NotReady 3"), TEXT("Finish 3"), TEXT("Destroy 3")};
	TestTrue(TEXT("Waited for IsReadyForFinishDestroy"), Log == ExpectedSlow);
	Log.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionSlotReuseTest, "System.CoreUObject.GarbageCollection.SlotReuse",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionSlotReuseTest::RunTest(const FString& Parameters)
{
	Collect();
	UGCTestObject* First = NewObject<UGCTestObject>();
	const int32 Index = GUObjectArray.ObjectToIndex(First);
	TWeakObjectPtr<UGCTestObject> WeakFirst = First;
	const int32 FirstSerial = GUObjectArray.GetSerialNumber(Index);
	TestTrue(TEXT("A weak pointer allocated a serial number"), FirstSerial != 0);

	Collect();
	TestEqual(TEXT("The freed slot has no serial number"), GUObjectArray.GetSerialNumber(Index), 0);
	// The last freed slot is reused first.
	UGCTestObject* Second = NewObject<UGCTestObject>();
	TestEqual(TEXT("Slot reused"), GUObjectArray.ObjectToIndex(Second), Index);
	TWeakObjectPtr<UGCTestObject> WeakSecond = Second;
	TestTrue(TEXT("New serial number"), GUObjectArray.GetSerialNumber(Index) != FirstSerial);
	TestTrue(TEXT("The old weak pointer stays stale"), WeakFirst.IsStale() && WeakFirst.Get() == nullptr);
	TestTrue(TEXT("The new one resolves"), WeakSecond.Get() == Second);
	TestFalse(TEXT("Different pointers"), WeakFirst.HasSameIndexAndSerialNumber(WeakSecond));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionCycleTest, "System.CoreUObject.GarbageCollection.Cycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionCycleTest::RunTest(const FString& Parameters)
{
	UGCTestObject* A = NewObject<UGCTestObject>();
	UGCTestObject* B = NewObject<UGCTestObject>();
	UGCTestObject* C = NewObject<UGCTestObject>();
	A->Ref = B;
	B->Ref = C;
	C->Ref = A;
	TWeakObjectPtr<UGCTestObject> WeakA = A;
	TWeakObjectPtr<UGCTestObject> WeakB = B;
	TWeakObjectPtr<UGCTestObject> WeakC = C;

	B->AddToRoot();
	Collect();
	TestTrue(TEXT("A rooted member keeps the cycle"), WeakA.IsValid() && WeakB.IsValid() && WeakC.IsValid());
	B->RemoveFromRoot();
	Collect();
	TestFalse(TEXT("An unreferenced cycle is collected"), WeakA.IsValid() || WeakB.IsValid() || WeakC.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionTimerTest, "System.CoreUObject.GarbageCollection.Timer",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionTimerTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("UE default interval"), FGarbageCollectionSettings().TimeBetweenPurgingPendingKillObjects, 61.1f);
	if (GConfig)
	{
		// The interval comes from [/Script/Engine.GarbageCollectionSettings], in an in-memory engine file.
		const FString TestIni = TEXT("LeonGarbageCollectionTest.ini");
		GConfig->Add(TestIni, FConfigFile())
			.CombineFromBuffer(
				TEXT("[/Script/Engine.GarbageCollectionSettings]\ngc.TimeBetweenPurgingPendingKillObjects=30\n"));
		TestEqual(TEXT("Interval from the config"),
			FGarbageCollectionSettings::LoadFromConfig(TestIni).TimeBetweenPurgingPendingKillObjects, 30.0f);
		GConfig->Remove(TestIni);
	}

	FGarbageCollectionSettings Settings;
	Settings.TimeBetweenPurgingPendingKillObjects = 1.0f;
	FGarbageCollectionTimer Timer(Settings);
	TWeakObjectPtr<UGCTestObject> Weak = NewObject<UGCTestObject>();
	TestFalse(TEXT("Not yet"), Timer.Tick(0.5f));
	TestTrue(TEXT("Still alive"), Weak.IsValid());
	TestTrue(TEXT("Collects once the interval passed"), Timer.Tick(0.6f));
	TestFalse(TEXT("Collected by the timer"), Weak.IsValid());
	TestEqual(TEXT("Timer restarts"), Timer.GetTimeSinceLastCollection(), 0.0f);
	Timer.ForceCollectOnNextTick();
	TestTrue(TEXT("Forced collection"), Timer.Tick(0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionBudgetTest, "System.CoreUObject.GarbageCollection.Budget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionBudgetTest::RunTest(const FString& Parameters)
{
	// The cost of a collection over NumObjects objects, for the platform budgets (PS2: Budgets.md). The objects form
	// a chain from one root, so the first collection marks all of them and the second destroys all of them.
	constexpr int32 NumObjects = 2000;
	Collect();
	const SIZE_T HeapBefore = FMemory::GetUsage().CurrentBytes;
	UGCTestObject* Head = NewObject<UGCTestObject>();
	Head->AddToRoot();
	UGCTestObject* Tail = Head;
	for (int32 Index = 1; Index < NumObjects; ++Index)
	{
		UGCTestObject* Next = NewObject<UGCTestObject>();
		Tail->Ref = Next;
		Tail = Next;
	}
	const SIZE_T HeapWithObjects = FMemory::GetUsage().CurrentBytes;
	const int32 ObjectsWithChain = GetNumGCObjects();

	Collect();
	const FGarbageCollectionStats MarkStats = GetLastGarbageCollectionStats();
	TestEqual(TEXT("Everything reachable"), MarkStats.NumObjectsCollected, 0);

	Head->RemoveFromRoot();
	Collect();
	const FGarbageCollectionStats PurgeStats = GetLastGarbageCollectionStats();
	TestEqual(TEXT("Everything collected"), PurgeStats.NumObjectsCollected, NumObjects);
	const SIZE_T HeapAfter = FMemory::GetUsage().CurrentBytes;
	TestTrue(TEXT("Heap returned"), HeapAfter < HeapWithObjects);

	UE_LOG(LogGarbage, Display,
		TEXT(
			"GC budget: %d objects alive; mark all %d reachable: %.3f ms; collect %d: %.3f ms (mark %.3f, purge %.3f); "
			"heap %d KB -> %d KB -> %d KB"),
		ObjectsWithChain, NumObjects, (MarkStats.MarkSeconds + MarkStats.PurgeSeconds) * 1000.0, NumObjects,
		(PurgeStats.MarkSeconds + PurgeStats.PurgeSeconds) * 1000.0, PurgeStats.MarkSeconds * 1000.0,
		PurgeStats.PurgeSeconds * 1000.0, int32(HeapBefore / 1024), int32(HeapWithObjects / 1024),
		int32(HeapAfter / 1024));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
