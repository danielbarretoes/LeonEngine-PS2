#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaterialIsTransparentUsesAlphaThresholdTest,
	"System.Renderer.Material.IsTransparentUsesAlphaThreshold",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMaterialIsTransparentUsesAlphaThresholdTest::RunTest(const FString& Parameters)
{
	// A material is transparent only when its alpha is below one.
	FMaterial Mat;
	Mat.Alpha = 1.0f;
	TestFalse("Opaque", Mat.IsTransparent());
	Mat.Alpha = 0.5f;
	TestTrue("Transparent", Mat.IsTransparent());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
