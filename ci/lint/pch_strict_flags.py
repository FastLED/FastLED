"""PCH builds keep strict validation flags; PCH timestamps are never disabled.

FastLED validates precompiled headers strictly (see the "Precompiled Header
Invariant" in agents/docs/testing-commands.md and the /pch-debug skill). A
stale or mismatched PCH must fail loudly, so a broken staleness guard gets
fixed instead of hidden. This lint scans the host build surface (``ci/``,
``.github/`` and every ``meson.build`` / ``meson.options``) and fails
``bash lint`` when:

  - ``-fno-pch-timestamp`` appears anywhere (it makes the PCH record mtime 0,
    so clang stops checking input timestamps; banned permanently);
  - a PCH **producer** (a command that compiles ``-x c++-header`` into a PCH)
    lacks any of ``-Werror=invalid-pch``, ``-verify-pch`` or
    ``-fpch-validate-input-files-content``;
  - a PCH **consumer** (a command passing ``-include-pch``) lacks
    ``-Werror=invalid-pch`` or ``-fpch-validate-input-files-content``.

A site is one bracketed argument list in a Meson file, or one function in a
Python file, so the flags must sit next to the PCH they guard. A Python
function is a producer only when it also mentions a PCH (``compile_pch``, a
``.pch`` path or a ``*pch*`` name), so IWYU's ``-x c++-header`` header
analysis is not one. Module-level flag-name tables and comparisons such as
``arg == "-include-pch"`` are not commands and are ignored.

Run directly: ``uv run python ci/lint/pch_strict_flags.py [--root DIR]``.
"""

from __future__ import annotations

import argparse
import ast
import re
import sys
from pathlib import Path
from typing import NamedTuple

from ci.lint.banned_build_tools import _tracked_and_untracked_files
from ci.lint.banned_linkers import is_in_scope


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

BANNED_FLAG = "-fno-pch-timestamp"
PRODUCER_FLAGS = (
    "-Werror=invalid-pch",
    "-verify-pch",
    "-fpch-validate-input-files-content",
)
CONSUMER_FLAGS = ("-Werror=invalid-pch", "-fpch-validate-input-files-content")

# Files whose -include-pch / c++-header strings are data, not commands. Each
# needs a reason; everything else in them is still scanned for the ban.
NOT_COMMANDS: dict[str, str] = {
    # Fixture argv for path-normalization tests; never executed as a compile.
    "ci/tests/test_meson_include_paths.py": "path-normalization fixture",
}

# Only the enforcement code and its tests may spell the banned flag.
BAN_EXEMPT = frozenset(
    {"ci/lint/pch_strict_flags.py", "ci/tests/test_pch_strict_flags.py"}
)


class Violation(NamedTuple):
    path: str
    line_no: int
    message: str


def _missing(kind: str, present: set[str], required: tuple[str, ...]) -> str | None:
    lacking = [f for f in required if f not in present]
    if not lacking:
        return None
    return f"PCH {kind} is missing {', '.join(lacking)}"


def _strip_meson_comment(line: str) -> str:
    """Drop a Meson ``#`` comment, keeping ``#`` inside '...' strings."""
    in_string = False
    for i, ch in enumerate(line):
        if ch == "\\" and in_string:
            continue
        if ch == "'" and (i == 0 or line[i - 1] != "\\"):
            in_string = not in_string
        elif ch == "#" and not in_string:
            return line[:i]
    return line


def check_meson(text: str) -> list[tuple[int, str]]:
    """Check every top-level bracketed list in a Meson file."""
    hits: list[tuple[int, str]] = []
    # Strip comments so a flag mentioned in a comment does not count.
    code = "\n".join(_strip_meson_comment(line) for line in text.splitlines())
    depth, start = 0, 0
    for i, ch in enumerate(code):
        if ch == "[":
            if depth == 0:
                start = i
            depth += 1
        elif ch == "]" and depth:
            depth -= 1
            if depth == 0:
                block = code[start : i + 1]
                tokens = set(re.findall(r"'([^']*)'", block))
                line_no = code.count("\n", 0, start) + 1
                if "c++-header" in tokens:
                    msg = _missing("producer", tokens, PRODUCER_FLAGS)
                    if msg:
                        hits.append((line_no, msg))
                if "-include-pch" in tokens:
                    msg = _missing("consumer", tokens, CONSUMER_FLAGS)
                    if msg:
                        hits.append((line_no, msg))
    return hits


