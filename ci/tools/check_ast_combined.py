"""Run semantic checks and coverage queries in one clang-query session per TU.

The two checks were independently dispatching clang-query child processes
per TU, so a 25-TU "all" scope spawned 50 clang-query processes (25 for
noexcept + 25 for array-param), each one re-parsing the same TU
independently. Parsing is the bulk of clang-query's wall time; running
both matchers against the same parsed AST halves the per-TU CPU work.

This module exposes a single ``find_combined_hits(scope)`` entry point
that returns ``(noexcept_hits, array_param_hits)``. The downstream
diff-against-baseline + result-shaping logic continues to live in
``check_noexcept.py`` and ``check_array_params.py`` - this file only
owns the shared matcher dispatch. Explicit target callers use
``analyze_translation_unit`` to supply real compiler settings and required
backend definitions; the legacy entry point retains the host profile.
"""

from __future__ import annotations

import json
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path
from tempfile import TemporaryDirectory

from running_process import PIPE, RunningProcess

from ci.tools.check_array_params import (
    ArrayParamHit,
    _find_array_params_in_signature,
)
from ci.tools.check_array_params import (
    _read_source_signature as _array_read_source_signature,
)
from ci.tools.check_array_params import (
    _scope_tus as _array_scope_tus,
)
from ci.tools.check_noexcept import (
    NoexceptCheckError,
    NoexceptHit,
    _compiler_args,
    _find_clang_query,
    _raise_on_query_errors,
)
from ci.tools.check_noexcept import (
    _read_source_signature as _noexcept_read_source_signature,
)
from ci.tools.check_noexcept import (
    _scope_tus as _noexcept_scope_tus,
)
from ci.tools.check_noexcept import (
    _signature_is_exempt as _noexcept_signature_is_exempt,
)
from ci.util.paths import PROJECT_ROOT


# Distinct binding names so we can route a single output stream back to
# the right matcher. Output line looks like:
#   src/fl/foo.h:42:5: note: "noexcept_hit" binds here
_BIND_NOEXCEPT = "noexcept_hit"
_BIND_ARRAY_PARAM = "array_param_hit"


@dataclass(frozen=True, order=True)
class DeclarationLocation:
    path: str
    line: int
    column: int


@dataclass
class TranslationUnitAnalysis:
    noexcept_hits: list[NoexceptHit]
    array_param_hits: list[ArrayParamHit]
    coverage: list[DeclarationLocation]
    present_records: list[str]
    dependencies: tuple[Path, ...]


_COVERAGE_OUTPUT_RE = re.compile(
    r'^(.+?):(\d+):(\d+): note: "coverage" binds here', re.MULTILINE
)

_COMBINED_OUTPUT_RE = re.compile(
    r"^(.+?):(\d+):\d+: note: \"("
    + _BIND_NOEXCEPT
    + r"|"
    + _BIND_ARRAY_PARAM
    + r")\" binds here",
    re.MULTILINE,
)


def _build_combined_query(
    file_regex: str, expected_records: tuple[str, ...] = ()
) -> str:
    """Build a clang-query script with both matchers in one session."""
    noexcept_matcher = (
        "match functionDecl("
        "unless(isNoThrow()), "
        "unless(isDeleted()), "
        "unless(isDefaulted()), "
        "unless(isImplicit()), "
        # SDK assertion macros can declare error-attributed diagnostic
        # sentinels at an owned expansion location. Calls cannot compile.
        'unless(hasAttr("attr::Error")), '
        "unless(cxxDestructorDecl()), "
        "unless(cxxMethodDecl(ofClass(cxxRecordDecl(isLambda())))), "
        "unless(hasParent(linkageSpecDecl())), "
        f"isExpansionInFileMatching({json.dumps(file_regex)}))"
        f'.bind("{_BIND_NOEXCEPT}")'
    )
    array_param_matcher = (
        "match functionDecl("
        "unless(isImplicit()), "
        "unless(cxxMethodDecl(ofClass(cxxRecordDecl(isLambda())))), "
        "unless(hasParent(linkageSpecDecl())), "
        "hasAnyParameter(parmVarDecl(hasType(pointerType()))), "
        f"isExpansionInFileMatching({json.dumps(file_regex)}))"
        f'.bind("{_BIND_ARRAY_PARAM}")'
    )
    # `set output diag` so each match emits a "<bind> binds here" note we
    # can split on. Both matchers run against the same parsed AST.
    coverage = (
        "match functionDecl(unless(isImplicit()), "
        f'isExpansionInFileMatching({json.dumps(file_regex)})).bind("coverage")'
    )
    assertions = [
        f"match cxxRecordDecl(isDefinition(), hasName({json.dumps(name)}))"
        f'.bind("expected_{index}")'
        for index, name in enumerate(expected_records)
    ]
    return "\n".join(
        [
            "set output diag",
            noexcept_matcher,
            array_param_matcher,
            coverage,
            *assertions,
            "",
        ]
    )


