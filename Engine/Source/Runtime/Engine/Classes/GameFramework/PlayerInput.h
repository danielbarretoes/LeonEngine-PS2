#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "PlayerInput.generated.h"

class APlayerController;
class UInputComponent;
class UWorld;

/** A key that triggers an action, with the modifiers it needs (UE: FInputActionKeyMapping). */
USTRUCT()
struct ENGINE_API FInputActionKeyMapping
{
	GENERATED_BODY()

	/** The action (UE: ActionName). */
	UPROPERTY()
	FName ActionName;

	/** Shift, Control, Alt and Command must be down (UE). */
	UPROPERTY()
	uint8 bShift : 1;

	UPROPERTY()
	uint8 bCtrl : 1;

	UPROPERTY()
	uint8 bAlt : 1;

	UPROPERTY()
	uint8 bCmd : 1;

	/** The key (UE: Key). */
	UPROPERTY()
	FKey Key;

	FInputActionKeyMapping(const FName InActionName = NAME_None, const FKey InKey = EKeys::Invalid,
		const bool bInShift = false, const bool bInCtrl = false, const bool bInAlt = false, const bool bInCmd = false);

	bool operator==(const FInputActionKeyMapping& Other) const
	{
		return ActionName == Other.ActionName && Key == Other.Key && bShift == Other.bShift && bCtrl == Other.bCtrl &&
			bAlt == Other.bAlt && bCmd == Other.bCmd;
	}
};

/** A key that feeds an axis, times a scale (UE: FInputAxisKeyMapping). */
USTRUCT()
struct ENGINE_API FInputAxisKeyMapping
{
	GENERATED_BODY()

	/** The axis (UE: AxisName). */
	UPROPERTY()
	FName AxisName;

	/** The key's value is multiplied by this (UE: Scale); -1 for the opposite direction. */
	UPROPERTY()
	float Scale = 1.0f;

	/** The key (UE: Key). */
	UPROPERTY()
	FKey Key;

	FInputAxisKeyMapping(
		const FName InAxisName = NAME_None, const FKey InKey = EKeys::Invalid, const float InScale = 1.0f)
		: AxisName(InAxisName)
		, Scale(InScale)
		, Key(InKey)
	{
	}

	bool operator==(const FInputAxisKeyMapping& Other) const
	{
		return AxisName == Other.AxisName && Key == Other.Key && Scale == Other.Scale;
	}
};

/** How an axis key's raw value is shaped (UE: FInputAxisProperties). */
USTRUCT()
struct ENGINE_API FInputAxisProperties
{
	GENERATED_BODY()

	/** Values below this are 0 (UE: DeadZone). */
	UPROPERTY()
	float DeadZone = 0.2f;

	/** Multiplies the value (UE: Sensitivity): the mouse's degrees per pixel in Leon's input. */
	UPROPERTY()
	float Sensitivity = 1.0f;

	/** The value's curve (UE: Exponent). */
	UPROPERTY()
	float Exponent = 1.0f;

	/** Negates the value (UE: bInvert). */
	UPROPERTY()
	uint8 bInvert : 1;

	FInputAxisProperties()
		: bInvert(0)
	{
	}
};

/** The properties of one axis key (UE: FInputAxisConfigEntry). */
USTRUCT()
struct ENGINE_API FInputAxisConfigEntry
{
	GENERATED_BODY()

	/** The axis key, e.g. MouseX (UE: AxisKeyName). */
	UPROPERTY()
	FName AxisKeyName;

	UPROPERTY()
	FInputAxisProperties AxisProperties;
};

/** A console command bound to a key, for development (UE: FKeyBind, UPlayerInput::DebugExecBindings). */
USTRUCT()
struct ENGINE_API FKeyBind
{
	GENERATED_BODY()

	/** The key (UE: Key). */
	UPROPERTY()
	FKey Key;

	/** The command, `|`-separated for several; `OnRelease <Cmd>` runs on release (UE: Command). */
	UPROPERTY()
	FString Command;

	/** The modifiers that must be down (UE: Control, Shift, Alt, Cmd) or may be (UE: bIgnoreCtrl ...). */
	UPROPERTY()
	uint8 Control : 1;

	UPROPERTY()
	uint8 Shift : 1;

	UPROPERTY()
	uint8 Alt : 1;

	UPROPERTY()
	uint8 Cmd : 1;

	UPROPERTY()
	uint8 bIgnoreCtrl : 1;

