#pragma once

// ---------------------------------------------------------------------------
// TestSupport.hpp — shared harness setup for every TrueGaze test executable.
//
// AUDIT 2026-10-15 F-02: with asserts live, a failing assert() in a Debug CRT
// build opens the modal "Debug Assertion Failed" dialog, and in a Release CRT
// build abort() can raise the Windows Error Reporting dialog. Either way ctest
// HANGS instead of failing. Both were observed during the audit.
//
// Including this header (first, before any test code) installs a static
// initializer that routes CRT assert/error reports to stderr and suppresses
// every dialog, so a failed assertion prints its message and the process exits
// non-zero — which is what CI needs.
// ---------------------------------------------------------------------------

#if defined(_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#    include <crtdbg.h>
#    include <cstdlib>
#    include <stdlib.h>

namespace TrueGaze::Tests::Detail
{
    struct NonInteractiveFailures
    {
        NonInteractiveFailures() noexcept
        {
            // No GPF / critical-error / open-file boxes from the OS.
            ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

            // Release CRT: assert() writes to stderr; abort() neither prints the
            // "abnormal program termination" box nor calls Windows Error Reporting.
            _set_error_mode(_OUT_TO_STDERR);
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

            // Debug CRT: route assert/error/warn reports to stderr instead of a dialog.
            // (These macros compile to nothing when _DEBUG is not defined.)
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        }
    };

    // One instance per test executable (inline variable, C++17).
    inline NonInteractiveFailures g_nonInteractiveFailures{};
} // namespace TrueGaze::Tests::Detail
#endif

// Fail loudly at compile time if someone builds the tests with asserts disabled.
// The CMake helper truegaze_force_asserts() is responsible for this; this guard
// makes a regression of audit finding F-01 impossible to miss.
#ifdef NDEBUG
#    error "TrueGaze tests must be built with assert() enabled (NDEBUG is defined). See truegaze_force_asserts() in CMakeLists.txt."
#endif
