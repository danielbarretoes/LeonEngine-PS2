#include "CoreMinimal.h"
#include "GSLocalMemory.h"
#include "GSTexelDecoder.h"
#include "GSTextureLayout.h"
#include "GSTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The GS local memory's layout (GS User's Manual chapter 8). The expected values are typed from the manual's figures
// (block arrangement in a page, 8.3; pixel order in a column, 8.3; occupied blocks, 8.4; pointing within a page, 8.5),
// not computed with FGSLocalMemory's own tables.

namespace
{

	/** A page's block table as the manual draws it: Rows x Columns block numbers, row by row. */
	struct FBlockTable
	{
		EGSPixelFormat Format;
		uint32 Rows;
		uint32 Columns;
		uint8 Blocks[32];
	};

	/** The formats' tables of 8.3.1 to 8.3.4. */
	const FBlockTable ManualBlockTables[] = {
		{EGSPixelFormat::PSMCT32, 4, 8,
			{0, 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26,
				27, 30, 31}},
		{EGSPixelFormat::PSMCT24, 4, 8,
			{0, 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26,
				27, 30, 31}},
		{EGSPixelFormat::PSMT8H, 4, 8,
			{0, 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26,
				27, 30, 31}},
		{EGSPixelFormat::PSMT4HL, 4, 8,
			{0, 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26,
				27, 30, 31}},
		{EGSPixelFormat::PSMT4HH, 4, 8,
			{0, 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26,
				27, 30, 31}},
		{EGSPixelFormat::PSMZ32, 4, 8,
			{24, 25, 28, 29, 8, 9, 12, 13, 26, 27, 30, 31, 10, 11, 14, 15, 16, 17, 20, 21, 0, 1, 4, 5, 18, 19, 22, 23,
				2, 3, 6, 7}},
		{EGSPixelFormat::PSMZ24, 4, 8,
			{24, 25, 28, 29, 8, 9, 12, 13, 26, 27, 30, 31, 10, 11, 14, 15, 16, 17, 20, 21, 0, 1, 4, 5, 18, 19, 22, 23,
				2, 3, 6, 7}},
		{EGSPixelFormat::PSMCT16, 8, 4,
			{0, 2, 8, 10, 1, 3, 9, 11, 4, 6, 12, 14, 5, 7, 13, 15, 16, 18, 24, 26, 17, 19, 25, 27, 20, 22, 28, 30, 21,
				23, 29, 31}},
		{EGSPixelFormat::PSMCT16S, 8, 4,
			{0, 2, 16, 18, 1, 3, 17, 19, 8, 10, 24, 26, 9, 11, 25, 27, 4, 6, 20, 22, 5, 7, 21, 23, 12, 14, 28, 30, 13,
				15, 29, 31}},
		{EGSPixelFormat::PSMZ16, 8, 4,
			{24, 26, 16, 18, 25, 27, 17, 19, 28, 30, 20, 22, 29, 31, 21, 23, 8, 10, 0, 2, 9, 11, 1, 3, 12, 14, 4, 6, 13,
				15, 5, 7}},
		{EGSPixelFormat::PSMZ16S, 8, 4,
			{24, 26, 8, 10, 25, 27, 9, 11, 16, 18, 0, 2, 17, 19, 1, 3, 28, 30, 12, 14, 29, 31, 13, 15, 20, 22, 4, 6, 21,
				23, 5, 7}},
		{EGSPixelFormat::PSMT8, 4, 8,
			{0, 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26,
				27, 30, 31}},
		{EGSPixelFormat::PSMT4, 8, 4,
			{0, 2, 8, 10, 1, 3, 9, 11, 4, 6, 12, 14, 5, 7, 13, 15, 16, 18, 24, 26, 17, 19, 25, 27, 20, 22, 28, 30, 21,
				23, 29, 31}},
	};

	/** A pixel's word and first bit in memory. */
	struct FPixelVector
	{
		const TCHAR* Name;
		EGSPixelFormat Format;
		uint32 BasePointer;
		uint32 BufferWidth;
		uint32 X;
		uint32 Y;
		uint32 Word;
		uint32 Shift;
	};