def _run_combined_clang_query(
    clang_query: list[str], tu: str, file_regex: str
) -> tuple[list[NoexceptHit], list[ArrayParamHit]]:
    result = analyze_translation_unit(clang_query, tu, file_regex, _compiler_args())
    return result.noexcept_hits, result.array_param_hits


def analyze_translation_unit(
    clang_query: list[str],
    tu: str,
    file_regex: str,
    compiler_args: list[str],
    *,
    expected_records: tuple[str, ...] = (),
    process_env: dict[str, str] | None = None,
) -> TranslationUnitAnalysis:
    """Run checks, declaration inventory, and presence assertions in one parse."""
    # Real SDK include lists exceed Windows' process command-line limit. A
    # compilation database carries exact argv tokens without shell/response
    # quoting rules. clang-tool matches FixedCompilationDatabase's synthetic
    # driver used by the previous `clang-query tu -- args` invocation.
    source = str((PROJECT_ROOT / tu).resolve())
    with TemporaryDirectory(prefix="fastled-ast-") as database_dir:
        database = Path(database_dir) / "compile_commands.json"
        database.write_text(
            json.dumps(
                [
                    {
                        "directory": str(PROJECT_ROOT),
                        "file": source,
                        "arguments": [
                            "clang-tool",
                            *compiler_args,
                            "-H",
                            "-fshow-skipped-includes",
                            source,
                        ],
                    }
                ]
            ),
            encoding="utf-8",
        )
        result = RunningProcess.run(
            [*clang_query, source, "-p", database_dir],
            input=_build_combined_query(file_regex, expected_records),
            stdout=PIPE,
            stderr=PIPE,
            text=True,
            cwd=str(PROJECT_ROOT),
            env=process_env,
            timeout=300,
            encoding="utf-8",
            errors="replace",
        )
    output = result.stdout + "\n" + result.stderr
    _raise_on_query_errors(result.returncode, output)
    dependencies = {Path(tu).resolve()}
    dependencies.update(
        Path(match.group(1)).resolve()
        for match in re.finditer(r"^\.+ (.+)$", result.stderr, re.MULTILINE)
    )
    for dependency in dependencies:
        if not dependency.is_file():
            raise NoexceptCheckError(f"AST dependency disappeared: {dependency}")
    present_records = [
        name
        for index, name in enumerate(expected_records)
        if re.search(rf' note: "expected_{index}" binds here', output)
    ]
    missing = sorted(set(expected_records) - set(present_records))
    if missing:
        raise NoexceptCheckError(
            f"{tu}: expected backend definitions missing: {', '.join(missing)}"
        )
    coverage: set[DeclarationLocation] = set()
    for location in _COVERAGE_OUTPUT_RE.finditer(output):
        path = Path(location.group(1)).resolve()
        if not path.is_relative_to(PROJECT_ROOT / "src"):
            continue
        normalized = path.relative_to(PROJECT_ROOT).as_posix()
        coverage.add(
            DeclarationLocation(
                normalized, int(location.group(2)), int(location.group(3))
            )
        )

    noexcept_hits: list[NoexceptHit] = []
    array_param_hits: list[ArrayParamHit] = []
    noexcept_seen: set[tuple[str, int]] = set()
    array_seen: set[tuple[str, int]] = set()

    for match in _COMBINED_OUTPUT_RE.finditer(output):
        source_path = Path(match.group(1)).resolve()
        if not source_path.is_relative_to(PROJECT_ROOT / "src"):
            continue
        filepath = source_path.relative_to(PROJECT_ROOT).as_posix()
        line_num = int(match.group(2))
        bind = match.group(3)
        key = (filepath, line_num)
        if bind == _BIND_NOEXCEPT:
            if key in noexcept_seen:
                continue
            noexcept_seen.add(key)
            line_text, signature = _noexcept_read_source_signature(filepath, line_num)
            if _noexcept_signature_is_exempt(signature):
                continue
            noexcept_hits.append(
                NoexceptHit(
                    path=filepath,
                    line=line_num,
                    line_text=line_text,
                    signature=signature,
                )
            )
        else:  # array_param_hit
            if key in array_seen:
                continue
            array_seen.add(key)
            line_text, signature = _array_read_source_signature(filepath, line_num)
            # Re-uses the authoritative filter from check_array_params.py:
            # parses the parameter list properly, handles extern "C",
            # macro-invocation, suppression comments, IRAM tagging. Empty
            # tuple means "no actionable findings" - drop the hit.
            findings = _find_array_params_in_signature(signature)
            if not findings:
                continue
            array_param_hits.append(
                ArrayParamHit(
                    path=filepath,
                    line=line_num,
                    line_text=line_text,
                    signature=signature,
                    findings=findings,
                )
            )

    return TranslationUnitAnalysis(
        noexcept_hits,
        array_param_hits,
        sorted(coverage),
        present_records,
        tuple(sorted(dependencies)),
    )


