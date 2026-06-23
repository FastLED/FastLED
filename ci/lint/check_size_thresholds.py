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
#   4. Update the row in `docs/SIZE_THRESHOLD_HISTORY.md` (status, notes,
#      historical bump table for that board).
#   5. Reference the issue # in the PR body. CodeRabbit will flag the diff
#      and the lockdown rule in `.coderabbit.yaml` will require maintainer
#      sign-off.
#
# Each value is a frozenset of integers. Multiple values are allowed for
# workflows that legitimately run the same target with different configs
# (see `check_uno_size.yml`'s `build_no_forced_inline` job which uses -1
# as a "no check" sentinel).
#
# Each entry is annotated as one of:
#   - real ceiling — actual chip / framework limit, no follow-up required.
#   - band-aid (#NNNN) — value was raised to clear CI; real ceiling is lower
#     and would be restored once the linked regression is fixed.
#
# See `docs/SIZE_THRESHOLD_HISTORY.md` for the full audit and per-board bump
# history.
# ============================================================================
FROZEN_THRESHOLDS: dict[str, dict[str, frozenset[int]]] = {
    "check_bluepill_size.yml": {
        # real ceiling — STM32F103C8 has 64 KB flash; never bumped.
        "max_size": frozenset({55000}),
        "max_size_apa102": frozenset({45000}),
    },
    "check_esp32_size.yml": {
        # real ceiling (currently RED on #3298) — 330 KB is the canonical
        # ceiling restored by PR #3268 / preserved by PR #3303. CI stays red
        # until #3298 (fbuild ESP32 board-flag propagation) is fixed; the
        # red signal is the point — DO NOT raise to make CI green. The 700K
        # band-aids (PR #2790, PR #3295) were both reverted.
        "max_size": frozenset({330000}),
        "max_size_apa102": frozenset({330000}),
    },
    "check_teensy30_size.yml": {
        # real ceiling — Teensy 3.0 (MK20DX128) 128 KB flash; never bumped.
        "max_size": frozenset({60000}),
        "max_size_apa102": frozenset({50000}),
    },
    "check_teensy31_size.yml": {
        # real ceiling — Teensy 3.1 (MK20DX256) 256 KB flash; never bumped.
        "max_size": frozenset({80000}),
        "max_size_apa102": frozenset({65000}),
    },
    "check_teensy32_size.yml": {
        # real ceiling — Teensy 3.2 (MK20DX256) 256 KB flash; never bumped.
        "max_size": frozenset({80000}),
        "max_size_apa102": frozenset({65000}),
    },
    "check_teensy35_size.yml": {
        # real ceiling — Teensy 3.5 (MK64FX512) 512 KB flash; never bumped.
        "max_size": frozenset({100000}),
        "max_size_apa102": frozenset({85000}),
    },
    "check_teensy36_size.yml": {
        # real ceiling — Teensy 3.6 (MK66FX1M0) 1 MB flash; never bumped.
        "max_size": frozenset({120000}),
        "max_size_apa102": frozenset({100000}),
    },
    "check_teensy41_size.yml": {
        # real ceiling — Teensy 4.1 (MIMXRT1062) Blink reflects real growth.
        "max_size": frozenset({120000}),
        # band-aid (#2802) — bumped from 88000 in PR #2804 to clear CI
        # after fl::ifstream / fl::posix_filebuf / fl::strerror /
        # fl::AsyncLog* over-linked into the Apa102 build. Real ceiling is
        # 88000 once #2802 is fixed; current real size ≈148476 B.
        "max_size_apa102": frozenset({165000}),
    },
    "check_teensylc_size.yml": {
        # real ceiling — Teensy LC (MKL26Z64) 64 KB flash; never bumped.
        "max_size": frozenset({35000}),
        "max_size_apa102": frozenset({30000}),
    },
    "check_uno_size.yml": {
        # real ceiling — ATmega328P 32 KB flash. Apa102 was tightened
        # 12050 → 9300 in 7edaf80f0 (real optimisation, not a bump).
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
