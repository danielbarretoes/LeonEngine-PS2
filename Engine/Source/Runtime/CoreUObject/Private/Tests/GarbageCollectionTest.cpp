#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
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
	TestEqual(TEXT("Leon's interval"), FGarbageCollectionSettings().TimeBetweenPurgingPendingKillObjects, 10.0f);
	if (GConfig)
	{
		// The interval comes from [/Script/Engine.GarbageCollectionSettings], in an in-memory engine file.
		const FString TestIni = TEXT("LeonGarbageCollectionTest.ini");
		GConfig->Add(TestIni, FConfigFile())
			.CombineFromBuffer(
				TEXT("[/Script/Engine.GarbageCollectionSettings]\ngc.TimeBetweenPurgingPendingKillObjects="
					 "30\ngc.IncrementalObjectsPerStep=7\n"));
		const FGarbageCollectionSettings FromConfig = FGarbageCollectionSettings::LoadFromConfig(TestIni);
		TestEqual(TEXT("Interval from the config"), FromConfig.TimeBetweenPurgingPendingKillObjects, 30.0f);
		TestEqual(TEXT("Budget from the config"), FromConfig.IncrementalObjectsPerStep, 7);
		GConfig->Remove(TestIni);
	}

	// The interval starts an incremental collection; each Tick then visits the budget until it ends.
	Collect();
	FGarbageCollectionSettings Settings;
	Settings.TimeBetweenPurgingPendingKillObjects = 1.0f;
	Settings.IncrementalObjectsPerStep = 16;
	FGarbageCollectionTimer Timer(Settings);
	TWeakObjectPtr<UGCTestObject> Weak = NewObject<UGCTestObject>();
	TestFalse(TEXT("Not yet"), Timer.Tick(0.5f));
	TestFalse(TEXT("No collection under way"), IsIncrementalReachabilityAnalysisPending());
	TestTrue(TEXT("Still alive"), Weak.IsValid());
	int32 NumTicks = 1;
	bool bEnded = Timer.Tick(0.6f);
	TestTrue(TEXT("Started once the interval passed"), bEnded || IsIncrementalReachabilityAnalysisPending());
	while (!bEnded && NumTicks < 10000)
	{
		TestTrue(TEXT("Alive until the end"), Weak.IsValid());
		bEnded = Timer.Tick(0.0f);
		++NumTicks;
	}
	TestTrue(TEXT("The collection ended"), bEnded);
	TestTrue(TEXT("In several slices"), NumTicks > 1 && GetLastGarbageCollectionStats().NumSlices == NumTicks);
	TestTrue(TEXT("An incremental collection"), GetLastGarbageCollectionStats().bIncremental);
	TestFalse(TEXT("Collected by the timer"), Weak.IsValid());
	TestEqual(TEXT("Timer restarts"), Timer.GetTimeSinceLastCollection(), 0.0f);
	// A forced collection is a full one, and replaces one under way.
	TWeakObjectPtr<UGCTestObject> Forced = NewObject<UGCTestObject>();
	TestFalse(TEXT("A new collection"), Timer.Tick(1.0f));
	TestTrue(TEXT("Under way"), IsIncrementalReachabilityAnalysisPending());
	Timer.ForceCollectOnNextTick();
	TestTrue(TEXT("Forced collection"), Timer.Tick(0.0f));
	TestFalse(TEXT("It replaced the incremental one"), IsIncrementalReachabilityAnalysisPending());
	TestFalse(TEXT("Full"), GetLastGarbageCollectionStats().bIncremental);
	TestFalse(TEXT("Collected at once"), Forced.IsValid());
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

namespace
{
	/** A random graph of test objects, and what the test knows of it (Leon's incremental collection tests). */
	struct FGCRandomGraph
	{
		TArray<UGCTestObject*> Objects;
		TArray<TWeakObjectPtr<UGCTestObject>> Weak;
		FRandomStream Random;

		explicit FGCRandomGraph(int32 Seed)
			: Random(Seed)
		{
		}

		UGCTestObject* NewNode()
		{
			UGCTestObject* Object = NewObject<UGCTestObject>();
			Objects.Add(Object);
			Weak.Add(Object);
			return Object;
		}

