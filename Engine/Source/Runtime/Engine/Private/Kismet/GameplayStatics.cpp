#include "Kismet/GameplayStatics.h"

#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Sound/SoundWave.h"

// The sound, effect and URL option helpers of UGameplayStatics (UE: GameplayStatics.cpp); the traces and the damage
// have files of their own.

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

int32 UGameplayStatics::SpawnImpactMark(const UObject* WorldContextObject, const FVector& Location,
	const FVector& Normal, float Size, const FLinearColor& Color, float LifeSpan)
{
	UWorld* World = GetWorldFromContextObject(WorldContextObject);
	if (World == nullptr)
	{
		return INDEX_NONE;
	}
	FImpactMark Mark;
	Mark.Location = Location;
	Mark.Normal = Normal;
	Mark.Size = Size;
	Mark.Color = Color;
	Mark.LifeSpan = LifeSpan;
	return World->ImpactMarks.AddMark(Mark);
}

void UGameplayStatics::SpawnTracer(const UObject* WorldContextObject, const FVector& Start, const FVector& End,
	const FLinearColor& Color, float Width, float LifeSpan)
{
	UWorld* World = GetWorldFromContextObject(WorldContextObject);
	if (World == nullptr || LifeSpan <= 0.0f)
	{
		return;
	}
	FTracer Tracer;
	Tracer.Start = Start;
	Tracer.End = End;
	Tracer.Color = Color;
	Tracer.Width = Width;
	Tracer.LifeSpan = LifeSpan;
	World->Tracers.AddTracer(Tracer);
}

APointLight* UGameplayStatics::SpawnPointLightAtLocation(const UObject* WorldContextObject, const FVector& Location,
	const FLinearColor& Color, float Intensity, float AttenuationRadius, float LifeSpan)
{
	UWorld* World = GetWorldFromContextObject(WorldContextObject);
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.ObjectFlags |= RF_Transient;
	APointLight* Light = World->SpawnActor<APointLight>(Location, FRotator::ZeroRotator, SpawnInfo);
	if (Light == nullptr)
	{
		return nullptr;
	}
	// The light's proxy takes the new values (the render state is a snapshot).
	UPointLightComponent* LightComponent = Light->GetPointLightComponent();
	LightComponent->LightColor = Color;
	LightComponent->Intensity = Intensity;
	LightComponent->AttenuationRadius = AttenuationRadius;
	LightComponent->CastShadows = false;
	LightComponent->MarkRenderStateDirty();
	Light->SetLifeSpan(LifeSpan);
	return Light;
}