	/**
	 * Hand-checked vectors: the block from the tables above (64 words each, a page 2048), the column (16 words) from
	 * the block's quarter, the word and bits from the column figures.
	 */
	const FPixelVector ManualPixelVectors[] = {
		// PSMCT32 column: 0 1 4 5 8 9 12 13 / 2 3 6 7 10 11 14 15.
		{TEXT("CT32 (0, 0)"), EGSPixelFormat::PSMCT32, 0, 1, 0, 0, 0, 0},
		{TEXT("CT32 (1, 0)"), EGSPixelFormat::PSMCT32, 0, 1, 1, 0, 1, 0},
		{TEXT("CT32 (2, 0)"), EGSPixelFormat::PSMCT32, 0, 1, 2, 0, 4, 0},
		{TEXT("CT32 (0, 1)"), EGSPixelFormat::PSMCT32, 0, 1, 0, 1, 2, 0},
		{TEXT("CT32 (7, 1)"), EGSPixelFormat::PSMCT32, 0, 1, 7, 1, 15, 0},
		{TEXT("CT32 column 1"), EGSPixelFormat::PSMCT32, 0, 1, 0, 2, 16, 0},
		{TEXT("CT32 block 1"), EGSPixelFormat::PSMCT32, 0, 1, 8, 0, 64, 0},
		{TEXT("CT32 block 2"), EGSPixelFormat::PSMCT32, 0, 1, 0, 8, 128, 0},
		{TEXT("CT32 last word of the page"), EGSPixelFormat::PSMCT32, 0, 1, 63, 31, 2047, 0},
		{TEXT("CT32 the page below"), EGSPixelFormat::PSMCT32, 0, 1, 0, 32, 2048, 0},
		{TEXT("CT32 the page to the right"), EGSPixelFormat::PSMCT32, 0, 2, 64, 0, 2048, 0},
		{TEXT("CT32 two pages across: the next row"), EGSPixelFormat::PSMCT32, 0, 2, 0, 32, 4096, 0},
		{TEXT("CT32 at block 5"), EGSPixelFormat::PSMCT32, 5, 1, 0, 0, 320, 0},
		{TEXT("CT24 as CT32"), EGSPixelFormat::PSMCT24, 0, 1, 9, 3, 64 + 16 + 3, 0},
		{TEXT("Z32 (0, 0): block 24"), EGSPixelFormat::PSMZ32, 0, 1, 0, 0, 24 * 64, 0},
		{TEXT("Z32 (32, 16): block 0"), EGSPixelFormat::PSMZ32, 0, 1, 32, 16, 0, 0},
		{TEXT("Z24 (8, 0): block 25"), EGSPixelFormat::PSMZ24, 0, 1, 8, 0, 25 * 64, 0},
		// PSMT8H, PSMT4HL, PSMT4HH: PSMCT32's words, bits 24-31, 24-27, 28-31.
		{TEXT("T8H (1, 0)"), EGSPixelFormat::PSMT8H, 0, 1, 1, 0, 1, 24},
		{TEXT("T4HL block 2"), EGSPixelFormat::PSMT4HL, 0, 1, 0, 8, 128, 24},
		{TEXT("T4HH (2, 1)"), EGSPixelFormat::PSMT4HH, 0, 1, 2, 1, 6, 28},
		// PSMCT16: the 32-bit column's words, the left 8 pixels in bits 0-15, the right 8 in 16-31.
		{TEXT("CT16 (0, 0)"), EGSPixelFormat::PSMCT16, 0, 1, 0, 0, 0, 0},
		{TEXT("CT16 (8, 0): the high half"), EGSPixelFormat::PSMCT16, 0, 1, 8, 0, 0, 16},
		{TEXT("CT16 (1, 0)"), EGSPixelFormat::PSMCT16, 0, 1, 1, 0, 1, 0},
		{TEXT("CT16 (9, 1)"), EGSPixelFormat::PSMCT16, 0, 1, 9, 1, 3, 16},
		{TEXT("CT16 column 1"), EGSPixelFormat::PSMCT16, 0, 1, 0, 2, 16, 0},
		{TEXT("CT16 block 1 below"), EGSPixelFormat::PSMCT16, 0, 1, 0, 8, 64, 0},
		{TEXT("CT16 block 2 across"), EGSPixelFormat::PSMCT16, 0, 1, 16, 0, 128, 0},
		{TEXT("CT16 last word of the page"), EGSPixelFormat::PSMCT16, 0, 1, 63, 63, 2047, 16},
		{TEXT("CT16S (32, 0): block 16"), EGSPixelFormat::PSMCT16S, 0, 1, 32, 0, 16 * 64, 0},
		{TEXT("CT16S (0, 16): block 8"), EGSPixelFormat::PSMCT16S, 0, 1, 0, 16, 8 * 64, 0},
		{TEXT("CT16S (0, 32): block 4"), EGSPixelFormat::PSMCT16S, 0, 1, 0, 32, 4 * 64, 0},
		{TEXT("Z16 (0, 0): block 24"), EGSPixelFormat::PSMZ16, 0, 1, 0, 0, 24 * 64, 0},
		{TEXT("Z16 (32, 32): block 0"), EGSPixelFormat::PSMZ16, 0, 1, 32, 32, 0, 0},
		{TEXT("Z16S (32, 16): block 0"), EGSPixelFormat::PSMZ16S, 0, 1, 32, 16, 0, 0},
		{TEXT("Z16S (48, 32): block 14"), EGSPixelFormat::PSMZ16S, 0, 1, 48, 32, 14 * 64, 0},
		// PSMT8: columns 0 and 2 are 0 1 4 5 8 9 12 13 / 2 3 6 7 10 11 14 15 in bits 0-7 (16-23 on the right half),
		// then the same words shifted by 4 pixels in bits 8-15 (24-31); columns 1 and 3 swap the two halves.
		{TEXT("T8 (0, 0)"), EGSPixelFormat::PSMT8, 0, 2, 0, 0, 0, 0},
		{TEXT("T8 (1, 0)"), EGSPixelFormat::PSMT8, 0, 2, 1, 0, 1, 0},
		{TEXT("T8 (8, 0): bits 16-23"), EGSPixelFormat::PSMT8, 0, 2, 8, 0, 0, 16},
		{TEXT("T8 (0, 1)"), EGSPixelFormat::PSMT8, 0, 2, 0, 1, 2, 0},
		{TEXT("T8 (0, 2): bits 8-15 of word 8"), EGSPixelFormat::PSMT8, 0, 2, 0, 2, 8, 8},
		{TEXT("T8 (4, 2): bits 8-15 of word 0"), EGSPixelFormat::PSMT8, 0, 2, 4, 2, 0, 8},
		{TEXT("T8 (12, 3): bits 24-31 of word 2"), EGSPixelFormat::PSMT8, 0, 2, 12, 3, 2, 24},
		{TEXT("T8 column 1 (0, 4)"), EGSPixelFormat::PSMT8, 0, 2, 0, 4, 16 + 8, 0},
		{TEXT("T8 column 1 (4, 4)"), EGSPixelFormat::PSMT8, 0, 2, 4, 4, 16, 0},
		{TEXT("T8 column 1 (0, 6)"), EGSPixelFormat::PSMT8, 0, 2, 0, 6, 16, 8},
		{TEXT("T8 column 2 (1, 8)"), EGSPixelFormat::PSMT8, 0, 2, 1, 8, 32 + 1, 0},
		{TEXT("T8 column 3 (15, 15)"), EGSPixelFormat::PSMT8, 0, 2, 15, 15, 48 + 15, 24},
		{TEXT("T8 block 1"), EGSPixelFormat::PSMT8, 0, 2, 16, 0, 64, 0},
		{TEXT("T8 block 2"), EGSPixelFormat::PSMT8, 0, 2, 0, 16, 128, 0},
		{TEXT("T8 last word of the page"), EGSPixelFormat::PSMT8, 0, 2, 127, 63, 2047, 24},
		// PSMT4: four 8-pixel groups across in bits 0-3, 8-11, 16-19, 24-27 (rows 0-1), 4-7 ... 28-31 (rows 2-3).
		{TEXT("T4 (0, 0)"), EGSPixelFormat::PSMT4, 0, 2, 0, 0, 0, 0},
		{TEXT("T4 (1, 0)"), EGSPixelFormat::PSMT4, 0, 2, 1, 0, 1, 0},
		{TEXT("T4 (8, 0)"), EGSPixelFormat::PSMT4, 0, 2, 8, 0, 0, 8},
		{TEXT("T4 (16, 0)"), EGSPixelFormat::PSMT4, 0, 2, 16, 0, 0, 16},
		{TEXT("T4 (24, 0)"), EGSPixelFormat::PSMT4, 0, 2, 24, 0, 0, 24},
		{TEXT("T4 (0, 2)"), EGSPixelFormat::PSMT4, 0, 2, 0, 2, 8, 4},
		{TEXT("T4 (4, 2)"), EGSPixelFormat::PSMT4, 0, 2, 4, 2, 0, 4},
		{TEXT("T4 column 1 (0, 4)"), EGSPixelFormat::PSMT4, 0, 2, 0, 4, 16 + 8, 0},
		{TEXT("T4 column 3 (31, 15)"), EGSPixelFormat::PSMT4, 0, 2, 31, 15, 48 + 15, 28},
		{TEXT("T4 block 1 below"), EGSPixelFormat::PSMT4, 0, 2, 0, 16, 64, 0},
		{TEXT("T4 block 2 across"), EGSPixelFormat::PSMT4, 0, 2, 32, 0, 128, 0},
		{TEXT("T4 last word of the page"), EGSPixelFormat::PSMT4, 0, 2, 127, 127, 2047, 28},
	};

