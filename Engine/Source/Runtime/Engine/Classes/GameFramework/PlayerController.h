#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <leon/gameplay/Controller.h>
#include <leon/gameplay/PlayerState.h>
#include <leon/net/NetProtocol.h>
#include <memory>
#include <type_traits>
#include <utility>

namespace leon {

class Character;
class Engine;

/// Drives a possessed Character from player input (Unreal-style PlayerController).
class PlayerController : public Controller {
public:
    PlayerController() : playerState_(std::make_unique<PlayerState>()) {}

    using Controller::Possess;
    void Possess(Character* character);

    [[nodiscard]] Character* GetCharacter() const { return Controller::GetCharacter(); }
    [[nodiscard]] bool HasCharacter() const { return GetCharacter() != nullptr; }

    [[nodiscard]] PlayerState& GetPlayerState() { return *playerState_; }
    [[nodiscard]] const PlayerState& GetPlayerState() const { return *playerState_; }

    /// Replaces owned PlayerState. Caller must GameMode::Logout (or RemovePlayerState) first
    /// so GameState::PlayerArray does not keep a dangling pointer.
    template <typename T, typename... Args>
    T* SetPlayerState(Args&&... args) {
        static_assert(std::is_base_of_v<PlayerState, T>, "T must derive from leon::PlayerState");
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        playerState_ = std::move(owned);
        return raw;
    }

    /// Apply input to the possessed Character. Packs override. Returns wish direction for
    /// debug HUD; default is a no-op.
    virtual glm::vec3 TickInput(Engine& engine);

    /// Unreal-like: drive view from possessed pawn SpringArm (packs override).
    virtual void UpdateCamera(Engine& engine, float deltaTime);

    /// When false, this PC is driven by remote InputCmd (listen-server remote player).
    [[nodiscard]] bool IsLocalController() const { return bLocalController_; }
    void SetIsLocalController(bool local) { bLocalController_ = local; }

    // Flow: Local input → InputCmdMsg → authority ApplyRemoteInput
    /// Latches current button mask; rising edges vs previous frame go into pressedEdges_.
    void LatchButtons(std::uint16_t pressedNow);
    [[nodiscard]] bool WasButtonPressed(net::EInputButton button) const;
    [[nodiscard]] bool IsButtonDown(net::EInputButton button) const;
    [[nodiscard]] std::uint16_t GetButtonDownMask() const { return downButtons_; }
    /// Returns rising-edge mask from the last LatchButtons and clears it.
    [[nodiscard]] std::uint16_t ConsumeButtonPressedMask();

private:
    std::unique_ptr<PlayerState> playerState_;
    bool bLocalController_ = true;

    std::uint16_t prevButtons_ = 0;
    std::uint16_t downButtons_ = 0;
    std::uint16_t pressedEdges_ = 0;
};

} // namespace leon
