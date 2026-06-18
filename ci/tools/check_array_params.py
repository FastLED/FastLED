#!/usr/bin/env python3
"""AST-backed enforcement against decayed C array parameters.

C++ adjusts function parameters spelled as arrays, such as ``float out[2]``,
to pointer parameters. The extent is therefore documentation only. This check
uses clang-query to find FastLED-owned function declarations with pointer
parameters, then inspects the source-spelled signature to catch parameters
written with the misleading array syntax.

Usage:
    uv run python ci/tools/check_array_params.py
    uv run python ci/tools/check_array_params.py --scope platforms
    uv run python ci/tools/check_array_params.py --update-baseline
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
DEFAULT_BASELINE = PROJECT_ROOT / "ci" / "tools" / "array_param_baseline.txt"

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
_SUPPRESS_RE = re.compile(r"//\s*(?:ok\s+array\s+parameter|nolint)\b", re.IGNORECASE)
_MACRO_INVOCATION_RE = re.compile(r"^\s*[A-Z][A-Z0-9_]*\s*\(")
_ARRAY_PARAM_RE = re.compile(
    r"\b(?P<name>[A-Za-z_]\w*)\s*(?P<extent>(?:\[[^\]]*\]\s*)+)"
)
_IRAM_RE = re.compile(r"\bFL_IRAM\b")


class ArrayParamCheckError(RuntimeError):
    """Raised when the clang-query based check cannot run."""


@dataclass(frozen=True)
class ArrayParamFinding:
    """One source-spelled decayed array parameter."""

    name: str
    spelling: str
    is_iram: bool


@dataclass(frozen=True)
class ArrayParamHit:
    """One source signature with at least one decayed array parameter."""

    path: str
    line: int
    line_text: str
    signature: str
    findings: tuple[ArrayParamFinding, ...]

    @property
    def baseline_key(self) -> str:
        return f"{self.path}|{normalize_signature(self.signature)}"


def build_query(file_regex: str) -> str:
    """Build the clang-query matcher for candidate owned functions."""
    matcher = (
        "match functionDecl("
        "unless(isImplicit()), "
        "unless(cxxMethodDecl(ofClass(cxxRecordDecl(isLambda())))), "
        "unless(hasParent(linkageSpecDecl())), "
        "hasAnyParameter(parmVarDecl(hasType(pointerType()))), "
        f'isExpansionInFileMatching("{file_regex}")).bind("root")'
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


def write_baseline(hits: list[ArrayParamHit], path: Path = DEFAULT_BASELINE) -> None:
    """Write a deterministic baseline for the current AST findings."""
    keys = sorted(hit.baseline_key for hit in hits)
    content = [
        "# Known decayed C array parameters for ci/tools/check_array_params.py.",
        "# Generated with:",
        "#   uv run python ci/tools/check_array_params.py --scope all --update-baseline",
        "#",
        "# Format: src/path|normalized source signature",
        "# New entries fail the default unified C++ linter.",
        "",
    ]
    content.extend(keys)
    path.write_text("\n".join(content) + "\n", encoding="utf-8")


def diff_against_baseline(
    hits: list[ArrayParamHit], baseline: Counter[str]
) -> tuple[list[ArrayParamHit], list[str]]:
    """Return non-baselined hits and stale baseline keys."""
    remaining = baseline.copy()
    new_hits: list[ArrayParamHit] = []
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

    for idx in range(line_num - 1, min(line_num + 59, len(lines))):
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
    """Return True for documented non-actionable constructs."""
    if not signature:
        return True
    if _SUPPRESS_RE.search(signature):
        return True

    code = "\n".join(line.split("//", 1)[0] for line in signature.splitlines())
    if _MACRO_INVOCATION_RE.match(code):
        return True
    if 'extern "C"' in code:
        return True
    return False


def _strip_comments(signature: str) -> str:
    """Remove comments while preserving enough shape for signature parsing."""
    without_block = re.sub(r"/\*.*?\*/", "", signature, flags=re.DOTALL)
    return "\n".join(line.split("//", 1)[0] for line in without_block.splitlines())


def _extract_parameter_text(signature: str) -> str:
    """Return the first top-level function parameter list, without parens."""
    code = _strip_comments(signature)
    start = code.find("(")
    if start < 0:
        return ""

    depth = 0
    for index in range(start, len(code)):
        ch = code[index]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return code[start + 1 : index]
    return ""


def _split_parameters(parameter_text: str) -> list[str]:
    """Split a parameter list on top-level commas."""
    params: list[str] = []
    start = 0
    paren = angle = bracket = brace = 0
    for index, ch in enumerate(parameter_text):
        if ch == "(":
            paren += 1
        elif ch == ")":
            paren -= 1
        elif ch == "<":
            angle += 1
        elif ch == ">" and angle:
            angle -= 1
        elif ch == "[":
            bracket += 1
        elif ch == "]":
            bracket -= 1
        elif ch == "{":
            brace += 1
        elif ch == "}":
            brace -= 1
        elif ch == "," and not any((paren, angle, bracket, brace)):
            params.append(parameter_text[start:index].strip())
            start = index + 1
    tail = parameter_text[start:].strip()
    if tail:
        params.append(tail)
    return params


def _find_array_params_in_signature(signature: str) -> tuple[ArrayParamFinding, ...]:
    """Find source-spelled array parameters that decay to pointers."""
    if _signature_is_exempt(signature):
        return ()

    is_iram = bool(_IRAM_RE.search(_strip_comments(signature)))
    findings: list[ArrayParamFinding] = []
    for param in _split_parameters(_extract_parameter_text(signature)):
        for match in _ARRAY_PARAM_RE.finditer(param):
            name = match.group("name")
            extent = re.sub(r"\s+", "", match.group("extent"))
            spelling = f"{name}{extent}"
            findings.append(
                ArrayParamFinding(name=name, spelling=spelling, is_iram=is_iram)
            )
            break
    return tuple(findings)


def _run_clang_query(
    clang_query: list[str], tu: str, file_regex: str
) -> list[ArrayParamHit]:
    """Run clang-query and return source-filtered array parameter hits."""
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
        raise ArrayParamCheckError(output.strip() or "clang-query failed")
    if (
        "Error parsing argument" in output
        or "Error parsing matcher" in output
        or "Matcher not found" in output
    ):
        raise ArrayParamCheckError(output.strip())

    hits: list[ArrayParamHit] = []
    seen: set[tuple[str, int]] = set()
    for match in _MATCH_OUTPUT_RE.finditer(output):
        filepath = match.group(1).replace("\\", "/")
        line_num = int(match.group(2))
        key = (filepath, line_num)
        if key in seen:
            continue
        seen.add(key)

        line_text, signature = _read_source_signature(filepath, line_num)
        findings = _find_array_params_in_signature(signature)
        if not findings:
            continue
        hits.append(
            ArrayParamHit(
                path=filepath,
                line=line_num,
                line_text=line_text,
                signature=signature,
                findings=findings,
            )
        )
    return hits


def find_decayed_array_params(scope: str = "all") -> list[ArrayParamHit]:
    """Find current non-exempt decayed array parameter signatures."""
    clang_query = _find_clang_query()
    if not clang_query:
        raise ArrayParamCheckError(
            "clang-query not found. Install LLVM or the clang-tool-chain package."
        )

    all_hits: list[ArrayParamHit] = []
    for tu, file_regex in _SCOPES[scope]:
        if not (PROJECT_ROOT / tu).exists():
            raise ArrayParamCheckError(f"translation unit not found: {tu}")
        all_hits.extend(_run_clang_query(clang_query, tu, file_regex))
    return all_hits


def _diagnostic_for_hit(hit: ArrayParamHit) -> str:
    """Return the lint diagnostic for a hit."""
    spellings = ", ".join(finding.spelling for finding in hit.findings)
    if any(finding.is_iram for finding in hit.findings):
        replacement = "Use 'T (&value)[N]' for ISR-safe fixed-size parameters."
    else:
        replacement = "Use 'fl::span<T, N>' or 'T (&value)[N]'."
    return (
        f"Decayed C array parameter(s): {spellings}. Function parameter arrays "
        f"decay to pointers, so the size is not enforced. {replacement} "
        "Suppress only intentional C ABI/platform cases with '// ok array parameter'."
    )


def _print_hits(hits: list[ArrayParamHit]) -> None:
    by_file: dict[str, list[ArrayParamHit]] = {}
    for hit in hits:
        by_file.setdefault(hit.path, []).append(hit)

    for filepath in sorted(by_file):
        print(f"{filepath}:")
        for hit in sorted(by_file[filepath], key=lambda item: item.line):
            print(f"  Line {hit.line}: {hit.line_text}")
            print(f"    {_diagnostic_for_hit(hit)}")
        print()


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Check for decayed C array parameters in src/fl, src/platforms, "
            "and src/third_party using clang-query AST analysis."
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
        help="Baseline file for existing decayed array parameters",
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
        hits = find_decayed_array_params(args.scope)
    except ArrayParamCheckError as exc:
        print(f"ERROR: {exc}")
        return 1

    if args.update_baseline:
        write_baseline(hits, args.baseline)
        print(f"Wrote {len(hits)} array parameter baseline entries to {args.baseline}")
        return 0

    if args.no_baseline:
        report_hits = hits
        stale: list[str] = []
    else:
        report_hits, stale = diff_against_baseline(hits, load_baseline(args.baseline))

    if stale:
        print(
            f"NOTE: {len(stale)} stale array parameter baseline entrie(s) can be pruned."
        )
        print("      Re-run with --update-baseline after intentional cleanup.")
        print()

    if not report_hits:
        print("All non-baselined owned src functions avoid decayed array parameters.")
        print(f"Known baseline entries: {len(hits) - len(report_hits)}")
        return 0

    print(f"Found {len(report_hits)} non-baselined decayed array parameter use(s):")
    print()
    _print_hits(report_hits)
    print(
        "Use array references for ISR/FL_IRAM APIs, fl::span<T, N> or array "
        "references elsewhere, or add a documented suppression for deliberate "
        "C ABI/platform compatibility."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
