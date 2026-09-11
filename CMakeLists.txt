cmake_minimum_required(VERSION 3.23)

project(
    TrueGaze
    VERSION 1.0.0
    DESCRIPTION "True Gaze - Biological NPC Gaze & Kinematics Engine for Skyrim SE/AE/VR (An HCEP Product)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_SKYRIM_SE "Target Skyrim Special Edition (1.5.97)" ON)
option(BUILD_SKYRIM_AE "Target Skyrim Anniversary Edition (1.6+)" ON)
option(BUILD_SKYRIM_VR "Target Skyrim VR" OFF)

# CommonLibSSE-NG setup
# CommonLibSSE-NG can be included as a submodule in extern/CommonLibSSE-NG or via vcpkg/FetchContent
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/CMakeLists.txt")
    add_subdirectory(extern/CommonLibSSE-NG)
endif()

# Source files
file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# SKSE Plugin target
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Precompiled headers
target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h
)

# Compiler warnings and options
if(MSVC)
    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
            /W4
            /permissive-
            /Zc:preprocessor
            /EHsc
    )
endif()

# Link dependencies
if(TARGET CommonLibSSE::CommonLibSSE)
    target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)
endif()

# Output configuration: TrueGaze.dll
set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
        OUTPUT_NAME "TrueGaze"
        PREFIX ""
        SUFFIX ".dll"
)

message(STATUS "[TrueGaze] Configured successfully for Kirk LaSalle's HCEP Biological Oculomotor Engine.")
