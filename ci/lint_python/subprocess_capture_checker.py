"""Checker that bans the stdlib `subprocess` API in favor of `RunningProcess`.

Policy: **no stdlib subprocess anywhere.** `RunningProcess` (from the
`running_process` package) is the one sanctioned way to spawn a child. It
drains stdout and stderr concurrently through an atomic queue, so output stays
observable while the process runs and neither pipe can wedge the child.

Why the stdlib API is not good enough
-------------------------------------
When a child is spawned with both ``stdout=PIPE`` and ``stderr=PIPE`` and the
parent drains only one of them, the child blocks as soon as the un-drained
pipe fills its OS buffer -- roughly 64 KB on Linux, ~8 KB on Windows -- and the
parent then waits forever for a child that can never finish. Measured in this
repository: a child writing 20k lines to stderr while the parent read only
stdout never returned, while the identical child under ``RunningProcess``
completed in 0.3s. A verbose build easily clears 64 KB of stderr, which is why
this reads as "fine locally, hangs in CI".

``subprocess.run`` avoids that specific deadlock (it calls
``Popen.communicate()`` internally), but it still accumulates all output
in memory and hands it back only at exit, so a long build shows nothing until
it finishes -- and a hung child is indistinguishable from a slow one.

This is a hard ban, not a ratchet: there is no baseline and no ``# noqa``
escape. The migration finished in September 2026; every call site in the
tree uses ``RunningProcess`` and this checker keeps it that way.

Error codes
-----------
SRC001
    ``subprocess.run(...)``. Use ``RunningProcess.run()`` -- a drop-in
    replacement that streams instead of accumulating.
SRC002
    ``subprocess.Popen(...)``. The deadlock-prone API; see above. Use
    ``RunningProcess(...)`` (``get_next_line`` / ``wait`` / ``kill``) or
    ``running_process.launch_detached`` for daemons.
SRC003
    ``subprocess.check_output`` / ``check_call`` / ``call`` /
    ``getoutput`` / ``getstatusoutput``.
SRC004
    A capturing call in text mode with no explicit ``encoding=``. Python then
    decodes with the locale codec -- cp1252 on Windows -- so any non-ASCII
    child output either mojibakes or raises ``UnicodeDecodeError`` inside the
    reader thread. Applies to ``RunningProcess`` too: pass
    ``encoding="utf-8"`` (usually with ``errors="replace"``) whenever
    ``capture_output=True`` / ``capture=True`` / ``stdout=PIPE`` is used in
    text mode.
SRC006
    ``os.system`` / ``os.popen``. Worse than subprocess: ``os.system`` returns
    no handle at all and ``os.popen`` gives a single undrainable pipe, so
    neither can be bounded, interrupted, or drained.
SRC007
    ``import subprocess``, ``from subprocess import ...`` or any other
    reference to the module (``subprocess.PIPE``, ``subprocess.TimeoutExpired``
    ...). Everything the stdlib module exports that a caller still needs --
    ``PIPE``, ``DEVNULL``, ``STDOUT``, ``CompletedProcess``,
    ``CalledProcessError``, ``TimeoutExpired``, ``CREATE_NEW_PROCESS_GROUP`` --
    is re-exported by ``running_process``.

Run directly::

    uv run python ci/lint_python/subprocess_capture_checker.py . [--exclude ci/tmp]
"""

from __future__ import annotations

import argparse
import ast
import re
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

# Directories that are never repository source.
DEFAULT_EXCLUDES = (
    ".git",
    ".venv",
    ".build",
    ".cache",
    "node_modules",
    "__pycache__",
    "ci/tmp",
    "third_party",
)

# Regex pre-filter: quickly skip files that cannot contain a finding.
_PREFILTER_RE = re.compile(r"subprocess|\b(?:system|popen)\s*\(|RunningProcess")

_MESSAGES = {
    "SRC001": (
        "SRC001 subprocess.run(...). Use `RunningProcess.run()` from "
        "`running_process` -- a drop-in replacement that streams instead of "
        "accumulating."
    ),
    "SRC002": (
        "SRC002 subprocess.Popen(...) can deadlock: a child writing past the pipe "
        "buffer blocks while the parent waits, and the parent waits because the "
        "child never exits. Use `RunningProcess`, which drains concurrently."
    ),
    "SRC003": (
        "SRC003 subprocess.check_output/check_call/call/getoutput(...). Use "
        "`RunningProcess.run()` from `running_process`."
    ),
    "SRC004": (
        "SRC004 capturing call in text mode without an explicit `encoding=`. "
        "Python falls back to the locale codec (cp1252 on Windows), which "
        'mojibakes or crashes on non-ASCII output. Pass encoding="utf-8" '
        '(usually with errors="replace").'
    ),
    "SRC006": (
        "SRC006 os.system/os.popen shells out with no way to drain or bound "
        "the child. Use `RunningProcess`."
    ),
    "SRC007": (
        "SRC007 the stdlib `subprocess` module is banned; import "
        "`RunningProcess` (and PIPE/DEVNULL/STDOUT/CalledProcessError/"
        "TimeoutExpired/CompletedProcess) from `running_process` instead."
    ),
}

