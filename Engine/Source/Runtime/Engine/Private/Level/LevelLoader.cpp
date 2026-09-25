#include "Level/LevelLoader.h"

#include "EngineLogs.h"
#include "Level/LeonLevelFormat.h"
#include "Misc/Paths.h"
#include "StaticMesh.h"

namespace
{

	[[nodiscard]] bool HasLeonLevelExtension(const FString& Path)
	{
		// FString == ignores case, like the lowered comparison it replaces.
		return FPaths::GetExtension(Path, true) == LeonLevelExtension;
	}

} // namespace

void ApplyFitHeight(FTransform& Transform, const UStaticMesh& Mesh, float FitHeight)
{
	if (FitHeight <= 0.0f)
	{
		return;
	}

	// Existing location is kept as an offset after auto scale / ground align.
	const FVector LocationOffset = Transform.GetLocation();

	const FVector Mn = Mesh.GetLocalMin();
	const FVector Mx = Mesh.GetLocalMax();
	const FVector Extents = Mx - Mn;
	/** 0.1 cm: keeps a flat mesh from dividing by zero. The height is the Z extent. */
	const float Height = FMath::Max(Extents.Z, 0.1f);
	const float Scale = FitHeight / Height;
	const FVector Center = (Mn + Mx) * 0.5f;

	Transform.SetScale3D(FVector(Scale, Scale, Scale));
	/** cm above the floor, against z-fighting with the ground. */
	constexpr float GroundEpsilon = 0.8f;
	const FVector Grounded = FVector((-Center.X) * Scale, (-Center.Y) * Scale, ((-Mn.Z) * Scale) + GroundEpsilon);
	Transform.SetLocation(Grounded + LocationOffset);
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
