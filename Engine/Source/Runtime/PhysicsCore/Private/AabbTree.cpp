#include "AabbTree.h"

#include "Templates/Sorting.h"

/** An item while the tree is built: its box's centre and its index in the caller's arrays. */
struct FAabbTreeBuildItem
{
	FVector Center;
	int32 Source;
};

namespace
{

	/** A segment component below this (cm) is tested as a slab around the start. */
	constexpr float ParallelDirection = 1.0e-6f;

} // namespace

FAabbTreeSegment::FAabbTreeSegment(const FVector& InStart, const FVector& InEnd)
	: Start(InStart)
	, Dir(InEnd - InStart)
	, InvDir(FVector::ZeroVector)
	, bParallel{false, false, false}
{
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		bParallel[Axis] = FMath::Abs(Dir[Axis]) < ParallelDirection;
		InvDir[Axis] = bParallel[Axis] ? 0.0f : 1.0f / Dir[Axis];
	}
}

void FAabbTree::Reset()
{
	Nodes.Reset();
	Items.Reset();
	bBuilt = false;
}

void FAabbTree::Build(const FBox* Bounds, const int32* Ids, int32 Num)
{
	Reset();
	bBuilt = true;
	if (Num <= 0)
	{
		return;
	}
	TArray<FAabbTreeBuildItem> Work;
	Work.SetNumUninitialized(Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		Work[Index].Center = (Bounds[Index].Min + Bounds[Index].Max) * 0.5f;
		Work[Index].Source = Index;
	}
	Nodes.Reserve((2 * Num) / MaxLeafItems + 1);
	Items.Reserve(Num);
	(void)BuildNode(Work.GetData(), 0, Num, Bounds, Ids, 0);
}

int32 FAabbTree::BuildNode(
	FAabbTreeBuildItem* Work, int32 Begin, int32 End, const FBox* Bounds, const int32* Ids, int32 Depth)
{
	checkf(Depth < MaxDepth - 1, TEXT("FAabbTree: %d levels"), Depth);
	const int32 NodeIndex = Nodes.AddDefaulted();
	FVector Min = Bounds[Work[Begin].Source].Min;
	FVector Max = Bounds[Work[Begin].Source].Max;
	FVector CenterMin = Work[Begin].Center;
	FVector CenterMax = Work[Begin].Center;
	for (int32 Index = Begin + 1; Index < End; ++Index)
	{
		const FBox& Box = Bounds[Work[Index].Source];
		const FVector& Center = Work[Index].Center;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Min[Axis] = FMath::Min(Min[Axis], Box.Min[Axis]);
			Max[Axis] = FMath::Max(Max[Axis], Box.Max[Axis]);
			CenterMin[Axis] = FMath::Min(CenterMin[Axis], Center[Axis]);
			CenterMax[Axis] = FMath::Max(CenterMax[Axis], Center[Axis]);
		}
	}
	Nodes[NodeIndex].Min = Min;
	Nodes[NodeIndex].Max = Max;

	if (End - Begin <= MaxLeafItems)
	{
		Nodes[NodeIndex].Index = Items.Num();
		Nodes[NodeIndex].NumItems = End - Begin;
		for (int32 Index = Begin; Index < End; ++Index)
		{
			Items.Add(Ids[Work[Index].Source]);
		}
		return NodeIndex;
	}

	// The median along the widest spread of the centres; the source index breaks ties, so the tree does not depend on
	// the sort's algorithm.
	const FVector Spread = CenterMax - CenterMin;
	const int32 Axis = Spread.X >= Spread.Y && Spread.X >= Spread.Z ? 0 : (Spread.Y >= Spread.Z ? 1 : 2);
	Sort(Work + Begin, End - Begin, [Axis](const FAabbTreeBuildItem& A, const FAabbTreeBuildItem& B)
		{ return A.Center[Axis] < B.Center[Axis] || (A.Center[Axis] == B.Center[Axis] && A.Source < B.Source); });
	const int32 Middle = Begin + ((End - Begin) / 2);
	(void)BuildNode(Work, Begin, Middle, Bounds, Ids, Depth + 1);
	const int32 Second = BuildNode(Work, Middle, End, Bounds, Ids, Depth + 1);
	Nodes[NodeIndex].Index = Second;
	Nodes[NodeIndex].NumItems = 0;
	return NodeIndex;
}
