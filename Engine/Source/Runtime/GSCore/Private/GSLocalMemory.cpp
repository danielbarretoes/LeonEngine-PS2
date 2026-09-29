#include "GSLocalMemory.h"

namespace
{

	// The block arrangement in a page of each storage format (GS User's Manual 8.3), [block row][block column], as the
	// manual's figures draw them.

	/** PSMCT32, PSMCT24 (and PSMT8H, PSMT4HL, PSMT4HH), 8.3.1: 4 x 8 blocks of 8 x 8 pixels. */
	constexpr uint8 BlockTable32[4][8] = {
		{0, 1, 4, 5, 16, 17, 20, 21},
		{2, 3, 6, 7, 18, 19, 22, 23},
		{8, 9, 12, 13, 24, 25, 28, 29},
		{10, 11, 14, 15, 26, 27, 30, 31},
	};
	/** PSMZ32, PSMZ24, 8.3.1. */
	constexpr uint8 BlockTableZ32[4][8] = {
		{24, 25, 28, 29, 8, 9, 12, 13},
		{26, 27, 30, 31, 10, 11, 14, 15},
		{16, 17, 20, 21, 0, 1, 4, 5},
		{18, 19, 22, 23, 2, 3, 6, 7},
	};
	/** PSMCT16, 8.3.2: 8 x 4 blocks of 16 x 8 pixels. */
	constexpr uint8 BlockTable16[8][4] = {
		{0, 2, 8, 10},
		{1, 3, 9, 11},
		{4, 6, 12, 14},
		{5, 7, 13, 15},
		{16, 18, 24, 26},
		{17, 19, 25, 27},
		{20, 22, 28, 30},
		{21, 23, 29, 31},
	};
	/** PSMCT16S, 8.3.2. */
	constexpr uint8 BlockTable16S[8][4] = {
		{0, 2, 16, 18},
		{1, 3, 17, 19},
		{8, 10, 24, 26},
		{9, 11, 25, 27},
		{4, 6, 20, 22},
		{5, 7, 21, 23},
		{12, 14, 28, 30},
		{13, 15, 29, 31},
	};
	/** PSMZ16, 8.3.2. */
	constexpr uint8 BlockTableZ16[8][4] = {
		{24, 26, 16, 18},
		{25, 27, 17, 19},
		{28, 30, 20, 22},
		{29, 31, 21, 23},
		{8, 10, 0, 2},
		{9, 11, 1, 3},
		{12, 14, 4, 6},
		{13, 15, 5, 7},
	};
	/** PSMZ16S, 8.3.2. */
	constexpr uint8 BlockTableZ16S[8][4] = {
		{24, 26, 8, 10},
		{25, 27, 9, 11},
		{16, 18, 0, 2},
		{17, 19, 1, 3},
		{28, 30, 12, 14},
		{29, 31, 13, 15},
		{20, 22, 4, 6},
		{21, 23, 5, 7},
	};
	/** PSMT8, 8.3.3: 4 x 8 blocks of 16 x 16 pixels. */
	constexpr uint8 BlockTable8[4][8] = {
		{0, 1, 4, 5, 16, 17, 20, 21},
		{2, 3, 6, 7, 18, 19, 22, 23},
		{8, 9, 12, 13, 24, 25, 28, 29},
		{10, 11, 14, 15, 26, 27, 30, 31},
	};
	/** PSMT4, 8.3.4: 8 x 4 blocks of 32 x 16 pixels. */
	constexpr uint8 BlockTable4[8][4] = {
		{0, 2, 8, 10},
		{1, 3, 9, 11},
		{4, 6, 12, 14},
		{5, 7, 13, 15},
		{16, 18, 24, 26},
		{17, 19, 25, 27},
		{20, 22, 28, 30},
		{21, 23, 29, 31},
	};

