"""reld is the only linker for FastLED host builds; every other one is banned.

FastLED's native test/example/profile builds link through reld
(https://github.com/zackees/reld) on Linux, macOS and Windows -- see
``ci/tools/reld.py``. This lint keeps that single path: it scans the host
build surface (``ci/``, ``.github/`` and every ``meson.build`` /
``meson.options`` outside ``src/``) and fails ``bash lint`` on

  - any ``-fuse-ld=`` or ``--ld-path=`` outside the one reld wiring point,
  - any other linker named as a tool: ``lld``, ``ld.lld``, ``ld64.lld``,
    ``lld-link``, ``rust-lld``, ``mold``, ``ld.gold``, ``ld.bfd``,
    ``wild-linker``.

Out of scope, by design: board (MCU) links, which fbuild owns with each
vendor's GCC ``ld``, and WebAssembly, which only Emscripten's ``wasm-ld``
can produce; both live under ``src/`` and fbuild, which are not scanned.
``%lld`` printf specifiers are not linker names and do not match.

Allowed files are listed in ``ALLOWED_PATHS`` with the reason each needs to
name a linker. There is no warn-only mode.

Run directly: ``uv run python ci/lint/banned_linkers.py [--root DIR]``.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import NamedTuple

from ci.lint.banned_build_tools import _tracked_and_untracked_files


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

# Files that may name specific linker tokens, and exactly which ones. Every
# other token in these files is still checked, so e.g. `-fuse-ld=mold` added
# next to the reld wiring fails. ``None`` exempts the whole file (only the
# enforcement code, which must spell every banned name).
ALLOWED_TOKENS: dict[str, frozenset[str] | None] = {
    # The reld wiring point (`-fuse-ld=lld --ld-path=<reld>`); comments there
    # describe ld64.lld / lld flag compatibility of reld's bridge.
    "ci/meson/native/meson.build": frozenset(
        {"-fuse-ld=lld", "--ld-path=", "ld64.lld", "lld"}
    ),
    # Provisions reld and names the clang-tool-chain linker it bridges to.
    "ci/tools/reld.py": frozenset({"ld.lld", "ld64.lld", "lld", "rust-lld"}),
    # Parse linker *diagnostics* ("ld.lld: error: ..."), which reld's bridge
    # still emits; they never choose a linker.
    "ci/meson/link_retry.py": frozenset({"ld.lld"}),
    "ci/meson/compile.py": frozenset({"ld.lld"}),
    "ci/meson/streaming.py": frozenset({"ld.lld", "lld"}),
    "ci/meson/streaming_runner.py": frozenset({"ld.lld"}),
    "ci/meson/zccache_retry.py": frozenset({"ld.lld"}),
    "ci/tests/test_link_retry.py": frozenset({"ld.lld"}),
    # Board (fbuild) toolchain tool-name table for binary inspection.
    "ci/compiler/build_config.py": frozenset({"ld.lld"}),
    "ci/tests/test_build_info_from_fbuild.py": frozenset({"ld.lld"}),
    # Enforcement: this lint and its tests must spell every banned name.
    "ci/lint/banned_linkers.py": None,
    "ci/tests/test_banned_linkers.py": None,
}
ALLOWED_PATHS: frozenset[str] = frozenset(ALLOWED_TOKENS)

_SCANNED_SUFFIXES = (".py", ".yml", ".yaml", ".toml", ".sh", ".txt", ".ini", ".json")
_SCANNED_NAMES = frozenset({"meson.build", "meson.options", "meson_options.txt"})

_LINKER_RE = re.compile(
    # Linker-selection flags, captured with their value.
    r"-fuse-ld=[\w.+-]*|--ld-path=?"
    # Linkers named as tools.
    r"|(?<![\w%.-])(?:lld|ld\.lld|ld64\.lld|lld-link|rust-lld|mold|ld\.mold"
    r"|ld\.gold|ld\.bfd)(?![\w-])"
    # Toolchain launchers that run a linker directly (clang-tool-chain-ld,
    # ctc-ld.lld, ...), whose linker name is embedded in the command.
    r"|\b(?:clang-tool-chain|ctc)-(?:ld64\.lld|ld\.lld|lld-link|lld|ld)(?![\w.-])"
    # Release archives are named wild-linker-<version>-<triple>.
    r"|\bwild-linker\b"
)


class Violation(NamedTuple):
    path: str
    line_no: int
    token: str
    line: str


def is_in_scope(rel_path: str) -> bool:
    """Host build surface: ci/, .github/ and Meson files outside src/."""
    if rel_path.startswith("src/") or rel_path.endswith(".md"):
        return False
    name = Path(rel_path).name
    if name in _SCANNED_NAMES:
        return True
    if not (rel_path.startswith("ci/") or rel_path.startswith(".github/")):
        return False
    if "/target/" in rel_path or "/node_modules/" in rel_path:
        return False
    return rel_path.endswith(_SCANNED_SUFFIXES) or name in _SCANNED_NAMES


def find_banned_linkers(
    text: str, allowed: frozenset[str] = frozenset()
) -> list[tuple[int, str, str]]:
    """Return (line_no, token, line) for every non-reld linker token.

    Every match on every line is reported, except tokens in ``allowed``.
    """
    hits: list[tuple[int, str, str]] = []
    for line_no, line in enumerate(text.splitlines(), start=1):
        for match in _LINKER_RE.finditer(line):
            if match.group(0) not in allowed:
                hits.append((line_no, match.group(0), line.rstrip()))
    return hits


def scan(root: Path = PROJECT_ROOT) -> list[Violation]:
    violations: list[Violation] = []
    for rel_path in _tracked_and_untracked_files(root):
        rel_path = rel_path.replace("\\", "/")
        if not is_in_scope(rel_path):
            continue
        allowed = ALLOWED_TOKENS.get(rel_path, frozenset())
        if allowed is None:
            continue
        try:
            text = (root / rel_path).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for line_no, token, line in find_banned_linkers(text, allowed):
            violations.append(Violation(rel_path, line_no, token, line))
    return violations


def run_banned_linkers_lint(root: Path = PROJECT_ROOT) -> bool:
    """Return True when no host build surface names a linker other than reld."""
    violations = scan(root)
    if not violations:
        print("✅ Banned linkers: reld is the only host linker")
        return True
    print(
        f"❌ Banned linkers: {len(violations)} non-reld linker reference(s). "
        "Host builds link only through reld (ci/tools/reld.py); remove the "
        "selection, or add the exact token for that file to ALLOWED_TOKENS in "
        "ci/lint/banned_linkers.py with a reason."
    )
    for v in violations:
        print(f"  {v.path}:{v.line_no}: '{v.token}': {v.line.strip()[:160]}")
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=PROJECT_ROOT)
    args = parser.parse_args()
    return 0 if run_banned_linkers_lint(args.root) else 1


if __name__ == "__main__":
    sys.exit(main())
