#include "Kismet/GameplayStatics.h"

#include "Engine/Engine.h"
#include "Sound/SoundWave.h"

// The sound and URL option helpers of UGameplayStatics (UE: GameplayStatics.cpp); the traces and the damage have
// files of their own.

namespace
{

	/** Finds Key in `?Key=Value?Other` options: true when present, with its value (empty without one). */
	bool FindOption(const FString& Options, const FString& Key, FString& OutValue)
	{
		TArray<FString> Parts;
		Options.ParseIntoArray(Parts, TEXT("?"), true);
		for (const FString& Part : Parts)
		{
			FString PartKey = Part;
			FString PartValue;
			const int32 Equals = Part.Find(TEXT("="));
			if (Equals != INDEX_NONE)
			{
				PartKey = Part.Left(Equals);
				PartValue = Part.Mid(Equals + 1);
			}
			if (PartKey.TrimStartAndEnd() == Key)
			{
				OutValue = PartValue.TrimStartAndEnd();
				return true;
			}
		}
		return false;
	}

} // namespace

FString UGameplayStatics::ParseOption(const FString& Options, const FString& Key)
{
	FString Value;
	return FindOption(Options, Key, Value) ? Value : FString();
}

bool UGameplayStatics::HasOption(const FString& Options, const FString& Key)
{
	FString Value;
	return FindOption(Options, Key, Value);
}

int32 UGameplayStatics::GetIntOption(const FString& Options, const FString& Key, int32 DefaultValue)
{
	FString Value;
	return FindOption(Options, Key, Value) && !Value.IsEmpty() ? FCString::Atoi(*Value) : DefaultValue;
}

void UGameplayStatics::PlaySound2D(const UObject* WorldContextObject, USoundBase* Sound, float VolumeMultiplier)
{
	(void)WorldContextObject;
	const USoundWave* Wave = Cast<USoundWave>(Sound);
	if (Wave == nullptr || GEngine == nullptr)
	{
		return;
	}
	TArray<int16> Samples;
	GEngine->GetAudioDevice().PlaySound2D(Wave->GetPCMView(Samples), VolumeMultiplier);
}

void UGameplayStatics::PlaySoundAtLocation(
	const UObject* WorldContextObject, USoundBase* Sound, const FVector& Location, float VolumeMultiplier)
{
	(void)WorldContextObject;
	const USoundWave* Wave = Cast<USoundWave>(Sound);
	if (Wave == nullptr || GEngine == nullptr)
	{
		return;
	}
	TArray<int16> Samples;
	GEngine->GetAudioDevice().PlaySoundAtLocation(Wave->GetPCMView(Samples), Location, VolumeMultiplier);
}
