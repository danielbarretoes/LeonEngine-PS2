#include "GSGifPacket.h"

namespace
{

	constexpr uint64 EndOfPacketBit = uint64(1) << 15;

	/** Appends a tag and returns its index in Out (to set EOP on the last one). */
	int32 AddTag(TArray<uint64>& Out, uint32 Loops, EGSGifFormat Format)
	{
		const int32 Index = Out.Num();
		const bool bPacked = Format == EGSGifFormat::Packed;
		Out.Add(FGSGifPacket::MakeTag(Loops, false, Format, bPacked ? 1 : 0));
		Out.Add(bPacked ? FGSGifPacket::AddressData : 0);
		return Index;
	}

	/** Writes [First, First + Count) of Writes in PACKED A+D. */
	int32 AddPacked(TArray<uint64>& Out, TArrayView<const FGSRegisterWrite> Writes)
	{
		int32 LastTag = INDEX_NONE;
		for (int32 First = 0; First < Writes.Num(); First += int32(FGSGifPacket::MaxLoops))
		{
			const int32 Count = FMath::Min(Writes.Num() - First, int32(FGSGifPacket::MaxLoops));
			LastTag = AddTag(Out, uint32(Count), EGSGifFormat::Packed);
			for (int32 Index = First; Index < First + Count; ++Index)
			{
				Out.Add(Writes[Index].Value);
				Out.Add(uint64(Writes[Index].Register));
			}
		}
		return LastTag;
	}

	int32 AddImage(TArray<uint64>& Out, const TArray<uint8>& Data)
	{
		check(Data.Num() % 16 == 0);
		const int32 NumQuadwords = Data.Num() / 16;
		int32 LastTag = INDEX_NONE;
		for (int32 First = 0; First < NumQuadwords; First += int32(FGSGifPacket::MaxLoops))
		{
			const int32 Count = FMath::Min(NumQuadwords - First, int32(FGSGifPacket::MaxLoops));
			LastTag = AddTag(Out, uint32(Count), EGSGifFormat::Image);
			const int32 Start = Out.Num();
			Out.AddUninitialized(Count * 2);
			FMemory::Memcpy(&Out[Start], &Data[First * 16], size_t(Count) * 16);
		}
		return LastTag;
	}

} // namespace

uint64 FGSGifPacket::MakeTag(uint32 Loops, bool bEndOfPacket, EGSGifFormat Format, uint32 NumRegisters)
{
	check(Loops <= MaxLoops && NumRegisters <= 16);
	return uint64(Loops) | (bEndOfPacket ? EndOfPacketBit : 0) | (uint64(Format) << 58) |
		(uint64(NumRegisters & 0xf) << 60);
}

void FGSGifPacket::Build(const FGSCommandList& List, bool bFinish, TArray<uint64>& OutQuadwords)
{
	const TArray<FGSRegisterWrite>& Writes = List.GetWrites();
	int32 LastTag = INDEX_NONE;
	int32 RunStart = 0;
	for (int32 Index = 0; Index <= Writes.Num(); ++Index)
	{
		const bool bImage = Index < Writes.Num() && Writes[Index].Register == EGSRegister::HWREG;
		if (Index < Writes.Num() && !bImage)
		{
			continue;
		}
		if (Index > RunStart)
		{
			LastTag = AddPacked(OutQuadwords, MakeArrayView(&Writes[RunStart], Index - RunStart));
		}
		if (bImage)
		{
			LastTag = AddImage(OutQuadwords, List.GetImageData()[int32(Writes[Index].Value)]);
		}
		RunStart = Index + 1;
	}
	if (bFinish)
	{
		const FGSRegisterWrite Finish{EGSRegister::FINISH, 0};
		LastTag = AddPacked(OutQuadwords, MakeArrayView(&Finish, 1));
	}
	if (LastTag != INDEX_NONE)
	{
		OutQuadwords[LastTag] |= EndOfPacketBit;
	}
}
