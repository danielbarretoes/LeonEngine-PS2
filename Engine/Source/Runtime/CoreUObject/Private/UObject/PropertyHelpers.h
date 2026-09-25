#pragma once

// Text import and serialization helpers shared by the property types (UE: FPropertyHelpers).

#include "CoreMinimal.h"

class FArchive;
class FOutputDevice;
class UEnum;
struct FPropertyTag;

namespace UE::CoreUObject::Private
{
	/** Skips spaces and tabs. */
	const TCHAR* SkipWhitespace(const TCHAR* Buffer);

	/**
	 * Reads a token: a quoted string (with \" \\ \n \t escapes) or the text up to a delimiter (',' ')' whitespace).
	 * Returns the text after it, or nullptr on an unterminated string.
	 */
	const TCHAR* ReadToken(const TCHAR* Buffer, FString& OutToken, bool& bOutQuoted);

	/** Appends Value in quotes, escaping '"', '\\', newlines and tabs. */
	void AppendQuoted(FString& Out, const FString& Value);

	/** Writes an import error to ErrorText, or to the log when there is none. */
	void ReportImportError(FOutputDevice* ErrorText, const FString& Message);

	/** A number read from a tag of another numeric type (FProperty::ConvertFromType). */
	struct FNumericTagValue
	{
		bool bFloatingPoint = false;
		bool bUnsigned = false;
		int64 Signed = 0;
		uint64 Unsigned = 0;
		double Float = 0.0;
	};

	/** True for the tag of an integer: Int8 ... UInt64, or a byte without an enum. */
	bool IsIntegerTag(const FPropertyTag& Tag);

	/** True for the tag of a float or a double. */
	bool IsFloatingPointTag(const FPropertyTag& Tag);

	/** Reads the value of an integer or floating-point tag; false for any other tag. */
	bool ReadNumericTagValue(const FPropertyTag& Tag, FArchive& Ar, FNumericTagValue& OutValue);

	/**
	 * The value of the enumerator EnumValueName saved for Enum: its own name ("EMyEnum::Value"), or the same short name
	 * from another enum (a converted property). A name the enum does not have gives its _MAX value, with a warning
	 * unless the name is NAME_None (an invalid value saved as None) (UE: FEnumProperty::SerializeItem).
	 */
	int64 LoadEnumValue(const UEnum* Enum, FName EnumValueName, FArchive& Ar);

	/** The name an enum value is saved as: its enumerator's name, NAME_None when the value is not one (UE). */
	FName GetEnumValueNameForSave(const UEnum* Enum, int64 Value);
} // namespace UE::CoreUObject::Private
