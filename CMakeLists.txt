cmake_minimum_required(VERSION 3.23)

project(
    TrueGaze
    VERSION 1.0.7
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

# R14 E4.4 — fail fast with an actionable message when VCPKG_ROOT is unset.
# The presets reference $env{VCPKG_ROOT}; without it CMake's error is a cryptic
# "could not find toolchain file" deep inside configure.
if(NOT DEFINED ENV{VCPKG_ROOT} AND NOT TRUEGAZE_STANDALONE)
    message(FATAL_ERROR
        "\n"
        "  VCPKG_ROOT is not set.\n"
        "\n"
        "  The windows-* presets resolve the vcpkg toolchain from the VCPKG_ROOT\n"
        "  environment variable. Set it to your vcpkg checkout, e.g.:\n"
        "      setx VCPKG_ROOT \"C:\\dev\\vcpkg\"\n"
        "\n"
        "  (Standalone mode does not need vcpkg and is unaffected.)\n")
endif()

# ---------------------------------------------------------------------------
# Test infrastructure (R14 E3.4)
# ---------------------------------------------------------------------------
# CTest registration so `ctest` runs every suite from one command, and CI
# (scripts/Invoke-CiGate.ps1, E4.2) has a single invocation to gate on.
enable_testing()

# ---------------------------------------------------------------------------
# Platform-agnostic kinematics core
# ---------------------------------------------------------------------------
add_library(TrueGazeKinematics INTERFACE)
target_include_directories(TrueGazeKinematics INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(TrueGazeKinematics INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(TrueGazeKinematics INTERFACE /W4 /permissive- /Zc:preprocessor /EHsc)
endif()

# Test executables must keep asserts LIVE. Raw assert() compiles out under
# NDEBUG, so a Release-built test suite silently verifies nothing. Forcing the
# Debug runtime + no NDEBUG on the test targets makes that failure impossible
# (R14 E3.4). /RTC1 requires /Od (D8016), so optimization is disabled for the
# Release-configured test binaries — correctness of the tests matters more
# than their speed.
function(truegaze_force_asserts target)
    target_compile_definitions(${target} PRIVATE $<$<CONFIG:Release>:_DEBUG> $<$<CONFIG:Release>:/DNDEBUG>)
    target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/MDd>)
    target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/Od /RTC1>)
endfunction()

# ---------------------------------------------------------------------------
# Standalone mode: tests only, no SDK required
# ---------------------------------------------------------------------------
if(TRUEGAZE_STANDALONE)
    message(STATUS "[TrueGaze] STANDALONE mode - building kinematics library and tests only.")

    if(TRUEGAZE_BUILD_TESTS)
        add_executable(KinematicsTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/KinematicsTests.cpp")
        target_link_libraries(KinematicsTests PRIVATE TrueGazeKinematics)
        truegaze_force_asserts(KinematicsTests)
        set_target_properties(KinematicsTests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
        add_test(NAME KinematicsTests COMMAND KinematicsTests)

        # R14 E3.3 — telemetry validation fuzz-lite (SDK-free, standalone-safe).
        add_executable(ValidationTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidationTests.cpp")
        target_link_libraries(ValidationTests PRIVATE TrueGazeKinematics)
        truegaze_force_asserts(ValidationTests)
        set_target_properties(ValidationTests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
        add_test(NAME ValidationTests COMMAND ValidationTests)

        # Gaze-arrow / HCEP-panel geometry (SDK-free).
        add_executable(GeometryTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/GeometryTests.cpp")
        target_link_libraries(GeometryTests PRIVATE TrueGazeKinematics)
        truegaze_force_asserts(GeometryTests)
        set_target_properties(GeometryTests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
        add_test(NAME GeometryTests COMMAND GeometryTests)

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
        truegaze_force_asserts(HcepBridgeClientMock)
        set_target_properties(HcepBridgeClientMock PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
        # The mock needs a real pipe round-trip; give it a generous timeout.
        add_test(NAME HcepBridgeClientMock COMMAND HcepBridgeClientMock)
        set_tests_properties(HcepBridgeClientMock PROPERTIES TIMEOUT 120)
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
# R14 E4.4 — explicit source list. The previous file(GLOB_RECURSE) meant a new
# .cpp file was invisible to the build until CMake happened to re-run; with
# CONFIGURE_DEPENDS it re-globs per build, but that is still a known CMake
# anti-pattern (silent staleness, no portability guarantee). Enumerate instead.
set(TRUEGAZE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Bridge/NamedPipeServer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/AnimationHook.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ConfigManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/EyeAimConstraint.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/GazeEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/PlayerGazeResolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/TargetSelector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/TrueGazeAPI.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/VrController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Integrations/ConsoleCommands.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Integrations/EfmBlinkController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Integrations/OarConditions.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Visuals/VisualEffectsManager.cpp"
)

foreach(_src IN LISTS TRUEGAZE_SOURCES)
    if(NOT EXISTS "${_src}")
        message(FATAL_ERROR "Listed source file does not exist: ${_src}\n"
                            "Update TRUEGAZE_SOURCES in CMakeLists.txt when adding files.")
    endif()
endforeach()

add_library(${PROJECT_NAME} SHARED ${TRUEGAZE_SOURCES})

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
    truegaze_force_asserts(KinematicsTests)
    set_target_properties(KinematicsTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    add_test(NAME KinematicsTests COMMAND KinematicsTests)

    # R14 E3.3 — telemetry validation fuzz-lite (SDK-free, standalone-safe).
    add_executable(ValidationTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidationTests.cpp")
    target_link_libraries(ValidationTests PRIVATE TrueGazeKinematics)
    truegaze_force_asserts(ValidationTests)
    set_target_properties(ValidationTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    add_test(NAME ValidationTests COMMAND ValidationTests)

    # Gaze-arrow / HCEP-panel geometry (SDK-free).
    add_executable(GeometryTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/GeometryTests.cpp")
    target_link_libraries(GeometryTests PRIVATE TrueGazeKinematics)
    truegaze_force_asserts(GeometryTests)
    set_target_properties(GeometryTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    add_test(NAME GeometryTests COMMAND GeometryTests)

    add_executable(HcepBridgeClientMock
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/HcepBridgeClientMock.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Bridge/NamedPipeServer.cpp"
    )
    target_include_directories(HcepBridgeClientMock PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_precompile_headers(HcepBridgeClientMock PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")
    target_link_libraries(HcepBridgeClientMock PRIVATE TrueGazeKinematics)
    truegaze_force_asserts(HcepBridgeClientMock)
    set_target_properties(HcepBridgeClientMock PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    add_test(NAME HcepBridgeClientMock COMMAND HcepBridgeClientMock)
    set_tests_properties(HcepBridgeClientMock PROPERTIES TIMEOUT 120)
endif()

message(STATUS "[TrueGaze] Configured - CommonLibSSE-NG linked, SE=${BUILD_SKYRIM_SE} AE=${BUILD_SKYRIM_AE} VR=${BUILD_SKYRIM_VR}")
