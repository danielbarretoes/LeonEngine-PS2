#pragma once

#include <string_view>

namespace leon::InputActions {

/// Digital / axis action names used by the default mapping context.
inline constexpr std::string_view MoveForward = "MoveForward";
inline constexpr std::string_view MoveRight = "MoveRight";
inline constexpr std::string_view MoveUp = "MoveUp";
inline constexpr std::string_view Jump = "Jump";

} // namespace leon::InputActions
