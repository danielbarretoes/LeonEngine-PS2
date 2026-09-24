#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "InputCoreTypes.h"
#include "GameFramework/InputActions.h"
#include "GameFramework/InputMapping.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("InputMappingContext MakeDefault binds move and jump", "[core][inputmapping]") {
    const UInputMappingContext Ctx = UInputMappingContext::MakeDefault();

    REQUIRE(Ctx.GetAxes().count(std::string(Leon::InputActions::MoveForward)) == 1);
    REQUIRE(Ctx.GetAxes().count(std::string(Leon::InputActions::MoveRight)) == 1);
    REQUIRE(Ctx.GetAxes().count(std::string(Leon::InputActions::MoveUp)) == 1);
    REQUIRE(Ctx.GetActions().count(std::string(Leon::InputActions::Jump)) == 1);

    const auto& JumpKeys = Ctx.GetActions().at(std::string(Leon::InputActions::Jump));
    REQUIRE_FALSE(JumpKeys.empty());
    REQUIRE(JumpKeys.front() == ToKeyCode(EKeys::SpaceBar));
}

TEST_CASE("InputMappingContext BindAxisKey and BindActionKey", "[core][inputmapping]") {
    UInputMappingContext Ctx;
    Ctx.BindAxisKey("Strafe", EKeys::A, -1.0f);
    Ctx.BindAxisKey("Strafe", EKeys::D, 1.0f);
    Ctx.BindActionKey("Fire", EKeys::LeftControl);
    Ctx.BindAxisKey("", EKeys::W, 1.0f); // ignored
    Ctx.BindActionKey("Fire", 0);             // ignored

    REQUIRE(Ctx.GetAxes().at("Strafe").size() == 2);
    REQUIRE(Ctx.GetActions().at("Fire").size() == 1);
    REQUIRE(Ctx.GetActions().at("Fire").front() == ToKeyCode(EKeys::LeftControl));
}

TEST_CASE("PlayerInput ClearContexts empties maps after Update path", "[core][inputmapping]") {
    UPlayerInput Input;
    Input.AddMappingContext(UInputMappingContext::MakeDefault());
    Input.ClearContexts();
    REQUIRE_THAT(Input.GetAxisValue(Leon::InputActions::MoveForward), WithinAbs(0.0f, 1.0e-6f));
    REQUIRE_FALSE(Input.IsActionPressed(Leon::InputActions::Jump));
    REQUIRE_FALSE(Input.GetMoveAxes2D().Any());
}
