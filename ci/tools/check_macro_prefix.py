#!/usr/bin/env python3
"""Block *new* `FASTLED_*` macro names in FastLED-owned source.

`agents/docs/cpp-standards.md` is explicit that new macros take the `FL_`
prefix -- `FL_IS_<PLATFORM>` for platform detection, `FL_<COMPONENT>_<NAME>`
for per-component flags. The rule existed and was enforced by nobody:
`FASTLED_ESP8266_EMBEDDED_FS` was caught by review on one PR while the
identical `FASTLED_SAMD51_HW_SPI` merged on another and sat on master
(FastLED#4021). Two instances of one mistake, one caught by luck, is what a
rule with no linter looks like.

Renaming the existing 800-odd is not on the table -- most are public knobs
users set in their own sketches, and renaming those breaks them. So this is a
ratchet: the known names are baselined and a *new* one fails.

Two decisions worth stating, because the obvious implementations get both
wrong:

* **Names, not occurrences.** The standard is about what a macro is called.
  Baselining per-file counts would fire when a refactor moves an existing
  `#ifdef` between files -- noise that trains people to regenerate the
  baseline without reading it, which is how a ratchet dies.
* **References, not just definitions.** `FASTLED_SAMD51_HW_SPI` -- the defect
  that motivated this -- was never `#define`d here. It was a user-supplied
  opt-in, appearing only as `#if defined(...)`. A checker that scanned
  definitions alone would not have caught the case it exists for.

Usage:
    uv run python ci/tools/check_macro_prefix.py
    uv run python ci/tools/check_macro_prefix.py --update-baseline
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from typeguard import typechecked


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "src"
BASELINE_PATH = Path(__file__).resolve().parent / "macro_prefix_baseline.txt"

# Any preprocessor line that names a macro: a definition, or a conditional
# that tests one.
PREPROCESSOR_RE = re.compile(r"^\s*#\s*(define|undef|if|ifdef|ifndef|elif)\b")
NAME_RE = re.compile(r"FASTLED_[A-Za-z0-9_]+")

# Vendored code is not FastLED-owned and is not renamed to suit our standard.
EXCLUDED_PREFIXES = ("third_party/",)

# An escape hatch for the genuine cases -- a name something outside this repo
# already publishes. A reason is required, because a bare marker is
# indistinguishable from someone silencing the check.
SUPPRESSION_RE = re.compile(r"fl-lint:\s*macro-prefix-ok\s*\(([^)]+)\)")

SOURCE_SUFFIXES = (".h", ".hpp", ".cpp")


@typechecked
def is_excluded(relative: str) -> bool:
    for prefix in EXCLUDED_PREFIXES:
        if relative.startswith(prefix):
            return True
    return False


@typechecked
def names_in(text: str) -> set[str]:
    """Every `FASTLED_*` name this file defines or tests, minus suppressed ones."""

    lines = text.split("\n")
    found: set[str] = set()
    for index, line in enumerate(lines):
        if PREPROCESSOR_RE.match(line) is None:
            continue
        if SUPPRESSION_RE.search(line):
            continue
        if index > 0 and SUPPRESSION_RE.search(lines[index - 1]):
            continue
        for name in NAME_RE.findall(line):
            found.add(name)
    return found


@typechecked
def scan(root: Path) -> dict[str, str]:
    """Known names mapped to the first file that mentions each."""

    where: dict[str, str] = {}
    for path in sorted(root.rglob("*")):
        if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        if is_excluded(relative):
            continue
        for name in sorted(
            names_in(path.read_text(encoding="utf-8", errors="replace"))
        ):
            where.setdefault(name, relative)
    return where


@typechecked
def load_baseline(path: Path) -> set[str]:
    known: set[str] = set()
    if not path.is_file():
        return known
    for line in path.read_text(encoding="utf-8").split("\n"):
        stripped = line.strip()
        if stripped and not stripped.startswith("#"):
            known.add(stripped)
    return known


@typechecked
def render_baseline(names: list[str]) -> str:
    lines = [
        "# Known `FASTLED_*` macro names in src/, for",
        "# ci/tools/check_macro_prefix.py. Regenerate with:",
        "#   uv run python ci/tools/check_macro_prefix.py --update-baseline",
        "#",
        "# One name per line. Names may disappear freely -- renaming a macro to",
        "# FL_ just removes it. A NEW name fails, so introducing one has to be a",
        "# deliberate edit here that a reviewer sees.",
        "",
    ]
    lines.extend(names)
    return "\n".join(lines) + "\n"


@typechecked
def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--update-baseline", action="store_true")
    parsed = parser.parse_args(argv)

    found = scan(SOURCE_ROOT)

    if parsed.update_baseline:
        BASELINE_PATH.write_text(render_baseline(sorted(found)), encoding="utf-8")
        print(f"wrote {BASELINE_PATH.relative_to(PROJECT_ROOT)} ({len(found)} names)")
        return 0

    baseline = load_baseline(BASELINE_PATH)
    new_names = sorted(set(found) - baseline)
    if not new_names:
        print(f"macro prefix: no new FASTLED_* names ({len(baseline)} baselined)")
        return 0

    print("New `FASTLED_*` macro name(s):")
    for name in new_names:
        print(f"  {name}  (src/{found[name]})")
    print()
    print("agents/docs/cpp-standards.md: new macros take the FL_ prefix --")
    print("  FL_IS_<PLATFORM> for platform detection,")
    print("  FL_<COMPONENT>_<NAME> for per-component flags.")
    print()
    print("If the name genuinely has to match one published outside this repo,")
    print("add `// fl-lint: macro-prefix-ok(<reason>)` on or above the line.")
    print("Renaming an existing macro instead? Drop it from the baseline with")
    print("  uv run python ci/tools/check_macro_prefix.py --update-baseline")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
