"""Reader for the standardized per-platform smoke-sketch manifest.

Issue: https://github.com/FastLED/FastLED/issues/2411 (Part A).

The manifest at ``tests/platforms/_standard/SMOKE_SKETCHES.txt`` is the
canonical list of FastLED example sketches that EVERY supported board must
compile in CI. Per-board overrides remain under ``tests/platforms/<board>/``.

The format is intentionally trivial - one example name per line, ``#`` for
comments, blank lines ignored - so any consumer (Python, bash, shell scripts,
GitHub Actions) can parse it without pulling in YAML/TOML dependencies.

Typical usage::

    from ci.standardized_smoke_sketches import load_smoke_sketches
    for sketch in load_smoke_sketches(None):
        # feed to ci-compile.py / fbuild / pio_compile / arduino-cli ...
        ...

Per-board filtering (e.g. ``AudioFftParity`` is ESP32-only) is handled by the
existing ``@filter`` directives inside each ``.ino`` - this loader stays
platform-agnostic on purpose.
"""

import sys
from dataclasses import dataclass
from pathlib import Path

from typeguard import typechecked

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# Repo-relative location of the manifest. Kept as a module-level constant so
# CI tooling can reference it without re-deriving the path.
MANIFEST_RELATIVE_PATH = "tests/platforms/_standard/SMOKE_SKETCHES.txt"


@typechecked
@dataclass(frozen=True)
class SmokeSketch:
    """A single entry from the standardized smoke-sketch manifest.

    Attributes:
        name: Example name as it appears in the manifest (e.g. ``"Blink"``).
              Resolves to ``examples/<name>/`` under the project root.
        manifest_line: 1-indexed line number in the manifest the entry came
              from. Useful for error messages.
    """

    name: str
    manifest_line: int


def _project_root() -> Path:
    """Return the FastLED project root.

    The reader lives at ``ci/standardized_smoke_sketches.py``; the repo root
    is the parent of ``ci/``.
    """
    return Path(__file__).resolve().parent.parent


def manifest_path(project_root: Path | None) -> Path:
    """Return the absolute path to the smoke-sketch manifest.

    Args:
        project_root: Repo root override (used by tests). Pass ``None`` to
            use the FastLED repo root inferred from this file's location.
    """
    root = project_root if project_root is not None else _project_root()
    return root / MANIFEST_RELATIVE_PATH


def load_smoke_sketches_detailed(
    project_root: Path | None,
) -> list[SmokeSketch]:
    """Parse the manifest and return entries with line-number metadata.

    Comments (``#`` to end of line) and blank lines are skipped. Inline
    comments are supported (``Blink  # basic clockless``).

    Args:
        project_root: Repo root override (used by tests). Pass ``None`` to
            use the FastLED repo root inferred from this file's location.

    Returns:
        List of :class:`SmokeSketch` entries in manifest order.

    Raises:
        FileNotFoundError: If the manifest file does not exist.
    """
    path = manifest_path(project_root)
    if not path.is_file():
        raise FileNotFoundError(f"Standardized smoke-sketch manifest not found: {path}")

    entries: list[SmokeSketch] = []
    seen: set[str] = set()
    with path.open("r", encoding="utf-8") as fh:
        for lineno, raw in enumerate(fh, start=1):
            # Strip inline comments and surrounding whitespace.
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            if line in seen:
                # Duplicate names would silently double-compile the same
                # sketch - surface this as a hard error so the manifest
                # stays the single source of truth.
                raise ValueError(
                    f"Duplicate smoke sketch '{line}' on line {lineno} of {path}"
                )
            seen.add(line)
            entries.append(SmokeSketch(name=line, manifest_line=lineno))

    return entries


def load_smoke_sketches(project_root: Path | None) -> list[str]:
    """Return just the sketch names from the manifest, in manifest order.

    Convenience wrapper around :func:`load_smoke_sketches_detailed` for
    callers (e.g. shell pipelines) that don't need line-number metadata.

    Args:
        project_root: Repo root override (used by tests). Pass ``None`` to
            use the FastLED repo root inferred from this file's location.
    """
    names: list[str] = []
    for entry in load_smoke_sketches_detailed(project_root):
        names.append(entry.name)
    return names


def _main() -> int:
    """CLI entry point - prints the smoke-sketch names, one per line.

    Sample invocations::

        uv run python ci/standardized_smoke_sketches.py
        bash compile esp32dev --examples $(uv run python ci/standardized_smoke_sketches.py | tr '\\n' ' ')
    """
    try:
        for name in load_smoke_sketches(None):
            print(name)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        return 130
    except FileNotFoundError as exc:
        print(f"ERROR: {exc}")
        return 1
    except ValueError as exc:
        print(f"ERROR: {exc}")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(_main())
