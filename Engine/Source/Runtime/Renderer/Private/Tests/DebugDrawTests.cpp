#include "CoreMinimal.h"
#include "Debug/DebugDraw.h"
#include "Misc/AutomationTest.h"
#include "ViewMatrices.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugDrawBatchAccumulatesAndClearsTest,
	"System.Renderer.DebugDraw.BatchAccumulatesAndClears",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDebugDrawBatchAccumulatesAndClearsTest::RunTest(const FString& Parameters)
{
	// Lines, boxes and arrows fill the batch and Clear empties it.
	FDebugDraw Draw;
	TestTrue("Starts empty", Draw.IsEmpty());

	Draw.AddLine(FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f), FLinearColor(1.0f, 0.0f, 0.0f));
	TestFalse("Line added", Draw.IsEmpty());

	Draw.Clear();
	TestTrue("Cleared", Draw.IsEmpty());

	Draw.AddAabb(FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f), FLinearColor(0.0f, 1.0f, 0.0f));
	// AABB = 12 edges x 2 vertices
	TestFalse("Box added", Draw.IsEmpty());

	Draw.AddArrow(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f), FLinearColor(0.0f, 0.0f, 1.0f));
	TestFalse("Arrow added", Draw.IsEmpty());

	Draw.Clear();
	TestTrue("Cleared again", Draw.IsEmpty());
	return true;
}

namespace
{
	/** Line Index of the batch runs from A to B in Color. */
	void TestDebugLine(FAutomationTestBase& Test, const TCHAR* What, const FDebugDraw& Draw, int32 Index,
		const FVector& A, const FVector& B, const FLinearColor& Color)
	{
		const TArray<FDebugDraw::FLineVertex>& Vertices = Draw.GetVertices();
		if (!Test.TestTrue(*FString::Printf("%s exists", What), Index >= 0 && (Index * 2) + 1 < Vertices.Num()))
		{
			return;
		}
		const FDebugDraw::FLineVertex& Start = Vertices[Index * 2];
		const FDebugDraw::FLineVertex& End = Vertices[(Index * 2) + 1];
		Test.TestTrue(*FString::Printf("%s start", What), Start.Position.Equals(A, 1.0e-4f));
		Test.TestTrue(*FString::Printf("%s end", What), End.Position.Equals(B, 1.0e-4f));
		const FVector Rgb(Color.R, Color.G, Color.B);
		Test.TestTrue(*FString::Printf("%s color", What), Start.Color.Equals(Rgb) && End.Color.Equals(Rgb));
	}

