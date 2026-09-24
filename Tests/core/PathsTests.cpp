#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <leon/core/Paths.h>
#include <string>

namespace {

[[nodiscard]] std::string sourceAsset(const char* relative) {
#ifdef LEON_SOURCE_DIR
    return (std::filesystem::path(LEON_SOURCE_DIR) / "Engine" / relative).lexically_normal().string();
#else
    return relative;
#endif
}

} // namespace

TEST_CASE("ResolveAssetPath finds known shader under repo", "[core][paths]") {
    const std::string resolved = leon::ResolveAssetPath("assets/Shaders/blinn_phong.vert");
    REQUIRE(std::filesystem::exists(resolved));
    REQUIRE(std::filesystem::exists(sourceAsset("Assets/Shaders/blinn_phong.vert")));
}