	UPROPERTY()
	uint8 bIgnoreShift : 1;

	UPROPERTY()
	uint8 bIgnoreAlt : 1;

	UPROPERTY()
	uint8 bIgnoreCmd : 1;

	/** Not used (UE: bDisabled). */
	UPROPERTY()
	uint8 bDisabled : 1;

	FKeyBind()
		: Control(0)
		, Shift(0)
		, Alt(0)
		, Cmd(0)
		, bIgnoreCtrl(0)
		, bIgnoreShift(0)
		, bIgnoreAlt(0)
		, bIgnoreCmd(0)
		, bDisabled(0)
	{
	}
};

/** The input state of one key (UE: FKeyState). */
struct FKeyState
{
	/** The value the device reported, before the axis properties (UE: RawValue). */
	float RawValue = 0.0f;
	/** The value after the axis properties (UE: Value). */
	float Value = 0.0f;
	/** The key is down (UE: bDown). */
	uint8 bDown : 1;
	/** Down in the frame before (UE: bDownPrevious). */
	uint8 bDownPrevious : 1;
	/** A binding took the key this frame (UE: bConsumed). */
	uint8 bConsumed : 1;
	/** Samples received since the last frame (UE: SampleCountAccumulator). */
	uint8 SampleCountAccumulator = 0;
	/** Sum of the samples received since the last frame (UE: RawValueAccumulator, one axis here). */
	float RawValueAccumulator = 0.0f;
	/** This frame's events, as event numbers in arrival order (UE: EventCounts). */
	TArray<uint32> EventCounts[IE_MAX];
	/** The events received since the last frame (UE: EventAccumulator). */
	TArray<uint32> EventAccumulator[IE_MAX];

	FKeyState()
		: bDown(0)
		, bDownPrevious(0)
		, bConsumed(0)
	{
	}
};

/**
 * A player controller's input (UE: UPlayerInput, created by APlayerController::InitInputSystem with the controller as
 * its outer): the state of every key, and the action and axis mappings copied from UInputSettings (BaseInput.ini,
 * the project's DefaultInput.ini).
 *
 * - The viewport client sends key events (InputKey: pressed / released) and axis samples (InputAxis: the mouse's
 *   pixels) as they happen.
 * - Each frame the controller's ProcessPlayerInput builds the input stack and calls ProcessInputStack: the events and
 *   samples become this frame's key state, then every component on the stack, from the top, matches its action
 *   bindings against the mapped keys' events and sums each axis binding from its mapped keys' values times their
 *   scales; a binding takes (consumes) its keys from the components below. The action delegates run in the order the
 *   events came, then the axis delegates.
 */
