#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.generated.h"

/**
 * An input key (UE: FKey, InputCoreTypes.h): a keyboard key, a mouse button or axis, a gamepad button or axis, named
 * by an FName ("SpaceBar", "MouseX", "Gamepad_FaceButton_Bottom"). The EKeys members are the known keys; config text
 * writes a key as its name (`Key=SpaceBar`).
 *
 * The platform layers map keys to their own codes: the desktop window to GLFW key and mouse button codes, the PS2
 * input interface to the DualShock's buttons (Cross = Gamepad_FaceButton_Bottom, Circle = Gamepad_FaceButton_Right,
 * Square = Gamepad_FaceButton_Left, Triangle = Gamepad_FaceButton_Top, L1 / R1 = Gamepad_Left/RightShoulder,
 * L2 / R2 = Gamepad_Left/RightTrigger, Select = Gamepad_Special_Left, Start = Gamepad_Special_Right, L3 / R3 =
 * Gamepad_Left/RightThumbstick).
 */
USTRUCT(BlueprintType)
struct INPUTCORE_API FKey
{
	GENERATED_BODY()

	FKey() = default;
	FKey(const FName InName)
		: KeyName(InName)
	{
	}
	FKey(const TCHAR* InName)
		: KeyName(FName(InName))
	{
	}

	/** A key EKeys knows (UE: IsValid). */
	[[nodiscard]] bool IsValid() const;
	/** Shift, Control, Alt or Command (UE). */
	[[nodiscard]] bool IsModifierKey() const;
	/** A gamepad button or axis (UE). */
	[[nodiscard]] bool IsGamepadKey() const;
	/** A mouse button (UE). */
	[[nodiscard]] bool IsMouseButton() const;
	/** A one-dimensional axis: the mouse and stick axes (UE: IsAxis1D). */
	[[nodiscard]] bool IsAxis1D() const;
	/** A button: pressed or released, no axis (UE: IsDigital). */
	[[nodiscard]] bool IsDigital() const;
	/** An axis that reads 0 in a frame without samples: the mouse axes (UE: ShouldUpdateAxisWithoutSamples). */
	[[nodiscard]] bool ShouldUpdateAxisWithoutSamples() const;

	/** The key's name as text (UE: GetDisplayName; Leon has no localized key names). */
	[[nodiscard]] FText GetDisplayName(bool bLongDisplayName = true) const;
	[[nodiscard]] FString ToString() const;
	[[nodiscard]] FName GetFName() const
	{
		return KeyName;
	}

	/** The config text of a key: its name (UE). */
	bool ExportTextItem(
		FString& ValueStr, FKey const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	/** Reads a key name, quoted or not, from Buffer and advances it (UE). */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	friend bool operator==(const FKey& KeyA, const FKey& KeyB)
	{
		return KeyA.KeyName == KeyB.KeyName;
	}
	friend bool operator!=(const FKey& KeyA, const FKey& KeyB)
	{
		return KeyA.KeyName != KeyB.KeyName;
	}
	friend bool operator<(const FKey& KeyA, const FKey& KeyB)
	{
		return KeyA.KeyName.LexicalLess(KeyB.KeyName);
	}
	friend uint32 GetTypeHash(const FKey& Key)
	{
		return GetTypeHash(Key.KeyName);
	}

private:
	/** UE: KeyName. */
	UPROPERTY()
	FName KeyName;

	friend struct EKeys;
};

template <>
struct TStructOpsTypeTraits<FKey> : public TStructOpsTypeTraitsBase2<FKey>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithIdenticalViaEquality = true,
	};
};

/** What EKeys knows about a key (UE: FKeyDetails; Leon keeps the flags, not the localized names). */
struct INPUTCORE_API FKeyDetails
{
	enum EKeyFlags
	{
		GamepadKey = 1 << 0,
		MouseButton = 1 << 2,
		ModifierKey = 1 << 3,
		Axis1D = 1 << 5,
		UpdateAxisWithoutSamples = 1 << 7,
		NoFlags = 0,
	};

	explicit FKeyDetails(const FKey InKey, const uint32 InKeyFlags = 0);

	[[nodiscard]] bool IsModifierKey() const
	{
		return bIsModifierKey != 0;
	}
	[[nodiscard]] bool IsGamepadKey() const
	{
		return bIsGamepadKey != 0;
	}
	[[nodiscard]] bool IsMouseButton() const
	{
		return bIsMouseButton != 0;
	}
	[[nodiscard]] bool IsAxis1D() const
	{
		return bIsAxis1D != 0;
	}
	[[nodiscard]] bool ShouldUpdateAxisWithoutSamples() const
	{
		return bShouldUpdateAxisWithoutSamples != 0;
	}
	[[nodiscard]] const FKey& GetKey() const
	{
		return Key;
	}

private:
	FKey Key;
	uint8 bIsModifierKey : 1;
	uint8 bIsGamepadKey : 1;
	uint8 bIsMouseButton : 1;
	uint8 bIsAxis1D : 1;
	uint8 bShouldUpdateAxisWithoutSamples : 1;
};

/**
 * The known keys (UE: EKeys), static FKeys named as in UE. InputCore's StartupModule registers their details
 * (Initialize). Leon adds PrintScreen, NumPadEnter, NumPadEquals and Menu, which UE folds into other keys.
 */