	/** Packs pixel values as a transfer sends them: Bits each, little endian, the first 4-bit pixel low (manual 4.3).
	 */
	TArray<uint8> PackTransfer(const TArray<uint32>& Values, uint32 Bits)
	{
		TArray<uint8> Bytes;
		Bytes.SetNumZeroed(int32((uint64(Values.Num()) * Bits + 7) / 8));
		uint64 Cursor = 0;
		for (const uint32 Value : Values)
		{
			for (uint32 Bit = 0; Bit < Bits; ++Bit, ++Cursor)
			{
				Bytes[int32(Cursor / 8)] |= uint8(((Value >> Bit) & 1) << (Cursor % 8));
			}
		}
		return Bytes;
	}

	/** Uploads Width x Height values of Format at (X, Y) of the buffer at BasePointer. */
	void Upload(FGSLocalMemory& Memory, EGSPixelFormat Format, uint16 BasePointer, uint8 BufferWidth, uint16 X,
		uint16 Y, uint16 Width, uint16 Height, const TArray<uint32>& Values)
	{
		FGSBitBltBuf BitBltBuf;
		BitBltBuf.DBP = BasePointer;
		BitBltBuf.DBW = BufferWidth;
		BitBltBuf.DPSM = Format;
		FGSTrxPos TrxPos;
		TrxPos.DSAX = X;
		TrxPos.DSAY = Y;
		FGSTrxReg TrxReg;
		TrxReg.RRW = Width;
		TrxReg.RRH = Height;
		Memory.Transfer(BitBltBuf, TrxPos, TrxReg, PackTransfer(Values, GSBitsPerPixel(Format)));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSLocalMemoryBlockTablesTest, "System.GSCore.LocalMemory.BlockTables",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSLocalMemoryBlockTablesTest::RunTest(const FString& Parameters)
{
	// Every format's block arrangement in a page (8.3), sampled at each block's corner, and its page and block sizes.
	for (const FBlockTable& Table : ManualBlockTables)
	{
		uint32 PageWidth = 0;
		uint32 PageHeight = 0;
		uint32 BlockWidth = 0;
		uint32 BlockHeight = 0;
		FGSLocalMemory::GetPageSize(Table.Format, PageWidth, PageHeight);
		FGSLocalMemory::GetBlockSize(Table.Format, BlockWidth, BlockHeight);
		const FString Name = FString::Printf("Format 0x%02x", uint32(Table.Format));
		TestTrue(*(Name + TEXT(": blocks fill the page")),
			BlockWidth * Table.Columns == PageWidth && BlockHeight * Table.Rows == PageHeight);
		bool bMatches = true;
		for (uint32 Row = 0; Row < Table.Rows; ++Row)
		{
			for (uint32 Column = 0; Column < Table.Columns; ++Column)
			{
				const uint32 Expected = Table.Blocks[(Row * Table.Columns) + Column];
				// A block's corner and its opposite corner.
				bMatches &=
					FGSLocalMemory::GetBlockInPage(Table.Format, Column * BlockWidth, Row * BlockHeight) == Expected;
				bMatches &= FGSLocalMemory::GetBlockInPage(Table.Format, ((Column + 1) * BlockWidth) - 1,
								((Row + 1) * BlockHeight) - 1) == Expected;
			}
		}
		TestTrue(*(Name + TEXT(": the manual's table")), bMatches);
	}

	// Page sizes (8.1).
	uint32 Width = 0;
	uint32 Height = 0;
	FGSLocalMemory::GetPageSize(EGSPixelFormat::PSMCT32, Width, Height);
	TestTrue("PSMCT32 page 64 x 32", Width == 64 && Height == 32);
	FGSLocalMemory::GetPageSize(EGSPixelFormat::PSMZ16S, Width, Height);
	TestTrue("PSMZ16S page 64 x 64", Width == 64 && Height == 64);
	FGSLocalMemory::GetPageSize(EGSPixelFormat::PSMT8, Width, Height);
	TestTrue("PSMT8 page 128 x 64", Width == 128 && Height == 64);
	FGSLocalMemory::GetPageSize(EGSPixelFormat::PSMT4, Width, Height);
	TestTrue("PSMT4 page 128 x 128", Width == 128 && Height == 128);
	FGSLocalMemory::GetPageSize(EGSPixelFormat::PSMT8H, Width, Height);
	TestTrue("PSMT8H page: PSMCT32's", Width == 64 && Height == 32);

	// Occupied blocks (8.4): 8 x 8 PSMCT32 block 0; 32 x 32 blocks 0 to 15; 8 x 16 blocks 0 and 2; 8 x 32 blocks 0, 2,
	// 8 and 10.
	TestEqual("8 x 8: 1 block", FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, 8, 8), 1u);
	TestEqual("32 x 32: blocks 0 to 15", FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, 32, 32), 16u);
	TestEqual("8 x 16: up to block 2", FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, 8, 16), 3u);
	TestEqual("8 x 32: up to block 10", FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, 8, 32), 11u);
	TestEqual("A PSMT8 CLUT (16 x 16 PSMCT32): 4 blocks",
		FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, 16, 16), 4u);
	TestEqual(
		"A PSMT4 CLUT (8 x 2 PSMCT32): 1 block", FGSLocalMemory::GetBlockSpan(1, EGSPixelFormat::PSMCT32, 8, 2), 1u);
	TestEqual("A 640 x 448 PSMCT16S frame: 70 pages",
		FGSLocalMemory::GetBlockSpan(10, EGSPixelFormat::PSMCT16S, 640, 448), 70u * 32u);

	// Pointing within a page (8.5): the table shifts by the base pointer and runs into the next page.
	TestEqual("Block 1: (0, 0)", FGSLocalMemory::GetBlockAddress(1, 1, EGSPixelFormat::PSMCT32, 0, 0), 1u);
	TestEqual("Block 1: the last block is the next page's 0",
		FGSLocalMemory::GetBlockAddress(1, 1, EGSPixelFormat::PSMCT32, 56, 24), 32u);
	TestEqual("Block 9: (0, 16) is 17", FGSLocalMemory::GetBlockAddress(9, 1, EGSPixelFormat::PSMCT32, 0, 16), 17u);
	TestEqual("Block 9: (56, 8) is the next page's 0",
		FGSLocalMemory::GetBlockAddress(9, 1, EGSPixelFormat::PSMCT32, 56, 8), 32u);
	TestEqual("Block 9: (32, 16) is the next page's 1",
		FGSLocalMemory::GetBlockAddress(9, 1, EGSPixelFormat::PSMCT32, 32, 16), 33u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSLocalMemoryPixelAddressesTest, "System.GSCore.LocalMemory.PixelAddresses",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSLocalMemoryPixelAddressesTest::RunTest(const FString& Parameters)
{
	// Each pixel's word and bits (8.3's column figures).
	for (const FPixelVector& Vector : ManualPixelVectors)
	{
		const uint32 Bit =
			FGSLocalMemory::GetBitAddress(Vector.BasePointer, Vector.BufferWidth, Vector.Format, Vector.X, Vector.Y);
		TestEqual(*FString::Printf("%s: word", Vector.Name), Bit / 32, Vector.Word);
		TestEqual(*FString::Printf("%s: first bit", Vector.Name), Bit % 32, Vector.Shift);
	}

	// Same block and column, same memory, whatever the format (8.3): a PSMCT32 word read as the other formats.
	FGSLocalMemory Memory;
	Memory.WritePixel(0, 1, EGSPixelFormat::PSMCT32, 0, 0, 0x44332211u);
	TestEqual("T8 (0, 0): byte 0", Memory.ReadPixel(0, 2, EGSPixelFormat::PSMT8, 0, 0), 0x11u);
	TestEqual("T8 (4, 2): byte 1", Memory.ReadPixel(0, 2, EGSPixelFormat::PSMT8, 4, 2), 0x22u);
	TestEqual("T8 (8, 0): byte 2", Memory.ReadPixel(0, 2, EGSPixelFormat::PSMT8, 8, 0), 0x33u);
	TestEqual("T8 (12, 2): byte 3", Memory.ReadPixel(0, 2, EGSPixelFormat::PSMT8, 12, 2), 0x44u);
	TestEqual("T4 (4, 2): nibble 1", Memory.ReadPixel(0, 2, EGSPixelFormat::PSMT4, 4, 2), 0x1u);
	TestEqual("T4 (24, 0): nibble 6", Memory.ReadPixel(0, 2, EGSPixelFormat::PSMT4, 24, 0), 0x4u);
	TestEqual("CT16 (0, 0): the low half", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMCT16, 0, 0), 0x2211u);
	TestEqual("CT16 (8, 0): the high half", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMCT16, 8, 0), 0x4433u);
	TestEqual("T8H (0, 0): bits 24-31", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMT8H, 0, 0), 0x44u);

	// PSMCT24 and the H formats share the words: a 24-bit transfer and the H formats write their bits only.
	Upload(Memory, EGSPixelFormat::PSMCT24, 0, 1, 0, 0, 1, 1, {0xccbbaau});
	TestEqual("CT24 keeps the high byte", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMCT32, 0, 0), 0x44ccbbaau);
	Memory.WritePixel(0, 1, EGSPixelFormat::PSMT4HL, 0, 0, 0x9u);
	Memory.WritePixel(0, 1, EGSPixelFormat::PSMT4HH, 0, 0, 0x7u);
	TestEqual(
		"T4HL and T4HH: bits 24-27 and 28-31", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMCT32, 0, 0), 0x79ccbbaau);
	Memory.WritePixel(0, 1, EGSPixelFormat::PSMT8H, 0, 0, 0x12u);
	TestEqual("T8H: bits 24-31", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMCT32, 0, 0), 0x12ccbbaau);
	TestEqual("CT24 reads its word", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMCT24, 0, 0) & 0xffffffu, 0xccbbaau);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSLocalMemoryTransferTest, "System.GSCore.LocalMemory.TransferRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSLocalMemoryTransferTest::RunTest(const FString& Parameters)
{
	// Every format: a rectangle uploaded at an offset reads back pixel for pixel, and the pixels around it stay 0.
	constexpr uint16 X0 = 8;
	constexpr uint16 Y0 = 4;
	constexpr uint16 Width = 136;
	constexpr uint16 Height = 70;
	for (const EGSPixelFormat Format : {EGSPixelFormat::PSMCT32, EGSPixelFormat::PSMCT24, EGSPixelFormat::PSMCT16,
			 EGSPixelFormat::PSMCT16S, EGSPixelFormat::PSMT8, EGSPixelFormat::PSMT4, EGSPixelFormat::PSMT8H,
			 EGSPixelFormat::PSMT4HL, EGSPixelFormat::PSMT4HH, EGSPixelFormat::PSMZ32, EGSPixelFormat::PSMZ24,
			 EGSPixelFormat::PSMZ16, EGSPixelFormat::PSMZ16S})
	{
		const uint32 Bits = GSBitsPerPixel(Format);
		const uint32 Mask = Bits >= 32 ? 0xffffffffu : ((1u << Bits) - 1);
		TArray<uint32> Values;
		for (uint32 Y = 0; Y < Height; ++Y)
		{
			for (uint32 X = 0; X < Width; ++X)
			{
				Values.Add(((X * 0x9e3779b1u) ^ (Y * 0x85ebca6bu) ^ 0x5bd1e995u) & Mask);
			}
		}
		FGSLocalMemory Memory;
		Upload(Memory, Format, 64, 4, X0, Y0, Width, Height, Values);
		bool bRoundTrip = true;
		bool bAroundUntouched = true;
		for (uint32 Y = 0; Y < uint32(Y0 + Height + 4); ++Y)
		{
			for (uint32 X = 0; X < uint32(X0 + Width + 8); ++X)
			{
				const uint32 Read = FGSLocalMemory::StorageBits(Format) == 32 && Bits == 24
					? Memory.ReadPixel(64, 4, Format, X, Y) & 0xffffffu
					: Memory.ReadPixel(64, 4, Format, X, Y);
				const bool bInside = X >= X0 && X < uint32(X0 + Width) && Y >= Y0 && Y < uint32(Y0 + Height);
				if (bInside)
				{
					bRoundTrip &= Read == Values[int32(((Y - Y0) * Width) + (X - X0))];
				}
				else
				{
					bAroundUntouched &= Read == 0;
				}
			}
		}
		const FString Name = FString::Printf("Format 0x%02x", uint32(Format));
		TestTrue(*(Name + TEXT(" reads back")), bRoundTrip);
		TestTrue(*(Name + TEXT(" leaves the pixels around")), bAroundUntouched);
	}

	// A PSMT8H texture over a PSMCT24 image: both read back (they share the words).
	FGSLocalMemory Memory;
	TArray<uint32> Colors;
	TArray<uint32> Indices;
	for (uint32 Index = 0; Index < 64 * 32; ++Index)
	{
		Colors.Add((Index * 2654435761u) & 0xffffffu);
		Indices.Add(Index & 0xff);
	}
	Upload(Memory, EGSPixelFormat::PSMCT24, 32, 1, 0, 0, 64, 32, Colors);
	Upload(Memory, EGSPixelFormat::PSMT8H, 32, 1, 0, 0, 64, 32, Indices);
	bool bShared = true;
	for (uint32 Index = 0; Index < 64 * 32; ++Index)
	{
		const uint32 Word = Memory.ReadPixel(32, 1, EGSPixelFormat::PSMCT32, Index % 64, Index / 64);
		bShared &= Word == (Colors[int32(Index)] | (Indices[int32(Index)] << 24));
	}
	TestTrue("PSMCT24 and PSMT8H share a page", bShared);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSLocalMemoryClutTest, "System.GSCore.LocalMemory.ClutCsm1",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSLocalMemoryClutTest::RunTest(const FString& Parameters)
{
	// CSM1 CLUTs (manual 2.7.3, 3.4.7) packed as the texture cache packs them: a PSMT8 CLUT at CBP 4 (blocks 4 to 7),
	// a PSMT4 CLUT at CBP 8 (block 8) and a second PSMT8 CLUT at CBP 9. Each loads whole into the temporary buffer.
	const auto MakePalette = [](uint32 NumEntries, uint32 Seed)
	{
		TArray<uint32> Palette;
		for (uint32 Index = 0; Index < NumEntries; ++Index)
		{
			Palette.Add(((Index * 0x010307u) ^ Seed) | 0xff000000u);
		}
		return Palette;
	};
	const TArray<uint32> Palette8A = MakePalette(256, 0x123456u);
	const TArray<uint32> Palette4 = MakePalette(16, 0x654321u);
	const TArray<uint32> Palette8B = MakePalette(256, 0xabcdefu);

	FGSLocalMemory Memory;
	const auto UploadClut = [&Memory](const TArray<uint32>& Palette, uint16 CBP)
	{
		TArray<uint8> Image;
		uint16 Width = 0;
		uint16 Height = 0;
		FGSTextureLayout::MakeClutImage(Palette, Image, Width, Height);
		TArray<uint32> Texels;
		for (int32 Texel = 0; Texel < int32(Width) * Height; ++Texel)
		{
			Texels.Add(uint32(Image[Texel * 4]) | (uint32(Image[(Texel * 4) + 1]) << 8) |
				(uint32(Image[(Texel * 4) + 2]) << 16) | (uint32(Image[(Texel * 4) + 3]) << 24));
		}
		Upload(Memory, EGSPixelFormat::PSMCT32, CBP, 1, 0, 0, Width, Height, Texels);
	};
	UploadClut(Palette8A, 4);
	UploadClut(Palette4, 8);
	UploadClut(Palette8B, 9);

	// The CSM1 arrangement: entry 8 at (0, 1), entry 16 at (8, 0); a 16 x 16 CLUT's last texel in block CBP + 3.
	const uint32 Entry8 = (Palette8A[8] & 0xffffffu) | 0x80000000u;
	const uint32 Entry16 = (Palette8A[16] & 0xffffffu) | 0x80000000u;
	TestEqual("Entry 8 at (0, 1)", Memory.ReadPixel(4, 1, EGSPixelFormat::PSMCT32, 0, 1), Entry8);
	TestEqual("Entry 16 at (8, 0)", Memory.ReadPixel(4, 1, EGSPixelFormat::PSMCT32, 8, 0), Entry16);
	TestEqual("(15, 15) in block 7", FGSLocalMemory::GetBlockAddress(4, 1, EGSPixelFormat::PSMCT32, 15, 15), 7u);
	TestEqual("PSMT8's CLUT: 4 blocks", FGSTextureLayout::GetClutBlocks(EGSPixelFormat::PSMT8), 4u);
	TestEqual("PSMT4's CLUT: 1 block", FGSTextureLayout::GetClutBlocks(EGSPixelFormat::PSMT4), 1u);

	const auto Matches = [](const FGSClutBuffer& Clut, const TArray<uint32>& Palette, uint32 First)
	{
		bool bMatches = true;
		for (int32 Index = 0; Index < Palette.Num(); ++Index)
		{
			bMatches &= Clut.Entries[First + uint32(Index)] == ((Palette[Index] & 0xffffffu) | 0x80000000u);
		}
		return bMatches;
	};
	FGSTex0 Tex8;
	Tex8.PSM = EGSPixelFormat::PSMT8;
	Tex8.CPSM = EGSPixelFormat::PSMCT32;
	Tex8.CBP = 4;
	FGSClutBuffer Clut;
	Clut.Load(Memory, Tex8);
	TestTrue("The first PSMT8 CLUT whole", Matches(Clut, Palette8A, 0));
	FGSTex0 Tex4 = Tex8;
	Tex4.PSM = EGSPixelFormat::PSMT4;
	Tex4.CBP = 8;
	Tex4.CSA = 2;
	Clut.Load(Memory, Tex4);
	TestTrue("The PSMT4 CLUT at CSA 2", Matches(Clut, Palette4, 32));
	Tex8.CBP = 9;
	Clut.Load(Memory, Tex8);
	TestTrue("The second PSMT8 CLUT whole", Matches(Clut, Palette8B, 0));

	// The load control (CLD, manual 3.4.7): 0 keeps, 1 loads, 2 / 3 load and set CBP0 / CBP1, 4 / 5 load only when that
	// register differs from CBP and then set it; a texture that is not indexed never loads.
	const auto Update = [&Clut, &Memory, &Tex8](uint16 CBP, uint8 CLD)
	{
		FGSTex0 Tex0 = Tex8;
		Tex0.CBP = CBP;
		Tex0.CLD = CLD;
		return Clut.Update(Memory, Tex0);
	};
	TestTrue("CLD 2 loads", Update(4, 2) && Clut.CBP0 == 4 && Matches(Clut, Palette8A, 0));
	TestFalse("CLD 0 keeps", Update(9, 0));
	TestFalse("CLD 4 at CBP0 keeps", Update(4, 4));
	TestTrue("CLD 4 elsewhere loads", Update(9, 4) && Clut.CBP0 == 9 && Matches(Clut, Palette8B, 0));
	TestTrue("CLD 3 loads", Update(4, 3) && Clut.CBP1 == 4 && Matches(Clut, Palette8A, 0));
	TestTrue("CLD 1 loads", Update(9, 1) && Clut.CBP1 == 4 && Matches(Clut, Palette8B, 0));
	TestFalse("CLD 5 at CBP1 keeps", Update(4, 5));
	TestTrue("The buffer kept", Matches(Clut, Palette8B, 0));
	TestFalse("CLD 6 (reserved) keeps", Update(4, 6));
	FGSTex0 Direct = Tex8;
	Direct.PSM = EGSPixelFormat::PSMCT32;
	Direct.CBP = 4;
	Direct.CLD = 2;
	TestFalse("A PSMCT32 texture loads nothing", Clut.Update(Memory, Direct) || Clut.CBP0 != 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSLocalMemoryWrapTest, "System.GSCore.LocalMemory.Wrap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSLocalMemoryWrapTest::RunTest(const FString& Parameters)
{
	// Addresses wrap at 4 MB, transfer coordinates at 2048.
	TestEqual(
		"Block 16383 + 1 is block 0", FGSLocalMemory::GetBlockAddress(16383, 1, EGSPixelFormat::PSMCT32, 8, 0), 0u);
	FGSLocalMemory Memory;
	Memory.WritePixel(16383, 1, EGSPixelFormat::PSMCT32, 8, 0, 0xdeadbeefu);
	TestEqual("Read back from block 0", Memory.ReadPixel(0, 1, EGSPixelFormat::PSMCT32, 0, 0), 0xdeadbeefu);
	TArray<uint32> Values;
	for (uint32 Index = 0; Index < 8; ++Index)
	{
		Values.Add(Index + 1);
	}
	Upload(Memory, EGSPixelFormat::PSMCT32, 64, 1, 2044, 0, 8, 1, Values);
	TestEqual("X 2047", Memory.ReadPixel(64, 1, EGSPixelFormat::PSMCT32, 2047, 0), 4u);
	TestEqual("X 2048 is X 0", Memory.ReadPixel(64, 1, EGSPixelFormat::PSMCT32, 0, 0), 5u);
	TestEqual("X 2051 is X 3", Memory.ReadPixel(64, 1, EGSPixelFormat::PSMCT32, 3, 0), 8u);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
