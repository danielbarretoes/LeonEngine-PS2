#pragma once

#include "CoreMinimal.h"
#include "GSTypes.h"

/**
 * The GS's 4 MB local memory with a linear layout inside each buffer (Leon's model). The GS arranges a buffer's
 * pixels in pages and blocks (manual chapter 8); the model keeps them row by row from the buffer's base, which reads
 * back what was written as long as a buffer is always accessed with the same base, width and format, and buffers are
 * placed on page boundaries (then a buffer's linear span stays inside the pages the GS gives it). Addresses wrap at
 * 4 MB like the GS's.
 */
class GSREFERENCE_API FGSLocalMemory
{
public:
	static constexpr uint32 SizeInBytes = 4 * 1024 * 1024;

	FGSLocalMemory();

	/**
	 * A pixel's bits in a buffer that starts at BaseWords (32-bit words) and is WidthPixels wide: 32 bits for the
	 * 32-bit and 24-bit formats (a 24-bit pixel keeps its unused high byte), 16, 8 or 4 for the others.
	 */
	[[nodiscard]] uint32 ReadPixel(
		uint32 BaseWords, uint32 WidthPixels, EGSPixelFormat Format, uint32 X, uint32 Y) const;
	void WritePixel(uint32 BaseWords, uint32 WidthPixels, EGSPixelFormat Format, uint32 X, uint32 Y, uint32 Value);

	/** The bits a pixel of Format takes in memory (a 24-bit pixel takes 32). */
	[[nodiscard]] static uint32 StorageBits(EGSPixelFormat Format);

private:
	/** The pixel's first bit in memory, wrapped at 4 MB. */
	[[nodiscard]] static uint64 BitAddress(
		uint32 BaseWords, uint32 WidthPixels, EGSPixelFormat Format, uint32 X, uint32 Y);

	TArray<uint8> Bytes;
};
