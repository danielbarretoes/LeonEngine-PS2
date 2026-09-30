#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "EngineBaseTypes.generated.h"

class AActor;
class FTickTaskManager;
class UActorComponent;
class ULevel;

/** Whether the player who paused the game lets it go on (UE: FCanUnpause; AGameModeBase::SetPause): unbound, it may. */
DECLARE_DELEGATE_RetVal(bool, FCanUnpause);

/**
 * What a world tick runs (UE: ELevelTick): everything, or, while the world is paused (UWorld::IsPaused), only the tick
 * functions that tick when paused (FTickFunction::bTickEvenWhenPaused: the player controllers and the HUDs).
 */
enum ELevelTick
{
	/** Only the time moves (unused). */
	LEVELTICK_TimeOnly = 0,
	/** Only the viewports tick (unused). */
	LEVELTICK_ViewportsOnly = 1,
	/** Everything ticks. */
	LEVELTICK_All = 2,
	/** The world is paused: its time and timers stand still, and only bTickEvenWhenPaused tick functions run. */
	LEVELTICK_PauseTick = 3,
};

/**
 * When in a world step a tick function runs (UE: ETickingGroup, the groups Leon has). UWorld::Tick runs them in this
 * order around the physics step: PrePhysics (the controllers, the pawns and their movement: UE's default), the physics
 * step, DuringPhysics (whatever does not care about this step's physics: it runs after it on the one thread),
 * PostPhysics (what follows the bodies), then the timers and the camera managers, then PostUpdateWork (after the
 * cameras).
 */
enum ETickingGroup : uint8
{
	TG_PrePhysics = 0,
	TG_DuringPhysics,
	TG_PostPhysics,
	TG_PostUpdateWork,
	TG_MAX,
};

struct FTickFunction;

/** A tick function another one waits for, and the object that owns it (UE: FTickPrerequisite). */
struct ENGINE_API FTickPrerequisite
{
	/** The owner: once it is gone the prerequisite is ignored (UE: PrerequisiteObject). */
	TWeakObjectPtr<UObject> PrerequisiteObject;
	FTickFunction* PrerequisiteTickFunction = nullptr;

	FTickPrerequisite() = default;
	FTickPrerequisite(UObject* TargetObject, FTickFunction& TargetTickFunction);

	/** The tick function while its owner lives, else null (UE: Get). */
	[[nodiscard]] FTickFunction* Get() const;

	bool operator==(const FTickPrerequisite& Other) const
	{
		return PrerequisiteObject == Other.PrerequisiteObject &&
			PrerequisiteTickFunction == Other.PrerequisiteTickFunction;
	}
};

/**
 * Something the world ticks every step (UE: FTickFunction, the part Leon uses; one thread, no task graph). A subclass
 * says what to run (ExecuteTick); an actor's is AActor::PrimaryActorTick, a component's
 * UActorComponent::PrimaryComponentTick.
 *
 * - Set the options before registering: bCanEverTick (nothing registers without it), TickGroup, TickInterval and
 *   bStartWithTickEnabled. RegisterTickFunction puts it in its level's world (actors and components do it when they
 *   begin play, and undo it when they end play or unregister).
 * - The world keeps, for each group, the list of the enabled tick functions only, so a disabled or never-ticking one
 *   costs nothing. A list keeps the order of the actors in their level (AActor::GetLevelOrder), with an actor's
 *   components before the actor, each in the order they were first registered; a tick function that is disabled and
 *   enabled again goes back to its place. Other tick functions (tests) follow the actors in the order they were first
 *   registered.
 * - TickInterval > 0 ticks once that many seconds of steps have gone by, with the time since its last tick as the
 *   delta; the interval's remainder carries over, so it keeps its rate on average whatever the step.
 * - AddPrerequisite makes it wait, within its group, for another tick function of that group that is enabled and due
 *   this step (UE; Leon ignores a prerequisite of another group: the groups already run in order).
 */
struct ENGINE_API FTickFunction
{
	FTickFunction();
	virtual ~FTickFunction();

	FTickFunction(const FTickFunction&) = delete;
	FTickFunction& operator=(const FTickFunction&) = delete;

	/** The group it runs in (UE: TickGroup). Set it before registering. */
	ETickingGroup TickGroup = TG_PrePhysics;

	/** Whether it may ever tick; without it RegisterTickFunction does nothing (UE: bCanEverTick). */
	uint8 bCanEverTick : 1;

	/** Enabled when registered (UE: bStartWithTickEnabled). */
	uint8 bStartWithTickEnabled : 1;

