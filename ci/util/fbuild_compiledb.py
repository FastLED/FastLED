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


def fbuild_release_dir(build_root: Path, board_name: str) -> Path:
    """Path to the fbuild release dir for ``board_name`` under ``build_root``.

    Matches ``PioCompiler._artifacts_dir`` in ``ci/compiler/pio.py``.
    """
    return build_root / ".fbuild" / "build" / board_name / "release"


def was_compiled_with_fbuild(build_root: Path, board_name: str) -> bool:
    """True iff fbuild produced artifacts for ``board_name``.

    Detection probes the release dir (``.build/.fbuild/build/<env>/release/``)
    which fbuild's ESP32 orchestrator populates during compile. This matches
    the detection logic lifted from ``ci/ci-cppcheck.py`` and is the canonical
    fbuild-vs-PIO backend signal for post-compile tooling.
    """
    return fbuild_release_dir(build_root, board_name).exists()


def ensure_compile_commands(
    project_root: Path, build_root: Path, board_name: str
) -> Path | None:
    """Ensure ``compile_commands.json`` exists for ``board_name``; return its path.

    If the DB is already present, returns it immediately. Otherwise shells out
    to ``fbuild build -e <env> --target compiledb`` and returns the resulting
    path. Returns ``None`` if fbuild isn't on PATH or the subprocess fails.

    Args:
        project_root: FastLED repo root (the directory fbuild resolves
            ``fbuild.toml`` / ``pyproject.toml`` from).
        build_root: The ``.build/`` directory — fbuild writes its artifacts
            under ``<build_root>/.fbuild/build/<env>/``.
        board_name: Canonical board / fbuild environment name (e.g.
            ``esp32c2``, matching the ``-e`` flag on fbuild).

    Returns:
        Path to ``compile_commands.json`` on success, ``None`` otherwise.
    """
    release = fbuild_release_dir(build_root, board_name)
    cdb = release / "compile_commands.json"
    if cdb.exists():
        return cdb

    # Canonical FastLED fbuild invocation order (matches
    # ``ci/util/fbuild_runner.py:104-114``): project dir goes *before* the
    # ``build`` subcommand. ``cwd`` is still set so fbuild's config
    # discovery works identically whether the positional is honored or
    # ignored by future fbuild versions.
    try:
        subprocess.run(
            [
                "fbuild",
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

    return cdb if cdb.exists() else None
