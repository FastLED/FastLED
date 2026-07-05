"""Checker that bans raw pyserial in the AutoResearch complex.

Device serial in `ci/autoresearch/` MUST go through fbuild's native (Rust)
serial monitor, never raw pyserial. Use
`ci.util.serial_interface.create_serial_interface()` +
`ci.rpc_client.RpcClient` — the same transport ble/decode/driver_sweep and
the coroutine tests already use.

Why: hand-rolled pyserial request/reply loops drop replies ~one-per-session
on Windows (`in_waiting` under-reports, a pre-write `reset_input_buffer()`
races the reply, byte-at-a-time reads straddle the port timeout). The
firmware answers every RPC; pyserial loses them. Silicon-diagnosed
2026-07-04 on the LPC845 SPI/UART benches. Full rule:
`agents/docs/hardware-autoresearch.md` -> "Device serial".

The low-level serial plumbing under `ci/util/` (the pyserial backend that
`create_serial_interface(use_pyserial=True)` selects, plus port helpers) is
NOT scanned by this checker — that is where pyserial legitimately lives.

Error Codes:
    PYS001: raw pyserial import in ci/autoresearch/ — use RpcClient over
            fbuild's Rust serial monitor instead.
"""

from __future__ import annotations

import argparse
import ast
import re
import sys
from pathlib import Path


# Regex pre-filter: quickly skip files that never mention `serial`.
_SERIAL_RE = re.compile(r"\bserial\b")

# Matches a `noqa: PYS001` suppression comment.
_NOQA_RE = re.compile(r"#\s*noqa:\s*PYS001\b")

# NO allowlist by policy — every file in the AutoResearch complex must go
# through fbuild's Rust serial monitor. If a genuinely exceptional site
# ever needs raw pyserial, suppress it inline with a `noqa: PYS001` comment and a
# justification comment; do not grandfather files wholesale.

_MESSAGE = (
    "PYS001 raw pyserial is FORBIDDEN in ci/autoresearch/. Device serial must "
    "go through fbuild's Rust serial monitor: use "
    "ci.util.serial_interface.create_serial_interface() + "
    "ci.rpc_client.RpcClient (mandatory JSON-RPC id correlation, robust "
    "framing). Raw pyserial drops replies ~one-per-session on Windows. See "
    "agents/docs/hardware-autoresearch.md -> 'Device serial'."
)


class PySerialImportVisitor(ast.NodeVisitor):
    """Finds `import serial[...]` and `from serial import ...` statements.

    The import is the load-bearing signal — you cannot touch pyserial
    without importing it, so flagging the import catches every usage with
    zero false positives from unrelated identifiers named `serial`.
    """

    def __init__(self) -> None:
        self.violations: list[int] = []

    def visit_Import(self, node: ast.Import) -> None:  # noqa: N802
        for alias in node.names:
            # `import serial` or `import serial.tools...` or `import serial as x`
            if alias.name == "serial" or alias.name.startswith("serial."):
                self.violations.append(node.lineno)
                break
        self.generic_visit(node)

    def visit_ImportFrom(self, node: ast.ImportFrom) -> None:  # noqa: N802
        if node.module == "serial" or (
            node.module is not None and node.module.startswith("serial.")
        ):
            self.violations.append(node.lineno)
        self.generic_visit(node)


def check_file(path: str, source: str) -> list[tuple[int, str]]:
    """Parse source and return PYS001 violations not suppressed by noqa."""
    try:
        tree = ast.parse(source, filename=path)
    except SyntaxError:
        return []

    visitor = PySerialImportVisitor()
    visitor.visit(tree)

    lines = source.splitlines()
    result: list[tuple[int, str]] = []
    for line_no in visitor.violations:
        if line_no <= len(lines) and _NOQA_RE.search(lines[line_no - 1]):
            continue
        result.append((line_no, _MESSAGE))
    return result


def _is_excluded(path: Path, exclude_parts: list[str]) -> bool:
    path_str = path.as_posix()
    return any(exc in path_str for exc in exclude_parts)


def collect_python_files(paths: list[str], excludes: list[str]) -> list[Path]:
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


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Ban raw pyserial in the AutoResearch complex.",
    )
    parser.add_argument("paths", nargs="+", help="Files or directories to check")
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
        if not _SERIAL_RE.search(source):
            continue
        for line_no, message in check_file(str(path), source):
            print(f"{path}:{line_no}: {message}")
            total_violations += 1

    return 1 if total_violations > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
