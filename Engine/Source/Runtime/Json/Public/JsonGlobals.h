#pragma once

#include "CoreTypes.h"
#include "Logging/LogMacros.h"

JSON_API DECLARE_LOG_CATEGORY_EXTERN(LogJson, Log, All);

/** Type of a JSON value (UE: EJson). */
enum class EJson
{
	None,
	Null,
	String,
	Number,
	Boolean,
	Array,
	Object
};

/** What TJsonReader::ReadNext found (UE: EJsonNotation). */
enum class EJsonNotation
{
	ObjectStart,
	ObjectEnd,
	ArrayStart,
	ArrayEnd,
	Boolean,
	String,
	Number,
	Null,
	Error
};
