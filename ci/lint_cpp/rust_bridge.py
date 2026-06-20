"""Bridge helpers for the Rust-backed C++ lint checker subset."""

from __future__ import annotations

import json
import sys
from typing import Any, cast

from running_process import RunningProcess

from ci.lint_cpp.rust_binary_cache import ensure_rust_lint_binary
from ci.util.check_files import CheckerResults
from ci.util.paths import PROJECT_ROOT


def run_rust_linter(files: list[str] | None) -> dict[str, CheckerResults]:
    """Run the Rust linter and return results in the Python linter shape.

    The binary is rebuilt only when the Rust crate sources change; otherwise
    it is invoked directly with no soldr/cargo overhead per call. See
    :mod:`ci.lint_cpp.rust_binary_cache`.
    """
    binary = ensure_rust_lint_binary()
    cmd: list[str] = [
        str(binary),
        "--format",
        "json",
        "--project-root",
        str(PROJECT_ROOT),
    ]

    if files:
        cmd.extend(files)

    result = RunningProcess.run(
        cmd,
        cwd=str(PROJECT_ROOT),
        capture_output=True,
        check=False,
        timeout=300,
    )

    if result.stderr:
        print(
            result.stderr,
            file=sys.stderr,
            end="" if result.stderr.endswith("\n") else "\n",
        )

    if result.returncode not in (0, 1):
        raise RuntimeError(
            "Rust C++ linter failed before producing lint results "
            f"(exit {result.returncode})"
        )

    raw_violations = parse_rust_json_output(result.stdout)

    return _violations_to_results(raw_violations)


def parse_rust_json_output(stdout: str | None) -> list[dict[str, Any]]:
    """Extract the linter JSON array from soldr/cargo captured stdout."""
    text = stdout or "[]"
    decoder = json.JSONDecoder()

    for index, char in enumerate(text):
        if char != "[":
            continue
        try:
            value, _end = decoder.raw_decode(text[index:])
        except json.JSONDecodeError:
            continue
        if isinstance(value, list):
            return cast(list[dict[str, Any]], value)

    raise RuntimeError("Rust C++ linter did not produce valid JSON on stdout")


def merge_checker_results(
    destination: dict[str, CheckerResults],
    source: dict[str, CheckerResults],
) -> None:
    """Merge source checker results into destination in-place."""
    for checker_name, source_results in source.items():
        if checker_name not in destination:
            destination[checker_name] = source_results
            continue
        for file_path, file_violations in source_results.violations.items():
            for violation in file_violations.violations:
                destination[checker_name].add_violation(
                    file_path, violation.line_number, violation.content
                )


def _violations_to_results(
    raw_violations: list[dict[str, Any]],
) -> dict[str, CheckerResults]:
    results: dict[str, CheckerResults] = {}
    for item in raw_violations:
        checker = str(item["checker"])
        path = str(item["path"])
        line = int(item["line"])
        message = str(item["message"])
        results.setdefault(checker, CheckerResults()).add_violation(path, line, message)
    return results
