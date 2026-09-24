#include <iostream>
#include "GameFramework/GameplayRouter.h"


void FGameplayRouter::AddMode(std::unique_ptr<AGameModeBase> mode) {
    if (mode) {
        modes_.push_back(std::move(mode));
    }
}

void FGameplayRouter::SetDefaultMode(std::unique_ptr<AGameModeBase> mode) {
    defaultMode_ = std::move(mode);
}

void FGameplayRouter::SyncActiveMode(UGameEngine& engine, const FLevelDirector& director) {
    if (director.IsEmpty()) {
        if (active_ != nullptr) {
            active_->OnExit(engine);
            active_ = nullptr;
        }
        boundCatalogIndex_ = static_cast<std::size_t>(-1);
        return;
    }

    const std::size_t index = director.CurrentIndex();
    if (index == boundCatalogIndex_) {
        return;
    }
    boundCatalogIndex_ = index;

    const FLevelEntry& entry = director.Catalog().Entries()[index];
    const std::string& gameModeId = entry.gameMode;

    // Explicit override / pack soft-match first; otherwise ADefaultGameMode.
    AGameModeBase* next = nullptr;
    for (const auto& mode : modes_) {
        if (mode->Matches(entry, gameModeId)) {
            next = mode.get();
            break;
        }
    }
    if (next == nullptr) {
        next = defaultMode_.get();
    }

    if (active_ == next) {
        // Same mode instance, different level file — reload mode config only.
        if (active_ != nullptr) {
            active_->OnEnter(engine, entry.path);
        }
        return;
    }

    if (active_ != nullptr) {
        active_->OnExit(engine);
    }
    active_ = next;
    if (active_ != nullptr) {
        active_->OnEnter(engine, entry.path);
    }
}

void FGameplayRouter::Update(UGameEngine& engine, const FLevelDirector& director, float deltaTime) {
    SyncActiveMode(engine, director);
    if (active_ != nullptr) {
        active_->Tick(engine, deltaTime);
    }
    // ClientTravel / ServerTravel during Tick may change the catalog index — bind the new
    // GameMode this frame so Lobby/Menu OnEnter (and net callbacks) are not delayed.
    SyncActiveMode(engine, director);
}

