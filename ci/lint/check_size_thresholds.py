#!/usr/bin/env python3
"""Enforce frozen binary-size thresholds in `.github/workflows/check_*_size.yml`.

CI agents have repeatedly tried to "fix" red size-check workflows by raising the
`max_size` / `max_size_apa102` thresholds instead of fixing the underlying
regression. The thresholds in this file are the canonical ceilings. Any change
to a number in a `check_*_size.yml` workflow must be matched by an explicit
update to ``FROZEN_THRESHOLDS`` below — by design, the friction of touching
two places in one PR forces a maintainer to actually consider whether the
raise is justified.

If `bash lint` reports a violation here, the correct response is one of:
    1. Revert the YAML edit — the size regression that prompted it is real and
       must be fixed in code or in the build system.
    2. Open a PR that updates both the YAML and ``FROZEN_THRESHOLDS`` here,
       with an issue link explaining why the new ceiling is correct.

Failure modes deliberately blocked:
    - Agent silently bumps esp32 from 330000 → 700000 to clear CI.
    - Agent adds a new check_*_size.yml workflow without registering its frozen
      thresholds here (the linter fails on any unknown file).
    - Agent removes a `max_size:` line from an existing workflow.

This check is ERROR mode by default — there is no warn-only fallback. The
whole point is that the threshold values cannot drift silently.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import NamedTuple


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
WORKFLOWS_DIR = PROJECT_ROOT / ".github" / "workflows"

# ============================================================================
# FROZEN SIZE THRESHOLDS — single source of truth.
#
# To change a threshold:
#   1. Open an issue justifying the new ceiling (real architectural change,
#      not "CI is red and I want it green").
#   2. Edit the matching `.github/workflows/check_<board>_size.yml` value.
#   3. Edit the matching entry below to match.
#   4. Reference the issue # in the PR body. CodeRabbit will flag the diff
#      and the lockdown rule in `.coderabbit.yaml` will require maintainer
#      sign-off.
#
# Each value is a frozenset of integers. Multiple values are allowed for
# workflows that legitimately run the same target with different configs
# (see `check_uno_size.yml`'s `build_no_forced_inline` job which uses -1
# as a "no check" sentinel).
# ============================================================================
FROZEN_THRESHOLDS: dict[str, dict[str, frozenset[int]]] = {
    "check_bluepill_size.yml": {
        "max_size": frozenset({55000}),
        "max_size_apa102": frozenset({45000}),
    },
    "check_esp32_size.yml": {
        "max_size": frozenset({330000}),
        "max_size_apa102": frozenset({330000}),
    },
    "check_teensy30_size.yml": {
        "max_size": frozenset({60000}),
        "max_size_apa102": frozenset({50000}),
    },
    "check_teensy31_size.yml": {
        "max_size": frozenset({80000}),
        "max_size_apa102": frozenset({65000}),
    },
    "check_teensy32_size.yml": {
        "max_size": frozenset({80000}),
        "max_size_apa102": frozenset({65000}),
    },
    "check_teensy35_size.yml": {
        "max_size": frozenset({100000}),
        "max_size_apa102": frozenset({85000}),
    },
    "check_teensy36_size.yml": {
        "max_size": frozenset({120000}),
        "max_size_apa102": frozenset({100000}),
    },
    "check_teensy41_size.yml": {
        "max_size": frozenset({120000}),
        # 165000 was bumped from 88000 per #2802; the real ceiling is 88000
        # once #2802's fl::ifstream / fl::AsyncLog* over-link is fixed.
        "max_size_apa102": frozenset({165000}),
    },
    "check_teensylc_size.yml": {
        "max_size": frozenset({35000}),
        "max_size_apa102": frozenset({30000}),
    },
    "check_uno_size.yml": {
        # `build` job: hard check. `build_no_forced_inline` job: -1 (no check).
        "max_size": frozenset({11000, -1}),
        "max_size_apa102": frozenset({9300, -1}),
    },
}


_VALUE_RE = re.compile(r"^\s*(max_size|max_size_apa102)\s*:\s*(-?\d+)\s*$")


class Violation(NamedTuple):
    file: str
    line_no: int
    key: str
    found: int
    expected: frozenset[int]
    kind: str  # "unknown_file" | "unknown_key" | "wrong_value" | "missing_key"


def _scan_file(path: Path) -> list[tuple[int, str, int]]:
    """Return [(line_no, key, value), ...] for every max_size{,_apa102} line."""
    hits: list[tuple[int, str, int]] = []
    text = path.read_text(encoding="utf-8", errors="replace")
    for line_no, line in enumerate(text.splitlines(), start=1):
        # Skip commented-out lines so the lockdown banner doesn't trip the regex.
        stripped = line.lstrip()
        if stripped.startswith("#"):
            continue
        m = _VALUE_RE.match(line)
        if m:
            hits.append((line_no, m.group(1), int(m.group(2))))
    return hits


def check() -> list[Violation]:
    """Walk every `.github/workflows/check_*_size.yml` and validate."""
    violations: list[Violation] = []

    actual_files = sorted(p.name for p in WORKFLOWS_DIR.glob("check_*_size.yml"))
    known_files = set(FROZEN_THRESHOLDS.keys())

    # Unknown file: a new check_*_size.yml workflow without a frozen entry.
    for name in actual_files:
        if name not in known_files:
            violations.append(
                Violation(
                    file=name,
                    line_no=0,
                    key="<file>",
                    found=0,
                    expected=frozenset(),
                    kind="unknown_file",
                )
            )

    # Missing file: an entry in FROZEN_THRESHOLDS whose YAML was deleted.
    for name in known_files - set(actual_files):
        violations.append(
            Violation(
                file=name,
                line_no=0,
                key="<file>",
                found=0,
                expected=frozenset(),
                kind="missing_file",
            )
        )

    for name in actual_files:
        if name not in known_files:
            continue
        path = WORKFLOWS_DIR / name
        hits = _scan_file(path)
        expected_spec = FROZEN_THRESHOLDS[name]

        # Track which keys actually appeared so we can flag missing ones.
        seen_keys: set[str] = set()

        for line_no, key, value in hits:
            seen_keys.add(key)
            allowed = expected_spec.get(key)
            if allowed is None:
                violations.append(
                    Violation(
                        file=name,
                        line_no=line_no,
                        key=key,
                        found=value,
                        expected=frozenset(),
                        kind="unknown_key",
                    )
                )
                continue
            if value not in allowed:
                violations.append(
                    Violation(
                        file=name,
                        line_no=line_no,
                        key=key,
                        found=value,
                        expected=allowed,
                        kind="wrong_value",
                    )
                )

        for required_key in expected_spec.keys():
            if required_key not in seen_keys:
                violations.append(
                    Violation(
                        file=name,
                        line_no=0,
                        key=required_key,
                        found=0,
                        expected=expected_spec[required_key],
                        kind="missing_key",
                    )
                )

    return violations


def _print_violations(violations: list[Violation]) -> None:
    print()
    print("=" * 80)
    print(
        f"[size-thresholds-locked] ERROR — {len(violations)} frozen-threshold "
        f"violation(s) in `.github/workflows/check_*_size.yml`:"
    )
    print("=" * 80)
    print(
        "These thresholds are frozen by ci/lint/check_size_thresholds.py.\n"
        "Raising a number here to make CI green is forbidden — fix the\n"
        "underlying regression instead, OR (if the bump is genuinely\n"
        "justified) update both the YAML and FROZEN_THRESHOLDS in the lint\n"
        "script in one PR, with an issue link.\n"
    )
    for v in violations:
        if v.kind == "unknown_file":
            print(
                f"  {v.file}: new workflow has no entry in FROZEN_THRESHOLDS. "
                f"Add a frozenset for it in ci/lint/check_size_thresholds.py."
            )
        elif v.kind == "missing_file":
            print(
                f"  {v.file}: workflow was deleted but still listed in "
                f"FROZEN_THRESHOLDS. Remove the entry."
            )
        elif v.kind == "unknown_key":
            print(
                f"  {v.file}:{v.line_no}: unknown key `{v.key}: {v.found}` — "
                f"not in FROZEN_THRESHOLDS for this file."
            )
        elif v.kind == "wrong_value":
            allowed = ", ".join(str(x) for x in sorted(v.expected))
            print(
                f"  {v.file}:{v.line_no}: `{v.key}: {v.found}` does not match "
                f"frozen value(s) {{{allowed}}}."
            )
        elif v.kind == "missing_key":
            allowed = ", ".join(str(x) for x in sorted(v.expected))
            print(
                f"  {v.file}: missing required `{v.key}` line (frozen value "
                f"{{{allowed}}})."
            )
    print()
    print("=" * 80)


def run() -> bool:
    """Return True on clean, False on any violation."""
    if not WORKFLOWS_DIR.exists():
        return True
    violations = check()
    if not violations:
        print(
            "✅ Size-thresholds lockdown: "
            f"all {len(FROZEN_THRESHOLDS)} check_*_size.yml workflow(s) match "
            "frozen values."
        )
        return True
    _print_violations(violations)
    return False


def main(argv: list[str] | None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Validate `.github/workflows/check_*_size.yml` thresholds against "
            "the frozen list in ci/lint/check_size_thresholds.py."
        ),
    )
    parser.parse_args(argv)
    ok = run()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(None))
