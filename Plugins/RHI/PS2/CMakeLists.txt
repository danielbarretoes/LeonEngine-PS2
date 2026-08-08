# PS2 GS RHI — clear/flip, 2D unlit, 3D lit/textured box, debug HUD.

add_library(leon_rhi_ps2 STATIC
    src/Ps2GsContext.cpp
    src/Ps2RHIDevice.cpp
    src/Ps2SceneState.cpp
    src/Ps2DrawPrimitives.cpp
    src/Ps2Draw3D.cpp
    src/Ps2Texture.cpp
    src/Ps2DebugHud.cpp
)

target_include_directories(leon_rhi_ps2 PUBLIC
    $<BUILD_INTERFACE:${LEON_REPO_ROOT}/Engine/include>
)

target_link_libraries(leon_rhi_ps2 PUBLIC leon_rhi)

if(DEFINED ENV{PS2SDK})
    target_link_libraries(leon_rhi_ps2 PUBLIC draw math3d packet graph dma kernel)
endif()

add_library(Leon::RhiPs2 ALIAS leon_rhi_ps2)
