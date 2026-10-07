#!/usr/bin/env python3
"""AST-backed FL_NO_EXCEPT enforcement for FastLED-owned src/ code.

The check uses clang-query to find function declarations/definitions in
root src files, src/fl/**, src/platforms/**, and src/third_party/** whose parsed AST is not
nothrow, then filters documented source suppressions and non-actionable
constructs. Contract and lambda exclusions are determined by the AST.

Every non-exempt finding fails the check. Historical baselines cannot suppress
missing annotations or be regenerated to grandfather existing findings.

Usage:
    uv run python ci/tools/check_noexcept.py
    uv run python ci/tools/check_noexcept.py --scope platforms
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path

from running_process import PIPE, RunningProcess


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

_TU_PLATFORMS = "ci/tools/_noexcept_check_platforms_tu.cpp"
_TU_FL_ALL = "ci/tools/_noexcept_check_fl_tu.cpp"  # legacy fallback only
_TU_FX_HEADERS = "ci/tools/_noexcept_check_fx_headers_tu.cpp"
_TU_THIRD_PARTY = "ci/tools/_noexcept_check_third_party_tu.cpp"  # legacy fallback only

# Canonical per-subdir TU shims live in src/fl/build/ - the same files
# the meson unity build already maintains. Reusing them for clang-query
# means we get one parallel work unit per fl/<subdir> (~23 of them today)
# without inventing or generating any new TU files.
_FL_BUILD_DIR = "src/fl/build"
_FL_BUILD_PLATFORMS = "src/fl/build/platforms+.cpp"
_FL_BUILD_THIRD_PARTY = "src/fl/build/third_party+.cpp"


def _fl_subdir_tus() -> list[tuple[str, str]]:
    """Enumerate src/fl/build/fl.*+.cpp as (tu_path, file_regex) pairs.

    Each shim under src/fl/build/ matches `fl.<subdir>+.cpp` (with `.`
    as the path separator, so fl.system.sd+.cpp -> src/fl/system/sd/).
    Returns a list of (relative_tu_path, narrowing_file_regex) so each
    clang-query invocation only fires matches against files actually
    parsed by that TU.
    """
    build_dir = PROJECT_ROOT / _FL_BUILD_DIR
    if not build_dir.is_dir():
        return [(_TU_FL_ALL, ".*src.fl.*")]
    entries: list[tuple[str, str]] = []
    for path in sorted(build_dir.glob("fl.*+.cpp")):
        name = path.name  # e.g. "fl.system.sd+.cpp"
        # strip leading "fl." and trailing "+.cpp"
        subdir_dotted = name[3 : -len("+.cpp")]  # "system.sd"
        # `.` in clang-query's file_regex matches any char including /
        file_regex = f".*src.fl.{subdir_dotted}.*"
        rel_tu = path.relative_to(PROJECT_ROOT).as_posix()
        entries.append((rel_tu, file_regex))
    if not entries:
        return [(_TU_FL_ALL, ".*src.fl.*")]
    return entries


def _scope_tus(scope: str) -> list[tuple[str, str]]:
    """Resolve a scope name to its concrete TU list.

    fl/ expands to ~23 per-subdir TUs via src/fl/build/fl.*+.cpp so
    clang-query can fan out across cores instead of parsing the
    whole subsystem in one shot.
    """
    platforms_tu = (
        _FL_BUILD_PLATFORMS
        if (PROJECT_ROOT / _FL_BUILD_PLATFORMS).is_file()
        else _TU_PLATFORMS
    )
    third_party_tu = (
        _FL_BUILD_THIRD_PARTY
        if (PROJECT_ROOT / _FL_BUILD_THIRD_PARTY).is_file()
        else _TU_THIRD_PARTY
    )
    if scope == "platforms":
        return [(platforms_tu, ".*src.platforms.*")]
    if scope == "third_party":
        return [(third_party_tu, ".*src.third_party.*")]
    if scope == "fl":
        return [
            *_fl_subdir_tus(),
            (_TU_FX_HEADERS, ".*src.fl.fx.*"),
            # Public controllers are included by FastLED.h in the root router.
            ("src/fl/build/src.cpp", ".*src.fl.*"),
        ]
    if scope == "all":
        return [
            ("src/fl/build/src.cpp", ".*src.*"),
            (platforms_tu, ".*src.platforms.*"),
            *_fl_subdir_tus(),
            (_TU_FX_HEADERS, ".*src.fl.fx.*"),
            (third_party_tu, ".*src.third_party.*"),
        ]
    return _SCOPES[scope]


_SCOPES: dict[str, list[tuple[str, str]]] = {
    # Root FastLED APIs are assembled by this existing canonical source router.
    # On normalized POSIX paths this matcher selects files directly under src/.
    "root": [("src/fl/build/src.cpp", ".*src.[^/]+$")],
    "platforms": [(_TU_PLATFORMS, ".*src.platforms.*")],
    "fl": [(_TU_FL_ALL, ".*src.fl.*")],
    "third_party": [(_TU_THIRD_PARTY, ".*src.third_party.*")],
    "all": [
        (_TU_PLATFORMS, ".*src.platforms.*"),
        (_TU_FL_ALL, ".*src.fl.*"),
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
    # Parser-only contract validation: force the supported external override
    # even on the host. Production builds keep their own exception policy.
    "-DFL_NO_EXCEPT=noexcept",
    "-DFL_HAS_NOEXCEPT=1",
    "-fexceptions",
]


def _compiler_args(base_args: list[str] | None = None) -> list[str]:
    """Match the native Windows GNU header profile for AST parsing (#4773)."""
    args = list(_COMPILER_ARGS if base_args is None else base_args)
    if sys.platform != "win32":
        return args
    try:
        from clang_tool_chain.abi.windows_gnu import _get_gnu_target_args
        from clang_tool_chain.platform.detection import get_platform_info

        platform_name, arch = get_platform_info()
        # The helper's compile-only mode omits irrelevant linker options.
        args.extend(_get_gnu_target_args(platform_name, arch, [*args, "-c"]))
    except (ImportError, OSError, RuntimeError, ValueError) as error:
        raise NoexceptCheckError(
            f"Windows AST parsing requires the native GNU toolchain: {error}"
        ) from error
    return args


_MATCH_OUTPUT_RE = re.compile(r"(src[\\/]\S+):(\d+):\d+: note: .root. binds here")
_SUPPRESS_RE = re.compile(
    r"//\s*(?:ok\s+no\s+(?:noexcept|FL_NO_EXCEPT)|"
    r"noexcept\s+not\s+required|nolint)\b",
    re.IGNORECASE,
)
_DESTRUCTOR_SOURCE_RE = re.compile(r"(?:^|[^\w:])~\w+\s*\(")
_MACRO_INVOCATION_RE = re.compile(r"^\s*[A-Z][A-Z0-9_]*\s*\(")


class NoexceptCheckError(RuntimeError):
    """Raised when the clang-query based check cannot run."""


@dataclass(frozen=True)
class NoexceptHit:
    """One source signature that still needs an FL_NO_EXCEPT decision."""

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
    """Collapse a source signature into a stable one-line identity."""
    without_comments = " ".join(
        line.split("//", 1)[0].strip() for line in signature.splitlines()
    )
    return re.sub(r"\s+", " ", without_comments).strip()


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

    wrapper = shutil.which("clang-tool-chain-clang-query")
    if wrapper:
        return [wrapper]

    uv = shutil.which("uv")
    if uv:
        return [uv, "run", "clang-tool-chain-clang-query"]

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
        for column, ch in enumerate(code):
            if ch == "(":
                paren_depth += 1
                found_open = True
            elif ch == ")":
                paren_depth -= 1
            elif ch in (";", "{") and found_open and paren_depth == 0:
                # Only the declaration determines this function's contract.
                # An annotated lambda in a same-line body must not exempt
                # its enclosing, unannotated function (#4773).
                tail = raw[column + 1 :]
                signature_lines[-1] = raw[: column + 1]
                if tail.lstrip().startswith("//"):
                    signature_lines[-1] += tail
                return display_line, "\n".join(signature_lines)

    return display_line, "\n".join(signature_lines)


def _signature_is_exempt(signature: str) -> bool:
    """Return True for documented suppressions and non-actionable constructs."""
    if not signature:
        return True
    if _SUPPRESS_RE.search(signature):
        return True

    code = "\n".join(line.split("//", 1)[0] for line in signature.splitlines())
    # isNoThrow() already removes functions with a real nonthrowing contract.
    # Tokens in return/callback types, or noexcept(false), cannot excuse an AST
    # finding for this function (#4773).
    if _DESTRUCTOR_SOURCE_RE.search(code):
        return True
    if _MACRO_INVOCATION_RE.match(code):
        return True
    if 'extern "C"' in code:
        return True
    return False


_COMPILER_ERROR_RE = re.compile(r"(?m)^(?:.*?:\d+(?::\d+)?:\s*)?(?:fatal\s+)?error:")


def _raise_on_query_errors(returncode: int, output: str) -> None:
    """Reject incomplete ASTs, including clang-query's zero-exit parse failures."""
    if returncode != 0:
        raise NoexceptCheckError(output.strip() or "clang-query failed")
    if (
        _COMPILER_ERROR_RE.search(output)
        or "Error parsing argument" in output
        or "Error parsing matcher" in output
        or "Matcher not found" in output
    ):
        raise NoexceptCheckError(output.strip())


def _run_clang_query(
    clang_query: list[str], tu: str, file_regex: str
) -> list[NoexceptHit]:
    """Run clang-query and return filtered missing-FL_NO_EXCEPT hits."""
    result = RunningProcess.run(
        [*clang_query, tu, "--", *_compiler_args()],
        input=build_query(file_regex),
        stdout=PIPE,
        stderr=PIPE,
        text=True,
        # See check_array_params._run_clang_query: text=True without an
        # explicit encoding decodes clang-query's UTF-8 output with the
        # locale codec and crashes on Windows.
        encoding="utf-8",
        errors="replace",
        cwd=str(PROJECT_ROOT),
        timeout=300,
    )
    output = result.stdout + "\n" + result.stderr
    _raise_on_query_errors(result.returncode, output)

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
    """Find current non-exempt missing-FL_NO_EXCEPT signatures."""
    clang_query = _find_clang_query()
    if not clang_query:
        raise NoexceptCheckError(
            "clang-query not found. Install LLVM or the clang-tool-chain package."
        )

    tus = _scope_tus(scope)
    for tu, _ in tus:
        if not (PROJECT_ROOT / tu).exists():
            raise NoexceptCheckError(f"translation unit not found: {tu}")

    # The "all" scope dispatches 3 independent clang-query child processes
    # (platforms / fl / third_party). Each parses its own TU - they share
    # no state and can run concurrently. Sequential wall was ~17s on a
    # cold AST run; parallel is ~max(per-TU) instead of sum.
    if len(tus) == 1:
        tu, file_regex = tus[0]
        return _run_clang_query(clang_query, tu, file_regex)

    from concurrent.futures import ThreadPoolExecutor

    # max_workers defaults to host CPU count so the pool scales when
    # we split big TUs (e.g. fl/) into multiple parallel chunks. For
    # today's 3-TU shape ThreadPoolExecutor naturally caps at 3 (it
    # never spawns more workers than submitted futures), so this is a
    # no-op on current scopes - it's the right default for the next
    # round of TU splitting.
    max_workers = max(len(tus), os.cpu_count() or len(tus))

    all_hits: list[NoexceptHit] = []
    with ThreadPoolExecutor(max_workers=max_workers) as pool:
        futures = [
            pool.submit(_run_clang_query, clang_query, tu, file_regex)
            for tu, file_regex in tus
        ]
        for future in futures:
            all_hits.extend(future.result())
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
            "Check for missing FL_NO_EXCEPT in root src, src/fl, src/platforms, and "
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
        "--no-baseline",
        action="store_true",
        help="Compatibility alias: every invocation is strict and ignores baselines",
    )
    args = parser.parse_args()

    try:
        hits = find_missing_noexcept(args.scope)
    except NoexceptCheckError as exc:
        print(f"ERROR: {exc}")
        return 1

    if not hits:
        print("All checked owned src functions have FL_NO_EXCEPT.")
        return 0

    print(f"Found {len(hits)} function(s) missing FL_NO_EXCEPT:")
    print()
    _print_hits(hits)
    print("Add FL_NO_EXCEPT or a documented contract-specific suppression comment.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
