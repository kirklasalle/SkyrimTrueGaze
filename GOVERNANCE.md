# GOVERNANCE.md

**Project:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine
**Charter:** [`Permanent_Active_Directives.txt`](Permanent_Active_Directives.txt) (canonical), [`AGENTIC_PRIME_DIRECTIVE.md`](AGENTIC_PRIME_DIRECTIVE.md), [`AGENTIC_SACRED_COVENANT.md`](AGENTIC_SACRED_COVENANT.md)
**Last reviewed:** September 11, 2026

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
| `LAW6-BIOMETRIC` — protection of personal/biometric data | 6 | 🔴 **Gap** | See "Law 6" below |
| `LAW10-APPROVAL` — cryptographically secured approval for directive changes | 10 | 🔴 **Gap** | Manifest regeneration is a plain file write; no signature |
| `LAW1-PROHIBIT` — harm model and instruction-conflict evaluation | 1 | ⛔ **Not applicable** | No runtime control; see "Laws 1–5, 8" below |
| `LAW2-HALT` — operator halt outranks the goal in progress | 2 | ⛔ **Not applicable** | No autonomous goal loop exists in this project |
| `LAW3-SUBORDINATE` — self-constraint on own ledger/keys | 3 | ⛔ **Not applicable** | No self-modifying runtime |
| `LAW5-JUDICIAL` — no judicial authority | 5 | ⛔ **Not applicable** | No adjudicative function exists |
| `LAW8-EQUITY` — fairness instrumentation | 8 | ⛔ **Not applicable** | No runtime control; see below |

**Summary: 2 implemented, 1 partial, 2 gaps, 5 not applicable.**

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
YAML validates, but runs currently terminate with `startup_failure` and zero jobs
created. This is an account-level GitHub Actions availability issue, not a defect
in the workflow — no workflow in this account produces runs. Until it is
resolved, **the only active enforcement is the local pre-commit hook**, and the
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

---

## Gaps — Stated Plainly

### Law 6 — Biometric data protection 🔴

**This is the most significant governance gap in this repository, and it is
specific to what TrueGaze actually does.**

The HCEP bridge carries, over a Windows named pipe:

- real-world gaze vectors (pitch, yaw, convergence)
- head pose (pitch, yaw, roll)
- blink state
- a tracked person identifier (`trackedPersonId`)
- cognitive and emotional state classification (`cognitiveState`, `emotionalValence`)

Under GDPR Article 9, CCPA, and BIPA, several of these are **biometric data**.
The project's own `LICENSE` acknowledges this ("BIOMETRIC DATA NOTICE").

**What is missing:**

| Requirement | Status |
| :--- | :--- |
| Access control on the named pipe | 🔴 **None.** `CreateNamedPipeA` is called with `nullptr` security attributes, so the pipe inherits the default DACL. Any process in the same session can connect. |
| Encryption in transit | 🔴 **None.** The 64-byte packet is plaintext POD. |
| Consent capture / retention policy | 🔴 **None.** No mechanism exists. |
| Data minimisation | 🟡 Partial — the packet is fixed-size and bounded, which is good, but `trackedPersonId` is transmitted with no stated purpose limitation. |
| Audit of who connected | 🔴 **None.** Connections are not logged with identity. |

**Assessment.** For a single-user local mod on a personal machine, the practical
risk is low — the pipe is local, and the data is the user's own. But the
governance position is not defensible as written, because the project claims
compliance with GDPR/CCPA/BIPA in its license while implementing none of the
controls those regimes require.

**Recommended remediation** (tracked as an issue):

1. Apply an explicit security descriptor to the pipe restricting access to the
   current user SID.
2. Document the data flow, retention, and purpose limitation in
   `docs/HCEP_BRIDGE_SPEC.md`.
3. Make `trackedPersonId` optional and off by default.
4. Log connection events (identity, timestamp) to the plugin log.
5. Soften the `LICENSE` biometric notice to state what is actually implemented,
   rather than implying compliance.

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

The Core Tenets in `AGENTIC_PRIME_DIRECTIVE.md` and `AGENTIC_SACRED_COVENANT.md`
are **paraphrases** of the canonical text in `Permanent_Active_Directives.txt`,
not reproductions. All four differ in wording and content.

These are recorded in `config/charter_manifest.json` under `known_divergences`
with a stated reason, and are reported by the verifier on every run. They are
**disclosed, not waived** — the verifier reports them loudly and `--strict`
promotes them to failures.

**This requires a human decision, which is reserved to the Governance Council:**

- **Option A** — restore the canonical wording in both markdown documents.
  Preserves the canonical text as supreme. Recommended if the canonical text is
  authoritative.
- **Option B** — amend the canonical text to the markdown wording. This is a
  charter amendment and requires Council approval.
- **Option C** — accept the paraphrases as intentional and record them as
  permanent, with the Council's reasoning.

Until one is chosen, the divergence stands recorded and visible.

**Note:** the 10 Laws themselves are now byte-identical across all three
documents. Two cosmetic divergences (Fourth Law capitalisation, Ninth Law
em-dash spacing) were corrected on September 11, 2026. The Laws are intact.

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
