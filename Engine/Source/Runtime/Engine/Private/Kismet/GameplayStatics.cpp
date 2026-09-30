#include "Kismet/GameplayStatics.h"

#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "EngineLogs.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
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

void UGameplayStatics::OpenLevel(const UObject* WorldContextObject, FName LevelName, bool bAbsolute, FString Options)
{
	UWorld* World = GetWorldFromContextObject(WorldContextObject);
	if (World == nullptr || GEngine == nullptr)
	{
		return;
	}
	FString Cmd = LevelName.ToString();
	if (!Options.IsEmpty())
	{
		Cmd += TEXT("?") + Options;
	}
	// UE checks a local map's name here; the travel itself fails later and keeps the world playing.
	const FString MapName = LevelName.ToString();
	if (FPackageName::IsValidLongPackageName(MapName) && !FPackageName::DoesPackageExist(MapName))
	{
		UE_LOG(LogEngine, Warning, TEXT("OpenLevel: the map '%s' does not exist"), *MapName);
	}
	GEngine->SetClientTravel(World, *Cmd, bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative);
}

APlayerController* UGameplayStatics::GetPlayerController(const UObject* WorldContextObject, int32 PlayerIndex)
{
	const UWorld* World = GetWorldFromContextObject(WorldContextObject);
	if (World == nullptr || PlayerIndex < 0)
	{
		return nullptr;
	}
	int32 Index = 0;
	APlayerController* Found = nullptr;
	World->ForEach<APlayerController>(
		[&Index, &Found, PlayerIndex](APlayerController& PlayerController)
		{
			if (Found == nullptr && Index++ == PlayerIndex)
			{
				Found = &PlayerController;
			}
		});
	return Found;
}

bool UGameplayStatics::SetGamePaused(const UObject* WorldContextObject, bool bPaused)
{
	APlayerController* PlayerController = GetPlayerController(WorldContextObject, 0);
	return PlayerController != nullptr && PlayerController->SetPause(bPaused);
}

bool UGameplayStatics::IsGamePaused(const UObject* WorldContextObject)
{
	const UWorld* World = GetWorldFromContextObject(WorldContextObject);
	return World != nullptr && World->IsPaused();
}

void UGameplayStatics::PlaySound2D(const UObject* WorldContextObject, USoundBase* Sound, float VolumeMultiplier)
{
	(void)WorldContextObject;
	USoundWave* Wave = Cast<USoundWave>(Sound);
	if (Wave == nullptr || GEngine == nullptr)
	{
		return;
	}
	// The sound's buffer is resident since its load (FAudioDevice's sound buffers): the play is a voice.
	GEngine->GetAudioDevice().PlaySound2D(Wave->GetSoundBuffer(), VolumeMultiplier, Wave->Priority);
}

void UGameplayStatics::PlaySoundAtLocation(
	const UObject* WorldContextObject, USoundBase* Sound, const FVector& Location, float VolumeMultiplier)
{
	(void)WorldContextObject;
	USoundWave* Wave = Cast<USoundWave>(Sound);
	if (Wave == nullptr || GEngine == nullptr)
	{
		return;
	}
	GEngine->GetAudioDevice().PlaySoundAtLocation(Wave->GetSoundBuffer(), Location, VolumeMultiplier, Wave->Priority);
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

uint32 UGameplayStatics::SpawnEffectSprite(const UObject* WorldContextObject, const FVector& Location, float Size,
	const FLinearColor& Color, float LifeSpan, float FadeInTime, float FadeOutTime)
{
	UWorld* World = GetWorldFromContextObject(WorldContextObject);
	if (World == nullptr)
	{
		return 0;
	}
	FEffectSprite Sprite;
	Sprite.Location = Location;
	Sprite.Size = Size;
	Sprite.Color = Color;
	Sprite.LifeSpan = LifeSpan;
	Sprite.FadeInTime = FadeInTime;
	Sprite.FadeOutTime = FadeOutTime;
	return World->EffectSprites.AddSprite(Sprite);
}

APointLight* UGameplayStatics::SpawnPointLightAtLocation(const UObject* WorldContextObject, const FVector& Location,
	const FLinearColor& Color, float Intensity, float AttenuationRadius, float LifeSpan)
{
	UWorld* World = GetWorldFromContextObject(WorldContextObject);
	APointLight* Light = World != nullptr ? World->AcquirePooledPointLight(Location, LifeSpan) : nullptr;
	if (Light == nullptr)
	{
		return nullptr;
	}
	// The light's proxy takes the new values (the render state is a snapshot): shown now, or made again when it was
	// still lit.
	UPointLightComponent* LightComponent = Light->GetPointLightComponent();
	LightComponent->LightColor = Color;
	LightComponent->Intensity = Intensity;
	LightComponent->AttenuationRadius = AttenuationRadius;
	LightComponent->CastShadows = false;
	if (LightComponent->IsVisible())
	{
		LightComponent->MarkRenderStateDirty();
	}
	else
	{
		LightComponent->SetVisibility(true);
	}
	return Light;
}
