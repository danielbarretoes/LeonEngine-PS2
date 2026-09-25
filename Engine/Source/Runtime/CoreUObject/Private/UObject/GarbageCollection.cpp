// The garbage collector: mark from the roots, clear references to pending-kill objects, destroy the rest (UE:
// GarbageCollection.cpp, reduced to one thread and no clusters). See UObject/GarbageCollection.h.

#include "UObject/GarbageCollection.h"

#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/GCObject.h"
#include "UObject/Package.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY(LogGarbage);

namespace
{
	bool GIsGarbageCollecting = false;

	/** Unreachable objects waiting for FinishDestroy and destruction, in GUObjectArray order (UE: GUnreachableObjects).
	 */
	TArray<UObject*> GUnreachableObjects;

	/** How far IncrementalPurgeGarbage got: the objects before these indices are finished / destroyed. */
	int32 GFinishDestroyIndex = 0;
	int32 GDestroyIndex = 0;

	FGarbageCollectionStats GLastStats;

	/** The live FGCObjects (UE: UGCObjectReferencer::ReferencedObjects). */
	TArray<FGCObject*>& GetGCObjects()
	{
		static TArray<FGCObject*> GCObjects;
		return GCObjects;
	}

	/**
	 * True for an object that is kept whatever references it: the root set, native objects (classes, functions,
	 * structs, enums, which the registration made), class default objects and everything inside them, the compiled-in
	 * packages, and objects with one of KeepFlags. A pending-kill object is never kept by its flags (UE:
	 * MarkObjectsAsUnreachable).
	 */
	bool IsGarbageCollectionRoot(const FUObjectItem& Item, UObject* Object, EObjectFlags KeepFlags)
	{
		if (Item.HasAnyFlags(EInternalObjectFlags::RootSet | EInternalObjectFlags::GarbageCollectionKeepFlags))
		{
			return true;
		}
		if (Item.IsPendingKill())
		{
			return false;
		}
		if (KeepFlags != RF_NoFlags && Object->HasAnyFlags(KeepFlags))
		{
			return true;
		}
		// UE keeps these in the disregard-for-GC pool, the objects created before the engine finished starting.
		if (Object->HasAnyFlags(RF_ClassDefaultObject) ||
			(Object->HasAnyFlags(RF_ArchetypeObject) && Object->IsTemplate(RF_ClassDefaultObject)))
		{
			return true;
		}
		const UPackage* Package = ExactCast<UPackage>(Object);
		return Package && Package->HasAnyPackageFlags(PKG_CompiledIn);
	}

	/**
	 * Marks everything reachable from the roots (UE: FGCReferenceProcessor / TFastReferenceCollector). Every object
	 * starts unreachable; a reference to an unreachable object clears the flag and queues the object, whose references
	 * are then visited in turn.
	 */
	class FGarbageCollectionMarker final : public FReferenceCollector
	{
	public:
		FGarbageCollectionMarker()
		{
			ObjectsToSerialize.Reserve(1024);
		}

		/** A root: reachable whatever its flags say. */
		void AddRoot(UObject* Object)
		{
			ObjectsToSerialize.Add(Object);
		}

		/** Visits the queue until every reachable object has been processed. */
		void ProcessObjects()
		{
			for (int32 Index = 0; Index < ObjectsToSerialize.Num(); ++Index)
			{
				ProcessObject(ObjectsToSerialize[Index]);
			}
		}

		int32 GetNumReferencesCleared() const
		{
			return NumReferencesCleared;
		}

		virtual bool IsIgnoringArchetypeRef() const override
		{
			return false;
		}

		virtual bool IsIgnoringTransient() const override
		{
			return false;
		}

		/** One strong reference; cleared when it points at a pending-kill object and clearing is allowed (UE). */
		FORCEINLINE void MarkReference(UObject*& Object, bool bAllowElimination)
		{
			if (!Object)
			{
				return;
			}
			FUObjectItem* Item = GUObjectArray.ObjectToObjectItem(Object);
			checkf(Item && Item->Object == Object,
				"Garbage collection found a reference to an object that is not in GUObjectArray (%p): a UObject "
				"pointer outlived its object without a UPROPERTY or FGCObject",
				(void*)Object);
			if (Item->IsPendingKill() && bAllowElimination)
			{
				Object = nullptr;
				++NumReferencesCleared;
				return;
			}
			if (Item->HasAnyFlags(EInternalObjectFlags::Unreachable))
			{
				Item->ClearFlags(EInternalObjectFlags::Unreachable);
				ObjectsToSerialize.Add(Object);
			}
		}

