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
import tempfile
import threading
import uuid
from dataclasses import dataclass, field
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


@dataclass(frozen=True)
class AstUnitResult:
    """A completed semantic parse and the headers observed by that parse."""

    payload: dict[str, object]
    dependencies: tuple[Path, ...]
    complete: bool = True


@dataclass(frozen=True)
class AstUnitCacheResult:
    payload: dict[str, object]
    cache_hit: bool


@dataclass
class AstInputSnapshot:
    """Share byte hashing across jobs in ONE invocation, never across runs.

    Instantiate after provisioning/generated-input staging is complete. Callers
    must not mutate inputs while the matrix runs, just as with compilation.
    """

    _contents: dict[Path, str] = field(default_factory=dict)
    _inventories: dict[Path, str] = field(default_factory=dict)
    _available_files: dict[Path, set[Path]] = field(default_factory=dict)
    _lock: threading.RLock = field(default_factory=threading.RLock)

    def content(self, path: Path) -> str:
        with self._lock:
            if path not in self._contents:
                try:
                    with path.open("rb") as stream:
                        hasher = hashlib.sha256()
                        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                            hasher.update(chunk)
                        digest = hasher.hexdigest()
                except FileNotFoundError:
                    digest = "missing"
                self._contents[path] = digest
            return self._contents[path]

    def inventory(self, root: Path) -> str:
        """Fingerprint names and symlink targets, including extensionless files.

        Included-file bytes are tracked separately per TU. Header appearance,
        disappearance, search-path shadowing, and symlink retargeting invalidate
        affected profiles even when no previous include trace mentions them.
        """
        with self._lock:
            if root in self._inventories:
                return self._inventories[root]
            digest = hashlib.sha256()
            available: set[Path] = set()

            def visit(path: Path, ancestors: frozenset[Path]) -> None:
                resolved = path.resolve()
                digest.update(str(path).encode("utf-8"))
                digest.update(b"\0")
                digest.update(str(resolved).encode("utf-8"))
                digest.update(b"\0")
                if not path.exists():
                    digest.update(b"missing\0")
                elif path.is_dir():
                    digest.update(b"dir\0")
                    if resolved not in ancestors:
                        for child in sorted(path.iterdir()):
                            visit(child, ancestors | {resolved})
                else:
                    digest.update(b"file\0")
                    if path.is_file():
                        available.add(resolved)

            visit(root, frozenset())
            value = digest.hexdigest()
            self._inventories[root] = value
            self._available_files[root] = available
            return value

    def prepare_parse(self, roots: list[Path], files: list[Path]) -> None:
        """Snapshot possible newly discovered dependencies before a cold parse."""
        with self._lock:
            for root in roots:
                self.inventory(root)
                for path in self._available_files[root]:
                    self.content(path)
            for path in files:
                self.content(path.resolve())

    def verify_parse(self, roots: list[Path], paths: list[Path]) -> None:
        """Refuse results if inputs changed while parsing or were not declared."""
        after = AstInputSnapshot()
        for path in paths:
            resolved = path.resolve()
            before = self._contents.get(resolved)
            if before is None:
                raise ValueError(
                    f"AST dependency outside declared include roots: {path}"
                )
            if before != after.content(resolved):
                raise ValueError(f"AST input changed during analysis; retry: {path}")
        for root in roots:
            if self.inventory(root) != after.inventory(root):
                raise ValueError(
                    f"AST include availability changed during analysis; retry: {root}"
                )

    def verify_final(self) -> None:
        """Validate shared lookup hashes before reporting any matrix result.

        Cache hits bypass ``verify_parse``. Recheck the invocation's complete
        content and availability snapshot so edits after an earlier job cannot
        turn a later cached result into stale successful evidence.
        """
        with self._lock:
            contents = dict(self._contents)
            inventories = dict(self._inventories)
        after = AstInputSnapshot()
        for path, before in contents.items():
            if after.content(path) != before:
                raise ValueError(f"AST input changed during analysis; retry: {path}")
        for root, before in inventories.items():
            if after.inventory(root) != before:
                raise ValueError(
                    f"AST include availability changed during analysis; retry: {root}"
                )


def _unit_json(value: object) -> bytes:
    return json.dumps(
        value, sort_keys=True, separators=(",", ":"), allow_nan=False
    ).encode("utf-8")


