#include "CoreMinimal.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/MallocBinned.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Misc/MemStack.h"

#if WITH_DEV_AUTOMATION_TESTS

// FMallocBinned, the LLM tags and budgets, FMemStack (Docs/PLANS/ps2-shipping.md N17). The allocator tests use an
// allocator of their own (a small arena, no tags), so its numbers are exact; the tag tests go through GMalloc.

namespace
{
	/** The arena of the tests' allocators: 64 pages. */
	constexpr SIZE_T TestArenaSize = 64 * FMallocBinned::PageSize;

	[[nodiscard]] uint64 UsableSize(FMallocBinned& Malloc, void* Block)
	{
		SIZE_T Size = 0;
		return Malloc.GetAllocationSize(Block, Size) ? uint64(Size) : 0;
	}

	[[nodiscard]] bool IsBlockAligned(const void* Block, uint32 Alignment)
	{
		return (reinterpret_cast<UPTRINT>(Block) & (Alignment - 1)) == 0;
	}

	/** Fills a block with a pattern of its seed. */
	void FillPattern(void* Block, SIZE_T Size, uint32 Seed)
	{
		uint8* Bytes = static_cast<uint8*>(Block);
		for (SIZE_T Index = 0; Index < Size; ++Index)
		{
			Bytes[Index] = uint8((Seed * 131u + uint32(Index) * 7u) >> 1);
		}
	}

	[[nodiscard]] bool HasPattern(const void* Block, SIZE_T Size, uint32 Seed)
	{
		const uint8* Bytes = static_cast<const uint8*>(Block);
		for (SIZE_T Index = 0; Index < Size; ++Index)
		{
			if (Bytes[Index] != uint8((Seed * 131u + uint32(Index) * 7u) >> 1))
			{
				return false;
			}
		}
		return true;
	}

	/** The budget events a test hook saw. */
	struct FBudgetEvents
	{
		int32 NumWarnings = 0;
		int32 NumExceeded = 0;
		/** The tracker's static name: the hook allocates nothing. */
		const TCHAR* LastTag = nullptr;
		uint64 LastBudget = 0;
	};
	FBudgetEvents GBudgetEvents;

