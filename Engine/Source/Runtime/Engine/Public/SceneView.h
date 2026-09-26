#pragma once

#include "CoreMinimal.h"

class AActor;
class FSceneInterface;
class UCameraComponent;

/**
 * What a view family draws besides the scene (UE: FEngineShowFlags), toggled by the console's `show <Flag>`
 * (UGameViewportClient). UE's flags are Bounds, Collision and Navigation; the axes gizmo is Leon's.
 */
struct ENGINE_API FEngineShowFlags
{
	/** The static meshes' world boxes and the directional shadow volume (UE: ShowFlag.Bounds; F1 in LeonGame). */
	bool Bounds = false;
	/** The collision volumes and floor traces (UE: ShowFlag.Collision; F2; the renderer does not draw it yet). */
	bool Collision = false;
	/** The navigation mesh (UE: ShowFlag.Navigation; F3; the renderer does not draw it yet). */
	bool Navigation = false;
	/** 1 m world axes at the origin and a view orientation gizmo in the bottom-left corner (Leon; F6 in LeonGame). */
	bool AxesGizmo = false;

	/** The flag named Name (case ignored), or null (UE: FindIndexByName). */
	[[nodiscard]] bool* FindFlag(const FString& Name);
	/** The flags' names, for `show` without a flag. */
	[[nodiscard]] static const TCHAR* const* GetFlagNames(int32& OutNum);
};

/** The views drawn into one render target with one scene (UE: FSceneViewFamily). Leon draws one view per family. */
class ENGINE_API FSceneViewFamily
{
public:
	/** UE: FSceneViewFamily::ConstructionValues (render target, scene and show flags). */
	struct ConstructionValues
	{
		ConstructionValues(int32 InRenderTargetSizeX, int32 InRenderTargetSizeY, FSceneInterface* InScene,
			const FEngineShowFlags& InEngineShowFlags)
			: RenderTargetSizeX(InRenderTargetSizeX)
			, RenderTargetSizeY(InRenderTargetSizeY)
			, Scene(InScene)
			, EngineShowFlags(InEngineShowFlags)
		{
		}

		int32 RenderTargetSizeX = 0;
		int32 RenderTargetSizeY = 0;
		FSceneInterface* Scene = nullptr;
		FEngineShowFlags EngineShowFlags;
	};

	explicit FSceneViewFamily(const ConstructionValues& CVS);

	/** The draw framebuffer size in pixels (UE: the render target's size). */
	int32 RenderTargetSizeX = 0;
	int32 RenderTargetSizeY = 0;
	/** The scene to draw; null draws only the background (a world without a scene). */
	FSceneInterface* Scene = nullptr;
	FEngineShowFlags EngineShowFlags;
	/** The views, which the caller owns (UE: Views). */
	TArray<const class FSceneView*> Views;
};

/** How to make a FSceneView (UE: FSceneViewInitOptions). */
struct ENGINE_API FSceneViewInitOptions
{
	const FSceneViewFamily* ViewFamily = nullptr;
	/** The eye, world units (UE: ViewOrigin). */
	FVector ViewOrigin = FVector::ZeroVector;
	/**
	 * World to UE view space (x right, y up, z forward; ViewMatrices.h). UE passes the translation and the rotation
	 * apart; Leon's camera builds the whole matrix (orbit and free look).
	 */
	FMatrix ViewMatrix = FMatrix::Identity;
	/** UE's projection (depth [0, 1], not reversed); the GL renderer applies ToGLClipSpace to it. */
	FMatrix ProjectionMatrix = FMatrix::Identity;
	/** Vertical field of view, degrees (Leon's camera keeps a vertical one; UE's FOV is horizontal). */
	float FOV = 90.0f;
	/** The view model primitives' projection (Leon, FSceneView::ViewModelProjectionMatrix); the view's by default. */
	FMatrix ViewModelProjectionMatrix = FMatrix::Identity;
	/** The actor the view belongs to, for bOnlyOwnerSee / bOwnerNoSee (UE: ViewActor): the player's view target. */
	const AActor* ViewActor = nullptr;
	/** The view rectangle in the render target (UE: SetViewRectangle); the whole target in Leon. */
	FIntPoint ViewRectMin = FIntPoint(0, 0);
	FIntPoint ViewRectMax = FIntPoint(0, 0);
};

/**
 * A view of the scene for one frame (UE: FSceneView): the camera's matrices and the part of the render target it
 * covers. The engine builds it from its view camera (FromCamera) and hands its family to the renderer
 * (IRendererModule::BeginRenderingViewFamily).
 */
class ENGINE_API FSceneView
{
public:
	explicit FSceneView(const FSceneViewInitOptions& InitOptions);

	/**
	 * The init options of a view through a camera covering the whole family target; the view model projection has the
	 * camera's ViewModelFOV (its FOV without one) and a near plane of ViewModelNearPlane.
	 */
	[[nodiscard]] static FSceneViewInitOptions FromCamera(
		const FSceneViewFamily& ViewFamily, const UCameraComponent& Camera);

	const FSceneViewFamily* Family = nullptr;
	FVector ViewLocation = FVector::ZeroVector;
	/** World to UE view space (UE: ViewMatrices.GetViewMatrix()). */
	FMatrix ViewMatrix = FMatrix::Identity;
	/** UE's projection (UE: ViewMatrices.GetProjectionMatrix()). */
	FMatrix ProjectionMatrix = FMatrix::Identity;
	/**
	 * The projection of the view model primitives (Leon's view model pass, UPrimitiveComponent::bRenderAsViewModel):
	 * drawn after the scene over a cleared depth buffer, with their own field of view and a near plane of 1 cm.
	 */
	FMatrix ViewModelProjectionMatrix = FMatrix::Identity;
	/** The view's actor (UE: ViewActor): primitives with bOnlyOwnerSee / bOwnerNoSee are shown by their owners. */
	const AActor* ViewActor = nullptr;
	float FOV = 90.0f;
	/** UE: UnscaledViewRect (min, max). */
	FIntPoint ViewRectMin = FIntPoint(0, 0);
	FIntPoint ViewRectMax = FIntPoint(0, 0);
};
