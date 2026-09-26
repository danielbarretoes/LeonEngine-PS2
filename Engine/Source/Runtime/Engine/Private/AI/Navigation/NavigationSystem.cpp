#include "AI/Navigation/NavigationSystem.h"

#include "AI/Navigation/NavigationPath.h"
#include "AI/Navigation/NavigationWaypoint.h"
#include "CollisionShape.h"
#include "Debug/DebugDraw.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Physics/PhysScene.h"

DEFINE_LOG_CATEGORY_STATIC(LogNavigation, Log, All);

namespace
{

	/** How far above a point its floor trace starts (a point on its floor, or a hair inside it), cm. */
	constexpr float FloorTraceLift = 10.0f;

	/** A floor's normal must face up this much (UE's walkable floor angle, about 45 degrees). */
	constexpr float WalkableFloorNormalZ = 0.7f;

	/** The clearance above the step the walk's capsule sweeps at, cm. */
	constexpr float SweepClearance = 2.0f;

	/** How many of the nearest nodes FindNearestNode tries to walk to before settling for the nearest. */
	constexpr int32 MaxWalkCandidates = 8;

	/** The indices of Nodes sorted by the distance of their location to Point (ties by index). */
	TArray<int32> SortByDistance(const TArray<UNavigationSystem::FNode>& Nodes, const FVector& Point)
	{
		TArray<int32> Order;
		Order.Reserve(Nodes.Num());
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			Order.Add(Index);
		}
		Order.Sort(
			[&Nodes, &Point](int32 A, int32 B)
			{
				const float DistA = FVector::DistSquared(Nodes[A].Location, Point);
				const float DistB = FVector::DistSquared(Nodes[B].Location, Point);
				return DistA < DistB || (DistA == DistB && A < B);
			});
		return Order;
	}

} // namespace

// Build

void UNavigationSystem::Clear()
{
	Nodes.Reset();
	Waypoints.Reset();
	Physics = nullptr;
}

FWaypointLinkParams FWaypointLinkParams::FromConfig()
{
	FWaypointLinkParams Result;
	if (GConfig == nullptr)
	{
		return Result;
	}
	const TCHAR* Section = TEXT("/Script/Engine.NavigationSystem");
	float AgentHeight = Result.AgentHalfHeight * 2.0f;
	(void)GConfig->GetFloat(Section, TEXT("AgentRadius"), Result.AgentRadius, GEngineIni);
	if (GConfig->GetFloat(Section, TEXT("AgentHeight"), AgentHeight, GEngineIni))
	{
		Result.AgentHalfHeight = AgentHeight * 0.5f;
	}
	(void)GConfig->GetFloat(Section, TEXT("AgentMaxStepHeight"), Result.MaxStepHeight, GEngineIni);
	(void)GConfig->GetFloat(Section, TEXT("AgentMaxJumpHeight"), Result.MaxJumpHeight, GEngineIni);
	(void)GConfig->GetFloat(Section, TEXT("AgentMaxDropHeight"), Result.MaxDropHeight, GEngineIni);
	(void)GConfig->GetFloat(Section, TEXT("MaxLinkDistance"), Result.MaxLinkDistance, GEngineIni);
	return Result;
}

void UNavigationSystem::Build(const UWorld& World)
{
	Clear();
	Params = FWaypointLinkParams::FromConfig();
	Physics = &World.GetPhysicsScene();
	if (World.PersistentLevel == nullptr)
	{
		return;
	}
	TMap<const ANavigationWaypoint*, int32> IndexOf;
	for (const AActor* Actor : World.PersistentLevel->Actors)
	{
		const ANavigationWaypoint* Waypoint = Cast<ANavigationWaypoint>(Actor);
		if (Waypoint == nullptr || Waypoint->IsPendingKillPending())
		{
			continue;
		}
		FNode& Node = Nodes.AddDefaulted_GetRef();
		FVector Floor;
		Node.Location = FindFloorBelow(*Physics, Waypoint->GetActorLocation(), Params, Floor)
			? Floor
			: Waypoint->GetActorLocation();
		Node.Flags = Waypoint->Flags;
		IndexOf.Add(Waypoint, Waypoints.Num());
		Waypoints.Add(Waypoint);
	}
	for (int32 Index = 0; Index < Waypoints.Num(); ++Index)
	{
		for (const ANavigationWaypoint* Linked : Waypoints[Index]->Links)
		{
			if (const int32* LinkedIndex = IndexOf.Find(Linked))
			{
				Nodes[Index].Links.AddUnique(*LinkedIndex);
			}
		}
	}
	UE_LOG(LogNavigation, Log, TEXT("Navigation: %d waypoint(s), %d link(s)"), Nodes.Num(), GetNumLinks());
}

