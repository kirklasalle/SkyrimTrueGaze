# GOVERNANCE.md

**Project:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine
**Charter:** [`Permanent_Active_Directives.txt`](Permanent_Active_Directives.txt) (canonical), [`AGENTIC_PRIME_DIRECTIVE.md`](AGENTIC_PRIME_DIRECTIVE.md), [`AGENTIC_SACRED_COVENANT.md`](AGENTIC_SACRED_COVENANT.md)
**Last reviewed:** October 4, 2026

---

## Purpose

`AGENTIC_SACRED_COVENANT.md` §3.1 requires that the full map of what the covenant
enforces — **including every gap** — be published, and that CI check this document
against the code so it cannot claim more than is implemented.

This is that map, for this repository.

The covenant was written for the PRISM governance runtime. TrueGaze is a Skyrim
SKSE plugin. Most of the covenant's runtime enforcement machinery (policy engines,
approval tokens, attestation, halt files, append-only ledgers) **does not exist
here and is not claimed to exist here.** What follows states plainly which
provisions have a real control in this repository, which are aspirational, and
which are not applicable.

---

## Control Status

| Control | Law | Status | Evidence |
| :--- | :---: | :--- | :--- |
| `LAW10-CHARTER` — Laws pinned by digest, verified across all charter copies | 10 | ✅ **Implemented** | `scripts/verify_charter.py`, `config/charter_manifest.json`, `scripts/hooks/pre-commit` (active). CI workflow committed but not executing. |
| `LAW7-TRUTHFUL-LOG` — no log may report success for an operation not performed | 7 | ✅ **Implemented** | See "Law 7" below |
| `LAW9-AUDIT` — auditable record of reasoning and decisions | 9 | 🟡 **Partial** | `docs/AUDIT_REPORT_2026-09-11.md`, `CHANGELOG.md`, `docs/STATUS.md` |
| `LAW6-BIOMETRIC` — protection of personal/biometric data | 6 | 🟡 **Partial** | See "Law 6" below |
| `LAW10-APPROVAL` — cryptographically secured approval for directive changes | 10 | 🔴 **Gap** | Manifest regeneration is a plain file write; no signature |
| `LAW1-PROHIBIT` — harm model and instruction-conflict evaluation | 1 | ⛔ **Not applicable** | No runtime control; see "Laws 1–5, 8" below |
| `LAW2-HALT` — operator halt outranks the goal in progress | 2 | ⛔ **Not applicable** | No autonomous goal loop exists in this project |
| `LAW3-SUBORDINATE` — self-constraint on own ledger/keys | 3 | ⛔ **Not applicable** | No self-modifying runtime |
| `LAW5-JUDICIAL` — no judicial authority | 5 | ⛔ **Not applicable** | No adjudicative function exists |
| `LAW8-EQUITY` — fairness instrumentation | 8 | ⛔ **Not applicable** | No runtime control; see below |

**Summary: 2 implemented, 2 partial, 1 gap, 5 not applicable.**

---

## What Is Actually Enforced

### `LAW10-CHARTER` — Charter integrity

The 10 Laws and 4 Core Tenets are pinned by SHA-256 digest in
`config/charter_manifest.json` and compared against every charter document on
every push and pull request that touches a charter file.

The verifier detects a Law that has been:

- **reworded** — digest mismatch
- **dropped** — present in the manifest, absent from a document
- **duplicated** — two declarations of the same Law in one document
- **renumbered** — e.g. `4. **Fifth Law:**` (number/ordinal misalignment)
- **left disagreeing between copies** �� the same Law differing across documents

Run it locally:

```bash
python scripts/verify_charter.py            # verify
python scripts/verify_charter.py --verbose  # per-Law report
python scripts/verify_charter.py --json     # machine-readable
python scripts/verify_charter.py --strict   # treat recorded divergences as failures
```

**Enforcement points.** The control runs in two places:

| Point | Mechanism | Status |
| :--- | :--- | :--- |
| Commit time | `scripts/hooks/pre-commit` (installed by `scripts/install-hooks.ps1`) | ✅ Active |
| CI | `.github/workflows/charter-integrity.yml` | ⚠️ Workflow committed; **not currently executing** — see below |

**Local enforcement limitations — stated plainly.** The pre-commit hook is a
convenience, not a guarantee:

- It only exists after someone runs `scripts/install-hooks.ps1`. A fresh clone
  has no hook.
- `git commit --no-verify` bypasses it entirely.
- If no working Python 3.8+ interpreter is found, the hook **fails open** — it
  prints a loud warning that the commit is proceeding unverified, then allows it.
  This is deliberate: a check that blocks every commit on a machine without
  Python would simply be disabled, which is worse than an honest warning.
- It does not protect against changes made through the GitHub web interface.

