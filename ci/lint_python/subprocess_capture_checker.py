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

Error codes
-----------
SRC001
    ``subprocess.run(...)``. Use ``RunningProcess.run()`` -- a drop-in
    replacement that streams instead of accumulating.

SRC002
    ``subprocess.Popen(...)``. The deadlock-prone API; see above.

SRC003
    ``subprocess.check_output`` / ``check_call`` / ``call``.

SRC004
    A capturing call in text mode with no explicit ``encoding=``. Python then
    decodes with the locale codec -- cp1252 on Windows -- so any non-ASCII
    child output either mojibakes or raises ``UnicodeDecodeError`` inside the
    reader thread. In ``subprocess.run`` that surfaces as ``stdout is None``
    and a downstream ``TypeError``, which is a confusing way to learn the
    output was never readable. Pass ``encoding="utf-8"`` (usually with
    ``errors="replace"``).

SRC005
    ``Popen(stdout=PIPE, stderr=PIPE)`` with no ``communicate()`` -- the exact
    shape that hangs. Unlike the other codes this is held at **zero** rather
    than ratcheted: it is a live deadlock, not a style preference. The
    baseline deliberately contains no SRC005 entries, so a new one fails the
    build immediately.

SRC006
    ``os.system`` / ``os.popen``. Worse than subprocess: ``os.system`` returns
    no handle at all and ``os.popen`` gives a single undrainable pipe, so
    neither can be bounded, interrupted, or drained.

Baseline
--------
The repository predates this policy, so a per-file/per-code baseline of known
violations is stored alongside this module. Counts may shrink freely; any
increase fails. That makes the ban a ratchet: existing call sites migrate to
``RunningProcess`` over time and no new ones can appear. Regenerate after an
intentional migration with::

    uv run python ci/lint_python/subprocess_capture_checker.py ci test.py \
        tests build.py mcp_server.py --exclude ci/tmp --update-baseline
