"""Checker that flags sys.path modification calls.

sys.path hacks are NEVER allowed in this project. They break type checkers
(pyright, ty), IDE code navigation, and make imports non-reproducible.

All Python scripts MUST be launched via 'uv run' which handles path resolution
automatically. If your imports don't resolve, fix the package structure — do NOT
add sys.path hacks.

Error Codes:
    SPI001: sys.path modification detected — use 'uv run' instead
"""

from __future__ import annotations

import argparse
import ast
import re
import sys
from pathlib import Path


# Regex pre-filter: quickly skip files with no sys.path usage
_SYS_PATH_RE = re.compile(r"sys\.path")

# Matches noqa: SPI001 suppression comment
_NOQA_RE = re.compile(r"#\s*noqa:\s*SPI001\b")


class SysPathVisitor(ast.NodeVisitor):
    """AST visitor that finds sys.path.insert() and sys.path.append() calls."""

    def __init__(self) -> None:
        self.violations: list[tuple[int, str]] = []

    def visit_Call(self, node: ast.Call) -> None:  # noqa: N802
        if (
            isinstance(node.func, ast.Attribute)
            and node.func.attr in ("insert", "append")
            and isinstance(node.func.value, ast.Attribute)
            and node.func.value.attr == "path"
            and isinstance(node.func.value.value, ast.Name)
            and node.func.value.value.id == "sys"
        ):
            self.violations.append(
                (
                    node.lineno,
                    "SPI001 sys.path modification is FORBIDDEN. "
                    "Use 'uv run' to launch scripts (handles path resolution automatically). "
                    "Do NOT use bare 'python' — always 'uv run python' or 'uv run <script>'. "
                    "Remove the sys.path hack and fix imports to use proper package paths.",
                )
            )
        self.generic_visit(node)


def check_file(path: str, source: str) -> list[tuple[int, str]]:
    """Parse source and return SPI001 violations not suppressed by noqa."""
    try:
        tree = ast.parse(source, filename=path)
    except SyntaxError:
        return []

    visitor = SysPathVisitor()
    visitor.visit(tree)

    # Silently filter out violations on lines with noqa SPI001 suppression
    lines = source.splitlines()
    result: list[tuple[int, str]] = []
    for line_no, message in visitor.violations:
        if line_no <= len(lines) and _NOQA_RE.search(lines[line_no - 1]):
            continue
        result.append((line_no, message))
    return result


def collect_python_files(
    paths: list[str],
    excludes: list[str],
) -> list[Path]:
    """Walk paths and return all .py files, filtering out excludes."""
    result: list[Path] = []
    exclude_parts = [e.replace("\\", "/").strip("/") for e in excludes]

    for p_str in paths:
        p = Path(p_str)
        if p.is_file() and p.suffix == ".py":
            if not _is_excluded(p, exclude_parts):
                result.append(p)
        elif p.is_dir():
            for py_file in p.rglob("*.py"):
                if not _is_excluded(py_file, exclude_parts):
                    result.append(py_file)
    return result


def _is_excluded(path: Path, exclude_parts: list[str]) -> bool:
    """Return True if any component of path matches an exclude pattern."""
    path_str = path.as_posix()
    for exc in exclude_parts:
        if exc in path_str:
            return True
    return False


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check Python files for sys.path modification calls.",
    )
    parser.add_argument(
        "paths",
        nargs="+",
        help="Files or directories to check",
    )
    parser.add_argument(
        "--exclude",
        nargs="*",
        default=[],
        help="Substrings to exclude from file paths",
    )
    args = parser.parse_args(argv)

    files = collect_python_files(args.paths, args.exclude)

    total_violations = 0
    for path in files:
        try:
            source = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if not _SYS_PATH_RE.search(source):
            continue
        violations = check_file(str(path), source)
        for line_no, message in violations:
            print(f"{path}:{line_no}: {message}")
            total_violations += 1

    return 1 if total_violations > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
