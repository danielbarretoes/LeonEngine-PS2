#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

// FPaths still resolves with std::filesystem (desktop only) until the IPlatformFile rewrite.
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_DESKTOP

	#include "Misc/Paths.h"

	#include <filesystem>
	#include <string>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathsResolveAssetPathTest, "System.Core.Misc.Paths.ResolveAssetPath",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPathsResolveAssetPathTest::RunTest(const FString& Parameters)
{
	const std::string Resolved = FPaths::ResolveAssetPath("assets/Shaders/blinn_phong.vert");
	TestTrue("Resolved shader exists", std::filesystem::exists(Resolved));
	#ifdef LEON_ROOT_DIR
	TestTrue("Shader is in Engine/Shaders",
		std::filesystem::exists(std::filesystem::path(LEON_ROOT_DIR) / "Engine" / "Shaders" / "blinn_phong.vert"));
	#endif
	return true;
}

#endif
