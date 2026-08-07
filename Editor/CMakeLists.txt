# Leon Editor — configure from this directory:
#   cmake -S Editor -B Editor/build
#   Scripts\build.bat
#   Scripts\build-fast.bat   (Ninja, tests OFF — incremental day-to-day)

cmake_minimum_required(VERSION 3.20)
project(LeonEditor VERSION 0.10.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CONFIGURATION_TYPES)
    foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${_cfg}" _cfg_up)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_up} "${CMAKE_BINARY_DIR}/${_cfg}")
    endforeach()
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

get_filename_component(LEON_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

option(LEON_BUILD_TESTS "Build Catch2 unit tests" ON)
option(LEON_BUILD_CLIENT "Build Editor UI (ImGui)" ON)
option(LEON_WITH_JOLT "Fetch and build Jolt Physics backend" ON)

set(LEON_ENGINE_ASSETS "${LEON_REPO_ROOT}/Engine/Assets" CACHE PATH "Engine runtime assets root")
set(LEON_PROJECTS_ROOT "${LEON_REPO_ROOT}/Projects" CACHE PATH "Projects root")

include("${LEON_REPO_ROOT}/Build/Dependencies.cmake")
include("${LEON_REPO_ROOT}/Build/LeonCompileOptions.cmake")

if(LEON_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory("${LEON_REPO_ROOT}/Engine" "${CMAKE_BINARY_DIR}/_leon_engine")
add_subdirectory("${LEON_REPO_ROOT}/Runtime" "${CMAKE_BINARY_DIR}/_leon_runtime")

add_library(leon_editor STATIC
    Application/EditorApp.cpp
    Application/EditorBuild.cpp
    Application/EditorToast.cpp
    Application/EditorLayout.cpp
    Application/EditorOutputLog.cpp
    Application/EditorProject.cpp
    Application/EditorTheme.cpp
    Gizmos/TransformGizmo.cpp
    Gizmos/EditorViewportTarget.cpp
    SceneEditing/LevelSaver.cpp
    SceneEditing/EditorLevelFactory.cpp
    SceneEditing/EngineContent.cpp
    SceneEditing/EditorCatalogs.cpp
    SceneEditing/EditorHistory.cpp
    SceneEditing/EditorCommands.cpp
    SceneEditing/LightmapBaker.cpp
    SceneEditing/AssetTools.cpp
    Gameplay/PieGameMode.cpp
    Gameplay/PiePackRegistry.cpp
    Importers/AssetImport.cpp
    Importers/EditorFileDialog.cpp
    Importers/ImportDialog.cpp
    Preview/AssetPreviewPanel.cpp
    Preview/MaterialSpherePreview.cpp
    Panels/ContentBrowserPanel.cpp
    Panels/DetailsPanel.cpp
    Panels/OutlinerPanel.cpp
    Panels/ToolbarPanel.cpp
    Panels/ViewportPanel.cpp
    Panels/WelcomePanel.cpp
    Panels/WorldSettingsPanel.cpp
    Panels/ProjectSettingsPanel.cpp
    Panels/OutputLogPanel.cpp
    Panels/ConsolePanel.cpp
    Panels/MaterialEditorPanel.cpp
)

target_include_directories(leon_editor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${LEON_REPO_ROOT}/Engine/include>
)

target_link_libraries(leon_editor PUBLIC
    leon_engine
    leon_import
    leon_runtime
    imgui
    imguizmo
)
target_compile_definitions(leon_editor PUBLIC LEON_ENGINE_VERSION=\"${PROJECT_VERSION}\")
if(WIN32)
    target_link_libraries(leon_editor PUBLIC comdlg32 ole32)
    # Source undefs WIN32_LEAN_AND_MEAN before windows.h/commdlg (no /U — avoids D9025).
    set_source_files_properties(Importers/EditorFileDialog.cpp PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
    )
endif()

target_precompile_headers(leon_editor PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
)

add_executable(leon-editor
    Application/main.cpp
)
target_link_libraries(leon-editor PRIVATE leon_editor)
# Product binary name (CMake target stays leon-editor for scripts / deps).
set_target_properties(leon-editor PROPERTIES OUTPUT_NAME LeonEngine)

# Windows: embed .ico so Explorer / taskbar show Leon before GLFW sets the live icon.
if(WIN32)
    set(LEON_EDITOR_ICO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/LeonEditor.ico")
    file(TO_CMAKE_PATH "${LEON_EDITOR_ICO_SRC}" LEON_EDITOR_ICO)
    set(LEON_EDITOR_RC "${CMAKE_CURRENT_BINARY_DIR}/LeonEditor.rc")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/LeonEditor.rc.in"
        "${LEON_EDITOR_RC}"
        @ONLY
    )
    target_sources(leon-editor PRIVATE "${LEON_EDITOR_RC}")
    # Relink when the .ico changes (configure_file alone does not track binary deps).
    set_source_files_properties("${LEON_EDITOR_RC}" PROPERTIES OBJECT_DEPENDS "${LEON_EDITOR_ICO_SRC}")
endif()

# Sync assets / project packs only when file content differs (avoids ~30MB recopy every link).
set(_leon_sync "${LEON_REPO_ROOT}/Build/SyncDirectory.cmake")
set(_leon_editor_copy_cmds
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${LEON_ENGINE_ASSETS}
        -DDST=$<TARGET_FILE_DIR:leon-editor>/assets
        -DREMOVE_ORPHANS=ON
        -P ${_leon_sync}
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:leon-editor>/assets/brand
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:leon-editor>/assets/icons
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Brand/LeonLogoUi.png
        $<TARGET_FILE_DIR:leon-editor>/assets/brand/LeonLogoUi.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/LeonEditor.png
        $<TARGET_FILE_DIR:leon-editor>/assets/icons/LeonEditor.png
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:leon-editor>/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/Inter-Regular.ttf
        $<TARGET_FILE_DIR:leon-editor>/assets/fonts/Inter-Regular.ttf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/Inter-Medium.ttf
        $<TARGET_FILE_DIR:leon-editor>/assets/fonts/Inter-Medium.ttf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/Inter-Bold.ttf
        $<TARGET_FILE_DIR:leon-editor>/assets/fonts/Inter-Bold.ttf
)
# ThirdPerson packs (Bot mesh/Anims/mats) live in the template, not Engine.
if(EXISTS "${LEON_REPO_ROOT}/Templates/ThirdPerson/Content/assets")
    list(APPEND _leon_editor_copy_cmds
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${LEON_REPO_ROOT}/Templates/ThirdPerson/Content/assets
            -DDST=$<TARGET_FILE_DIR:leon-editor>/assets
            -P ${_leon_sync}
    )
elseif(EXISTS "${LEON_REPO_ROOT}/Templates/ThirdPerson/assets")
    list(APPEND _leon_editor_copy_cmds
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${LEON_REPO_ROOT}/Templates/ThirdPerson/assets
            -DDST=$<TARGET_FILE_DIR:leon-editor>/assets
            -P ${_leon_sync}
    )
endif()
if(EXISTS "${LEON_REPO_ROOT}/Templates")
    list(APPEND _leon_editor_copy_cmds
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${LEON_REPO_ROOT}/Templates
            -DDST=$<TARGET_FILE_DIR:leon-editor>/Templates
            -P ${_leon_sync}
    )
endif()
file(GLOB _leon_project_markers "${LEON_PROJECTS_ROOT}/*/leon.game.json")
foreach(_marker IN LISTS _leon_project_markers)
    get_filename_component(_pack_dir "${_marker}" DIRECTORY)
    get_filename_component(_pack_name "${_pack_dir}" NAME)
    list(APPEND _leon_editor_copy_cmds
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:leon-editor>/Projects/${_pack_name}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${_pack_dir}/leon.game.json
            $<TARGET_FILE_DIR:leon-editor>/Projects/${_pack_name}/leon.game.json
    )
    if(EXISTS "${_pack_dir}/Content/levels")
        list(APPEND _leon_editor_copy_cmds
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${_pack_dir}/Content/levels
                -DDST=$<TARGET_FILE_DIR:leon-editor>/Projects/${_pack_name}/Content/levels
                -P ${_leon_sync}
        )
    elseif(EXISTS "${_pack_dir}/levels")
        list(APPEND _leon_editor_copy_cmds
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${_pack_dir}/levels
                -DDST=$<TARGET_FILE_DIR:leon-editor>/Projects/${_pack_name}/levels
                -P ${_leon_sync}
        )
    endif()
    if(EXISTS "${_pack_dir}/Content/materials")
        list(APPEND _leon_editor_copy_cmds
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${_pack_dir}/Content/materials
                -DDST=$<TARGET_FILE_DIR:leon-editor>/Projects/${_pack_name}/Content/materials
                -P ${_leon_sync}
        )
    elseif(EXISTS "${_pack_dir}/materials")
        list(APPEND _leon_editor_copy_cmds
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${_pack_dir}/materials
                -DDST=$<TARGET_FILE_DIR:leon-editor>/Projects/${_pack_name}/materials
                -P ${_leon_sync}
        )
    endif()
    if(EXISTS "${_pack_dir}/Content/assets")
        list(APPEND _leon_editor_copy_cmds
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${_pack_dir}/Content/assets
                -DDST=$<TARGET_FILE_DIR:leon-editor>/Projects/${_pack_name}/Content/assets
                -P ${_leon_sync}
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${_pack_dir}/Content/assets
                -DDST=$<TARGET_FILE_DIR:leon-editor>/assets
                -P ${_leon_sync}
        )
    elseif(EXISTS "${_pack_dir}/assets")
        list(APPEND _leon_editor_copy_cmds
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${_pack_dir}/assets
                -DDST=$<TARGET_FILE_DIR:leon-editor>/Projects/${_pack_name}/assets
                -P ${_leon_sync}
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${_pack_dir}/assets
                -DDST=$<TARGET_FILE_DIR:leon-editor>/assets
                -P ${_leon_sync}
        )
    endif()
endforeach()
add_custom_command(TARGET leon-editor POST_BUILD ${_leon_editor_copy_cmds})

leon_apply_compile_options(leon_editor leon-editor)

if(LEON_BUILD_TESTS)
    add_subdirectory("${LEON_REPO_ROOT}/Tests" "${CMAKE_BINARY_DIR}/_leon_tests")
endif()
