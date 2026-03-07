"""Checker that flags subprocess.run with capture_output=True or stdout/stderr=PIPE.

Capturing output with subprocess.run can cause deadlocks when the output buffer
fills up. Always use `RunningProcess.run()` from `running_process` instead,
which provides a drop-in replacement with automatic pipe buffer protection.

Error Codes:
    SRC001: subprocess.run with capture — use RunningProcess.run() instead
"""

from __future__ import annotations

import argparse
import ast
import re
import sys
from pathlib import Path


# Regex pre-filter: quickly skip files with no subprocess usage
_SUBPROCESS_RE = re.compile(r"subprocess\.run")

_VIOLATION_MSG_CAPTURE = (
    "SRC001 subprocess.run with capture_output=True can deadlock "
    "when the output buffer fills. Use `RunningProcess.run()` from "
    "`running_process` instead (drop-in replacement with pipe buffer protection)."
)

_VIOLATION_MSG_PIPE = (
    "SRC001 subprocess.run with stdout/stderr=PIPE can deadlock "
    "when the output buffer fills. Use `RunningProcess.run()` from "
    "`running_process` instead (drop-in replacement with pipe buffer protection)."
)


class SubprocessCaptureVisitor(ast.NodeVisitor):
    """AST visitor that finds subprocess.run() calls with output capture."""

    def __init__(self, source_lines: list[str] | None = None) -> None:
        self.violations: list[tuple[int, str]] = []
        self.source_lines = source_lines or []

    def visit_Call(self, node: ast.Call) -> None:  # noqa: N802
        if not self._is_subprocess_run(node):
            self.generic_visit(node)
            return

        # Skip if this call uses input= (stdin piping is not a deadlock risk)
        if any(kw.arg == "input" for kw in node.keywords):
            self.generic_visit(node)
            return

        # Skip if this line has a noqa comment
        if self._has_noqa(node.lineno):
            self.generic_visit(node)
            return

        # Check keyword arguments for capture patterns
        for kw in node.keywords:
            if kw.arg == "capture_output" and self._is_true(kw.value):
                self.violations.append((node.lineno, _VIOLATION_MSG_CAPTURE))
                break
            if kw.arg in ("stdout", "stderr") and self._is_pipe(kw.value):
                self.violations.append((node.lineno, _VIOLATION_MSG_PIPE))
                break

        self.generic_visit(node)

    def _is_subprocess_run(self, node: ast.Call) -> bool:
        """Check if the call is subprocess.run(...)."""
        if isinstance(node.func, ast.Attribute) and node.func.attr == "run":
            if (
                isinstance(node.func.value, ast.Name)
                and node.func.value.id == "subprocess"
            ):
                return True
        return False

    def _is_true(self, node: ast.expr) -> bool:
        """Check if node is the literal True."""
        return isinstance(node, ast.Constant) and node.value is True

    def _is_pipe(self, node: ast.expr) -> bool:
        """Check if node is subprocess.PIPE."""
        if isinstance(node, ast.Attribute) and node.attr == "PIPE":
            if isinstance(node.value, ast.Name) and node.value.id == "subprocess":
                return True
        return False

    def _has_noqa(self, lineno: int) -> bool:
        """Check if a line has a noqa comment."""
        if not self.source_lines or lineno < 1 or lineno > len(self.source_lines):
            return False
        line = self.source_lines[lineno - 1]
        return "noqa: SRC001" in line or "# noqa" in line


def check_file(path: str, source: str) -> list[tuple[int, str]]:
    """Parse source and return all SRC001 violations."""
    try:
        tree = ast.parse(source, filename=path)
    except SyntaxError:
        return []

    source_lines = source.split("\n")
    visitor = SubprocessCaptureVisitor(source_lines)
    visitor.visit(tree)
    return visitor.violations


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
        description="Check Python files for subprocess.run with capture_output/PIPE.",
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
        if not _SUBPROCESS_RE.search(source):
            continue
        violations = check_file(str(path), source)
        for line_no, message in violations:
            print(f"{path}:{line_no}: {message}")
            total_violations += 1

    return 1 if total_violations > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
