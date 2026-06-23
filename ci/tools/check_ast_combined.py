"""Combined AST pass: run noexcept + array-param matchers in ONE clang-query session per TU.

The two checks were independently dispatching clang-query subprocesses
per TU, so a 25-TU "all" scope spawned 50 clang-query processes (25 for
noexcept + 25 for array-param), each one re-parsing the same TU
independently. Parsing is the bulk of clang-query's wall time; running
both matchers against the same parsed AST halves the per-TU CPU work.

This module exposes a single ``find_combined_hits(scope)`` entry point
that returns ``(noexcept_hits, array_param_hits)``. The downstream
diff-against-baseline + result-shaping logic continues to live in
``check_noexcept.py`` and ``check_array_params.py`` - this file only
owns the dual-matcher dispatch.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

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
    _COMPILER_ARGS,
    NoexceptCheckError,
    NoexceptHit,
    _find_clang_query,
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

_COMBINED_OUTPUT_RE = re.compile(
    r"(src[\\/]\S+):(\d+):\d+: note: \"("
    + _BIND_NOEXCEPT
    + r"|"
    + _BIND_ARRAY_PARAM
    + r")\" binds here"
)


def _build_combined_query(file_regex: str) -> str:
    """Build a clang-query script with both matchers in one session."""
    noexcept_matcher = (
        "match functionDecl("
        "unless(isNoThrow()), "
        "unless(isDeleted()), "
        "unless(isDefaulted()), "
        "unless(isImplicit()), "
        "unless(cxxDestructorDecl()), "
        "unless(cxxMethodDecl(ofClass(cxxRecordDecl(isLambda())))), "
        "unless(hasParent(linkageSpecDecl())), "
        f'isExpansionInFileMatching("{file_regex}"))'
        f'.bind("{_BIND_NOEXCEPT}")'
    )
    array_param_matcher = (
        "match functionDecl("
        "unless(isImplicit()), "
        "unless(cxxMethodDecl(ofClass(cxxRecordDecl(isLambda())))), "
        "unless(hasParent(linkageSpecDecl())), "
        "hasAnyParameter(parmVarDecl(hasType(pointerType()))), "
        f'isExpansionInFileMatching("{file_regex}"))'
        f'.bind("{_BIND_ARRAY_PARAM}")'
    )
    # `set output diag` so each match emits a "<bind> binds here" note we
    # can split on. Both matchers run against the same parsed AST.
    return f"set output diag\n{noexcept_matcher}\n{array_param_matcher}\n"


def _run_combined_clang_query(
    clang_query: list[str], tu: str, file_regex: str
) -> tuple[list[NoexceptHit], list[ArrayParamHit]]:
    """Run a single clang-query session and route hits by binding name."""
    result = subprocess.run(
        [*clang_query, tu, "--", *_COMPILER_ARGS],
        input=_build_combined_query(file_regex),
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

    noexcept_hits: list[NoexceptHit] = []
    array_param_hits: list[ArrayParamHit] = []
    noexcept_seen: set[tuple[str, int]] = set()
    array_seen: set[tuple[str, int]] = set()

    for match in _COMBINED_OUTPUT_RE.finditer(output):
        filepath = match.group(1).replace("\\", "/")
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

    return noexcept_hits, array_param_hits


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

    max_workers = max(len(tus), os.cpu_count() or len(tus))
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
    return all_noexcept, all_array_param