def find_combined_hits(
    scope: str = "all",
) -> tuple[list[NoexceptHit], list[ArrayParamHit]]:
    """Run both AST matchers per TU in a single clang-query session each.

    Returns (noexcept_hits, array_param_hits) - same shape the existing
    per-check callers expect. Callers still do their own
    diff_against_baseline + downstream shaping.
    """
    clang_query = _find_clang_query()
    if not clang_query:
        raise NoexceptCheckError(
            "clang-query not found. Install LLVM or the clang-tool-chain package."
        )

    # Both modules expose _scope_tus; either resolves the same set today.
    # Use noexcept's since it's the authoritative TU enumerator.
    tus = _noexcept_scope_tus(scope)
    # Sanity-check that the array-param resolver agrees - they should be
    # identical lists, but skew here would silently drop array-param
    # coverage on TUs the noexcept enumerator missed (and vice versa).
    if [t for t, _ in tus] != [t for t, _ in _array_scope_tus(scope)]:
        print(
            "warning: noexcept and array-param TU enumerators disagree; "
            "falling back to noexcept's list",
            file=sys.stderr,
        )

    for tu, _ in tus:
        if not (PROJECT_ROOT / tu).exists():
            raise NoexceptCheckError(f"translation unit not found: {tu}")

    if len(tus) == 1:
        tu, file_regex = tus[0]
        return _run_combined_clang_query(clang_query, tu, file_regex)

    max_workers = min(len(tus), os.cpu_count() or 1, 4)
    all_noexcept: list[NoexceptHit] = []
    all_array_param: list[ArrayParamHit] = []
    with ThreadPoolExecutor(max_workers=max_workers) as pool:
        futures = [
            pool.submit(_run_combined_clang_query, clang_query, tu, file_regex)
            for tu, file_regex in tus
        ]
        for future in futures:
            noexcept_hits, array_param_hits = future.result()
            all_noexcept.extend(noexcept_hits)
            all_array_param.extend(array_param_hits)
    # Public headers can be parsed through several canonical routers. Baseline
    # multiplicity represents physical declarations, not how many TUs see them.
    return (
        list({(hit.path, hit.line): hit for hit in all_noexcept}.values()),
        list({(hit.path, hit.line): hit for hit in all_array_param}.values()),
    )
