#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <leon/content/CookedSkeletal.h>

namespace fs = std::filesystem;

namespace {

[[nodiscard]] std::string sourceAsset(const char* relative) {
#ifdef LEON_SOURCE_DIR
    return (fs::path(LEON_SOURCE_DIR) / relative).lexically_normal().string();
#else
    return relative;
#endif
}

} // namespace

TEST_CASE("CookCharacterFromFbx writes Leon binary asset set", "[content][cook]") {
    const fs::path outDir = fs::temp_directory_path() / "leon_cook_bot_test";
    std::error_code ec;
    fs::remove_all(outDir, ec);
    fs::create_directories(outDir);

    REQUIRE(leon::CookCharacterFromFbx(
        "Bot", sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/BreathingIdle.fbx"),
        sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/Running.fbx"), outDir.string(),
        leon::CookJumpAnimPaths{
            sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/JumpingUp.fbx"),
            sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/FallingIdle.fbx"),
            sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/FallingToLanding.fbx"),
        }));

    REQUIRE(fs::exists(outDir / "Bot.lskel"));
    REQUIRE(fs::exists(outDir / "Bot.lskm"));
    REQUIRE(fs::exists(outDir / "Materials" / "M_Bot.lmat"));
    REQUIRE(fs::exists(outDir / "Anims" / "BreathingIdle.lanim"));
    REQUIRE(fs::exists(outDir / "Anims" / "Running.lanim"));
    REQUIRE(fs::exists(outDir / "Anims" / "JumpingUp.lanim"));
    REQUIRE(fs::exists(outDir / "Anims" / "FallingIdle.lanim"));
    REQUIRE(fs::exists(outDir / "Anims" / "FallingToLanding.lanim"));
    REQUIRE(fs::exists(outDir / "Bot_Locomotion.blendspace1d.json"));
    REQUIRE(fs::exists(outDir / "Bot.lchar"));

    leon::Skeleton skeleton;
    REQUIRE(leon::LoadSkeleton((outDir / "Bot.lskel").string(), skeleton));
    REQUIRE(skeleton.BoneCount() > 0);

    leon::SkeletalMeshData mesh;
    REQUIRE(leon::LoadSkeletalMesh((outDir / "Bot.lskm").string(), mesh));
    REQUIRE_FALSE(mesh.empty());
    REQUIRE(mesh.skeleton.BoneCount() == skeleton.BoneCount());

    leon::AnimSequence idle;
    REQUIRE(leon::LoadAnimSequence((outDir / "Anims" / "BreathingIdle.lanim").string(), idle));
    REQUIRE(idle.FrameCount() > 0);
    REQUIRE(idle.durationSeconds > 2.0f);

    leon::AnimSequence run;
    REQUIRE(leon::LoadAnimSequence((outDir / "Anims" / "Running.lanim").string(), run));
    REQUIRE(run.FrameCount() > 0);

    leon::BlendSpace1DAssetDesc bs;
    REQUIRE(leon::LoadBlendSpace1DJson((outDir / "Bot_Locomotion.blendspace1d.json").string(), bs));
    REQUIRE(bs.samples.size() == 2);

    leon::CharacterVisualDesc character;
    REQUIRE(leon::LoadCharacterVisual((outDir / "Bot.lchar").string(), character));
    REQUIRE(character.skeletalMeshRel == "Bot.lskm");
    REQUIRE(character.jumpStartAnimRel == "Anims/JumpingUp.lanim");
    REQUIRE(character.fallLoopAnimRel == "Anims/FallingIdle.lanim");
    REQUIRE(character.landAnimRel == "Anims/FallingToLanding.lanim");
}

TEST_CASE("Repo Bot.lchar cooked assets load", "[content][cook]") {
    const std::string characterPath = sourceAsset("Templates/ThirdPerson/Content/assets/characters/bot/Bot.lchar");
    if (!fs::exists(characterPath)) {
        SKIP("Bot.lchar not cooked into repo yet");
    }
    leon::CharacterVisualDesc desc;
    REQUIRE(leon::LoadCharacterVisual(characterPath, desc));
    const fs::path base = fs::path(characterPath).parent_path();
    leon::SkeletalMeshData mesh;
    REQUIRE(leon::LoadSkeletalMesh((base / desc.skeletalMeshRel).string(), mesh));
    REQUIRE_FALSE(mesh.empty());
}