int32 UNavigationSystem::GetNumLinks() const
{
	int32 Count = 0;
	for (const FNode& Node : Nodes)
	{
		Count += Node.Links.Num();
	}
	return Count;
}

int32 UNavigationSystem::FindNode(const ANavigationWaypoint* Waypoint) const
{
	return Waypoints.Find(Waypoint);
}

// Queries

bool UNavigationSystem::FindFloorBelow(
	const FPhysScene& InPhysics, const FVector& Point, const FWaypointLinkParams& InParams, FVector& OutFloor)
{
	const FCollisionQueryParams Query(FName(TEXT("NavigationFloor")));
	FHitResult Hit;
	const FVector Start = Point + FVector(0.0f, 0.0f, FloorTraceLift);
	const FVector End = Point - FVector(0.0f, 0.0f, InParams.FloorSearchDistance);
	if (!InPhysics.LineTraceSingleByChannel(Hit, Start, End, InParams.Channel, Query) ||
		Hit.ImpactNormal.Z < WalkableFloorNormalZ)
	{
		return false;
	}
	OutFloor = Hit.ImpactPoint;
	return true;
}

bool UNavigationSystem::CanWalkBetween(const FPhysScene& InPhysics, const FVector& From, const FVector& To,
	const FWaypointLinkParams& InParams, const TArray<const AActor*>& Ignore)
{
	FVector FloorFrom;
	FVector FloorTo;
	if (!FindFloorBelow(InPhysics, From, InParams, FloorFrom) || !FindFloorBelow(InPhysics, To, InParams, FloorTo))
	{
		return false;
	}
	const float Rise = FloorTo.Z - FloorFrom.Z;
	if (Rise > InParams.MaxJumpHeight || -Rise > InParams.MaxDropHeight)
	{
		return false;
	}

	// The standing capsule, its feet a step above the higher floor, swept from one end to the other.
	FCollisionQueryParams Query(FName(TEXT("NavigationWalk")));
	for (const AActor* Actor : Ignore)
	{
		Query.AddIgnoredActor(Actor);
	}
	const float HighFloor = FMath::Max(FloorFrom.Z, FloorTo.Z);
	const float CenterZ = HighFloor + InParams.MaxStepHeight + SweepClearance + InParams.AgentHalfHeight;
	const FVector SweepStart(FloorFrom.X, FloorFrom.Y, CenterZ);
	const FVector SweepEnd(FloorTo.X, FloorTo.Y, CenterZ);
	if (!SweepStart.Equals(SweepEnd, 0.1f))
	{
		FHitResult Hit;
		if (InPhysics.SweepSingleByChannel(Hit, SweepStart, SweepEnd, FQuat::Identity, InParams.Channel,
				FCollisionShape::MakeCapsule(InParams.AgentRadius, InParams.AgentHalfHeight), Query))
		{
			return false;
		}
	}

	// No gap: the floor under the way stays within a step below the lower end.
	const float LowestFloor = FMath::Min(FloorFrom.Z, FloorTo.Z) - InParams.MaxStepHeight;
	const float Distance2D = FVector::Dist2D(FloorFrom, FloorTo);
	const int32 NumProbes =
		FMath::Max(0, FMath::CeilToInt(Distance2D / FMath::Max(1.0f, InParams.FloorProbeSpacing)) - 1);
	for (int32 Probe = 1; Probe <= NumProbes; ++Probe)
	{
		const float Alpha = static_cast<float>(Probe) / static_cast<float>(NumProbes + 1);
		const FVector Point(FMath::Lerp(FloorFrom.X, FloorTo.X, Alpha), FMath::Lerp(FloorFrom.Y, FloorTo.Y, Alpha),
			CenterZ - InParams.AgentHalfHeight);
		FVector Floor;
		if (!FindFloorBelow(InPhysics, Point, InParams, Floor) || Floor.Z < LowestFloor)
		{
			return false;
		}
	}
	return true;
}

