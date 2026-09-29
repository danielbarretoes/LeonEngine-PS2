#include "GSGifPacket.h"

namespace
{

	[[nodiscard]] uint64 MakePackedOrImageTag(uint32 Loops, EGSGifFormat Format)
	{
		return FGSGifPacket::MakeTag(Loops, false, Format, Format == EGSGifFormat::Packed ? 1 : 0);
	}

	/** Build's output: the packet appended to an array of 64-bit words. */
	struct FArrayWriter
	{
		TArray<uint64>& Out;
		int32 LastTag = INDEX_NONE;

		void Tag(uint32 Loops, EGSGifFormat Format)
		{
			LastTag = Out.Num();
			Out.Add(MakePackedOrImageTag(Loops, Format));
			Out.Add(Format == EGSGifFormat::Packed ? FGSGifPacket::AddressData : 0);
		}

		void Write(const FGSRegisterWrite& RegisterWrite)
		{
			Out.Add(RegisterWrite.Value);
			Out.Add(uint64(RegisterWrite.Register));
		}

		void Image(const uint8* Data, int32 NumQuadwords)
		{
			const int32 Start = Out.Num();
			Out.AddUninitialized(NumQuadwords * 2);
			FMemory::Memcpy(&Out[Start], Data, size_t(NumQuadwords) * 16);
		}

		void Batch(const FGSCommandList& List, const FGSVertexBatch& VertexBatch)
		{
			(void)List;
			(void)VertexBatch;
			checkf(false, TEXT("FGSGifPacket::Build: a vertex batch's packet is VU1's (BuildChain)"));
		}

		void EndPacket()
		{
			if (LastTag != INDEX_NONE)
			{
				Out[LastTag] |= FGSGifPacket::EndOfPacketBit;
			}
		}
	};

	/**
	 * BuildChain's output: the packet as a DMA source chain for VIF1, written forward and never read (the PS2 writes it
	 * through the uncached accelerated segment). A section's DMAtag is written when the section closes, with its
	 * VIFcodes (a DIRECT of the section, after a FLUSH when batches came before it), and a GIFtag's EOP from the value
	 * kept aside.
	 */
	struct FChainWriter
	{
		uint64* Out;
		IGSVertexBatchEncoder* Encoder = nullptr;
		uint64* SectionTag = nullptr;
		uint32 SectionQuadwords = 0;
		/** The section after vertex batches: it waits for their microprograms and packets (FLUSH). */
		bool bFlushSection = false;
		/** The last GIFtag, while its packet is open (no EOP yet). */
		uint64* OpenTag = nullptr;
		uint64 OpenTagValue = 0;

		FChainWriter(uint64* InOut, IGSVertexBatchEncoder* InEncoder)
			: Out(InOut)
			, Encoder(InEncoder)
		{
			OpenSection();
		}

		void OpenSection()
		{
			SectionTag = Out;
			Out += 2;
			SectionQuadwords = 0;
		}

		void CloseSection(EGSDmaTag Id)
		{
			const uint32 First = FGSGifPacket::MakeVifCode(bFlushSection ? EGSVifCommand::Flush : EGSVifCommand::Nop);
			const uint32 Second = SectionQuadwords > 0
				? FGSGifPacket::MakeVifCode(EGSVifCommand::Direct, 0, SectionQuadwords)
				: FGSGifPacket::MakeVifCode(EGSVifCommand::Nop);
			SectionTag[0] = FGSGifPacket::MakeDmaTag(SectionQuadwords, Id);
			SectionTag[1] = FGSGifPacket::MakeVifCodes(First, Second);
			bFlushSection = false;
		}

		/** Ends the open GIF packet: its last GIFtag gets EOP. */
		void CloseGifPacket()
		{
			if (OpenTag != nullptr)
			{
				OpenTag[0] = OpenTagValue | FGSGifPacket::EndOfPacketBit;
				OpenTag = nullptr;
			}
		}

		/** Room for one more quadword in the section: a full one goes on in the next CNT (the GIF sees no DMAtag). */
		FORCEINLINE void AddQuadword()
		{
			if (SectionQuadwords == FGSGifPacket::MaxDmaQuadwords)
			{
				CloseSection(EGSDmaTag::Cnt);
				OpenSection();
			}
			++SectionQuadwords;
		}

