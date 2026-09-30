#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Level.h"
#include "Engine/VisibilityCellVolume.h"
#include "Engine/VisibilityPortal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RendererLog.h"
#include "ScenePrivate.h"
#include "SceneView.h"
#include "StaticMeshSceneProxy.h"

namespace
{

	/** The index at which an element with OrderKey keeps the array sorted (after equal keys). */
	template <typename InfoType>
	[[nodiscard]] int32 InsertionIndex(const TArray<InfoType>& Infos, uint64 OrderKey)
	{
		int32 Index = Infos.Num();
		while (Index > 0 && Infos[Index - 1].OrderKey > OrderKey)
		{
			--Index;
		}
		return Index;
	}

	template <typename InfoType, typename ComponentType>
	[[nodiscard]] int32 FindInfo(const TArray<InfoType>& Infos, const ComponentType* Component)
	{
		for (int32 Index = 0; Index < Infos.Num(); ++Index)
		{
			if (Infos[Index].Component == Component)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

} // namespace

FScene::FScene(UWorld* InWorld)
	: World(InWorld)
{
}

FScene::~FScene()
{
	// A component still in the scene when it goes (the world's owner did not unregister it) forgets its proxy.
	for (FPrimitiveSceneInfo& Info : Primitives)
	{
		if (Info.Component != nullptr && Info.Component->SceneProxy == Info.Proxy.Get())
		{
			Info.Component->SceneProxy = nullptr;
		}
	}
	for (FLightSceneInfo& Info : Lights)
	{
		if (Info.Component != nullptr && Info.Component->SceneProxy == Info.Proxy.Get())
		{
			Info.Component->SceneProxy = nullptr;
		}
	}
}

uint64 FScene::GetOrderKey(const UActorComponent* Component)
{
	const AActor* Owner = Component != nullptr ? Component->GetOwner() : nullptr;
	if (Owner == nullptr)
	{
		return TNumericLimits<uint64>::Max();
	}
	const uint64 ComponentIndex =
		static_cast<uint64>(FMath::Max(Owner->GetComponents().Find(const_cast<UActorComponent*>(Component)), 0));
	return (Owner->GetUniqueID() << 20) | (ComponentIndex & 0xFFFFFu);
}

void FScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	// A proxy's assets stay alive, pending kill or not: their references are not cleared while it may draw them.
	Collector.AllowEliminatingReferences(false);
	for (FPrimitiveSceneInfo& Info : Primitives)
	{
		if (Info.Proxy != nullptr)
		{
			Info.Proxy->AddReferencedObjects(Collector);
		}
	}
	Collector.AllowEliminatingReferences(true);
}

void FScene::AddPrimitive(UPrimitiveComponent* Primitive)
{
	if (Primitive == nullptr || FindInfo(Primitives, Primitive) != INDEX_NONE)
	{
		return;
	}
	FPrimitiveSceneProxy* Proxy = Primitive->CreateSceneProxy();
	if (Proxy == nullptr)
	{
		return;
	}
	FPrimitiveSceneInfo Info;
	Info.Component = Primitive;
	Info.Proxy.Reset(Proxy);
	Info.OrderKey = GetOrderKey(Primitive);
	Info.CellMask = GetCellMask(*Proxy);
	const int32 Index = InsertionIndex(Primitives, Info.OrderKey);
	Primitives.Insert(MoveTemp(Info), Index);
	Primitive->SceneProxy = Proxy;
}

void FScene::RemovePrimitive(UPrimitiveComponent* Primitive)
{
	const int32 Index = FindInfo(Primitives, Primitive);
	if (Index == INDEX_NONE)
	{
		return;
	}
	if (Primitive->SceneProxy == Primitives[Index].Proxy.Get())
	{
		Primitive->SceneProxy = nullptr;
	}
	Primitives.RemoveAt(Index);
}

void FScene::UpdatePrimitiveTransform(UPrimitiveComponent* Primitive)
{
	if (Primitive != nullptr && Primitive->SceneProxy != nullptr)
	{
		Primitive->SceneProxy->SetStepTransform(
			Primitive->GetComponentTransform(), World != nullptr ? World->GetStepCount() : 0);
	}
}

void FScene::InterpolateTransforms(float Alpha)
{
	for (FPrimitiveSceneInfo& Info : Primitives)
	{
		if (Info.Proxy != nullptr && Info.Proxy->IsInterpolated())
		{
			Info.Proxy->InterpolateTransform(Alpha);
		}
	}
}

void FScene::AddLight(ULightComponent* Light)
{
	if (Light == nullptr || FindInfo(Lights, Light) != INDEX_NONE)
	{
		return;
	}
	FLightSceneProxy* Proxy = Light->CreateSceneProxy();
	if (Proxy == nullptr)
	{
		return;
	}
	FLightSceneInfo Info;
	Info.Component = Light;
	Info.Proxy.Reset(Proxy);
	Info.OrderKey = GetOrderKey(Light);
	const int32 Index = InsertionIndex(Lights, Info.OrderKey);
	Lights.Insert(MoveTemp(Info), Index);
	Light->SceneProxy = Proxy;
}

void FScene::RemoveLight(ULightComponent* Light)
{
	const int32 Index = FindInfo(Lights, Light);
	if (Index == INDEX_NONE)
	{
		return;
	}
	if (Light->SceneProxy == Lights[Index].Proxy.Get())
	{
		Light->SceneProxy = nullptr;
	}
	Lights.RemoveAt(Index);
}

void FScene::UpdateLightTransform(ULightComponent* Light)
{
	if (Light != nullptr && Light->SceneProxy != nullptr)
	{
		Light->SceneProxy->SetTransform(Light->GetComponentTransform());
	}
}

uint64 FScene::GetCellMask(const FPrimitiveSceneProxy& Proxy) const
{
	return VisibilityCells.IsEmpty() || Proxy.IsViewModel() ? 0 : VisibilityCells.GetCellMask(Proxy.GetWorldBounds());
}

const FVisibilityCellGraph& FScene::GetVisibilityCells()
{
	if (!bVisibilityCellsDirty)
	{
		return VisibilityCells;
	}
	bVisibilityCellsDirty = false;
	VisibilityCells.Reset();
	const ULevel* Level = World != nullptr ? World->PersistentLevel : nullptr;
	if (Level != nullptr)
	{
		// The cells first (the portals name them), in the level's order; then the portals.
		for (const AActor* Actor : Level->Actors)
		{
			const AVisibilityCellVolume* Cell = Cast<AVisibilityCellVolume>(Actor);
			if (Cell != nullptr && !Cell->IsPendingKillPending() &&
				VisibilityCells.AddCell(Cell->CellName, Cell->GetBrushBounds()) == INDEX_NONE)
			{
				UE_LOG(LogRenderer, Warning, "Scene: the cell '%s' of %s is left out (a name twice, or more than %d)",
					*Cell->CellName.ToString(), *Cell->GetName(), FVisibilityCellGraph::MaxCells);
			}
		}
		for (const AActor* Actor : Level->Actors)
		{
			const AVisibilityPortal* Portal = Cast<AVisibilityPortal>(Actor);
			if (Portal == nullptr || Portal->IsPendingKillPending())
			{
				continue;
			}
			FVector Corners[4];
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				Corners[Corner] = Portal->Corners.IsValidIndex(Corner) ? Portal->Corners[Corner] : FVector::ZeroVector;
			}
			if (Portal->Corners.Num() != 4 ||
				!VisibilityCells.AddPortal(
					VisibilityCells.FindCell(Portal->CellA), VisibilityCells.FindCell(Portal->CellB), Corners))
			{
				UE_LOG(LogRenderer, Warning, "Scene: the portal %s joins no two cells of the map ('%s', '%s')",
					*Portal->GetName(), *Portal->CellA.ToString(), *Portal->CellB.ToString());
			}
		}
	}
	// Every primitive to the cells its bounds touch.
	for (FPrimitiveSceneInfo& Info : Primitives)
	{
		Info.CellMask = GetCellMask(*Info.Proxy);
	}
	return VisibilityCells;
}