def _unit_manifest_write(path: Path, dependencies: list[str]) -> None:
    """One atomic dependency record; result storage belongs to zccache (#3338)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        dir=path.parent,
        prefix=path.name,
        suffix=".tmp",
        delete=False,
    ) as stream:
        json.dump({"dependencies": dependencies}, stream)
        temporary = Path(stream.name)
    os.replace(temporary, path)


def cached_ast_unit(
    name: str,
    *,
    identity: dict[str, object],
    input_files: Iterable[Path],
    include_roots: Iterable[Path],
    runner: Callable[[], AstUnitResult],
    cache_dir: Path | None = None,
    snapshot: AstInputSnapshot | None = None,
) -> AstUnitCacheResult:
    """Reuse a complete TU/profile parse through zccache's native binding.

    ``identity`` must describe the full argument vector, target, SDK, matcher
    and expected-declaration contract. ``input_files`` must include the TU,
    compiler binary, tool sources and generated configuration. ``include_roots``
    must include every search root, including compiler resources and quoted
    include directories. The runner supplies transitive dependencies obtained
    from the SAME successful parse (for example Clang's -H trace).

    The tiny local manifest discovers the previous dependency list. All cached
    analysis bytes live in zccache; this extends #3338's intended integration
    instead of creating another result store. Explicit byte digests also guard
    against same-size edits with restored timestamps. Parse failures propagate;
    violations in a complete parse are cached verbatim.
    """
    from zccache import exec_cached

    active_snapshot = snapshot if snapshot is not None else AstInputSnapshot()
    files = sorted({Path(os.path.abspath(path)) for path in input_files})
    roots = sorted({Path(os.path.abspath(path)) for path in include_roots})
    descriptor = {
        "schema": 1,
        "name": name,
        "identity": identity,
        "files": [str(path) for path in files],
        "roots": [str(path) for path in roots],
    }
    descriptor_bytes = _unit_json(descriptor)
    manifest = (cache_dir or _CACHE_DIR / "units") / (
        hashlib.sha256(descriptor_bytes).hexdigest() + ".json"
    )
    dependencies: list[str] = []
    manifest_valid = False
    try:
        stored = json.loads(manifest.read_text(encoding="utf-8"))
        candidate = stored["dependencies"]
        if isinstance(candidate, list) and all(
            isinstance(path, str) for path in candidate
        ):
            dependencies = candidate
            manifest_valid = True
    except (OSError, ValueError, KeyError, TypeError):
        pass

    inventories = [(str(root), active_snapshot.inventory(root)) for root in roots]

    def key_for(paths: list[str]) -> bytes:
        contents = [
            (str(path), active_snapshot.content(path))
            for path in sorted(set(files) | {Path(path) for path in paths})
        ]
        return _unit_json(
            {
                "descriptor": descriptor,
                "contents": contents,
                "availability": inventories,
            }
        )

    parsed: AstUnitResult | None = None

    def run() -> bytes:
        nonlocal parsed
        active_snapshot.prepare_parse(roots, files)
        parsed = runner()
        if not parsed.complete:
            raise ValueError("Cannot cache an incomplete AST parse")
        if not isinstance(parsed.payload, dict):
            raise ValueError("AST payload must be a JSON object")
        active_snapshot.verify_parse(roots, [*files, *parsed.dependencies])
        return _unit_json(
            {
                "payload": parsed.payload,
                "dependencies": sorted(
                    {str(path.resolve()) for path in parsed.dependencies}
                ),
                "complete": True,
            }
        )

    def execute(key: bytes, callback: Callable[[], bytes]) -> bytes:
        # Content digests in extra_key validate input bytes regardless of daemon
        # stat-cache shortcuts. No path list is serialized onto a command line.
        return exec_cached(
            name="fastled_ast_unit",
            input_files=[],
            input_env={},
            extra_key=key,
            runner=callback,
        )

    initial_key = key_for(dependencies)
    if not manifest_valid:
        # A key without discovered dependencies must never be reused if the
        # manifest is missing, corrupted, or concurrent discovery is underway.
        initial_key += uuid.uuid4().hex.encode("ascii")
    encoded = execute(initial_key, run)
    try:
        result = json.loads(encoded)
        if (
            not isinstance(result, dict)
            or result.get("complete") is not True
            or not isinstance(result.get("payload"), dict)
            or not isinstance(result.get("dependencies"), list)
            or not all(isinstance(path, str) for path in result["dependencies"])
        ):
            raise ValueError("Invalid cached AST result")
        # Reject non-JSON numbers, accepted by Python's permissive decoder.
        _unit_json(result)
    except (ValueError, TypeError, KeyError):
        result = json.loads(run())
        # A malformed cached value is never reported as a hit or success.
        return AstUnitCacheResult(result["payload"], False)

    discovered = result["dependencies"]
    if parsed is not None and (discovered != dependencies or not manifest_valid):
        # Initial dependency discovery gets a second store, not a second parse.
        # Future warm calls use this complete dependency key immediately.
        execute(key_for(discovered), lambda: encoded)
    try:
        _unit_manifest_write(manifest, discovered)
    except OSError as exc:
        print(f"warning: AST dependency manifest write failed: {exc}", file=sys.stderr)
    return AstUnitCacheResult(result["payload"], parsed is None)


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
