"""Repo-wide ban on the PlatformIO tool: the words may not appear anywhere.

fbuild is the only board build backend. The PlatformIO tool was purged from
the build system, CI and tooling in September 2026, and this lint keeps it
out: every tracked (or untracked, non-ignored) text file is grepped for the
banned words and any hit fails ``bash lint``. There is no warn-only mode.

Banned:
  - ``platformio`` in any case, unless the match is the literal file name
    ``platformio.ini`` / ``platformio.lock`` (the project format fbuild reads)
    or the ``PLATFORMIO_SRC_DIR`` environment variable that fbuild honours. The
    names themselves are allowed; nothing else is.
  - ``pio`` as a whole lowercase word (regex ``\\bpio\\b``). ``GPIO``, ``PIO``
    and ``pio_sm_init`` are not matches.

Allowed paths (the surfaces downstream users need, RP2040's PIO peripheral,
and the enforcement code itself) are listed in ``ALLOWED_PATHS`` and
``ALLOWED_PREFIXES``. Add to them only with a reason in the comment.

Run directly: ``uv run python ci/lint/banned_build_tools.py [--root DIR]``.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path
from typing import Iterable, NamedTuple


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

# Whole files where the words are allowed. Repo-relative POSIX paths.
ALLOWED_PATHS: frozenset[str] = frozenset(
    {
        # Downstream users consume FastLED through the PlatformIO IDE; these
        # are the library's manifest and its frozen root project file.
        "library.json",
        "platformio.ini",
        "idf_component.yml",
        "src/platforms/ldf_headers.h",  # LDF hints for that IDE's dependency finder
        # User-facing docs may say "the PlatformIO IDE" once.
        "README.md",
        "SLIDE.md",
        "release_notes.md",
        "CLAUDE.md",  # one sentence explaining the ban
        "examples/README.md",
        # Names an external data branch of FastLED/boards that is literally
        # called `platformio`; the registry doc has to spell it.
        "agents/docs/usb-vid-pid-registry.md",
        # Enforcement: the shell hook that blocks the commands, this lint,
        # and its tests must spell the words out.
        "ci/hooks/check_forbidden_commands.py",
        # The one sanctioned invocation: registry publishing of a release
        # (CLAUDE.md, "One exception to the PlatformIO ban"). Publish only.
        "ci/release.py",
        "ci/lint/banned_build_tools.py",
        "ci/tests/test_banned_build_tools.py",
    }
)

# Directory prefixes (repo-relative, POSIX, trailing slash) where the words
# are allowed.
ALLOWED_PREFIXES: tuple[str, ...] = (
    "src/platforms/arm/rp/",  # RP2040/RP2350 PIO peripheral (hardware/pio.h, `PIO pio`)
    "src/platforms/arm/sam/",  # SAM3X PIO controller (`Pio* pio`)
    "examples/AutoResearch/AutoResearchRpPio",  # RP PIO peripheral bring-up tests
    "cookbook/",  # user-facing getting-started docs
    ".github/ISSUE_TEMPLATE/",  # bug report asks which IDE the user has
)

# File names allowed anywhere: the ini is fbuild's project format, and every
# per-platform ldf_headers.h carries dependency-finder hints for the IDE.
ALLOWED_BASENAMES: frozenset[str] = frozenset(
    {"platformio.ini", "platformio.lock", "ldf_headers.h"}
)

# Binary and generated content is not scanned.
SKIP_SUFFIXES: tuple[str, ...] = (
    ".png",
    ".jpg",
    ".jpeg",
    ".gif",
    ".ico",
    ".webp",
    ".svg",
    ".pdf",
    ".zip",
    ".gz",
    ".zst",
    ".xz",
    ".bz2",
    ".tar",
    ".bin",
    ".elf",
    ".hex",
    ".map",
    ".wasm",
    ".ttf",
    ".otf",
    ".woff",
    ".woff2",
    ".mp3",
    ".wav",
    ".ogg",
    ".mp4",
    ".pyc",
    ".so",
    ".dll",
    ".dylib",
    ".a",
    ".o",
    ".lock",
    ".fetch",
)

MAX_FILE_BYTES = 4 * 1024 * 1024

_PLATFORMIO_RE = re.compile(r"platformio", re.IGNORECASE)
# The spellings that survive: fbuild's project-file name, the
# PLATFORMIO_SRC_DIR environment variable that fbuild itself honours, and the
# name of the external PlatformIO-Starter repository fed to project drift sync.
_PLATFORMIO_ALLOWED_TOKEN_RE = re.compile(
    r"platformio\.(?:ini|lock)\b|PLATFORMIO_SRC_DIR\b|PlatformIO-Starter\b",
    re.IGNORECASE,
)
_PIO_WORD_RE = re.compile(r"\bpio\b")
# `pio` spellings that name the RP2040 PIO peripheral rather than the tool:
# the SDK header and the autoresearch `--rp-pio-*` flags.
_PIO_ALLOWED_CONTEXT_RE = re.compile(r"hardware/pio\.h|rp-pio")


class Violation(NamedTuple):
    path: str  # repo-relative POSIX
    line_no: int
    word: str
    line: str


def _is_allowed_path(rel_path: str) -> bool:
    if rel_path in ALLOWED_PATHS:
        return True
    if Path(rel_path).name in ALLOWED_BASENAMES:
        return True
    return any(rel_path.startswith(prefix) for prefix in ALLOWED_PREFIXES)


def find_banned_words(text: str) -> list[tuple[int, str, str]]:
    """Return (line_no, word, line) for every banned occurrence in ``text``."""
    hits: list[tuple[int, str, str]] = []
    for line_no, line in enumerate(text.splitlines(), start=1):
        for m in _PLATFORMIO_RE.finditer(line):
            # Allowed only as part of the literal project-file name.
            file_match = _PLATFORMIO_ALLOWED_TOKEN_RE.match(line, m.start())
            if file_match is None:
                hits.append((line_no, m.group(0), line.rstrip()))
                break
        for m in _PIO_WORD_RE.finditer(line):
            context = line[max(0, m.start() - 9) : m.end() + 2]
            if _PIO_ALLOWED_CONTEXT_RE.search(context):
                continue
            hits.append((line_no, "pio", line.rstrip()))
            break
    return hits


def _tracked_and_untracked_files(root: Path) -> list[str]:
    """Repo-relative paths git knows about plus untracked, non-ignored ones."""
    try:
        from running_process import RunningProcess  # noqa: PLC0415 - lazy

        result = RunningProcess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            cwd=str(root),
            check=True,
            timeout=60,
        )
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import (  # noqa: PLC0415 - lazy
            handle_keyboard_interrupt,
        )

        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return _walk_files(root)
    files = [p.strip() for p in (result.stdout or "").splitlines() if p.strip()]
    # Deleted-but-still-indexed paths must not be read.
    return [p for p in files if (root / p).is_file()]


def _walk_files(root: Path) -> list[str]:
    """Fallback for a non-git checkout: walk the tree, pruning noise dirs."""
    skip = {".git", ".build", ".venv", ".cache", "node_modules", "__pycache__"}
    out: list[str] = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in skip]
        for name in filenames:
            rel = os.path.relpath(os.path.join(dirpath, name), root).replace("\\", "/")
            out.append(rel)
    return out


def scan(root: Path, files: Iterable[str] | None = None) -> list[Violation]:
    """Scan ``files`` (repo-relative to ``root``) and return every violation."""
    if files is None:
        files = _tracked_and_untracked_files(root)
    violations: list[Violation] = []
    for rel in sorted(files):
        rel = rel.replace("\\", "/")
        if _is_allowed_path(rel):
            continue
        if rel.lower().endswith(SKIP_SUFFIXES):
            continue
        path = root / rel
        try:
            if path.stat().st_size > MAX_FILE_BYTES:
                continue
            data = path.read_bytes()
        except OSError:
            continue
        if b"\0" in data[:8192]:
            continue  # binary
        text = data.decode("utf-8", "replace")
        for line_no, word, line in find_banned_words(text):
            violations.append(Violation(rel, line_no, word, line))
    return violations


def run_banned_build_tools_lint(root: Path = PROJECT_ROOT) -> bool:
    """Lint stage entry point. Returns True when the tree is clean."""
    violations = scan(root)
    if not violations:
        print("Banned build tools: no PlatformIO references in the tree")
        return True
    print(
        f"[banned-build-tools] {len(violations)} reference(s) to the PlatformIO "
        "tool found. fbuild is the only board build backend; rename, rewrite "
        "or delete these (the file name platformio.ini is the one allowed spelling):"
    )
    for v in violations:
        shown = v.line if len(v.line) <= 120 else v.line[:117] + "..."
        print(f"  {v.path}:{v.line_no}: [{v.word}] {shown}")
    print(
        "\nAllowed surfaces are listed in ci/lint/banned_build_tools.py "
        "(ALLOWED_PATHS / ALLOWED_PREFIXES)."
    )
    return False


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=PROJECT_ROOT)
    args = parser.parse_args(argv)
    return 0 if run_banned_build_tools_lint(args.root) else 1


if __name__ == "__main__":
    sys.exit(main())
