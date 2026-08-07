# Leon unit tests (Catch2). Requires LEON_BUILD_TESTS=ON at configure time.

add_executable(leon_tests
    animation/AnimInstanceTests.cpp
    animation/BlendSpace1DTests.cpp
    animation/FbxLoadTests.cpp
    animation/SkeletonTests.cpp
    content/ContentValidatorTests.cpp
    content/CookedSkeletalTests.cpp
    core/AsciiTests.cpp
    core/CameraTests.cpp
    core/InputMappingTests.cpp
    core/InputMoveTests.cpp
    core/PathsTests.cpp
    core/TransformTests.cpp
    debug/DebugDrawTests.cpp
    engine/EngineFlagsTests.cpp
    gameplay/CharacterMovementTests.cpp
    gameplay/FrameworkHardeningTests.cpp
    gameplay/GameplayTests.cpp
    gameplay/TravelAndEditorSmokeTests.cpp
    level/LevelBasicTests.cpp
    level/LevelCatalogTests.cpp
    net/NetDriverTests.cpp
    physics/CollisionShapeTests.cpp
    physics/CollisionQueryTests.cpp
    physics/TraceDebugDrawTests.cpp
    physics/PhysSceneTests.cpp
    physics/TriangleMeshCollisionTests.cpp
    physics/JoltPhysicsTests.cpp
    render/FrustumTests.cpp
    render/MaterialTests.cpp
    render/MeshDataTests.cpp
    render/PrimitivesTests.cpp
)

target_link_libraries(leon_tests PRIVATE
    leon_engine
    leon_import
    Catch2::Catch2WithMain
)

# Repo root so tests can resolve Engine/Assets, Templates/, and Projects/.
get_filename_component(_leon_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
string(REPLACE "\\" "/" LEON_SRC_DIR_FOR_TESTS "${_leon_repo_root}")
target_compile_definitions(leon_tests PRIVATE "LEON_SOURCE_DIR=\"${LEON_SRC_DIR_FOR_TESTS}\"")

if(MSVC)
    target_compile_options(leon_tests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    target_compile_definitions(leon_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(leon_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(leon_tests
    WORKING_DIRECTORY "${_leon_repo_root}"
    TEST_PREFIX "leon."
)
