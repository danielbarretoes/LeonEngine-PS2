#pragma once

// References to UObjects from things that are not UObjects (UE: UObject/GCObject.h).

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"

/**
 * A non-UObject that keeps UObjects alive: the garbage collector calls AddReferencedObjects on every live FGCObject
 * and treats what it reports as roots (UE: FGCObject). Use it for UObject pointers held by plain C++ objects (an
 * FTickable, a subsystem, a cache); a UPROPERTY does the same for UObject members. Constructing it registers it,
 * destroying it unregisters it. UE keeps the list in a UGCObjectReferencer object; Leon keeps it in the collector.
 */
class COREUOBJECT_API FGCObject
{
public:
	FGCObject();
	FGCObject(const FGCObject& Other);
	FGCObject(FGCObject&& Other);
	virtual ~FGCObject();

	FGCObject& operator=(const FGCObject&)
	{
		return *this;
	}

	FGCObject& operator=(FGCObject&&)
	{
		return *this;
	}

	/** Reports each UObject this holds; a reported pointer to a pending-kill object is set to null (UE). */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) = 0;

	/** A name for debugging (UE). */
	virtual FString GetReferencerName() const
	{
		return TEXT("Unknown FGCObject");
	}

	/** Every live FGCObject, in registration order (Leon: the collector's root list). */
	static const TArray<FGCObject*>& GetRegisteredGCObjects();

private:
	void RegisterGCObject();
	void UnregisterGCObject();
};
