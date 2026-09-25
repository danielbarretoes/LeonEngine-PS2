#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Script.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectGlobals.h"

class FArchive;
class UFunction;

/**
 * The base class of every reflected object (UE: UObject). Created with NewObject; its UClass describes its
 * properties and functions; the class default object (CDO) holds the defaults. Destruction and garbage collection
 * arrive with P10, loading and saving with P11.
 */
class COREUOBJECT_API UObject : public UObjectBaseUtility
{
	// UObject is intrinsic: its boilerplate is written by hand, as in UE (Object.h).
	DECLARE_CLASS(UObject, UObject, CLASS_Abstract | CLASS_NoExport | CLASS_Intrinsic | CLASS_MatchedSerializers,
		CASTCLASS_None, TEXT("/Script/CoreUObject"), NO_API)
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UObject)
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UObject)

	static void StaticRegisterNativesUObject()
	{
	}

	/** The config file of the class: Engine unless UCLASS(Config=...) says otherwise (UE). */
	static const TCHAR* StaticConfigName()
	{
		return TEXT("Engine");
	}

	UObject();
	explicit UObject(const FObjectInitializer& ObjectInitializer);
	UObject(EStaticConstructor, EObjectFlags InFlags);
	UObject(FVTableHelper& Helper);

	/**
	 * Creates a default subobject of the object being constructed; only valid inside its constructor (UE). Each
	 * instance builds its own subobjects (no archetype instancing; plan decision D12).
	 */
	template <class TReturnType>
	TReturnType* CreateDefaultSubobject(FName SubobjectName, bool bTransient = false)
	{
		UClass* ReturnType = TReturnType::StaticClass();
		return static_cast<TReturnType*>(
			CreateDefaultSubobject(SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/true, bTransient));
	}

	/** Like CreateDefaultSubobject<TReturnType>, creating a TClassToCreateByDefault unless overridden (UE). */
	template <class TReturnType, class TClassToCreateByDefault>
	TReturnType* CreateDefaultSubobject(FName SubobjectName, bool bTransient = false)
	{
		return static_cast<TReturnType*>(CreateDefaultSubobject(SubobjectName, TReturnType::StaticClass(),
			TClassToCreateByDefault::StaticClass(), /*bIsRequired =*/true, bTransient));
	}

	UObject* CreateDefaultSubobject(
		FName SubobjectFName, UClass* ReturnType, UClass* ClassToCreateByDefault, bool bIsRequired, bool bIsTransient);

	/** Called once the constructor and the property initialization from the defaults are done (UE). */
	virtual void PostInitProperties();

	/** Called after the object is loaded (P11). */
	virtual void PostLoad();

	/** First step of destruction; releases resources (P10 garbage collection calls it). */
	virtual void BeginDestroy();

	/** True once asynchronous cleanup started in BeginDestroy has finished (P10). */
	virtual bool IsReadyForFinishDestroy();

	/** Last step before the memory is freed (P10). */
	virtual void FinishDestroy();

	/** Loads or saves the object's native data (P11; the reflected properties go through their FProperty). */
	virtual void Serialize(FArchive& Ar);

	/**
	 * The object this one was initialized from: its class default object, or, for a class default object, its super
	 * class's (UE: GetArchetype; Leon does not track per-instance archetypes).
	 */
	UObject* GetArchetype() const;

	/** True for an object created by CreateDefaultSubobject (UE). */
	bool IsDefaultSubobject() const;

	/** The default subobjects this object owns (UE). */
	void GetDefaultSubobjects(TArray<UObject*>& OutDefaultSubobjects) const;

	/** Finds a UFunction of this object's class or its supers by name (UE). */
	UFunction* FindFunction(FName InName) const;
	UFunction* FindFunctionChecked(FName InName) const;

	/**
	 * Calls a UFunction on this object with a parameter block laid out like the function's parameters (UE:
	 * ProcessEvent). The native exec thunk reads the parameters from Parms and writes the return value there.
	 */
	virtual void ProcessEvent(UFunction* Function, void* Parms);
};
