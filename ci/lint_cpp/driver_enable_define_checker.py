"""Lint: driver selection is by template, never a code-enabling define.

A driver must be selected by template — a `BusTraits<Bus::X>` /
`BusSupports<Bus::X, Chipset>` specialization — and left to the linker's
dead-strip pass, not switched on by an opt-in `#define`. An opt-in define moves
the decision to the build system and prevents the driver's translation unit from
being link-eliminated *or* compile-verified (FastLED#2428 / #4940 / #4949).

This checker flags a preprocessor guard that turns platform/driver code on by
wrapping an `#include "platforms/...` header under `src/platforms/**` in an
`FL_*` / `FASTLED_*` macro. The guards that ship today are amnestied in
`driver_enable_define_amnesty.txt`; the check is designed to catch *new* ones.
When a guard is intentionally added after review, add its macro name to that
file (see `--generate-amnesty`).

This is a Python-only checker because the distinction needs to look at both the
guard and the line it protects; the Rust C++ linter's per-line model does not
express it (see `ci/lint_cpp/run_all_checkers.py`).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import TYPE_CHECKING

from ci.util.check_files import FileContentChecker


if TYPE_CHECKING:
    from ci.util.check_files import FileContent

_AMNESTY_PATH = Path(__file__).with_name("driver_enable_define_amnesty.txt")

_GUARD = re.compile(r"^\s*#\s*(?P<kind>ifdef|ifndef|if)\b(?P<cond>.*)$")
_INCLUDE = re.compile(r'^\s*#\s*include\s+"(?P<header>platforms/[^"]+)"')
_MACRO = re.compile(r"\b(?:FL|FASTLED)_[A-Z0-9_]+\b")

_PLATFORMS_PREFIX = "src/platforms/"


def _is_platforms_source(file_path: str) -> bool:
    """True for a C/C++ source under `src/platforms/` (relative or absolute)."""
    normalized = file_path.replace("\\", "/")
    marker = f"/{_PLATFORMS_PREFIX}"
    if not (normalized.startswith(_PLATFORMS_PREFIX) or marker in normalized):
        return False
    return Path(normalized).suffix in {".h", ".hpp", ".c", ".cpp"}


def _guarded_platform_macros(lines: list[str]) -> list[tuple[int, str]]:
    """(line_number, macro_name) for each guard that wraps a platform include."""
    found: list[tuple[int, str]] = []
    for index, line in enumerate(lines):
        guard = _GUARD.match(line)
        if guard is None:
            continue
        # The guard's own macro tokens.
        names = _MACRO.findall(guard.group("cond"))
        if not names:
            continue
        # Does this guard directly protect an `#include "platforms/...`? Allow a
        # few blank/comment lines, and stop at a nested directive.
        for probe in range(index + 1, min(index + 5, len(lines))):
            probe_line = lines[probe]
            if _GUARD.match(probe_line):
                break
            if _INCLUDE.match(probe_line):
                for name in names:
                    found.append((index + 1, name))
                break
    return found


def load_amnesty(path: Path = _AMNESTY_PATH) -> frozenset[str]:
    """Macro names whose guard-around-include is grandfathered in."""
    if not path.exists():
        return frozenset()
    return frozenset(
        line.strip()
        for line in path.read_text().splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    )


class DriverEnableDefineChecker(FileContentChecker):
    """Flags a new opt-in `#define`-style guard around a platform driver include."""

    def __init__(self) -> None:
        self._amnesty = load_amnesty()

    def should_process_file(self, file_path: str) -> bool:
        return _is_platforms_source(file_path)

    def check_file_content(self, file_content: "FileContent") -> list[str]:
        issues: list[str] = []
        for line_number, name in _guarded_platform_macros(file_content.lines):
            if name in self._amnesty:
                continue
            issues.append(
                f"{file_content.path}:{line_number}: '{name}' turns platform code "
                f"on via a #define. Select drivers by template "
                f"(`BusTraits<Bus::X>` / `BusSupports<Bus::X, Chipset>`) and let "
                f"the linker eliminate the unused ones — see "
                f"agents/docs/cpp-standards.md (FastLED#4940). If this guard is "
                f"deliberate, add '{name}' to "
                f"ci/lint_cpp/driver_enable_define_amnesty.txt."
            )
        return issues


def _iter_source_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if path.is_file() and path.suffix in {".h", ".hpp", ".c", ".cpp"}:
            files.append(path)
    return sorted(files)


def generate_amnesty(project_root: Path, out_path: Path = _AMNESTY_PATH) -> int:
    """Write every currently-guarded macro name to the amnesty file."""
    names: set[str] = set()
    platforms_root = project_root / _PLATFORMS_PREFIX
    for path in _iter_source_files(platforms_root):
        lines = path.read_text(errors="ignore").splitlines()
        names.update(name for _, name in _guarded_platform_macros(lines))
    header = (
        "# Macros whose #ifdef-style guard around a `platforms/...` include is\n"
        "# grandfathered in. Regenerate with:\n"
        "#   uv run ci/lint_cpp/driver_enable_define_checker.py --generate-amnesty\n"
        "# New entries must be justified in review: driver selection belongs to\n"
        "# `BusTraits`/`BusSupports`, not a code-enabling define (FastLED#4940).\n"
    )
    out_path.write_text(header + "\n".join(sorted(names)) + "\n")
    return len(names)


if __name__ == "__main__":  # pragma: no cover - manual maintenance entry point
    from ci.util.paths import PROJECT_ROOT

    if "--generate-amnesty" in sys.argv:
        count = generate_amnesty(PROJECT_ROOT)
        print(f"Wrote {count} macro name(s) to {_AMNESTY_PATH}")
    else:
        print("Usage: driver_enable_define_checker.py --generate-amnesty")
        sys.exit(2)
