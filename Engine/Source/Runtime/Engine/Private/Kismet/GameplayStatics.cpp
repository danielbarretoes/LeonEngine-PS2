#include "Kismet/GameplayStatics.h"

#include "Engine/Engine.h"
#include "Sound/SoundWave.h"

// The sound helpers of UGameplayStatics (UE: GameplayStatics.cpp); the traces and the damage have files of their own.

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
