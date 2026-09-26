#include "SceneView.h"

#include "Camera/CameraComponent.h"

namespace
{

	const TCHAR* const ShowFlagNames[] = {TEXT("Bounds"), TEXT("Collision"), TEXT("Navigation"), TEXT("AxesGizmo")};

} // namespace

bool* FEngineShowFlags::FindFlag(const FString& Name)
{
	bool* const Flags[] = {&Bounds, &Collision, &Navigation, &AxesGizmo};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ShowFlagNames); ++Index)
	{
		if (Name.Equals(ShowFlagNames[Index], ESearchCase::IgnoreCase))
		{
			return Flags[Index];
		}
	}
	return nullptr;
}

const TCHAR* const* FEngineShowFlags::GetFlagNames(int32& OutNum)
{
	OutNum = UE_ARRAY_COUNT(ShowFlagNames);
	return ShowFlagNames;
}

FSceneViewFamily::FSceneViewFamily(const ConstructionValues& CVS)
	: RenderTargetSizeX(CVS.RenderTargetSizeX)
	, RenderTargetSizeY(CVS.RenderTargetSizeY)
	, Scene(CVS.Scene)
	, EngineShowFlags(CVS.EngineShowFlags)
{
}

FSceneView::FSceneView(const FSceneViewInitOptions& InitOptions)
	: Family(InitOptions.ViewFamily)
	, ViewLocation(InitOptions.ViewOrigin)
	, ViewMatrix(InitOptions.ViewMatrix)
	, ProjectionMatrix(InitOptions.ProjectionMatrix)
	, ViewModelProjectionMatrix(InitOptions.ViewModelProjectionMatrix)
	, ViewActor(InitOptions.ViewActor)
	, FOV(InitOptions.FOV)
	, ViewRectMin(InitOptions.ViewRectMin)
	, ViewRectMax(InitOptions.ViewRectMax)
{
}

FSceneViewInitOptions FSceneView::FromCamera(const FSceneViewFamily& ViewFamily, const UCameraComponent& Camera)
{
	FSceneViewInitOptions InitOptions;
	InitOptions.ViewFamily = &ViewFamily;
	InitOptions.ViewOrigin = Camera.GetCameraLocation();
	InitOptions.ViewMatrix = Camera.ViewMatrix();
	InitOptions.ProjectionMatrix = Camera.ProjectionMatrix();
	InitOptions.FOV = Camera.FieldOfView();
	InitOptions.ViewRectMax = FIntPoint(ViewFamily.RenderTargetSizeX, ViewFamily.RenderTargetSizeY);
	// The view model pass: its own vertical field of view, the camera's aspect, a near plane at the weapon.
	const float ViewModelFOV = Camera.ViewModelFOV > 0.0f ? Camera.ViewModelFOV : Camera.FieldOfView();
	const float HalfFov = FMath::DegreesToRadians(FMath::Clamp(ViewModelFOV, 5.0f, 120.0f)) / 2.0f;
	InitOptions.ViewModelProjectionMatrix = Camera.IsOrthographic()
		? Camera.ProjectionMatrix()
		: FMatrix(FPerspectiveMatrix(
			  HalfFov, HalfFov, 1.0f / Camera.GetAspect(), 1.0f, ViewModelNearPlane, Camera.GetFarPlane()));
	return InitOptions;
}
