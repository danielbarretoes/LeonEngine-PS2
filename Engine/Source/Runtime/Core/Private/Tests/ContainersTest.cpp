#include "Algo/BinarySearch.h"
#include "Algo/Sort.h"
#include "Algo/StableSort.h"
#include "Containers/ArrayView.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Templates/Greater.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Counts live instances to catch missing / double destruction. */
	struct FTracked
	{
		static int32 Live;
		int32 Value = 0;

		FTracked(int32 InValue = 0)
			: Value(InValue)
		{
			++Live;
		}
		FTracked(const FTracked& Other)
			: Value(Other.Value)
		{
			++Live;
		}
		FTracked& operator=(const FTracked& Other) = default;
		~FTracked()
		{
			--Live;
		}
		bool operator==(const FTracked& Other) const
		{
			return Value == Other.Value;
		}
		bool operator<(const FTracked& Other) const
		{
			return Value < Other.Value;
		}
	};
	int32 FTracked::Live = 0;

	/** Same key, different payload: checks stability. */
	struct FKeyed
	{
		int32 Key;
		int32 Order;
		bool operator<(const FKeyed& Other) const
		{
			return Key < Other.Key;
		}
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArrayTest, "System.Core.Containers.Array",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArrayTest::RunTest(const FString& Parameters)
{
	{
		TArray<FTracked> Array;
		for (int32 Index = 0; Index < 100; ++Index)
		{
			Array.Emplace(99 - Index);
		}
		TestEqual("Num", Array.Num(), 100);
		TestEqual("Live", FTracked::Live, 100);

		Array.Sort();
		TestEqual("Sorted first", Array[0].Value, 0);
		TestEqual("Sorted last", Array.Last().Value, 99);

		Array.RemoveAt(10, 5);
		TestEqual("RemoveAt", Array.Num(), 95);
		TestEqual("RemoveAt keeps order", Array[10].Value, 15);

		// 0..9 and 15..99: 47 even values.
		TestEqual("RemoveAll count", Array.RemoveAll([](const FTracked& T) { return T.Value % 2 == 0; }), 47);
		TestEqual("RemoveAll result", Array.Num(), 48);
		TestEqual("Live after removals", FTracked::Live, 48);

		TArray<FTracked> Copy = Array;
		TestTrue("Copy equals", Copy == Array);
		TArray<FTracked> Moved = MoveTemp(Copy);
		TestEqual("Moved-from is empty", Copy.Num(), 0);
		TestEqual("Moved-to", Moved.Num(), 48);

		Moved.Insert(FTracked(-1), 0);
		TestEqual("Insert", Moved[0].Value, -1);
		Moved.RemoveAtSwap(0);
		TestEqual("RemoveAtSwap moves the last element", Moved[0].Value, 99);
		TestEqual("Find", Moved.Find(FTracked(99)), 0);
		TestEqual("Find missing", Moved.Find(FTracked(1000)), int32(INDEX_NONE));
		TestTrue("Contains", Moved.Contains(FTracked(3)));
		TestEqual("IndexOfByPredicate", Moved.IndexOfByPredicate([](const FTracked& T) { return T.Value == 3; }), 2);
	}
	TestEqual("No leaks", FTracked::Live, 0);

	TArray<int32> Numbers = {5, 3, 8};
	Numbers.AddUnique(3);
	TestEqual("AddUnique", Numbers.Num(), 3);
	Numbers.SetNum(5);
	TestEqual("SetNum zero-fills ints", Numbers[4], 0);
	Numbers.SetNumZeroed(2);
	TestEqual("SetNum shrinks", Numbers.Num(), 2);
	Numbers.Append({1, 2});
	TestEqual("Append", Numbers.Num(), 4);
	int32 Sum = 0;
	for (int32 Value : Numbers)
	{
		Sum += Value;
	}
	TestEqual("Ranged for", Sum, 5 + 3 + 1 + 2);
	for (auto It = Numbers.CreateIterator(); It; ++It)
	{
		if (*It == 3)
		{
			It.RemoveCurrent();
		}
	}
	TestFalse("Iterator RemoveCurrent", Numbers.Contains(3));
	TestEqual("Pop", Numbers.Pop(), 2);
	Numbers.Reset();
	TestTrue("Reset keeps the allocation", Numbers.IsEmpty() && Numbers.Max() > 0);
	Numbers.Empty();
	TestEqual("Empty frees", Numbers.Max(), 0);

	TArray<int32, TInlineAllocator<4>> Inline = {5, 3, 1};
	TestEqual("Inline capacity", Inline.Max(), 4);
	Inline.Add(7);
	Inline.Add(9);
	TestEqual("Inline spilled", Inline.Num(), 5);
	Inline.StableSort();
	TestEqual("Inline sorted", Inline[0], 1);
	Inline.RemoveAt(0, 3);
	TestEqual("Back to inline", Inline[1], 9);
	TArray<int32, TInlineAllocator<4>> InlineMoved = MoveTemp(Inline);
	TestEqual("Inline move", InlineMoved.Num(), 2);

	TArray<int32, TFixedAllocator<3>> Fixed;
	Fixed.Add(1);
	Fixed.Add(2);
	TestEqual("Fixed", Fixed.Max(), 3);

	TArray<int32> Heap;
	for (int32 Value : {5, 1, 9, 3, 7})
	{
		Heap.HeapPush(Value);
	}
	int32 Top = 0;
	Heap.HeapPop(Top);
	TestEqual("HeapPop min", Top, 1);
	Heap.HeapPop(Top);
	TestEqual("HeapPop next", Top, 3);

	// Arrays of pointers sort by the pointees (UE).
	int32 A = 3;
	int32 B = 1;
	int32 C = 2;
	TArray<int32*> Pointers = {&A, &B, &C};
	Pointers.Sort();
	TestTrue("Pointer arrays sort by value", Pointers[0] == &B && Pointers[2] == &A);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlgoTest, "System.Core.Containers.Algo",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAlgoTest::RunTest(const FString& Parameters)
{
	TArray<int32> Values;
	uint32 Seed = 12345;
	for (int32 Index = 0; Index < 500; ++Index)
	{
		Seed = Seed * 1664525u + 1013904223u;
		Values.Add(int32(Seed % 1000));
	}
	Algo::Sort(Values);
	bool bSorted = true;
	for (int32 Index = 1; Index < Values.Num(); ++Index)
	{
		bSorted &= Values[Index - 1] <= Values[Index];
	}
	TestTrue("Algo::Sort", bSorted);

	Algo::Sort(Values, TGreater<>());
	TestTrue("Descending", Values[0] >= Values.Last());

	TArray<FKeyed> Keyed;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		Keyed.Add({Index % 5, Index});
	}
	Algo::StableSort(Keyed);
	bool bStable = true;
	for (int32 Index = 1; Index < Keyed.Num(); ++Index)
	{
		if (Keyed[Index - 1].Key == Keyed[Index].Key)
		{
			bStable &= Keyed[Index - 1].Order < Keyed[Index].Order;
		}
	}
	TestTrue("Algo::StableSort keeps the order of equal keys", bStable);

	TArray<int32> Sorted = {1, 3, 3, 5, 9};
	TestEqual("LowerBound", Algo::LowerBound(Sorted, 3), 1);
	TestEqual("UpperBound", Algo::UpperBound(Sorted, 3), 3);
	TestEqual("BinarySearch hit", Algo::BinarySearch(Sorted, 5), 3);
	TestEqual("BinarySearch miss", Algo::BinarySearch(Sorted, 4), int32(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBitArrayTest, "System.Core.Containers.BitArray",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBitArrayTest::RunTest(const FString& Parameters)
{
	TBitArray<> Bits;
	for (int32 Index = 0; Index < 70; ++Index)
	{
		Bits.Add(Index % 3 == 0);
	}
	TestEqual("Num", Bits.Num(), 70);
	TestEqual("CountSetBits", Bits.CountSetBits(), 24);
	TestEqual("Find(false)", Bits.Find(false), 1);
	TestEqual("FindLast(true)", Bits.FindLast(true), 69);

	int32 Visited = 0;
	bool bOnlySet = true;
	for (TConstSetBitIterator<> It(Bits); It; ++It)
	{
		bOnlySet &= It.GetIndex() % 3 == 0;
		++Visited;
	}
	TestTrue("Set-bit iterator", bOnlySet && Visited == 24);

	Bits.RemoveAt(0, 3);
	TestEqual("RemoveAt", Bits.Num(), 67);
	TestTrue("Bits shift down", Bits[0]);

	TBitArray<> Zeros(false, 40);
	TestEqual("FindAndSetFirstZeroBit", Zeros.FindAndSetFirstZeroBit(), 0);
	TestEqual("FindAndSetFirstZeroBit again", Zeros.FindAndSetFirstZeroBit(), 1);
	Zeros.SetRange(0, 40, true);
	TestEqual("Full", Zeros.FindAndSetFirstZeroBit(), int32(INDEX_NONE));
	Zeros.Empty();
	TestEqual("Empty", Zeros.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSparseArrayTest, "System.Core.Containers.SparseArray",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSparseArrayTest::RunTest(const FString& Parameters)
{
	{
		TSparseArray<FTracked> Sparse;
		const int32 First = Sparse.Add(FTracked(10));
		const int32 Second = Sparse.Add(FTracked(20));
		Sparse.Add(FTracked(30));
		Sparse.RemoveAt(Second);
		TestEqual("Num after remove", Sparse.Num(), 2);
		TestFalse("Hole", Sparse.IsValidIndex(Second));
		TestEqual("Reuses the hole", Sparse.Add(FTracked(40)), Second);
		TestEqual("Stable index", Sparse[First].Value, 10);

		Sparse.RemoveAt(First);
		TSparseArray<FTracked> Copy = Sparse;
		TestEqual("Copy keeps holes", Copy.GetMaxIndex(), Sparse.GetMaxIndex());
		TestFalse("Copy is not compact", Copy.IsCompact());
		Copy.Compact();
		TestTrue("Compact", Copy.IsCompact() && Copy.Num() == 2);

		int32 Sum = 0;
		for (const FTracked& Element : Sparse)
		{
			Sum += Element.Value;
		}
		TestEqual("Iteration skips holes", Sum, 70);
	}
	TestEqual("No leaks", FTracked::Live, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSetTest, "System.Core.Containers.Set",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSetTest::RunTest(const FString& Parameters)
{
	TSet<int32> Set = {1, 2, 3, 2};
	TestEqual("Duplicates collapse", Set.Num(), 3);
	TestTrue("Contains", Set.Contains(2));
	Set.Remove(2);
	TestFalse("Remove", Set.Contains(2));
	for (int32 Value = 0; Value < 1000; ++Value)
	{
		Set.Add(Value);
	}
	TestEqual("Grows", Set.Num(), 1000);
	TestTrue("Rehashed lookups", Set.Contains(999) && Set.Contains(0));

	TSet<int32> Other = {998, 999, 5000};
	TestEqual("Intersect", Set.Intersect(Other).Num(), 2);
	TestEqual("Union", Set.Union(Other).Num(), 1001);
	TestEqual("Difference", Other.Difference(Set).Num(), 1);

	for (auto It = Set.CreateIterator(); It; ++It)
	{
		if (*It % 2)
		{
			It.RemoveCurrent();
		}
	}
	TestEqual("Iterator RemoveCurrent", Set.Num(), 500);
	Set.Compact();
	TestTrue("Compact keeps lookups", Set.Contains(998));

	TSet<FString> Strings;
	Strings.Add("Leon");
	TestTrue("FString keys ignore case", Strings.Contains("LEON"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapTest, "System.Core.Containers.Map",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMapTest::RunTest(const FString& Parameters)
{
	TMap<int32, int32> Map;
	for (int32 Key = 0; Key < 500; ++Key)
	{
		Map.Add(Key, Key * 2);
	}
	TestEqual("Num", Map.Num(), 500);
	TestEqual("operator[]", Map[250], 500);
	TestEqual("FindRef missing", Map.FindRef(9999), 0);
	Map.Add(250, 7);
	TestEqual("Add replaces", Map.Num(), 500);
	TestEqual("Replaced value", *Map.Find(250), 7);
	TestEqual("FindOrAdd existing", Map.FindOrAdd(1), 2);
	Map.FindOrAdd(1000) = 5;
	TestEqual("FindOrAdd new", Map[1000], 5);

	int32 KeySum = 0;
	for (auto& [Key, Value] : Map)
	{
		KeySum += Key;
		(void)Value;
	}
	TestEqual("Structured bindings", KeySum, 499 * 500 / 2 + 1000);

	int32 Removed = 0;
	TestTrue("RemoveAndCopyValue", Map.RemoveAndCopyValue(3, Removed) && Removed == 6);
	TestFalse("Removed", Map.Contains(3));

	Map.KeySort(TGreater<int32>());
	TestEqual("KeySort", Map.begin()->Key, 1000);
	for (auto It = Map.CreateIterator(); It; ++It)
	{
		if (It.Key() % 2)
		{
			It.RemoveCurrent();
		}
	}
	TestEqual("Iterator RemoveCurrent", Map.Num(), 251);

	TArray<int32> Keys;
	Map.GenerateKeyArray(Keys);
	TestEqual("GenerateKeyArray", Keys.Num(), 251);

	TMap<FString, int32> ByName = {{"One", 1}, {"Two", 2}};
	TestTrue("FString keys ignore case", ByName.Contains("TWO"));
	TestEqual("Initializer list", ByName["one"], 1);

	TMultiMap<int32, int32> Multi;
	Multi.Add(1, 10);
	Multi.Add(1, 11);
	Multi.Add(2, 20);
	TArray<int32> Values;
	Multi.MultiFind(1, Values, true);
	TestTrue("MultiFind in insertion order", Values.Num() == 2 && Values[0] == 10);
	TestEqual("Num(Key)", Multi.Num(1), 2);
	TestEqual("Remove pair", Multi.Remove(1, 11), 1);
	TestEqual("Num after pair removal", Multi.Num(1), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArrayViewTest, "System.Core.Containers.ArrayView",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArrayViewTest::RunTest(const FString& Parameters)
{
	TArray<int32> Backing = {1, 2, 3, 4};
	TArrayView<int32> View(Backing);
	TestEqual("Num", View.Num(), 4);
	TestEqual("Slice", View.Slice(1, 2)[0], 2);
	View[0] = 10;
	TestEqual("Writes through", Backing[0], 10);
	TConstArrayView<int32> ConstView = MakeArrayView(Backing);
	TestTrue("Contains", ConstView.Contains(4));
	TestEqual("RightChop", ConstView.RightChop(3)[0], 4);
	int32 Raw[] = {7, 8};
	TestEqual("From C array", MakeArrayView(Raw).Num(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
