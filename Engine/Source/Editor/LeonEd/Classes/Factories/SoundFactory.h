#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "SoundFactory.generated.h"

/**
 * Imports `.wav` files as USoundWave (UE: USoundFactory): RIFF / WAVE with 16-bit PCM samples (plain or
 * WAVE_FORMAT_EXTENSIBLE), any channel count and rate; anything else is refused with an error. The samples are kept
 * as they are (Leon: no resampling, no compression; UE keeps the file as the source and cooks compressed audio).
 */
UCLASS()
class LEONED_API USoundFactory
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	USoundFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context,
		const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled) override;

	/** Reads the PCM16 samples of a RIFF / WAVE file; false with an error for anything else (Leon). */
	static bool ReadWave(const uint8* Buffer, int64 Size, TArray<int16>& OutSamples, int32& OutChannels,
		int32& OutSampleRate, FString& OutError);

	// FReimportHandler
	bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	EReimportResult::Type Reimport(UObject* Obj) override;
	void GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const override;
};