UCLASS(Config = Input, Transient)
class ENGINE_API UPlayerInput : public UObject
{
	GENERATED_BODY()

public:
	UPlayerInput(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Copies the input settings' mappings (UE: the constructor and ForceRebuildingKeyMaps). */
	void PostInitProperties() override;

	/** A key event (UE: InputKey): pressed, released or repeated, with how far it is pressed. */
	virtual bool InputKey(FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad);

	/** An axis sample (UE: InputAxis): the mouse's pixels this event, added to the frame's value. */
	virtual bool InputAxis(FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad);

	/**
	 * Turns the frame's events and samples into the key state and dispatches the bindings of the components, the last
	 * of the array first (UE: ProcessInputStack).
	 */
	virtual void ProcessInputStack(
		const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused);

	/** Releases every key and forgets the pending events (UE: FlushPressedKeys). */
	void FlushPressedKeys();

	/** The key is down (UE: IsPressed). */
	[[nodiscard]] bool IsPressed(FKey InKey) const;
	/** The key went down this frame (UE: WasJustPressed). */
	[[nodiscard]] bool WasJustPressed(FKey InKey) const;
	/** The key went up this frame (UE: WasJustReleased). */
	[[nodiscard]] bool WasJustReleased(FKey InKey) const;
	/** The key's value this frame, after the axis properties (UE: GetKeyValue). */
	[[nodiscard]] float GetKeyValue(FKey InKey) const;
	/** The key's value before the axis properties (UE: GetRawKeyValue). */
	[[nodiscard]] float GetRawKeyValue(FKey InKey) const;

	/** The keys mapped to an action (UE: GetKeysForAction). */
	[[nodiscard]] const TArray<FInputActionKeyMapping>& GetKeysForAction(const FName ActionName) const;
	/** The keys mapped to an axis (UE: GetKeysForAxis). */
	[[nodiscard]] const TArray<FInputAxisKeyMapping>& GetKeysForAxis(const FName AxisName) const;

	/** Adds a mapping to this player only (UE: AddActionMapping / AddAxisMapping). */
	void AddActionMapping(const FInputActionKeyMapping& KeyMapping);
	void AddAxisMapping(const FInputAxisKeyMapping& KeyMapping);
	/** Removes a mapping (UE). */
	void RemoveActionMapping(const FInputActionKeyMapping& KeyMapping);
	void RemoveAxisMapping(const FInputAxisKeyMapping& KeyMapping);

	/** Rebuilds the key maps; with bRestoreDefaults the mappings are copied from the settings again (UE). */
	void ForceRebuildingKeyMaps(const bool bRestoreDefaults = false);

	/**
	 * Sets the mouse axes' sensitivity for this player (UE: SetMouseSensitivity, an Exec command): the degrees a pixel
	 * turns the view in Leon's input (the settings' AxisConfig holds the defaults, [/Script/Engine.InputSettings] of
	 * the Input config).
	 */
	UFUNCTION(Exec)
	void SetMouseSensitivity(float Sensitivity);
	/** Different sensitivities for MouseX and MouseY (UE's two-value SetMouseSensitivity). */
	void SetMouseSensitivity(float SensitivityX, float SensitivityY);
	/** UE: GetMouseSensitivityX / GetMouseSensitivityY (1 when the axis has no config). */
	[[nodiscard]] float GetMouseSensitivityX();
	[[nodiscard]] float GetMouseSensitivityY();

	/** The controller this input belongs to (its outer), or null (UE: GetOuterAPlayerController). */
	[[nodiscard]] APlayerController* GetOuterAPlayerController() const;

	/**
	 * Keys that run console commands, outside shipping builds (UE: DebugExecBindings, [/Script/Engine.PlayerInput] of
	 * the Input config): LeonGame's F1-F6.
	 */
	UPROPERTY(Config)
	TArray<FKeyBind> DebugExecBindings;

	/** The command bound to a key with the current modifiers, or empty (UE: GetBind). */
	[[nodiscard]] FString GetBind(FKey Key) const;

	/**
	 * Runs a bound command through the player's Exec chain; `OnRelease` commands run on release, the others on
	 * press (UE: ExecInputCommands).
	 */
	bool ExecInputCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);

	/** This player's action mappings (UE: ActionMappings). */
	UPROPERTY(Transient)
	TArray<FInputActionKeyMapping> ActionMappings;

	/** This player's axis mappings (UE: AxisMappings). */
	UPROPERTY(Transient)
	TArray<FInputAxisKeyMapping> AxisMappings;

protected:
	/** The axis properties shape a key's raw value (UE: MassageAxisInput). */
	[[nodiscard]] float MassageAxisInput(FKey Key, float RawValue);

private:
	struct FActionKeyDetails
	{
		TArray<FInputActionKeyMapping> Actions;
	};
	struct FAxisKeyDetails
	{
		TArray<FInputAxisKeyMapping> KeyMappings;
	};
	struct FDelegateDispatchDetails;

	void ConditionalBuildKeyMappings();
	void ConditionalInitAxisProperties();
	[[nodiscard]] bool IsKeyConsumed(FKey Key) const;
	void ConsumeKey(FKey Key);
	[[nodiscard]] bool KeyEventOccurred(FKey Key, EInputEvent Event, TArray<uint32>& EventIndices) const;
	[[nodiscard]] bool IsKeyHandledByAction(FKey Key) const;
	[[nodiscard]] static bool ModifiersMatch(const FInputActionKeyMapping& Mapping, const UPlayerInput& Input);

	/** Every key seen (UE: KeyStateMap). */
	TMap<FKey, FKeyState> KeyStateMap;
	TMap<FName, FActionKeyDetails> ActionKeyMap;
	TMap<FName, FAxisKeyDetails> AxisKeyMap;
	/** The axis properties of the settings' AxisConfig, by key (UE: AxisProperties). */
	TMap<FKey, FInputAxisProperties> AxisProperties;
	/** The event of the key InputKey is handling (UE: CurrentEvent), for ExecInputCommands. */
	EInputEvent CurrentEvent = IE_Pressed;
	/** Numbers the events so the actions dispatch in arrival order (UE: EventCount). */
	uint32 EventCount = 0;
	bool bKeyMapsBuilt = false;
	bool bAxisPropertiesInitialized = false;
};