		void Tag(uint32 Loops, EGSGifFormat Format)
		{
			AddQuadword();
			OpenTag = Out;
			OpenTagValue = MakePackedOrImageTag(Loops, Format);
			Out[0] = OpenTagValue;
			Out[1] = Format == EGSGifFormat::Packed ? FGSGifPacket::AddressData : 0;
			Out += 2;
		}

		FORCEINLINE void Write(const FGSRegisterWrite& RegisterWrite)
		{
			AddQuadword();
			Out[0] = RegisterWrite.Value;
			Out[1] = uint64(RegisterWrite.Register);
			Out += 2;
		}

		/**
		 * The pixels by reference: the CNT closes after their IMAGE GIFtag, a REF points at them with a DIRECT of its
		 * own (the GIF packet goes on across the two DIRECTs: nothing else reaches the GIF between them), a new CNT
		 * follows.
		 */
		void Image(const uint8* Data, int32 NumQuadwords)
		{
			check((UPTRINT(Data) & 15) == 0);
			CloseSection(EGSDmaTag::Cnt);
			Out[0] = FGSGifPacket::MakeDmaTag(uint32(NumQuadwords), EGSDmaTag::Ref, FGSGifPacket::GetDmaAddress(Data));
			Out[1] = FGSGifPacket::MakeVifCodes(FGSGifPacket::MakeVifCode(EGSVifCommand::Nop),
				FGSGifPacket::MakeVifCode(EGSVifCommand::Direct, 0, uint32(NumQuadwords)));
			Out += 2;
			OpenSection();
		}

		/**
		 * A vertex batch: the GS packet before it ends, the section closes (or is dropped when empty) and the Encoder
		 * writes the batch; the section after it flushes.
		 */
		void Batch(const FGSCommandList& List, const FGSVertexBatch& VertexBatch)
		{
			checkf(Encoder != nullptr, TEXT("FGSGifPacket::BuildChain: a vertex batch needs an encoder"));
			CloseGifPacket();
			if (SectionQuadwords > 0)
			{
				CloseSection(EGSDmaTag::Cnt);
			}
			else
			{
				// Nothing in it: its tag's place goes to the batch; a FLUSH it had waits for the next section.
				Out = SectionTag;
			}
			Out = Encoder->WriteBatch(List, VertexBatch, Out);
			OpenSection();
			bFlushSection = true;
		}

		void EndPacket()
		{
			CloseGifPacket();
			CloseSection(EGSDmaTag::End);
		}
	};

	/** Writes [First, First + Num) of Writes in PACKED A+D, a GIFtag every MaxLoops. */
	template <typename WriterType>
	void WritePacked(WriterType& Writer, const FGSRegisterWrite* Writes, int32 Num)
	{
		for (int32 First = 0; First < Num; First += int32(FGSGifPacket::MaxLoops))
		{
			const int32 Count = FMath::Min(Num - First, int32(FGSGifPacket::MaxLoops));
			Writer.Tag(uint32(Count), EGSGifFormat::Packed);
			for (int32 Index = First; Index < First + Count; ++Index)
			{
				Writer.Write(Writes[Index]);
			}
		}
	}

	template <typename WriterType>
	void WriteImage(WriterType& Writer, TArrayView<const uint8> Data)
	{
		check(Data.Num() % 16 == 0);
		const int32 NumQuadwords = Data.Num() / 16;
		for (int32 First = 0; First < NumQuadwords; First += int32(FGSGifPacket::MaxLoops))
		{
			const int32 Count = FMath::Min(NumQuadwords - First, int32(FGSGifPacket::MaxLoops));
			Writer.Tag(uint32(Count), EGSGifFormat::Image);
			Writer.Image(&Data[First * 16], Count);
		}
	}

