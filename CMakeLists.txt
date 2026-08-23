cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED ENV{COMMONLIB_SSE_FOLDER})
    message(FATAL_ERROR "Missing COMMONLIB_SSE_FOLDER environment variable")
endif()

set(AUTHOR_NAME "Legendman")
set(PRODUCT_NAME "MCMMemory")
set(BEAUTIFUL_NAME "MCM Memory")

project(${PRODUCT_NAME} VERSION 1.0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory("$ENV{COMMONLIB_SSE_FOLDER}" ${CMAKE_CURRENT_BINARY_DIR}/_deps/clib-build)
get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)

file(GLOB_RECURSE headers CONFIGURE_DEPENDS include/*.h include/*.hpp)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cpp)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${headers} ${sources})

include(${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    AUTHOR ${AUTHOR_NAME}
    SOURCES ${headers} ${sources}
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE include/plugin.hpp)
target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE
        BEAUTIFUL_NAME="${BEAUTIFUL_NAME}"
        NOMINMAX
        PRODUCT_NAME="${PRODUCT_NAME}"
        WIN32_LEAN_AND_MEAN
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/DEBUG:FULL>")
endif()

if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(MOD_OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${BEAUTIFUL_NAME}")
    set(PLUGIN_OUTPUT_FOLDER "${MOD_OUTPUT_FOLDER}/SKSE/Plugins")
    set(PLUGIN_CONFIG_OUTPUT_FOLDER "${PLUGIN_OUTPUT_FOLDER}/${PRODUCT_NAME}")
    set(LEGACY_PLUGIN_OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/MCM Settings Loader/SKSE/Plugins")

    message(STATUS "Vortex staging output: ${PLUGIN_OUTPUT_FOLDER}")
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_OUTPUT_FOLDER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_CONFIG_OUTPUT_FOLDER}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${LEGACY_PLUGIN_OUTPUT_FOLDER}/MCMSettingsLoader.dll"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${LEGACY_PLUGIN_OUTPUT_FOLDER}/MCMSettingsLoader.pdb"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${PLUGIN_OUTPUT_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets/Profile.example.json" "${PLUGIN_CONFIG_OUTPUT_FOLDER}/Profile.example.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets/Settings.example.json" "${PLUGIN_CONFIG_OUTPUT_FOLDER}/Settings.example.json"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    if(MSVC)
        add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${PLUGIN_OUTPUT_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
endif()
