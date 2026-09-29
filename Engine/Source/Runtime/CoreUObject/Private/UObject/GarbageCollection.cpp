// The garbage collector: mark from the roots, clear references to pending-kill objects, destroy the rest (UE:
// GarbageCollection.cpp, reduced to one thread and no clusters), at once or incrementally over the engine's steps.
// See UObject/GarbageCollection.h.

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

	/** An incremental collection's mark of an object's slot (GIncremental.MarkStates). */
	enum class EMarkState : uint8
	{
		/** Not reached yet: collected unless something reaches it by the end. */
		White,
		/** Reached, its references not visited yet (in the queue). */
		Gray,
		/** Reached and visited. */
		Black,
		/** The slot was empty when the collection started: an object in it now is new, and is kept. */
		Empty,
	};

	/**
	 * What a slice recorded of a visited object: a reference slot and its value, or a container's header (an array of
	 * object pointers: its elements too) and a copy of its bytes. The end of the collection compares them with the
	 * object's memory, exactly: an object whose references changed since it was visited is visited again.
	 */
	struct FGCSlotRecord
	{
		const void* Address;
		/** The bytes themselves up to 8, else where their copy starts in the snapshot. */
		uint64 Value;
		uint32 Size;
	};

	/** A visited object and its records (GIncremental.Records). */
	struct FGCVisitedObject
	{
		int32 ObjectIndex;
		int32 FirstRecord;
		int32 NumRecords;
		/** Its outer when visited (the end marks a new one; its class is a root and never changes). */
		UObject* Outer;
		/**
		 * Its class reports references itself (AddReferencedObjects), so the end asks it again; not for a native object
		 * (a class, a function, a struct, an enum), whose reports are native objects and class defaults: roots.
		 */
		bool bReportsReferences;
	};

	/** Up to 8 bytes at Address as a value. */
	FORCEINLINE uint64 ReadSmallSlot(const void* Address, uint32 Size)
	{
		uint64 Value = 0;
		FMemory::Memcpy(&Value, Address, Size);
		return Value;
	}

	/** What Address holds now: a record of its value, or of its bytes copied to the end of Snapshot. */
	FORCEINLINE FGCSlotRecord CaptureSlot(const void* Address, uint32 Size, TArray<uint8>& Snapshot)
	{
		if (Size <= sizeof(uint64))
		{
			return FGCSlotRecord{Address, ReadSmallSlot(Address, Size), Size};
		}
		const int32 Offset = Snapshot.Num();
		Snapshot.Append(static_cast<const uint8*>(Address), static_cast<int32>(Size));
		return FGCSlotRecord{Address, static_cast<uint64>(Offset), Size};
	}

	/** Whether the memory of a record changed since it was taken. */
	FORCEINLINE bool HasSlotChanged(const FGCSlotRecord& Record, const TArray<uint8>& Snapshot)
	{
		// Most records are object pointers: one load.
		if (Record.Size == sizeof(UPTRINT))
		{
			return static_cast<uint64>(*static_cast<const UPTRINT*>(Record.Address)) != Record.Value;
		}
		if (Record.Size <= sizeof(uint64))
		{
			return ReadSmallSlot(Record.Address, Record.Size) != Record.Value;
		}
		return FMemory::Memcmp(Record.Address, &Snapshot[static_cast<int32>(Record.Value)], Record.Size) != 0;
	}

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
			for (; ProcessedIndex < ObjectsToSerialize.Num(); ++ProcessedIndex)
			{
				ProcessObject(ObjectsToSerialize[ProcessedIndex]);
			}
		}

		/**
		 * Incremental: the marks live in MarkStates (the objects' flags are left alone, so weak pointers, iterators and
		 * finds see every object between the slices), and with InRecords each visit records what it read (the end of
		 * the collection checks it).
		 */
		void SetIncremental(TArray<uint8>* InMarkStates, TArray<FGCSlotRecord>* InRecords, TArray<uint8>* InSnapshot,
			TArray<FGCVisitedObject>* InVisited)
		{
			MarkStates = InMarkStates;
			Records = InRecords;
			Snapshot = InSnapshot;
			Visited = InVisited;
		}

		/** Visits at most Budget queued objects; true when the queue is empty. */
		bool ProcessObjects(int32 Budget)
		{
			for (int32 Count = 0; ProcessedIndex < ObjectsToSerialize.Num() && Count < Budget; ++Count)
			{
				ProcessObject(ObjectsToSerialize[ProcessedIndex++]);
			}
			return ProcessedIndex >= ObjectsToSerialize.Num();
		}

		/** Queues an object reached another way (a new object, a root found at the end), marked. */
		void Enqueue(UObject* Object)
		{
			ObjectsToSerialize.Add(Object);
		}

		/** An object visited in a slice: its outer and class again, and its references if they changed. */
		void Revisit(UObject* Object, const FGCVisitedObject& Entry, const TArray<FGCSlotRecord>& Log,
			const TArray<uint8>& InSnapshot)
		{
			UObject* Outer = Object->GetOuter();
			if (Outer != Entry.Outer)
			{
				MarkReference(Outer, false);
			}
			for (int32 Index = Entry.FirstRecord; Index < Entry.FirstRecord + Entry.NumRecords; ++Index)
			{
				// In order: a container's header comes before its elements, so a changed header stops the reads before
				// they reach memory the container may have let go.
				const FGCSlotRecord& Record = Log[Index];
				if (HasSlotChanged(Record, InSnapshot))
				{
					++NumRevisited;
					ProcessObject(Object);
					return;
				}
			}
			// What a class reports itself is not recorded: it reports again (its current references).
			if (Entry.bReportsReferences)
			{
				Object->GetClass()->CallAddReferencedObjects(Object, *this);
			}
		}

		/** Stops recording (the end of an incremental collection visits what is left at once). */
		void StopRecording()
		{
			Records = nullptr;
			Snapshot = nullptr;
			Visited = nullptr;
		}

		int32 GetNumReferencesCleared() const
		{
			return NumReferencesCleared;
		}
		int32 GetNumRevisited() const
		{
			return NumRevisited;
		}
		int32 GetNumProcessed() const
		{
			return ProcessedIndex;
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
			if (MarkStates != nullptr)
			{
				// A slot empty at the start (a new object) is kept and visited at the end.
				const int32 Index = GUObjectArray.ObjectToIndex(Object);
				if (Index < MarkStates->Num() && (*MarkStates)[Index] == static_cast<uint8>(EMarkState::White))
				{
					(*MarkStates)[Index] = static_cast<uint8>(EMarkState::Gray);
					ObjectsToSerialize.Add(Object);
				}
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
				Record(Value, sizeof(UObject*));
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				VisitStruct(StructProperty->Struct, Value);
			}
			else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				// The header first (see Revisit), filled once the elements are visited.
				const int32 Header = ReserveRecord();
				FScriptArrayHelper ArrayHelper(ArrayProperty, Value);
				const bool bObjects = CastField<FObjectProperty>(ArrayProperty->Inner) != nullptr;
				for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
				{
					if (bObjects)
					{
						MarkReference(*(UObject**)ArrayHelper.GetRawPtr(Index), bAllowEliminatingReferences);
					}
					else
					{
						VisitValue(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index));
					}
				}
				FillRecord(Header, Value, static_cast<uint32>(ArrayProperty->ElementSize));
				if (bObjects && ArrayHelper.Num() > 0)
				{
					// An array of object pointers: its elements as one hash.
					Record(ArrayHelper.GetRawPtr(0),
						static_cast<uint32>(ArrayHelper.Num()) *
							static_cast<uint32>(ArrayProperty->Inner->ElementSize));
				}
			}
			else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
			{
				const int32 Header = ReserveRecord();
				VisitSet(SetProperty, Value);
				FillRecord(Header, Value, static_cast<uint32>(SetProperty->ElementSize));
			}
			else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
			{
				const int32 Header = ReserveRecord();
				VisitMap(MapProperty, Value);
				FillRecord(Header, Value, static_cast<uint32>(MapProperty->ElementSize));
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
		/** Records what Address holds (a slice's visit). */
		FORCEINLINE void Record(const void* Address, uint32 Size)
		{
			if (Records != nullptr)
			{
				Records->Add(CaptureSlot(Address, Size, *Snapshot));
			}
		}
		/** A record whose value is known once its container is visited (FillRecord); INDEX_NONE when not recording. */
		FORCEINLINE int32 ReserveRecord()
		{
			return Records != nullptr ? Records->Add(FGCSlotRecord{nullptr, 0, 0}) : INDEX_NONE;
		}
		FORCEINLINE void FillRecord(int32 Index, const void* Address, uint32 Size)
		{
			if (Index != INDEX_NONE)
			{
				(*Records)[Index] = CaptureSlot(Address, Size, *Snapshot);
			}
		}

		/** The outer, the class, the class's strong reference properties and its AddReferencedObjects (UE). */
		void ProcessObject(UObject* Object)
		{
			if (MarkStates != nullptr)
			{
				const int32 ObjectIndex = GUObjectArray.ObjectToIndex(Object);
				if (ObjectIndex < MarkStates->Num())
				{
					(*MarkStates)[ObjectIndex] = static_cast<uint8>(EMarkState::Black);
				}
				if (Visited != nullptr)
				{
					const UClass* Class = Object->GetClass();
					const bool bReportsReferences =
						Class->ClassAddReferencedObjects != &UObject::AddReferencedObjects &&
						!GUObjectArray.IndexToObject(ObjectIndex)->HasAnyFlags(EInternalObjectFlags::Native);
					Visited->Add(
						FGCVisitedObject{ObjectIndex, Records->Num(), 0, Object->GetOuter(), bReportsReferences});
				}
			}
			ProcessObjectReferences(Object);
			if (Visited != nullptr)
			{
				FGCVisitedObject& Entry = Visited->Last();
				Entry.NumRecords = Records->Num() - Entry.FirstRecord;
			}
		}

		void ProcessObjectReferences(UObject* Object)
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
		/** The queue's objects before this one are visited. */
		int32 ProcessedIndex = 0;
		int32 NumReferencesCleared = 0;
		int32 NumRevisited = 0;
		/** Incremental mode (SetIncremental), null for a full collection. */
		TArray<uint8>* MarkStates = nullptr;
		TArray<FGCSlotRecord>* Records = nullptr;
		TArray<uint8>* Snapshot = nullptr;
		TArray<FGCVisitedObject>* Visited = nullptr;
	};

	/** An incremental collection under way (StartIncrementalGarbageCollection). */
	struct FIncrementalCollection
	{
		bool bPending = false;
		EObjectFlags KeepFlags = RF_NoFlags;
		/** Each slot's EMarkState, for the slots the array had when it started. */
		TArray<uint8> MarkStates;
		TArray<FGCSlotRecord> Records;
		/** The copies of the recorded containers' bytes. */
		TArray<uint8> Snapshot;
		TArray<FGCVisitedObject> Visited;
		TUniquePtr<FGarbageCollectionMarker> Marker;
		FGarbageCollectionStats Stats;
		double MarkSeconds = 0.0;
		int32 NumSlices = 0;
	};
	FIncrementalCollection GIncremental;

	/** Heap in use, for the statistics. */
	SIZE_T GetHeapBytes()
	{
		return FMemory::GetUsage().CurrentBytes;
	}

	/** Forgets an incremental collection (its marks are its own: nothing to undo in the objects). */
	void ResetIncrementalCollection()
	{
		GIncremental.bPending = false;
		GIncremental.MarkStates.Reset();
		GIncremental.Records.Reset();
		GIncremental.Snapshot.Reset();
		GIncremental.Visited.Reset();
		GIncremental.Marker.Reset();
		GIncremental.MarkSeconds = 0.0;
		GIncremental.NumSlices = 0;
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
	// A full collection replaces an incremental one under way; what the last collection left for later is destroyed
	// first (UE).
	if (GIncremental.bPending)
	{
		ResetIncrementalCollection();
	}
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

bool IsIncrementalReachabilityAnalysisPending()
{
	return GIncremental.bPending;
}

void StartIncrementalGarbageCollection(EObjectFlags KeepFlags)
{
	checkf(!GIsGarbageCollecting, "StartIncrementalGarbageCollection called while garbage is being collected");
	if (!UObjectInitialized() || GIncremental.bPending)
	{
		return;
	}
	// The last collection's objects go first: no slot is freed while the marks are taken.
	IncrementalPurgeGarbage(false);
	GIsGarbageCollecting = true;
	const double StartTime = FPlatformTime::Seconds();
	FIncrementalCollection& Collection = GIncremental;
	Collection.bPending = true;
	Collection.KeepFlags = KeepFlags;
	Collection.Stats = FGarbageCollectionStats();
	Collection.Stats.HeapBytesBefore = GetHeapBytes();
	Collection.Stats.NumObjectsBefore = GUObjectArray.GetObjectArrayNumMinusAvailable();
	Collection.Marker = MakeUnique<FGarbageCollectionMarker>();
	FGarbageCollectionMarker& Marker = *Collection.Marker;
	Marker.SetIncremental(&Collection.MarkStates, &Collection.Records, &Collection.Snapshot, &Collection.Visited);

	// Every object white but the roots, which are queued; an empty slot is marked so (a later object in it is new).
	const int32 NumSlots = GUObjectArray.GetObjectArrayNum();
	Collection.MarkStates.SetNumUninitialized(NumSlots);
	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		FUObjectItem* Item = GUObjectArray.IndexToObject(Index);
		if (Item->Object == nullptr)
		{
			Collection.MarkStates[Index] = static_cast<uint8>(EMarkState::Empty);
			continue;
		}
		UObject* Object = (UObject*)Item->Object;
		if (IsGarbageCollectionRoot(*Item, Object, KeepFlags))
		{
			Collection.MarkStates[Index] = static_cast<uint8>(EMarkState::Gray);
			Marker.Enqueue(Object);
		}
		else
		{
			Collection.MarkStates[Index] = static_cast<uint8>(EMarkState::White);
		}
	}
	// The references non-UObjects hold now (they report again at the end).
	const TArray<FGCObject*> GCObjects = GetGCObjects();
	for (FGCObject* GCObject : GCObjects)
	{
		GCObject->AddReferencedObjects(Marker);
	}
	Collection.MarkSeconds += FPlatformTime::Seconds() - StartTime;
	GIsGarbageCollecting = false;
}

