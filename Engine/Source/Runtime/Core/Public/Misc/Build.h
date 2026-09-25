#pragma once

// Build configuration switches (UE: Misc/Build.h). LeonBuildTool defines LEON_BUILD_<CONFIGURATION>=1.

#include "HAL/Platform.h"

#ifndef LEON_BUILD_DEBUG
	#define LEON_BUILD_DEBUG 0
#endif
#ifndef LEON_BUILD_DEVELOPMENT
	#define LEON_BUILD_DEVELOPMENT 0
#endif
#ifndef LEON_BUILD_SHIPPING
	#define LEON_BUILD_SHIPPING 0
#endif

#define UE_BUILD_DEBUG LEON_BUILD_DEBUG
#define UE_BUILD_SHIPPING LEON_BUILD_SHIPPING
// Code built outside LeonBuildTool (host tools, IDE indexers) counts as Development.
#if !UE_BUILD_DEBUG && !UE_BUILD_SHIPPING
	#define UE_BUILD_DEVELOPMENT 1
#else
	#define UE_BUILD_DEVELOPMENT LEON_BUILD_DEVELOPMENT
#endif

#if UE_BUILD_DEBUG + UE_BUILD_DEVELOPMENT + UE_BUILD_SHIPPING != 1
	#error "Exactly one of UE_BUILD_DEBUG, UE_BUILD_DEVELOPMENT and UE_BUILD_SHIPPING must be set"
#endif

/** check()/verify() failures stop the program (UE: DO_CHECK). */
#ifndef DO_CHECK
	#define DO_CHECK !UE_BUILD_SHIPPING
#endif

/** checkSlow() and friends (UE: DO_GUARD_SLOW). */
#ifndef DO_GUARD_SLOW
	#define DO_GUARD_SLOW UE_BUILD_DEBUG
#endif

/** ensure() reports failures (UE: DO_ENSURE). */
#ifndef DO_ENSURE
	#define DO_ENSURE !UE_BUILD_SHIPPING
#endif

/** UE_LOG compiles out (UE: NO_LOGGING, USE_LOGGING_IN_SHIPPING=0). */
#ifndef NO_LOGGING
	#define NO_LOGGING UE_BUILD_SHIPPING
#endif

/**
 * Editor-only reflected data (#if WITH_EDITORONLY_DATA members and UPROPERTYs): desktop builds outside Shipping keep
 * it, the PS2 and Shipping builds strip it (UE: WITH_EDITORONLY_DATA, set per target by UBT; plan decision D14).
 */
#ifndef WITH_EDITORONLY_DATA
	#if PLATFORM_DESKTOP && !UE_BUILD_SHIPPING
		#define WITH_EDITORONLY_DATA 1
	#else
		#define WITH_EDITORONLY_DATA 0
	#endif
#endif

/** Automation tests are compiled into test executables only (LeonBuildTool COLLECT_AUTOMATION_TESTS). */
#ifndef WITH_DEV_AUTOMATION_TESTS
	#define WITH_DEV_AUTOMATION_TESTS 0
#endif
