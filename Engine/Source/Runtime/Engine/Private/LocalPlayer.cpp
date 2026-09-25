#include "Engine/LocalPlayer.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

bool ULocalPlayer::SpawnPlayActor(const FString& URL, FString& OutError, UWorld* InWorld)
{
	check(InWorld != nullptr);
	FURL PlayerURL(nullptr, *URL, TRAVEL_Absolute);
	// The player's name and the game's login options (UE).
	const FString PlayerName = GetNickname();
	if (!PlayerName.IsEmpty())
	{
		PlayerURL.AddOption(*FString::Printf(TEXT("Name=%s"), *PlayerName));
	}
	const FString GameUrlOptions = GetGameLoginOptions();
	if (!GameUrlOptions.IsEmpty())
	{
		PlayerURL.AddOption(*GameUrlOptions);
	}
	PlayerController = InWorld->SpawnPlayActor(this, PlayerURL, OutError);
	return PlayerController != nullptr;
}

void ULocalPlayer::PlayerAdded(UGameViewportClient* InViewportClient, int32 InControllerId)
{
	ViewportClient = InViewportClient;
	SetControllerId(InControllerId);
}

void ULocalPlayer::PlayerRemoved()
{
	PlayerController = nullptr;
}

FString ULocalPlayer::GetNickname() const
{
	return FString();
}

FString ULocalPlayer::GetGameLoginOptions() const
{
	return FString();
}

UGameInstance* ULocalPlayer::GetGameInstance() const
{
	return OwningGameInstance;
}

UWorld* ULocalPlayer::GetWorld() const
{
	return OwningGameInstance != nullptr ? OwningGameInstance->GetWorld() : nullptr;
}

bool ULocalPlayer::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// The viewport client (show, the game instance, the engine), then the controller's chain (UE).
	if (ViewportClient != nullptr && ViewportClient->Exec(InWorld, Cmd, Ar))
	{
		return true;
	}
	return Super::Exec(InWorld, Cmd, Ar);
}