		/** The strong references held by one value of Property at Value (a member, element, key or struct field). */
		void VisitValue(const FProperty* Property, void* Value)
		{
			if (CastField<FObjectProperty>(Property))
			{
				MarkReference(*(UObject**)Value, bAllowEliminatingReferences);
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				VisitStruct(StructProperty->Struct, Value);
			}
			else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, Value);
				for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
				{
					VisitValue(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index));
				}
			}
			else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
			{
				VisitSet(SetProperty, Value);
			}
			else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
			{
				VisitMap(MapProperty, Value);
			}
		}

	protected:
		virtual void HandleObjectReference(
			UObject*& InObject, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override
		{
			(void)InReferencingObject;
			(void)InReferencingProperty;
			MarkReference(InObject, bAllowEliminatingReferences);
		}

	private:
		/** The outer, the class, the class's strong reference properties and its AddReferencedObjects (UE). */
		void ProcessObject(UObject* Object)
		{
			// An object keeps its outer and its class alive; neither reference is ever cleared (UE: the persistent
			// tokens of UObject's token stream).
			UObject* Outer = Object->GetOuter();
			MarkReference(Outer, false);
			UClass* Class = Object->GetClass();
			UObject* ClassObject = Class;
			MarkReference(ClassObject, false);

			Class->AssembleReferenceTokenStream();
			for (FProperty* Property : Class->ReferenceTokenStream)
			{
				for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
				{
					VisitValue(Property, Property->ContainerPtrToValuePtr<void>(Object, Index));
				}
			}
			if (Class->ClassAddReferencedObjects != &UObject::AddReferencedObjects)
			{
				Class->CallAddReferencedObjects(Object, *this);
			}
		}

		void VisitStruct(const UScriptStruct* Struct, void* StructData)
		{
			for (FProperty* Property = Struct->RefLink; Property; Property = Property->NextRef)
			{
				if (!Property->ContainsObjectReference(EPropertyObjectReferenceType::Strong))
				{
					continue;
				}
				for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
				{
					VisitValue(Property, Property->ContainerPtrToValuePtr<void>(StructData, Index));
				}
			}
		}

		/** True for an element that is an object pointer to a pending-kill object (removed rather than cleared). */
		bool IsPendingKillObjectElement(const FProperty* ElementProp, const void* Element) const
		{
			if (!bAllowEliminatingReferences || !CastField<FObjectProperty>(ElementProp))
			{
				return false;
			}
			const UObject* Object = *(UObject* const*)Element;
			const FUObjectItem* Item = Object ? GUObjectArray.ObjectToObjectItem(Object) : nullptr;
			return Item && Item->IsPendingKill();
		}

		/**
		 * The elements of a set. An element that is a pointer to a pending-kill object is removed (clearing it would
		 * break the hash); a struct element whose references changed makes the set rehash.
		 */
		void VisitSet(const FSetProperty* SetProperty, void* Value)
		{
			FScriptSetHelper SetHelper(SetProperty, Value);
			const FProperty* ElementProp = SetProperty->ElementProp;
			const int32 ClearedBefore = NumReferencesCleared;
			for (int32 Index = 0; Index < SetHelper.GetMaxIndex(); ++Index)
			{
				if (!SetHelper.IsValidIndex(Index))
				{
					continue;
				}
				if (IsPendingKillObjectElement(ElementProp, SetHelper.GetElementPtr(Index)))
				{
					SetHelper.RemoveAt(Index);
					++NumReferencesCleared;
					continue;
				}
				VisitValue(ElementProp, SetHelper.GetElementPtr(Index));
			}
			if (NumReferencesCleared != ClearedBefore && !CastField<FObjectProperty>(ElementProp))
			{
				SetHelper.Rehash();
			}
		}

		/** The pairs of a map: like a set for the keys; a value is cleared like any reference. */
		void VisitMap(const FMapProperty* MapProperty, void* Value)
		{
			FScriptMapHelper MapHelper(MapProperty, Value);
			bool bKeysChanged = false;
			for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); ++Index)
			{
				if (!MapHelper.IsValidIndex(Index))
				{
					continue;
				}
				if (IsPendingKillObjectElement(MapProperty->KeyProp, MapHelper.GetKeyPtr(Index)))
				{
					MapHelper.RemoveAt(Index);
					++NumReferencesCleared;
					continue;
				}
				const int32 ClearedBefore = NumReferencesCleared;
				VisitValue(MapProperty->KeyProp, MapHelper.GetKeyPtr(Index));
				bKeysChanged |= NumReferencesCleared != ClearedBefore;
				VisitValue(MapProperty->ValueProp, MapHelper.GetValuePtr(Index));
			}
			if (bKeysChanged)
			{
				MapHelper.Rehash();
			}
		}

		TArray<UObject*> ObjectsToSerialize;
		int32 NumReferencesCleared = 0;
	};

	/** Heap in use, for the statistics. */
	SIZE_T GetHeapBytes()
	{
		return FMemory::GetUsage().CurrentBytes;
	}

	/** BeginDestroy on every unreachable object (UE: UnhashUnreachableObjects). */
	void BeginDestroyUnreachableObjects()
	{
		for (UObject* Object : GUnreachableObjects)
		{
			Object->ConditionalBeginDestroy();
		}
	}

	/** Seconds after which a full purge gives up waiting for IsReadyForFinishDestroy. */
	constexpr double MaxFinishDestroyWaitSeconds = 30.0;
} // namespace

