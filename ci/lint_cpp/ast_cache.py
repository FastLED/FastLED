"""Fingerprint-based cache for the clang-query AST passes.

The two AST passes (`check_noexcept`, `check_array_params`) call
clang-query under the hood. Each call costs 10-17 seconds even when
nothing has changed because clang-query re-parses the source tree from
scratch every time. The result is deterministic given:

  - the source files in the requested scope (src/fl, src/platforms, etc.)
  - the baseline file (lists known-existing violations to subtract)
  - the tool's own Python source (so detection-rule changes invalidate)

So we fingerprint those inputs (blake2b of path + size + mtime per file)
and stash the resulting violations as JSON keyed on the fingerprint. On
a hit we return the cached violations without ever spawning clang-query.

Cache files live under `.cache/ast_lint/<name>.fingerprint` and
`<name>.violations.json`. Both writes are atomic via tmpfile+replace so
a concurrent invocation cannot see a torn pair.
"""

from __future__ import annotations

import hashlib
import json
import os
import sys
from pathlib import Path
from typing import Callable, Iterable

from ci.util.check_files import CheckerResults
from ci.util.paths import PROJECT_ROOT


_CACHE_DIR = PROJECT_ROOT / ".cache" / "ast_lint"

# Source file extensions that affect clang-query AST output.
_TRACKED_EXTENSIONS = (".h", ".hpp", ".cpp", ".cc", ".cxx", ".cpp.hpp")

# Scope -> list of src/ subdirectories to fingerprint.
#
# IMPORTANT: clang-query parses each TU's TRANSITIVE include closure -
# not just the files under the named scope. A change to a header that's
# not directly in scope (e.g. src/FastLED.h, src/crgb.h, src/lib8tion/*)
# can change clang-query's output. To stay correct, we walk the entire
# src/ tree regardless of scope and fingerprint every C/C++ source +
# header. The scope name is preserved as a key for the cache file name
# but no longer narrows the file set.
#
# Cost: ~100 extra files vs the previous scope-narrow walk (~2000 ->
# ~2100). Fingerprint is per-file os.stat (mtime+size); ~50ms total.
_SRC_ROOT = "src"


def _iter_scope_files(scope: str) -> Iterable[Path]:
    # `scope` retained in the signature for API stability + cache-key
    # differentiation, but we always walk the full src/ tree because
    # any header anywhere can affect a TU's parse.
    del scope
    root = PROJECT_ROOT / _SRC_ROOT
    if not root.is_dir():
        return
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        name = path.name.lower()
        if any(name.endswith(ext) for ext in _TRACKED_EXTENSIONS):
            yield path


def _compute_fingerprint(scope: str, extra_inputs: list[Path]) -> str:
    """blake2b over (path, size, mtime_ns) of every input - cheap, mtime-based."""
    hasher = hashlib.blake2b(digest_size=16)
    files = sorted(_iter_scope_files(scope))
    files.extend(p for p in extra_inputs if p.is_file())
    files.sort()
    for path in files:
        try:
            st = path.stat()
        except OSError:
            continue
        try:
            relative = path.resolve().relative_to(PROJECT_ROOT)
        except ValueError:
            relative = path
        rel_str = str(relative).replace("\\", "/")
        hasher.update(rel_str.encode("utf-8"))
        hasher.update(b"\0")
        hasher.update(str(st.st_size).encode("ascii"))
        hasher.update(b"\0")
        hasher.update(str(st.st_mtime_ns).encode("ascii"))
        hasher.update(b"\0")
    return hasher.hexdigest()


def _atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = path.with_suffix(path.suffix + ".tmp")
    tmp_path.write_text(content, encoding="utf-8")
    os.replace(tmp_path, path)


def _serialize_results(results: CheckerResults) -> str:
    payload: list[dict[str, object]] = []
    for file_path, file_violations in results.violations.items():
        for violation in file_violations.violations:
            payload.append(
                {
                    "path": file_path,
                    "line": int(violation.line_number),
                    "message": str(violation.content),
                }
            )
    return json.dumps(payload, separators=(",", ":"))


def _deserialize_results(text: str) -> CheckerResults:
    payload = json.loads(text)
    results = CheckerResults()
    for entry in payload:
        results.add_violation(
            str(entry["path"]),
            int(entry["line"]),
            str(entry["message"]),
        )
    return results


def cached_ast_check(
    name: str,
    scope: str,
    tool_sources: list[Path],
    baseline_path: Path | None,
    runner: Callable[[], CheckerResults],
) -> CheckerResults:
    """Return results from cache if fingerprint matches, else run + cache.

    The fingerprint covers every file under the scope's directories
    (filtered to C++ extensions) PLUS the tool's Python source PLUS the
    baseline. Any of those changing invalidates the cache.

    The cache stores violations verbatim so a run that produced violations
    can still be replayed instantly - the user just sees the same output
    until they edit the underlying file.
    """
    extras = list(tool_sources)
    if baseline_path is not None and baseline_path.is_file():
        extras.append(baseline_path)

    current_fp = _compute_fingerprint(scope, extras)
    fp_path = _CACHE_DIR / f"{name}.fingerprint"
    result_path = _CACHE_DIR / f"{name}.violations.json"

    if fp_path.exists() and result_path.exists():
        try:
            cached_fp = fp_path.read_text(encoding="utf-8").strip()
        except OSError:
            cached_fp = ""
        if cached_fp == current_fp:
            try:
                return _deserialize_results(result_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError, KeyError, ValueError) as exc:
                print(
                    f"warning: ast_cache cached result for {name} unreadable, re-running: {exc}",
                    file=sys.stderr,
                )

    results = runner()
    try:
        _atomic_write(result_path, _serialize_results(results))
        _atomic_write(fp_path, current_fp)
    except OSError as exc:
        print(
            f"warning: ast_cache failed to persist result for {name}: {exc}",
            file=sys.stderr,
        )
    return results
