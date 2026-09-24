#include "Level/LevelLoader.h"

#include "Engine/Level.h"
#include "Level/LeonLevelFormat.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{

	[[nodiscard]] bool HasLeonLevelExtension(const std::string& Path)
	{
		std::string Extension = std::filesystem::path(Path).extension().string();
		for (char& C : Extension)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		return Extension == LeonLevelExtension;
	}

} // namespace

void ApplyFitHeight(UStaticMeshComponent& Object, float FitHeight)
{
	if (Object.Mesh == nullptr || FitHeight <= 0.0f)
	{
		return;
	}

	// Existing position is kept as an offset after auto scale / ground align.
	const glm::vec3 PositionOffset = Object.Transform.Position;

	const glm::vec3 Mn = Object.Mesh->GetLocalMin();
	const glm::vec3 Mx = Object.Mesh->GetLocalMax();
	const glm::vec3 Extents = Mx - Mn;
	const float Height = std::max(Extents.y, 0.001f);
	const float Scale = FitHeight / Height;
	const glm::vec3 Center = (Mn + Mx) * 0.5f;

	Object.Transform.Scale = {Scale, Scale, Scale};
	constexpr float GroundEpsilon = 0.008f;
	const glm::vec3 Grounded{(-Center.x) * Scale, ((-Mn.y) * Scale) + GroundEpsilon, (-Center.z) * Scale};
	Object.Transform.Position = Grounded + PositionOffset;
}

bool LoadLevelFile(UGameEngine& Engine, const std::string& LevelPath)
{
	if (!HasLeonLevelExtension(LevelPath))
	{
		std::cerr << "LevelLoader: '" << LevelPath << "' is not a Leon Level -- expected '" << LeonLevelExtension
				  << "'\n";
		return false;
	}

	FLevelDocument Doc;
	if (!LoadLeonLevelFile(LevelPath, Doc))
	{
		return false;
	}

	return ApplyLevelDocument(Engine, Doc, LevelPath);
}
