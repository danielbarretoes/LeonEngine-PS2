#include "Level/LevelLoader.h"

#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineLogs.h"
#include "Level/LeonLevelFormat.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

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

	const FVector Mn = Mesh.GetBoundingBox().Min;
	const FVector Mx = Mesh.GetBoundingBox().Max;
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

void GetLegacyPlayFromHereView(const UCameraComponent& Framing, FVector& OutLocation, FRotator& OutRotation)
{
	// The legacy engine camera took the framing, then its default game mode flew from the eye looking at the target
	// (an orbit framing keeps its view, a free-look one turns to the target): the same camera math gives the same view.
	UCameraComponent* Camera = NewObject<UCameraComponent>(GetTransientPackage());
	Camera->SetTarget(Framing.GetTarget());
	Camera->SetDistance(Framing.GetDistance());
	Camera->SetViewRotation(Framing.GetViewRotation());
	Camera->SetEyeLocation(Framing.EyeLocation());
	Camera->SetMode(Framing.GetMode());
	const FVector Eye = Camera->GetCameraLocation();
	FVector Look = Camera->GetTarget() - Eye;
	const float LookLen = Look.Size();
	if (LookLen > 1.0e-3f)
	{
		Look /= LookLen;
	}
	else
	{
		Look = FVector(0.0f, -1.0f, 0.0f);
	}
	Camera->SetMode(ECameraMode::FreeLook);
	Camera->SetEyeLocation(Eye);
	Camera->SetViewRotation(Look.Rotation());
	OutLocation = Camera->EyeLocation();
	OutRotation = Camera->GetViewRotation();
	Camera->MarkPendingKill();
}

bool LoadLevelFile(UWorld& World, const FString& LevelPath)
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

	return ApplyLevelDocument(World, Doc, LevelPath);
}
