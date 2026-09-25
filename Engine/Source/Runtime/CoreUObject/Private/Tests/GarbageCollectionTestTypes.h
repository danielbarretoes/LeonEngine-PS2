// Reflected fixtures of the CoreUObject garbage collection, reference and delegate tests.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "GarbageCollectionTestTypes.generated.h"

/** A struct holding strong references, directly and in an array. */
USTRUCT()
struct FGCTestStruct
{
	GENERATED_BODY()

	UPROPERTY()
	UObject* Object = nullptr;

	UPROPERTY()
	TArray<UObject*> Objects;
};

/** An object that references others in every way the collector follows, and some it must not. */
UCLASS()
class UGCTestObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UObject* Ref = nullptr;

	UPROPERTY()
	TArray<UObject*> RefArray;

	UPROPERTY()
	TMap<FName, UObject*> RefMap;

	UPROPERTY()
	TMap<UObject*, int32> KeyMap;

	UPROPERTY()
	TSet<UObject*> RefSet;

	UPROPERTY()
	FGCTestStruct Struct;

	UPROPERTY()
	TArray<FGCTestStruct> StructArray;

	UPROPERTY()
	UObject* FixedRefs[2];

	UPROPERTY()
	TSubclassOf<UObject> ClassRef;

	/** Weak and soft references do not keep their object alive. */
	UPROPERTY()
	TWeakObjectPtr<UObject> WeakRef;

	UPROPERTY()
	TSoftObjectPtr<UObject> SoftRef;

	/** Not reflected: reported by AddReferencedObjects. */
	UObject* NativeRef = nullptr;

	/** Not reflected and not reported: does not keep its object alive. */
	UObject* UnreportedRef = nullptr;

	/** Delegate target. */
	int32 Received = 0;

	UGCTestObject();

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	void OnValue(int32 Value)
	{
		Received += Value;
	}

	int32 GetReceived() const
	{
		return Received;
	}
};

/** Records its destruction steps, in order, in a log the tests read. */
UCLASS()
class UGCTestDestroyTracker : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UGCTestDestroyTracker() override;

	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;

	UPROPERTY()
	int32 Id = 0;

	/** How many IsReadyForFinishDestroy calls answer false before it is ready. */
	int32 NotReadyCount = 0;

	/** "Begin 1", "NotReady 1", "Finish 1", "Destroy 1", ... */
	static TArray<FString>& GetEventLog();
};
