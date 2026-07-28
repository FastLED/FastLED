"""The warn-only checker list is duplicated in Rust and Python; keep them equal.

The Rust binary decides its own exit code, and the Python orchestrator tallies
the same findings a second time to decide its own. If the two lists drift, the
stricter layer wins silently: a checker meant to be warn-only starts failing
CI, or -- worse -- one meant to block stops blocking.
"""

from __future__ import annotations

import re
from pathlib import Path

from ci.lint_cpp.run_all_checkers import WARN_ONLY_CHECKERS


PROJECT_ROOT = Path(__file__).resolve().parents[2]
RUST_SOURCE = PROJECT_ROOT / "ci" / "lint_cpp_rs" / "src" / "lint_core" / "warn_only.rs"


def _parse_rust_warn_only() -> set[str]:
    """Extract the string literals from the Rust WARN_ONLY_CHECKERS slice."""
    text = RUST_SOURCE.read_text(encoding="utf-8")
    match = re.search(
        r"pub const WARN_ONLY_CHECKERS:\s*&\[&str\]\s*=\s*&\[(.*?)\];",
        text,
        re.DOTALL,
    )
    assert match is not None, f"WARN_ONLY_CHECKERS not found in {RUST_SOURCE}"

    body = match.group(1)
    # Strip line comments so a checker name mentioned in prose is not counted.
    body = re.sub(r"//[^\n]*", "", body)
    return set(re.findall(r'"([^"]+)"', body))


def test_warn_only_lists_match() -> None:
    rust = _parse_rust_warn_only()
    python = set(WARN_ONLY_CHECKERS)
    assert rust == python, (
        "warn-only checker lists have drifted.\n"
        f"  only in Rust:   {sorted(rust - python)}\n"
        f"  only in Python: {sorted(python - rust)}"
    )


def test_warn_only_list_is_not_empty() -> None:
    """Guards the parser itself.

    An empty result would make test_warn_only_lists_match pass vacuously if
    the regex ever stopped matching.
    """
    assert _parse_rust_warn_only(), "parsed an empty Rust warn-only list"