	/** Ticks while the world is paused too (UE: bTickEvenWhenPaused): the player's input and UI. */
	uint8 bTickEvenWhenPaused : 1;

	/** Seconds between two ticks; 0 ticks every step (UE: TickInterval). */
	float TickInterval = 0.0f;

	/** Adds it to the world of Level (UE: RegisterTickFunction); nothing without bCanEverTick or a world. */
	void RegisterTickFunction(ULevel* Level);
	/** Takes it out of its world (UE: UnRegisterTickFunction). */
	void UnRegisterTickFunction();
	[[nodiscard]] bool IsTickFunctionRegistered() const
	{
		return TickTaskManager != nullptr;
	}

	/** Turns it on or off (UE: SetTickFunctionEnable); before registering it says whether it starts enabled. */
	void SetTickFunctionEnable(bool bInEnabled);
	/** Whether it is enabled; before it registers, whether it will start enabled. */
	[[nodiscard]] bool IsTickFunctionEnabled() const;

	/** Changes the interval; the time already waited counts toward the new one (UE: UpdateTickIntervalAndCoolDown). */
	void UpdateTickIntervalAndCoolDown(float NewTickInterval);

	/** Waits, in its group, for TargetTickFunction of TargetObject (UE: AddPrerequisite); once only. */
	void AddPrerequisite(UObject* TargetObject, FTickFunction& TargetTickFunction);
	void RemovePrerequisite(UObject* TargetObject, FTickFunction& TargetTickFunction);
	[[nodiscard]] const TArray<FTickPrerequisite>& GetPrerequisites() const
	{
		return Prerequisites;
	}

	/** Runs the tick (UE: ExecuteTick, without the task graph's arguments). */
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType) = 0;

	/** What it ticks, for logs (UE: DiagnosticMessage). */
	[[nodiscard]] virtual FString DiagnosticMessage()
	{
		return TEXT("FTickFunction");
	}

protected:
	/**
	 * Where it goes in its group's list (lower first): an actor's and its components' place in their level, else a
	 * registration serial after every actor. Called once, on its first registration.
	 */
	[[nodiscard]] virtual uint64 MakeTickOrder();

	/**
	 * The tick functions this one also waits for: its own prerequisites, and for a component in its owner's group the
	 * owner's (Leon: a component ticks with its actor, as the pawn's did after its controller).
	 */
	virtual void GetEffectivePrerequisites(
		TArray<const FTickPrerequisite*, TInlineAllocator<8>>& OutPrerequisites) const;

private:
	friend class FTickTaskManager;

	TArray<FTickPrerequisite> Prerequisites;
	/** The world's manager while registered. */
	FTickTaskManager* TickTaskManager = nullptr;
	/** The place in the group's list (MakeTickOrder), kept across registrations. */
	uint64 TickOrder = 0;
	/** Seconds left until an interval tick is due (it ticks at 0 or below). */
	float TickCooldown = 0.0f;
	/** Seconds since it last ticked (the delta an interval tick gets). */
	float TimeSinceLastTick = 0.0f;
	/** The manager's step when it last ran, or was due, or was found not due (TickState's step). */
	uint32 StateFrame = 0;
	/** In this step: 0 not due, 1 due and not yet run, 2 run. */
	uint8 TickState = 0;
	/** The group whose list holds it (bInList). */
	uint8 ListGroup = 0;
	uint8 bTickEnabled : 1;
	uint8 bHasTickOrder : 1;
	/** In its group's list (enabled while registered). */
	uint8 bInList : 1;
	/** SetTickFunctionEnable was called: registering keeps that state instead of bStartWithTickEnabled. */
	uint8 bEnableRequested : 1;
};

/** An actor's tick (UE: FActorTickFunction): AActor::TickActor. */
struct ENGINE_API FActorTickFunction : public FTickFunction
{
	/** The actor (UE: Target). */
	AActor* Target = nullptr;

	void ExecuteTick(float DeltaTime, ELevelTick TickType) override;
	[[nodiscard]] FString DiagnosticMessage() override;

protected:
	[[nodiscard]] uint64 MakeTickOrder() override;
};

/** A component's tick (UE: FActorComponentTickFunction): UActorComponent::TickComponent. */
struct ENGINE_API FActorComponentTickFunction : public FTickFunction
{
	/** The component (UE: Target). */
	UActorComponent* Target = nullptr;

	void ExecuteTick(float DeltaTime, ELevelTick TickType) override;
	[[nodiscard]] FString DiagnosticMessage() override;

protected:
	[[nodiscard]] uint64 MakeTickOrder() override;
	void GetEffectivePrerequisites(
		TArray<const FTickPrerequisite*, TInlineAllocator<8>>& OutPrerequisites) const override;
};