	void RecordBudgetEvent(ELLMBudgetEvent Event, const TCHAR* TagName, uint64 /*CurrentBytes*/, uint64 BudgetBytes)
	{
		if (Event == ELLMBudgetEvent::Warning)
		{
			++GBudgetEvents.NumWarnings;
		}
		else
		{
			++GBudgetEvents.NumExceeded;
		}
		GBudgetEvents.LastTag = TagName;
		GBudgetEvents.LastBudget = BudgetBytes;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallocBinnedSizeClassesTest, "System.Core.Memory.Binned.SizeClasses",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMallocBinnedSizeClassesTest::RunTest(const FString& Parameters)
{
	FMallocBinned Malloc(TestArenaSize, false);
	// Each request gets its class: 16 bytes apart up to 1024, then the system heap with the size asked for.
	const SIZE_T Requests[] = {0, 1, 15, 16, 17, 32, 33, 1000, 1008, 1009, 1023, 1024, 1025, 4000};
	const uint64 Expected[] = {16, 16, 16, 16, 32, 32, 48, 1008, 1008, 1024, 1024, 1024, 1025, 4000};
	for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Requests)); ++Index)
	{
		void* Block = Malloc.Malloc(Requests[Index]);
		const FString What = FString::Printf("%d bytes", int32(Requests[Index]));
		TestEqual(*(What + " usable size"), UsableSize(Malloc, Block), Expected[Index]);
		TestEqual(*(What + " quantized"), uint64(Malloc.QuantizeSize(Requests[Index], DEFAULT_ALIGNMENT)),
			Requests[Index] == 0 ? uint64(16) : Expected[Index]);
		TestEqual(*(What + " from the arena"), Malloc.IsSmallBlock(Block), Expected[Index] <= 1024);
		TestTrue(*(What + " 16-byte aligned"), IsBlockAligned(Block, 16));
		Malloc.Free(Block);
	}
	FMallocSizeClassStats Classes[FMallocBinned::NumSizeClasses];
	TestEqual("Size classes", Malloc.GetSizeClassStats(Classes, FMallocBinned::NumSizeClasses),
		FMallocBinned::NumSizeClasses);
	TestEqual("First class", Classes[0].BlockSize, 16u);
	TestEqual("Last class", Classes[FMallocBinned::NumSizeClasses - 1].BlockSize, 1024u);
	TestEqual("Every block back", Malloc.GetUsage().NumAllocations, uint64(0));
	TestEqual("Every page back", Malloc.GetUsage().ArenaUsedBytes, uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallocBinnedAlignmentTest, "System.Core.Memory.Binned.Alignment",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMallocBinnedAlignmentTest::RunTest(const FString& Parameters)
{
	FMallocBinned Malloc(TestArenaSize, false);
	const uint32 Alignments[] = {8, 16, 32, 64, 128, 256, 1024, 4096};
	const SIZE_T Sizes[] = {1, 24, 100, 700, 3000};
	for (const uint32 Alignment : Alignments)
	{
		for (const SIZE_T Size : Sizes)
		{
			void* Blocks[3];
			for (void*& Block : Blocks)
			{
				Block = Malloc.Malloc(Size, Alignment);
			}
			const FString What = FString::Printf("%d bytes aligned to %u", int32(Size), Alignment);
			for (void* Block : Blocks)
			{
				TestTrue(*What, IsBlockAligned(Block, Alignment < 16 ? 16 : Alignment));
				TestTrue(*(What + ": usable size"), UsableSize(Malloc, Block) >= Size);
			}
			// A small block of a larger alignment takes a class that is a multiple of it.
			if (Alignment > 16 && Alignment <= FMallocBinned::MaxSmallBlockSize && Size <= Alignment)
			{
				TestEqual(*(What + ": class"), UsableSize(Malloc, Blocks[0]), uint64(Alignment));
			}
			Blocks[1] = Malloc.Realloc(Blocks[1], Size * 3 + 5, Alignment);
			TestTrue(*(What + ": grown"), IsBlockAligned(Blocks[1], Alignment < 16 ? 16 : Alignment));
			for (void* Block : Blocks)
			{
				Malloc.Free(Block);
			}
		}
	}
	TestEqual("Every block back", Malloc.GetUsage().CurrentBytes, uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallocBinnedReallocTest, "System.Core.Memory.Binned.Realloc",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMallocBinnedReallocTest::RunTest(const FString& Parameters)
{
	FMallocBinned Malloc(TestArenaSize, false);
	void* Block = Malloc.Realloc(nullptr, 20);
	TestEqual("Realloc(nullptr) allocates", UsableSize(Malloc, Block), uint64(32));
	FillPattern(Block, 20, 1);
	void* Same = Malloc.Realloc(Block, 30);
	TestTrue("The same class keeps the block", Same == Block);
	void* Grown = Malloc.Realloc(Same, 600);
	TestTrue("Grown into another class", Grown != Same && UsableSize(Malloc, Grown) == 608);
	TestTrue("Grown keeps the contents", HasPattern(Grown, 20, 1));
	FillPattern(Grown, 600, 2);
	void* Large = Malloc.Realloc(Grown, 5000);
	TestTrue("Into the system heap", !Malloc.IsSmallBlock(Large));
	TestTrue("Large keeps the contents", HasPattern(Large, 600, 2));
	FillPattern(Large, 5000, 3);
	void* Kept = Malloc.Realloc(Large, 4000);
	TestTrue("A large block shrinking by less than half stays", Kept == Large);
	void* Small = Malloc.Realloc(Kept, 100);
	TestTrue("Back into the arena", Malloc.IsSmallBlock(Small));
	TestTrue("Shrunk keeps the contents", HasPattern(Small, 100, 3));
	TestTrue("Realloc to zero frees", Malloc.Realloc(Small, 0) == nullptr);
	TestEqual("Every block back", Malloc.GetUsage().NumAllocations, uint64(0));
	TestEqual("Allocations counted (the moves too)", Malloc.GetUsage().TotalAllocations, uint64(4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallocBinnedStatsTest, "System.Core.Memory.Binned.Stats",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMallocBinnedStatsTest::RunTest(const FString& Parameters)
{
	FMallocBinned Malloc(TestArenaSize, false);
	TestEqual("Arena", Malloc.GetUsage().ArenaBytes, uint64(TestArenaSize));
	// 300 blocks of 48 bytes: 85 to a page, so 4 pages.
	TArray<void*> Blocks;
	for (int32 Index = 0; Index < 300; ++Index)
	{
		Blocks.Add(Malloc.Malloc(40));
	}
	void* Large = Malloc.Malloc(10000);
	FMallocUsage Usage = Malloc.GetUsage();
	TestEqual("Current", Usage.CurrentBytes, uint64(300 * 48 + 10000));
	TestEqual("Peak", Usage.PeakBytes, uint64(300 * 48 + 10000));
	TestEqual("Live", Usage.NumAllocations, uint64(301));
	TestEqual("Pages in use", Usage.ArenaUsedBytes, uint64(4 * FMallocBinned::PageSize));
	FMallocSizeClassStats Classes[FMallocBinned::NumSizeClasses];
	(void)Malloc.GetSizeClassStats(Classes, FMallocBinned::NumSizeClasses);
	TestEqual("Class 48: blocks", Classes[2].CurrentBlocks, 300u);
	TestEqual("Class 48: pages", Classes[2].NumPages, 4u);
	TestEqual("Class 16: nothing", Classes[0].CurrentBlocks, 0u);

	// Freeing every other block empties no page; freeing the rest gives the pages back.
	for (int32 Index = 0; Index < Blocks.Num(); Index += 2)
	{
		Malloc.Free(Blocks[Index]);
	}
	TestEqual("Half the blocks", Malloc.GetUsage().CurrentBytes, uint64(150 * 48 + 10000));
	TestEqual("Pages kept", Malloc.GetUsage().ArenaUsedBytes, uint64(4 * FMallocBinned::PageSize));
	for (int32 Index = 1; Index < Blocks.Num(); Index += 2)
	{
		Malloc.Free(Blocks[Index]);
	}
	Malloc.Free(Large);
	Usage = Malloc.GetUsage();
	TestEqual("Current after", Usage.CurrentBytes, uint64(0));
	TestEqual("Peak kept", Usage.PeakBytes, uint64(300 * 48 + 10000));
	TestEqual("Pages back", Usage.ArenaUsedBytes, uint64(0));
	TestEqual("Peak pages", Usage.ArenaPeakBytes, uint64(4 * FMallocBinned::PageSize));
	(void)Malloc.GetSizeClassStats(Classes, FMallocBinned::NumSizeClasses);
	TestEqual("Class 48: peak", Classes[2].PeakBlocks, 300u);
	TestEqual("Class 48: no page", Classes[2].NumPages, 0u);

	// A freed page serves any class: another class takes one of the same pages again.
	void* Reused = Malloc.Malloc(1024);
	const UPTRINT ReusedPage = reinterpret_cast<UPTRINT>(Reused) & ~UPTRINT(FMallocBinned::PageSize - 1);
	bool bSamePage = false;
	for (void* Block : Blocks)
	{
		bSamePage |= (reinterpret_cast<UPTRINT>(Block) & ~UPTRINT(FMallocBinned::PageSize - 1)) == ReusedPage;
	}
	TestTrue("A freed page is reused", bSamePage);
	Malloc.Free(Reused);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallocBinnedOverflowTest, "System.Core.Memory.Binned.ArenaOverflow",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMallocBinnedOverflowTest::RunTest(const FString& Parameters)
{
	// Two pages: 8 blocks of 1024 bytes; the next ones go to the system heap.
	FMallocBinned Malloc(2 * FMallocBinned::PageSize, false);
	void* Blocks[12];
	for (int32 Index = 0; Index < 12; ++Index)
	{
		Blocks[Index] = Malloc.Malloc(1024);
		FillPattern(Blocks[Index], 1024, uint32(Index));
	}
	TestTrue("The arena first", Malloc.IsSmallBlock(Blocks[7]));
	TestTrue("Then the system heap", !Malloc.IsSmallBlock(Blocks[8]));
	TestEqual("Overflows counted", Malloc.GetUsage().ArenaOverflows, uint64(4));
	TestEqual("An overflow counts its class size", UsableSize(Malloc, Blocks[11]), uint64(1024));
	for (int32 Index = 0; Index < 12; ++Index)
	{
		TestTrue("Contents", HasPattern(Blocks[Index], 1024, uint32(Index)));
		Malloc.Free(Blocks[Index]);
	}
	TestEqual("Every block back", Malloc.GetUsage().CurrentBytes, uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallocBinnedStressTest, "System.Core.Memory.Binned.Stress",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMallocBinnedStressTest::RunTest(const FString& Parameters)
{
	// Random allocations, frees and reallocs of every size class, some large and some aligned, each block filled with
	// its own pattern; the patterns are checked when the blocks go, and a checksum of the run's choices must come out
	// the same on every platform (the same seed makes the same calls).
	FMallocBinned Malloc(TestArenaSize, false);
	struct FLiveBlock
	{
		void* Block;
		SIZE_T Size;
		uint32 Seed;
	};
	TArray<FLiveBlock> Live;
	FRandomStream Random(1234);
	uint32 Checksum = 0;
	bool bContentsKept = true;
	for (int32 Step = 0; Step < 20000; ++Step)
	{
		const int32 Action = Random.RandHelper(10);
		if (Action < 5 || Live.Num() == 0)
		{
			const bool bLarge = Random.RandHelper(20) == 0;
			const SIZE_T Size = SIZE_T(bLarge ? Random.RandRange(1025, 9000) : Random.RandRange(0, 1024));
			const uint32 Alignment = Random.RandHelper(8) == 0 ? 64u : 0u;
			FLiveBlock Block{Malloc.Malloc(Size, Alignment), Size, uint32(Step)};
			FillPattern(Block.Block, Size, Block.Seed);
			Live.Add(Block);
			Checksum = Checksum * 31u + uint32(Size);
		}
		else if (Action < 9)
		{
			const int32 Index = Random.RandHelper(Live.Num());
			bContentsKept &= HasPattern(Live[Index].Block, Live[Index].Size, Live[Index].Seed);
			Malloc.Free(Live[Index].Block);
			Checksum = Checksum * 31u + uint32(Index);
			Live.RemoveAtSwap(Index);
		}
		else
		{
			const int32 Index = Random.RandHelper(Live.Num());
			FLiveBlock& Block = Live[Index];
			const SIZE_T NewSize = SIZE_T(Random.RandRange(1, 3000));
			Block.Block = Malloc.Realloc(Block.Block, NewSize);
			const SIZE_T Kept = NewSize < Block.Size ? NewSize : Block.Size;
			bContentsKept &= HasPattern(Block.Block, Kept, Block.Seed);
			Block.Size = NewSize;
			Block.Seed = uint32(Step);
			FillPattern(Block.Block, NewSize, Block.Seed);
			Checksum = Checksum * 31u + uint32(NewSize);
		}
	}
	for (const FLiveBlock& Block : Live)
	{
		bContentsKept &= HasPattern(Block.Block, Block.Size, Block.Seed);
		Malloc.Free(Block.Block);
	}
	TestTrue("Every block kept its contents", bContentsKept);
	TestEqual("The run's checksum", Checksum, 697405103u);
	const FMallocUsage Usage = Malloc.GetUsage();
	TestEqual("Every byte back", Usage.CurrentBytes, uint64(0));
	TestEqual("Every block back", Usage.NumAllocations, uint64(0));
	TestEqual("Every page back", Usage.ArenaUsedBytes, uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLowLevelMemTrackerTagsTest, "System.Core.Memory.LLM.Tags",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLowLevelMemTrackerTagsTest::RunTest(const FString& Parameters)
{
	if (!FLowLevelMemTracker::IsEnabled())
	{
		return true;
	}
	TestEqual("Outside a scope", int32(FLowLevelMemTracker::GetActiveTag()), int32(ELLMTag::EngineMisc));
	TestEqual("Tag name", FLowLevelMemTracker::GetTagName(ELLMTag::SceneRender), TEXT("SceneRender"));
	TestEqual("Total's name", FLowLevelMemTracker::GetTagName(ELLMTag::Count), TEXT("Total"));
	const FLLMTagStats PhysicsBefore = FLowLevelMemTracker::GetTagStats(ELLMTag::Physics);
	const FLLMTagStats AIBefore = FLowLevelMemTracker::GetTagStats(ELLMTag::AI);
	const FLLMTagStats TotalBefore = FLowLevelMemTracker::GetTagStats(ELLMTag::Count);
	void* PhysicsBlock = nullptr;
	void* AIBlock = nullptr;
	{
		LLM_SCOPE(ELLMTag::Physics);
		PhysicsBlock = FMemory::Malloc(1000);
		{
			LLM_SCOPE(ELLMTag::AI);
			TestEqual("The innermost scope", int32(FLowLevelMemTracker::GetActiveTag()), int32(ELLMTag::AI));
			AIBlock = FMemory::Malloc(5000);
		}
		TestEqual("Back to the outer scope", int32(FLowLevelMemTracker::GetActiveTag()), int32(ELLMTag::Physics));
	}
	TestEqual("Scope closed", int32(FLowLevelMemTracker::GetActiveTag()), int32(ELLMTag::EngineMisc));
	TestEqual("Physics charged its class",
		FLowLevelMemTracker::GetTagStats(ELLMTag::Physics).CurrentBytes - PhysicsBefore.CurrentBytes, uint64(1008));
	TestEqual(
		"AI charged", FLowLevelMemTracker::GetTagStats(ELLMTag::AI).CurrentBytes - AIBefore.CurrentBytes, uint64(5000));
	TestEqual("Total charged", FLowLevelMemTracker::GetTagStats(ELLMTag::Count).CurrentBytes - TotalBefore.CurrentBytes,
		uint64(6008));
	TestEqual("Allocations counted",
		FLowLevelMemTracker::GetTagStats(ELLMTag::AI).TotalAllocations - AIBefore.TotalAllocations, uint64(1));
	// A block is given back to its own tag, whatever the scope that frees it.
	{
		LLM_SCOPE(ELLMTag::Audio);
		FMemory::Free(PhysicsBlock);
		FMemory::Free(AIBlock);
	}
	TestEqual(
		"Physics back", FLowLevelMemTracker::GetTagStats(ELLMTag::Physics).CurrentBytes, PhysicsBefore.CurrentBytes);
	TestEqual("AI back", FLowLevelMemTracker::GetTagStats(ELLMTag::AI).CurrentBytes, AIBefore.CurrentBytes);
	TestTrue("Peak kept", FLowLevelMemTracker::GetTagStats(ELLMTag::AI).PeakBytes >= AIBefore.CurrentBytes + 5000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLowLevelMemTrackerBudgetsTest, "System.Core.Memory.LLM.Budgets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLowLevelMemTrackerBudgetsTest::RunTest(const FString& Parameters)
{
	if (!FLowLevelMemTracker::IsEnabled())
	{
		return true;
	}
	// A budget well over what the tag holds: 1 000 bytes under its 90 % nothing happens, 500 over it warns, over the
	// budget is exceeded (large blocks: their exact sizes are charged). The hook takes the events: without it the
	// warning is logged and the excess is a fatal error.
	const ELLMTag Tag = ELLMTag::Animation;
	const FLLMTagStats Before = FLowLevelMemTracker::GetTagStats(Tag);
	const uint64 Base = Before.CurrentBytes;
	const uint64 Budget = Base * 2 + 20000;
	const uint64 Warning = Budget / 100 * 90 + Budget % 100 * 90 / 100;
	GBudgetEvents = FBudgetEvents();
	const FLowLevelMemTracker::FBudgetHook PreviousHook = FLowLevelMemTracker::SetBudgetHook(&RecordBudgetEvent);
	const int32 PreviousPercent = FLowLevelMemTracker::GetWarningPercent();
	FLowLevelMemTracker::SetWarningPercent(90);
	FLowLevelMemTracker::SetBudget(Tag, Budget);
	void* First = nullptr;
	void* Second = nullptr;
	void* Third = nullptr;
	{
		LLM_SCOPE(Tag);
		First = FMemory::Malloc(SIZE_T(Warning - Base - 1000));
		TestEqual("Under 90 %: nothing", GBudgetEvents.NumWarnings + GBudgetEvents.NumExceeded, 0);
		Second = FMemory::Malloc(1500);
		TestEqual("Over 90 %: a warning", GBudgetEvents.NumWarnings, 1);
		TestEqual("The warning names the tag", GBudgetEvents.LastTag, TEXT("Animation"));
		TestEqual("Its budget", GBudgetEvents.LastBudget, Budget);
		Third = FMemory::Malloc(SIZE_T(Budget - Warning) + 1000);
		TestEqual("Over the budget: exceeded", GBudgetEvents.NumExceeded, 1);
		TestEqual("Warned once", GBudgetEvents.NumWarnings, 1);
		FMemory::Free(FMemory::Malloc(100));
		TestEqual("Reported once", GBudgetEvents.NumExceeded, 1);
	}
	FMemory::Free(First);
	FMemory::Free(Second);
	FMemory::Free(Third);

	// The total's budget names Total.
	GBudgetEvents = FBudgetEvents();
	const uint64 PreviousTotalBudget = FLowLevelMemTracker::GetTagStats(ELLMTag::Count).BudgetBytes;
	FLowLevelMemTracker::SetBudget(
		ELLMTag::Count, FLowLevelMemTracker::GetTagStats(ELLMTag::Count).CurrentBytes + 2000);
	void* Big = FMemory::Malloc(4000);
	TestEqual("Total exceeded", GBudgetEvents.NumExceeded, 1);
	TestEqual("Named Total", GBudgetEvents.LastTag, TEXT("Total"));
	FMemory::Free(Big);

	FLowLevelMemTracker::SetBudget(Tag, Before.BudgetBytes);
	FLowLevelMemTracker::SetBudget(ELLMTag::Count, PreviousTotalBudget);
	FLowLevelMemTracker::SetWarningPercent(PreviousPercent);
	FLowLevelMemTracker::SetBudgetHook(PreviousHook);
	TestEqual("The tag's bytes back", FLowLevelMemTracker::GetTagStats(Tag).CurrentBytes, Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemStackMarksTest, "System.Core.Memory.MemStack.Marks",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMemStackMarksTest::RunTest(const FString& Parameters)
{
	FMemStackBase Stack(1024);
	TestTrue("Empty", Stack.IsEmpty());
	{
		FMemMark Outer(Stack);
		uint8* A = Stack.PushBytes(100, 16);
		TestTrue("16-byte aligned", IsBlockAligned(A, 16));
		uint8* B = Stack.PushBytes(10, 64);
		TestTrue("64-byte aligned", IsBlockAligned(B, 64));
		TestTrue("After the first", B >= A + 100);
		const SIZE_T AfterTwo = Stack.GetByteCount();
		{
			FMemMark Inner(Stack);
			TestEqual("Two marks", Stack.GetNumMarks(), 2);
			// More than the chunk holds: a new chunk, then one of its own for a large request.
			for (int32 Index = 0; Index < 20; ++Index)
			{
				FMemory::Memset(Stack.PushBytes(100, 16), 0xAB, 100);
			}
			uint8* Large = Stack.PushBytes(5000, 16);
			TestTrue("A large request fits", Stack.ContainsPointer(Large) && Stack.ContainsPointer(Large + 4999));
		}
		TestEqual("The inner mark popped back", uint64(Stack.GetByteCount()), uint64(AfterTwo));
		TestTrue("The first push survives", Stack.ContainsPointer(A));
	}
	TestTrue("Empty after the outer mark", Stack.IsEmpty());
	TestTrue("Peak recorded", Stack.GetPeakByteCount() > 5000);
	const int32 Chunks = Stack.GetNumChunkAllocations();
	const SIZE_T ChunkBytes = Stack.GetChunkBytes();
	TestTrue("Chunks kept", Chunks >= 3 && ChunkBytes >= 5000 + 2048);
	{
		// The same pushes again take no memory from the heap.
		FMemMark Again(Stack);
		for (int32 Index = 0; Index < 22; ++Index)
		{
			(void)Stack.PushBytes(100, 16);
		}
		(void)Stack.PushBytes(5000, 16);
	}
	TestEqual("Kept chunks reused", Stack.GetNumChunkAllocations(), Chunks);
	struct FPoint
	{
		int32 X;
		int32 Y;
	};
	const FPoint* Point = new (Stack) FPoint{3, 4};
	TestTrue("operator new", Stack.ContainsPointer(Point) && Point->Y == 4);
	Stack.EndFrame();
	TestTrue("The frame's end pops everything", Stack.IsEmpty());
	TestEqual("And keeps the chunks", uint64(Stack.GetChunkBytes()), uint64(ChunkBytes));
	Stack.Flush();
	TestEqual("Flush gives them back", uint64(Stack.GetChunkBytes()), uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemStackContainersTest, "System.Core.Memory.MemStack.Containers",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMemStackContainersTest::RunTest(const FString& Parameters)
{
	FMemStack& Stack = FMemStack::Get();
	const int32 ContainersBefore = Stack.GetNumContainerAllocations();
	const uint64 HeapBefore = FMemory::GetUsage().TotalAllocations;
	{
		FMemMark Mark(Stack);
		TArray<int32, TMemStackAllocator<>> Values;
		for (int32 Index = 0; Index < 1000; ++Index)
		{
			Values.Add(Index * 3);
		}
		TestEqual("Grown on the stack", Values[999], 2997);
		TestTrue("On the stack", Stack.ContainsPointer(Values.GetData()));
		TestEqual("Counted while it holds memory", Stack.GetNumContainerAllocations(), ContainersBefore + 1);
		Values.Empty();
		TestEqual("Not after Empty", Stack.GetNumContainerAllocations(), ContainersBefore);
		Values.Add(7);
		TArray<int32, TMemStackAllocator<>> Copy = Values;
		TestEqual("Copied", Copy[0], 7);
		TestEqual("Two holding memory", Stack.GetNumContainerAllocations(), ContainersBefore + 2);
	}
	TestEqual("None after the mark", Stack.GetNumContainerAllocations(), ContainersBefore);
	{
		// Warm: the same work again takes nothing from the heap.
		FMemMark Mark(Stack);
		TArray<int32, TMemStackAllocator<>> Values;
		for (int32 Index = 0; Index < 1000; ++Index)
		{
			Values.Add(Index);
		}
	}
	const uint64 HeapWarm = FMemory::GetUsage().TotalAllocations;
	{
		FMemMark Mark(Stack);
		TArray<int32, TMemStackAllocator<>> Values;
		for (int32 Index = 0; Index < 1000; ++Index)
		{
			Values.Add(Index);
		}
	}
	TestTrue("The stack took chunks at most once", HeapWarm - HeapBefore <= 4);
	TestEqual("No heap allocation once warm", FMemory::GetUsage().TotalAllocations, HeapWarm);
	return true;
}

#endif
