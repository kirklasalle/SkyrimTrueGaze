// Compiles the vendored Open Animation Replacer API sources (issue #6).
//
// The files under extern/OpenAnimationReplacer-API/API are kept byte-identical to
// upstream so they can be diffed against a newer OAR release. They call
// GetModuleHandle and GetProcAddress, and assume the including project's
// precompiled header brings in <windows.h>. TrueGaze's PCH does not, so this
// wrapper includes it first instead of editing the vendored files.
//
// License: the vendored files are GPL-3.0 with OAR's Modding Exception, which is
// compatible with TrueGaze's GPL-3.0 license. See
// extern/OpenAnimationReplacer-API/NOTICE.md.

#include <windows.h>

#include "API/OpenAnimationReplacerAPI-Conditions.cpp"
#include "API/OpenAnimationReplacer-ConditionTypes.cpp"