		/** A random live node, or null (a live node is reachable or not: the test picks from the reachable ones). */
		UGCTestObject* Pick(const TArray<UGCTestObject*>& From)
		{
			return From.Num() > 0 ? From[Random.RandRange(0, From.Num() - 1)] : nullptr;
		}

		/** Every strong reference slot of a node the test uses: Ref, RefArray, RefMap values, the struct, NativeRef. */
		static void GetTargets(UGCTestObject& Node, TArray<UObject*>& OutTargets)
		{
			OutTargets.Reset();
			OutTargets.Add(Node.Ref);
			OutTargets.Append(Node.RefArray);
			for (const TPair<FName, UObject*>& Pair : Node.RefMap)
			{
				OutTargets.Add(Pair.Value);
			}
			OutTargets.Add(Node.Struct.Object);
			OutTargets.Append(Node.Struct.Objects);
			for (const FGCTestStruct& Element : Node.StructArray)
			{
				OutTargets.Add(Element.Object);
			}
			OutTargets.Add(Node.NativeRef);
		}

		/** The nodes reachable from the rooted ones through the slots above (the test's own reachability). */
		TArray<UGCTestObject*> GetReachable() const
		{
			TArray<UGCTestObject*> Reachable;
			for (const TWeakObjectPtr<UGCTestObject>& Node : Weak)
			{
				if (Node.IsValid() && Node->IsRooted())
				{
					Reachable.AddUnique(Node.Get());
				}
			}
			TArray<UObject*> Targets;
			for (int32 Index = 0; Index < Reachable.Num(); ++Index)
			{
				GetTargets(*Reachable[Index], Targets);
				for (UObject* Target : Targets)
				{
					UGCTestObject* Node = Cast<UGCTestObject>(Target);
					if (Node != nullptr && !Node->IsPendingKill())
					{
						Reachable.AddUnique(Node);
					}
				}
			}
			return Reachable;
		}

		/** A reference from Node to Target in one of its slots, at random. */
		void Link(UGCTestObject& Node, UGCTestObject* Target)
		{
			switch (Random.RandRange(0, 5))
			{
				case 0:
					Node.Ref = Target;
					break;
				case 1:
					Node.RefArray.Add(Target);
					break;
				case 2:
					Node.RefMap.Add(FName(TEXT("Key"), Random.RandRange(1, 4)), Target);
					break;
				case 3:
					Node.Struct.Objects.Add(Target);
					break;
				case 4:
					Node.StructArray.AddDefaulted_GetRef().Object = Target;
					break;
				default:
					Node.NativeRef = Target;
					break;
			}
		}

		/** Takes one of Node's references away (the slot emptied), and gives the target, or null. */
		UGCTestObject* Unlink(UGCTestObject& Node)
		{
			UObject* Taken = nullptr;
			switch (Random.RandRange(0, 5))
			{
				case 0:
					Swap(Taken, Node.Ref);
					break;
				case 1:
					if (Node.RefArray.Num() > 0)
					{
						Taken = Node.RefArray.Pop(false);
					}
					break;
				case 2:
					for (TPair<FName, UObject*>& Pair : Node.RefMap)
					{
						Swap(Taken, Pair.Value);
						break;
					}
					break;
				case 3:
					if (Node.Struct.Objects.Num() > 0)
					{
						Taken = Node.Struct.Objects[0];
						Node.Struct.Objects.RemoveAt(0);
					}
					break;
				case 4:
					if (Node.StructArray.Num() > 0)
					{
						Taken = Node.StructArray.Last().Object;
						Node.StructArray.Pop(false);
					}
					break;
				default:
					Swap(Taken, Node.NativeRef);
					break;
			}
			return Cast<UGCTestObject>(Taken);
		}

		/** Count nodes, a few rooted, each with up to four random references. */
		void Build(int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				NewNode();
			}
			for (UGCTestObject* Node : Objects)
			{
				if (Random.FRand() < 0.05f)
				{
					Node->AddToRoot();
				}
				for (int32 Link0 = Random.RandRange(0, 4); Link0 > 0; --Link0)
				{
					Link(*Node, Objects[Random.RandRange(0, Objects.Num() - 1)]);
				}
			}
			Objects[0]->AddToRoot();
		}

