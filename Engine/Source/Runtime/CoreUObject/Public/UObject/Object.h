#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Script.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectGlobals.h"

class FArchive;
class FOutputDevice;
class FProperty;
class FReferenceCollector;
class UFunction;

/**
 * The base class of every reflected object (UE: UObject). Created with NewObject; its UClass describes its
 * properties and functions; the class default object (CDO) holds the defaults. The garbage collector destroys it
 * once nothing references it (UObject/GarbageCollection.h); packages save and load it (UPackage::Save, LoadObject).
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

	/**
	 * Called once the constructor and the property initialization from the defaults are done; a class default object
	 * (or a PerObjectConfig instance) has loaded its config by then (UE).
	 */
	virtual void PostInitProperties();

	/**
	 * Called once the object was loaded, after every object of the LoadPackage call (dependency packages included) was
	 * serialized, so other loaded objects it references hold their loaded values (UE). Overrides call Super::PostLoad.
	 */
	virtual void PostLoad();

	/** PostLoad unless it already ran: clears RF_NeedPostLoad, post-loads the archetype first, then PostLoad (UE). */
	void ConditionalPostLoad();

	/**
	 * First step of destruction, called by the garbage collector on an unreachable object: release resources, start
	 * asynchronous cleanup. Overrides must call Super::BeginDestroy, which renames the object to NAME_None (UE).
	 */
	virtual void BeginDestroy();

	/** True once asynchronous cleanup started in BeginDestroy has finished; FinishDestroy waits for it (UE). */
	virtual bool IsReadyForFinishDestroy();

	/** Last step before the destructor runs and the memory is freed; overrides must call Super::FinishDestroy (UE). */
	virtual void FinishDestroy();

	/** BeginDestroy unless it already ran; false when it had (UE). */
	bool ConditionalBeginDestroy();

	/** FinishDestroy unless it already ran; false when it had (UE). */
	bool ConditionalFinishDestroy();

	/**
	 * Reports the references of InThis that its reflected properties do not show (UE). A class that holds UObjects
	 * in members the collector cannot see declares its own static AddReferencedObjects, calls Super's and reports them
	 * through Collector; IMPLEMENT_CLASS records it in UClass::ClassAddReferencedObjects.
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// Config (UE: Obj.cpp). The section of a class is its path, "/Script/<Module>.<Class without prefix>".

	/**
	 * Sets the UPROPERTY(Config) members from the config: the section of ConfigClass (this object's class by default),
	 * or of the declaring class for a GlobalConfig member, in the class's file (UClass::GetConfigName) unless
	 * Filename names a GConfig key. A TArray member takes the "+Key=" values, or "Key[N]=" ones; a C array "Key[N]=".
	 * Values are parsed with FProperty::ImportText. PerObjectConfig objects read "<Name> <Class>". Missing keys keep
	 * their value (UE: LoadConfig).
	 */
	void LoadConfig(UClass* ConfigClass = nullptr, const TCHAR* Filename = nullptr,
		uint32 PropagationFlags = UE4::LCPF_None, FProperty* PropertyToLoad = nullptr);

	/**
	 * Writes the members with all of Flags (the config ones by default) to Config and saves the file's user layer,
	 * <Project>/Saved/Config/<Platform>/<File>.ini (desktop only: consoles log and write nothing, D8). An instance
	 * also copies the values to its class default object (UE: SaveConfig).
	 */
	void SaveConfig(uint64 Flags = CPF_Config, const TCHAR* Filename = nullptr, FConfigCacheIni* Config = GConfig,
		bool bAllowCopyToDefaultObject = true);

	/** LoadConfig that also reaches the instances and calls PostReloadConfig (UE: ReloadConfig). */
	void ReloadConfig(UClass* ConfigClass = nullptr, const TCHAR* Filename = nullptr,
		uint32 PropagationFlags = UE4::LCPF_None, FProperty* PropertyToLoad = nullptr);

	/** Called on each object after ReloadConfig set its members (UE). */
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded);

	/** Lets a PerObjectConfig class change the section name it reads and writes (UE). */
	virtual void OverridePerObjectConfigSection(FString& SectionName);

	/** <Project>/Config/Default<ConfigName>.ini, the file DefaultConfig classes are edited in (UE). */
	FString GetDefaultConfigFilename() const;

	// Console commands (UE: ScriptCore.cpp).

	/**
	 * Calls the UFUNCTION(Exec) named by the first word of Cmd with the following words as its parameters, each
	 * parsed with FProperty::ImportText (a last FString parameter takes the rest of the line). An object parameter
	 * first in the list receives Executor when it fits. Missing trailing parameters stay zero / default-initialized
	 * with a warning (Leon has no metadata for UE's CPP_Default_ values). Returns false when there is no such function
	 * or it is not Exec (unless bForceCallWithNonExec); true once handled, even when a parameter was bad (reported on
	 * Ar) (UE).
	 */
	bool CallFunctionByNameWithArguments(
		const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor, bool bForceCallWithNonExec = false);

	/** Runs a console command on this object: CallFunctionByNameWithArguments by default (UE). */
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor);

	/**
	 * Loads or saves the object (UE). This base version serializes the reflected properties
	 * (SerializeScriptProperties); a class with native data overrides it, calls Super::Serialize(Ar) first, then
	 * serializes its own members (the "native tail" of the object's package data), for example an FByteBulkData.
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Loads or saves the reflected properties as tagged properties: on save only those that differ from the archetype,
	 * then NAME_None; on load the tags present, skipping unknown ones (UE).
	 */
	void SerializeScriptProperties(FArchive& Ar) const;

	/**
	 * The object this one takes its defaults from (UE: GetArchetype): its class default object; for a class default
	 * object its super class's; for a default subobject, the subobject of the same name in its outer's archetype (the
	 * class default object's subobject), which the outer's constructor built the same way (D12). Packages save the
	 * properties that differ from it.
	 */
	UObject* GetArchetype() const;

	/**
	 * True for an asset: public, not transient, not a class default object, directly in a package other than the
	 * transient one (UE: IsAsset).
	 */
	virtual bool IsAsset() const;

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

/** True for a live object: not null, not pending kill (UE: IsValid). */
FORCEINLINE bool IsValid(const UObject* Test)
{
	return Test && !Test->IsPendingKill();
}

/** The GConfig key UObject::LoadConfig reads for SourceObject by default: its class's config file (UE). */
COREUOBJECT_API FString GetConfigFilename(UObject* SourceObject);

/** True when SourceObject's class is PerObjectConfig: its config section is named after the object (UE). */
COREUOBJECT_API bool UsesPerObjectConfig(UObject* SourceObject);