	/**
	 * The word (0..15) of each pixel of a 32-bit column, 8 x 2 pixels (8.3.1). A 16-bit column (8.3.2) is two of them
	 * side by side: its left 8 pixels in bits 0-15 of these words, its right 8 in bits 16-31.
	 */
	constexpr uint8 ColumnWords32[2][8] = {
		{0, 1, 4, 5, 8, 9, 12, 13},
		{2, 3, 6, 7, 10, 11, 14, 15},
	};
	/**
	 * The word of each pixel of an 8-pixel group of a PSMT8 or PSMT4 column (4 rows), by the column's parity: columns
	 * 0 and 2, then 1 and 3 (8.3.3, 8.3.4). PSMT8's column is two groups across (bits 0-7 and 16-23 of the words in
	 * rows 0-1, 8-15 and 24-31 in rows 2-3); PSMT4's is four (bits 0-3, 8-11, 16-19, 24-27, then 4-7 ... 28-31).
	 */
	constexpr uint8 ColumnWords8[2][4][8] = {
		{
			{0, 1, 4, 5, 8, 9, 12, 13},
			{2, 3, 6, 7, 10, 11, 14, 15},
			{8, 9, 12, 13, 0, 1, 4, 5},
			{10, 11, 14, 15, 2, 3, 6, 7},
		},
		{
			{8, 9, 12, 13, 0, 1, 4, 5},
			{10, 11, 14, 15, 2, 3, 6, 7},
			{0, 1, 4, 5, 8, 9, 12, 13},
			{2, 3, 6, 7, 10, 11, 14, 15},
		},
	};

	/** How a format's pixels sit in a column's words. */
	enum class EColumnLayout : uint8
	{
		/** A pixel a word, from bit Shift. */
		Word,
		/** Two pixels a word. */
		HalfWord,
		/** PSMT8's column. */
		Byte,
		/** PSMT4's column. */
		Nibble,
	};

	/** A storage format's page and block geometry. */
	struct FFormatLayout
	{
		uint32 PageWidth;
		uint32 PageHeight;
		uint32 BlockWidth;
		uint32 BlockHeight;
		/** The block table, BlockTableColumns wide. */
		const uint8* BlockTable;
		uint32 BlockTableColumns;
		EColumnLayout Column;
		/** The first bit of a Word layout's pixel (PSMT8H: 24, PSMT4HL: 24, PSMT4HH: 28). */
		uint32 Shift;
	};

	[[nodiscard]] FFormatLayout GetLayout(EGSPixelFormat Format)
	{
		switch (Format)
		{
			case EGSPixelFormat::PSMZ32:
			case EGSPixelFormat::PSMZ24:
				return {64, 32, 8, 8, &BlockTableZ32[0][0], 8, EColumnLayout::Word, 0};
			case EGSPixelFormat::PSMT8H:
			case EGSPixelFormat::PSMT4HL:
				return {64, 32, 8, 8, &BlockTable32[0][0], 8, EColumnLayout::Word, 24};
			case EGSPixelFormat::PSMT4HH:
				return {64, 32, 8, 8, &BlockTable32[0][0], 8, EColumnLayout::Word, 28};
			case EGSPixelFormat::PSMCT16:
				return {64, 64, 16, 8, &BlockTable16[0][0], 4, EColumnLayout::HalfWord, 0};
			case EGSPixelFormat::PSMCT16S:
				return {64, 64, 16, 8, &BlockTable16S[0][0], 4, EColumnLayout::HalfWord, 0};
			case EGSPixelFormat::PSMZ16:
				return {64, 64, 16, 8, &BlockTableZ16[0][0], 4, EColumnLayout::HalfWord, 0};
			case EGSPixelFormat::PSMZ16S:
				return {64, 64, 16, 8, &BlockTableZ16S[0][0], 4, EColumnLayout::HalfWord, 0};
			case EGSPixelFormat::PSMT8:
				return {128, 64, 16, 16, &BlockTable8[0][0], 8, EColumnLayout::Byte, 0};
			case EGSPixelFormat::PSMT4:
				return {128, 128, 32, 16, &BlockTable4[0][0], 4, EColumnLayout::Nibble, 0};
			case EGSPixelFormat::PSMCT32:
			case EGSPixelFormat::PSMCT24:
				break;
		}
		return {64, 32, 8, 8, &BlockTable32[0][0], 8, EColumnLayout::Word, 0};
	}

