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
	/** GMalloc bytes in use before and after the collection. */
	SIZE_T HeapBytesBefore = 0;
	SIZE_T HeapBytesAfter = 0;
};

/** The statistics of the last collection (Leon). */
COREUOBJECT_API const FGarbageCollectionStats& GetLastGarbageCollectionStats();

/** Number of objects in GUObjectArray, pending-purge ones included (UE: GUObjectArray.GetObjectArrayNumMinusAvailable).
 */
COREUOBJECT_API int32 GetNumGCObjects();

/**
 * When the engine collects: [/Script/Engine.GarbageCollectionSettings] of the engine config (UE: the gc.* console
 * variables that section sets). Only the interval is read for now.
 */
struct COREUOBJECT_API FGarbageCollectionSettings
{
	/** Seconds between two collections (UE: gc.TimeBetweenPurgingPendingKillObjects, default 61.1). */
	float TimeBetweenPurgingPendingKillObjects = 61.1f;

	/**
	 * The settings from the section of IniFilename (a GConfig key; GEngineIni when empty), keeping the defaults for
	 * missing keys or when there is no config.
	 */
	static FGarbageCollectionSettings LoadFromConfig(const FString& IniFilename = FString());
};

/**
 * Collects at the configured interval (UE: UEngine::ConditionalCollectGarbage and its TimeSinceLastPendingKillPurge).
 * Whoever owns the frame ticks it at a safe point: P13's UEngine does, after the world tick; LoadMap and the round
 * restart call CollectGarbage directly (plan decision D11).
 */
class COREUOBJECT_API FGarbageCollectionTimer
{
public:
	explicit FGarbageCollectionTimer(const FGarbageCollectionSettings& InSettings = FGarbageCollectionSettings());

	/** Adds DeltaSeconds; when the interval has passed, collects with KeepFlags and returns true. */
	bool Tick(float DeltaSeconds, EObjectFlags KeepFlags = GARBAGE_COLLECTION_KEEPFLAGS);

	/** Makes the next Tick collect (UE: ForceGarbageCollection). */
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