namespace
{
	/**
	 * The end of an incremental collection, at once: what changed since the slices is marked (roots and new objects,
	 * the non-UObjects' references, the visited objects whose references changed, what their classes report), the
	 * rest of the queue is visited, then the white objects are swept and purged as in CollectGarbage.
	 */
	void FinishIncrementalGarbageCollection()
	{
		FIncrementalCollection& Collection = GIncremental;
		FGarbageCollectionMarker& Marker = *Collection.Marker;
		const double StartTime = FPlatformTime::Seconds();
		const int32 NumMarked = Collection.MarkStates.Num();
		const int32 NumSlots = GUObjectArray.GetObjectArrayNum();
		// Recording stops: the visits of the end append nothing to the records it checks.
		const TArray<FGCVisitedObject>& Visited = Collection.Visited;
		Marker.StopRecording();

		// The objects made during the collection are kept and visited, and so is an object rooted since.
		Collection.MarkStates.SetNum(NumSlots);
		for (int32 Index = 0; Index < NumSlots; ++Index)
		{
			FUObjectItem* Item = GUObjectArray.IndexToObject(Index);
			if (Item->Object == nullptr)
			{
				continue;
			}
			UObject* Object = (UObject*)Item->Object;
			const EMarkState State =
				Index < NumMarked ? static_cast<EMarkState>(Collection.MarkStates[Index]) : EMarkState::Empty;
			if (State == EMarkState::Empty ||
				(State == EMarkState::White && IsGarbageCollectionRoot(*Item, Object, Collection.KeepFlags)))
			{
				Collection.MarkStates[Index] = static_cast<uint8>(EMarkState::Gray);
				Marker.Enqueue(Object);
			}
		}
		const TArray<FGCObject*> GCObjects = GetGCObjects();
		for (FGCObject* GCObject : GCObjects)
		{
			GCObject->AddReferencedObjects(Marker);
		}
		// The objects visited in the slices whose references changed are visited again.
		for (const FGCVisitedObject& Entry : Visited)
		{
			Marker.Revisit((UObject*)GUObjectArray.IndexToObject(Entry.ObjectIndex)->Object, Entry, Collection.Records,
				Collection.Snapshot);
		}
		Marker.ProcessObjects();
		const double MarkEndTime = FPlatformTime::Seconds();

		// Sweep: the white objects are the unreachable ones (the flags are set now, as a full collection sets them).
		for (int32 Index = 0; Index < NumSlots; ++Index)
		{
			FUObjectItem* Item = GUObjectArray.IndexToObject(Index);
			if (Item->Object == nullptr)
			{
				continue;
			}
			if (Collection.MarkStates[Index] == static_cast<uint8>(EMarkState::White))
			{
				Item->SetFlags(EInternalObjectFlags::Unreachable);
				GUnreachableObjects.Add((UObject*)Item->Object);
			}
			else
			{
				Item->ClearFlags(EInternalObjectFlags::Unreachable);
			}
		}
		FGarbageCollectionStats Stats = Collection.Stats;
		Stats.NumObjectsCollected = GUnreachableObjects.Num();
		Stats.NumReferencesCleared = Marker.GetNumReferencesCleared();
		Stats.NumObjectsRevisited = Marker.GetNumRevisited();
		Stats.NumSlices = Collection.NumSlices;
		const int32 NumProcessed = Marker.GetNumProcessed();
		const double SliceSeconds = Collection.MarkSeconds;
		BeginDestroyUnreachableObjects();
		ResetIncrementalCollection();
		GIsGarbageCollecting = false;

		// Purged at this safe point.
		IncrementalPurgeGarbage(false);
		const double EndTime = FPlatformTime::Seconds();
		Stats.MarkSeconds = SliceSeconds + (MarkEndTime - StartTime);
		Stats.FinishSeconds = MarkEndTime - StartTime;
		Stats.PurgeSeconds = EndTime - MarkEndTime;
		Stats.bIncremental = true;
		Stats.NumObjectsAfter = GUObjectArray.GetObjectArrayNumMinusAvailable();
		Stats.HeapBytesAfter = GetHeapBytes();
		GLastStats = Stats;
		UE_LOG(LogGarbage, Log,
			TEXT("Collected %d of %d objects incrementally: %d slice(s), %d visited, %d visited again at the end "
				 "(end %.3f ms, purge %.3f ms), %d references cleared, %d objects left"),
			Stats.NumObjectsCollected, Stats.NumObjectsBefore, Stats.NumSlices, NumProcessed, Stats.NumObjectsRevisited,
			Stats.FinishSeconds * 1000.0, Stats.PurgeSeconds * 1000.0, Stats.NumReferencesCleared,
			Stats.NumObjectsAfter);
	}
} // namespace