	/** Pixel coordinates wrap at 2048 (the transfer's and the texture's 11 bits). */
	constexpr uint32 CoordinateMask = 2047;

	/** The block of the page (0, 1, ...) holding (X, Y), relative to the buffer's base pointer. */
	[[nodiscard]] uint32 GetRelativeBlock(const FFormatLayout& Layout, uint32 BufferWidth, uint32 X, uint32 Y)
	{
		const uint32 PagesPerRow = (BufferWidth * 64) / Layout.PageWidth;
		const uint32 Page = ((Y / Layout.PageHeight) * PagesPerRow) + (X / Layout.PageWidth);
		const uint32 Row = (Y % Layout.PageHeight) / Layout.BlockHeight;
		const uint32 Column = (X % Layout.PageWidth) / Layout.BlockWidth;
		return (Page * FGSLocalMemory::BlocksPerPage) + Layout.BlockTable[(Row * Layout.BlockTableColumns) + Column];
	}

} // namespace

FGSLocalMemory::FGSLocalMemory()
{
	Words.SetNumZeroed(SizeInBytes / 4);
}

uint32 FGSLocalMemory::StorageBits(EGSPixelFormat Format)
{
	const uint32 Bits = GSBitsPerPixel(Format);
	return Bits == 24 ? 32 : Bits;
}

void FGSLocalMemory::GetPageSize(EGSPixelFormat Format, uint32& OutWidth, uint32& OutHeight)
{
	const FFormatLayout Layout = GetLayout(Format);
	OutWidth = Layout.PageWidth;
	OutHeight = Layout.PageHeight;
}

void FGSLocalMemory::GetBlockSize(EGSPixelFormat Format, uint32& OutWidth, uint32& OutHeight)
{
	const FFormatLayout Layout = GetLayout(Format);
	OutWidth = Layout.BlockWidth;
	OutHeight = Layout.BlockHeight;
}

uint32 FGSLocalMemory::GetBlockInPage(EGSPixelFormat Format, uint32 X, uint32 Y)
{
	const FFormatLayout Layout = GetLayout(Format);
	const uint32 Row = (Y % Layout.PageHeight) / Layout.BlockHeight;
	const uint32 Column = (X % Layout.PageWidth) / Layout.BlockWidth;
	return Layout.BlockTable[(Row * Layout.BlockTableColumns) + Column];
}

uint32 FGSLocalMemory::GetBlockAddress(
	uint32 BasePointer, uint32 BufferWidth, EGSPixelFormat Format, uint32 X, uint32 Y)
{
	const FFormatLayout Layout = GetLayout(Format);
	const uint32 Block = GetRelativeBlock(Layout, BufferWidth, X & CoordinateMask, Y & CoordinateMask);
	return (BasePointer + Block) % NumBlocks;
}

uint32 FGSLocalMemory::GetBitAddress(uint32 BasePointer, uint32 BufferWidth, EGSPixelFormat Format, uint32 X, uint32 Y)
{
	const FFormatLayout Layout = GetLayout(Format);
	X &= CoordinateMask;
	Y &= CoordinateMask;
	const uint32 Block = (BasePointer + GetRelativeBlock(Layout, BufferWidth, X, Y)) % NumBlocks;
	// A block is 4 columns, each a quarter of its height; Row is the pixel's row in its column.
	const uint32 ColumnHeight = Layout.BlockHeight / 4;
	const uint32 Column = (Y % Layout.BlockHeight) / ColumnHeight;
	const uint32 Row = Y % ColumnHeight;
	uint32 Word = 0;
	uint32 Shift = 0;
	switch (Layout.Column)
	{
		case EColumnLayout::Word:
			Word = ColumnWords32[Row][X % 8];
			Shift = Layout.Shift;
			break;
		case EColumnLayout::HalfWord:
			Word = ColumnWords32[Row][X % 8];
			Shift = 16 * ((X % 16) / 8);
			break;
		case EColumnLayout::Byte:
			Word = ColumnWords8[Column % 2][Row][X % 8];
			Shift = (16 * ((X % 16) / 8)) + (8 * (Row / 2));
			break;
		case EColumnLayout::Nibble:
			Word = ColumnWords8[Column % 2][Row][X % 8];
			Shift = (8 * ((X % 32) / 8)) + (4 * (Row / 2));
			break;
	}
	const uint32 WordAddress = (Block * WordsPerBlock) + (Column * (BytesPerColumn / 4)) + Word;
	return (WordAddress * 32) + Shift;
}