// FReferenceCollector

FReferenceCollector::~FReferenceCollector()
{
}

void FReferenceCollector::AddReferencedObjects(const UScriptStruct* ScriptStruct, void* StructMemory,
	const UObject* ReferencingObject, const FProperty* ReferencingProperty)
{
	checkf(ScriptStruct && StructMemory, "AddReferencedObjects without a struct");
	for (FProperty* Property = ScriptStruct->RefLink; Property; Property = Property->NextRef)
	{
		if (!Property->ContainsObjectReference(EPropertyObjectReferenceType::Strong))
		{
			continue;
		}
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			void* Value = Property->ContainerPtrToValuePtr<void>(StructMemory, Index);
			if (CastField<FObjectProperty>(Property))
			{
				HandleObjectReference(*(UObject**)Value, ReferencingObject, ReferencingProperty);
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				AddReferencedObjects(StructProperty->Struct, Value, ReferencingObject, ReferencingProperty);
			}
			else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, Value);
				for (int32 Element = 0; Element < ArrayHelper.Num(); ++Element)
				{
					if (CastField<FObjectProperty>(ArrayProperty->Inner))
					{
						HandleObjectReference(
							*(UObject**)ArrayHelper.GetRawPtr(Element), ReferencingObject, ReferencingProperty);
					}
					else if (const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProperty->Inner))
					{
						AddReferencedObjects(InnerStruct->Struct, ArrayHelper.GetRawPtr(Element), ReferencingObject,
							ReferencingProperty);
					}
				}
			}
			// Sets and maps inside a struct are reported by the garbage collector's own walk only (their keys cannot
			// be cleared through a plain reference).
		}
	}
}

// FGCObject

FGCObject::FGCObject()
{
	RegisterGCObject();
}

FGCObject::FGCObject(const FGCObject& Other)
{
	(void)Other;
	RegisterGCObject();
}

FGCObject::FGCObject(FGCObject&& Other)
{
	(void)Other;
	RegisterGCObject();
}

FGCObject::~FGCObject()
{
	UnregisterGCObject();
}

const TArray<FGCObject*>& FGCObject::GetRegisteredGCObjects()
{
	return GetGCObjects();
}

void FGCObject::RegisterGCObject()
{
	checkf(!GIsGarbageCollecting, "An FGCObject cannot be created while garbage is being collected");
	GetGCObjects().Add(this);
}

void FGCObject::UnregisterGCObject()
{
	GetGCObjects().RemoveSingleSwap(this, false);
}

// Collection