int32 UNavigationSystem::FindNearestNode(const FVector& Location, bool bRequireWalk, bool bFromNode) const
{
	if (Nodes.Num() == 0)
	{
		return INDEX_NONE;
	}
	const TArray<int32> Order = SortByDistance(Nodes, Location);
	if (!bRequireWalk || Physics == nullptr)
	{
		return Order[0];
	}
	const int32 NumCandidates = FMath::Min(MaxWalkCandidates, Order.Num());
	for (int32 Candidate = 0; Candidate < NumCandidates; ++Candidate)
	{
		const FVector& NodeLocation = Nodes[Order[Candidate]].Location;
		if (bFromNode ? CanWalkBetween(*Physics, NodeLocation, Location, Params)
					  : CanWalkBetween(*Physics, Location, NodeLocation, Params))
		{
			return Order[Candidate];
		}
	}
	// Nothing walkable near: the nearest anyway (the path follower's straight line may still get there).
	return Order[0];
}

bool UNavigationSystem::ProjectPointToNavigation(const FVector& Point, FVector& OutProjected) const
{
	const int32 Node = FindNearestNode(Point, false);
	if (Node == INDEX_NONE)
	{
		return false;
	}
	OutProjected = Nodes[Node].Location;
	return true;
}

bool UNavigationSystem::FindNodePath(const TArray<FNode>& InNodes, int32 From, int32 To, TArray<int32>& OutPath)
{
	OutPath.Reset();
	if (!InNodes.IsValidIndex(From) || !InNodes.IsValidIndex(To))
	{
		return false;
	}
	const int32 Num = InNodes.Num();
	TArray<float> Cost;
	TArray<float> Estimate;
	TArray<int32> CameFrom;
	TArray<uint8> Closed;
	Cost.Init(TNumericLimits<float>::Max(), Num);
	Estimate.Init(TNumericLimits<float>::Max(), Num);
	CameFrom.Init(INDEX_NONE, Num);
	Closed.Init(0, Num);
	TArray<int32> Open;
	Cost[From] = 0.0f;
	Estimate[From] = FVector::Dist(InNodes[From].Location, InNodes[To].Location);
	Open.Add(From);
	while (Open.Num() > 0)
	{
		// The open node with the lowest estimate (ties: the lower index), for a deterministic order.
		int32 BestSlot = 0;
		for (int32 Slot = 1; Slot < Open.Num(); ++Slot)
		{
			const int32 Candidate = Open[Slot];
			const int32 Best = Open[BestSlot];
			if (Estimate[Candidate] < Estimate[Best] || (Estimate[Candidate] == Estimate[Best] && Candidate < Best))
			{
				BestSlot = Slot;
			}
		}
		const int32 Current = Open[BestSlot];
		Open.RemoveAtSwap(BestSlot);
		if (Current == To)
		{
			for (int32 Node = To; Node != INDEX_NONE; Node = CameFrom[Node])
			{
				OutPath.Insert(Node, 0);
			}
			return true;
		}
		Closed[Current] = 1;
		for (const int32 Next : InNodes[Current].Links)
		{
			if (!InNodes.IsValidIndex(Next) || Closed[Next] != 0)
			{
				continue;
			}
			const float NewCost = Cost[Current] + FVector::Dist(InNodes[Current].Location, InNodes[Next].Location);
			if (NewCost < Cost[Next])
			{
				Cost[Next] = NewCost;
				Estimate[Next] = NewCost + FVector::Dist(InNodes[Next].Location, InNodes[To].Location);
				CameFrom[Next] = Current;
				Open.AddUnique(Next);
			}
		}
	}
	return false;
}