int32 FScene::GatherPrimitives(const FSceneView& View, const FVisibilityCellGraph::FVisibleCells& VisibleCells,
	FSceneRenderList<const FStaticMeshSceneProxy*>& OutWorldMeshes,
	FSceneRenderList<const FStaticMeshSceneProxy*>& OutViewModelMeshes,
	FSceneRenderList<const FPrimitiveSceneInfo*>& OutWorldSkeletalMeshes,
	FSceneRenderList<const FPrimitiveSceneInfo*>& OutViewModelSkeletalMeshes)
{
	OutWorldMeshes.Reset();
	OutViewModelMeshes.Reset();
	OutWorldSkeletalMeshes.Reset();
	OutViewModelSkeletalMeshes.Reset();
	OutWorldMeshes.Reserve(Primitives.Num());
	const bool bCells = !VisibilityCells.IsEmpty();
	int32 NumCulledByCells = 0;
	for (FPrimitiveSceneInfo& Info : Primitives)
	{
		const FPrimitiveSceneProxy* Proxy = Info.Proxy.Get();
		const bool bSkeletal = Proxy->GetProxyType() == EPrimitiveSceneProxyType::SkeletalMesh;
		if (Proxy->IsViewModel())
		{
			if (!Proxy->IsShown(&View))
			{
				continue;
			}
			if (bSkeletal)
			{
				OutViewModelSkeletalMeshes.Add(&Info);
			}
			else
			{
				OutViewModelMeshes.Add(static_cast<const FStaticMeshSceneProxy*>(Proxy));
			}
			continue;
		}
		// A shown proxy the view's actor may not see (owner-only, owner-hidden) leaves the frame; a hidden static mesh
		// stays (the F1 bounds show it).
		if ((bSkeletal || Proxy->IsShown()) && !Proxy->IsShown(&View))
		{
			continue;
		}
		// The cells and portals (N15): what moves is assigned again as it moves.
		if (bCells)
		{
			// A skeletal mesh moves or poses out of its bounds: a Static one keeps the cells it was added in, and the
			// portals cull it from the cells it walks into (a character's mesh is Movable, ACharacter).
			ensureMsgf(!bSkeletal || !Proxy->HasStaticLighting(),
				"Scene: a Static skeletal mesh in a map with cells keeps its first cells; make it Movable");
			if (!Proxy->HasStaticLighting())
			{
				Info.CellMask = GetCellMask(*Proxy);
			}
			if (!FVisibilityCellGraph::IsVisible(Info.CellMask, VisibleCells))
			{
				++NumCulledByCells;
				continue;
			}
		}
		if (bSkeletal)
		{
			OutWorldSkeletalMeshes.Add(&Info);
		}
		else
		{
			OutWorldMeshes.Add(static_cast<const FStaticMeshSceneProxy*>(Proxy));
		}
	}
	return NumCulledByCells;
}
