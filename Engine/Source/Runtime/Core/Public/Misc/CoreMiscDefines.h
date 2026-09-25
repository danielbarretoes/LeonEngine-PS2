#pragma once

// Engine-wide constants and tag types (UE: Misc/CoreMiscDefines.h).

/** Index returned by searches that found nothing. */
enum
{
	INDEX_NONE = -1
};

/** Constructor tag: initialise to defaults / zero (UE: EForceInit). */
enum EForceInit
{
	ForceInit,
	ForceInitToZero
};

/** Constructor tag: leave members uninitialised (UE: ENoInit). */
enum ENoInit
{
	NoInit
};

/** Constructor tag used by containers that are built in place. */
enum EInPlace
{
	InPlace
};

/** Deletes the copy and move operations of a type. */
#define UE_NONCOPYABLE(TypeName)                                                                                       \
	TypeName(TypeName&&) = delete;                                                                                     \
	TypeName(const TypeName&) = delete;                                                                                \
	TypeName& operator=(const TypeName&) = delete;                                                                     \
	TypeName& operator=(TypeName&&) = delete;
