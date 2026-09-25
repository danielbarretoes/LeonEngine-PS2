#include "Factories/SoundFactory.h"

#include "LeonEdLog.h"
#include "Sound/SoundWave.h"

USoundFactory::USoundFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundWave::StaticClass();
	Formats.Add(TEXT("wav;Sound"));
	bEditorImport = 1;
}

bool USoundFactory::ReadWave(const uint8* Buffer, int64 Size, TArray<int16>& OutSamples, int32& OutChannels,
	int32& OutSampleRate, FString& OutError)
{
	auto ReadU32 = [Buffer](int64 Offset)
	{
		return static_cast<uint32>(Buffer[Offset]) | (static_cast<uint32>(Buffer[Offset + 1]) << 8) |
			(static_cast<uint32>(Buffer[Offset + 2]) << 16) | (static_cast<uint32>(Buffer[Offset + 3]) << 24);
	};
	auto ReadU16 = [Buffer](int64 Offset)
	{ return static_cast<uint16>(Buffer[Offset] | (static_cast<uint16>(Buffer[Offset + 1]) << 8)); };
	auto HasTag = [Buffer](int64 Offset, const char* Tag) { return FMemory::Memcmp(Buffer + Offset, Tag, 4) == 0; };

	if (Buffer == nullptr || Size < 12 || !HasTag(0, "RIFF") || !HasTag(8, "WAVE"))
	{
		OutError = TEXT("not a RIFF / WAVE file");
		return false;
	}
	int32 Format = 0;
	int32 BitsPerSample = 0;
	int64 DataOffset = INDEX_NONE;
	int64 DataSize = 0;
	OutChannels = 0;
	OutSampleRate = 0;
	for (int64 Offset = 12; Offset + 8 <= Size;)
	{
		const int64 ChunkSize = static_cast<int64>(ReadU32(Offset + 4));
		const int64 ChunkData = Offset + 8;
		if (ChunkData + ChunkSize > Size)
		{
			break;
		}
		if (HasTag(Offset, "fmt ") && ChunkSize >= 16)
		{
			Format = ReadU16(ChunkData);
			OutChannels = ReadU16(ChunkData + 2);
			OutSampleRate = static_cast<int32>(ReadU32(ChunkData + 4));
			BitsPerSample = ReadU16(ChunkData + 14);
			// WAVE_FORMAT_EXTENSIBLE: the real format is the sub-format GUID's first two bytes.
			if (Format == 0xFFFE && ChunkSize >= 26)
			{
				Format = ReadU16(ChunkData + 24);
			}
		}
		else if (HasTag(Offset, "data"))
		{
			DataOffset = ChunkData;
			DataSize = ChunkSize;
		}
		// Chunks are padded to an even size.
		Offset = ChunkData + ChunkSize + (ChunkSize & 1);
	}
	if (Format != 1 || BitsPerSample != 16 || OutChannels <= 0 || OutSampleRate <= 0 || DataOffset == INDEX_NONE)
	{
		OutError = FString::Printf("not 16-bit PCM (format %d, %d bits)", Format, BitsPerSample);
		return false;
	}
	const int32 NumSamples = static_cast<int32>(DataSize / 2);
	OutSamples.SetNumUninitialized(NumSamples);
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		OutSamples[Index] = static_cast<int16>(ReadU16(DataOffset + (static_cast<int64>(Index) * 2)));
	}
	return true;
}

UObject* USoundFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled)
{
	(void)InClass;
	(void)Context;
	(void)Type;
	bOutOperationCanceled = false;
	TArray<int16> Samples;
	int32 NumChannels = 0;
	int32 SampleRate = 0;
	FString Error;
	if (!ReadWave(Buffer, BufferEnd - Buffer, Samples, NumChannels, SampleRate, Error))
	{
		UE_LOG(LogLeonEd, Error, "SoundFactory: '%s' is %s", *CurrentFilename, *Error);
		return nullptr;
	}
	USoundWave* Sound = CreateOrOverwriteAsset<USoundWave>(InParent, InName, Flags);
	if (Sound == nullptr)
	{
		return nullptr;
	}
	(void)Sound->SetPCMData(Samples.GetData(), Samples.Num() / NumChannels, NumChannels, SampleRate);
	UpdateAssetImportData(Sound, CurrentFilename);
	Buffer = BufferEnd;
	return Sound;
}

bool USoundFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return FactoryCanReimport(Obj, OutFilenames);
}

void USoundFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	FactorySetReimportPaths(Obj, NewReimportPaths);
}

EReimportResult::Type USoundFactory::Reimport(UObject* Obj)
{
	return FactoryReimport(Obj);
}

void USoundFactory::GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
{
	FactoryGetAdditionalReimportedObjects(OutObjects);
}
