#include "Engine/GameEngine.h"
#include "GameplayMinimal.h"
#include "Level/LeonLevelFormat.h"
#include "Level/LevelLoader.h"
#include "Misc/CString.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("Editor-style level save load apply headless", "[editor][level]")
{
#ifdef LEON_ROOT_DIR
	const std::string TemplateLevel = std::string(LEON_ROOT_DIR) + "/Engine/Content/LevelTemplates/Blank.llev";
	UGameEngine Engine;
	REQUIRE(Engine.InitializeHeadless());
	REQUIRE(LoadLevelFile(Engine, TemplateLevel));

	FLevelDocument Doc = BuildLevelDocument(Engine.GetLevel(), Engine.GetCamera());
	REQUIRE_FALSE(Doc.Name.empty());

	const std::vector<std::uint8_t> Bytes = SerializeLeonLevel(Doc);
	REQUIRE_FALSE(Bytes.empty());

	FLevelDocument RoundTrip;
	REQUIRE(DeserializeLeonLevel(Bytes, RoundTrip));
	REQUIRE(RoundTrip.Name == Doc.Name);

	REQUIRE(ApplyLevelDocument(Engine, RoundTrip, "memory-editor-smoke"));
	Engine.Shutdown();
#else
	SUCCEED("LEON_ROOT_DIR unset");
#endif
}

TEST_CASE("AIChaseBehavior MoveTo when target present", "[gameplay][bt][ai]")
{
	UWorld World;
	auto* Character = World.SpawnActor<ACharacter>();
	auto* Target = World.SpawnActor<ACharacter>();
	Target->SetActorLocationAndRotation({5.0f, 0.0f, 0.0f}, 0.0f);

	AAIController Ai;
	Ai.Possess(Character);
	FAIChaseBehavior Chase;
	(void)Chase.Tick(Ai, Target, 0.016f);
	REQUIRE(Ai.GetLogicState() == EAILogicState::Chase);
	(void)Chase.Tick(Ai, nullptr, 0.016f);
	REQUIRE(Ai.GetLogicState() == EAILogicState::Idle);
}
