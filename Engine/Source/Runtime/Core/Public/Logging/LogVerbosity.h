#pragma once

#include "CoreTypes.h"

/** Log verbosity levels (UE: ELogVerbosity). Lower values are more severe. */
namespace ELogVerbosity
{
	enum Type : uint8
	{
		/** Not used. */
		NoLogging = 0,

		/** Always printed; stops the program even with logging disabled. */
		Fatal,

		/** Printed in red; fails automation tests. */
		Error,

		/** Printed in yellow. */
		Warning,

		/** Printed to the console and the log. */
		Display,

		/** Printed to the log (and the console unless it filters). */
		Log,

		/** Detailed logging, usually off. */
		Verbose,

		/** Very detailed logging, usually off. */
		VeryVerbose,

		All = VeryVerbose,
		NumVerbosity,
		VerbosityMask = 0xf,
		SetColor = 0x40,
		BreakOnLog = 0x80
	};
} // namespace ELogVerbosity

static_assert(ELogVerbosity::NumVerbosity - 1 < ELogVerbosity::VerbosityMask, "Bad verbosity mask.");
static_assert(!(ELogVerbosity::VerbosityMask & ELogVerbosity::BreakOnLog), "Bad verbosity mask.");

/** "Fatal", "Error", ... (UE: ToString(ELogVerbosity::Type)). */
CORE_API const TCHAR* ToString(ELogVerbosity::Type Verbosity);

/** Parses a verbosity name ignoring case; ELogVerbosity::NoLogging when unknown. */
CORE_API ELogVerbosity::Type ParseLogVerbosityFromString(const TCHAR* VerbosityString);
