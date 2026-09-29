#include "TickTaskManager.h"

#include "Components/ActorComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	/** The tick order of what is not an actor's or a component's: after every actor, in registration order. */
	constexpr uint64 SerialTickOrderBase = 1ull << 63;
	/** An actor's own tick follows its components' (the low 16 bits of its order). */
	constexpr uint64 ActorTickSubOrder = 0xFFFFull;
	/**
	 * How far below zero an interval's cool-down may stay and still be due: the steps' sum rounds (three steps of
	 * 1/30 s are not exactly 0.1 s in float), and an interval must not slip a step for it.
	 */
	constexpr float TickCooldownTolerance = 1.0e-5f;

	/** The states of FTickFunction::TickState in a step. */
	constexpr uint8 TickStateNotDue = 0;
	constexpr uint8 TickStateDue = 1;
	constexpr uint8 TickStateDone = 2;
} // namespace

// FTickPrerequisite

FTickPrerequisite::FTickPrerequisite(UObject* TargetObject, FTickFunction& TargetTickFunction)
	: PrerequisiteObject(TargetObject)
	, PrerequisiteTickFunction(&TargetTickFunction)
{
}

FTickFunction* FTickPrerequisite::Get() const
{
	return PrerequisiteObject.IsValid(true) ? PrerequisiteTickFunction : nullptr;
}

// FTickFunction

FTickFunction::FTickFunction()
	: bCanEverTick(false)
	, bStartWithTickEnabled(true)
	, bTickEnabled(false)
	, bHasTickOrder(false)
	, bInList(false)
	, bEnableRequested(false)
{
}

FTickFunction::~FTickFunction()
{
	UnRegisterTickFunction();
}

void FTickFunction::RegisterTickFunction(ULevel* Level)
{
	if (IsTickFunctionRegistered() || !bCanEverTick || Level == nullptr || Level->OwningWorld == nullptr)
	{
		return;
	}
	FTickTaskManager& Manager = Level->OwningWorld->GetTickTaskManager();
	// A tick function enabled or disabled before it registered keeps that; otherwise it starts as it says (UE).
	if (!bEnableRequested)
	{
		bTickEnabled = bStartWithTickEnabled;
	}
	Manager.AddTickFunction(*this);
}

void FTickFunction::UnRegisterTickFunction()
{
	if (TickTaskManager != nullptr)
	{
		TickTaskManager->RemoveTickFunction(*this);
	}
}

void FTickFunction::SetTickFunctionEnable(bool bInEnabled)
{
	bEnableRequested = true;
	if (bTickEnabled == bInEnabled)
	{
		return;
	}
	bTickEnabled = bInEnabled;
	if (TickTaskManager != nullptr)
	{
		TickTaskManager->OnTickFunctionEnableChanged(*this);
	}
}

bool FTickFunction::IsTickFunctionEnabled() const
{
	// Before it registers, what it will start with.
	if (!IsTickFunctionRegistered() && !bEnableRequested)
	{
		return bCanEverTick && bStartWithTickEnabled;
	}
	return bTickEnabled;
}

void FTickFunction::UpdateTickIntervalAndCoolDown(float NewTickInterval)
{
	const float Waited = TickInterval - TickCooldown;
	TickInterval = FMath::Max(0.0f, NewTickInterval);
	TickCooldown = TickInterval - Waited;
}

void FTickFunction::AddPrerequisite(UObject* TargetObject, FTickFunction& TargetTickFunction)
{
	if (TargetObject == nullptr || &TargetTickFunction == this)
	{
		return;
	}
	Prerequisites.AddUnique(FTickPrerequisite(TargetObject, TargetTickFunction));
}

void FTickFunction::RemovePrerequisite(UObject* TargetObject, FTickFunction& TargetTickFunction)
{
	Prerequisites.Remove(FTickPrerequisite(TargetObject, TargetTickFunction));
}

uint64 FTickFunction::MakeTickOrder()
{
	return TickTaskManager != nullptr ? SerialTickOrderBase | TickTaskManager->NextSerialTickOrder()
									  : SerialTickOrderBase;
}

