#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class FObjectInitializer;
class UClass;
class UObject;
class UObjectBase;

/**
 * State of the object construction in progress (UE: FUObjectThreadContext; Leon constructs objects on one thread).
 * StaticAllocateObject records the object it prepared; UObjectBase's constructor takes it. The initializer stack holds
 * the FObjectInitializer of every object whose constructor is running, innermost last.
 */
class FUObjectThreadContext
{
public:
	static FUObjectThreadContext& Get();

	/** An object StaticAllocateObject prepared and whose UObjectBase constructor has not run yet. */
	struct FPendingConstruction
	{
		UObjectBase* Memory;
		UClass* Class;
		UObject* Outer;
		FName Name;
		EObjectFlags Flags;
	};

	TArray<FPendingConstruction> PendingConstructions;
	TArray<FObjectInitializer*> InitializerStack;

	/**
	 * While UPackage::Save collects the references of its exports: the packages of the soft object paths they save
	 * (UE: the soft package references of FArchiveSaveTagImports).
	 */
	TArray<FName>* SoftPackageReferenceCollector = nullptr;

	FORCEINLINE FObjectInitializer* TopInitializer() const
	{
		return InitializerStack.Num() ? InitializerStack.Last() : nullptr;
	}
};

/** Records CoreUObject's intrinsic classes like generated ones (Class.cpp; called by UObjectBaseInit). */
void UObjectRegisterIntrinsicClasses();

/** Registers the class objects whose registration waited for the object system (UObjectBase.cpp). */
void UObjectProcessRegistrants();
