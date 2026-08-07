# Static PS2 (GS) RHI plugin — clear/flip + unlit mesh path.

add_library(leon_rhi_ps2 STATIC
    src/Ps2RHIDevice.cpp
    src/Ps2UnlitMesh.cpp
)

target_include_directories(leon_rhi_ps2 PUBLIC
    $<BUILD_INTERFACE:${LEON_REPO_ROOT}/Engine/include>
)

target_link_libraries(leon_rhi_ps2 PUBLIC leon_rhi)

if(DEFINED ENV{PS2SDK})
    # draw_clear + packet GIF path for visible framebuffer fills.
    target_link_libraries(leon_rhi_ps2 PUBLIC draw packet graph dma kernel)
endif()

add_library(Leon::RhiPs2 ALIAS leon_rhi_ps2)
