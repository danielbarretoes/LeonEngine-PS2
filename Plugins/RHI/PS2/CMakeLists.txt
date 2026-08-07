# PS2 GS RHI — clear/flip + unlit 2D primitives.

add_library(leon_rhi_ps2 STATIC
    src/Ps2GsContext.cpp
    src/Ps2RHIDevice.cpp
    src/Ps2DrawPrimitives.cpp
)

target_include_directories(leon_rhi_ps2 PUBLIC
    $<BUILD_INTERFACE:${LEON_REPO_ROOT}/Engine/include>
)

target_link_libraries(leon_rhi_ps2 PUBLIC leon_rhi)

if(DEFINED ENV{PS2SDK})
    target_link_libraries(leon_rhi_ps2 PUBLIC draw packet graph dma kernel)
endif()

add_library(Leon::RhiPs2 ALIAS leon_rhi_ps2)
