#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Net/NetProtocol.h"
#include <memory>
#include <type_traits>
#include <utility>


class ACharacter;
class UGameEngine;

/// Drives a possessed Character from player input (Unreal-style APlayerController).
class APlayerController : public AController {
public:
    APlayerController() : playerState_(std::make_unique<APlayerState>()) {}

    using AController::Possess;
    void Possess(ACharacter* character);

    [[nodiscard]] ACharacter* GetCharacter() const { return AController::GetCharacter(); }
    [[nodiscard]] bool HasCharacter() const { return GetCharacter() != nullptr; }

    [[nodiscard]] APlayerState& GetPlayerState() { return *playerState_; }
    [[nodiscard]] const APlayerState& GetPlayerState() const { return *playerState_; }

    /// Replaces owned PlayerState. Caller must GameMode::Logout (or RemovePlayerState) first
    /// so GameState::PlayerArray does not keep a dangling pointer.
    template <typename T, typename... Args>
    T* SetPlayerState(Args&&... args) {
        static_assert(std::is_base_of_v<APlayerState, T>, "T must derive from PlayerState");
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        playerState_ = std::move(owned);
        return raw;
    }

    /// Apply input to the possessed Character. Packs override. Returns wish direction for
    /// debug HUD; default is a no-op.
    virtual glm::vec3 TickInput(UGameEngine& engine);

    /// Unreal-like: drive view from possessed pawn SpringArm (packs override).
    virtual void UpdateCamera(UGameEngine& engine, float deltaTime);

    /// When false, this PC is driven by remote InputCmd (listen-server remote player).
    [[nodiscard]] bool IsLocalController() const { return bLocalController_; }
    void SetIsLocalController(bool local) { bLocalController_ = local; }

    // Flow: Local input → FInputCmdMsg → authority ApplyRemoteInput
    /// Latches current button mask; rising edges vs previous frame go into pressedEdges_.
    void LatchButtons(std::uint16_t pressedNow);
    [[nodiscard]] bool WasButtonPressed(Leon::Net::EInputButton button) const;
    [[nodiscard]] bool IsButtonDown(Leon::Net::EInputButton button) const;
    [[nodiscard]] std::uint16_t GetButtonDownMask() const { return downButtons_; }
    /// Returns rising-edge mask from the last LatchButtons and clears it.
    [[nodiscard]] std::uint16_t ConsumeButtonPressedMask();

private:
    std::unique_ptr<APlayerState> playerState_;
    bool bLocalController_ = true;

    std::uint16_t prevButtons_ = 0;
    std::uint16_t downButtons_ = 0;
    std::uint16_t pressedEdges_ = 0;
};

