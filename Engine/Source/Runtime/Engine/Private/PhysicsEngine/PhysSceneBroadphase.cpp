#include "Physics/PhysSceneBroadphase.h"

namespace
{

	/** Growth of a body's box for rounding (cm, and a share of its largest coordinate). */
	constexpr float BodyBoxMargin = 0.5f;
	constexpr float BodyBoxRelativeMargin = 1.0e-5f;
	/** Dead items the tree keeps before a rebuild compacts it. */
	constexpr int32 MinDeadTreeItemsToRebuild = 16;

} // namespace

FBox FPhysSceneBroadphase::CalcBodyBounds(const FBodyInstance& Body, const FTriangleMeshCollision* Mesh)
{
	FVector Extent = Body.HalfExtents.GetAbs();
	if (Body.CollisionShape == EBodyCollisionShape::Capsule)
	{
		// The capsule's radius is HalfExtents.X, its half height with the caps max(HalfExtents.Z, radius).
		const float Radius = FMath::Max(Body.HalfExtents.X, 0.0f);
		Extent = FVector(FMath::Max(Extent.X, Radius), FMath::Max(Extent.Y, Radius), FMath::Max(Extent.Z, Radius));
	}
	FVector Min = Body.Position - Extent;
	FVector Max = Body.Position + Extent;
	if (Mesh != nullptr)
	{
		// The body's box only gates the triangles, which may lie out of it: the tree's root holds them.
		if (!Mesh->Tree.IsBuilt())
		{
			Mesh->BuildTree();
		}
		if (Mesh->Tree.GetNodes().Num() > 0)
		{
			const FAabbTreeNode& Root = Mesh->Tree.GetNodes()[0];
			Min = FVector(FMath::Min(Min.X, Root.Min.X), FMath::Min(Min.Y, Root.Min.Y), FMath::Min(Min.Z, Root.Min.Z));
			Max = FVector(FMath::Max(Max.X, Root.Max.X), FMath::Max(Max.Y, Root.Max.Y), FMath::Max(Max.Z, Root.Max.Z));
		}
	}
	const float Largest = FMath::Max(Min.GetAbsMax(), Max.GetAbsMax());
	const FVector Margin(BodyBoxMargin + (Largest * BodyBoxRelativeMargin));
	return FBox(Min - Margin, Max + Margin);
}

const FTriangleMeshCollision* FPhysSceneBroadphase::GetBodyMesh(
	const FBodyInstance& Body, int32 Index, const TArray<FTriangleMeshCollision>& Meshes)
{
	return Body.CollisionShape == EBodyCollisionShape::TriangleMesh && Index < Meshes.Num() && Meshes[Index].IsValid()
		? &Meshes[Index]
		: nullptr;
}

void FPhysSceneBroadphase::Reset()
{
	Tree.Reset();
	List.Reset();
	Bounds.Reset();
	TreeSlots.Reset();
	ListSlots.Reset();
	ListMaxWidthX = 0.0f;
	NumDeadTreeItems = 0;
	bTreeDirty = false;
}

void FPhysSceneBroadphase::AddBody(const FBodyInstance& Body, const FTriangleMeshCollision* Mesh, bool bMovable)
{
	const int32 Index = Bounds.Add(CalcBodyBounds(Body, Mesh));
	TreeSlots.Add(INDEX_NONE);
	ListSlots.Add(INDEX_NONE);
	if (bMovable)
	{
		InsertInList(Index);
	}
	else
	{
		bTreeDirty = true;
	}
}

void FPhysSceneBroadphase::RemoveBodyAtSwap(int32 Index)
{
	RemoveFromTree(Index);
	RemoveFromList(Index);
	const int32 Last = Bounds.Num() - 1;
	if (Index != Last)
	{
		if (TreeSlots[Last] != INDEX_NONE)
		{
			Tree.SetItem(TreeSlots[Last], Index);
		}
		if (ListSlots[Last] != INDEX_NONE)
		{
			List[ListSlots[Last]].BodyIndex = Index;
		}
	}
	Bounds.RemoveAtSwap(Index, 1, false);
	TreeSlots.RemoveAtSwap(Index, 1, false);
	ListSlots.RemoveAtSwap(Index, 1, false);
}

void FPhysSceneBroadphase::UpdateBody(int32 Index, const FBodyInstance& Body, const FTriangleMeshCollision* Mesh)
{
	const FBox NewBounds = CalcBodyBounds(Body, Mesh);
	if (ListSlots[Index] != INDEX_NONE)
	{
		Bounds[Index] = NewBounds;
		const int32 Slot = ListSlots[Index];
		List[Slot].MinX = NewBounds.Min.X;
		ListMaxWidthX = FMath::Max(ListMaxWidthX, NewBounds.Max.X - NewBounds.Min.X);
		ResortListEntry(Slot);
		return;
	}
	if (NewBounds == Bounds[Index])
	{
		return;
	}
	// A static body that moves: from now on it is looked up in the list.
	RemoveFromTree(Index);
	Bounds[Index] = NewBounds;
	InsertInList(Index);
}

