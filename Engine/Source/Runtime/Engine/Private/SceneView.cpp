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
	return InitOptions;
}