bool UNavigationSystem::FindPath(
	const FVector& Start, const FVector& End, TArray<FVector>& OutPath, TArray<int32>* OutNodes) const
{
	OutPath.Reset();
	if (OutNodes != nullptr)
	{
		OutNodes->Reset();
	}
	if (Physics != nullptr && FVector::Dist(Start, End) <= Params.MaxLinkDistance &&
		CanWalkBetween(*Physics, Start, End, Params))
	{
		OutPath.Add(End);
		if (OutNodes != nullptr)
		{
			OutNodes->Add(INDEX_NONE);
		}
		return true;
	}
	const int32 From = FindNearestNode(Start);
	const int32 To = FindNearestNode(End, true, true);
	TArray<int32> NodePath;
	if (From == INDEX_NONE || To == INDEX_NONE || !FindNodePath(Nodes, From, To, NodePath))
	{
		return false;
	}
	// The start between the first two waypoints goes straight to the second; the end between the last two is reached
	// from the one before the last.
	int32 First = 0;
	int32 Last = NodePath.Num() - 1;
	if (Physics != nullptr && NodePath.Num() >= 2 &&
		CanWalkBetween(*Physics, Start, Nodes[NodePath[1]].Location, Params))
	{
		First = 1;
	}
	if (Physics != nullptr && Last - First >= 1 &&
		CanWalkBetween(*Physics, Nodes[NodePath[Last - 1]].Location, End, Params))
	{
		--Last;
	}
	for (int32 Index = First; Index <= Last; ++Index)
	{
		OutPath.Add(Nodes[NodePath[Index]].Location);
		if (OutNodes != nullptr)
		{
			OutNodes->Add(NodePath[Index]);
		}
	}
	OutPath.Add(End);
	if (OutNodes != nullptr)
	{
		OutNodes->Add(INDEX_NONE);
	}
	return true;
}

UNavigationPath* UNavigationSystem::FindPathToLocationSynchronously(
	UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd)
{
	UWorld* World = UGameplayStatics::GetWorldFromContextObject(WorldContextObject);
	if (World == nullptr)
	{
		return nullptr;
	}
	UNavigationPath* Path = NewObject<UNavigationPath>(World);
	TArray<FVector> Points;
	if (World->GetNavigationSystem().FindPath(PathStart, PathEnd, Points))
	{
		Path->PathPoints.Add(PathStart);
		Path->PathPoints.Append(Points);
	}
	return Path;
}

int32 UNavigationSystem::AutoLinkWaypoints(UWorld& World, const FWaypointLinkParams& InParams)
{
	if (World.PersistentLevel == nullptr)
	{
		return 0;
	}
	TArray<ANavigationWaypoint*> Found;
	for (AActor* Actor : World.PersistentLevel->Actors)
	{
		if (ANavigationWaypoint* Waypoint = Cast<ANavigationWaypoint>(Actor))
		{
			Found.Add(Waypoint);
		}
	}
	const FPhysScene& Scene = World.GetPhysicsScene();
	int32 Added = 0;
	for (ANavigationWaypoint* From : Found)
	{
		for (ANavigationWaypoint* To : Found)
		{
			if (From == To || From->Links.Contains(To) ||
				FVector::Dist(From->GetActorLocation(), To->GetActorLocation()) > InParams.MaxLinkDistance)
			{
				continue;
			}
			if (!CanWalkBetween(Scene, From->GetActorLocation(), To->GetActorLocation(), InParams))
			{
				continue;
			}
			// A walk or a jump must also be walkable back (a capsule that starts against a wall sees it, one that
			// ends against it may not: the link would be one way by accident); only a drop too high to climb back
			// stays one way.
			FVector FloorFrom;
			FVector FloorTo;
			const bool bFloors = FindFloorBelow(Scene, From->GetActorLocation(), InParams, FloorFrom) &&
				FindFloorBelow(Scene, To->GetActorLocation(), InParams, FloorTo);
			const bool bDropOnly = bFloors && FloorFrom.Z - FloorTo.Z > InParams.MaxJumpHeight;
			if (bDropOnly || CanWalkBetween(Scene, To->GetActorLocation(), From->GetActorLocation(), InParams))
			{
				From->Links.Add(To);
				++Added;
			}
		}
	}
	UE_LOG(LogNavigation, Log, TEXT("AutoLinkWaypoints: %d waypoint(s), %d link(s) added"), Found.Num(), Added);
	return Added;
}

void UNavigationSystem::AppendDebugDraw(FDebugDraw& Draw) const
{
	const FLinearColor NodeColor(0.2f, 1.0f, 0.3f);
	const FLinearColor LinkColor(0.2f, 0.7f, 1.0f);
	const FVector Lift(0.0f, 0.0f, 20.0f);
	const FVector Extent(15.0f, 15.0f, 15.0f);
	for (const FNode& Node : Nodes)
	{
		Draw.AddAabb(Node.Location + Lift - Extent, Node.Location + Lift + Extent, NodeColor);
		for (const int32 Link : Node.Links)
		{
			if (Nodes.IsValidIndex(Link))
			{
				Draw.AddArrow(Node.Location + Lift, Nodes[Link].Location + Lift, LinkColor);
			}
		}
	}
}