uint32 FGSLocalMemory::GetBlockSpan(uint32 BufferWidth, EGSPixelFormat Format, uint32 Width, uint32 Height)
{
	if (Width == 0 || Height == 0)
	{
		return 0;
	}
	const FFormatLayout Layout = GetLayout(Format);
	uint32 LastBlock = 0;
	for (uint32 Y = 0; Y < Height; Y += Layout.BlockHeight)
	{
		for (uint32 X = 0; X < Width; X += Layout.BlockWidth)
		{
			LastBlock = FMath::Max(LastBlock, GetRelativeBlock(Layout, BufferWidth, X, Y));
		}
	}
	return LastBlock + 1;
}

uint32 FGSLocalMemory::ReadPixel(
	uint32 BasePointer, uint32 BufferWidth, EGSPixelFormat Format, uint32 X, uint32 Y) const
{
	const uint32 Bit = GetBitAddress(BasePointer, BufferWidth, Format, X, Y);
	const uint32 Bits = StorageBits(Format);
	const uint32 Mask = Bits == 32 ? 0xffffffffu : ((1u << Bits) - 1);
	return (Words[int32(Bit / 32)] >> (Bit % 32)) & Mask;
}

void FGSLocalMemory::WritePixel(
	uint32 BasePointer, uint32 BufferWidth, EGSPixelFormat Format, uint32 X, uint32 Y, uint32 Value)
{
	const uint32 Bit = GetBitAddress(BasePointer, BufferWidth, Format, X, Y);
	const uint32 Bits = StorageBits(Format);
	const uint32 Mask = (Bits == 32 ? 0xffffffffu : ((1u << Bits) - 1)) << (Bit % 32);
	uint32& Word = Words[int32(Bit / 32)];
	Word = (Word & ~Mask) | ((Value << (Bit % 32)) & Mask);
}

void FGSLocalMemory::Transfer(
	const FGSBitBltBuf& BitBltBuf, const FGSTrxPos& TrxPos, const FGSTrxReg& TrxReg, TArrayView<const uint8> Data)
{
	const uint32 Bits = GSBitsPerPixel(BitBltBuf.DPSM);
	const uint32 Base = BitBltBuf.DBP;
	const uint32 Width = BitBltBuf.DBW;
	uint64 BitCursor = 0;
	for (uint32 Y = 0; Y < TrxReg.RRH; ++Y)
	{
		for (uint32 X = 0; X < TrxReg.RRW; ++X)
		{
			// Little endian, the first 4-bit pixel in the low nibble (manual 4.3).
			uint32 Value = 0;
			for (uint32 Bit = 0; Bit < Bits; ++Bit)
			{
				const uint64 Source = BitCursor + Bit;
				Value |= uint32((Data[int32(Source / 8)] >> (Source % 8)) & 1) << Bit;
			}
			BitCursor += Bits;
			const uint32 PixelX = (uint32(TrxPos.DSAX) + X) & CoordinateMask;
			const uint32 PixelY = (uint32(TrxPos.DSAY) + Y) & CoordinateMask;
			if (Bits == 24)
			{
				// A 24-bit pixel keeps its word's high byte.
				Value |= ReadPixel(Base, Width, BitBltBuf.DPSM, PixelX, PixelY) & 0xff000000u;
			}
			WritePixel(Base, Width, BitBltBuf.DPSM, PixelX, PixelY, Value);
		}
	}
}
