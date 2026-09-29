#pragma once

#include "CoreMinimal.h"

struct FAabbTreeBuildItem;

/**
 * A node of FAabbTree. An internal node's children are the next node and the node at Index; a leaf holds NumItems
 * items of the tree from Index. Flat, with no pointers, so a built tree can be saved as it is (the level's cooked
 * collision).
 */
struct PHYSICSCORE_API FAabbTreeNode
{
	FVector Min = FVector::ZeroVector;
	/** Internal node: the second child. Leaf: the first item. */
	int32 Index = 0;
	FVector Max = FVector::ZeroVector;
	/** 0 for an internal node. */
	int32 NumItems = 0;
};

/**
 * A segment prepared for box tests: Start + Dir * T. An axis the segment barely moves along is tested as a slab around
 * Start.
 */
struct PHYSICSCORE_API FAabbTreeSegment
{
	FAabbTreeSegment(const FVector& InStart, const FVector& InEnd);

	/**
	 * Whether the segment, for T in [0, MaxT], crosses the box [Min, Max]; OutTEnter is where it enters (0 when it
	 * starts inside).
	 */
	[[nodiscard]] FORCEINLINE bool Crosses(const FVector& Min, const FVector& Max, float MaxT, float& OutTEnter) const
	{
		float TEnter = 0.0f;
		float TExit = MaxT;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (bParallel[Axis])
			{
				if (Start[Axis] < Min[Axis] || Start[Axis] > Max[Axis])
				{
					return false;
				}
				continue;
			}
			float T0 = (Min[Axis] - Start[Axis]) * InvDir[Axis];
			float T1 = (Max[Axis] - Start[Axis]) * InvDir[Axis];
			if (T0 > T1)
			{
				Swap(T0, T1);
			}
			TEnter = FMath::Max(TEnter, T0);
			TExit = FMath::Min(TExit, T1);
			if (TEnter > TExit)
			{
				return false;
			}
		}
		OutTEnter = TEnter;
		return true;
	}

	FVector Start;
	FVector Dir;
	FVector InvDir;
	bool bParallel[3];
};

/**
 * A bounding volume hierarchy of axis-aligned boxes over numbered items (Leon; UE's physics keeps AABB trees for its
 * scene queries): built once, top down, splitting at the median of the item centres along their widest axis, with at
 * most MaxLeafItems items per leaf. Queries visit the items of the leaves a box or a segment reaches, the nearest
 * leaves first for a segment; an item can be renumbered or removed (INDEX_NONE) in place without a rebuild. The
 * boxes must contain whatever a query tests the items for: the tree only culls.
 */
class PHYSICSCORE_API FAabbTree
{
public:
	/** Items per leaf. */
	static constexpr int32 MaxLeafItems = 4;
	/** Deepest tree a query walks (a median split of 2^31 items stays far below it). */
	static constexpr int32 MaxDepth = 64;

	void Reset();

	/** Builds the tree over Num items: the item I has the box Bounds[I] and is reported as Ids[I]. */
	void Build(const FBox* Bounds, const int32* Ids, int32 Num);

	/** Built since the last Reset (a tree over no item is built and empty). */
	[[nodiscard]] bool IsBuilt() const
	{
		return bBuilt;
	}

	[[nodiscard]] const TArray<FAabbTreeNode>& GetNodes() const
	{
		return Nodes;
	}

	/** The items in leaf order; an item's slot is its index here. */
	[[nodiscard]] const TArray<int32>& GetItems() const
	{
		return Items;
	}

	/** Renumbers the item in Slot; INDEX_NONE removes it (its leaf keeps its box). */
	void SetItem(int32 Slot, int32 Id)
	{
		Items[Slot] = Id;
	}