		/** Unroots every node (the test's end: everything goes). */
		void UnrootAll()
		{
			for (const TWeakObjectPtr<UGCTestObject>& Node : Weak)
			{
				if (Node.IsValid() && Node->IsRooted())
				{
					Node->RemoveFromRoot();
				}
			}
		}
	};

	/** Runs an incremental collection to its end, calling Between between its slices; how many slices it took. */
	template <typename BetweenType>
	int32 CollectIncrementally(int32 Budget, BetweenType&& Between)
	{
		StartIncrementalGarbageCollection(GARBAGE_COLLECTION_KEEPFLAGS);
		int32 Slices = 1;
		while (!IncrementalCollectGarbageStep(Budget))
		{
			Between();
			++Slices;
		}
		return Slices;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionIncrementalMatchesFullTest,
	"System.CoreUObject.GarbageCollection.IncrementalMatchesFull",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionIncrementalMatchesFullTest::RunTest(const FString& Parameters)
{
	// On random graphs left alone between the slices, an incremental collection keeps exactly what a full one keeps:
	// the nodes the roots reach, and nothing else (the unreachable ones and the pending-kill ones go).
	for (int32 Seed = 1; Seed <= 8; ++Seed)
	{
		Collect();
		FGCRandomGraph Graph(Seed);
		Graph.Build(200);
		for (UGCTestObject* Node : Graph.Objects)
		{
			if (!Node->IsRooted() && Graph.Random.FRand() < 0.05f)
			{
				Node->MarkPendingKill();
			}
		}
		const TArray<UGCTestObject*> Expected = Graph.GetReachable();
		const int32 Slices = CollectIncrementally(Seed, [] {});
		TestTrue(TEXT("In slices"), Slices > 1 && GetLastGarbageCollectionStats().bIncremental);
		int32 Wrong = 0;
		for (const TWeakObjectPtr<UGCTestObject>& Node : Graph.Weak)
		{
			const bool bAlive = Node.IsValid(true);
			const bool bExpected = bAlive ? Expected.Contains(Node.Get(true)) : false;
			Wrong += bAlive != bExpected ? 1 : 0;
		}
		int32 NumAlive = 0;
		for (const TWeakObjectPtr<UGCTestObject>& Node : Graph.Weak)
		{
			NumAlive += Node.IsValid(true) ? 1 : 0;
		}
		TestEqual(TEXT("Survivors are the reachable nodes"), NumAlive, Expected.Num());
		TestEqual(TEXT("No node kept or lost wrongly"), Wrong, 0);
		// A full collection after it finds nothing more.
		Collect();
		int32 NumAliveAfterFull = 0;
		for (const TWeakObjectPtr<UGCTestObject>& Node : Graph.Weak)
		{
			NumAliveAfterFull += Node.IsValid(true) ? 1 : 0;
		}
		TestEqual(TEXT("Same as a full collection"), NumAliveAfterFull, NumAlive);
		Graph.UnrootAll();
		Collect();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionIncrementalMutationsTest,
	"System.CoreUObject.GarbageCollection.IncrementalMutations",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionIncrementalMutationsTest::RunTest(const FString& Parameters)
{
	// Between the slices the "game" moves references between reachable nodes (a reference taken from one and stored
	// in another that may have been visited already), makes new nodes, drops references and destroys nodes. No node
	// reachable at the end may be collected, no reference may dangle, and a full collection after it leaves exactly
	// the reachable nodes (what the incremental one kept for being reachable during it goes then).
	int32 NumRevisited = 0;
	for (int32 Seed = 11; Seed <= 18; ++Seed)
	{
		Collect();
		FGCRandomGraph Graph(Seed);
		Graph.Build(150);
		int32 NumMutations = 0;
		const auto Mutate = [&Graph, &NumMutations]()
		{
			for (int32 Count = 0; Count < 4; ++Count)
			{
				++NumMutations;
				const TArray<UGCTestObject*> Reachable = Graph.GetReachable();
				UGCTestObject* From = Graph.Pick(Reachable);
				UGCTestObject* To = Graph.Pick(Reachable);
				if (From == nullptr || To == nullptr)
				{
					return;
				}
				switch (Graph.Random.RandRange(0, 4))
				{
					case 0:
					case 1:
						// A move: the only path to the node may be the one taken away.
						if (UGCTestObject* Moved = Graph.Unlink(*From))
						{
							if (!Moved->IsPendingKill())
							{
								Graph.Link(*To, Moved);
							}
						}
						break;
					case 2:
						// A new node, linked from a reachable one, linking to another.
						{
							UGCTestObject* New = Graph.NewNode();
							Graph.Link(*New, Graph.Pick(Reachable));
							Graph.Link(*To, New);
						}
						break;
					case 3:
						(void)Graph.Unlink(*From);
						break;
					default:
						if (!To->IsRooted())
						{
							To->MarkPendingKill();
						}
						break;
				}
			}
		};
		(void)CollectIncrementally(1 + (Seed % 3), Mutate);
		TestTrue(TEXT("Mutations between the slices"), NumMutations > 0);
		NumRevisited += GetLastGarbageCollectionStats().NumObjectsRevisited;

		// Everything reachable now is alive, and every reference of theirs points at a live object.
		const TArray<UGCTestObject*> Reachable = Graph.GetReachable();
		int32 Lost = 0;
		int32 Dangling = 0;
		TArray<UObject*> Targets;
		for (UGCTestObject* Node : Reachable)
		{
			Lost += GUObjectArray.IsValid(Node) ? 0 : 1;
			if (!GUObjectArray.IsValid(Node))
			{
				continue;
			}
			FGCRandomGraph::GetTargets(*Node, Targets);
			for (UObject* Target : Targets)
			{
				Dangling += Target != nullptr && !GUObjectArray.IsValid(Target) ? 1 : 0;
			}
		}
		TestEqual(TEXT("No reachable node collected"), Lost, 0);
		TestEqual(TEXT("No dangling reference"), Dangling, 0);

		// A full collection then leaves exactly the reachable nodes.
		Collect();
		const TArray<UGCTestObject*> ReachableAfter = Graph.GetReachable();
		int32 NumAlive = 0;
		for (const TWeakObjectPtr<UGCTestObject>& Node : Graph.Weak)
		{
			NumAlive += Node.IsValid(true) ? 1 : 0;
		}
		TestEqual(TEXT("A full collection after it keeps the reachable nodes only"), NumAlive, ReachableAfter.Num());
		Graph.UnrootAll();
		Collect();
	}
	TestTrue(TEXT("The end visited changed objects again"), NumRevisited > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarbageCollectionIncrementalWeakPointersTest,
	"System.CoreUObject.GarbageCollection.IncrementalWeakPointers",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGarbageCollectionIncrementalWeakPointersTest::RunTest(const FString& Parameters)
{
	// Between the slices nothing reads as collected: a weak pointer to an object the collection has not reached still
	// resolves, and an object it reaches through it (stored into a visited object) is kept. Pending kill reads as gone
	// at once, and its references are cleared by the end.
	Collect();
	UGCTestObject* Root = NewObject<UGCTestObject>();
	Root->AddToRoot();
	UGCTestObject* Holder = NewObject<UGCTestObject>();
	Root->Ref = Holder;
	UGCTestObject* Loose = NewObject<UGCTestObject>();
	TWeakObjectPtr<UGCTestObject> WeakLoose = Loose;
	UGCTestObject* Doomed = NewObject<UGCTestObject>();
	Holder->RefArray.Add(Doomed);
	TWeakObjectPtr<UGCTestObject> WeakDoomed = Doomed;
	bool bStored = false;
	(void)CollectIncrementally(1,
		[&]()
		{
			TestTrue(TEXT("An unreached object still resolves"), WeakLoose.IsValid());
			if (!bStored)
			{
				// The root was visited in the first slice: the new reference is found at the end.
				Root->NativeRef = WeakLoose.Get();
				Root->RefSet.Add(WeakLoose.Get());
				bStored = true;
				Doomed->MarkPendingKill();
				TestFalse(TEXT("Pending kill reads as gone"), WeakDoomed.IsValid());
			}
		});
	TestTrue(TEXT("The object the game stored is kept"), WeakLoose.IsValid());
	TestFalse(TEXT("The pending-kill object is collected"), WeakDoomed.IsValid(true));
	TestTrue(TEXT("Its reference is cleared"), Holder->RefArray.Num() == 0 || Holder->RefArray[0] == nullptr);
	Root->RemoveFromRoot();
	Collect();
	TestFalse(TEXT("All gone"), WeakLoose.IsValid(true));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
