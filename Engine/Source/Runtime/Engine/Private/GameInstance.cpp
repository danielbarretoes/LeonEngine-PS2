#include "Engine/GameInstance.h"

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineLogs.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

void FWorldContext::SetCurrentWorld(UWorld* World)
{
	ThisCurrentWorld = World;
	if (World != nullptr)
	{
		World->OwningGameInstance = OwningGameInstance;
	}
}

void UGameInstance::Init()
{
}

void UGameInstance::Shutdown()
{
	// The players leave before the world goes (UE: the local players are removed on shutdown).
	for (int32 Index = LocalPlayers.Num() - 1; Index >= 0; --Index)
	{
		if (ULocalPlayer* Player = LocalPlayers[Index])
		{
			Player->PlayerRemoved();
		}
	}
	LocalPlayers.Empty();
}

void UGameInstance::InitializeStandalone(FName InPackageName, UPackage* InWorldPackage)
{
	WorldContext.WorldType = EWorldType::Game;
	WorldContext.OwningGameInstance = this;
	WorldContext.ContextHandle = FName(TEXT("Context_0"));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, InPackageName, InWorldPackage);
	WorldContext.SetCurrentWorld(World);
}

void UGameInstance::StartGameInstance()
{
	UEngine* const Engine = GetEngine();
	if (Engine == nullptr)
	{
		UE_LOG(LogLoad, Error, TEXT("StartGameInstance: the game instance has no engine"));
		RequestEngineExit("StartGameInstance: no engine");
		return;
	}

	// The map: the first command-line token (UE), else `-map=` (Leon's alias, which the scripts use), else the
	// project's GameDefaultMap. A first token that is the project file is skipped.
	const TCHAR* CommandLine = FCommandLine::Get();
	const FString DefaultMap = UGameMapsSettings::GetGameDefaultMap();
	FString PackageName;
	if (!FParse::Value(CommandLine, TEXT("map="), PackageName) || PackageName.IsEmpty())
	{
		const TCHAR* Stream = CommandLine;
		FString Token = FParse::Token(Stream, false);
		if (Token.EndsWith(TEXT(".lproj")))
		{
			Token = FParse::Token(Stream, false);
		}
		if (Token.IsEmpty() || Token[0] == '-')
		{
			PackageName = DefaultMap + GetDefault<UGameMapsSettings>()->LocalMapOptions;
		}
		else
		{
			PackageName = Token;
		}
	}

	const FURL DefaultURL;
	const FURL URL(&DefaultURL, *PackageName, TRAVEL_Partial);
	FString Error;
	EBrowseReturnVal::Type BrowseRet = EBrowseReturnVal::Failure;
	if (URL.Valid != 0)
	{
		BrowseRet = Engine->Browse(WorldContext, URL, Error);
	}
	if (BrowseRet != EBrowseReturnVal::Success)
	{
		// UE asks whether to open the default map instead; Leon fails, so a script with a wrong map stops (exit 1).
		UE_LOG(LogLoad, Error, TEXT("Failed to enter %s: %s. Please check the log for errors."), *URL.Map, *Error);
		RequestEngineExit("StartGameInstance: failed to enter the startup map");
	}
}

AGameModeBase* UGameInstance::CreateGameModeForURL(FURL InURL, UWorld* InWorld)
{
	FString Options;
	FString GameParam;
	for (const FString& Option : InURL.Op)
	{
		Options += TEXT("?");
		Options += Option;
		(void)FParse::Value(*Option, TEXT("GAME="), GameParam);
	}

	UWorld* World = InWorld != nullptr ? InWorld : GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	const AWorldSettings* Settings = World->GetWorldSettings();

	// The level's game mode (plan decision D18); the URL overrides it.
	TSubclassOf<AGameModeBase> GameClass = Settings != nullptr ? Settings->DefaultGameMode : nullptr;
	if (!GameParam.IsEmpty())
	{
		const FString GameClassName = UGameMapsSettings::GetGameModeForName(GameParam);
		UClass* GameModeParamClass = LoadClass<AGameModeBase>(nullptr, *GameClassName);
		if (GameModeParamClass != nullptr)
		{
			GameClass = GameModeParamClass;
		}
		else
		{
			UE_LOG(LogLoad, Warning, TEXT("Failed to load game mode '%s' specified by URL options."), *GameClassName);
		}
	}

	// Then the map name's prefix, then the project's global default (UE).
	if (GameClass == nullptr)
	{
		const FString PrefixGameMode = UGameMapsSettings::GetGameModeForMapName(FPaths::GetBaseFilename(InURL.Map));
		if (!PrefixGameMode.IsEmpty())
		{
			GameClass = LoadClass<AGameModeBase>(nullptr, *PrefixGameMode);
		}
	}
	if (GameClass == nullptr)
	{
		const FString GlobalDefault = UGameMapsSettings::GetGlobalDefaultGameMode();
		if (!GlobalDefault.IsEmpty())
		{
			GameClass = LoadClass<AGameModeBase>(nullptr, *GlobalDefault);
		}
	}
	if (GameClass == nullptr)
	{
		GameClass = AGameModeBase::StaticClass();
	}
	else
	{
		GameClass = OverrideGameModeClass(GameClass, FPaths::GetBaseFilename(InURL.Map), Options, InURL.Portal);
	}

	// Game modes are never saved into a map (UE).
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.ObjectFlags |= RF_Transient;
	return World->SpawnActor<AGameModeBase>(GameClass, SpawnInfo);
}

