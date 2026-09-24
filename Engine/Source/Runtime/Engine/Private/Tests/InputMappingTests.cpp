#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "InputCoreTypes.h"
#include "GameFramework/InputActions.h"
#include "GameFramework/InputMapping.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("InputMappingContext MakeDefault binds move and jump", "[core][inputmapping]") {
    const UInputMappingContext ctx = UInputMappingContext::MakeDefault();

    REQUIRE(ctx.Axes().count(std::string(Leon::InputActions::MoveForward)) == 1);
    REQUIRE(ctx.Axes().count(std::string(Leon::InputActions::MoveRight)) == 1);
    REQUIRE(ctx.Axes().count(std::string(Leon::InputActions::MoveUp)) == 1);
    REQUIRE(ctx.Actions().count(std::string(Leon::InputActions::Jump)) == 1);

    const auto& jumpKeys = ctx.Actions().at(std::string(Leon::InputActions::Jump));
    REQUIRE_FALSE(jumpKeys.empty());
    REQUIRE(jumpKeys.front() == ToKeyCode(EKeys::SpaceBar));
}

TEST_CASE("InputMappingContext BindAxisKey and BindActionKey", "[core][inputmapping]") {
    UInputMappingContext ctx;
    ctx.BindAxisKey("Strafe", EKeys::A, -1.0f);
    ctx.BindAxisKey("Strafe", EKeys::D, 1.0f);
    ctx.BindActionKey("Fire", EKeys::LeftControl);
    ctx.BindAxisKey("", EKeys::W, 1.0f); // ignored
    ctx.BindActionKey("Fire", 0);             // ignored

    REQUIRE(ctx.Axes().at("Strafe").size() == 2);
    REQUIRE(ctx.Actions().at("Fire").size() == 1);
    REQUIRE(ctx.Actions().at("Fire").front() == ToKeyCode(EKeys::LeftControl));
}

TEST_CASE("PlayerInput ClearContexts empties maps after Update path", "[core][inputmapping]") {
    UPlayerInput input;
    input.AddMappingContext(UInputMappingContext::MakeDefault());
    input.ClearContexts();
    REQUIRE_THAT(input.GetAxisValue(Leon::InputActions::MoveForward), WithinAbs(0.0f, 1.0e-6f));
    REQUIRE_FALSE(input.IsActionPressed(Leon::InputActions::Jump));
    REQUIRE_FALSE(input.GetMoveAxes2D().any());
}