	/** The index of the batch's line drawn in Color, or INDEX_NONE. */
	[[nodiscard]] int32 FindDebugLine(const FDebugDraw& Draw, const FLinearColor& Color)
	{
		const TArray<FDebugDraw::FLineVertex>& Vertices = Draw.GetVertices();
		for (int32 Index = 0; (Index * 2) < Vertices.Num(); ++Index)
		{
			if (Vertices[Index * 2].Color.Equals(FVector(Color.R, Color.G, Color.B)))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** The gizmo line in Color ends at End (axes at the same view depth may come in either order). */
	void TestGizmoAxis(FAutomationTestBase& Test, const TCHAR* What, const FDebugDraw& Draw, const FVector& End,
		const FLinearColor& Color)
	{
		TestDebugLine(Test, What, Draw, FindDebugLine(Draw, Color), FVector::ZeroVector, End, Color);
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugDrawAxesTest, "System.Renderer.DebugDraw.Axes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDebugDrawAxesTest::RunTest(const FString& Parameters)
{
	// Three lines from the origin, X red, Y green, Z blue (UE's DrawDebugCoordinateSystem).
	FDebugDraw Draw;
	const FVector Origin(10.0f, 20.0f, 30.0f);
	Draw.AddAxes(Origin);
	TestEqual("Three lines", Draw.GetVertices().Num(), 6);
	TestDebugLine(*this, "X", Draw, 0, Origin, Origin + FVector(100.0f, 0.0f, 0.0f), FLinearColor::Red);
	TestDebugLine(*this, "Y", Draw, 1, Origin, Origin + FVector(0.0f, 100.0f, 0.0f), FLinearColor::Green);
	TestDebugLine(*this, "Z", Draw, 2, Origin, Origin + FVector(0.0f, 0.0f, 100.0f), FLinearColor::Blue);

	// A transform draws its rotated unit axes: yaw 90 turns X to +Y and Y to -X (left-handed); scale is ignored.
	Draw.Clear();
	const FTransform Transform(FRotator(0.0f, 90.0f, 0.0f), Origin, FVector(3.0f, 3.0f, 3.0f));
	Draw.AddAxes(Transform, 50.0f);
	TestEqual("Three transform lines", Draw.GetVertices().Num(), 6);
	TestDebugLine(*this, "Yawed X", Draw, 0, Origin, Origin + FVector(0.0f, 50.0f, 0.0f), FLinearColor::Red);
	TestDebugLine(*this, "Yawed Y", Draw, 1, Origin, Origin + FVector(-50.0f, 0.0f, 0.0f), FLinearColor::Green);
	TestDebugLine(*this, "Yawed Z", Draw, 2, Origin, Origin + FVector(0.0f, 0.0f, 50.0f), FLinearColor::Blue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugDrawViewAxesTest, "System.Renderer.DebugDraw.ViewAxes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDebugDrawViewAxesTest::RunTest(const FString& Parameters)
{
	// The corner gizmo projects the world axes through the view rotation only; the camera location is ignored.
	const FVector Eye(-500.0f, 250.0f, 180.0f);
	FDebugDraw Draw;

	// Looking along +X: X points into the screen (drawn first), Y to the right, Z up.
	Draw.AddViewAxes(MakeViewMatrix(Eye, FRotator(0.0f, 0.0f, 0.0f)));
	TestEqual("Three lines", Draw.GetVertices().Num(), 6);
	TestEqual("Forward: X farthest", FindDebugLine(Draw, FLinearColor::Red), 0);
	TestGizmoAxis(*this, "Forward X", Draw, FVector::ZeroVector, FLinearColor::Red);
	TestGizmoAxis(*this, "Forward Y", Draw, FVector(0.8f, 0.0f, 0.0f), FLinearColor::Green);
	TestGizmoAxis(*this, "Forward Z", Draw, FVector(0.0f, 0.8f, 0.0f), FLinearColor::Blue);

	// Looking down from above: X up the screen, Y to its right (left-handed), Z at the viewer (drawn last).
	Draw.Clear();
	Draw.AddViewAxes(MakeViewMatrix(Eye, FRotator(-90.0f, 0.0f, 0.0f)), 1.0f);
	TestEqual("Top: Z nearest", FindDebugLine(Draw, FLinearColor::Blue), 2);
	TestGizmoAxis(*this, "Top X", Draw, FVector(0.0f, 1.0f, 0.0f), FLinearColor::Red);
	TestGizmoAxis(*this, "Top Y", Draw, FVector(1.0f, 0.0f, 0.0f), FLinearColor::Green);
	TestGizmoAxis(*this, "Top Z", Draw, FVector::ZeroVector, FLinearColor::Blue);

	// Yaw 90 looks along +Y: Y goes into the screen and X points left.
	Draw.Clear();
	Draw.AddViewAxes(MakeViewMatrix(Eye, FRotator(0.0f, 90.0f, 0.0f)), 1.0f);
	TestEqual("Yawed: Y farthest", FindDebugLine(Draw, FLinearColor::Green), 0);
	TestGizmoAxis(*this, "Yawed X", Draw, FVector(-1.0f, 0.0f, 0.0f), FLinearColor::Red);
	TestGizmoAxis(*this, "Yawed Y", Draw, FVector::ZeroVector, FLinearColor::Green);
	TestGizmoAxis(*this, "Yawed Z", Draw, FVector(0.0f, 1.0f, 0.0f), FLinearColor::Blue);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
