#pragma once

#include "CoreTypes.h"
#include "Logging/LogCategory.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"

#include <type_traits>

/** Lowest verbosity compiled in anywhere (UE: COMPILED_IN_MINIMUM_VERBOSITY). */
#ifndef COMPILED_IN_MINIMUM_VERBOSITY
	#define COMPILED_IN_MINIMUM_VERBOSITY VeryVerbose
#endif

/** Log entry points behind UE_LOG (UE: FMsg). */
struct CORE_API FMsg
{
	/** Formats and sends a message to GLog; Fatal flushes the log and terminates. */
	static void Logf(const ANSICHAR* File, int32 Line, const FName& Category, ELogVerbosity::Type Verbosity,
		const TCHAR* Fmt, ...) LEON_PRINTF_FORMAT(5, 6);

	/** Sends an already formatted message. */
	static void Log(
		const ANSICHAR* File, int32 Line, const FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Message);
};

/** Declares a log category defined elsewhere with DEFINE_LOG_CATEGORY (UE). */
#define DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity)                              \
	extern struct FLogCategory##CategoryName                                                                           \
		: public FLogCategory<ELogVerbosity::DefaultVerbosity, ELogVerbosity::CompileTimeVerbosity>                    \
	{                                                                                                                  \
		FORCEINLINE FLogCategory##CategoryName()                                                                       \
			: FLogCategory(TEXT(#CategoryName))                                                                        \
		{                                                                                                              \
		}                                                                                                              \
	} CategoryName;

/** Defines a category declared with DECLARE_LOG_CATEGORY_EXTERN (UE). */
#define DEFINE_LOG_CATEGORY(CategoryName) FLogCategory##CategoryName CategoryName;

/** Declares and defines a category local to one .cpp (UE). */
#define DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity)                               \
	static struct FLogCategory##CategoryName                                                                           \
		: public FLogCategory<ELogVerbosity::DefaultVerbosity, ELogVerbosity::CompileTimeVerbosity>                    \
	{                                                                                                                  \
		FORCEINLINE FLogCategory##CategoryName()                                                                       \
			: FLogCategory(TEXT(#CategoryName))                                                                        \
		{                                                                                                              \
		}                                                                                                              \
	} CategoryName;

/** True when the category / verbosity is compiled in and not suppressed (UE: UE_LOG_ACTIVE). */
#define UE_LOG_ACTIVE(CategoryName, Verbosity)                                                                         \
	(((ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) <= ELogVerbosity::COMPILED_IN_MINIMUM_VERBOSITY) &&    \
		((ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) <=                                                  \
			FLogCategory##CategoryName::CompileTimeVerbosity) &&                                                       \
		!CategoryName.IsSuppressed(ELogVerbosity::Verbosity))

#define UE_GET_LOG_VERBOSITY(CategoryName) CategoryName.GetVerbosity()
#define UE_SET_LOG_VERBOSITY(CategoryName, Verbosity) CategoryName.SetVerbosity(ELogVerbosity::Verbosity);

#if NO_LOGGING
	// Only Fatal messages survive (they still stop the program).
	#define UE_LOG(CategoryName, Verbosity, Format, ...)                                                               \
		do                                                                                                             \
		{                                                                                                              \
			if constexpr (ELogVerbosity::Verbosity == ELogVerbosity::Fatal)                                            \
			{                                                                                                          \
				FMsg::Logf(                                                                                            \
					__FILE__, __LINE__, CategoryName.GetCategoryName(), ELogVerbosity::Fatal, Format, ##__VA_ARGS__);  \
			}                                                                                                          \
		} while (0)
#else
	/** Logs a printf-style message: UE_LOG(LogTemp, Warning, TEXT("Value %d"), Value). */
	#define UE_LOG(CategoryName, Verbosity, Format, ...)                                                               \
		do                                                                                                             \
		{                                                                                                              \
			static_assert(std::is_const_v<std::remove_reference_t<decltype(Format)>> ||                                \
					std::is_pointer_v<std::remove_reference_t<decltype(Format)>>,                                      \
				"Formatting string must be a TCHAR array or pointer.");                                                \
			if constexpr (((ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) <=                                \
							  ELogVerbosity::COMPILED_IN_MINIMUM_VERBOSITY) &&                                         \
				((ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) <=                                          \
					FLogCategory##CategoryName::CompileTimeVerbosity))                                                 \
			{                                                                                                          \
				if (!CategoryName.IsSuppressed(ELogVerbosity::Verbosity))                                              \
				{                                                                                                      \
					FMsg::Logf(__FILE__, __LINE__, CategoryName.GetCategoryName(), ELogVerbosity::Verbosity, Format,   \
						##__VA_ARGS__);                                                                                \
				}                                                                                                      \
			}                                                                                                          \
		} while (0)
#endif

/** Logs only when Condition is true (UE: UE_CLOG). */
#define UE_CLOG(Condition, CategoryName, Verbosity, Format, ...)                                                       \
	do                                                                                                                 \
	{                                                                                                                  \
		if (Condition)                                                                                                 \
		{                                                                                                              \
			UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__);                                                    \
		}                                                                                                              \
	} while (0)

// Core's categories.
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogTemp, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogCore, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogInit, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogExit, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMemory, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogModuleManager, Log, All);
CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogOutputDevice, Log, All);