	/**
	 * The packet's one encoding: runs of register writes, each HWREG's image, each vertex batch, then FINISH; EOP on
	 * the last GIFtag.
	 */
	template <typename WriterType>
	void WritePacket(const FGSCommandList& List, bool bFinish, WriterType& Writer)
	{
		const TArray<FGSRegisterWrite>& Writes = List.GetWrites();
		int32 RunStart = 0;
		for (int32 Index = 0; Index <= Writes.Num(); ++Index)
		{
			const EGSRegister Register = Index < Writes.Num() ? Writes[Index].Register : EGSRegister::FINISH;
			const bool bImage = Index < Writes.Num() && Register == EGSRegister::HWREG;
			const bool bBatch = Index < Writes.Num() && Register == EGSRegister::VertexBatch;
			if (Index < Writes.Num() && !bImage && !bBatch)
			{
				continue;
			}
			if (Index > RunStart)
			{
				WritePacked(Writer, &Writes[RunStart], Index - RunStart);
			}
			if (bImage)
			{
				WriteImage(Writer, List.GetImage(int32(Writes[Index].Value)));
			}
			if (bBatch)
			{
				Writer.Batch(List, List.GetVertexBatches()[int32(Writes[Index].Value)]);
			}
			RunStart = Index + 1;
		}
		if (bFinish)
		{
			const FGSRegisterWrite Finish{EGSRegister::FINISH, 0};
			WritePacked(Writer, &Finish, 1);
		}
		Writer.EndPacket();
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
	FArrayWriter Writer{OutQuadwords};
	WritePacket(List, bFinish, Writer);
}

uint64 FGSGifPacket::MakeDmaTag(uint32 Quadwords, EGSDmaTag Id, uint32 Address)
{
	check(Quadwords <= MaxDmaQuadwords && (Address & 0x8000000fu) == 0);
	return uint64(Quadwords) | (uint64(Id) << 28) | (uint64(Address) << 32);
}

uint32 FGSGifPacket::MakeVifCode(EGSVifCommand Command, uint32 Num, uint32 Immediate)
{
	check(Num <= 0xff && Immediate <= 0xffff);
	return Immediate | (Num << 16) | (uint32(Command) << 24);
}

uint32 FGSGifPacket::GetDmaAddress(const void* Data)
{
	return uint32(UPTRINT(Data)) & 0x0fffffffu;
}

uint32 FGSGifPacket::GetChainCapacity(const FGSCommandList& List, bool bFinish, const IGSVertexBatchEncoder* Encoder)
{
	uint32 ImageChunks = 0;
	for (int32 Index = 0; Index < List.GetNumImages(); ++Index)
	{
		ImageChunks += (uint32(List.GetImage(Index).Num() / 16) + MaxLoops - 1) / MaxLoops;
	}
	// The register writes and their GIFtags (a run between two images or batches, the FINISH run, one more every
	// MaxLoops), each image chunk's GIFtag, REF and the CNT after it, the sections a full CNT splits, the first and the
	// END; each batch's own quadwords and the CNT after it, and the encoder's prologue.
	const uint32 NumImages = uint32(List.GetNumImages());
	const uint32 NumBatches = uint32(List.GetVertexBatches().Num());
	const uint32 NumWrites = uint32(List.GetWrites().Num()) - NumImages - NumBatches + (bFinish ? 1u : 0u);
	const uint32 Data = NumWrites + (NumWrites / MaxLoops) + NumImages + NumBatches + 2 + (3 * ImageChunks);
	uint32 Capacity = Data + (Data / MaxDmaQuadwords) + 2;
	if (NumBatches > 0)
	{
		check(Encoder != nullptr);
		Capacity += Encoder->GetPrologueQuadwords() + (NumBatches * (Encoder->GetMaxBatchQuadwords() + 1));
	}
	return Capacity;
}

uint32 FGSGifPacket::BuildChain(const FGSCommandList& List, bool bFinish, uint64* OutQuadwords,
	uint32 CapacityQuadwords, IGSVertexBatchEncoder* Encoder)
{
	checkSlow(GetChainCapacity(List, bFinish, Encoder) <= CapacityQuadwords);
	uint64* Start = OutQuadwords;
	if (Encoder != nullptr && List.GetVertexBatches().Num() > 0)
	{
		Start = Encoder->WritePrologue(OutQuadwords);
	}
	FChainWriter Writer(Start, Encoder);
	WritePacket(List, bFinish, Writer);
	const uint32 Written = uint32((Writer.Out - OutQuadwords) / 2);
	check(Written <= CapacityQuadwords);
	return Written;
}
