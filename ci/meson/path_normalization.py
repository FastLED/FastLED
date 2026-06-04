#!/usr/bin/env python3
"""Path normalization and validation for zccache strict mode (issue #2378).

Meson emits attached path-bearing compiler flags like ``-Ici/meson/native`` and
``-I..\\..\\src``. ``ZCCACHE_STRICT_PATHS=absolute`` rejects any such flag that
is not absolute, forward-slash, and free of ``.``/``..`` components. We rewrite
them to canonical absolute forward-slash form in ``build.ninja`` and
``compile_commands.json`` so cache hit rates stay high and CI fails fast on
violations.

Also exports the AR optimization status check used by the runner.
"""
# pyright: reportUnknownVariableType=false

import json
import re
import sys
from pathlib import Path
from typing import cast

from ci.util.timestamp_print import ts_print as _ts_print


# Module-level cache for ar optimization status.
# Populated by inject_ar_optimization_patches() to avoid repeated build.ninja reads.
# Key: str(build_dir), Value: bool (True if both patches active)
_ar_opt_status_cache: dict[str, bool] = {}


# Path-bearing compiler flag prefixes that zccache strict path mode validates
# (issue #2378). All accept an attached path (``-Idir``, ``-isystemdir``, ...)
# as Meson always emits a single attached token per flag. Order matters: longer
# prefixes must come first so the regex alternation matches the longest form
# (``-include-pch`` before ``-include``).
_STRICT_PATH_FLAG_PREFIXES: tuple[str, ...] = (
    "-include-pch",
    "-iframework",
    "-idirafter",
    "-isystem",
    "-imacros",
    "-include",
    "-iquote",
    "-imsvc",
    "-I",
    "-F",
    "/I",
)

# Path char class excludes ``-`` as the FIRST character so the regex does not
# mis-match the bare ``"-include-pch"`` token by falling back to the ``-include``
# alternative + consuming ``-pch`` as the path. Real include paths never begin
# with a hyphen.
_STRICT_PATH_FLAG_RE = re.compile(
    r'(?P<prefix>(?<!\S)"?(?:'
    + "|".join(re.escape(p) for p in _STRICT_PATH_FLAG_PREFIXES)
    + r'))(?P<path>[^\s"\-][^\s"]*)(?P<suffix>"?)'
)


def is_ar_content_preserving_active(build_dir: Path) -> bool:
    """
    Check if the content-preserving archive optimization is active in build.ninja.

    Returns True if both patches are present:
    - ar_content_preserving.py wrapper in the STATIC_LINKER command
    - restat = 1 in the STATIC_LINKER rule

    When this returns True, the BuildOptimizer's DLL fingerprinting is redundant:
    - ar_content_preserving.py prevents the DLL relink cascade at the source
    - BuildOptimizer.touch_dlls_if_lib_unchanged() (~5-6s) is no longer needed
    - BuildOptimizer.save_fingerprints() (~5-6s) is no longer needed
    - Skipping BuildOptimizer saves ~10-12 seconds per build

    Performance: Uses a module-level cache populated by inject_ar_optimization_patches().
    When inject_ar_optimization_patches() was called earlier in the same process
    (which setup_meson_build() does), this function returns immediately from the cache
    without reading build.ninja (~2ms saved per call).

    Args:
        build_dir: Meson build directory containing build.ninja

    Returns:
        True if both ar_content_preserving.py and restat=1 patches are active
    """
    # Fast path: check module-level cache populated by inject_ar_optimization_patches().
    # In the normal code path, setup_meson_build() always calls that function first,
    # so we get a cache hit here and avoid the build.ninja read (~2ms).
    build_dir_key = str(build_dir)
    if build_dir_key in _ar_opt_status_cache:
        return _ar_opt_status_cache[build_dir_key]

    # Cache miss: read build.ninja and check directly
    build_ninja = build_dir / "build.ninja"
    if not build_ninja.exists():
        return False
    try:
        content = build_ninja.read_text(encoding="utf-8")
        result = "ar_content_preserving.py" in content and "restat = 1" in content
        _ar_opt_status_cache[build_dir_key] = result
        return result
    except OSError:
        return False


def _is_forward_slash_absolute(path: str) -> bool:
    return path.startswith("/") or re.match(r"^[A-Za-z]:/", path) is not None


def _has_dot_components(path: str) -> bool:
    for part in path.split("/"):
        if part in (".", ".."):
            return True
    return False


