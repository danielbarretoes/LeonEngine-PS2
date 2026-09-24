#pragma once

#include "CoreTypes.h"

/**
 * Input keys (UE: EKeys / FKey from InputCoreTypes.h, as an enum until Core has FName).
 * Keyboard values equal GLFW key codes so desktop backends pass them straight through.
 * Gamepad values follow the UE names; the PS2 DualShock maps as:
 *   Cross = FaceButton_Bottom, Circle = FaceButton_Right, Square = FaceButton_Left,
 *   Triangle = FaceButton_Top, L1/R1 = Left/RightShoulder, L2/R2 = Left/RightTrigger,
 *   Select = Special_Left, Start = Special_Right, L3/R3 = Left/RightThumbstick.
 */
enum class EKeys : int32
{
	Invalid = 0,

	SpaceBar = 32,
	Apostrophe = 39,
	Comma = 44,
	Hyphen = 45,
	Period = 46,
	Slash = 47,

	Zero = 48,
	One = 49,
	Two = 50,
	Three = 51,
	Four = 52,
	Five = 53,
	Six = 54,
	Seven = 55,
	Eight = 56,
	Nine = 57,

	Semicolon = 59,
	Equals = 61,

	A = 65,
	B = 66,
	C = 67,
	D = 68,
	E = 69,
	F = 70,
	G = 71,
	H = 72,
	I = 73,
	J = 74,
	K = 75,
	L = 76,
	M = 77,
	N = 78,
	O = 79,
	P = 80,
	Q = 81,
	R = 82,
	S = 83,
	T = 84,
	U = 85,
	V = 86,
	W = 87,
	X = 88,
	Y = 89,
	Z = 90,

	LeftBracket = 91,
	Backslash = 92,
	RightBracket = 93,
	Tilde = 96,

	Escape = 256,
	Enter = 257,
	Tab = 258,
	BackSpace = 259,
	Insert = 260,
	Delete = 261,
	Right = 262,
	Left = 263,
	Down = 264,
	Up = 265,
	PageUp = 266,
	PageDown = 267,
	Home = 268,
	End = 269,

	CapsLock = 280,
	ScrollLock = 281,
	NumLock = 282,
	PrintScreen = 283,
	Pause = 284,

	F1 = 290,
	F2 = 291,
	F3 = 292,
	F4 = 293,
	F5 = 294,
	F6 = 295,
	F7 = 296,
	F8 = 297,
	F9 = 298,
	F10 = 299,
	F11 = 300,
	F12 = 301,

	NumPadZero = 320,
	NumPadOne = 321,
	NumPadTwo = 322,
	NumPadThree = 323,
	NumPadFour = 324,
	NumPadFive = 325,
	NumPadSix = 326,
	NumPadSeven = 327,
	NumPadEight = 328,
	NumPadNine = 329,
	Decimal = 330,
	Divide = 331,
	Multiply = 332,
	Subtract = 333,
	Add = 334,
	NumPadEnter = 335,  // Leon: UE folds it into Enter
	NumPadEquals = 336, // Leon

	LeftShift = 340,
	LeftControl = 341,
	LeftAlt = 342,
	LeftCommand = 343,
	RightShift = 344,
	RightControl = 345,
	RightAlt = 346,
	RightCommand = 347,
	Menu = 348, // Leon

	// Gamepad buttons
	Gamepad_FaceButton_Bottom = 1000,
	Gamepad_FaceButton_Right,
	Gamepad_FaceButton_Left,
	Gamepad_FaceButton_Top,
	Gamepad_LeftShoulder,
	Gamepad_RightShoulder,
	Gamepad_LeftTrigger,
	Gamepad_RightTrigger,
	Gamepad_Special_Left,
	Gamepad_Special_Right,
	Gamepad_LeftThumbstick,
	Gamepad_RightThumbstick,
	Gamepad_DPad_Up,
	Gamepad_DPad_Down,
	Gamepad_DPad_Left,
	Gamepad_DPad_Right,

	// Gamepad analog axes in [-1, 1]; Y is +1 when the stick is pushed up
	Gamepad_LeftX = 1100,
	Gamepad_LeftY,
	Gamepad_RightX,
	Gamepad_RightY,
};

/** True for EKeys::Gamepad_* buttons and axes. */
inline constexpr bool IsGamepadKey(EKeys Key)
{
	return static_cast<int32>(Key) >= static_cast<int32>(EKeys::Gamepad_FaceButton_Bottom);
}

/** Integer code of a keyboard key (the GLFW key code on desktop). */
inline constexpr int32 ToKeyCode(EKeys Key)
{
	return static_cast<int32>(Key);
}
