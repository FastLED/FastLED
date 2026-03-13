"""Checker that flags complex dict type annotations and suggests @dataclass(slots=True).

Complex dict types like `dict[str, str | int | float]` are hard to read and
lack semantic meaning. Named @dataclass(slots=True) types are self-documenting,
provide attribute access, are validated by type checkers, and have faster
attribute access with lower memory usage than regular dataclasses.

Error Codes:
    DCT001: Complex dict type annotation — use a named @dataclass(slots=True) instead
"""

from __future__ import annotations

import argparse
import ast
import re
import sys
from pathlib import Path


# Regex pre-filter: quickly skip files with no dict type hints
_DICT_TYPE_RE = re.compile(r"dict\s*\[")

# Matches noqa: DCT001 suppression comment
_NOQA_RE = re.compile(r"#\s*noqa:\s*DCT001\b")

# Minimum union size to flag: dict[str, X | Y | Z] where the value type
# has >= this many union members is considered "complex"
_MIN_UNION_MEMBERS = 3


def _count_union_members(node: ast.expr) -> int:
    """Count the number of types in a union (X | Y | Z -> 3)."""
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.BitOr):
        return _count_union_members(node.left) + _count_union_members(node.right)
    return 1


def _is_complex_dict(node: ast.expr) -> bool:
    """Check if a type annotation is a complex dict type.

    Flags dict[K, V] where V is a union of >= _MIN_UNION_MEMBERS types.
    Also flags dict[K, V] where K is a union of >= _MIN_UNION_MEMBERS types.
    """
    # Match dict[K, V] as ast.Subscript
    if not isinstance(node, ast.Subscript):
        return False

    # Check the base is "dict"
    if isinstance(node.value, ast.Name) and node.value.id == "dict":
        pass
    elif isinstance(node.value, ast.Attribute) and node.value.attr == "Dict":
        pass  # typing.Dict
    else:
        return False

    # Extract the slice (K, V)
    slc = node.slice
    if not isinstance(slc, ast.Tuple) or len(slc.elts) != 2:
        return False

    key_type, value_type = slc.elts

    # Check if either key or value has >= _MIN_UNION_MEMBERS union members
    if _count_union_members(value_type) >= _MIN_UNION_MEMBERS:
        return True
    if _count_union_members(key_type) >= _MIN_UNION_MEMBERS:
        return True

    return False


class DictTypeVisitor(ast.NodeVisitor):
    """AST visitor that finds complex dict type annotations."""

    def __init__(self) -> None:
        self.violations: list[tuple[int, str]] = []

    def _check_annotation(self, node: ast.expr) -> None:
        if _is_complex_dict(node):
            self.violations.append(
                (
                    node.lineno,
                    "DCT001 Complex dict type annotation detected. "
                    "Use a named @dataclass(slots=True) instead of dict[K, V1 | V2 | V3] "
                    "for better readability, type safety, and performance. "
                    "Suppress with: # noqa: DCT001",
                )
            )

    def visit_AnnAssign(self, node: ast.AnnAssign) -> None:  # noqa: N802
        if node.annotation:
            self._check_annotation(node.annotation)
        self.generic_visit(node)

    def visit_FunctionDef(self, node: ast.FunctionDef) -> None:  # noqa: N802
        self._check_function(node)

    def visit_AsyncFunctionDef(self, node: ast.AsyncFunctionDef) -> None:  # noqa: N802
        self._check_function(node)

    def _check_function(self, node: ast.FunctionDef | ast.AsyncFunctionDef) -> None:
        # Check return annotation
        if node.returns:
            self._check_annotation(node.returns)
        # Check argument annotations
        for arg in node.args.args + node.args.kwonlyargs:
            if arg.annotation:
                self._check_annotation(arg.annotation)
        if node.args.vararg and node.args.vararg.annotation:
            self._check_annotation(node.args.vararg.annotation)
        if node.args.kwarg and node.args.kwarg.annotation:
            self._check_annotation(node.args.kwarg.annotation)
        self.generic_visit(node)


def check_file(path: str, source: str) -> list[tuple[int, str]]:
    """Parse source and return DCT001 violations not suppressed by noqa."""
    try:
        tree = ast.parse(source, filename=path)
    except SyntaxError:
        return []

    visitor = DictTypeVisitor()
    visitor.visit(tree)

    # Filter out violations on lines with noqa DCT001 suppression
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
        description="Check Python files for complex dict type annotations.",
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
        if not _DICT_TYPE_RE.search(source):
            continue
        violations = check_file(str(path), source)
        for line_no, message in violations:
            print(f"{path}:{line_no}: {message}")
            total_violations += 1

    return 1 if total_violations > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
