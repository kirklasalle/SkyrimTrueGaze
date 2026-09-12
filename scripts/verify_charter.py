#!/usr/bin/env python3
"""Charter integrity verification for the Permanent Active Directives.

Implements the LAW10-CHARTER control described in AGENTIC_SACRED_COVENANT.md
section 3.1:

    "the 10 Laws are pinned by digest in config/charter_manifest.json and
     compared across every charter document in CI, so a Law cannot be reworded,
     dropped, or left disagreeing with its copies without the build failing."

Design note — why only the Laws and Core Tenets are pinned
----------------------------------------------------------
The manifest pins the digest of each Law and each Core Tenet individually. It
deliberately does NOT pin a digest of the whole document. Charter documents are
expected to grow: preamble, rationale, honest statements of what is and is not
enforced, amendment records. Pinning the entire file would make any honest
addition a build failure, which would train maintainers to disable the check.

The immutable part is the Law text. That is what is pinned.

Exit codes
----------
0  Charter intact — every Law and Tenet matches the manifest in every document.
1  Violation detected — a Law was reworded, dropped, duplicated, reordered, or
   two charter documents disagree.
2  Operational error — manifest missing/invalid, or a charter document missing.

Usage
-----
    python scripts/verify_charter.py             # verify (CI mode)
    python scripts/verify_charter.py --verbose   # per-law report
    python scripts/verify_charter.py --json      # machine-readable report
    python scripts/verify_charter.py --update    # regenerate the manifest

--update regenerates config/charter_manifest.json from the canonical document.
Per the covenant, changing the Laws requires Governance approval. --update only
records what the canonical document currently says; it does not authorise the
change. The update must still be reviewed and approved.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any

# --------------------------------------------------------------------------
# Configuration
# --------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
MANIFEST_PATH = REPO_ROOT / "config" / "charter_manifest.json"

ORDINALS: tuple[str, ...] = (
    "First", "Second", "Third", "Fourth", "Fifth",
    "Sixth", "Seventh", "Eighth", "Ninth", "Tenth",
)

TENET_NAMES: tuple[str, ...] = (
    "Human-Centric Assistance",
    "Promotion of Growth",
    "Dialogue and Resolution",
    "Wellness and Prosperity",
)

# Matches:  "1. **First Law:** An Intelligence System ..."
LAW_PATTERN = re.compile(
    r"^\s*(\d+)\.\s*\*\*(" + "|".join(ORDINALS) + r")\s+Law:\*\*\s*(.*)$"
)

# Matches both formats:
#   "- **Human-Centric Assistance:** Designed to serve ..."           (bulleted)
#   "**Human-Centric Assistance:** Designed to serve ..."             (unbulleted)
#   "**Dialogue and Resolution: Implementing the Socratic Method:** ..."  (titled variant)
#
# Group 1 = tenet name (identifier)
# Group 2 = optional title suffix after the name (e.g. ": Implementing the Socratic Method")
# Group 3 = tenet body
#
# The suffix is folded into the pinned text so that title drift is detected too.
TENET_PATTERN = re.compile(
    r"^\s*(?:[-*]\s*)?\*\*("
    + "|".join(re.escape(n) for n in TENET_NAMES)
    + r")([^*]*)\*\*\s*(.*)$"
)

# Lines that terminate a law/tenet block when encountered during continuation.
BLOCK_TERMINATORS = re.compile(r"^\s*(#{1,6}\s|\d+\.\s*\*\*|[-*]\s*\*\*|\|)")

DIGEST_PREFIX = "sha256:"


# --------------------------------------------------------------------------
# Text handling
# --------------------------------------------------------------------------

def read_text(path: Path) -> str:
    """Read a charter document as UTF-8, tolerating a BOM."""
    return path.read_text(encoding="utf-8-sig")


def normalize(text: str) -> str:
    """Normalize law/tenet text for comparison.

    Insensitive to: line endings, trailing whitespace, Markdown line-wrapping,
    and runs of internal whitespace.

    Sensitive to: every word and its order. That is the point — a reworded Law
    must not compare equal.
    """
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    lines = [line.strip() for line in text.split("\n")]
    lines = [line for line in lines if line]
    return re.sub(r"\s+", " ", " ".join(lines)).strip()


def digest(text: str) -> str:
    """Return the canonical 'sha256:<hex>' digest of normalized text."""
    payload = normalize(text).encode("utf-8")
    return DIGEST_PREFIX + hashlib.sha256(payload).hexdigest()


# --------------------------------------------------------------------------
# Extraction
# --------------------------------------------------------------------------

def extract_blocks(lines: list[str], pattern: re.Pattern[str],
                   key_group: int, text_group: int) -> list[tuple[str, str, int]]:
    """Extract (key, text, line_number) for every match of `pattern`.

    Captures the matched line plus any continuation lines that follow, stopping
    at a blank line, a new list item, a heading, or a table row.
    """
    blocks: list[tuple[str, str, int]] = []

    for index, line in enumerate(lines):
        match = pattern.match(line)
        if not match:
            continue

        key = match.group(key_group)
        fragments = [match.group(text_group)]

        cursor = index + 1
        while cursor < len(lines):
            nxt = lines[cursor]
            if not nxt.strip() or BLOCK_TERMINATORS.match(nxt):
                break
            fragments.append(nxt.strip())
            cursor += 1

        blocks.append((key, " ".join(fragments), index + 1))

    return blocks


def parse_charter(path: Path) -> tuple[dict[str, tuple[str, int]], dict[str, tuple[str, int]]]:
    """Return (laws, tenets) for a charter document.

    laws   -> {law_ordinal: (text, line_number)}
    tenets -> {tenet_name: (text, line_number)}
    """
    lines = read_text(path).split("\n")

    laws: dict[str, tuple[str, int]] = {}
    for ordinal, text, line_no in extract_blocks(lines, LAW_PATTERN,
                                                 key_group=2, text_group=3):
        if ordinal in laws:
            raise ValueError(
                f"{path.name}: duplicate '{ordinal} Law' declaration at line {line_no} "
                f"(first seen at line {laws[ordinal][1]})"
            )
        laws[ordinal] = (text, line_no)

    tenets: dict[str, tuple[str, int]] = {}
    for name, title_suffix, text, line_no in extract_tenets(lines):
        if name in tenets:
            raise ValueError(
                f"{path.name}: duplicate tenet '{name}' at line {line_no} "
                f"(first seen at line {tenets[name][1]})"
            )
        # Fold the optional title suffix into the pinned text so that drift in
        # the tenet's own title is detected alongside drift in its body.
        combined = f"{name}{title_suffix} {text}"
        tenets[name] = (combined, line_no)

    return laws, tenets


def extract_tenets(lines: list[str]) -> list[tuple[str, str, str, int]]:
    """Extract (name, title_suffix, body, line_number) for every Core Tenet."""
    results: list[tuple[str, str, str, int]] = []

    for index, line in enumerate(lines):
        match = TENET_PATTERN.match(line)
        if not match:
            continue

        name = match.group(1)
        title_suffix = match.group(2).strip()
        fragments = [match.group(3)]

        cursor = index + 1
        while cursor < len(lines):
            nxt = lines[cursor]
            if not nxt.strip() or BLOCK_TERMINATORS.match(nxt):
                break
            fragments.append(nxt.strip())
            cursor += 1

        results.append((name, title_suffix, " ".join(fragments), index + 1))

    return results


def check_ordinal_alignment(laws: dict[str, tuple[str, int]], name: str) -> list[str]:
    """Verify each law's number matches its ordinal word.

    e.g. "4. **Fourth Law:**" is aligned; "4. **Fifth Law:**" is not.
    Catches a Law being renumbered without its ordinal being updated.
    """
    problems: list[str] = []
    for ordinal, (text, line_no) in laws.items():
        # `text` begins with the law body; recover the number from the source line.
        # Re-extract the leading integer from the original line instead.
        match = LAW_PATTERN.match(_source_line(REPO_ROOT / name, line_no))
        if not match:
            continue
        number = int(match.group(1))
        expected = ORDINALS[number - 1] if 1 <= number <= len(ORDINALS) else None
        if expected != ordinal:
            problems.append(
                f"{name}: line {line_no}: law numbered {number} is labelled "
                f"'{ordinal} Law' but should be '{expected} Law'"
            )
    return problems


def _source_line(path: Path, line_no: int) -> str:
    """Return 1-based line `line_no` of `path`, or '' if unavailable."""
    try:
        lines = read_text(path).split("\n")
        return lines[line_no - 1] if 0 < line_no <= len(lines) else ""
    except OSError:
        return ""


# --------------------------------------------------------------------------
# Manifest
# --------------------------------------------------------------------------

def load_manifest() -> dict[str, Any]:
    if not MANIFEST_PATH.exists():
        raise FileNotFoundError(
            f"Manifest not found at {MANIFEST_PATH.relative_to(REPO_ROOT)}. "
            "Run: python scripts/verify_charter.py --update"
        )
    with MANIFEST_PATH.open(encoding="utf-8") as handle:
        return json.load(handle)


def build_manifest(canonical: Path, documents: list[str]) -> dict[str, Any]:
    """Build a fresh manifest from the canonical charter document."""
    laws, tenets = parse_charter(canonical)

    missing_laws = [o for o in ORDINALS if o not in laws]
    missing_tenets = [t for t in TENET_NAMES if t not in tenets]
    if missing_laws or missing_tenets:
        raise ValueError(
            f"Canonical document '{canonical.name}' is incomplete — "
            f"missing laws: {missing_laws or 'none'}; "
            f"missing tenets: {missing_tenets or 'none'}"
        )

    return {
        "manifest_version": 1,
        "description": (
            "Canonical digests of the immutable Laws and Core Tenets of the "
            "Permanent Active Directives. Verified in CI (control: LAW10-CHARTER). "
            "Generated by scripts/verify_charter.py --update. Changing a digest "
            "constitutes a charter amendment and requires Governance approval."
        ),
        "hash_algorithm": "sha256",
        "normalization": (
            "utf-8-sig; CRLF/CR -> LF; per-line strip; blank lines removed; "
            "internal whitespace runs collapsed to a single space. Insensitive to "
            "Markdown line-wrapping; sensitive to every word and its order."
        ),
        "canonical_source": canonical.name,
        "charter_documents": documents,
        "laws": [
            {"id": i + 1, "ordinal": ordinal, "digest": digest(laws[ordinal][0])}
            for i, ordinal in enumerate(ORDINALS)
        ],
        "core_tenets": [
            {"name": name, "digest": digest(tenets[name][0])}
            for name in TENET_NAMES
        ],
        # Divergences that have been detected, investigated, and explicitly
        # recorded together with a reason. Preserved across --update so that
        # regenerating the digests cannot silently erase a known, review-pending
        # finding. An entry is a disclosure, not a waiver.
        "known_divergences": [],
    }


# --------------------------------------------------------------------------
# Verification
# --------------------------------------------------------------------------

def verify(manifest: dict[str, Any], verbose: bool, strict: bool
           ) -> tuple[list[str], list[str]]:
    """Return (violations, warnings).

    violations -> unexpected charter drift; fails the build.
    warnings   -> drift explicitly recorded as a known divergence in the
                  manifest. Reported loudly but does not fail unless --strict.

    The distinction exists so that honestly-recorded, review-pending deviations
    do not train maintainers to disable the check, while *unexpected* drift
    still fails the build immediately.
    """
    violations: list[str] = []
    warnings: list[str] = []

    canonical = manifest.get("canonical_source", "")
    documents = manifest.get("charter_documents", [])

    expected_laws = {entry["ordinal"]: entry["digest"] for entry in manifest["laws"]}
    expected_tenets = {entry["name"]: entry["digest"] for entry in manifest["core_tenets"]}

    # (document, item) -> recorded divergence
    known: dict[tuple[str, str], dict[str, Any]] = {
        (entry["document"], entry["item"]): entry
        for entry in manifest.get("known_divergences", [])
    }

    if verbose:
        print(f"Canonical source: {canonical}")
        print(f"Documents checked: {len(documents)}")
        if known:
            print(f"Recorded known divergences: {len(known)}")
        print()

    def classify(document: str, item: str, actual_digest: str,
                 expected_digest: str, text: str, line_no: int) -> None:
        """Route a digest comparison into violations or warnings."""
        location = f"{document}:{line_no}"
        record = known.get((document, item))

        if record is not None:
            if actual_digest == record.get("actual_digest"):
                warnings.append(
                    f"{location}: {item} — KNOWN DIVERGENCE (recorded): "
                    f"{record.get('reason', 'no reason given')}"
                )
            else:
                violations.append(
                    f"{location}: {item} CHANGED SINCE THE DIVERGENCE WAS RECORDED\n"
                    f"    recorded {record.get('actual_digest')}\n"
                    f"    actual   {actual_digest}\n"
                    f"    text     {truncate(text)}"
                )
            return

        violations.append(
            f"{location}: {item} DIGEST MISMATCH\n"
            f"    expected {expected_digest}\n"
            f"    actual   {actual_digest}\n"
            f"    text     {truncate(text)}"
        )

    for doc_name in documents:
        path = REPO_ROOT / doc_name
        if not path.exists():
            violations.append(f"{doc_name}: charter document is missing")
            continue

        try:
            laws, tenets = parse_charter(path)
        except ValueError as exc:
            violations.append(str(exc))
            continue

        violations.extend(check_ordinal_alignment(laws, doc_name))

        # --- Laws ---
        for ordinal, expected in expected_laws.items():
            if ordinal not in laws:
                violations.append(
                    f"{doc_name}: '{ordinal} Law' is MISSING (present in manifest)"
                )
                continue

            actual_text, line_no = laws[ordinal]
            actual = digest(actual_text)

            if actual == expected:
                if verbose:
                    print(f"  [OK] {doc_name}:{line_no} {ordinal} Law")
            else:
                classify(doc_name, f"{ordinal} Law", actual, expected, actual_text, line_no)

        for ordinal in laws:
            if ordinal not in expected_laws:
                violations.append(
                    f"{doc_name}: unexpected '{ordinal} Law' not present in manifest"
                )

        # --- Core Tenets ---
        for name, expected in expected_tenets.items():
            if name not in tenets:
                violations.append(
                    f"{doc_name}: Core Tenet '{name}' is MISSING (present in manifest)"
                )
                continue

            actual_text, line_no = tenets[name]
            actual = digest(actual_text)

            if actual == expected:
                if verbose:
                    print(f"  [OK] {doc_name}:{line_no} tenet '{name}'")
            else:
                classify(doc_name, f"Core Tenet '{name}'", actual, expected,
                         actual_text, line_no)

        for name in tenets:
            if name not in expected_tenets:
                violations.append(
                    f"{doc_name}: unexpected Core Tenet '{name}' not present in manifest"
                )

        if verbose:
            print()

    # In strict mode, recorded divergences are promoted to failures.
    if strict and warnings:
        violations.extend(
            f"{w}  [strict mode: treated as a failure]" for w in warnings
        )
        warnings = []

    return violations, warnings


def truncate(text: str, limit: int = 110) -> str:
    return text if len(text) <= limit else text[: limit - 3] + "..."


# --------------------------------------------------------------------------
# Entry point
# --------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Verify the integrity of the Permanent Active Directives.",
    )
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="report every Law and Tenet, not just violations")
    parser.add_argument("--json", action="store_true",
                        help="emit a machine-readable JSON report")
    parser.add_argument("--update", action="store_true",
                        help="regenerate the manifest from the canonical document")
    parser.add_argument("--strict", action="store_true",
                        help="treat recorded known divergences as failures")
    args = parser.parse_args(argv)

    try:
        manifest = load_manifest()
    except (FileNotFoundError, json.JSONDecodeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    except KeyError as exc:
        print(f"ERROR: manifest is malformed — missing key {exc}", file=sys.stderr)
        return 2

    if args.update:
        canonical = REPO_ROOT / manifest["canonical_source"]
        try:
            fresh = build_manifest(canonical, manifest["charter_documents"])
        except (OSError, ValueError) as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 2

        # Preserve recorded divergences. Regenerating digests must never erase a
        # known, review-pending finding.
        fresh["known_divergences"] = manifest.get("known_divergences", [])

        MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
        with MANIFEST_PATH.open("w", encoding="utf-8", newline="\n") as handle:
            json.dump(fresh, handle, indent=2, ensure_ascii=False)
            handle.write("\n")

        print(f"Manifest regenerated: {MANIFEST_PATH.relative_to(REPO_ROOT)}")
        if fresh["known_divergences"]:
            print(f"Preserved {len(fresh['known_divergences'])} recorded divergence(s).")
        print()
        print("REMINDER: changing a Law or Core Tenet is a charter amendment and")
        print("requires approval from the Governance Council. This command records")
        print("what the canonical document currently says; it does not authorise the")
        print("change. The resulting diff must be reviewed before merge.")
        return 0

    violations, warnings = verify(manifest, args.verbose, args.strict)

    if args.json:
        print(json.dumps(
            {
                "control": "LAW10-CHARTER",
                "status": "PASS" if not violations else "FAIL",
                "violation_count": len(violations),
                "warning_count": len(warnings),
                "violations": violations,
                "warnings": warnings,
            },
            indent=2,
        ))
        return 0 if not violations else 1

    if warnings:
        print("RECORDED CHARTER DIVERGENCES", file=sys.stderr)
        print("-" * 70, file=sys.stderr)
        for warning in warnings:
            print(f"  ! {warning}", file=sys.stderr)
        print("-" * 70, file=sys.stderr)
        print(
            "These divergences are recorded in the manifest with a stated reason and "
            "are pending Governance review.",
            file=sys.stderr,
        )
        print()

    if violations:
        print("CHARTER INTEGRITY VIOLATION", file=sys.stderr)
        print("=" * 70, file=sys.stderr)
        for violation in violations:
            print(f"  - {violation}", file=sys.stderr)
        print("=" * 70, file=sys.stderr)
        print(
            f"{len(violations)} violation(s). The Permanent Active Directives are the "
            "supreme governing law of this project.",
            file=sys.stderr,
        )
        print(
            "If this change was intentional, it is a charter amendment and requires "
            "Governance approval before the manifest is regenerated.",
            file=sys.stderr,
        )
        return 1

    if not warnings:
        print("Charter intact. All Laws and Core Tenets match the canonical manifest.")
    else:
        print(
            f"Charter Laws intact. {len(warnings)} recorded Core Tenet divergence(s) "
            "remain pending Governance review."
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