"""

from __future__ import annotations

import argparse
import ast
import re
import sys
from collections import Counter
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_BASELINE = Path(__file__).resolve().parent / "subprocess_baseline.txt"

# Regex pre-filter: quickly skip files with no subprocess usage at all.
_SUBPROCESS_RE = re.compile(r"subprocess\s*\.|os\s*\.\s*(?:system|popen)\s*\(")

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
        "SRC003 subprocess.check_output/check_call/call(...). Use "
        "`RunningProcess.run()` from `running_process`."
    ),
    "SRC005": (
        "SRC005 subprocess.Popen(stdout=PIPE, stderr=PIPE) with no "
        "communicate() DEADLOCKS once either pipe fills. Use "
        "`RunningProcess` (it drains both streams concurrently), or "
        "`stderr=subprocess.STDOUT` to merge into one pipe."
    ),
    "SRC006": (
        "SRC006 os.system/os.popen shells out with no way to drain or bound "
        "the child. Use `RunningProcess`."
    ),
    "SRC004": (
        "SRC004 capturing subprocess call in text mode without an explicit "
        "`encoding=`. Python falls back to the locale codec (cp1252 on Windows), "
        'which mojibakes or crashes on non-ASCII output. Pass encoding="utf-8" '
        '(usually with errors="replace").'
    ),
}


class SubprocessVisitor(ast.NodeVisitor):
    """AST visitor that finds discouraged stdlib subprocess usage."""

    def __init__(self, source_lines: list[str] | None = None) -> None:
        self.violations: list[tuple[int, str, str]] = []
        self.source_lines = source_lines or []

    def visit_Call(self, node: ast.Call) -> None:  # noqa: N802
        # `os.system` / `os.popen` are spawn APIs too, and worse than
        # subprocess: os.system gives no handle at all, os.popen gives one
        # undrainable pipe. Neither can be bounded or interrupted.
        if self._is_os_spawn(node) and not self._has_noqa(node.lineno):
            self._add(node.lineno, "SRC006")
            self.generic_visit(node)
            return

        attr = self._subprocess_attr(node)
        if attr is None:
            self.generic_visit(node)
            return

        if self._has_noqa(node.lineno):
            self.generic_visit(node)
            return

        captures = self._captures_output(node)

        if attr == "run":
            self._add(node.lineno, "SRC001")
        elif attr == "Popen":
            self._add(node.lineno, "SRC002")
            # SRC005 is the subset of SRC002 that actually deadlocks. Reported
            # in addition to SRC002 so the broad convention rule can stay on
            # its large baseline while this one is held at zero.
            if self._is_undrained_dual_pipe(node):
                self._add(node.lineno, "SRC005")
        elif attr in ("check_output", "check_call", "call"):
            self._add(node.lineno, "SRC003")

        # The decode hazard only exists when the parent actually reads and
        # decodes the stream. Byte-mode capture is unaffected.
        if (
            captures
            and self._is_text_mode(node)
            and not self._has_kwarg(node, "encoding")
        ):
            self._add(node.lineno, "SRC004")

        self.generic_visit(node)

    def _add(self, lineno: int, code: str) -> None:
        self.violations.append((lineno, code, _MESSAGES[code]))

    def _is_os_spawn(self, node: ast.Call) -> bool:
        """True for `os.system(...)` / `os.popen(...)`."""
        func = node.func
        if not isinstance(func, ast.Attribute):
            return False
        if not (isinstance(func.value, ast.Name) and func.value.id == "os"):
            return False
        return func.attr in ("system", "popen")

    def _subprocess_attr(self, node: ast.Call) -> str | None:
        """Return the attribute name for a `subprocess.<attr>(...)` call."""
        func = node.func
        if not isinstance(func, ast.Attribute):
            return None
        if not (isinstance(func.value, ast.Name) and func.value.id == "subprocess"):
            return None
        return func.attr

    def _is_undrained_dual_pipe(self, node: ast.Call) -> bool:
        """True for the exact shape that deadlocks.

        `Popen(stdout=PIPE, stderr=PIPE)` where nothing ever calls
        `.communicate()` on the result. With both pipes open the parent must
        drain both concurrently; draining one and then waiting wedges the
        moment the other fills its OS buffer (~64 KB on Linux, ~8 KB on
        Windows). Measured on this repo: a child writing 20k lines to stderr
        while the parent read only stdout never returned, while the same child
        under `RunningProcess` finished in 0.3s.

        `communicate()` is the stdlib's own correct drain (it selects/threads
        over both), so its presence anywhere in the enclosing function clears
        the call. That is deliberately generous: this rule is meant to be held
        at zero, so a false positive is more expensive than a missed exotic
        case, and the broader SRC002 still covers everything.

        `stderr=subprocess.STDOUT` is safe and does NOT trigger: it merges
        stderr into the single stdout pipe, leaving nothing undrained.
        """
        stdout_piped = False
        stderr_piped = False
        for kw in node.keywords:
            if kw.arg == "stdout" and self._is_pipe(kw.value):
                stdout_piped = True
            elif kw.arg == "stderr" and self._is_pipe(kw.value):
                stderr_piped = True
        if not (stdout_piped and stderr_piped):
            return False
        return not self._function_calls_communicate(node)

    def _function_calls_communicate(self, node: ast.Call) -> bool:
        """True when `.communicate(` appears in the enclosing function body.

        Scope is found by text search rather than AST parent links (ast nodes
        carry no parent pointer): walk outward from the Popen line to the
        nearest enclosing `def` and scan to the end of its indented block.
        """
        if not self.source_lines:
            return False
        index = node.lineno - 1
        if index >= len(self.source_lines):
            return False

        # Walk up to the enclosing `def` / `async def`.
        start = 0
        def_indent = 0
        for i in range(index, -1, -1):
            stripped = self.source_lines[i].lstrip()
            if stripped.startswith("def ") or stripped.startswith("async def "):
                start = i
                def_indent = len(self.source_lines[i]) - len(stripped)
                break

        # Walk down to the end of that block.
        end = len(self.source_lines)
        for i in range(start + 1, len(self.source_lines)):
            line = self.source_lines[i]
            if not line.strip():
                continue
            indent = len(line) - len(line.lstrip())
            if indent <= def_indent and i > start:
                end = i
                break

        return any(".communicate(" in line for line in self.source_lines[start:end])

    def _captures_output(self, node: ast.Call) -> bool:
        """True when the call captures stdout/stderr through a pipe."""
        for kw in node.keywords:
            if kw.arg == "capture_output" and self._is_true(kw.value):
                return True
            if kw.arg in ("stdout", "stderr") and self._is_pipe(kw.value):
                return True
        return False

    def _is_text_mode(self, node: ast.Call) -> bool:
        """True when output is decoded to str rather than left as bytes."""
        for kw in node.keywords:
            if kw.arg in ("text", "universal_newlines") and self._is_true(kw.value):
                return True
            # An explicit errors= without encoding= still implies text mode.
            if kw.arg == "errors":
                return True
        return False

    def _has_kwarg(self, node: ast.Call, name: str) -> bool:
        return any(kw.arg == name for kw in node.keywords)

    def _is_true(self, node: ast.expr) -> bool:
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
        return "noqa" in line and ("SRC0" in line or "noqa:" not in line)


def check_file(path: str, source: str) -> list[tuple[int, str, str]]:
    """Parse source and return all violations as (lineno, code, message)."""
    try:
        tree = ast.parse(source, filename=path)
    except SyntaxError:
        return []

    visitor = SubprocessVisitor(source.split("\n"))
    visitor.visit(tree)
    return visitor.violations


def _rel_path(path: Path) -> str:
    """Project-relative posix path, or the path itself if outside the tree."""
    try:
        return path.resolve().relative_to(PROJECT_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def _baseline_key(path: Path, code: str) -> str:
    """Stable per-file/per-code key, independent of line numbers."""
    return f"{_rel_path(path)}|{code}"


def _key_file(key: str) -> str:
    """The file portion of a baseline key."""
    return key.rsplit("|", 1)[0]


def load_baseline(path: Path = DEFAULT_BASELINE) -> Counter[str]:
    """Load the checked-in baseline as key -> allowed count."""
    baseline: Counter[str] = Counter()
    if not path.exists():
        return baseline
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, _, count = line.rpartition("|")
        if not key:
            continue
        try:
            baseline[key] = int(count)
        except ValueError:
            continue
    return baseline


def write_baseline(counts: Counter[str], path: Path = DEFAULT_BASELINE) -> None:
    """Write a deterministic baseline for the current findings."""
    lines = [
        "# Known stdlib `subprocess` usages, per file and error code.",
        "# Generated by ci/lint_python/subprocess_capture_checker.py.",
        "#",
        "# Format: <path>|<code>|<count>",
        "#",
        "# Counts may shrink freely -- migrating a call site to RunningProcess",
        "# just lowers the number. Any INCREASE fails the linter, so new raw",
        "# subprocess usage has to be deliberate. Regenerate with:",
        "#   uv run python ci/lint_python/subprocess_capture_checker.py ci \\",
        "#       --update-baseline",
        "",
    ]
    lines.extend(f"{key}|{count}" for key, count in sorted(counts.items()))
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


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
    """Return True if any component of path matches an exclude pattern."""
    path_str = path.as_posix()
    return any(exc in path_str for exc in exclude_parts)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Restrict raw stdlib subprocess use in favor of RunningProcess.",
    )
    parser.add_argument("paths", nargs="+", help="Files or directories to check")
    parser.add_argument(
        "--exclude", nargs="*", default=[], help="Substrings to exclude from file paths"
    )
    parser.add_argument(
        "--baseline",
        default=str(DEFAULT_BASELINE),
        help="Path to the baseline file",
    )
    parser.add_argument(
        "--update-baseline",
        action="store_true",
        help="Rewrite the baseline from the current findings",
    )
    args = parser.parse_args(argv)

    files = collect_python_files(args.paths, args.exclude)

    counts: Counter[str] = Counter()
    detail: dict[str, list[str]] = {}
    # Which files this invocation actually looked at. A baseline entry for a
    # file outside this set says nothing about that file -- it was simply not
    # examined -- so it must never be treated as fixed or dropped.
    scanned: set[str] = set()
    for path in files:
        try:
            source = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        scanned.add(_rel_path(path))
        if not _SUBPROCESS_RE.search(source):
            continue
        for line_no, code, message in check_file(str(path), source):
            key = _baseline_key(path, code)
            counts[key] += 1
            detail.setdefault(key, []).append(f"{path}:{line_no}: {message}")

    baseline_path = Path(args.baseline)
    baseline = load_baseline(baseline_path)

    if args.update_baseline:
        # Merge rather than overwrite. Running with a narrow path list
        # (`... check.py some_file.py --update-baseline`) would otherwise
        # rewrite the whole file from that one file's findings and silently
        # delete every other entry, turning the ratchet off everywhere.
        merged = Counter(
            {k: v for k, v in baseline.items() if _key_file(k) not in scanned}
        )
        merged.update(counts)
        write_baseline(merged, baseline_path)
        total = sum(merged.values())
        kept = len(merged) - len(counts)
        print(
            f"Wrote baseline with {total} finding(s) across {len(merged)} entries "
            f"({len(scanned)} file(s) rescanned, {kept} untouched entry(ies) kept)."
        )
        return 0

    regressions = 0
    for key, count in sorted(counts.items()):
        allowed = baseline.get(key, 0)
        if count <= allowed:
            continue
        # Report only the overflow so a file with a long-standing baseline
        # does not spam every pre-existing line on an unrelated change.
        for line in detail[key][allowed:]:
            print(line)
            regressions += 1

    if regressions:
        print(
            f"\n{regressions} new raw-subprocess usage(s) beyond the baseline.\n"
            "Use RunningProcess from `running_process`, or if this is genuinely "
            "required, append `# noqa: SRC00x` on the call line with a comment "
            "explaining why."
        )
        return 1

    # Only files this run actually scanned can be said to have improved.
    # Without the `scanned` filter a narrow invocation reports every baselined
    # entry in the repo as "now gone", which is both wrong and an invitation
    # to run --update-baseline and wipe them.
    improved = sum(
        max(0, allowed - counts.get(key, 0))
        for key, allowed in baseline.items()
        if _key_file(key) in scanned
    )
    if improved:
        print(
            f"NOTE: {improved} baselined subprocess usage(s) are now gone. "
            "Re-run with --update-baseline to lock in the improvement."
        )
    print(f"No new raw-subprocess usage. Baseline entries: {len(baseline)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
