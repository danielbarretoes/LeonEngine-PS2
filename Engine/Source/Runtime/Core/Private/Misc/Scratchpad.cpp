#include "Misc/Scratchpad.h"

#include "HAL/PlatformMemory.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Templates/AlignmentTemplates.h"

namespace
{

	/** The scratchpad of a platform without one (and of the PS2 with -nospr): main RAM, the same size. */
	alignas(64) uint8 GScratchpadInRam[FScratchpad::Size];

} // namespace

FScratchpad& FScratchpad::Get()
{
	static FScratchpad Scratchpad;
	return Scratchpad;
}

FScratchpad::FScratchpad()
{
	uint8* OnChip = FPlatformMemory::GetOnChipScratchpad();
	bOnChip = OnChip != nullptr && !FParse::Param(FCommandLine::Get(), TEXT("nospr"));
	Base = bOnChip ? OnChip : GScratchpadInRam;
}

uint8* FScratchpad::PushBytes(SIZE_T AllocSize, SIZE_T Alignment)
{
	const SIZE_T Start = Align(Used, Alignment);
	if (NumMarks > 0 && Start + AllocSize <= Size)
	{
		Used = Start + AllocSize;
		PeakUsed = Used > PeakUsed ? Used : PeakUsed;
		return Base + Start;
	}
	// Full (or no mark to give it back): the frame's stack takes it.
	++NumOverflows;
	return FMemStack::Get().PushBytes(AllocSize, Alignment);
}

bool FScratchpad::GrowTop(const uint8* Block, SIZE_T OldSize, SIZE_T NewSize)
{
	if (!Contains(Block))
	{
		return false;
	}
	const SIZE_T Start = SIZE_T(Block - Base);
	if (Start + OldSize != Used || Start + NewSize > Size)
	{
		return false;
	}
	Used = Start + NewSize;
	PeakUsed = Used > PeakUsed ? Used : PeakUsed;
	return true;
}

FScratchpadMark::FScratchpadMark()
	: Used(FScratchpad::Get().Used)
{
	++FScratchpad::Get().NumMarks;
}

FScratchpadMark::~FScratchpadMark()
{
	FScratchpad& Scratchpad = FScratchpad::Get();
	check(Scratchpad.NumMarks > 0);
	--Scratchpad.NumMarks;
	Scratchpad.Used = Used;
}
