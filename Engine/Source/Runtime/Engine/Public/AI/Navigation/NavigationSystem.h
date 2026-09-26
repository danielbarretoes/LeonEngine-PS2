#pragma once

#include "CollisionQuery.h"
#include "CoreMinimal.h"

class AActor;
class ANavigationWaypoint;
class FDebugDraw;
class FPhysScene;
class UNavigationPath;
class UObject;
class UWorld;

/**
 * What a walk between two waypoints must allow (UNavigationSystem::CanWalkBetween, AutoLinkWaypoints): the agent's
 * standing capsule, the step it takes without jumping, the height it can jump onto, the drop it survives, and how
 * far apart two waypoints may be to be linked. The defaults are ShooterGame's character (CS's hull, a 45 cm step, a
 * jump onto a 1.1 m crate).
 */
struct ENGINE_API FWaypointLinkParams
{
	/** The standing agent's capsule (radius, half height with the caps), cm. */
	float AgentRadius = 40.0f;
	float AgentHalfHeight = 91.5f;
	/** Climbed walking, cm. */
	float MaxStepHeight = 45.0f;
	/** Climbed with a jump (a link up to this is a jump link), cm. */
	float MaxJumpHeight = 115.0f;
	/** Dropped down (a one-way link), cm. */
	float MaxDropHeight = 300.0f;
	/** The longest link, cm. */
	float MaxLinkDistance = 2000.0f;
	/** The floor is probed along the walk every this many cm (a gap or a hole breaks the walk). */
	float FloorProbeSpacing = 50.0f;
	/** How far below a point its floor is looked for, cm. */
	float FloorSearchDistance = 400.0f;
	/** The channel the capsule sweeps on (the character's object type). */
	ECollisionChannel Channel = ECC_Pawn;
};

/**
 * Leon's navigation (UE: UNavigationSystemV1 over a navmesh; plan P20 replaces the grid navmesh of the legacy engine
 * with a waypoint graph, the UE3 path node model). The graph is the level's ANavigationWaypoint actors and their Links,
 * built when the world begins play (Build): each waypoint is a node standing on the floor below it.
 *
 * - FindPath: the start's and the end's nearest waypoints that can be walked to (CanWalkBetween: a capsule sweep and a
 *   floor probe), A* between them over the links (costs and heuristic: the distance), then the end; a start that can
 *   walk straight to the end, or to the second waypoint, skips the waypoints before. The path's points stand on the
 *   floor.
 * - FindPathToLocationSynchronously (UE's name): the same as a UNavigationPath.
 * - AutoLinkWaypoints: links every pair of waypoints within MaxLinkDistance that an agent can walk between both ways
 *   (a step or a jump up and back down), and one way down a drop too high to climb; the map importer runs it, so
 *   the links are saved in the map.
 *
 * Not a UObject (as the grid navigation was): the world owns it by value and the waypoints own nothing.
 */
class ENGINE_API UNavigationSystem
{
public:
	/** A node of the graph: a waypoint's floor point and its links. */
	struct FNode
	{
		/** The floor below the waypoint (the waypoint's own location without a floor). */
		FVector Location = FVector::ZeroVector;
		/** The indices of the nodes this one links to (one way). */
		TArray<int32> Links;
		/** The waypoint's flags. */
		TArray<FName> Flags;
	};

	/** The graph of a world's persistent level (its waypoints, their links) and its physics for the queries. */
	void Build(const UWorld& World);
	void Clear();

	[[nodiscard]] bool HasNavigationData() const
	{
		return Nodes.Num() > 0;
	}
	[[nodiscard]] const TArray<FNode>& GetNodes() const
	{
		return Nodes;
	}
	[[nodiscard]] int32 GetNumLinks() const;
	/** The node of a waypoint, INDEX_NONE when it is not in the graph. */
	[[nodiscard]] int32 FindNode(const ANavigationWaypoint* Waypoint) const;

	/** The link parameters of the reachability tests (the defaults: FWaypointLinkParams). */
	void SetLinkParams(const FWaypointLinkParams& InParams)
	{
		Params = InParams;
	}
	[[nodiscard]] const FWaypointLinkParams& GetLinkParams() const
	{
		return Params;
	}

	/**
	 * The nearest node an agent at Location can walk to (bRequireWalk; any nearest otherwise), INDEX_NONE without one.
	 * Ties go to the lower index.
	 */
	[[nodiscard]] int32 FindNearestNode(const FVector& Location, bool bRequireWalk = true) const;
	/** The nearest node's point (UE: ProjectPointToNavigation); false without navigation data. */
	[[nodiscard]] bool ProjectPointToNavigation(const FVector& Point, FVector& OutProjected) const;
	/** A path from Start to End (see the class comment): its points, the end last; false without one (cleared). */
	[[nodiscard]] bool FindPath(const FVector& Start, const FVector& End, TArray<FVector>& OutPath) const;

	/** A* over the nodes' links from From to To (the node indices, both included); false when To is unreachable. */
	[[nodiscard]] static bool FindNodePath(const TArray<FNode>& InNodes, int32 From, int32 To, TArray<int32>& OutPath);

	/**
	 * UE's UNavigationSystemV1::FindPathToLocationSynchronously: the path in the world of WorldContextObject, as a
	 * transient UNavigationPath (IsValid false without one); null without a world.
	 */
	[[nodiscard]] static UNavigationPath* FindPathToLocationSynchronously(
		UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd);

	/**
	 * Whether an agent can walk from From to To (points at or above their floors): their floors within reach (a step,
	 * a jump up, a drop down), the standing capsule swept between them a step above the higher floor meets nothing
	 * on Params.Channel, and the floor under the way never falls more than a step below the lower end (no gaps).
	 * Ignore: actors the sweep passes through.
	 */
	[[nodiscard]] static bool CanWalkBetween(const FPhysScene& Physics, const FVector& From, const FVector& To,
		const FWaypointLinkParams& InParams, const TArray<const AActor*>& Ignore = TArray<const AActor*>());

	/** The floor below Point within Params.FloorSearchDistance; false over nothing. */
	[[nodiscard]] static bool FindFloorBelow(
		const FPhysScene& Physics, const FVector& Point, const FWaypointLinkParams& InParams, FVector& OutFloor);

	/**
	 * Links the waypoints of World's persistent level (see the class comment); keeps the links they have. Returns
	 * the links added.
	 */
	static int32 AutoLinkWaypoints(UWorld& World, const FWaypointLinkParams& InParams = FWaypointLinkParams());

	/** Draws the nodes (small boxes) and the links (arrows) at the floor (F3 / debug overlay). */
	void AppendDebugDraw(FDebugDraw& Draw) const;

private:
	TArray<FNode> Nodes;
	/** The waypoint of each node (the level keeps them alive; compared, never dereferenced after a world's end). */
	TArray<const ANavigationWaypoint*> Waypoints;
	const FPhysScene* Physics = nullptr;
	FWaypointLinkParams Params;
};
