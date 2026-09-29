#pragma once

#include "AabbTree.h"
#include "BodyInstance.h"
#include "CoreMinimal.h"
#include "TriangleCollision.h"

/**
 * Where FPhysScene finds the bodies a query or a contact can touch (Leon; UE: the scene query structures of the
 * physics scene, a static pruner and a dynamic one). The bodies that do not move are in an AABB tree, built when
 * static bodies were added (loading a map), not every frame; the bodies that move (dynamic ones, movable components,
 * and a static body the first time it moves) are in a list sorted on the X of their boxes, kept sorted as they move
 * (sort and sweep). Every body is in one of the two, by the index FPhysScene gives it. The boxes are grown a little for
 * rounding, so the broadphase only culls: the scene's own tests decide.
 */
class ENGINE_API FPhysSceneBroadphase
{
public:
	/**
	 * The box a body can be touched in, grown for rounding: its AABB; around a capsule its upright capsule (a sphere's
	 * is taller than its AABB when HalfExtents.Z is below the radius); around a triangle mesh (Mesh, when the body is
	 * one) its triangles too, which may reach out of its box.
	 */
	[[nodiscard]] static FBox CalcBodyBounds(const FBodyInstance& Body, const FTriangleMeshCollision* Mesh);

	/** The triangles of a TriangleMesh body among Meshes (parallel to the bodies), or null. */
	[[nodiscard]] static const FTriangleMeshCollision* GetBodyMesh(
		const FBodyInstance& Body, int32 Index, const TArray<FTriangleMeshCollision>& Meshes);

	void Reset();

	/**
	 * Adds a body with the next index: a movable one to the sorted list, a static one to the tree at the next Flush.
	 */
	void AddBody(const FBodyInstance& Body, const FTriangleMeshCollision* Mesh, bool bMovable);

	/** Removes the body at Index; the last body takes its index (as TArray::RemoveAtSwap). */
	void RemoveBodyAtSwap(int32 Index);

	/**
	 * The body at Index moved or changed its shape. A static body whose box changed leaves the tree for the list, for
	 * good; a movable one moves in the list.
	 */
	void UpdateBody(int32 Index, const FBodyInstance& Body, const FTriangleMeshCollision* Mesh);

	/**
	 * Takes every box again from Bodies and Meshes, edited in place; bodies past the known ones are added (a dynamic
	 * one movable), a dynamic body in the tree becomes movable, and the tree is rebuilt at the next Flush.
	 */
	void Refresh(const TArray<FBodyInstance>& Bodies, const TArray<FTriangleMeshCollision>& Meshes);

	/** Builds the tree when static bodies were added, or when half of its items are gone. */
	void Flush();

	[[nodiscard]] int32 GetNumBodies() const
	{
		return Bounds.Num();
	}

	[[nodiscard]] bool IsMovable(int32 Index) const
	{
		return ListSlots[Index] != INDEX_NONE;
	}

	/** Times the tree was built (tests, stats). */
	[[nodiscard]] int32 GetNumTreeBuilds() const
	{
		return NumTreeBuilds;
	}

	/** Visit(Index) for every movable body, in no particular order. */
	template <typename VisitorType>
	void ForEachMovable(VisitorType&& Visit) const
	{
		for (const FListEntry& Entry : List)
		{
			Visit(Entry.BodyIndex);
		}
	}

	/** Visit(Index) for every body whose box overlaps Box (closed intervals), each once, in no particular order. */
	template <typename VisitorType>
	void ForEachOverlap(const FBox& Box, VisitorType&& Visit) const
	{
		checkf(!bTreeDirty, TEXT("FPhysSceneBroadphase: query before Flush"));
		Tree.ForEachOverlap(Box, Visit);
		for (int32 Slot = FindFirstListSlot(Box.Min.X); Slot < List.Num() && List[Slot].MinX <= Box.Max.X; ++Slot)
		{
			const FBox& Body = Bounds[List[Slot].BodyIndex];
			if (Body.Max.X >= Box.Min.X && Body.Min.Y <= Box.Max.Y && Body.Max.Y >= Box.Min.Y &&
				Body.Min.Z <= Box.Max.Z && Body.Max.Z >= Box.Min.Z)
			{
				Visit(List[Slot].BodyIndex);
			}
		}
	}

	/**
	 * Visit(Index, MaxT) for every body whose box, grown by Expand on each side, the segment crosses before MaxT, each
	 * once; Visit returns the new MaxT (the same to see every body, a nearer one to prune).
	 */
	template <typename VisitorType>
	void ForEachSegmentHit(
		const FAabbTreeSegment& Segment, const FVector& Expand, float MaxT, VisitorType&& Visit) const
	{
		checkf(!bTreeDirty, TEXT("FPhysSceneBroadphase: query before Flush"));
		Tree.ForEachSegmentHit(Segment, Expand, MaxT,
			[&](int32 Index, float CurrentMaxT) -> float
			{
				MaxT = Visit(Index, CurrentMaxT);
				return MaxT;
			});
		const float EndX = Segment.Start.X + Segment.Dir.X;
		const float MinX = FMath::Min(Segment.Start.X, EndX) - Expand.X;
		const float MaxX = FMath::Max(Segment.Start.X, EndX) + Expand.X;
		for (int32 Slot = FindFirstListSlot(MinX); Slot < List.Num() && List[Slot].MinX <= MaxX; ++Slot)
		{
			const FBox& Body = Bounds[List[Slot].BodyIndex];
			float TEnter = 0.0f;
			if (Segment.Crosses(Body.Min - Expand, Body.Max + Expand, MaxT, TEnter))
			{
				MaxT = Visit(List[Slot].BodyIndex, MaxT);
			}
		}
	}

private:
	/** A movable body in the list: the list is sorted on MinX, its box's lowest X. */
	struct FListEntry
	{
		float MinX;
		int32 BodyIndex;
	};

	/** The first list slot whose box can reach X or beyond (its MinX is at least X minus the widest box). */
	[[nodiscard]] int32 FindFirstListSlot(float X) const;
	/** Puts the body in the list, in order. */
	void InsertInList(int32 Index);
	/** Takes the body out of the list. */
	void RemoveFromList(int32 Index);
	/** Moves the list entry in Slot to its place after its MinX changed. */
	void ResortListEntry(int32 Slot);
	/** Takes the body out of the tree (its item is left empty until the next build). */
	void RemoveFromTree(int32 Index);

	/** The static bodies. */
	FAabbTree Tree;
	/** The movable bodies, sorted on MinX. */
	TArray<FListEntry> List;
	/** Per body: its box. */
	TArray<FBox> Bounds;
	/** Per body: its item in Tree, or INDEX_NONE. */
	TArray<int32> TreeSlots;
	/** Per body: its entry in List, or INDEX_NONE (a static body). */
	TArray<int32> ListSlots;
	/** The widest box of the list on X (cm); it only grows until Refresh. */
	float ListMaxWidthX = 0.0f;
	/** Tree items of bodies that left the tree. */
	int32 NumDeadTreeItems = 0;
	/** Static bodies wait for a Flush. */
	bool bTreeDirty = false;
	int32 NumTreeBuilds = 0;
};