struct INPUTCORE_API EKeys
{
	static const FKey AnyKey;

	static const FKey MouseX;
	static const FKey MouseY;
	static const FKey MouseScrollUp;
	static const FKey MouseScrollDown;
	static const FKey MouseWheelAxis;

	static const FKey LeftMouseButton;
	static const FKey RightMouseButton;
	static const FKey MiddleMouseButton;
	static const FKey ThumbMouseButton;
	static const FKey ThumbMouseButton2;

	static const FKey BackSpace;
	static const FKey Tab;
	static const FKey Enter;
	static const FKey Pause;

	static const FKey CapsLock;
	static const FKey Escape;
	static const FKey SpaceBar;
	static const FKey PageUp;
	static const FKey PageDown;
	static const FKey End;
	static const FKey Home;

	static const FKey Left;
	static const FKey Up;
	static const FKey Right;
	static const FKey Down;

	static const FKey Insert;
	static const FKey Delete;

	static const FKey Zero;
	static const FKey One;
	static const FKey Two;
	static const FKey Three;
	static const FKey Four;
	static const FKey Five;
	static const FKey Six;
	static const FKey Seven;
	static const FKey Eight;
	static const FKey Nine;

	static const FKey A;
	static const FKey B;
	static const FKey C;
	static const FKey D;
	static const FKey E;
	static const FKey F;
	static const FKey G;
	static const FKey H;
	static const FKey I;
	static const FKey J;
	static const FKey K;
	static const FKey L;
	static const FKey M;
	static const FKey N;
	static const FKey O;
	static const FKey P;
	static const FKey Q;
	static const FKey R;
	static const FKey S;
	static const FKey T;
	static const FKey U;
	static const FKey V;
	static const FKey W;
	static const FKey X;
	static const FKey Y;
	static const FKey Z;

	static const FKey NumPadZero;
	static const FKey NumPadOne;
	static const FKey NumPadTwo;
	static const FKey NumPadThree;
	static const FKey NumPadFour;
	static const FKey NumPadFive;
	static const FKey NumPadSix;
	static const FKey NumPadSeven;
	static const FKey NumPadEight;
	static const FKey NumPadNine;

	static const FKey Multiply;
	static const FKey Add;
	static const FKey Subtract;
	static const FKey Decimal;
	static const FKey Divide;

	static const FKey F1;
	static const FKey F2;
	static const FKey F3;
	static const FKey F4;
	static const FKey F5;
	static const FKey F6;
	static const FKey F7;
	static const FKey F8;
	static const FKey F9;
	static const FKey F10;
	static const FKey F11;
	static const FKey F12;

	static const FKey NumLock;
	static const FKey ScrollLock;

	static const FKey LeftShift;
	static const FKey RightShift;
	static const FKey LeftControl;
	static const FKey RightControl;
	static const FKey LeftAlt;
	static const FKey RightAlt;
	static const FKey LeftCommand;
	static const FKey RightCommand;

	static const FKey Semicolon;
	static const FKey Equals;
	static const FKey Comma;
	static const FKey Hyphen;
	static const FKey Period;
	static const FKey Slash;
	static const FKey Tilde;
	static const FKey LeftBracket;
	static const FKey Backslash;
	static const FKey RightBracket;
	static const FKey Apostrophe;

	/** Leon: keys UE folds into others (PrintScreen has no UE key; UE's Enter covers the keypad's). */
	static const FKey PrintScreen;
	static const FKey NumPadEnter;
	static const FKey NumPadEquals;
	static const FKey Menu;

	static const FKey Gamepad_LeftX;
	static const FKey Gamepad_LeftY;
	static const FKey Gamepad_RightX;
	static const FKey Gamepad_RightY;

	static const FKey Gamepad_DPad_Up;
	static const FKey Gamepad_DPad_Down;
	static const FKey Gamepad_DPad_Right;
	static const FKey Gamepad_DPad_Left;

	static const FKey Gamepad_FaceButton_Bottom;
	static const FKey Gamepad_FaceButton_Right;
	static const FKey Gamepad_FaceButton_Left;
	static const FKey Gamepad_FaceButton_Top;
	static const FKey Gamepad_LeftShoulder;
	static const FKey Gamepad_RightShoulder;
	static const FKey Gamepad_LeftTrigger;
	static const FKey Gamepad_RightTrigger;
	static const FKey Gamepad_LeftThumbstick;
	static const FKey Gamepad_RightThumbstick;
	static const FKey Gamepad_Special_Left;
	static const FKey Gamepad_Special_Right;

	/** No key (UE: EKeys::Invalid, named None). */
	static const FKey Invalid;

	/** Registers the details of every key above (UE: EKeys::Initialize, from InputCore's StartupModule). */
	static void Initialize();

	/** Registers a key (UE: AddKey). */
	static void AddKey(const FKeyDetails& KeyDetails);

	/** Every registered key, in registration order (UE: GetAllKeys). */
	static void GetAllKeys(TArray<FKey>& OutKeys);

	/** The key's details, or null for an unknown key (UE returns a shared pointer). */
	[[nodiscard]] static const FKeyDetails* GetKeyDetails(const FKey Key);
};