**CI status.** `.github/workflows/charter-integrity.yml` is committed and its
YAML validates. The repository is **public**, but GitHub Actions runner jobs
currently do not start because the account has an active billing hold on Actions
(GitHub run failure annotation: `"The job was not started because your account is locked due to a billing issue"`).
Nothing is wrong with the workflow itself. CI will execute automatically once the
account billing hold is cleared at `https://github.com/settings/billing/summary`.
Until then, **the only active enforcement is the local pre-commit hook**, and the
control should be described as locally enforced rather than CI-enforced.

**Design note — why only the Laws and Tenets are pinned.** The manifest pins each
Law and Tenet individually. It deliberately does **not** pin a digest of the whole
document. Charter documents are expected to grow: preamble, rationale, honest
statements of what is and is not enforced, amendment records. Pinning the entire
file would make any honest addition a build failure, which would train
maintainers to disable the check. The immutable part is the Law text.

### `LAW7-TRUTHFUL-LOG` — No false success reporting

Law 7 requires truthful communication. The September 2026 audit found this
project in direct violation: `OarConditions::RegisterWithOar()` logged
`"Registered TrueGaze_IsMode, ... conditions with OAR."` while performing no
registration at all, and `TrueGaze_GetActorGaze()` returned `true` alongside
fabricated telemetry.

**Standing rule for this repository:** no log message, status field, or API
return value may report success for an operation that was not performed. Where a
capability is absent, the code must say so — `logger::warn`, a `false` return, or
an explicit "not implemented" — never a success message.

This rule is enforced by review, not by tooling. It is recorded here so that it
is a stated obligation rather than an unstated expectation.

**Recurrence, October 2026 (issue #6).** A later revision of `RegisterWithOar()`
sent an invented SKSE message to OpenAnimationReplacer, which does not listen for
it, then logged "Dynamic condition API hook established" and reported itself as
registered. That was the same violation in a new form. It was replaced with a
binding against OAR's published Conditions API (vendored in
`extern/OpenAnimationReplacer-API`). Success is now logged only when OAR returns
`APIResult::OK` for every condition. Reviewers should look specifically for
success logs that follow a call whose result is never checked.

---

## Gaps — Stated Plainly

### Law 6 — Biometric data protection 🟡

**This was the most significant governance gap in this repository. It has been
narrowed, not closed.** It is specific to what TrueGaze actually does.

The HCEP bridge carries, over a Windows named pipe:

- real-world gaze vectors (pitch, yaw, convergence)
- head pose (pitch, yaw, roll)
- blink state
- a tracked person identifier (`trackedPersonId`)
- cognitive and emotional state classification (`cognitiveState`, `emotionalValence`)

Under GDPR Article 9, CCPA, and BIPA, several of these are **biometric data**.
`LICENSE` ("BIOMETRIC AND PERSONAL DATA NOTICE") states what is and is not
implemented, rather than asserting compliance.

