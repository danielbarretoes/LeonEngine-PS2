#include "Misc/Paths.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

namespace
{

	[[nodiscard]] std::string SourceAsset(const char* Relative)
	{
#ifdef LEON_ROOT_DIR
		return (std::filesystem::path(LEON_ROOT_DIR) / "Engine" / Relative).lexically_normal().string();
#else
		return relative;
#endif
	}

} // namespace

TEST_CASE("ResolveAssetPath finds known shader under repo", "[core][paths]")
{
	const std::string Resolved = FPaths::ResolveAssetPath("assets/Shaders/blinn_phong.vert");
	REQUIRE(std::filesystem::exists(Resolved));
	REQUIRE(std::filesystem::exists(SourceAsset("Shaders/blinn_phong.vert")));
}
