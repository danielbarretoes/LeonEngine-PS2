#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ViewMatrices.h"
#include "VisibilityCells.h"

#if WITH_DEV_AUTOMATION_TESTS

// Cells and portals (Docs/PLANS/ps2-shipping.md N15): three rooms in a row along X, 10 m each, joined by doorways; what
// an eye in the first room sees through them.

namespace
{

	/** Rooms A (x 0 to 10 m), B (10 to 20 m) and C (20 to 30 m), 10 m wide (y 0 to 10 m) and 3 m high. */
	void AddRooms(FVisibilityCellGraph& Graph)
	{
		for (int32 Room = 0; Room < 3; ++Room)
		{
			const float X = float(Room) * 1000.0f;
			const TCHAR* Names[3] = {TEXT("A"), TEXT("B"), TEXT("C")};
			Graph.AddCell(FName(Names[Room]), FBox(FVector(X, 0.0f, 0.0f), FVector(X + 1000.0f, 1000.0f, 300.0f)));
		}
	}

	/** A doorway 1 m wide and 2 m high in the wall at X, centred at Y. */
	void AddDoor(FVisibilityCellGraph& Graph, const TCHAR* From, const TCHAR* To, float X, float Y)
	{
		const FVector Corners[4] = {FVector(X, Y - 50.0f, 0.0f), FVector(X, Y + 50.0f, 0.0f),
			FVector(X, Y + 50.0f, 200.0f), FVector(X, Y - 50.0f, 200.0f)};
		Graph.AddPortal(Graph.FindCell(FName(From)), Graph.FindCell(FName(To)), Corners);
	}

	/** An eye's view through UE's perspective (90 degrees, near 10 cm): world to clip space, w the depth. */
	FMatrix MakeViewProjection(const FVector& Eye, const FVector& Target)
	{
		const FMatrix View = MakeLookAtView(Eye, Target, FVector(0.0f, 0.0f, 1.0f));
		const float HalfFov = FMath::DegreesToRadians(90.0f) * 0.5f;
		return View * FMatrix(FPerspectiveMatrix(HalfFov, HalfFov, 1.0f, 1.0f, 10.0f, 100000.0f));
	}

