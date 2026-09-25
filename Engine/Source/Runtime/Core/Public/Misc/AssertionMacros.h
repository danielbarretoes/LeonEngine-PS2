#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformMisc.h"

/** Assertion reporting behind check / verify / ensure (UE: FDebug). */
struct CORE_API FDebug
{
	/** Reports a failed check() / verify() through GLog and terminates the process. */
	[[noreturn]] static void CheckFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line);
	[[noreturn]] static void CheckFailedf(
		const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format, ...) LEON_PRINTF_FORMAT(4, 5);

	/** Reports a failed ensure() through GLog; execution continues. */
	static void EnsureFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line);
	static void EnsureFailedf(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format, ...)
		LEON_PRINTF_FORMAT(4, 5);

	/** Number of ensure() failures reported so far (automation tests read it). */
	static int32 GetNumEnsureFailures();
};

/** Breaks into an attached debugger; does nothing otherwise (UE: UE_DEBUG_BREAK). */
#define UE_DEBUG_BREAK() ((void)(FPlatformMisc::IsDebuggerPresent() && ([]() { PLATFORM_BREAK(); }(), true)))

// ---------------------------------------------------------------------------------------------------------------------
// check / verify: fatal. check() compiles out without DO_CHECK; verify() always evaluates its expression.
// ---------------------------------------------------------------------------------------------------------------------

#if DO_CHECK
	#define check(InExpression)                                                                                        \
		do                                                                                                             \
		{                                                                                                              \
			if (UNLIKELY(!(InExpression)))                                                                             \
			{                                                                                                          \
				UE_DEBUG_BREAK();                                                                                      \
				FDebug::CheckFailed(#InExpression, __FILE__, __LINE__);                                                \
			}                                                                                                          \
		} while (0)

	#define checkf(InExpression, InFormat, ...)                                                                        \
		do                                                                                                             \
		{                                                                                                              \
			if (UNLIKELY(!(InExpression)))                                                                             \
			{                                                                                                          \
				UE_DEBUG_BREAK();                                                                                      \
				FDebug::CheckFailedf(#InExpression, __FILE__, __LINE__, InFormat, ##__VA_ARGS__);                      \
			}                                                                                                          \
		} while (0)

	#define verify(InExpression) check(InExpression)
	#define verifyf(InExpression, InFormat, ...) checkf(InExpression, InFormat, ##__VA_ARGS__)

	#define checkCode(Code)                                                                                            \
		do                                                                                                             \
		{                                                                                                              \
			Code;                                                                                                      \
		} while (0)

	#define checkNoEntry() FDebug::CheckFailed("Enclosing block should never be called", __FILE__, __LINE__)

	#define checkNoReentry()                                                                                           \
		do                                                                                                             \
		{                                                                                                              \
			static bool bBeenHere = false;                                                                             \
			if (bBeenHere)                                                                                             \
			{                                                                                                          \
				FDebug::CheckFailed("Enclosing block was called more than once", __FILE__, __LINE__);                  \
			}                                                                                                          \
			bBeenHere = true;                                                                                          \
		} while (0)

	#define unimplemented() FDebug::CheckFailed("Unimplemented function called", __FILE__, __LINE__)
#else
	// The expression is not evaluated (sizeof keeps variables used only in checks referenced).
	#define check(InExpression)                                                                                        \
		do                                                                                                             \
		{                                                                                                              \
			(void)sizeof(!!(InExpression));                                                                            \
		} while (0)
	#define checkf(InExpression, InFormat, ...)                                                                        \
		do                                                                                                             \
		{                                                                                                              \
			(void)sizeof(!!(InExpression));                                                                            \
		} while (0)
	#define verify(InExpression)                                                                                       \
		do                                                                                                             \
		{                                                                                                              \
			(void)(InExpression);                                                                                      \
		} while (0)
	#define verifyf(InExpression, InFormat, ...)                                                                       \
		do                                                                                                             \
		{                                                                                                              \
			(void)(InExpression);                                                                                      \
		} while (0)
	#define checkCode(Code)                                                                                            \
		do                                                                                                             \
		{                                                                                                              \
		} while (0)
	#define checkNoEntry()                                                                                             \
		do                                                                                                             \
		{                                                                                                              \
		} while (0)
	#define checkNoReentry()                                                                                           \
		do                                                                                                             \
		{                                                                                                              \
		} while (0)
	#define unimplemented()                                                                                            \
		do                                                                                                             \
		{                                                                                                              \
		} while (0)
#endif

#if DO_GUARD_SLOW
	#define checkSlow(InExpression) check(InExpression)
	#define checkfSlow(InExpression, InFormat, ...) checkf(InExpression, InFormat, ##__VA_ARGS__)
	#define verifySlow(InExpression) check(InExpression)
#else
	#define checkSlow(InExpression)                                                                                    \
		do                                                                                                             \
		{                                                                                                              \
			(void)sizeof(!!(InExpression));                                                                            \
		} while (0)
	#define checkfSlow(InExpression, InFormat, ...)                                                                    \
		do                                                                                                             \
		{                                                                                                              \
			(void)sizeof(!!(InExpression));                                                                            \
		} while (0)
	#define verifySlow(InExpression)                                                                                   \
		do                                                                                                             \
		{                                                                                                              \
			(void)(InExpression);                                                                                      \
		} while (0)
#endif

// ---------------------------------------------------------------------------------------------------------------------
// ensure: reports once per call site (ensureAlways: every time) and evaluates to the expression's truth, so it can
// guard a branch: if (!ensure(Ptr)) { return; }
// ---------------------------------------------------------------------------------------------------------------------

#if DO_ENSURE
	#define UE_ENSURE_IMPL(bAlways, InExpression, FailCall)                                                            \
		(LIKELY(!!(InExpression)) || ([&]() -> bool {                                                                  \
			static bool bExecuted = false;                                                                             \
			if ((bAlways) || !bExecuted)                                                                               \
			{                                                                                                          \
				bExecuted = true;                                                                                      \
				FailCall;                                                                                              \
				UE_DEBUG_BREAK();                                                                                      \
			}                                                                                                          \
			return false;                                                                                              \
		}()))

	#define ensure(InExpression)                                                                                       \
		UE_ENSURE_IMPL(false, InExpression, FDebug::EnsureFailed(#InExpression, __FILE__, __LINE__))
	#define ensureMsgf(InExpression, InFormat, ...)                                                                    \
		UE_ENSURE_IMPL(                                                                                                \
			false, InExpression, FDebug::EnsureFailedf(#InExpression, __FILE__, __LINE__, InFormat, ##__VA_ARGS__))
	#define ensureAlways(InExpression)                                                                                 \
		UE_ENSURE_IMPL(true, InExpression, FDebug::EnsureFailed(#InExpression, __FILE__, __LINE__))
	#define ensureAlwaysMsgf(InExpression, InFormat, ...)                                                              \
		UE_ENSURE_IMPL(                                                                                                \
			true, InExpression, FDebug::EnsureFailedf(#InExpression, __FILE__, __LINE__, InFormat, ##__VA_ARGS__))
#else
	#define ensure(InExpression) (!!(InExpression))
	#define ensureMsgf(InExpression, InFormat, ...) (!!(InExpression))
	#define ensureAlways(InExpression) (!!(InExpression))
	#define ensureAlwaysMsgf(InExpression, InFormat, ...) (!!(InExpression))
#endif