bool IncrementalCollectGarbageStep(int32 ObjectBudget)
{
	checkf(!GIsGarbageCollecting, "IncrementalCollectGarbageStep called while garbage is being collected");
	if (!GIncremental.bPending)
	{
		return false;
	}
	checkf(FUObjectThreadContext::Get().InitializerStack.Num() == 0 &&
			FUObjectThreadContext::Get().PendingConstructions.Num() == 0,
		"IncrementalCollectGarbageStep called while an object is being constructed");
	GIsGarbageCollecting = true;
	++GIncremental.NumSlices;
	const double StartTime = FPlatformTime::Seconds();
	const bool bQueueEmpty = GIncremental.Marker->ProcessObjects(FMath::Max(1, ObjectBudget));
	GIncremental.MarkSeconds += FPlatformTime::Seconds() - StartTime;
	if (!bQueueEmpty)
	{
		GIsGarbageCollecting = false;
		return false;
	}
	// The queue is empty: the collection ends in this step.
	FinishIncrementalGarbageCollection();
	return true;
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
		const TCHAR* Section = TEXT("/Script/Engine.GarbageCollectionSettings");
		GConfig->GetFloat(Section, TEXT("gc.TimeBetweenPurgingPendingKillObjects"),
			Settings.TimeBetweenPurgingPendingKillObjects, Filename);
		GConfig->GetInt(Section, TEXT("gc.IncrementalObjectsPerStep"), Settings.IncrementalObjectsPerStep, Filename);
	}
	return Settings;
}

FGarbageCollectionTimer::FGarbageCollectionTimer(const FGarbageCollectionSettings& InSettings)
	: Settings(InSettings)
{
}

bool FGarbageCollectionTimer::Tick(float DeltaSeconds, EObjectFlags KeepFlags)
{
	if (bForceCollection)
	{
		// A full collection (a level loaded, a round restarted): it replaces an incremental one under way.
		CollectGarbage(KeepFlags);
		TimeSinceLastCollection = 0.0f;
		bForceCollection = false;
		return true;
	}
	if (IsIncrementalReachabilityAnalysisPending())
	{
		return IncrementalCollectGarbageStep(Settings.IncrementalObjectsPerStep);
	}
	TimeSinceLastCollection += DeltaSeconds;
	if (TimeSinceLastCollection < Settings.TimeBetweenPurgingPendingKillObjects)
	{
		return false;
	}
	TimeSinceLastCollection = 0.0f;
	StartIncrementalGarbageCollection(KeepFlags);
	return IncrementalCollectGarbageStep(Settings.IncrementalObjectsPerStep);
}

void FGarbageCollectionTimer::ForceCollectOnNextTick()
{
	bForceCollection = true;
}