bool IsGarbageCollecting()
{
	return GIsGarbageCollecting;
}

bool IsIncrementalPurgePending()
{
	return GUnreachableObjects.Num() > 0;
}

const FGarbageCollectionStats& GetLastGarbageCollectionStats()
{
	return GLastStats;
}

int32 GetNumGCObjects()
{
	return GUObjectArray.GetObjectArrayNumMinusAvailable();
}

void IncrementalPurgeGarbage(bool bUseTimeLimit, float TimeLimit)
{
	if (GUnreachableObjects.Num() == 0)
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();
	const auto IsTimeLimitExceeded = [bUseTimeLimit, TimeLimit, StartTime]()
	{ return bUseTimeLimit && FPlatformTime::Seconds() - StartTime > double(TimeLimit); };

	// FinishDestroy on all of them before any destructor runs: FinishDestroy may still use other unreachable objects
	// (UE). An object whose asynchronous cleanup is not done waits; a full purge waits for it.
	while (GFinishDestroyIndex < GUnreachableObjects.Num())
	{
		bool bAllReady = true;
		for (int32 Index = GFinishDestroyIndex; Index < GUnreachableObjects.Num(); ++Index)
		{
			UObject* Object = GUnreachableObjects[Index];
			if (Object->HasAnyFlags(RF_FinishDestroyed))
			{
				continue;
			}
			if (Object->IsReadyForFinishDestroy())
			{
				Object->ConditionalFinishDestroy();
			}
			else
			{
				bAllReady = false;
			}
		}
		if (bAllReady)
		{
			GFinishDestroyIndex = GUnreachableObjects.Num();
			break;
		}
		if (bUseTimeLimit)
		{
			// The next call polls again.
			return;
		}
		if (FPlatformTime::Seconds() - StartTime > MaxFinishDestroyWaitSeconds)
		{
			UE_LOG(LogGarbage, Fatal, TEXT("Objects have not been ready for FinishDestroy for %.0f seconds"),
				MaxFinishDestroyWaitSeconds);
		}
	}

	// Destructors and memory. The destructor frees the GUObjectArray slot, which makes weak pointers stale (UE).
	while (GDestroyIndex < GUnreachableObjects.Num())
	{
		UObject* Object = GUnreachableObjects[GDestroyIndex++];
		checkf(Object->HasAnyFlags(RF_FinishDestroyed), "Destroying an object that did not FinishDestroy");
		((UObjectBase*)Object)->~UObjectBase();
		FMemory::Free(Object);
		if (IsTimeLimitExceeded())
		{
			break;
		}
	}

	if (GDestroyIndex >= GUnreachableObjects.Num())
	{
		GUnreachableObjects.Reset();
		GFinishDestroyIndex = 0;
		GDestroyIndex = 0;
	}
}

