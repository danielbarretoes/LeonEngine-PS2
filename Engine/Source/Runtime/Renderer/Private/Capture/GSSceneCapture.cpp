#include "GSSceneCapture.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GS/GSSceneRenderer.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSEmulator/PS2TexturePreview.h"
#include "GameFramework/WorldSettings.h"
#include "ScenePrivate.h"
#include "SceneView.h"

FGSSceneCapture::FGSSceneCapture()
	: Renderer(MakeUnique<FGSSceneRenderer>())
{
}

FGSSceneCapture::~FGSSceneCapture() = default;

int32 FGSSceneCapture::RecordStaticWorld(UWorld& World, const FVector& ViewOrigin, const FMatrix& ViewMatrix,
	const FMatrix& ProjectionMatrix, const FGSDrawEnvironment& Environment, uint32 TextureArenaFirstBlock,
	uint32 TextureArenaBlocks, FGSCommandList& List)
{
	if (World.PersistentLevel == nullptr)
	{
		return 0;
	}
	// A scene of the capture's own, of the Static mesh components the game shows.
	struct FAdded
	{
		UStaticMeshComponent* Mesh = nullptr;
		FPrimitiveSceneProxy* PreviousProxy = nullptr;
	};
	FScene Scene(&World);
	TArray<FAdded> Added;
	for (AActor* Actor : World.PersistentLevel->Actors)
	{
		if (Actor == nullptr || Actor->IsPendingKillPending() || Actor->IsHidden())
		{
			continue;
		}
		for (UActorComponent* Component : Actor->GetComponents())
		{
			UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
			if (Mesh == nullptr || Mesh->Mobility != EComponentMobility::Static || !Mesh->IsVisible() ||
				Mesh->GetStaticMesh() == nullptr)
			{
				continue;
			}
			FPrimitiveSceneProxy* const Previous = Mesh->SceneProxy;
			Scene.AddPrimitive(Mesh);
			if (Mesh->SceneProxy != Previous)
			{
				Added.Add({Mesh, Previous});
			}
		}
	}
	if (Added.Num() > 0)
	{
		// Neither the sky nor the fog: a picture of the geometry, the same at any height.
		AWorldSettings* WorldSettings = World.GetWorldSettings();
		const FWorldSkySettings SkySettings =
			WorldSettings != nullptr ? WorldSettings->SkySettings : FWorldSkySettings();
		const bool bFog = WorldSettings != nullptr && WorldSettings->FogSettings.bEnableFog;
		if (WorldSettings != nullptr)
		{
			WorldSettings->SkySettings = FWorldSkySettings();
			WorldSettings->FogSettings.bEnableFog = false;
		}

		FSceneViewFamily Family(FSceneViewFamily::ConstructionValues(
			int32(Environment.Width), int32(Environment.Height), &Scene, FEngineShowFlags()));
		FSceneViewInitOptions Options;
		Options.ViewFamily = &Family;
		Options.ViewOrigin = ViewOrigin;
		Options.ViewMatrix = ViewMatrix;
		Options.ProjectionMatrix = ProjectionMatrix;
		Options.ViewModelProjectionMatrix = ProjectionMatrix;
		Options.ViewRectMax = FIntPoint(int32(Environment.Width), int32(Environment.Height));
		const FSceneView View(Options);
		Family.Views.Add(&View);

		Renderer->GetTextureCache().SetArena(TextureArenaFirstBlock, TextureArenaBlocks);
		Renderer->GetTextureCache().SetTextureConverter(&ConvertTextureAsPS2Cook);
		Renderer->GetTextureCache().SetUploadBudgetKB(0);
		// The skeletal meshes' throttle keeps the world's own views: the capture leaves them as they were.
		const TArray<FVector> ViewLocations = World.ViewLocationsRenderedLastFrame;
		Renderer->Render(Family, Environment, List);
		World.ViewLocationsRenderedLastFrame = ViewLocations;

		if (WorldSettings != nullptr)
		{
			WorldSettings->SkySettings = SkySettings;
			WorldSettings->FogSettings.bEnableFog = bFog;
		}
	}
	for (const FAdded& Entry : Added)
	{
		Scene.RemovePrimitive(Entry.Mesh);
		Entry.Mesh->SceneProxy = Entry.PreviousProxy;
	}
	return Added.Num();
}
