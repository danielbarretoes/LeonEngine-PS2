# Shared MSVC / GCC compile options for Leon targets.
# Usage: include(...) then leon_apply_compile_options(tgt1 tgt2 ...)

function(leon_apply_compile_options)
    foreach(tgt IN LISTS ARGN)
        if(TARGET ${tgt})
            if(MSVC)
                target_compile_options(${tgt} PRIVATE
                    /W4
                    /permissive-
                    /Zc:__cplusplus
                    /MP
                    $<$<CONFIG:Debug,RelWithDebInfo>:/FS>
                )
                target_compile_definitions(${tgt} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
            elseif(LEON_PLATFORM_PS2)
                target_compile_options(${tgt} PRIVATE -Wall -Wextra)
            else()
                target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
            endif()
        endif()
    endforeach()
endfunction()

# /MP on third-party STATIC libs (warnings stay local to each dep).
function(leon_enable_msvc_mp)
    foreach(tgt IN LISTS ARGN)
        if(TARGET ${tgt} AND MSVC)
            target_compile_options(${tgt} PRIVATE /MP $<$<CONFIG:Debug,RelWithDebInfo>:/FS>)
        endif()
    endforeach()
endfunction()
