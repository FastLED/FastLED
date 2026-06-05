#!/usr/bin/env python3
"""Detect compiler-flag drift between fbuild and PlatformIO for a single board.

Background (FastLED#2410)
=========================
fbuild silently drifted from PlatformIO and broke Teensy 4.x for 50+
consecutive master CI runs (`.progmem` section-type conflict). The
teensy30/31/LC envs kept using PlatformIO, stayed green, and masked the
problem from anyone reading the CI dashboard.

What this script does
=====================
For one board (``--board <name>``):

1. Drive ``ci/ci-compile.py`` once with ``--fbuild`` and once with
   ``--pio`` to compile a canonical sketch (default: ``Blink``). Both
   backends are already cache-friendly, so reruns are cheap.
2. Locate each backend's ``build_info_<example>.json`` (or
   ``build_info.json`` fallback) and normalize its flags into a common
   shape (``cc_flags``, ``cxx_flags``, ``defines``, ``includes``) with
   path-specific tokens stripped and the lists sorted.
3. Diff the two normalized sets.
4. Print a structured report to stdout and exit 0 if no drift,
   1 if drift is detected, 2 on hard failure (missing build_info,
   compile failed).

Usage
-----
    uv run python ci/check_backend_flag_drift.py --board teensy41
    uv run python ci/check_backend_flag_drift.py --board teensy41 --example Blink
    uv run python ci/check_backend_flag_drift.py --board teensy41 --skip-build  # use cached build_info

Follow-ups (out of scope for the initial prototype, tracked in #2410):
- Matrix over every board the project ships (esp32*, teensy*, avr, rp2040, ...).
- Wire into a CI workflow that posts the diff as a check annotation
  before flipping to a required gate.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

from running_process import RunningProcess

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# ---------------------------------------------------------------------------
# Normalization
# ---------------------------------------------------------------------------

# Flags that legitimately differ between backends and would otherwise drown
# the real signal in noise. These are reviewed below in `_should_ignore_flag`:
#
#   -MMD / -MD / -MF / -MP / -MT / -MQ      dep-file machinery, generated paths
#   -o <path>                               output path
#   -c <path>                               source path
#   -include <path>                         per-build PCH/forced include path
#   -fmacro-prefix-map / -ffile-prefix-map  reproducible-build path rewrites
#   --sysroot=<path>                        toolchain-relative
#   -fdiagnostics-color=*                   purely cosmetic
#   --target=*                              clang vs gcc; not a real semantic diff
_PATH_LIKE_FLAG_PREFIXES: tuple[str, ...] = (
    "-MF",
    "-MT",
    "-MQ",
    "-fmacro-prefix-map=",
    "-ffile-prefix-map=",
    "--sysroot=",
    "-fdiagnostics-color",
    "--target=",
)

_STANDALONE_PATH_FLAGS: frozenset[str] = frozenset(
    {"-MMD", "-MD", "-MP", "-o", "-c", "-include", "-imacros"}
)


# Defines we deliberately strip because they reflect which backend ran the
# build, not a real difference in the compiler's view of the program:
#   PLATFORMIO         set by PIO and also forwarded by fbuild for parity
#   PLATFORMIO=NNN     version stamp; ignore the version itself, keep the name
_PIO_VERSION_DEFINE_RE = re.compile(r"^PLATFORMIO=\d+$")


@dataclass(frozen=True)
class NormalizedFlags:
    """Backend-agnostic view of the compiler invocation for one board."""

    cc_flags: frozenset[str]
    cxx_flags: frozenset[str]
    defines: frozenset[str]
    includes: frozenset[str]


@dataclass
class DriftReport:
    """Per-section diff produced by :func:`compute_drift`."""

    board: str
    cc_only_in_fbuild: list[str] = field(default_factory=list)
    cc_only_in_pio: list[str] = field(default_factory=list)
    cxx_only_in_fbuild: list[str] = field(default_factory=list)
    cxx_only_in_pio: list[str] = field(default_factory=list)
    defines_only_in_fbuild: list[str] = field(default_factory=list)
    defines_only_in_pio: list[str] = field(default_factory=list)
    includes_only_in_fbuild: list[str] = field(default_factory=list)
    includes_only_in_pio: list[str] = field(default_factory=list)

    def has_drift(self) -> bool:
        return any(
            (
                self.cc_only_in_fbuild,
                self.cc_only_in_pio,
                self.cxx_only_in_fbuild,
                self.cxx_only_in_pio,
                self.defines_only_in_fbuild,
                self.defines_only_in_pio,
                self.includes_only_in_fbuild,
                self.includes_only_in_pio,
            )
        )


def _should_ignore_flag(flag: str) -> bool:
    """True if a raw cc/cxx flag is build-noise and not a real drift signal."""
    if flag in _STANDALONE_PATH_FLAGS:
        return True
    for prefix in _PATH_LIKE_FLAG_PREFIXES:
        if flag.startswith(prefix):
            return True
    return False


def _normalize_define(define: str) -> str | None:
    """Strip backend-version noise from a single define; None to drop entirely."""
    # PIO emits "PLATFORMIO=60119"; fbuild emits "PLATFORMIO". Normalize both
    # to just the bare name so we don't false-positive on the version stamp.
    if _PIO_VERSION_DEFINE_RE.match(define):
        return "PLATFORMIO"
    return define


def _normalize_include(path: str) -> str:
    """Make include paths comparable across the two backend cache roots.

    fbuild stores frameworks under ``~/.fbuild/prod/cache/platforms/<...>/``
    while PIO stores them under ``~/.platformio/packages/<...>/``. Both
    eventually point at e.g. ``framework-arduinoteensy/cores/teensy4``. We
    strip everything up to and including a known cache-root marker so two
    paths that resolve to the same logical directory compare equal.
    """
    normalized = path.replace("\\", "/").rstrip("/")

    # Markers in preference order. Whatever lives *after* the marker is the
    # framework-relative subpath that actually matters for drift detection.
    cache_markers = (
        "/.fbuild/prod/cache/platforms/",
        "/.fbuild/cache/platforms/",
        "/.platformio/packages/",
        "/.platformio/lib/",
    )
    for marker in cache_markers:
        idx = normalized.rfind(marker)
        if idx != -1:
            tail = normalized[idx + len(marker) :]
            # Drop the first path component (e.g. "framework-arduinoteensy"
            # or a long fbuild hash like "dl-framework-arduinoteensy-1.160.0/<hash>/1.160.0").
            # We keep only what's after the first '/cores/' or '/libraries/'
            # since those are the meaningful directories.
            for keep in ("/cores/", "/libraries/", "/variants/", "/tools/"):
                k = tail.find(keep)
                if k != -1:
                    return tail[k:].lstrip("/")
            return tail

    # Repo-relative paths: strip the repo prefix when present so two clones in
    # different parent dirs still compare equal.
    for marker in ("/fastled5/", "/FastLED/", "/.build/pio/"):
        idx = normalized.rfind(marker)
        if idx != -1:
            return normalized[idx + len(marker) :]

    return normalized


def _normalize_flag(flag: str) -> str:
    """Strip a single cc/cxx flag of path-specific tokens.

    Currently a no-op for flags we keep — most semantic flags (``-mcpu``,
    ``-O2``, ``-std=gnu++17``, ``-Wno-*``) are already path-free. Extracted
    as its own function so the right place to grow per-flag rewrites is
    obvious.
    """
    return flag


def _split_fbuild_combined_flags(
    cc_flags: list[str],
) -> tuple[list[str], list[str], list[str]]:
    """fbuild's ``cc_flags`` mixes ``-D``/``-I`` in; split them back out.

    Returns (pure_cc_flags, defines, includes). Defines drop the ``-D``
    prefix to match PIO's format. Includes drop the ``-I`` prefix.
    """
    pure: list[str] = []
    defines: list[str] = []
    includes: list[str] = []
    for raw in cc_flags:
        if raw.startswith("-D"):
            defines.append(raw[2:])
        elif raw.startswith("-I"):
            includes.append(raw[2:])
        else:
            pure.append(raw)
    return pure, defines, includes


def normalize_build_info(raw: dict, board: str, backend: str) -> NormalizedFlags:
    """Convert one backend's ``build_info_<example>.json`` into NormalizedFlags.

    Both backends key the per-board record by ``board`` (e.g.
    ``"teensy41"``). The schemas differ slightly:

    - fbuild emits ``cc_flags`` that already contains ``-D``/``-I`` tokens
      alongside the real cc flags, plus separate ``defines`` and ``includes``
      mirrors. We prefer the separate mirrors when present.
    - PIO emits ``cc_flags`` / ``cxx_flags`` as pure compiler flags and keeps
      ``defines`` (no ``-D``) and ``includes.build`` / ``includes.compatlib``
      as the source of truth.
    """
    if board not in raw:
        # Some build_info variants are keyed by the example or env name.
        # Use the first (and usually only) top-level entry as a fallback.
        if len(raw) != 1:
            raise ValueError(
                f"{backend} build_info has unexpected top-level keys "
                f"{sorted(raw)}; expected exactly one ('{board}')"
            )
        board_record = next(iter(raw.values()))
    else:
        board_record = raw[board]

    raw_cc = list(board_record.get("cc_flags", []))
    raw_cxx = list(board_record.get("cxx_flags", []))
    raw_defines = list(board_record.get("defines", []))
    raw_includes_field = board_record.get("includes", [])

    # Split fbuild's combined cc_flags back into clean buckets.
    if backend == "fbuild":
        raw_cc, extracted_defines, extracted_includes = _split_fbuild_combined_flags(
            raw_cc
        )
        if not raw_defines:
            raw_defines = extracted_defines
        if not raw_includes_field:
            raw_includes_field = extracted_includes
        # fbuild often duplicates the same cc set into cxx_flags or doesn't
        # populate cxx_flags separately. If empty, fall back to cc.
        if raw_cxx:
            raw_cxx, _, _ = _split_fbuild_combined_flags(raw_cxx)
        else:
            raw_cxx = list(raw_cc)

    # PIO's includes is a dict-of-lists keyed by scope (build/compatlib/...).
    # Flatten to a single list for the comparison.
    if isinstance(raw_includes_field, dict):
        flat_includes: list[str] = []
        for scope in ("build", "compatlib", "toolchain"):
            flat_includes.extend(raw_includes_field.get(scope, []))
    else:
        flat_includes = list(raw_includes_field)

    cc_clean = {_normalize_flag(f) for f in raw_cc if not _should_ignore_flag(f)}
    cxx_clean = {_normalize_flag(f) for f in raw_cxx if not _should_ignore_flag(f)}
    defines_clean = {
        norm for d in raw_defines if (norm := _normalize_define(d)) is not None
    }
    includes_clean = {_normalize_include(p) for p in flat_includes if p}

    return NormalizedFlags(
        cc_flags=frozenset(cc_clean),
        cxx_flags=frozenset(cxx_clean),
        defines=frozenset(defines_clean),
        includes=frozenset(includes_clean),
    )


def compute_drift(
    fbuild: NormalizedFlags, pio: NormalizedFlags, board: str
) -> DriftReport:
    """Set-diff each section and return the structured report."""
    return DriftReport(
        board=board,
        cc_only_in_fbuild=sorted(fbuild.cc_flags - pio.cc_flags),
        cc_only_in_pio=sorted(pio.cc_flags - fbuild.cc_flags),
        cxx_only_in_fbuild=sorted(fbuild.cxx_flags - pio.cxx_flags),
        cxx_only_in_pio=sorted(pio.cxx_flags - fbuild.cxx_flags),
        defines_only_in_fbuild=sorted(fbuild.defines - pio.defines),
        defines_only_in_pio=sorted(pio.defines - fbuild.defines),
        includes_only_in_fbuild=sorted(fbuild.includes - pio.includes),
        includes_only_in_pio=sorted(pio.includes - fbuild.includes),
    )


def render_report(report: DriftReport) -> str:
    """Human-readable rendering. Empty sections collapse to a one-liner."""
    lines: list[str] = []
    header = f"Backend flag-drift report for {report.board!r}"
    lines.append(header)
    lines.append("=" * len(header))
    lines.append("")

    sections = [
        ("cc_flags", report.cc_only_in_fbuild, report.cc_only_in_pio),
        ("cxx_flags", report.cxx_only_in_fbuild, report.cxx_only_in_pio),
        ("defines", report.defines_only_in_fbuild, report.defines_only_in_pio),
        ("includes", report.includes_only_in_fbuild, report.includes_only_in_pio),
    ]

    for name, only_fbuild, only_pio in sections:
        if not only_fbuild and not only_pio:
            lines.append(f"  [{name}] OK (no drift)")
            continue
        lines.append(f"  [{name}] drift:")
        for f in only_fbuild:
            lines.append(f"    + (fbuild only) {f}")
        for f in only_pio:
            lines.append(f"    - (pio only)    {f}")

    lines.append("")
    lines.append("VERDICT: DRIFT DETECTED" if report.has_drift() else "VERDICT: clean")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Locating build_info_*.json
# ---------------------------------------------------------------------------


def _find_build_info(
    project_root: Path, board: str, example: str, backend: str
) -> Path | None:
    """Locate the most specific ``build_info_*.json`` for (board, example, backend).

    Search order:
      1. ``.build/pio/<board>/build_info_<example>.json``  (both backends)
      2. ``.build/pio/<board>/build_info.json``
      3. ``<project_root>/build_info_<board>.json`` (fbuild top-level dump)
      4. ``<project_root>/build_info.json`` (fbuild generic dump)

    The ``backend`` arg is kept for future use (e.g. backend-specific
    suffixes) but currently both backends write into the same
    ``.build/pio/<board>/`` directory because that's the convention
    ``generate_build_info_json_from_existing_build`` uses.
    """
    del backend  # reserved for future use
    candidates: list[Path] = [
        project_root / ".build" / "pio" / board / f"build_info_{example}.json",
        project_root / ".build" / "pio" / board / "build_info.json",
        project_root / f"build_info_{board}.json",
        project_root / "build_info.json",
    ]
    for c in candidates:
        if c.exists():
            return c
    return None


# ---------------------------------------------------------------------------
# Driving the two backend compiles
# ---------------------------------------------------------------------------


def _run_compile(
    project_root: Path,
    board: str,
    example: str,
    backend: str,
    *,
    timeout_seconds: int,
) -> bool:
    """Invoke ``ci/ci-compile.py`` for ``board`` with the chosen ``backend``.

    Returns True on success. Output is streamed so a slow first-time
    framework download is visible to the user.
    """
    if backend not in {"fbuild", "platformio"}:
        raise ValueError(f"unknown backend {backend!r}")

    backend_flag = "--fbuild" if backend == "fbuild" else "--pio"
    cmd: list[str] = [
        "uv",
        "run",
        "python",
        "ci/ci-compile.py",
        board,
        "--examples",
        example,
        backend_flag,
    ]

    print(f"\n>>> Compiling [{backend}]: {' '.join(cmd)}", flush=True)
    try:
        result = RunningProcess.run(
            cmd,
            check=False,
            cwd=str(project_root),
            timeout=timeout_seconds,
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise

    success = result.returncode == 0
    if not success:
        print(
            f"\n!!! Compile failed for backend={backend} board={board} "
            f"(rc={result.returncode})",
            file=sys.stderr,
        )
    return success


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Detect compiler-flag drift between the fbuild and PlatformIO "
            "backends for a single board. Exits non-zero on drift."
        )
    )
    parser.add_argument(
        "--board",
        required=True,
        help="Board to check (e.g. teensy41, esp32dev).",
    )
    parser.add_argument(
        "--example",
        default="Blink",
        help="Sketch to compile in each backend (default: Blink).",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help=(
            "Skip the actual compiles and reuse any existing "
            "build_info_*.json files. Fails if neither backend has produced "
            "one yet."
        ),
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=900,
        help="Per-backend compile timeout in seconds (default: 900).",
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=None,
        help="Override the FastLED repo root (default: auto-detect).",
    )
    args = parser.parse_args(argv)

    project_root = (
        args.project_root.resolve()
        if args.project_root is not None
        else Path(__file__).resolve().parent.parent
    )

    # The two backends overwrite the same .build/pio/<board>/build_info_*.json
    # file, so we must snapshot fbuild's output BEFORE running pio. The
    # --skip-build path only works when the user has already manually staged
    # both backends' build_info JSONs (or saved one elsewhere); we don't have
    # a way to recover a fresh fbuild snapshot from disk after pio has run.

    fbuild_info: dict
    pio_info: dict

    if args.skip_build:
        bi_path = _find_build_info(project_root, args.board, args.example, "fbuild")
        if bi_path is None:
            print(
                f"ERROR: no build_info_*.json found for board={args.board}. "
                f"Run without --skip-build first.",
                file=sys.stderr,
            )
            return 2
        print(
            f"WARNING: --skip-build only found a single build_info at {bi_path};"
            " cannot diff two backends. Re-run without --skip-build.",
            file=sys.stderr,
        )
        return 2

    # Compile fbuild, snapshot its build_info, then compile pio, snapshot.
    if not _run_compile(
        project_root,
        args.board,
        args.example,
        backend="fbuild",
        timeout_seconds=args.timeout,
    ):
        return 2
    fbuild_bi_path = _find_build_info(project_root, args.board, args.example, "fbuild")
    if fbuild_bi_path is None:
        print(
            f"ERROR: fbuild compile succeeded but no build_info_*.json was "
            f"emitted for board={args.board}.",
            file=sys.stderr,
        )
        return 2
    fbuild_info = json.loads(fbuild_bi_path.read_text())
    print(f"  fbuild snapshot: {fbuild_bi_path}")

    if not _run_compile(
        project_root,
        args.board,
        args.example,
        backend="platformio",
        timeout_seconds=args.timeout,
    ):
        return 2
    pio_bi_path = _find_build_info(project_root, args.board, args.example, "platformio")
    if pio_bi_path is None:
        print(
            f"ERROR: PlatformIO compile succeeded but no build_info_*.json "
            f"was emitted for board={args.board}.",
            file=sys.stderr,
        )
        return 2
    pio_info = json.loads(pio_bi_path.read_text())
    print(f"  pio    snapshot: {pio_bi_path}")

    try:
        fbuild_norm = normalize_build_info(fbuild_info, args.board, "fbuild")
        pio_norm = normalize_build_info(pio_info, args.board, "platformio")
    except ValueError as e:
        print(f"ERROR: failed to normalize build_info: {e}", file=sys.stderr)
        return 2

    report = compute_drift(fbuild_norm, pio_norm, args.board)
    print()
    print(render_report(report))
    return 1 if report.has_drift() else 0


if __name__ == "__main__":
    # The KeyboardInterrupt is allowed to propagate (per project policy);
    # _run_compile already routes it through handle_keyboard_interrupt.
    sys.exit(main())