_SUBPROCESS_CALL_CODES = {
    "run": "SRC001",
    "Popen": "SRC002",
    "check_output": "SRC003",
    "check_call": "SRC003",
    "call": "SRC003",
    "getoutput": "SRC003",
    "getstatusoutput": "SRC003",
}


class SubprocessVisitor(ast.NodeVisitor):
    """AST visitor that finds banned process-spawning APIs."""

    def __init__(self, source_lines: list[str] | None = None) -> None:
        self.violations: list[tuple[int, str, str]] = []
        self.source_lines = source_lines or []
        # Lines already reported for SRC007 so a `subprocess.run(...)` call
        # yields SRC001 plus one SRC007, not two.
        self._module_ref_lines: set[int] = set()
        # Names bound to the banned modules in this file, so `import
        # subprocess as sp` / `import os as o` are seen through.
        self._subprocess_names: set[str] = {"subprocess"}
        self._os_names: set[str] = {"os"}
        # Bare names bound to os.system / os.popen by `from os import ...`.
        self._os_spawn_names: set[str] = set()

    # -- imports ----------------------------------------------------------

    def visit_Import(self, node: ast.Import) -> None:  # noqa: N802
        for alias in node.names:
            if alias.name == "subprocess" or alias.name.startswith("subprocess."):
                self._add_module_ref(node.lineno)
                self._subprocess_names.add(alias.asname or alias.name.split(".")[0])
            elif alias.name == "os":
                self._os_names.add(alias.asname or "os")
        self.generic_visit(node)

    def visit_ImportFrom(self, node: ast.ImportFrom) -> None:  # noqa: N802
        module = node.module or ""
        if module == "subprocess" or module.startswith("subprocess."):
            self._add_module_ref(node.lineno)
        elif module == "os":
            spawn_names = [a for a in node.names if a.name in ("system", "popen")]
            if spawn_names:
                self._add(node.lineno, "SRC006")  # once per import line
                for alias in spawn_names:
                    self._os_spawn_names.add(alias.asname or alias.name)
        self.generic_visit(node)

    # -- attribute references (subprocess.PIPE, subprocess.TimeoutExpired) --

    def visit_Attribute(self, node: ast.Attribute) -> None:  # noqa: N802
        if isinstance(node.value, ast.Name) and node.value.id in self._subprocess_names:
            self._add_module_ref(node.lineno)
        self.generic_visit(node)

    # -- calls -------------------------------------------------------------

    def visit_Call(self, node: ast.Call) -> None:  # noqa: N802
        if self._is_os_spawn(node):
            self._add(node.lineno, "SRC006")
            self.generic_visit(node)
            return

        attr = self._subprocess_attr(node)
        if attr is not None:
            code = _SUBPROCESS_CALL_CODES.get(attr)
            if code is not None:
                self._add(node.lineno, code)
            if self._captures_output(node) and self._is_stdlib_text_mode(node):
                if not self._has_kwarg(node, "encoding"):
                    self._add(node.lineno, "SRC004")
            self.generic_visit(node)
            return

        # RunningProcess.run(...) / RunningProcess(...): text mode is the
        # default, so a capture without encoding= is the same locale hazard.
        if self._is_running_process_call(node) and self._captures_output(node):
            if not self._is_explicit_bytes(node) and not self._has_kwarg(
                node, "encoding"
            ):
                self._add(node.lineno, "SRC004")

        self.generic_visit(node)

    # -- helpers -------------------------------------------------------------

    def _add(self, lineno: int, code: str) -> None:
        self.violations.append((lineno, code, _MESSAGES[code]))

    def _add_module_ref(self, lineno: int) -> None:
        if lineno in self._module_ref_lines:
            return
        self._module_ref_lines.add(lineno)
        self._add(lineno, "SRC007")

    def _is_os_spawn(self, node: ast.Call) -> bool:
        """True for `os.system(...)` / `os.popen(...)`."""
        func = node.func
        if isinstance(func, ast.Name):
            return func.id in self._os_spawn_names
        if not isinstance(func, ast.Attribute):
            return False
        if not (isinstance(func.value, ast.Name) and func.value.id in self._os_names):
            return False
        return func.attr in ("system", "popen")

    def _subprocess_attr(self, node: ast.Call) -> str | None:
        """Return the attribute name for a `subprocess.<attr>(...)` call."""
        func = node.func
        if not isinstance(func, ast.Attribute):
            return None
        if not (
            isinstance(func.value, ast.Name) and func.value.id in self._subprocess_names
        ):
            return None
        return func.attr

    def _is_running_process_call(self, node: ast.Call) -> bool:
        """True for `RunningProcess(...)` and `RunningProcess.run(...)`."""
        func = node.func
        if isinstance(func, ast.Name):
            return func.id == "RunningProcess"
        if isinstance(func, ast.Attribute) and func.attr == "run":
            return (
                isinstance(func.value, ast.Name) and func.value.id == "RunningProcess"
            )
        return False

    def _captures_output(self, node: ast.Call) -> bool:
        """True when the call captures stdout/stderr through a pipe."""
        for kw in node.keywords:
            if kw.arg in ("capture_output", "capture") and self._is_true(kw.value):
                return True
            if kw.arg in ("stdout", "stderr") and self._is_pipe(kw.value):
                return True
        return False

    def _is_stdlib_text_mode(self, node: ast.Call) -> bool:
        """stdlib calls decode only when asked (text=True / errors=)."""
        for kw in node.keywords:
            if kw.arg in ("text", "universal_newlines") and self._is_true(kw.value):
                return True
            if kw.arg == "errors":
                return True
        return False

    def _is_explicit_bytes(self, node: ast.Call) -> bool:
        """RunningProcess defaults to text; only text=False opts out."""
        for kw in node.keywords:
            if kw.arg == "text" and isinstance(kw.value, ast.Constant):
                return kw.value.value is False
        return False

    def _has_kwarg(self, node: ast.Call, name: str) -> bool:
        return any(kw.arg == name for kw in node.keywords)

    def _is_true(self, node: ast.expr) -> bool:
        return isinstance(node, ast.Constant) and node.value is True

    def _is_pipe(self, node: ast.expr) -> bool:
        """`PIPE`, `subprocess.PIPE` or `running_process.PIPE`."""
        if isinstance(node, ast.Name):
            return node.id == "PIPE"
        return isinstance(node, ast.Attribute) and node.attr == "PIPE"