void FTickFunction::GetEffectivePrerequisites(
	TArray<const FTickPrerequisite*, TInlineAllocator<8>>& OutPrerequisites) const
{
	for (const FTickPrerequisite& Prerequisite : Prerequisites)
	{
		OutPrerequisites.Add(&Prerequisite);
	}
}

// FActorTickFunction

void FActorTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	if (Target != nullptr && !Target->IsPendingKillPending())
	{
		Target->TickActor(DeltaTime, TickType, *this);
	}
}

FString FActorTickFunction::DiagnosticMessage()
{
	return Target != nullptr ? Target->GetFullName() + TEXT("[TickActor]") : FString(TEXT("[TickActor]"));
}

uint64 FActorTickFunction::MakeTickOrder()
{
	// The actor's place in its level, after its components.
	return Target != nullptr && Target->GetLevelOrder() != 0 ? (Target->GetLevelOrder() << 16) | ActorTickSubOrder
															 : FTickFunction::MakeTickOrder();
}

// FActorComponentTickFunction

void FActorComponentTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	(void)TickType;
	if (Target != nullptr && Target->IsRegistered() && !Target->IsPendingKill())
	{
		Target->TickComponent(DeltaTime);
	}
}

FString FActorComponentTickFunction::DiagnosticMessage()
{
	return Target != nullptr ? Target->GetFullName() + TEXT("[TickComponent]") : FString(TEXT("[TickComponent]"));
}

uint64 FActorComponentTickFunction::MakeTickOrder()
{
	// The owner's place, then the component's first registration among its owner's (before the owner's own tick).
	AActor* Owner = Target != nullptr ? Target->GetOwner() : nullptr;
	if (Owner == nullptr || Owner->GetLevelOrder() == 0)
	{
		return FTickFunction::MakeTickOrder();
	}
	return (Owner->GetLevelOrder() << 16) |
		FMath::Min<uint64>(Owner->NextComponentTickSubOrder(), ActorTickSubOrder - 1);
}

void FActorComponentTickFunction::GetEffectivePrerequisites(
	TArray<const FTickPrerequisite*, TInlineAllocator<8>>& OutPrerequisites) const
{
	FTickFunction::GetEffectivePrerequisites(OutPrerequisites);
	// A component waits for what its actor waits for (a pawn's for its controller), in the same group.
	const AActor* Owner = Target != nullptr ? Target->GetOwner() : nullptr;
	if (Owner != nullptr && Owner->PrimaryActorTick.TickGroup == TickGroup)
	{
		for (const FTickPrerequisite& Prerequisite : Owner->PrimaryActorTick.GetPrerequisites())
		{
			OutPrerequisites.Add(&Prerequisite);
		}
	}
}

// FTickTaskManager

FTickTaskManager::~FTickTaskManager()
{
	UnregisterAll();
}

void FTickTaskManager::UnregisterAll()
{
	// Copies: RemoveTickFunction edits the lists.
	const TArray<FTickFunction*> Registered = AllTickFunctions;
	for (FTickFunction* TickFunction : Registered)
	{
		RemoveTickFunction(*TickFunction);
	}
}

void FTickTaskManager::StartFrame()
{
	++FrameCounter;
}

int32 FTickTaskManager::GetNumEnabledTickFunctions(ETickingGroup Group) const
{
	int32 Count = 0;
	for (const FTickFunction* TickFunction : Enabled[Group])
	{
		Count += TickFunction != nullptr ? 1 : 0;
	}
	return Count;
}

void FTickTaskManager::AddTickFunction(FTickFunction& TickFunction)
{
	check(TickFunction.TickTaskManager == nullptr);
	TickFunction.TickTaskManager = this;
	AllTickFunctions.Add(&TickFunction);
	if (!TickFunction.bHasTickOrder)
	{
		TickFunction.TickOrder = TickFunction.MakeTickOrder();
		TickFunction.bHasTickOrder = true;
	}
	// An interval tick first comes once the interval has gone by.
	TickFunction.TickCooldown = TickFunction.TickInterval;
	TickFunction.TimeSinceLastTick = 0.0f;
	TickFunction.TickState = TickStateNotDue;
	TickFunction.StateFrame = 0;
	if (TickFunction.bTickEnabled)
	{
		InsertInList(TickFunction);
	}
}

