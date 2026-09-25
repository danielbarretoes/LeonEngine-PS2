#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MaterialAsset.h"
#include "MaterialShared.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Parses a JSON object literal; an empty object when the text does not parse. */
	FJsonObject ParseObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) || !Object.IsValid())
		{
			return FJsonObject();
		}
		return *Object;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaterialRoughnessFromShininessDecreasesWithShininessTest,
	"System.Renderer.Material.RoughnessFromShininessDecreasesWithShininess",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMaterialRoughnessFromShininessDecreasesWithShininessTest::RunTest(const FString& Parameters)
{
	// Higher shininess gives lower roughness, kept within [0.04, 1].
	const float RoughSoft = RoughnessFromShininess(8.0f);
	const float RoughHard = RoughnessFromShininess(256.0f);
	TestTrue("Shinier is smoother", RoughSoft > RoughHard);
	TestTrue("Lower bound", RoughHard >= 0.04f);
	TestTrue("Upper bound", RoughSoft <= 1.0f);
	return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaterialSyncRoughnessFromShininessTest,
	"System.Renderer.Material.SyncRoughnessFromShininess",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMaterialSyncRoughnessFromShininessTest::RunTest(const FString& Parameters)
{
	// SyncRoughnessFromShininess sets the roughness from the current shininess.
	FMaterial Mat;
	Mat.Shininess = 128.0f;
	Mat.SyncRoughnessFromShininess();
	TestEqual("Roughness", Mat.Roughness, RoughnessFromShininess(128.0f), 1.0e-6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaterialHasMaterialSurfaceFieldsDetectsSurfaceKeysTest,
	"System.Renderer.Material.HasMaterialSurfaceFieldsDetectsSurfaceKeys",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMaterialHasMaterialSurfaceFieldsDetectsSurfaceKeysTest::RunTest(const FString& Parameters)
{
	// Surface keys (albedo, maps) count as material fields; gameplay-only keys and empty objects do not.
	TestTrue("albedo", HasMaterialSurfaceFields(ParseObject("{\"albedo\": [1, 1, 1]}")));
	TestTrue("albedoMap", HasMaterialSurfaceFields(ParseObject("{\"albedoMap\": \"checker\"}")));
	TestFalse("tag only", HasMaterialSurfaceFields(ParseObject("{\"tag\": \"player\"}")));
	TestFalse("empty object", HasMaterialSurfaceFields(FJsonObject()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
