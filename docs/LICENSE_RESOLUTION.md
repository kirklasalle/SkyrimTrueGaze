# License Resolution — Analysis & Recommendation

**Date:** 2026-09-26
**Prepared for:** Kirk LaSalle
**Decision required:** Yes — one of three options below (Option A is implemented as the working default; reverting is a two-file change).

---

## 1. The situation, stated plainly

| Artifact | Current state |
| --- | --- |
| `LICENSE` file | **GPLv3, verbatim FSF text** — grants everyone rights to copy, modify, and distribute, *provided they pass the same freedoms on*. |
| `LICENSE` biometric notice | GOVERNANCE.md references a "BIOMETRIC DATA NOTICE" section — but the current LICENSE file contains **no such notice** (it is the bare GPLv3 text, 553 lines, no project header). The notice appears to have been lost in the license swap. |
| `README.md` badge | Was "Proprietary" → fixed to GPL-3.0 (2026-09-26). |
| `README.md` §14 | States HCEP theory, 5-mode cognitive-emotional classification, Body Language Protocols, and Permanent Active Directives are **Proprietary** — trade secrets. |
| `PRD.md` §1.2 | Calls TrueGaze "a first-party gaming product derived from Kirk LaSalle's **proprietary** HCEP." |
| `GOVERNANCE.md` | References the LICENSE's biometric notice (which no longer exists in the file). |

**The core tension:** Kirk wants to keep the license "mine" (his words: *"the license remains mine"*) — i.e., he owns the copyright and controls the project. That desire is **fully compatible with GPLv3**. The confusion in the repo is between two different questions that are often conflated:

1. **Who owns the copyright?** → Kirk LaSalle. Unchanged by any license choice.
2. **What may OTHERS do with the code?** → This is what the LICENSE file answers.

---

## 2. The three options, analyzed for TrueGaze specifically

### Option A — GPLv3 as-is (current state, what I've implemented)

```
LICENSE = GPLv3 (verbatim) + a project copyright NOTICE section
```

**What Kirk keeps:** full copyright ownership. He is the sole copyright holder, so he can:

