#pragma once

// The vertical blank interrupt of the PS2 RHI (private; Docs/PLANS/ps2-shipping.md N10).

#include "CoreTypes.h"

namespace Leon::PS2
{

	/**
	 * Counts the fields: an INTC_VBLANK_S handler adds one at the start of every vertical blank and signals a
	 * semaphore, so the EE thread sleeps in WaitSema until the blank it wants instead of polling the GS's CSR.
	 */
	struct FPS2VerticalBlank
	{
		/** Adds the handler and its semaphore and enables the interrupt (once; later calls do nothing). */
		[[nodiscard]] static bool Install();
		/** Disables the interrupt and removes the handler and the semaphore. */
		static void Remove();
		/** The vertical blanks that have begun since Install. */
		[[nodiscard]] static uint32 GetFieldCount();
		/**
		 * Sleeps until the blank of Field has begun (FGSFieldPacer::HasBegun), at once if it has; returns the count
		 * then.
		 */
		static uint32 WaitForField(uint32 Field);
	};

} // namespace Leon::PS2
