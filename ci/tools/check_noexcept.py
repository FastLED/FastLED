#!/usr/bin/env python3
"""AST-backed FL_NOEXCEPT enforcement for FastLED-owned src/ code.

The check uses clang-query to find function declarations/definitions in
src/fl/**, src/platforms/**, and src/third_party/** whose parsed AST is not
nothrow, then filters source signatures that are already annotated with
FL_NOEXCEPT/noexcept or have an explicit suppression comment.

Default mode compares current findings against a checked-in baseline. This
keeps normal C++ lint strict for newly introduced misses while allowing the
existing annotation backlog to be ratcheted down deliberately.

Usage:
    uv run python ci/tools/check_noexcept.py
    uv run python ci/tools/check_noexcept.py --scope platforms
    uv run python ci/tools/check_noexcept.py --update-baseline
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_BASELINE = PROJECT_ROOT / "ci" / "tools" / "noexcept_baseline.txt"

_TU_PLATFORMS = "ci/tools/_noexcept_check_platforms_tu.cpp"
_TU_FL = "ci/tools/_noexcept_check_fl_tu.cpp"
_TU_THIRD_PARTY = "ci/tools/_noexcept_check_third_party_tu.cpp"

_SCOPES: dict[str, list[tuple[str, str]]] = {
    "platforms": [(_TU_PLATFORMS, ".*src.platforms.*")],
    "fl": [(_TU_FL, ".*src.fl.*")],
    "third_party": [(_TU_THIRD_PARTY, ".*src.third_party.*")],
    "all": [
        (_TU_PLATFORMS, ".*src.platforms.*"),
        (_TU_FL, ".*src.fl.*"),
        (_TU_THIRD_PARTY, ".*src.third_party.*"),
    ],
}

_COMPILER_ARGS = [
    "-std=c++17",
    "-Isrc",
    "-Isrc/platforms/stub",
    "-DSTUB_PLATFORM",
    "-DARDUINO=10808",
    "-DFASTLED_USE_STUB_ARDUINO",
    "-DFASTLED_STUB_IMPL",
    "-DFASTLED_TESTING",
    "-DFASTLED_NO_AUTO_NAMESPACE",
    "-fno-exceptions",
]

_MATCH_OUTPUT_RE = re.compile(r"(src[\\/]\S+):(\d+):\d+: note: .root. binds here")
_SUPPRESS_RE = re.compile(
    r"//\s*(?:ok\s+no\s+(?:noexcept|FL_NOEXCEPT)|"
    r"noexcept\s+not\s+required|nolint)\b",
    re.IGNORECASE,
)
_HAS_NOEXCEPT_RE = re.compile(r"\b(?:FL_NOEXCEPT|noexcept)\b")
_LAMBDA_SOURCE_RE = re.compile(r"\[[^\]]*\]\s*\(")
_DESTRUCTOR_SOURCE_RE = re.compile(r"(?:^|[^\w:])~\w+\s*\(")
_MACRO_INVOCATION_RE = re.compile(r"^\s*[A-Z][A-Z0-9_]*\s*\(")


class NoexceptCheckError(RuntimeError):
    """Raised when the clang-query based check cannot run."""


@dataclass(frozen=True)
class NoexceptHit:
    """One source signature that still needs an FL_NOEXCEPT decision."""

    path: str
    line: int
    line_text: str
    signature: str

    @property
    def baseline_key(self) -> str:
        return f"{self.path}|{normalize_signature(self.signature)}"


def build_query(file_regex: str) -> str:
    """Build the clang-query matcher for actionable owned functions."""
    matcher = (
        "match functionDecl("
        "unless(isNoThrow()), "
        "unless(isDeleted()), "
        "unless(isDefaulted()), "
        "unless(isImplicit()), "
        "unless(cxxDestructorDecl()), "
        "unless(cxxMethodDecl(ofClass(cxxRecordDecl(isLambda())))), "
        "unless(hasParent(linkageSpecDecl())), "
        f'isExpansionInFileMatching("{file_regex}"))'
    )
    return f"set output diag\n{matcher}\n"


def normalize_signature(signature: str) -> str:
    """Collapse a source signature into a stable one-line baseline key."""
    without_comments = " ".join(
        line.split("//", 1)[0].strip() for line in signature.splitlines()
    )
    return re.sub(r"\s+", " ", without_comments).strip()


def load_baseline(path: Path = DEFAULT_BASELINE) -> Counter[str]:
    """Load the checked-in baseline as a counted set of signature keys."""
    baseline: Counter[str] = Counter()
    if not path.exists():
        return baseline
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        baseline[line] += 1
    return baseline


def write_baseline(hits: list[NoexceptHit], path: Path = DEFAULT_BASELINE) -> None:
    """Write a deterministic baseline for the current AST findings."""
    keys = sorted(hit.baseline_key for hit in hits)
    content = [
        "# Known missing FL_NOEXCEPT signatures for ci/tools/check_noexcept.py.",
        "# Generated with:",
        "#   uv run python ci/tools/check_noexcept.py --scope all --update-baseline",
        "#",
        "# Format: src/path|normalized source signature",
        "# New entries fail the default unified C++ linter.",
        "",
    ]
    content.extend(keys)
    path.write_text("\n".join(content) + "\n", encoding="utf-8")


def diff_against_baseline(
    hits: list[NoexceptHit], baseline: Counter[str]
) -> tuple[list[NoexceptHit], list[str]]:
    """Return non-baselined hits and stale baseline keys."""
    remaining = baseline.copy()
    new_hits: list[NoexceptHit] = []
    current: Counter[str] = Counter()

    for hit in hits:
        key = hit.baseline_key
        current[key] += 1
        if remaining[key] > 0:
            remaining[key] -= 1
        else:
            new_hits.append(hit)

    stale: list[str] = []
    for key, count in (baseline - current).items():
        stale.extend([key] * count)
    return new_hits, stale


def _find_clang_query() -> list[str]:
    """Find clang-query, preferring direct binaries but falling back to uv."""
    system_path = Path("C:/Program Files/LLVM/bin/clang-query.exe")
    if system_path.exists():
        return [str(system_path)]

    cache_path = (
        PROJECT_ROOT / ".cache" / "clang-tools" / "clang" / "bin" / "clang-query.exe"
    )
    if cache_path.exists():
        return [str(cache_path)]

    found = shutil.which("clang-query")
    if found:
        return [found]

    wrapper = shutil.which("clang-tool-chain-query")
    if wrapper:
        return [wrapper]

    uv = shutil.which("uv")
    if uv:
        return [uv, "run", "clang-tool-chain-query"]

    return []


def _read_source_signature(filepath: str, line_num: int) -> tuple[str, str]:
    """Return display line and full source signature for an AST match."""
    full_path = PROJECT_ROOT / filepath
    if not full_path.exists():
        return "", ""

    lines = full_path.read_text(encoding="utf-8", errors="replace").splitlines()
    if line_num < 1 or line_num > len(lines):
        return "", ""

    display_line = lines[line_num - 1].strip()
    signature_lines: list[str] = []
    paren_depth = 0
    found_open = False

    for idx in range(line_num - 1, min(line_num + 39, len(lines))):
        raw = lines[idx].rstrip()
        signature_lines.append(raw)
        code = raw.split("//", 1)[0]
        for ch in code:
            if ch == "(":
                paren_depth += 1
                found_open = True
            elif ch == ")":
                paren_depth -= 1
            elif ch in (";", "{") and found_open and paren_depth == 0:
                return display_line, "\n".join(signature_lines)

    return display_line, "\n".join(signature_lines)


def _signature_is_exempt(signature: str) -> bool:
    """Return True for annotations and documented non-actionable constructs."""
    if not signature:
        return True
    if _SUPPRESS_RE.search(signature):
        return True

    code = "\n".join(line.split("//", 1)[0] for line in signature.splitlines())
    if _HAS_NOEXCEPT_RE.search(code):
        return True
    if _LAMBDA_SOURCE_RE.search(code):
        return True
    if _DESTRUCTOR_SOURCE_RE.search(code):
        return True
    if _MACRO_INVOCATION_RE.match(code):
        return True
    if 'extern "C"' in code:
        return True
    return False


def _run_clang_query(
    clang_query: list[str], tu: str, file_regex: str
) -> list[NoexceptHit]:
    """Run clang-query and return filtered missing-FL_NOEXCEPT hits."""
    result = subprocess.run(
        [*clang_query, tu, "--", *_COMPILER_ARGS],
        input=build_query(file_regex),
        capture_output=True,
        text=True,
        cwd=str(PROJECT_ROOT),
        timeout=300,
    )
    output = result.stdout + result.stderr
    if result.returncode != 0:
        raise NoexceptCheckError(output.strip() or "clang-query failed")
    if (
        "Error parsing argument" in output
        or "Error parsing matcher" in output
        or "Matcher not found" in output
    ):
        raise NoexceptCheckError(output.strip())

    hits: list[NoexceptHit] = []
    seen: set[tuple[str, int]] = set()
    for match in _MATCH_OUTPUT_RE.finditer(output):
        filepath = match.group(1).replace("\\", "/")
        line_num = int(match.group(2))
        key = (filepath, line_num)
        if key in seen:
            continue
        seen.add(key)

        line_text, signature = _read_source_signature(filepath, line_num)
        if _signature_is_exempt(signature):
            continue
        hits.append(
            NoexceptHit(
                path=filepath,
                line=line_num,
                line_text=line_text,
                signature=signature,
            )
        )
    return hits


def find_missing_noexcept(scope: str = "all") -> list[NoexceptHit]:
    """Find current non-exempt missing-FL_NOEXCEPT signatures."""
    clang_query = _find_clang_query()
    if not clang_query:
        raise NoexceptCheckError(
            "clang-query not found. Install LLVM or the clang-tool-chain package."
        )

    all_hits: list[NoexceptHit] = []
    for tu, file_regex in _SCOPES[scope]:
        if not (PROJECT_ROOT / tu).exists():
            raise NoexceptCheckError(f"translation unit not found: {tu}")
        all_hits.extend(_run_clang_query(clang_query, tu, file_regex))
    return all_hits


def _print_hits(hits: list[NoexceptHit]) -> None:
    by_file: dict[str, list[NoexceptHit]] = {}
    for hit in hits:
        by_file.setdefault(hit.path, []).append(hit)

    for filepath in sorted(by_file):
        print(f"{filepath}:")
        for hit in sorted(by_file[filepath], key=lambda item: item.line):
            print(f"  Line {hit.line}: {hit.line_text}")
        print()


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Check for missing FL_NOEXCEPT in src/fl, src/platforms, and "
            "src/third_party using clang-query AST analysis."
        )
    )
    parser.add_argument(
        "--scope",
        choices=sorted(_SCOPES.keys()),
        default="all",
        help="Which owned src scope to check (default: all)",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        default=DEFAULT_BASELINE,
        help="Baseline file for existing missing annotations",
    )
    parser.add_argument(
        "--no-baseline",
        action="store_true",
        help="Report every current finding instead of only new findings",
    )
    parser.add_argument(
        "--update-baseline",
        action="store_true",
        help="Rewrite the baseline with the current findings",
    )
    args = parser.parse_args()

    try:
        hits = find_missing_noexcept(args.scope)
    except NoexceptCheckError as exc:
        print(f"ERROR: {exc}")
        return 1

    if args.update_baseline:
        write_baseline(hits, args.baseline)
        print(f"Wrote {len(hits)} FL_NOEXCEPT baseline entries to {args.baseline}")
        return 0

    if args.no_baseline:
        report_hits = hits
        stale: list[str] = []
    else:
        report_hits, stale = diff_against_baseline(hits, load_baseline(args.baseline))

    if stale:
        print(f"NOTE: {len(stale)} stale FL_NOEXCEPT baseline entrie(s) can be pruned.")
        print("      Re-run with --update-baseline after intentional cleanup.")
        print()

    if not report_hits:
        print("All non-baselined owned src functions have FL_NOEXCEPT.")
        print(f"Known baseline entries: {len(hits) - len(report_hits)}")
        return 0

    print(f"Found {len(report_hits)} non-baselined function(s) missing FL_NOEXCEPT:")
    print()
    _print_hits(report_hits)
    print(
        "Add FL_NOEXCEPT, add a documented suppression comment, or update the "
        "baseline only for deliberate existing debt."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
