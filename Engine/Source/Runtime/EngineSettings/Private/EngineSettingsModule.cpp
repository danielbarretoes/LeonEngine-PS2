#include "GameMapsSettings.h"
#include "GeneralProjectSettings.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, EngineSettings)

UGameMapsSettings::UGameMapsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UGameMapsSettings::GetGameDefaultMap()
{
	return GetDefault<UGameMapsSettings>()->GameDefaultMap.GetLongPackageName();
}

FString UGameMapsSettings::GetGlobalDefaultGameMode()
{
	return GetDefault<UGameMapsSettings>()->GlobalDefaultGameMode.ToString();
}

FString UGameMapsSettings::GetGameModeForName(const FString& GameModeName)
{
	for (const FGameModeName& Alias : GetDefault<UGameMapsSettings>()->GameModeClassAliases)
	{
		if (Alias.Name == GameModeName)
		{
			return Alias.GameMode.ToString();
		}
	}
	return GameModeName;
}

FString UGameMapsSettings::GetGameModeForMapName(const FString& MapName)
{
	for (const FGameModeName& Prefix : GetDefault<UGameMapsSettings>()->GameModeMapPrefixes)
	{
		if (!Prefix.Name.IsEmpty() && MapName.StartsWith(Prefix.Name))
		{
			return Prefix.GameMode.ToString();
		}
	}
	return FString();
}

void UGameMapsSettings::SetGameDefaultMap(const FString& NewMap)
{
	GetMutableDefault<UGameMapsSettings>()->GameDefaultMap = FSoftObjectPath(NewMap);
}

void UGameMapsSettings::SetGlobalDefaultGameMode(const FString& NewGameMode)
{
	GetMutableDefault<UGameMapsSettings>()->GlobalDefaultGameMode = FSoftClassPath(NewGameMode);
}

UGeneralProjectSettings::UGeneralProjectSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
