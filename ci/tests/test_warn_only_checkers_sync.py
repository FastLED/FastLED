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


def _parse_warn_only_slice(text: str) -> set[str]:
    """Extract the string literals from a Rust WARN_ONLY_CHECKERS slice."""
    # Strip line comments first so neither a checker name mentioned in prose
    # nor a `];` inside a comment can affect the match.
    text = re.sub(r"//[^\n]*", "", text)
    match = re.search(
        r"pub const WARN_ONLY_CHECKERS:\s*&\[&str\]\s*=\s*&\[(.*?)\];",
        text,
        re.DOTALL,
    )
    assert match is not None, f"WARN_ONLY_CHECKERS not found in {RUST_SOURCE}"

    return set(re.findall(r'"([^"]+)"', match.group(1)))


def _parse_rust_warn_only() -> set[str]:
    return _parse_warn_only_slice(RUST_SOURCE.read_text(encoding="utf-8"))


def test_warn_only_lists_match() -> None:
    rust = _parse_rust_warn_only()
    python = set(WARN_ONLY_CHECKERS)
    assert rust == python, (
        "warn-only checker lists have drifted.\n"
        f"  only in Rust:   {sorted(rust - python)}\n"
        f"  only in Python: {sorted(python - rust)}"
    )


def test_parser_reads_entries() -> None:
    """Guards the parser itself.

    The real list is empty after FastLED#4557, so without this a regex that
    stopped matching entries would make test_warn_only_lists_match pass
    vacuously.
    """
    sample = (
        "pub const WARN_ONLY_CHECKERS: &[&str] = &[\n"
        '    // "CommentedChecker" in prose; a stray ]; here too\n'
        '    "FooChecker",\n'
        "];\n"
    )
    assert _parse_warn_only_slice(sample) == {"FooChecker"}


def test_promoted_checkers_hard_fail() -> None:
    """FastLED#4557: these must never silently return to warn-only."""
    promoted = {
        "SingletonElisionChecker",
        "PreferConstexprChecker",
        "ContainerElementAddressChecker",
    }
    assert not promoted & _parse_rust_warn_only()
    assert not promoted & set(WARN_ONLY_CHECKERS)
