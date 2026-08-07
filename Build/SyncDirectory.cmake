# Build-time directory sync: copy files only when content differs.
# Invoke: cmake -DSRC=... -DDST=... -P Build/SyncDirectory.cmake
#
# Optional: -DREMOVE_ORPHANS=ON deletes files in DST that are not in SRC.

if(NOT DEFINED SRC OR NOT DEFINED DST)
    message(FATAL_ERROR "SyncDirectory.cmake requires -DSRC= and -DDST=")
endif()

if(NOT EXISTS "${SRC}")
    message(FATAL_ERROR "SyncDirectory: SRC does not exist: ${SRC}")
endif()

file(MAKE_DIRECTORY "${DST}")

file(GLOB_RECURSE _leon_src_files LIST_DIRECTORIES false RELATIVE "${SRC}" "${SRC}/*")
foreach(_f IN LISTS _leon_src_files)
    set(_src_file "${SRC}/${_f}")
    set(_dst_file "${DST}/${_f}")
    get_filename_component(_dst_dir "${_dst_file}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dst_dir}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_src_file}" "${_dst_file}")
endforeach()

if(REMOVE_ORPHANS)
    file(GLOB_RECURSE _leon_dst_files LIST_DIRECTORIES false RELATIVE "${DST}" "${DST}/*")
    foreach(_f IN LISTS _leon_dst_files)
        if(NOT EXISTS "${SRC}/${_f}")
            file(REMOVE "${DST}/${_f}")
        endif()
    endforeach()
endif()
