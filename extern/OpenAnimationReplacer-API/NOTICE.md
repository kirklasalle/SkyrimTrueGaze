# Open Animation Replacer: Conditions API (vendored)

| Field | Value |
| :--- | :--- |
| Upstream | https://github.com/ersh1/OpenAnimationReplacer |
| Path upstream | `src/API/` |
| Commit | `f4e7688b065175aff70aa523073857911e15aca3` (branch `main`, fetched 2026-10-04) |
| Interface | Conditions API **V3** (`RequestPluginAPI_Conditions`), condition API version V4 |
| Author | ersh1 |
| License | GPL-3.0 (`COPYING`) **with the Modding Exception and GPL-3.0 Linking Exception** (`EXCEPTIONS`) |

## Files

`API/` holds **byte-identical** copies of these upstream files. Do not edit them.
If something needs to change, put the change in TrueGaze code; see
`src/Integrations/OarApiVendored.cpp` for how they are compiled.

- `OpenAnimationReplacerAPI-Conditions.h` / `.cpp`
- `OpenAnimationReplacer-ConditionTypes.h` / `.cpp`
- `OpenAnimationReplacer-SharedTypes.h`

The upstream header says: *"For modders: Copy this file into your own project if
you wish to use this API."* The reference consumer is
https://github.com/ersh1/OpenAnimationReplacer-ExamplePlugin.

## Updating

1. Pick an OAR release or commit and record its SHA above.
2. Replace the five files with upstream copies, unmodified.
3. Rebuild, then check the plugin log at startup for
   `OAR integration active: 3/3 conditions registered`.

## Licensing note

TrueGaze's software is licensed under GPL-3.0 (see the repository `LICENSE` and
`docs/LICENSE_RESOLUTION.md`), so these GPL-3.0 files are license-compatible.
Keep `COPYING` and `EXCEPTIONS` in this directory, and include the corresponding
source with any binary release. See `GOVERNANCE.md`, "Third-Party Licenses".