void FPhysSceneBroadphase::Refresh(const TArray<FBodyInstance>& Bodies, const TArray<FTriangleMeshCollision>& Meshes)
{
	const int32 Known = Bounds.Num();
	Bounds.SetNum(Bodies.Num());
	TreeSlots.SetNum(Bodies.Num());
	ListSlots.SetNum(Bodies.Num());
	List.Reset();
	ListMaxWidthX = 0.0f;
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		const bool bWasMovable = Index < Known && ListSlots[Index] != INDEX_NONE;
		Bounds[Index] = CalcBodyBounds(Bodies[Index], GetBodyMesh(Bodies[Index], Index, Meshes));
		TreeSlots[Index] = INDEX_NONE;
		ListSlots[Index] = INDEX_NONE;
		if (bWasMovable || Bodies[Index].Type == EBodyType::Dynamic)
		{
			InsertInList(Index);
		}
	}
	Tree.Reset();
	NumDeadTreeItems = 0;
	bTreeDirty = true;
}

void FPhysSceneBroadphase::Flush()
{
	const bool bCompact =
		NumDeadTreeItems >= MinDeadTreeItemsToRebuild && NumDeadTreeItems * 2 >= Tree.GetItems().Num();
	if (!bTreeDirty && !bCompact)
	{
		return;
	}
	TArray<FBox> StaticBounds;
	TArray<int32> StaticIndices;
	for (int32 Index = 0; Index < Bounds.Num(); ++Index)
	{
		TreeSlots[Index] = INDEX_NONE;
		if (ListSlots[Index] == INDEX_NONE)
		{
			StaticBounds.Add(Bounds[Index]);
			StaticIndices.Add(Index);
		}
	}
	Tree.Build(StaticBounds.GetData(), StaticIndices.GetData(), StaticIndices.Num());
	const TArray<int32>& Items = Tree.GetItems();
	for (int32 Slot = 0; Slot < Items.Num(); ++Slot)
	{
		TreeSlots[Items[Slot]] = Slot;
	}
	NumDeadTreeItems = 0;
	bTreeDirty = false;
	++NumTreeBuilds;
}

int32 FPhysSceneBroadphase::FindFirstListSlot(float X) const
{
	// The first entry whose MinX is not below X minus the widest box: the ones before end before X.
	const float From = X - ListMaxWidthX;
	int32 Low = 0;
	int32 High = List.Num();
	while (Low < High)
	{
		const int32 Middle = Low + ((High - Low) / 2);
		if (List[Middle].MinX < From)
		{
			Low = Middle + 1;
		}
		else
		{
			High = Middle;
		}
	}
	return Low;
}

void FPhysSceneBroadphase::InsertInList(int32 Index)
{
	const float MinX = Bounds[Index].Min.X;
	ListMaxWidthX = FMath::Max(ListMaxWidthX, Bounds[Index].Max.X - MinX);
	// After the entries with the same MinX.
	int32 Slot = List.Num();
	while (Slot > 0 && List[Slot - 1].MinX > MinX)
	{
		--Slot;
	}
	List.Insert(FListEntry{MinX, Index}, Slot);
	for (int32 Moved = Slot; Moved < List.Num(); ++Moved)
	{
		ListSlots[List[Moved].BodyIndex] = Moved;
	}
}

void FPhysSceneBroadphase::RemoveFromList(int32 Index)
{
	const int32 Slot = ListSlots[Index];
	if (Slot == INDEX_NONE)
	{
		return;
	}
	List.RemoveAt(Slot, 1, false);
	ListSlots[Index] = INDEX_NONE;
	for (int32 Moved = Slot; Moved < List.Num(); ++Moved)
	{
		ListSlots[List[Moved].BodyIndex] = Moved;
	}
}

void FPhysSceneBroadphase::ResortListEntry(int32 Slot)
{
	while (Slot > 0 && List[Slot - 1].MinX > List[Slot].MinX)
	{
		Swap(List[Slot - 1], List[Slot]);
		ListSlots[List[Slot].BodyIndex] = Slot;
		--Slot;
		ListSlots[List[Slot].BodyIndex] = Slot;
	}
	while (Slot + 1 < List.Num() && List[Slot + 1].MinX < List[Slot].MinX)
	{
		Swap(List[Slot + 1], List[Slot]);
		ListSlots[List[Slot].BodyIndex] = Slot;
		++Slot;
		ListSlots[List[Slot].BodyIndex] = Slot;
	}
}

void FPhysSceneBroadphase::RemoveFromTree(int32 Index)
{
	if (TreeSlots[Index] != INDEX_NONE)
	{
		Tree.SetItem(TreeSlots[Index], INDEX_NONE);
		TreeSlots[Index] = INDEX_NONE;
		++NumDeadTreeItems;
	}
}