def _is_command_element(node: ast.Constant, parents: dict[ast.AST, ast.AST]) -> bool:
    """True if the string is an element of a list/tuple literal (an argv), not a comparison."""
    return isinstance(parents.get(node), (ast.List, ast.Tuple))


def check_python(text: str) -> list[tuple[int, str]]:
    """Check every function in a Python file."""
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return []
    parents = {
        child: node for node in ast.walk(tree) for child in ast.iter_child_nodes(node)
    }
    hits: list[tuple[int, str]] = []
    # Module-level flag lists (``PCH_USE_FLAGS = ["-Werror=invalid-pch", ...]``)
    # count for every function that references them, so shared flag lists do
    # not have to be repeated at each call site.
    module_lists: dict[str, set[str]] = {}
    for stmt in tree.body:
        if isinstance(stmt, ast.Assign) and isinstance(
            stmt.value, (ast.List, ast.Tuple)
        ):
            values = {
                e.value
                for e in stmt.value.elts
                if isinstance(e, ast.Constant) and isinstance(e.value, str)
            }
            for target in stmt.targets:
                if isinstance(target, ast.Name):
                    module_lists[target.id] = values
    functions = [
        n
        for n in ast.walk(tree)
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))
    ]
    for fn in functions:
        # Only this function's own body, not nested functions (checked on their own).
        nested = {
            id(sub)
            for inner in ast.walk(fn)
            if inner is not fn
            and isinstance(inner, (ast.FunctionDef, ast.AsyncFunctionDef))
            for sub in ast.walk(inner)
        }
        nodes = [n for n in ast.walk(fn) if id(n) not in nested]
        strings = {
            n.value
            for n in nodes
            if isinstance(n, ast.Constant) and isinstance(n.value, str)
        }
        argv = {
            n.value
            for n in nodes
            if isinstance(n, ast.Constant)
            and isinstance(n.value, str)
            and _is_command_element(n, parents)
        }
        names = {n.id for n in nodes if isinstance(n, ast.Name)} | {
            n.attr for n in nodes if isinstance(n, ast.Attribute)
        }
        for name in names & module_lists.keys():
            strings |= module_lists[name]
            argv |= module_lists[name]
        mentions_pch = any(
            "compile_pch" in s or s.endswith(".pch") for s in strings
        ) or any("pch" in name.lower() for name in names)
        if "c++-header" in argv and mentions_pch:
            msg = _missing("producer", strings, PRODUCER_FLAGS)
            if msg:
                hits.append((fn.lineno, f"{msg} (in {fn.name}())"))
        if "-include-pch" in argv:
            msg = _missing("consumer", strings, CONSUMER_FLAGS)
            if msg:
                hits.append((fn.lineno, f"{msg} (in {fn.name}())"))
    return hits


def scan(root: Path = PROJECT_ROOT) -> list[Violation]:
    violations: list[Violation] = []
    for rel_path in _tracked_and_untracked_files(root):
        rel_path = rel_path.replace("\\", "/")
        if not is_in_scope(rel_path):
            continue
        try:
            text = (root / rel_path).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if rel_path not in BAN_EXEMPT:
            for line_no, line in enumerate(text.splitlines(), start=1):
                if BANNED_FLAG in line:
                    violations.append(
                        Violation(
                            rel_path,
                            line_no,
                            f"{BANNED_FLAG} is banned: it stops clang checking PCH input "
                            "timestamps and masks stale-PCH bugs",
                        )
                    )
        name = Path(rel_path).name
        if name in ("meson.build", "meson.options"):
            hits = check_meson(text)
        elif (
            rel_path.endswith(".py")
            and rel_path not in BAN_EXEMPT
            and rel_path not in NOT_COMMANDS
        ):
            hits = check_python(text)
        else:
            hits = []
        violations += [Violation(rel_path, line_no, msg) for line_no, msg in hits]
    return violations


def run_pch_strict_flags_lint(root: Path = PROJECT_ROOT) -> bool:
    """Return True when every PCH site keeps strict validation."""
    violations = scan(root)
    if not violations:
        print("✅ PCH strict flags: every PCH producer/consumer validates strictly")
        return True
    print(
        f"❌ PCH strict flags: {len(violations)} violation(s). PCH builds must keep "
        "strict validation; fix the staleness guard instead of relaxing it "
        "(see the /pch-debug skill)."
    )
    for v in violations:
        print(f"  {v.path}:{v.line_no}: {v.message}")
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=PROJECT_ROOT)
    args = parser.parse_args()
    return 0 if run_pch_strict_flags_lint(args.root) else 1


if __name__ == "__main__":
    sys.exit(main())
