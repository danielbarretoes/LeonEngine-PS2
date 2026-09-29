#pragma once

#include "CoreMinimal.h"

/** The PS2's television modes: the CRTC's interlaced NTSC and PAL, one field per vertical blank. */
enum class EGSVideoMode : uint8
{
	/** 640 x 448 visible, 59.94 fields a second. */
	Ntsc,
	/** 640 x 512 visible, 50 fields a second. */
	Pal,
};

/**
 * Frame pacing in fields (Leon; UE's counterpart is rhi.SyncInterval): the PS2 counts the vertical blanks with an
 * interrupt, and a frame is shown (the CRTC's frame buffer changes) at the start of a vertical blank, never in the
 * middle of a field. With a sync interval of N a frame stays on screen for at least N fields: 2 is 30 fps on NTSC and
 * 25 on PAL. A frame that is ready after its blank has begun waits for the next one, so every frame lasts a whole
 * number of fields. Plain integers, so the decision is the same, and tested, on every platform.
 */
struct GSCORE_API FGSFieldPacer
{
	/** The fields (vertical blanks) of a second in Mode. */
	[[nodiscard]] static float GetFieldsPerSecond(EGSVideoMode Mode);
	/** A field's length in Mode, in microseconds (rounded): 16 683 on NTSC, 20 000 on PAL. */
	[[nodiscard]] static uint32 GetFieldMicroseconds(EGSVideoMode Mode);
	/** The visible lines of Mode's interlaced frame: 448 on NTSC, 512 on PAL. */
	[[nodiscard]] static int32 GetVisibleLines(EGSVideoMode Mode);

	/**
	 * The field to show the next frame on, as a count of vertical blanks: SyncInterval (at least 1) fields after the
	 * last frame's, or, when that blank has already begun at CurrentField (the frame is late), the next one. The counts
	 * wrap around at 2^32.
	 */
	[[nodiscard]] static uint32 GetFlipField(uint32 LastFlipField, uint32 CurrentField, uint32 SyncInterval);

	/** Whether the blank of Field has begun once the count is CurrentField (wrap-around safe). */
	[[nodiscard]] static bool HasBegun(uint32 Field, uint32 CurrentField)
	{
		return int32(CurrentField - Field) >= 0;
	}
};