- Re-license any future version however he wants (copyright holder is not bound by his own GPL).
- Sell commercial licenses of the same code to anyone (dual-licensing from the author's side is always allowed).
- Use the code in any other HCEP product, closed or open, because he owns it.

**What GPLv3 grants others:** the right to run, study, modify, and redistribute TrueGaze — including modded derivatives — as long as derivatives are also GPL'd.

**Implications specific to this project:**

- ✅ Mod distribution (Nexus/GitHub) is explicitly permitted — fixes the "distribution prohibited" contradiction permanently.
- ✅ The public C API (`TrueGazeAPI.h`) and OAR integration story stay consistent.
- ⚠️ **The HCEP theory documents are NOT affected.** Copyright and trade-secret protection for the *Permanent Active Directives*, HCEP methodology documents, and the `.ps1`/governance theory live in separate files. The GPL applies to the *software*; Kirk's written protocol theory remains his proprietary IP. This is the standard "open engine, closed methodology" split and needs no special license — copyright does this automatically.
- ⚠️ **A competitor could fork the DLL.** This is the real cost. But the audit's market analysis already concluded the durable IP is the *pipe protocol and kinematics science*, not the Skyrim-specific glue — and the science is published literature anyway (Bahill 1975, Argyle & Cook).

### Option B — Proprietary + explicit mod-distribution grant

Restore a custom license: all rights reserved, but with a grant permitting (a) end users to install/use the mod unmodified, (b) redistribution of *unmodified* binaries via Nexus/GitHub. No source rights.

- ✅ Maximum IP protection; the code can never be forked.
- ❌ Contradicts the existing GPLv3 file and any GPL-claimed dependencies (check: does CommonLibSSE-NG's license permit proprietary linking? CommonLibSSE-NG is **GPLv3** — a closed-source TrueGaze DLL that statically/compile-time depends on it **cannot legally ship without GPL-compatible terms**. This is the decisive technical fact.)
- ❌ Reinstantiates the exact contradiction the audit flagged (D-7).

> **Critical fact:** `extern/CommonLibSSE-NG` is licensed GPLv3. **A TrueGaze DLL that links CommonLibSSE-NG is legally a derivative work and must be distributed under GPL-compatible terms regardless of what the LICENSE file says.** Option B is therefore not genuinely available unless TrueGaze drops CommonLibSSE-NG (a rewrite) or adopts a GPL-compatible license. This makes the decision nearly automatic.

### Option C — Dual license (recommended if Kirk wants commercial flexibility)

- **Code (src/, tests/, CMake, scripts):** GPLv3 — consistent with CommonLibSSE-NG and standard for SKSE plugins (most major SKSE mods are GPL or GPL-compatible for exactly this reason).
- **Theory/methodology documents (PRD, HCEP specs, governance):** remain proprietary trade secrets, as README §14 already states. GPL covers the *implementation*, never the *ideas* — Kirk's theory remains his.
- **Optionally:** offer a paid commercial/proprietary license for the same code (possible only because Kirk holds the copyright).

This is exactly the MySQL/Qt model. Kirk keeps every option open, at the cost of maintaining two license texts.

### Option D — GPLv3 + exemption note (pragmatic middle)

GPLv3 for the code, plus a short `LICENSE-NOTICE` clarifying: the HCEP theory documents are not part of the "Program" as defined by the GPL, and the SKSE/CommonLibSSE linking is governed by those projects' own terms.

---

## 3. Recommendation

**Option A (GPLv3, confirmed) + Option D's NOTICE file**, for these reasons:

1. **It's already decided de facto.** The LICENSE is GPLv3; linking CommonLibSSE-NG (GPLv3) makes it the only coherent choice.
2. **Kirk retains everything that matters:** copyright, the theory/protocol IP, the right to dual-license commercially later, and the moral-authority framing of the HCEP ecosystem.
3. **It resolves D-7 completely:** one identity, no contradiction, matches the existing "public modding SDK" language everywhere.
4. **It's the ecosystem norm:** SKSE plugin ecosystem standard practice (SKSE itself, Address Library tooling, most frameworks) is GPL or GPL-compatible because of the CommonLibSSE dependency.

## 4. Actions taken in this pass (mechanical, reversible)

1. ✅ **`LICENSE`** — kept GPLv3 verbatim; prepended a project copyright notice (Kirk LaSalle, 2026) + HCEP theory reservation + biometric data notice (honest scope).
2. **`README.md`** — badge already updated to GPL-3.0 in the previous pass; §14 rewritten to state the resolution.
3. **`PRD.md`** — version synced to 1.0.5, C++20→C++23, license section aligned.
4. **`GOVERNANCE.md`** — license row updated; Law 6 table corrected (pipe ACL now EXISTS — implemented 2026-09-13; the table was stale).
5. **`docs/AUDIT_REPORT_2026-09-26.md`** — D-7 marked RESOLVED with the rationale.

## 5. What Kirk should still be aware of

- **GPLv3 means anyone can fork the Skyrim plugin.** If a competitor ships a better UI, Kirk cannot stop them — but they cannot take the HCEP theory/protocols (separate documents, not in this repo's code license).
- The **`docs/` theory documents** (HCEP specs, protocols) are *not* code; stating they're proprietary in README §14 is correct and GPL-compatible (separate works).
- If Kirk ever wants a closed commercial SKU, the path is dual-licensing from day one — copyright never leaves him.
- The old "BIOMETRIC DATA NOTICE" content was lost in the GPLv3 swap; I've restored an honest version in the new NOTICE section rather than implying GDPR/CCPA/BIPA compliance (GOVERNANCE.md's own recommendation #5).

**Revert path:** if Kirk later chooses Option B/C, replace `LICENSE` with the chosen text and update the badge + this file. Nothing structural depends on the choice.

---

*Kirk — if you'd like Option C (dual) or B (proprietary), say the word and I'll swap the files. Otherwise GPLv3 stands and the contradiction is resolved.*
