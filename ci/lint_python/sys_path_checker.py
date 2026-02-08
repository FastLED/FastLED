"""Checker that flags sys.path modification calls.

These calls break type checkers (pyright, ty), IDE code navigation, and
make imports non-reproducible. Prefer proper package structure instead.

Error Codes:
    SPI001: sys.path modification detected - breaks type checking and IDE support
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
                    "SPI001 sys.path modification detected - breaks type checking "
                    "and IDE support. Suppress with # noqa: SPI001 if unavoidable.",
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

    # Filter out violations on lines with noqa SPI001 suppression
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
