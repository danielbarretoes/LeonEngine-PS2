#include "CollisionShape.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHalfExtentsFromScaleTest, "System.PhysicsCore.Collision.HalfExtentsFromScale",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FHalfExtentsFromScaleTest::RunTest(const FString& Parameters)
{
	// The half extents are the absolute half scale of a 100 cm basic cube.
	float Hx = 0.0f;
	float Hy = 0.0f;
	float Hz = 0.0f;
	HalfExtentsFromScale(FVector(2.0f, -4.0f, 6.0f), Hx, Hy, Hz);
	TestEqual("HalfX", Hx, 100.0f, 1.0e-3f);
	TestEqual("HalfY", Hy, 200.0f, 1.0e-3f);
	TestEqual("HalfZ", Hz, 300.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMassFromHalfExtentsTest, "System.PhysicsCore.Collision.MassFromHalfExtents",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMassFromHalfExtentsTest::RunTest(const FString& Parameters)
{
	// Tiny volumes get a floor; the mass is the volume in cubic metres (half extents in cm).
	TestTrue("Floor", MassFromHalfExtents(1.0f, 1.0f, 1.0f) >= 0.08f);
	TestEqual("2 m cube", MassFromHalfExtents(100.0f, 100.0f, 100.0f), 8.0f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClampPositionXZTest, "System.PhysicsCore.Collision.ClampPositionXZ",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FClampPositionXZTest::RunTest(const FString& Parameters)
{
	FVector P(10000.0f, 500.0f, -5000.0f);
	ClampPositionXZ(P, 1800.0f);
	TestEqual("X", P.X, 1800.0f, 1.0e-3f);
	TestEqual("Y untouched", P.Y, 500.0f, 1.0e-3f);
	TestEqual("Z", P.Z, -1800.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXzDiscOverlapsAabbTest, "System.PhysicsCore.Collision.XzDiscOverlapsAabb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FXzDiscOverlapsAabbTest::RunTest(const FString& Parameters)
{
	TestTrue("Overlap", XzDiscOverlapsAabb(0.0f, 0.0f, 50.0f, 0.0f, 0.0f, 50.0f, 50.0f, 0.0f));
	TestFalse("Apart", XzDiscOverlapsAabb(500.0f, 0.0f, 30.0f, 0.0f, 0.0f, 50.0f, 50.0f, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCapsuleAabbMtvTest, "System.PhysicsCore.Collision.CapsuleAabbMtv",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCapsuleAabbMtvTest::RunTest(const FString& Parameters)
{
	// The capsule is pushed out of the box.
	FVector2D Normal = FVector2D::ZeroVector;
	float Penetration = 0.0f;
	TestTrue("Overlap", CapsuleAabbMtv(0.0f, 0.0f, 50.0f, 0.0f, 0.0f, 40.0f, 40.0f, Normal, Penetration));
	TestTrue("Penetration", Penetration > 0.0f);
	TestTrue("Normal", Normal.Size() > 0.5f);
	TestFalse("Apart", CapsuleAabbMtv(300.0f, 0.0f, 30.0f, 0.0f, 0.0f, 40.0f, 40.0f, Normal, Penetration));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeparateAabbTest, "System.PhysicsCore.Collision.SeparateAabb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSeparateAabbTest::RunTest(const FString& Parameters)
{
	FVector A(0.0f, 0.0f, 0.0f);
	FVector B(50.0f, 0.0f, 0.0f);
	const FVector Half(50.0f, 50.0f, 50.0f);
	FVector Normal = FVector::ZeroVector;
	TestTrue("Separated", SeparateAabb(A, Half, B, Half, 0.5f, 0.5f, &Normal));
	// The centers move apart along X.
	TestTrue("A moved", A.X < 0.0f);
	TestTrue("B moved", B.X > 50.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
