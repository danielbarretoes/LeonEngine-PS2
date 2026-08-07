# Static OpenGL RHI plugin — owns GPU resource / Renderer implementations + RHI device.

add_library(leon_rhi_opengl STATIC
    src/OpenGLDevice.cpp
    src/DebugDraw.cpp
    src/DebugOverlay.cpp
    src/EnvMap.cpp
    src/GpuPassTimer.cpp
    src/LdrColorTarget.cpp
    src/PlanarReflection.cpp
    src/Renderer.cpp
    src/ResourceCache.cpp
    src/SceneColorTarget.cpp
    src/Shader.cpp
    src/ShadowMap.cpp
    src/SkeletalMesh.cpp
    src/SsaoTarget.cpp
    src/StaticMesh.cpp
    src/Texture.cpp
    src/UniformBuffer.cpp
)

target_include_directories(leon_rhi_opengl PUBLIC
    $<BUILD_INTERFACE:${LEON_REPO_ROOT}/Engine/include>
    $<BUILD_INTERFACE:${LEON_REPO_ROOT}/ThirdParty/stb>
)

# Static lib: leave Engine symbols unresolved until the final exe links leon_engine_core.
target_link_libraries(leon_rhi_opengl PUBLIC
    leon_rhi
    glad
    glm::glm
    nlohmann_json::nlohmann_json
)

add_library(Leon::RHI_OpenGL ALIAS leon_rhi_opengl)
