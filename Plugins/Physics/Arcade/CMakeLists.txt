# Static Arcade physics plugin — PhysScene + collision queries.

add_library(leon_physics_arcade STATIC
    src/ArcadePhysicsBackend.cpp
    src/CollisionQuery.cpp
    src/CollisionShape.cpp
    src/PhysScene.cpp
    src/TriangleCollision.cpp
)

target_include_directories(leon_physics_arcade PUBLIC
    $<BUILD_INTERFACE:${LEON_REPO_ROOT}/Engine/include>
)

target_link_libraries(leon_physics_arcade PUBLIC
    leon_physics_iface
    glm::glm
)

add_library(Leon::Physics_Arcade ALIAS leon_physics_arcade)