void FTickTaskManager::RemoveTickFunction(FTickFunction& TickFunction)
{
	if (TickFunction.TickTaskManager != this)
	{
		return;
	}
	if (TickFunction.bInList)
	{
		RemoveFromList(TickFunction);
	}
	AllTickFunctions.RemoveSingleSwap(&TickFunction, false);
	TickFunction.TickTaskManager = nullptr;
}

void FTickTaskManager::OnTickFunctionEnableChanged(FTickFunction& TickFunction)
{
	if (TickFunction.bTickEnabled && !TickFunction.bInList)
	{
		// A new start: the interval counts from now.
		TickFunction.TickCooldown = TickFunction.TickInterval;
		TickFunction.TimeSinceLastTick = 0.0f;
		InsertInList(TickFunction);
	}
	else if (!TickFunction.bTickEnabled && TickFunction.bInList)
	{
		RemoveFromList(TickFunction);
	}
}

void FTickTaskManager::InsertInList(FTickFunction& TickFunction)
{
	const int32 Group = FMath::Clamp<int32>(TickFunction.TickGroup, 0, TG_MAX - 1);
	TArray<FTickFunction*>& List = Enabled[Group];
	// From the end: a new actor has the highest order, so it usually goes last.
	int32 InsertIndex = List.Num();
	for (int32 Index = List.Num() - 1; Index >= 0; --Index)
	{
		if (List[Index] == nullptr)
		{
			continue;
		}
		if (List[Index]->TickOrder < TickFunction.TickOrder)
		{
			break;
		}
		InsertIndex = Index;
	}
	List.Insert(&TickFunction, InsertIndex);
	TickFunction.bInList = true;
	TickFunction.ListGroup = static_cast<uint8>(Group);
	// Behind the running tick function: it ticks from the next step.
	if (Group == RunningGroup && InsertIndex <= RunningIndex)
	{
		++RunningIndex;
	}
}

void FTickTaskManager::RemoveFromList(FTickFunction& TickFunction)
{
	TArray<FTickFunction*>& List = Enabled[TickFunction.ListGroup];
	const int32 Index = List.Find(&TickFunction);
	TickFunction.bInList = false;
	if (Index == INDEX_NONE)
	{
		return;
	}
	if (TickFunction.ListGroup == RunningGroup)
	{
		// The running group walks the list by index: the slot empties and the list compacts when the group ends.
		List[Index] = nullptr;
		bHasNullSlots = true;
	}
	else
	{
		List.RemoveAt(Index);
	}
}

bool FTickTaskManager::IsDueThisFrame(FTickFunction& TickFunction, float DeltaSeconds)
{
	if (TickFunction.StateFrame == FrameCounter)
	{
		return TickFunction.TickState != TickStateNotDue;
	}
	TickFunction.StateFrame = FrameCounter;
	TickFunction.TimeSinceLastTick += DeltaSeconds;
	if (TickFunction.TickInterval > 0.0f)
	{
		TickFunction.TickCooldown -= DeltaSeconds;
		if (TickFunction.TickCooldown > TickCooldownTolerance)
		{
			TickFunction.TickState = TickStateNotDue;
			return false;
		}
		// The remainder carries over (the rate holds on average); a long stall does not make it tick twice.
		TickFunction.TickCooldown = FMath::Max(0.0f, TickFunction.TickCooldown + TickFunction.TickInterval);
	}
	TickFunction.TickState = TickStateDue;
	return true;
}

