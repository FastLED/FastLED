"""Generate and locate fbuild's compile_commands.json for a given board.

fbuild 2.1+ supports ``fbuild build -e <env> --target compiledb``, which
emits ``compile_commands.json`` into the fbuild release dir without
performing a full compile. Static-analysis entry points
(``ci/ci-cppcheck.py``, ``ci/ci-iwyu.py``) can feed that database to
``cppcheck --project=``, ``clang-tool-chain-iwyu-tool -p``, etc., so
they no longer depend on a PlatformIO project tree.

See FastLED#2301 / #2302 / #2303 for background.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from ci.util.fbuild_runner import get_fbuild_executable


def _candidate_fbuild_release_dirs(
    project_root: Path, build_root: Path, board_name: str
) -> list[Path]:
    """Candidate locations where fbuild may have written artifacts.

    FastLED compiles fbuild with ``build_dir=<repo>/.build/pio/<board>/`` (see
    ``ci/compiler/pio.py::_artifacts_dir``), so fbuild's output lives at
    ``.build/pio/<board>/.fbuild/build/<env>/release/``. The standalone CLI
    (``fbuild <repo> build --target compiledb``) instead emits
    ``<repo>/.fbuild/build/<env>/release/`` — we treat both as valid so that a
    direct ``--target compiledb`` invocation (no prior FastLED compile) also
    works. An older ``.build/.fbuild/…`` layout is kept as a tail fallback.

    ``project_root`` is passed explicitly rather than derived from
    ``build_root.name == ".build"`` — callers always know their repo root
    (every call site has ``_PROJECT_ROOT`` or ``project_root`` locally), and
    an explicit arg removes the fragile name-based heuristic that silently
    collapsed candidates when ``build_root`` wasn't conventionally named.

    Returned in preference order (first hit wins).
    """
    return [
        build_root / "pio" / board_name / ".fbuild" / "build" / board_name / "release",
        project_root / ".fbuild" / "build" / board_name / "release",
        build_root / ".fbuild" / "build" / board_name / "release",
    ]


def fbuild_release_dir(project_root: Path, build_root: Path, board_name: str) -> Path:
    """Return the canonical fbuild release dir for ``board_name``.

    Prefers the first of the candidate locations in
    :func:`_candidate_fbuild_release_dirs` that already exists, and falls back
    to the FastLED-orchestrated layout
    (``.build/pio/<board>/.fbuild/build/<env>/release/``) when none do — so
    callers can still materialize artifacts there via
    :func:`ensure_compile_commands`.
    """
    candidates = _candidate_fbuild_release_dirs(project_root, build_root, board_name)
    for cand in candidates:
        if cand.exists():
            return cand
    return candidates[0]


def was_compiled_with_fbuild(
    project_root: Path, build_root: Path, board_name: str
) -> bool:
    """True iff fbuild produced artifacts for ``board_name`` anywhere we look.

    Probes every candidate release dir from
    :func:`_candidate_fbuild_release_dirs` — fbuild's ESP32 / AVR / Teensy
    orchestrators populate this directory during compile, so its presence is
    the canonical fbuild-vs-PIO backend signal for post-compile tooling.
    """
    return any(
        cand.exists()
        for cand in _candidate_fbuild_release_dirs(project_root, build_root, board_name)
    )


def _find_existing_compile_db(
    project_root: Path, build_root: Path, board_name: str
) -> Path | None:
    """Return an existing ``compile_commands.json`` for ``board_name``, or None."""
    for cand in _candidate_fbuild_release_dirs(project_root, build_root, board_name):
        cdb = cand / "compile_commands.json"
        if cdb.exists():
            return cdb
    return None


def ensure_compile_commands(
    project_root: Path, build_root: Path, board_name: str
) -> Path | None:
    """Ensure ``compile_commands.json`` exists for ``board_name``; return its path.

    If the DB is already present at any known location (see
    :func:`_candidate_fbuild_release_dirs`), returns it immediately. Otherwise
    shells out to ``fbuild <project_root> build -e <env> --target compiledb``
    and returns the resulting path. Returns ``None`` if fbuild isn't on PATH
    or the subprocess fails.

    Args:
        project_root: FastLED repo root (the directory fbuild resolves
            ``fbuild.toml`` / ``pyproject.toml`` from).
        build_root: The ``.build/`` directory.
        board_name: Canonical board / fbuild environment name (e.g.
            ``esp32c2``, matching the ``-e`` flag on fbuild).

    Returns:
        Path to ``compile_commands.json`` on success, ``None`` otherwise.
    """
    cdb = _find_existing_compile_db(project_root, build_root, board_name)
    if cdb is not None:
        return cdb

    # Canonical FastLED fbuild invocation order (matches
    # ``ci/util/fbuild_runner.py:104-114``): project dir goes *before* the
    # ``build`` subcommand. ``cwd`` is still set so fbuild's config
    # discovery works identically whether the positional is honored or
    # ignored by future fbuild versions.
    fbuild_exe = get_fbuild_executable()
    if fbuild_exe is None:
        print(
            f"ensure_compile_commands: fbuild executable not found for '{board_name}'",
            file=sys.stderr,
        )
        return None
    try:
        subprocess.run(
            [
                fbuild_exe,
                str(project_root),
                "build",
                "-e",
                board_name,
                "--target",
                "compiledb",
            ],
            check=True,
            cwd=str(project_root),
        )
    except KeyboardInterrupt as ki:
        # Per project guideline: all try/except blocks must handle
        # KeyboardInterrupt before broader exceptions so Ctrl-C is never
        # swallowed.
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        handle_keyboard_interrupt(ki)
        raise
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        print(
            f"ensure_compile_commands: failed to generate fbuild compile DB "
            f"for '{board_name}': {exc}",
            file=sys.stderr,
        )
        return None

    # Re-scan candidate locations — ``fbuild --target compiledb`` writes to
    # ``<repo>/.fbuild/build/<env>/release/`` regardless of which candidate
    # the prior FastLED compile populated.
    return _find_existing_compile_db(project_root, build_root, board_name)
