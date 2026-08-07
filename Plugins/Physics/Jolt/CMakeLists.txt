# Static Jolt physics plugin — rigid Step + narrow-phase traces behind PhysScene
# (CMC side resolve / QuerySupportY / floor+slope overlays stay Arcade).

add_library(leon_physics_jolt STATIC
    src/JoltPhysicsBackend.cpp
)

target_include_directories(leon_physics_jolt PUBLIC
    $<BUILD_INTERFACE:${LEON_REPO_ROOT}/Engine/include>
)

target_link_libraries(leon_physics_jolt PUBLIC
    leon_physics_iface
    glm::glm
    Jolt
)

target_compile_definitions(leon_physics_jolt PUBLIC LEON_WITH_JOLT=1)

add_library(Leon::Physics_Jolt ALIAS leon_physics_jolt)
