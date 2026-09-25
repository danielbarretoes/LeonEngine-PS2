#include "Level/LevelLoader.h"

#include "Engine/Level.h"
#include "EngineLogs.h"
#include "Level/LeonLevelFormat.h"
#include "Misc/Paths.h"

namespace
{

	[[nodiscard]] bool HasLeonLevelExtension(const FString& Path)
	{
		// FString == ignores case, like the lowered comparison it replaces.
		return FPaths::GetExtension(Path, true) == LeonLevelExtension;
	}

} // namespace

void ApplyFitHeight(UStaticMeshComponent& Object, float FitHeight)
{
	if (Object.Mesh == nullptr || FitHeight <= 0.0f)
	{
		return;
	}

	// Existing location is kept as an offset after auto scale / ground align.
	const FVector LocationOffset = Object.Transform.GetLocation();

	const FVector Mn = Object.Mesh->GetLocalMin();
	const FVector Mx = Object.Mesh->GetLocalMax();
	const FVector Extents = Mx - Mn;
	const float Height = FMath::Max(Extents.Y, 0.001f);
	const float Scale = FitHeight / Height;
	const FVector Center = (Mn + Mx) * 0.5f;

	Object.Transform.SetScale3D(FVector(Scale, Scale, Scale));
	constexpr float GroundEpsilon = 0.008f;
	const FVector Grounded = FVector((-Center.X) * Scale, ((-Mn.Y) * Scale) + GroundEpsilon, (-Center.Z) * Scale);
	Object.Transform.SetLocation(Grounded + LocationOffset);
}

bool LoadLevelFile(UGameEngine& Engine, const FString& LevelPath)
{
	if (!HasLeonLevelExtension(LevelPath))
	{
		UE_LOG(
			LogLevel, Error, "LevelLoader: '%s' is not a Leon Level -- expected '%s'", *LevelPath, LeonLevelExtension);
		return false;
	}

	FLevelDocument Doc;
	if (!LoadLeonLevelFile(LevelPath, Doc))
	{
		return false;
	}

	return ApplyLevelDocument(Engine, Doc, LevelPath);
}
