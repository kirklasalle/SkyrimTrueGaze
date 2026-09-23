cmake_minimum_required(VERSION 3.23)

project(
    TrueGaze
    VERSION 1.0.4
    DESCRIPTION "True Gaze - Biological NPC Gaze & Kinematics Engine for Skyrim SE/AE/VR (An HCEP Product)"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Build modes
# ---------------------------------------------------------------------------
#
# TRUEGAZE_STANDALONE builds only the platform-agnostic kinematics library and
# its tests. It does NOT produce a loadable Skyrim plugin. It exists so that the
# mathematical core can be unit-tested on a machine with no game toolchain.
#
# The default build REQUIRES CommonLibSSE-NG. This is deliberate. The original
# build guarded the SDK with `if(EXISTS ...)`, which meant a checkout without the
# submodule produced a valid-looking DLL containing no game-facing code at all.
# The build succeeded, CI stayed green, and the plugin silently did nothing.
# See docs/AUDIT_REPORT_2026-09-11.md finding C-2.
#
option(TRUEGAZE_STANDALONE "Build only the standalone kinematics library (no SKSE plugin)" OFF)
option(TRUEGAZE_BUILD_TESTS "Build the standalone test executables" ON)

# CommonLibSSE-NG v7.5.4 requires C++23.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_SKYRIM_SE "Target Skyrim Special Edition (1.5.97)" ON)
option(BUILD_SKYRIM_AE "Target Skyrim Anniversary Edition (1.6+)" ON)
option(BUILD_SKYRIM_VR "Target Skyrim VR" ON)

if(NOT BUILD_SKYRIM_SE AND NOT BUILD_SKYRIM_AE AND NOT BUILD_SKYRIM_VR)
    message(FATAL_ERROR "At least one Skyrim runtime must be enabled.")
endif()

# ---------------------------------------------------------------------------
# Platform-agnostic kinematics core
# ---------------------------------------------------------------------------
add_library(TrueGazeKinematics INTERFACE)
target_include_directories(TrueGazeKinematics INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(TrueGazeKinematics INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(TrueGazeKinematics INTERFACE /W4 /permissive- /Zc:preprocessor /EHsc)
endif()

# ---------------------------------------------------------------------------
# Standalone mode: tests only, no SDK required
# ---------------------------------------------------------------------------
if(TRUEGAZE_STANDALONE)
    message(STATUS "[TrueGaze] STANDALONE mode - building kinematics library and tests only.")

    if(TRUEGAZE_BUILD_TESTS)
        add_executable(KinematicsTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/KinematicsTests.cpp")
        target_link_libraries(KinematicsTests PRIVATE TrueGazeKinematics)
        set_target_properties(KinematicsTests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

        # The bridge mock exercises NamedPipeServer directly, so it needs the
        # implementation and the precompiled header the server expects.
        add_executable(HcepBridgeClientMock
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/HcepBridgeClientMock.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/Bridge/NamedPipeServer.cpp"
        )
        target_include_directories(HcepBridgeClientMock PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_precompile_headers(HcepBridgeClientMock PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")
        target_link_libraries(HcepBridgeClientMock PRIVATE TrueGazeKinematics)
        set_target_properties(HcepBridgeClientMock PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    endif()

    message(STATUS "[TrueGaze] Standalone configuration complete.")
    return()
endif()

# ---------------------------------------------------------------------------
# SKSE plugin build: CommonLibSSE-NG is mandatory
# ---------------------------------------------------------------------------
set(COMMONLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")

if(NOT EXISTS "${COMMONLIB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "\n"
        "  CommonLibSSE-NG is missing.\n"
        "\n"
        "  Expected at: ${COMMONLIB_DIR}\n"
        "\n"
        "  Bootstrap it with:\n"
        "      git submodule update --init --recursive\n"
        "\n"
        "  Or build the kinematics library alone (no Skyrim plugin):\n"
        "      cmake -DTRUEGAZE_STANDALONE=ON ...\n"
        "\n"
        "  This build refuses to continue because a plugin compiled without the\n"
        "  SDK contains no game-facing code and would silently do nothing.\n"
        "  See docs/AUDIT_REPORT_2026-09-11.md finding C-2.\n")
endif()

set(BUILD_TESTS OFF CACHE BOOL "CommonLibSSE-NG tests" FORCE)
set(ENABLE_SKYRIM_SE ${BUILD_SKYRIM_SE} CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ${BUILD_SKYRIM_AE} CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ${BUILD_SKYRIM_VR} CACHE BOOL "" FORCE)

add_subdirectory("${COMMONLIB_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/CommonLibSSE")

if(NOT TARGET CommonLibSSE::CommonLibSSE)
    message(FATAL_ERROR
        "CommonLibSSE-NG was added but did not produce the CommonLibSSE::CommonLibSSE "
        "target. Check the vcpkg dependencies (spdlog, fmt, directxtk, xbyak, ...) are "
        "installed for the x64-windows-static-md triplet.")
endif()

# ---------------------------------------------------------------------------
# TrueGaze plugin
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_precompile_headers(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")

if(MSVC)
    # /Zc:__cplusplus makes the __cplusplus macro report the true standard version,
    # which CommonLibSSE-NG relies on.
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /EHsc
        $<$<CONFIG:Release>:/Zi>)
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)

# Defines the runtime set for RE:: / REL:: feature gating.
if(BUILD_SKYRIM_SE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_SKYRIM_SE=1)
endif()
if(BUILD_SKYRIM_AE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_SKYRIM_AE=1)
endif()
if(BUILD_SKYRIM_VR)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_SKYRIM_VR=1)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "TrueGaze"
    PREFIX ""
    SUFFIX ".dll"
)

# ---------------------------------------------------------------------------
# Keep the packaged copy in step with the build.
#
# skyrim/SKSE/Plugins/TrueGaze.dll is what gets zipped and handed to users, and
# it has silently gone stale twice - once shipping the build with the invalid
# vtable hook. Copying on every successful link removes the opportunity.
#
# Failure is a warning, not an error: the plugin still built, and refusing to
# link because a convenience copy failed would be worse than the inconvenience.
# ---------------------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/skyrim/SKSE/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${CMAKE_CURRENT_SOURCE_DIR}/skyrim/SKSE/Plugins/TrueGaze.dll"
    COMMENT "[TrueGaze] Refreshed packaged copy in skyrim/SKSE/Plugins/"
    VERBATIM
)

# ---------------------------------------------------------------------------
# Optional tests
# ---------------------------------------------------------------------------
if(TRUEGAZE_BUILD_TESTS)
    add_executable(KinematicsTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/KinematicsTests.cpp")
    target_link_libraries(KinematicsTests PRIVATE TrueGazeKinematics)
    set_target_properties(KinematicsTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

    add_executable(HcepBridgeClientMock
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/HcepBridgeClientMock.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Bridge/NamedPipeServer.cpp"
    )
    target_include_directories(HcepBridgeClientMock PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_precompile_headers(HcepBridgeClientMock PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")
    target_link_libraries(HcepBridgeClientMock PRIVATE TrueGazeKinematics)
    set_target_properties(HcepBridgeClientMock PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif()

message(STATUS "[TrueGaze] Configured - CommonLibSSE-NG linked, SE=${BUILD_SKYRIM_SE} AE=${BUILD_SKYRIM_AE} VR=${BUILD_SKYRIM_VR}")
