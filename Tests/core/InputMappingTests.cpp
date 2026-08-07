#include <GLFW/glfw3.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <leon/core/InputActions.h>
#include <leon/core/InputMapping.h>

using Catch::Matchers::WithinAbs;

TEST_CASE("InputMappingContext MakeDefault binds move and jump", "[core][inputmapping]") {
    const leon::InputMappingContext ctx = leon::InputMappingContext::MakeDefault();

    REQUIRE(ctx.Axes().count(std::string(leon::InputActions::MoveForward)) == 1);
    REQUIRE(ctx.Axes().count(std::string(leon::InputActions::MoveRight)) == 1);
    REQUIRE(ctx.Axes().count(std::string(leon::InputActions::MoveUp)) == 1);
    REQUIRE(ctx.Actions().count(std::string(leon::InputActions::Jump)) == 1);

    const auto& jumpKeys = ctx.Actions().at(std::string(leon::InputActions::Jump));
    REQUIRE_FALSE(jumpKeys.empty());
    REQUIRE(jumpKeys.front() == GLFW_KEY_SPACE);
}

TEST_CASE("InputMappingContext BindAxisKey and BindActionKey", "[core][inputmapping]") {
    leon::InputMappingContext ctx;
    ctx.BindAxisKey("Strafe", GLFW_KEY_A, -1.0f);
    ctx.BindAxisKey("Strafe", GLFW_KEY_D, 1.0f);
    ctx.BindActionKey("Fire", GLFW_KEY_LEFT_CONTROL);
    ctx.BindAxisKey("", GLFW_KEY_W, 1.0f); // ignored
    ctx.BindActionKey("Fire", 0);          // ignored

    REQUIRE(ctx.Axes().at("Strafe").size() == 2);
    REQUIRE(ctx.Actions().at("Fire").size() == 1);
    REQUIRE(ctx.Actions().at("Fire").front() == GLFW_KEY_LEFT_CONTROL);
}

TEST_CASE("PlayerInput ClearContexts empties maps after Update path", "[core][inputmapping]") {
    leon::PlayerInput input;
    input.AddMappingContext(leon::InputMappingContext::MakeDefault());
    input.ClearContexts();
    // Without Window sampling, axis/action queries stay at defaults (0 / false).
    REQUIRE_THAT(input.GetAxisValue(leon::InputActions::MoveForward), WithinAbs(0.0f, 1.0e-6f));
    REQUIRE_FALSE(input.IsActionPressed(leon::InputActions::Jump));
    REQUIRE_FALSE(input.GetMoveAxes2D().any());
}