def normalize_meson_private_include_paths(build_dir: Path) -> bool:
    """Normalize Meson-emitted path flags for zccache strict path mode.

    Issue #2378 — ``ZCCACHE_STRICT_PATHS=absolute`` rejects path-bearing
    compiler flags (``-I``, ``-isystem``, ``-iquote``, ``-iframework``,
    ``-idirafter``, ``-imsvc``, ``-include``, ``-include-pch``, ``-imacros``,
    ``-F``, ``/I``) that are not absolute, forward-slash, and free of ``.``
    or ``..`` components.

    Meson emits attached single-token flags like ``"-Ici/meson/native"`` and
    ``"-I..\\..\\src"`` from subdir and ``implicit_include_directories``
    behavior; this function rewrites them to canonical absolute forward-slash
    form in both ``build.ninja`` and ``compile_commands.json``.
    """

    changed_any = False

    def _replacement(match: re.Match[str]) -> str:
        prefix, raw_path, suffix = match.groups()
        normalized = raw_path.replace("\\", "/")
        if _is_forward_slash_absolute(normalized) and not _has_dot_components(
            normalized
        ):
            absolute = normalized
        else:
            absolute = (build_dir / normalized).resolve().as_posix()
        if absolute == raw_path:
            return match.group(0)
        return f"{prefix}{absolute}{suffix}"

    def _normalize_text(content: str) -> str:
        return _STRICT_PATH_FLAG_RE.sub(_replacement, content)

    build_ninja = build_dir / "build.ninja"
    if build_ninja.exists():
        try:
            content = build_ninja.read_text(encoding="utf-8")
        except OSError as e:
            raise RuntimeError(
                f"Could not read {build_ninja} while normalizing Meson strict paths"
            ) from e
        else:
            normalized = _normalize_text(content)
            if normalized != content:
                try:
                    build_ninja.write_text(normalized, encoding="utf-8")
                    changed_any = True
                except OSError as e:
                    raise RuntimeError(
                        f"Could not write {build_ninja} after normalizing Meson strict paths"
                    ) from e

    compile_commands = build_dir / "compile_commands.json"
    if compile_commands.exists():
        try:
            data = json.loads(compile_commands.read_text(encoding="utf-8"))
        except OSError as e:
            raise RuntimeError(
                f"Could not read {compile_commands} while normalizing Meson strict paths"
            ) from e
        except json.JSONDecodeError as e:
            raise RuntimeError(
                f"Could not parse {compile_commands} while normalizing Meson strict paths"
            ) from e
        if isinstance(data, list):
            changed_json = False
            for entry in data:
                if not isinstance(entry, dict):
                    continue
                entry_dict = cast(dict[str, object], entry)
                command = entry_dict.get("command")
                if not isinstance(command, str):
                    continue
                normalized = _normalize_text(command)
                if normalized != command:
                    entry_dict["command"] = normalized
                    changed_json = True
            if changed_json:
                try:
                    compile_commands.write_text(
                        json.dumps(data, indent=2) + "\n",
                        encoding="utf-8",
                    )
                    changed_any = True
                except OSError as e:
                    raise RuntimeError(
                        f"Could not write {compile_commands} after normalizing Meson strict paths"
                    ) from e

    return changed_any


def find_strict_path_violations(build_dir: Path) -> list[str]:
    """Return human-readable strict-path violations in ``compile_commands.json``.

    Issue #2378 — every ``-I`` / ``-isystem`` / ``-iquote`` / ``-iframework`` /
    ``-idirafter`` / ``-imsvc`` / ``-include`` / ``-include-pch`` / ``-imacros``
    / ``-F`` / ``/I`` path must be absolute, forward-slash, and free of ``.``
    or ``..`` components. This is the same predicate enforced by
    ``ZCCACHE_STRICT_PATHS=absolute`` at compile time; checking it ourselves
    after meson setup lets us fail before ninja schedules any compile work.

    Returns an empty list when the file is missing (a fresh tree) or clean.
    """
    cc_json = build_dir / "compile_commands.json"
    if not cc_json.exists():
        return []
    try:
        data = json.loads(cc_json.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    if not isinstance(data, list):
        return []

    violations: list[str] = []
    for entry in data:
        if not isinstance(entry, dict):
            continue
        entry_dict = cast(dict[str, object], entry)
        command = entry_dict.get("command")
        if not isinstance(command, str):
            continue
        for match in _STRICT_PATH_FLAG_RE.finditer(command):
            raw = match.group("path")
            normalized = raw.replace("\\", "/")
            if "\\" in raw:
                violations.append(f"{match.group('prefix').lstrip(chr(34))}{raw}")
                continue
            if not _is_forward_slash_absolute(normalized):
                violations.append(f"{match.group('prefix').lstrip(chr(34))}{raw}")
                continue
            if _has_dot_components(normalized):
                violations.append(f"{match.group('prefix').lstrip(chr(34))}{raw}")
    return violations


def _enforce_strict_path_violations(build_dir: Path) -> None:
    """Raise loudly when ``compile_commands.json`` still has strict-path offenders.

    The normalizer above is meant to leave no offenders behind; this is the
    fail-fast guard for issue #2378's "validation step that checks generated
    compile commands before CI compiles". Any violation here is a bug in the
    normalizer or a new Meson code-path that emits unnormalized flags.
    """
    violations = find_strict_path_violations(build_dir)
    if not violations:
        return

    sample = violations[:10]
    extra = len(violations) - len(sample)
    lines = [
        f"[MESON] ZCCACHE_STRICT_PATHS=absolute would reject {len(violations)}"
        " compile flag(s) in compile_commands.json (issue #2378):",
    ]
    lines.extend(f"  {flag}" for flag in sample)
    if extra > 0:
        lines.append(f"  ... {extra} more")
    message = "\n".join(lines)
    _ts_print(message, file=sys.stderr)
    raise RuntimeError(message)