void CollectGarbage(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
	checkf(!GIsGarbageCollecting, "CollectGarbage called while garbage is being collected");
	checkf(FUObjectThreadContext::Get().InitializerStack.Num() == 0 &&
			FUObjectThreadContext::Get().PendingConstructions.Num() == 0,
		"CollectGarbage called while an object is being constructed");
	if (!UObjectInitialized())
	{
		return;
	}
	// What the last collection left for later is destroyed first (UE).
	IncrementalPurgeGarbage(false);

	GIsGarbageCollecting = true;
	FGarbageCollectionStats Stats;
	Stats.HeapBytesBefore = GetHeapBytes();
	Stats.NumObjectsBefore = GUObjectArray.GetObjectArrayNumMinusAvailable();
	const double StartTime = FPlatformTime::Seconds();

	// Mark: every object unreachable except the roots, then everything the roots reach (UE: MarkObjectsAsUnreachable,
	// PerformReachabilityAnalysis). The marker and its queue are released before the purge, so the heap figures do not
	// count them.
	int32 NumReferencesCleared = 0;
	const int32 NumSlots = GUObjectArray.GetObjectArrayNum();
	{
		FGarbageCollectionMarker Marker;
		for (int32 Index = 0; Index < NumSlots; ++Index)
		{
			FUObjectItem* Item = GUObjectArray.IndexToObject(Index);
			if (!Item->Object)
			{
				continue;
			}
			UObject* Object = (UObject*)Item->Object;
			if (IsGarbageCollectionRoot(*Item, Object, KeepFlags))
			{
				Item->ClearFlags(EInternalObjectFlags::Unreachable);
				Marker.AddRoot(Object);
			}
			else
			{
				Item->SetFlags(EInternalObjectFlags::Unreachable);
			}
		}
		// The references non-UObjects hold (UE: UGCObjectReferencer::AddReferencedObjects). A copy:
		// AddReferencedObjects must not create or destroy FGCObjects, but a copy keeps the loop safe if it does.
		const TArray<FGCObject*> GCObjects = GetGCObjects();
		for (FGCObject* GCObject : GCObjects)
		{
			GCObject->AddReferencedObjects(Marker);
		}
		Marker.ProcessObjects();
		NumReferencesCleared = Marker.GetNumReferencesCleared();
	}
	const double MarkEndTime = FPlatformTime::Seconds();

	// Sweep: what is still unreachable leaves the name hash and begins its destruction (UE: GatherUnreachableObjects,
	// UnhashUnreachableObjects).
	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		FUObjectItem* Item = GUObjectArray.IndexToObject(Index);
		if (Item->Object && Item->HasAnyFlags(EInternalObjectFlags::Unreachable))
		{
			GUnreachableObjects.Add((UObject*)Item->Object);
		}
	}
	Stats.NumObjectsCollected = GUnreachableObjects.Num();
	Stats.NumReferencesCleared = NumReferencesCleared;
	BeginDestroyUnreachableObjects();
	GIsGarbageCollecting = false;

	if (bPerformFullPurge)
	{
		IncrementalPurgeGarbage(false);
	}
	const double EndTime = FPlatformTime::Seconds();

	Stats.MarkSeconds = MarkEndTime - StartTime;
	Stats.PurgeSeconds = EndTime - MarkEndTime;
	Stats.NumObjectsAfter = GUObjectArray.GetObjectArrayNumMinusAvailable();
	Stats.HeapBytesAfter = GetHeapBytes();
	GLastStats = Stats;
	UE_LOG(LogGarbage, Log,
		TEXT("Collected %d of %d objects in %.3f ms (mark %.3f ms, purge %.3f ms%s), %d references cleared, %d objects "
			 "left"),
		Stats.NumObjectsCollected, Stats.NumObjectsBefore, (EndTime - StartTime) * 1000.0, Stats.MarkSeconds * 1000.0,
		Stats.PurgeSeconds * 1000.0, bPerformFullPurge ? TEXT("") : TEXT(", incremental"), Stats.NumReferencesCleared,
		Stats.NumObjectsAfter);
}

bool TryCollectGarbage(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
	if (GIsGarbageCollecting)
	{
		return false;
	}
	CollectGarbage(KeepFlags, bPerformFullPurge);
	return true;
}

// Settings and timer

FGarbageCollectionSettings FGarbageCollectionSettings::LoadFromConfig(const FString& IniFilename)
{
	FGarbageCollectionSettings Settings;
	const FString& Filename = IniFilename.IsEmpty() ? GEngineIni : IniFilename;
	if (GConfig && !Filename.IsEmpty())
	{
		GConfig->GetFloat(TEXT("/Script/Engine.GarbageCollectionSettings"),
			TEXT("gc.TimeBetweenPurgingPendingKillObjects"), Settings.TimeBetweenPurgingPendingKillObjects, Filename);
	}
	return Settings;
}

FGarbageCollectionTimer::FGarbageCollectionTimer(const FGarbageCollectionSettings& InSettings)
	: Settings(InSettings)
{
}

bool FGarbageCollectionTimer::Tick(float DeltaSeconds, EObjectFlags KeepFlags)
{
	TimeSinceLastCollection += DeltaSeconds;
	if (!bForceCollection && TimeSinceLastCollection < Settings.TimeBetweenPurgingPendingKillObjects)
	{
		return false;
	}
	CollectGarbage(KeepFlags);
	TimeSinceLastCollection = 0.0f;
	bForceCollection = false;
	return true;
}

void FGarbageCollectionTimer::ForceCollectOnNextTick()
{
	bForceCollection = true;
}
