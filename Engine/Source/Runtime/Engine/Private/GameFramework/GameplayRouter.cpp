#include "GameFramework/GameplayRouter.h"

#include <iostream>

void FGameplayRouter::AddMode(std::unique_ptr<AGameModeBase> Mode)
{
	if (Mode)
	{
		Modes.push_back(std::move(Mode));
	}
}

void FGameplayRouter::SetDefaultMode(std::unique_ptr<AGameModeBase> Mode)
{
	DefaultMode = std::move(Mode);
}

void FGameplayRouter::SyncActiveMode(UGameEngine& Engine, const FLevelDirector& Director)
{
	if (Director.IsEmpty())
	{
		if (Active != nullptr)
		{
			Active->OnExit(Engine);
			Active = nullptr;
		}
		BoundCatalogIndex = static_cast<std::size_t>(-1);
		return;
	}

	const std::size_t Index = Director.GetCurrentIndex();
	if (Index == BoundCatalogIndex)
	{
		return;
	}
	BoundCatalogIndex = Index;

	const FLevelEntry& Entry = Director.GetCatalog().GetEntries()[Index];
	const std::string& GameModeId = Entry.GameMode;

	// Explicit override / pack soft-match first; otherwise ADefaultGameMode.
	AGameModeBase* Next = nullptr;
	for (const auto& Mode : Modes)
	{
		if (Mode->Matches(Entry, GameModeId))
		{
			Next = Mode.get();
			break;
		}
	}
	if (Next == nullptr)
	{
		Next = DefaultMode.get();
	}

	if (Active == Next)
	{
		// Same mode instance, different level file — reload mode config only.
		if (Active != nullptr)
		{
			Active->OnEnter(Engine, Entry.Path);
		}
		return;
	}

	if (Active != nullptr)
	{
		Active->OnExit(Engine);
	}
	Active = Next;
	if (Active != nullptr)
	{
		Active->OnEnter(Engine, Entry.Path);
	}
}

void FGameplayRouter::Update(UGameEngine& Engine, const FLevelDirector& Director, float DeltaTime)
{
	SyncActiveMode(Engine, Director);
	if (Active != nullptr)
	{
		Active->Tick(Engine, DeltaTime);
	}
	// ClientTravel / ServerTravel during Tick may change the catalog index — bind the new
	// GameMode this frame so Lobby/Menu OnEnter (and net callbacks) are not delayed.
	SyncActiveMode(Engine, Director);
}