	[[nodiscard]] bool Sees(const FVisibilityCellGraph::FVisibleCells& Visible, int32 Cell)
	{
		return (Visible.Mask >> uint32(Cell)) & 1u;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibilityCellsTraversalTest, "System.RenderCore.VisibilityCells.Traversal",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FVisibilityCellsTraversalTest::RunTest(const FString& Parameters)
{
	// Doors in line: from A, looking down the rooms, B and C are seen through both doors.
	FVisibilityCellGraph Graph;
	AddRooms(Graph);
	AddDoor(Graph, TEXT("A"), TEXT("B"), 1000.0f, 500.0f);
	AddDoor(Graph, TEXT("B"), TEXT("C"), 2000.0f, 500.0f);
	const FVector Eye(100.0f, 500.0f, 100.0f);
	FVisibilityCellGraph::FVisibleCells Visible;
	Graph.FindVisibleCells(MakeViewProjection(Eye, FVector(3000.0f, 500.0f, 100.0f)), Eye, Visible);
	TestEqual("The eye's cell", Visible.EyeCell, 0);
	TestTrue("A, B and C through the doors", Sees(Visible, 0) && Sees(Visible, 1) && Sees(Visible, 2));
	TestTrue("B through the first door only: narrowed",
		Visible.Rects[1].GetSize().X < 0.2f && Visible.Rects[1].GetSize().X > 0.0f);
	TestTrue("C through both: narrower still", Visible.Rects[2].GetSize().X <= Visible.Rects[1].GetSize().X);

	// Looking back: the doors are behind the eye; only its own room.
	Graph.FindVisibleCells(MakeViewProjection(Eye, FVector(-1000.0f, 500.0f, 100.0f)), Eye, Visible);
	TestTrue("Looking away: A only", Sees(Visible, 0) && !Sees(Visible, 1) && !Sees(Visible, 2));
	TestEqual("A's door tested, not walked through", Visible.NumPortalsTested, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibilityCellsNarrowingTest, "System.RenderCore.VisibilityCells.Narrowing",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FVisibilityCellsNarrowingTest::RunTest(const FString& Parameters)
{
	// The second door 4 m aside: C is in the view's frustum, but no line through the first door reaches the second, so
	// the narrowed rectangle leaves C out. From beside the second door's line, through the first door's side, C shows.
	FVisibilityCellGraph Graph;
	AddRooms(Graph);
	AddDoor(Graph, TEXT("A"), TEXT("B"), 1000.0f, 500.0f);
	AddDoor(Graph, TEXT("B"), TEXT("C"), 2000.0f, 900.0f);
	const FVector Eye(100.0f, 500.0f, 100.0f);
	FVisibilityCellGraph::FVisibleCells Visible;
	Graph.FindVisibleCells(MakeViewProjection(Eye, FVector(3000.0f, 500.0f, 100.0f)), Eye, Visible);
	TestTrue("B through the door", Sees(Visible, 1));
	TestFalse("C: its door is out of the first door's rectangle", Sees(Visible, 2));

	// An eye on the line of both doors (through the first door's edge to the second).
	const FVector Aside(100.0f, 110.0f, 100.0f);
	Graph.FindVisibleCells(MakeViewProjection(Aside, FVector(2000.0f, 900.0f, 100.0f)), Aside, Visible);
	TestTrue("C on the line through both doors", Sees(Visible, 1) && Sees(Visible, 2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibilityCellsEdgeCasesTest, "System.RenderCore.VisibilityCells.EdgeCases",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FVisibilityCellsEdgeCasesTest::RunTest(const FString& Parameters)
{
	// An eye in no cell sees everything; an eye in a doorway looking along the wall still sees the next room (the near
	// plane cuts the door's quad away); a primitive's cells are the cells its box touches, none outside them all.
	FVisibilityCellGraph Graph;
	AddRooms(Graph);
	AddDoor(Graph, TEXT("A"), TEXT("B"), 1000.0f, 500.0f);
	FVisibilityCellGraph::FVisibleCells Visible;
	const FVector Above(500.0f, 500.0f, 1000.0f);
	Graph.FindVisibleCells(MakeViewProjection(Above, FVector(500.0f, 500.0f, 0.0f)), Above, Visible);
	TestEqual("Above the map: no cell", Visible.EyeCell, INDEX_NONE);
	TestTrue("Everything", Visible.Mask == ~uint64(0));
	TestTrue("A primitive of no cell is seen", FVisibilityCellGraph::IsVisible(0, Visible));

	const FVector Doorway(995.0f, 500.0f, 100.0f);
	Graph.FindVisibleCells(MakeViewProjection(Doorway, FVector(995.0f, 1000.0f, 100.0f)), Doorway, Visible);
	TestTrue("In the doorway: both rooms", Sees(Visible, 0) && Sees(Visible, 1));

	TestEqual("A box in A", Graph.GetCellMask(FBox(FVector(100.0f), FVector(200.0f))), uint64(1));
	TestEqual("A box across the door",
		Graph.GetCellMask(FBox(FVector(950.0f, 450.0f, 0.0f), FVector(1050.0f, 550.0f, 100.0f))), uint64(3));
	TestEqual("A box outside", Graph.GetCellMask(FBox(FVector(-500.0f), FVector(-400.0f))), uint64(0));
	TestEqual("The smallest cell holding a point", Graph.FindCellAt(FVector(1500.0f, 500.0f, 100.0f)), 1);
	TestEqual("A cell's name once", Graph.AddCell(FName(TEXT("A")), FBox(FVector(0.0f), FVector(1.0f))), INDEX_NONE);
	const FVector Corners[4] = {FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector};
	TestFalse("A portal to no cell", Graph.AddPortal(0, 7, Corners));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