TSubclassOf<AGameModeBase> UGameInstance::OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass,
	const FString& /*MapName*/, const FString& /*Options*/, const FString& /*Portal*/) const
{
	return GameModeClass;
}

void UGameInstance::LoadComplete(const float /*LoadTime*/, const FString& /*MapName*/)
{
	NotifyLevelOpened();
}

void UGameInstance::DestroyWorldContextWorld()
{
	UWorld* World = WorldContext.World();
	if (World == nullptr)
	{
		return;
	}
	WorldContext.SetCurrentWorld(nullptr);
	World->DestroyWorld(true);
}

ULocalPlayer* UGameInstance::CreateInitialPlayer(FString& OutError)
{
	return CreateLocalPlayer(0, OutError, false);
}

ULocalPlayer* UGameInstance::CreateLocalPlayer(int32 ControllerId, FString& OutError, bool bSpawnPlayerController)
{
	UEngine* Engine = GetEngine();
	UClass* PlayerClass = Engine != nullptr && Engine->LocalPlayerClass != nullptr ? Engine->LocalPlayerClass.Get()
																				   : ULocalPlayer::StaticClass();
	// UE creates the player inside the engine.
	UObject* Outer = Engine != nullptr ? static_cast<UObject*>(Engine) : static_cast<UObject*>(this);
	ULocalPlayer* NewPlayer = NewObject<ULocalPlayer>(Outer, PlayerClass);
	(void)AddLocalPlayer(NewPlayer, ControllerId);
	UWorld* World = GetWorld();
	if (bSpawnPlayerController && World != nullptr)
	{
		if (!NewPlayer->SpawnPlayActor(WorldContext.LastURL.ToString(true), OutError, World))
		{
			(void)RemoveLocalPlayer(NewPlayer);
			return nullptr;
		}
	}
	return NewPlayer;
}

int32 UGameInstance::AddLocalPlayer(ULocalPlayer* NewPlayer, int32 ControllerId)
{
	if (NewPlayer == nullptr)
	{
		return INDEX_NONE;
	}
	const int32 InsertIndex = LocalPlayers.AddUnique(NewPlayer);
	NewPlayer->OwningGameInstance = this;
	NewPlayer->PlayerAdded(GetGameViewportClient(), ControllerId);
	return InsertIndex;
}

bool UGameInstance::RemoveLocalPlayer(ULocalPlayer* ExistingPlayer)
{
	if (ExistingPlayer == nullptr || !LocalPlayers.Contains(ExistingPlayer))
	{
		return false;
	}
	if (APlayerController* PlayerController = ExistingPlayer->PlayerController)
	{
		PlayerController->Destroy();
	}
	ExistingPlayer->PlayerRemoved();
	LocalPlayers.Remove(ExistingPlayer);
	return true;
}

ULocalPlayer* UGameInstance::GetFirstGamePlayer() const
{
	return LocalPlayers.Num() > 0 ? LocalPlayers[0] : nullptr;
}

APlayerController* UGameInstance::GetFirstLocalPlayerController(const UWorld* World) const
{
	const UWorld* InWorld = World != nullptr ? World : GetWorld();
	for (ULocalPlayer* Player : LocalPlayers)
	{
		if (Player != nullptr)
		{
			if (APlayerController* PlayerController = Player->GetPlayerController(InWorld))
			{
				return PlayerController;
			}
		}
	}
	return nullptr;
}

UEngine* UGameInstance::GetEngine() const
{
	return Cast<UEngine>(GetOuter());
}

bool UGameInstance::Exec(UWorld* /*InWorld*/, const TCHAR* /*Cmd*/, FOutputDevice& /*Ar*/)
{
	return false;
}