def check_file(path: str, source: str) -> list[tuple[int, str, str]]:
    """Parse source and return all violations as (lineno, code, message)."""
    try:
        tree = ast.parse(source, filename=path)
    except SyntaxError:
        return []

    visitor = SubprocessVisitor(source.split("\n"))
    visitor.visit(tree)
    return sorted(visitor.violations)


def _rel_path(path: Path) -> str:
    """Project-relative posix path, or the path itself if outside the tree."""
    try:
        return path.resolve().relative_to(PROJECT_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def collect_python_files(paths: list[str], excludes: list[str]) -> list[Path]:
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
    return sorted(set(result))


def _is_excluded(path: Path, exclude_parts: list[str]) -> bool:
    """True if any path component sequence matches an exclude pattern."""
    parts = path.as_posix()
    padded = f"/{parts}/"
    return any(f"/{exc}/" in padded for exc in exclude_parts)


def scan(paths: list[str], excludes: list[str]) -> list[str]:
    """Return every finding as `path:line: message`."""
    findings: list[str] = []
    for path in collect_python_files(paths, excludes):
        try:
            source = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if not _PREFILTER_RE.search(source):
            continue
        for line_no, _code, message in check_file(str(path), source):
            findings.append(f"{_rel_path(path)}:{line_no}: {message}")
    return findings


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Ban the stdlib subprocess module in favor of RunningProcess.",
    )
    parser.add_argument("paths", nargs="+", help="Files or directories to check")
    parser.add_argument(
        "--exclude",
        nargs="*",
        default=[],
        help="Extra path components to exclude (in addition to the defaults)",
    )
    args = parser.parse_args(argv)

    findings = scan(args.paths, [*DEFAULT_EXCLUDES, *args.exclude])
    if not findings:
        print("No stdlib subprocess usage found.")
        return 0

    for line in findings:
        print(line)
    print(
        f"\n{len(findings)} banned process-spawning usage(s). The stdlib "
        "`subprocess` module, `os.system` and `os.popen` are not allowed; use "
        "`RunningProcess` from `running_process` (see "
        "ci/lint_python/subprocess_capture_checker.py)."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