	/** Visit(Id) for every item whose leaf overlaps Box (closed intervals). */
	template <typename VisitorType>
	void ForEachOverlap(const FBox& Box, VisitorType&& Visit) const
	{
		if (Nodes.Num() == 0)
		{
			return;
		}
		int32 Stack[MaxDepth];
		int32 Top = 0;
		Stack[Top++] = 0;
		while (Top > 0)
		{
			const FAabbTreeNode& Node = Nodes[Stack[--Top]];
			if (Node.Min.X > Box.Max.X || Node.Max.X < Box.Min.X || Node.Min.Y > Box.Max.Y || Node.Max.Y < Box.Min.Y ||
				Node.Min.Z > Box.Max.Z || Node.Max.Z < Box.Min.Z)
			{
				continue;
			}
			if (Node.NumItems > 0)
			{
				for (int32 Slot = Node.Index; Slot < Node.Index + Node.NumItems; ++Slot)
				{
					if (Items[Slot] != INDEX_NONE)
					{
						Visit(Items[Slot]);
					}
				}
				continue;
			}
			const int32 NodeIndex = static_cast<int32>(&Node - Nodes.GetData());
			Stack[Top++] = Node.Index;
			Stack[Top++] = NodeIndex + 1;
		}
	}

	/**
	 * Visit(Id, MaxT) for every item whose leaf, grown by Expand on each side, the segment crosses before MaxT, nearest
	 * leaves first. Visit returns the new MaxT (the same to see every item; a nearer one to prune what lies beyond).
	 */
	template <typename VisitorType>
	void ForEachSegmentHit(
		const FAabbTreeSegment& Segment, const FVector& Expand, float MaxT, VisitorType&& Visit) const
	{
		if (Nodes.Num() == 0)
		{
			return;
		}
		float TEnter = 0.0f;
		if (!Segment.Crosses(Nodes[0].Min - Expand, Nodes[0].Max + Expand, MaxT, TEnter))
		{
			return;
		}
		int32 StackNodes[MaxDepth];
		float StackTimes[MaxDepth];
		int32 Top = 0;
		StackNodes[Top] = 0;
		StackTimes[Top++] = TEnter;
		while (Top > 0)
		{
			--Top;
			if (StackTimes[Top] > MaxT)
			{
				continue;
			}
			const int32 NodeIndex = StackNodes[Top];
			const FAabbTreeNode& Node = Nodes[NodeIndex];
			if (Node.NumItems > 0)
			{
				for (int32 Slot = Node.Index; Slot < Node.Index + Node.NumItems; ++Slot)
				{
					if (Items[Slot] != INDEX_NONE)
					{
						MaxT = Visit(Items[Slot], MaxT);
					}
				}
				continue;
			}
			const int32 First = NodeIndex + 1;
			const int32 Second = Node.Index;
			float TFirst = 0.0f;
			float TSecond = 0.0f;
			const bool bFirst = Segment.Crosses(Nodes[First].Min - Expand, Nodes[First].Max + Expand, MaxT, TFirst);
			const bool bSecond = Segment.Crosses(Nodes[Second].Min - Expand, Nodes[Second].Max + Expand, MaxT, TSecond);
			// The nearer child is pushed last, so it is walked first.
			if (bFirst && bSecond && TFirst <= TSecond)
			{
				StackNodes[Top] = Second;
				StackTimes[Top++] = TSecond;
				StackNodes[Top] = First;
				StackTimes[Top++] = TFirst;
			}
			else if (bFirst && bSecond)
			{
				StackNodes[Top] = First;
				StackTimes[Top++] = TFirst;
				StackNodes[Top] = Second;
				StackTimes[Top++] = TSecond;
			}
			else if (bFirst)
			{
				StackNodes[Top] = First;
				StackTimes[Top++] = TFirst;
			}
			else if (bSecond)
			{
				StackNodes[Top] = Second;
				StackTimes[Top++] = TSecond;
			}
		}
	}

private:
	/** Builds the node over Work[Begin, End) and returns its index. */
	int32 BuildNode(
		FAabbTreeBuildItem* Work, int32 Begin, int32 End, const FBox* Bounds, const int32* Ids, int32 Depth);

	TArray<FAabbTreeNode> Nodes;
	TArray<int32> Items;
	bool bBuilt = false;
};
