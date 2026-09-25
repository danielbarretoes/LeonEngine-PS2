#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FBase
	{
		virtual ~FBase() = default;
		int32 Id = 1;
	};

	struct FDerived : FBase
	{
		FDerived()
		{
			Id = 2;
		}
	};

	struct FSelfShared : public TSharedFromThis<FSelfShared>
	{
		int32 Value = 5;
	};

	struct FDestroyCounter
	{
		int32* Counter;
		explicit FDestroyCounter(int32* InCounter)
			: Counter(InCounter)
		{
		}
		~FDestroyCounter()
		{
			++*Counter;
		}
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTupleTest, "System.Core.Templates.Tuple",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTupleTest::RunTest(const FString& Parameters)
{
	TPair<int32, float> Pair(1, 2.0f);
	TestEqual("Pair Key", Pair.Key, 1);
	TestEqual("Pair Value", Pair.Value, 2.0f);

	auto Tuple = MakeTuple(1, 2, 3);
	TestEqual("Get<2>", Tuple.Get<2>(), 3);
	int32 X = 0;
	int32 Y = 0;
	int32 Z = 0;
	Tie(X, Y, Z) = Tuple;
	TestEqual("Tie", X + Y + Z, 6);

	auto [First, Second] = MakeTuple(FString("a"), 7);
	TestEqual("Structured bindings", First, TEXT("a"));
	TestEqual("Structured bindings 2", Second, 7);

	TestTrue("operator==", MakeTuple(1, 2) == MakeTuple(1, 2));
	TestTrue("operator<", MakeTuple(1, 2) < MakeTuple(1, 3));
	TestEqual(
		"ApplyAfter", Tuple.ApplyAfter([](int32 Base, int32 A, int32 B, int32 C) { return Base + A + B + C; }, 10), 16);
	TestEqual("Hash is stable", GetTypeHash(MakeTuple(1, 2)), GetTypeHash(MakeTuple(1, 2)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUniquePtrTest, "System.Core.Templates.UniquePtr",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FUniquePtrTest::RunTest(const FString& Parameters)
{
	TUniquePtr<FBase> Unique = MakeUnique<FDerived>();
	TestEqual("Derived", Unique->Id, 2);
	TUniquePtr<FBase> Other = MoveTemp(Unique);
	TestFalse("Moved-from", Unique.IsValid());
	TestTrue("Moved-to", Other.IsValid());

	TUniquePtr<int32[]> Ints = MakeUnique<int32[]>(4);
	TestEqual("Array value-initialised", Ints[3], 0);

	int32 Destroyed = 0;
	{
		TUniquePtr<FDestroyCounter> Counted = MakeUnique<FDestroyCounter>(&Destroyed);
		Counted.Reset(new FDestroyCounter(&Destroyed));
		TestEqual("Reset destroys the old object", Destroyed, 1);
	}
	TestEqual("Destructor destroys", Destroyed, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedPointerTest, "System.Core.Templates.SharedPointer",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSharedPointerTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBase> Shared = MakeShared<FDerived>();
	TestEqual("Count", Shared.GetSharedReferenceCount(), 1);
	TWeakPtr<FBase> Weak = Shared;
	{
		TSharedPtr<FBase> Pinned = Weak.Pin();
		TestEqual("Pinned count", Pinned.GetSharedReferenceCount(), 2);
	}
	Shared.Reset();
	TestFalse("Weak expires", Weak.Pin().IsValid());

	TSharedRef<FSelfShared> Self = MakeShared<FSelfShared>();
	TSharedRef<FSelfShared> Again = Self->AsShared();
	TestEqual("AsShared", Again.GetSharedReferenceCount(), 2);

	TSharedPtr<FSelfShared, ESPMode::ThreadSafe> Safe = MakeShareable(new FSelfShared());
	TestEqual("ThreadSafe + MakeShareable", Safe->Value, 5);

	int32 Destroyed = 0;
	{
		TSharedPtr<FDestroyCounter> Owner(new FDestroyCounter(&Destroyed));
		TSharedPtr<FDestroyCounter> Copy = Owner;
		Owner.Reset();
		TestEqual("Alive while shared", Destroyed, 0);
	}
	TestEqual("Destroyed with the last reference", Destroyed, 1);

	TSharedRef<FBase> BaseRef = MakeShared<FDerived>();
	TSharedRef<FDerived> DerivedRef = StaticCastSharedRef<FDerived>(BaseRef);
	TestEqual("StaticCastSharedRef", DerivedRef->Id, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFunctionTest, "System.Core.Templates.Function",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFunctionTest::RunTest(const FString& Parameters)
{
	const int32 Captured = 3;
	TFunction<int32(int32)> AddCaptured = [Captured](int32 Value) { return Value + Captured; };
	TestEqual("Call", AddCaptured(4), 7);
	TFunction<int32(int32)> Copy = AddCaptured;
	TestEqual("Copy", Copy(1), 4);

	struct FBig
	{
		int64 Data[16] = {};
	};
	FBig Big;
	Big.Data[15] = 9;
	TFunction<int64()> Heap = [Big]() { return Big.Data[15]; };
	TFunction<int64()> HeapCopy = Heap;
	TestEqual("Heap-stored callable", int64(HeapCopy()), int64(9));

	TUniqueFunction<int32()> UniqueFn = [Owned = MakeUnique<int32>(8)]() { return *Owned; };
	TUniqueFunction<int32()> Moved = MoveTemp(UniqueFn);
	TestEqual("Move-only callable", Moved(), 8);
	TestFalse("Moved-from is unset", UniqueFn.IsSet());

	int32 Counter = 0;
	auto Increment = [&Counter]() { ++Counter; };
	TFunctionRef<void()> Ref = Increment;
	Ref();
	Ref();
	TestEqual("TFunctionRef", Counter, 2);

	TFunction<void()> Empty;
	TestFalse("Default is unset", Empty.IsSet());
	Empty = []() {};
	TestTrue("Assigned", Empty.IsSet());
	Empty = nullptr;
	TestFalse("Reset with nullptr", Empty.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOptionalTest, "System.Core.Templates.Optional",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FOptionalTest::RunTest(const FString& Parameters)
{
	TOptional<int32> Maybe;
	TestFalse("Unset", Maybe.IsSet());
	TestEqual("Default", Maybe.Get(4), 4);
	Maybe = 12;
	TestEqual("Set", *Maybe, 12);
	TOptional<FString> Text(FString("x"));
	TOptional<FString> Copy = Text;
	TestTrue("Copy equals", Copy == Text);
	Text.Reset();
	TestFalse("Reset", Text.IsSet());
	TestNull("GetPtrOrNull", Text.GetPtrOrNull());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