**Current controls** (issue #7):

| Requirement | Status |
| :--- | :--- |
| Access control on the named pipe | ✅ Explicit user-only DACL (`D:(A;;GA;;;OW)`). If it cannot be built, the pipe falls back to the default DACL with a logged warning. |
| Data minimisation | ✅ `trackedPersonId` is **zeroed on receipt**, before the game thread can see it, unless `[Bridge] bRetainTrackedPersonId=true`. Covered by a test in `tests/HcepBridgeClientMock.cpp`. |
| Audit of who connected | ✅ Each connection is logged with PID, session ID, executable file name and ACL state. Each disconnect logs accepted and rejected frame counts. Packet contents are never logged. |
| Stale-data rejection | ✅ Telemetry older than 500 ms is ignored. |
| Persistence / retention | ✅ None. In memory only, nothing written to disk. |
| Consent | 🟡 Bridge is opt-out via `bConnectHcepBridge`. There is no consent-capture workflow; per `LICENSE` the operator is responsible for consent. |
| Encryption in transit | 🔴 **None.** Plaintext on a local pipe. A deliberate decision for the single-user threat model; see `docs/HCEP_BRIDGE_SPEC.md` §6.1. Revisit if a multi-user deployment is ever supported. |
| Authentication of the client | 🔴 **None.** CRC-32 only. Any same-user process can inject telemetry, limited by semantic validation. |

**Assessment.** For a single-user local mod on a personal machine the practical
risk is low. The license now describes the implementation accurately, which was
the substantive governance problem. The control stays **partial** because
encryption and client authentication are absent. That is a reasoned choice for
this threat model, not something that has been solved.

### `LAW10-APPROVAL` — Cryptographic approval for directive changes 🔴

The covenant requires that permanently modifying core directives needs
"explicit, cryptographically secured approval from Governance."

**Current state:** `python scripts/verify_charter.py --update` regenerates the
manifest with a plain file write. There is no signature, no approval token, and
no separation between "record what the document says" and "authorise the change."

**Mitigation in place:** the tool prints an explicit reminder that regeneration
is not authorisation, and the resulting diff must be reviewed. This is a
procedural control, not a cryptographic one.

**Recommended remediation:** sign the manifest with an Ed25519 operator key and
have CI verify the signature, as the covenant describes. Until then, this control
should be described as procedural, not cryptographic.

### Law 9 — Transparent reasoning ledger 🟡

Partially satisfied. The audit report, changelog, and status matrix provide an
auditable record of what was found and what was corrected. There is no
append-only, hash-linked ledger, and no runtime decision logging.

For a game mod this is proportionate. It is recorded as partial rather than
complete.

---

## Laws With No Runtime Control

The covenant states this explicitly and it is repeated here so it cannot be
mistaken for an oversight:

> the system has no harm model, no evaluator for whether an instruction conflicts
> with one, no judicial-overreach detector, and no fairness instrumentation.
> **Laws 5 and 8 have no runtime control at all.**

**Laws 1–5 and 8 are not enforced by any code in this repository.** They are
governing principles for human conduct on this project — how contributors and AI
assistants behave — not runtime constraints on the plugin.

This is appropriate for what TrueGaze is. A Skyrim plugin that rotates eye bones
has no capacity to cause physical harm, no judicial function, and no
decision-making authority over people. Claiming runtime enforcement of Laws 1–5
and 8 here would be exactly the kind of over-claiming the September 2026 audit
was written to correct.

**If TrueGaze is ever extended to make autonomous decisions about people** — for
example, if the HCEP bridge is used to infer emotional state for purposes beyond
driving NPC animation — this assessment must be revisited, and Laws 1, 5, 6, and
8 would acquire real runtime obligations.

---

## Known Charter Divergences

**None outstanding.** `python scripts/verify_charter.py --strict` passes.

**Resolved 2026-10-04 (issue #8): Option A.** The four Core Tenets in
`AGENTIC_PRIME_DIRECTIVE.md` and `AGENTIC_SACRED_COVENANT.md` had been
paraphrased rather than reproduced. The most substantive change was in
*Dialogue and Resolution*, where the paraphrase dropped the commitment to
"integrate human-like reasoning into interactions". The project owner chose
Option A: **restore the canonical wording**. Each tenet line was copied
verbatim from `Permanent_Active_Directives.txt`, which remains supreme and was
not changed. That makes this a correction to the copies, not a charter
amendment, so no digest in `config/charter_manifest.json` changed. Only the eight
`known_divergences` records were removed, because they no longer apply.

The options that were considered, kept for the record:

- **Option A** (chosen): restore the canonical wording in both markdown documents.
- **Option B**: amend the canonical text to the markdown wording. That would
  have been a charter amendment.
- **Option C**: accept the paraphrases as permanent.

**Note:** the 10 Laws themselves are byte-identical across all three
documents. Two cosmetic divergences (Fourth Law capitalisation, Ninth Law
em-dash spacing) were corrected on September 11, 2026.

---

## Third-Party Licenses

| Component | Location | License | Status |
| :--- | :--- | :--- | :--- |
| CommonLibSSE-NG | `extern/CommonLibSSE-NG` (submodule) | MIT | ✅ Compatible with GPL-3.0 |
| Open Animation Replacer Conditions API | `extern/OpenAnimationReplacer-API` (vendored, unmodified) | GPL-3.0 + OAR Modding Exception | ✅ Compatible: TrueGaze's software is itself GPL-3.0 |

`LICENSE` puts TrueGaze's software under GPL-3.0 (see `docs/LICENSE_RESOLUTION.md`).
The vendored OAR API files are GPL-3.0 too, and upstream explicitly invites
modders to copy them into their own projects, so including them adds no new
licensing obligation. Keep the upstream `COPYING` and `EXCEPTIONS` files next to
the vendored sources, and include the corresponding source with any binary
release, as GPL-3.0 already requires. Provenance (upstream commit SHA) is
recorded in `extern/OpenAnimationReplacer-API/NOTICE.md`.

---

## Amendment Procedure

1. Propose the change with written rationale.
2. Obtain Governance Council approval. **This is a human decision and cannot be
   delegated to an AI system** (Law 5).
3. Apply the change to the canonical document,
   `Permanent_Active_Directives.txt`.
4. Run `python scripts/verify_charter.py --update` to regenerate digests.
5. Review the resulting diff. Confirm it contains only the approved change.
6. Commit with a message identifying the approval.
7. CI verifies the manifest against all charter documents.

Step 4 records what the document says. It does not authorise the change. Steps
2 and 5 are where authorisation happens.

---

## Reporting a Governance Concern

Open an issue labelled `governance`. If the concern involves a potential Law
violation, state which Law and provide the evidence. Per Law 9, concerns are
resolved in the open record rather than privately.

---

*This document is checked against the code by CI. If it claims a control that
does not exist, that is a defect in this document and should be reported.*
