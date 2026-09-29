#pragma once

// Garbage collection: statistics, the collection interval setting and its timer (UE: UObject/GarbageCollection.h).
// CollectGarbage, FReferenceCollector and GARBAGE_COLLECTION_KEEPFLAGS are declared in UObject/UObjectGlobals.h, as
// in UE; FGCObject in UObject/GCObject.h.
//
// The collector is UE4's mark-and-sweep, stop the world, over the reflected properties (plan decision D11):
//  - Roots: objects in the root set (AddToRoot), native objects (classes, functions, structs, enums), class default
//    objects and everything inside them, the compiled-in /Script packages, objects with the KeepFlags of the call, and
//    whatever FGCObject holders report.
//  - Mark: from the roots through each object's outer and class, the strong references of its class
//    (UClass::ReferenceTokenStream: UObject*, TSubclassOf, and arrays / sets / maps / structs holding them) and the
//    class's AddReferencedObjects. Weak and soft references do not keep objects alive. A strong reference to a
//    pending-kill object (MarkPendingKill) is cleared, so the object is collected even while referenced (UE 4.27).
//  - Sweep: every object left unmarked gets BeginDestroy (it leaves the name hash), then FinishDestroy once
//    IsReadyForFinishDestroy, then its destructor; its GUObjectArray slot is freed, so weak pointers go stale.
//
// It only runs when called: never from inside a constructor or while a raw, unreported UObject* is live on the stack
// of code that relies on it. UPROPERTY members and FGCObject / TStrongObjectPtr are the ways to keep objects.
//
// Incremental collection (Leon, ps2-shipping N18; UE 5's incremental reachability needs TObjectPtr barriers, which
// Leon's raw UPROPERTY pointers cannot give): StartIncrementalGarbageCollection takes the roots, then each
// IncrementalCollectGarbageStep visits a budget of queued objects (a count, not a time, so a run collects at the same
// step on every machine), while the game runs in between. The marks are the collection's own: no object is flagged
// unreachable before the end, so weak pointers, iterators and finds see every object meanwhile. Each visit records the
// reference slots it read (their values, and a copy of each container's header and of an object array's elements).
// When the queue is empty the collection ends at once: the objects made since and the roots are kept, the FGCObjects
// report again, every visited object whose records changed is visited again (a reference stored into it after its visit
// is found) and what the classes' AddReferencedObjects report is reported again; then the white objects are swept and
// purged. Nothing is freed while a collection is under way, so the records' memory stays valid. A reference to an
// object that went pending kill after its referrer was visited is cleared by the next collection (the object lives
// until then). A full CollectGarbage replaces a collection under way.

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogGarbage, Log, All);

/** Figures of the last CollectGarbage (Leon: for tests and the platform budgets). */
struct FGarbageCollectionStats
{
	/** Objects in GUObjectArray when the collection started. */
	int32 NumObjectsBefore = 0;
	/** Objects found unreachable (and destroyed, once purged). */
	int32 NumObjectsCollected = 0;
	/** Objects in GUObjectArray after the purge. */
	int32 NumObjectsAfter = 0;
	/** Strong references to pending-kill objects that were cleared. */
	int32 NumReferencesCleared = 0;
	/** Seconds spent marking (roots and reachability). */
	double MarkSeconds = 0.0;
	/** Seconds spent destroying and freeing (BeginDestroy through the destructors). */
	double PurgeSeconds = 0.0;
	/** An incremental collection: its slices, its end's seconds and the objects its end visited again. */
	bool bIncremental = false;
	int32 NumSlices = 0;
	double FinishSeconds = 0.0;
	int32 NumObjectsRevisited = 0;
	/** GMalloc bytes in use before and after the collection. */
	SIZE_T HeapBytesBefore = 0;
	SIZE_T HeapBytesAfter = 0;
};

/** The statistics of the last collection (Leon). */
COREUOBJECT_API const FGarbageCollectionStats& GetLastGarbageCollectionStats();

/** Number of objects in GUObjectArray, pending-purge ones included (UE: GUObjectArray.GetObjectArrayNumMinusAvailable).
 */
COREUOBJECT_API int32 GetNumGCObjects();

/** Whether an incremental collection is under way (UE 5: IsIncrementalReachabilityAnalysisPending). */
COREUOBJECT_API bool IsIncrementalReachabilityAnalysisPending();

/** Starts an incremental collection with KeepFlags: the roots are taken now (see the header comment). */
COREUOBJECT_API void StartIncrementalGarbageCollection(EObjectFlags KeepFlags);

/**
 * Visits at most ObjectBudget objects of the incremental collection under way; when none is left, ends it (marks what
 * changed, sweeps and purges) and returns true. Call it at a safe point only, as CollectGarbage.
 */
COREUOBJECT_API bool IncrementalCollectGarbageStep(int32 ObjectBudget);

/**
 * When the engine collects: [/Script/Engine.GarbageCollectionSettings] of the engine config (UE: the gc.* console
 * variables that section sets).
 */
struct COREUOBJECT_API FGarbageCollectionSettings
{
	/**
	 * Seconds of game between the starts of two incremental collections (UE: gc.TimeBetweenPurgingPendingKillObjects,
	 * 61.1 for its full ones; Leon's incremental ones cost a slice a step, so they come more often).
	 */
	float TimeBetweenPurgingPendingKillObjects = 10.0f;

	/**
	 * Objects an incremental collection visits in a step (gc.IncrementalObjectsPerStep): about 0.5 ms of the EE at
	 * Budgets.md's 5 us an object. A count, so the collection advances the same on every machine (D4).
	 */
	int32 IncrementalObjectsPerStep = 100;

	/**
	 * The settings from the section of IniFilename (a GConfig key; GEngineIni when empty), keeping the defaults for
	 * missing keys or when there is no config.
	 */
	static FGarbageCollectionSettings LoadFromConfig(const FString& IniFilename = FString());
};

/**
 * Collects incrementally (UE: UEngine::ConditionalCollectGarbage and its TimeSinceLastPendingKillPurge): every
 * TimeBetweenPurgingPendingKillObjects seconds an incremental collection starts, and each Tick while it is under way
 * visits IncrementalObjectsPerStep objects, until it ends (sweeps and purges). ForceCollectOnNextTick makes the next
 * Tick a full collection. Whoever owns the frame ticks it at a safe point: UEngine does, after each world step; LoadMap
 * collects at once (plan decision D11), a game asks for a full collection with UEngine::ForceGarbageCollection.
 */
class COREUOBJECT_API FGarbageCollectionTimer
{
public:
	explicit FGarbageCollectionTimer(const FGarbageCollectionSettings& InSettings = FGarbageCollectionSettings());

	/**
	 * A step: a forced full collection, else a slice of the collection under way, else DeltaSeconds more and a new
	 * incremental collection once the interval has passed. True when a collection ended in this Tick.
	 */
	bool Tick(float DeltaSeconds, EObjectFlags KeepFlags = GARBAGE_COLLECTION_KEEPFLAGS);

	/** Makes the next Tick a full collection (UE: ForceGarbageCollection(true)). */
	void ForceCollectOnNextTick();

	FORCEINLINE float GetTimeSinceLastCollection() const
	{
		return TimeSinceLastCollection;
	}

	FORCEINLINE const FGarbageCollectionSettings& GetSettings() const
	{
		return Settings;
	}

private:
	FGarbageCollectionSettings Settings;
	float TimeSinceLastCollection = 0.0f;
	bool bForceCollection = false;
};