FTickFunction* FTickTaskManager::FindPendingPrerequisite(FTickFunction& TickFunction, float DeltaSeconds)
{
	TArray<const FTickPrerequisite*, TInlineAllocator<8>> Prerequisites;
	TickFunction.GetEffectivePrerequisites(Prerequisites);
	for (const FTickPrerequisite* Prerequisite : Prerequisites)
	{
		FTickFunction* Other = Prerequisite->Get();
		// Only an enabled tick function of this group that is due and has not run yet makes it wait.
		if (Other == nullptr || Other == &TickFunction || Other->TickTaskManager != this || !Other->bInList ||
			Other->ListGroup != TickFunction.ListGroup)
		{
			continue;
		}
		if (IsDueThisFrame(*Other, DeltaSeconds) && Other->TickState == TickStateDue)
		{
			return Other;
		}
	}
	return nullptr;
}

void FTickTaskManager::Execute(FTickFunction& TickFunction, float DeltaSeconds, ELevelTick TickType)
{
	const float TickDelta = TickFunction.TickInterval > 0.0f ? TickFunction.TimeSinceLastTick : DeltaSeconds;
	TickFunction.TickState = TickStateDone;
	TickFunction.TimeSinceLastTick = 0.0f;
	TickFunction.ExecuteTick(TickDelta, TickType);
}

bool FTickTaskManager::CanRunWaiting(const FTickFunction& TickFunction) const
{
	return TickFunction.TickTaskManager == this && TickFunction.bInList && TickFunction.StateFrame == FrameCounter &&
		TickFunction.TickState == TickStateDue;
}

void FTickTaskManager::ReleaseWaiting(FTickFunction& Finished, float DeltaSeconds, ELevelTick TickType)
{
	// In waiting order; a released tick function releases what waited for it before the next one of Finished's.
	for (;;)
	{
		const int32 Found =
			Waiting.IndexOfByPredicate([&Finished](const FWaiting& Entry) { return Entry.Blocker == &Finished; });
		if (Found == INDEX_NONE)
		{
			return;
		}
		FTickFunction* TickFunction = Waiting[Found].TickFunction;
		if (!CanRunWaiting(*TickFunction))
		{
			Waiting.RemoveAt(Found);
			continue;
		}
		if (FTickFunction* Blocker = FindPendingPrerequisite(*TickFunction, DeltaSeconds))
		{
			Waiting[Found].Blocker = Blocker;
			continue;
		}
		Waiting.RemoveAt(Found);
		Execute(*TickFunction, DeltaSeconds, TickType);
		ReleaseWaiting(*TickFunction, DeltaSeconds, TickType);
	}
}

void FTickTaskManager::RunTickGroup(ETickingGroup Group, float DeltaSeconds, ELevelTick TickType)
{
	check(RunningGroup < 0);
	RunningGroup = Group;
	Waiting.Reset();
	TArray<FTickFunction*>& List = Enabled[Group];
	for (RunningIndex = 0; RunningIndex < List.Num(); ++RunningIndex)
	{
		FTickFunction* TickFunction = List[RunningIndex];
		if (TickFunction == nullptr || !IsDueThisFrame(*TickFunction, DeltaSeconds) ||
			TickFunction->TickState != TickStateDue)
		{
			continue;
		}
		if (FTickFunction* Blocker = FindPendingPrerequisite(*TickFunction, DeltaSeconds))
		{
			Waiting.Add(FWaiting{TickFunction, Blocker});
			continue;
		}
		Execute(*TickFunction, DeltaSeconds, TickType);
		ReleaseWaiting(*TickFunction, DeltaSeconds, TickType);
	}
	// What still waits lost its prerequisite during the group (it was disabled or went away): it runs now.
	while (Waiting.Num() > 0)
	{
		FTickFunction* TickFunction = Waiting[0].TickFunction;
		Waiting.RemoveAt(0);
		if (CanRunWaiting(*TickFunction))
		{
			Execute(*TickFunction, DeltaSeconds, TickType);
			ReleaseWaiting(*TickFunction, DeltaSeconds, TickType);
		}
	}
	RunningGroup = -1;
	RunningIndex = -1;
	if (bHasNullSlots)
	{
		bHasNullSlots = false;
		List.RemoveAll([](const FTickFunction* TickFunction) { return TickFunction == nullptr; });
	}
}
