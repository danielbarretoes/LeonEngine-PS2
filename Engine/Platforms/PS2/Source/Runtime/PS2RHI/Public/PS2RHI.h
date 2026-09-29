#pragma once

#include "CoreTypes.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSTypes.h"

/**
 * PlayStation 2 Graphics Synthesizer API (platform extension RHI). What is drawn is recorded in FGSCommandLists
 * (the Renderer's scene and canvas, FGSDebugDraw's text and rectangles) and appended to the frame with Submit;
 * WaitVSync sends the frame to the GIF as one packet (Docs/PLANS/ps2-gs-parity.md, P3).
 *
 * Frame flow:
 *   1. InitDisplay: VRAM, CRTC and the drawing environment (done by FPS2Window)
 *   2. ClearColor, then Submit the frame's lists (recorded against GetDrawEnvironment)
 *   3. WaitVSync: send the frame, wait for the vertical blank and show it (FPS2Window::SwapBuffers)
 */
class PS2RHI_API FPS2RHI
{
public:
	/**
	 * Width x Height, double buffered, with a Z24 buffer, in the console's television mode (NTSC, or PAL with the frame
	 * centred in its 512 lines; -PAL / -NTSC choose). ColorFormat is PSMCT16S (dithered, the engine's) or PSMCT32;
	 * ReservedVramBytes at the start of the VRAM are left to the caller. Installs the vertical blank interrupt handler
	 * that counts the fields.
	 */
	static bool InitDisplay(
		int Width, int Height, EGSPixelFormat ColorFormat = EGSPixelFormat::PSMCT16S, uint32 ReservedVramBytes = 0);
	/** Turns the display off and removes the vertical blank handler (FPS2Window::Destroy). */
	static void ShutdownDisplay();
	static void ClearColor(float R, float G, float B);
	/**
	 * Sends the frame, sleeps until the vertical blank and shows what was drawn; with a sync interval of N it shows it
	 * no sooner than N fields after the last one (FGSFieldPacer, on the count of the vertical blank interrupt).
	 */
	static void WaitVSync();
	/**
	 * The fields a frame is shown for at least (UE: rhi.SyncInterval): 1 is the display's rate (59.94 Hz NTSC, 50 Hz
	 * PAL), 2 a steady 30 fps (25 on PAL; a frame late for the second blank waits for the third).
	 */
	static void SetSyncInterval(int32 Interval);

	/**
	 * Appends a recorded list to the frame; the drawing environment is restored after it. The images the list holds in
	 * place (FGSCommandList::UploadImageInPlace: a cooked texture's levels) stay in place: the frame's DMA chain reads
	 * them where they are.
	 */
	static void Submit(const FGSCommandList& List);
	/**
	 * Before data that lists hold in place changes or goes (a resident texture released): the frame being recorded
	 * copies its in-place images, and the frame being sent is waited for until its DMA has read them.
	 */
	static void RetireInPlaceImages();
	/**
	 * The drawing environment of the frame being drawn, which a Submit list is recorded against (its PixelVertex takes
	 * pixels from the frame's top left).
	 */
	static FGSDrawEnvironment GetDrawEnvironment();
	/**
	 * Hands the caller the VRAM left after the display (and whatever was allocated before), for its textures: the
	 * first 64-word block and the number of blocks, page aligned. Once only; false when nothing is left.
	 */
	static bool AllocateTextureArena(uint32& OutFirstBlock, uint32& OutNumBlocks);
};
