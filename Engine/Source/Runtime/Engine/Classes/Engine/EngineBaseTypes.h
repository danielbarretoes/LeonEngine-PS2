#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EngineBaseTypes.generated.h"

/** How a URL is read against a base URL (UE: ETravelType). */
enum ETravelType
{
	/** The URL is complete: nothing comes from the base. */
	TRAVEL_Absolute,
	/** The base's options carry over; the map and the portal are the URL's. */
	TRAVEL_Partial,
	/** The base's map, portal and options carry over unless the URL gives them. */
	TRAVEL_Relative,
	TRAVEL_MAX,
};

/** What UEngine::Browse did (UE: EBrowseReturnVal). */
namespace EBrowseReturnVal
{
	enum Type
	{
		/** The map was loaded. */
		Success,
		/** The URL was invalid or the map could not be loaded; the error says why. */
		Failure,
		/** A network connection is pending (unused: Leon has no networking). */
		Pending,
	};
} // namespace EBrowseReturnVal

/**
 * A travel URL (UE: FURL): the map to open, its options and the portal to enter through, written
 * `Map?Option1=Value?Option2#Portal`. `?game=<GameMode>` picks the game mode (plan decision D18) and the portal is the
 * PlayerStartTag of the start to spawn at.
 *
 * The map is a long package name (`/Engine/Maps/Entry`, `/Game/Maps/X`) or the path of a `.lmap` file; a path that
 * starts with a drive letter, or with `/` without being a long package name, is a plain file name (UE's rule). Leon has
 * no networking, so the protocol, host and port of UE's URL are left out.
 */
USTRUCT()
struct ENGINE_API FURL
{
	GENERATED_BODY()

	/** The map (UE: Map); the project's GameDefaultMap when a URL gives none. */
	UPROPERTY()
	FString Map;

	/** The options, each `Key=Value` or `Key` (UE: Op). */
	UPROPERTY()
	TArray<FString> Op;

	/** The portal to enter through (UE: Portal), empty by default. */
	UPROPERTY()
	FString Portal;

	/** 1 when the text parsed (UE: Valid). */
	UPROPERTY()
	int32 Valid = 1;

	/**
	 * A URL for a map (UE). Without a file name the map stays empty (UE puts the default map there; Leon keeps the
	 * struct's defaults free of config reads, and the parsing constructor fills the default map).
	 */
	explicit FURL(const TCHAR* Filename = nullptr);

	/**
	 * Parses TextURL; Base supplies what Type carries over, and a text without a map gets the base's map (relative) or
	 * the project's GameDefaultMap (UE).
	 */
	FURL(const FURL* Base, const TCHAR* TextURL, ETravelType Type);

	/** Whether an option is present, `Key` or `Key=...`, compared without case (UE: HasOption). */
	[[nodiscard]] bool HasOption(const TCHAR* Test) const;

	/**
	 * The text after Match in the first option starting with it (`Match` = "game=" returns the value), or Default
	 * (UE: GetOption).
	 */
	[[nodiscard]] const TCHAR* GetOption(const TCHAR* Match, const TCHAR* Default) const;

	/** Adds `Key=Value` or `Key`, replacing an option with the same key (UE: AddOption). */
	void AddOption(const TCHAR* Str);

	/** Removes the options with this key (UE: RemoveOption). */
	void RemoveOption(const TCHAR* Key);

	/** `Map?Op1?Op2#Portal` (UE: ToString; FullyQualified would add the protocol, which Leon does not have). */
	[[nodiscard]] FString ToString(bool FullyQualified = false) const;

	/** The map, options and portal are the same (maps and portals compared without case) (UE). */
	bool operator==(const FURL& Other) const;
	bool operator!=(const FURL& Other) const
	{
		return !(*this == Other);
	}
};

/** How the game window captures the mouse (UE: EMouseCaptureMode). */
UENUM()
enum class EMouseCaptureMode : uint8
{
	/** Never captured. */
	NoCapture,
	/** Captured from the start, until the game releases it. */
	CapturePermanently,
	/** Captured from the start, the first click included (UE's default). */
	CapturePermanently_IncludingInitialMouseDown,
	/** Captured while a mouse button is down. */
	CaptureDuringMouseDown,
	/** Captured while the right mouse button is down. */
	CaptureDuringRightMouseDown,
};

/** Input events (UE: EInputEvent, EngineBaseTypes.h). */
enum EInputEvent
{
	IE_Pressed = 0,
	IE_Released = 1,
	IE_Repeat = 2,
	IE_DoubleClick = 3,
	IE_Axis = 4,
	IE_MAX = 5,
};
