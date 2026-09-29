#include "PS2VerticalBlank.h"

#include "GSFieldPacer.h"

#include <kernel.h>

namespace Leon::PS2
{

	namespace
	{

		/** Written by the handler, read by the EE thread. */
		volatile uint32 GFieldCount = 0;
		int32 GVBlankSema = -1;
		int32 GVBlankHandler = -1;

		/** INTC_VBLANK_S: a field has ended. Runs with interrupts off, so it only counts and signals. */
		int VBlankStartHandler(int Cause)
		{
			(void)Cause;
			GFieldCount = GFieldCount + 1;
			// The semaphore holds at most one signal: a thread that was not waiting finds a stale one, which
			// WaitForField drops before it sleeps.
			iSignalSema(GVBlankSema);
			ExitHandler();
			return 0;
		}

	} // namespace

	bool FPS2VerticalBlank::Install()
	{
		if (GVBlankHandler >= 0)
		{
			return true;
		}
		ee_sema_t Sema = {};
		Sema.init_count = 0;
		Sema.max_count = 1;
		GVBlankSema = CreateSema(&Sema);
		if (GVBlankSema < 0)
		{
			return false;
		}
		DIntr();
		GVBlankHandler = AddIntcHandler(INTC_VBLANK_S, &VBlankStartHandler, -1);
		if (GVBlankHandler >= 0)
		{
			EnableIntc(INTC_VBLANK_S);
		}
		EIntr();
		if (GVBlankHandler < 0)
		{
			DeleteSema(GVBlankSema);
			GVBlankSema = -1;
			return false;
		}
		return true;
	}

	void FPS2VerticalBlank::Remove()
	{
		if (GVBlankHandler < 0)
		{
			return;
		}
		DIntr();
		DisableIntc(INTC_VBLANK_S);
		RemoveIntcHandler(INTC_VBLANK_S, GVBlankHandler);
		EIntr();
		DeleteSema(GVBlankSema);
		GVBlankHandler = -1;
		GVBlankSema = -1;
	}

	uint32 FPS2VerticalBlank::GetFieldCount()
	{
		return GFieldCount;
	}

	uint32 FPS2VerticalBlank::WaitForField(uint32 Field)
	{
		// Drop a signal left from a blank nobody waited for, then sleep blank after blank: the count is read after
		// the drop, so a blank that begins in between leaves its signal and WaitSema returns at once.
		PollSema(GVBlankSema);
		uint32 Count = GFieldCount;
		while (!FGSFieldPacer::HasBegun(Field, Count))
		{
			WaitSema(GVBlankSema);
			Count = GFieldCount;
		}
		return Count;
	}

} // namespace Leon::PS2