/** How a URL is read against a base URL (UE: ETravelType). */
enum ETravelType
{
	/** The URL is complete: nothing comes from the base. */
	TRAVEL_Absolute,
	/** The base's options carry over; the map and the portal are the URL's. */
	TRAVEL_Partial,
	/** The base's map, portal and options carry over unless the URL gives them. */
	TRAVEL_Relative,
	TRAVEL_MAX,
};

/** What UEngine::Browse did (UE: EBrowseReturnVal). */
namespace EBrowseReturnVal
{
	enum Type
	{
		/** The map was loaded. */
		Success,
		/** The URL was invalid or the map could not be loaded; the error says why. */
		Failure,
		/** A network connection is pending (unused: Leon has no networking). */
		Pending,
	};
} // namespace EBrowseReturnVal

/**
 * A travel URL (UE: FURL): the map to open, its options and the portal to enter through, written
 * `Map?Option1=Value?Option2#Portal`. `?game=<GameMode>` picks the game mode (plan decision D18) and the portal is the
 * PlayerStartTag of the start to spawn at.
 *
 * The map is a long package name (`/Engine/Maps/Entry`, `/Game/Maps/X`) or the path of a `.lmap` file; a path that
 * starts with a drive letter, or with `/` without being a long package name, is a plain file name (UE's rule). Leon has
 * no networking, so the protocol, host and port of UE's URL are left out.
 */
USTRUCT()
struct ENGINE_API FURL
{
	GENERATED_BODY()

	/** The map (UE: Map); the project's GameDefaultMap when a URL gives none. */
	UPROPERTY()
	FString Map;

	/** The options, each `Key=Value` or `Key` (UE: Op). */
	UPROPERTY()
	TArray<FString> Op;

	/** The portal to enter through (UE: Portal), empty by default. */
	UPROPERTY()
	FString Portal;

	/** 1 when the text parsed (UE: Valid). */
	UPROPERTY()
	int32 Valid = 1;

	/**
	 * A URL for a map (UE). Without a file name the map stays empty (UE puts the default map there; Leon keeps the
	 * struct's defaults free of config reads, and the parsing constructor fills the default map).
	 */
	explicit FURL(const TCHAR* Filename = nullptr);

	/**
	 * Parses TextURL; Base supplies what Type carries over, and a text without a map gets the base's map (relative) or
	 * the project's GameDefaultMap (UE).
	 */
	FURL(const FURL* Base, const TCHAR* TextURL, ETravelType Type);

	/** Whether an option is present, `Key` or `Key=...`, compared without case (UE: HasOption). */
	[[nodiscard]] bool HasOption(const TCHAR* Test) const;

	/**
	 * The text after Match in the first option starting with it (`Match` = "game=" returns the value), or Default
	 * (UE: GetOption).
	 */
	[[nodiscard]] const TCHAR* GetOption(const TCHAR* Match, const TCHAR* Default) const;

	/** Adds `Key=Value` or `Key`, replacing an option with the same key (UE: AddOption). */
	void AddOption(const TCHAR* Str);

	/** Removes the options with this key (UE: RemoveOption). */
	void RemoveOption(const TCHAR* Key);

	/** `Map?Op1?Op2#Portal` (UE: ToString; FullyQualified would add the protocol, which Leon does not have). */
	[[nodiscard]] FString ToString(bool FullyQualified = false) const;

	/** The map, options and portal are the same (maps and portals compared without case) (UE). */
	bool operator==(const FURL& Other) const;
	bool operator!=(const FURL& Other) const
	{
		return !(*this == Other);
	}
};

/** How the game window captures the mouse (UE: EMouseCaptureMode). */
UENUM()
enum class EMouseCaptureMode : uint8
{
	/** Never captured. */
	NoCapture,
	/** Captured from the start, until the game releases it. */
	CapturePermanently,
	/** Captured from the start, the first click included (UE's default). */
	CapturePermanently_IncludingInitialMouseDown,
	/** Captured while a mouse button is down. */
	CaptureDuringMouseDown,
	/** Captured while the right mouse button is down. */
	CaptureDuringRightMouseDown,
};

/** Input events (UE: EInputEvent, EngineBaseTypes.h). */
enum EInputEvent
{
	IE_Pressed = 0,
	IE_Released = 1,
	IE_Repeat = 2,
	IE_DoubleClick = 3,
	IE_Axis = 4,
	IE_MAX = 5,
};
