#include "Engine/VisibilityCellVolume.h"
#include "Engine/VisibilityPortal.h"
#include "Engine/World.h"
#include "SceneInterface.h"

namespace
{

	/** The scene rebuilds its cells and portals from the level before its next frame. */
	void UpdateSceneVisibilityCells(const AActor& Actor)
	{
		UWorld* World = Actor.GetWorld();
		if (World != nullptr && World->Scene != nullptr)
		{
			World->Scene->UpdateVisibilityCells();
		}
	}

} // namespace

AVisibilityCellVolume::AVisibilityCellVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AVisibilityCellVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	UpdateSceneVisibilityCells(*this);
}

void AVisibilityCellVolume::Destroyed()
{
	UpdateSceneVisibilityCells(*this);
	Super::Destroyed();
}

AVisibilityPortal::AVisibilityPortal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AVisibilityPortal::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	UpdateSceneVisibilityCells(*this);
}

void AVisibilityPortal::Destroyed()
{
	UpdateSceneVisibilityCells(*this);
	Super::Destroyed();
}
