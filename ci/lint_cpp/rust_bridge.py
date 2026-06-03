"""Bridge helpers for the Rust-backed C++ lint checker subset."""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

from running_process import RunningProcess

from ci.util.check_files import CheckerResults, FileContentChecker
from ci.util.paths import PROJECT_ROOT


RUST_SUPPORTED_CHECKERS = frozenset(
    {
        "ArduinoMacroUsageChecker",
        "AsmJsLocationChecker",
        "AttributeChecker",
        "BareAllocationChecker",
        "BareUsingChecker",
        "BannedDefineChecker",
        "BannedHeadersChecker",
        "BannedMacrosChecker",
        "BannedNamespaceChecker",
        "BuiltinMemcpyChecker",
        "CppHppHeaderPairChecker",
        "CppHppIncludesChecker",
        "CppIncludeChecker",
        "CtypeGlobalChecker",
        "EnumClassChecker",
        "EspRomPrintfChecker",
        "ExampleSerialChecker",
        "FastLEDHeaderUsageChecker",
        "FlIsDefinedChecker",
        "HeadersExistChecker",
        "IncludeAfterNamespaceChecker",
        "IncludePathsChecker",
        "ImplHppIncludesChecker",
        "IsHeaderIncludeChecker",
        "IwyuPragmaBlockChecker",
        "LoggingInIramChecker",
        "MemberStyleChecker",
        "NamespaceFlDeclarationChecker",
        "NamespaceIncludesChecker",
        "NamespacePlatformsChecker",
        "NativePlatformDefinesChecker",
        "NoexceptSpecialMembersChecker",
        "NumericLimitMacroChecker",
        "PlatformIncludesChecker",
        "PlatformsFlNamespaceChecker",
        "PragmaOnceChecker",
        "PlatformPragmaChecker",
        "PlatformTrampolineChecker",
        "RawNoexceptChecker",
        "RawPragmaChecker",
        "ReinterpretCastChecker",
        "RelativeIncludeChecker",
        "SerialPrintfChecker",
        "SimdIntrinsicsChecker",
        "SleepForChecker",
        "SingletonInHeadersChecker",
        "SpanFromPointerChecker",
        "StaticInHeaderChecker",
        "StdNamespaceChecker",
        "StdintTypeChecker",
        "SubdirNamespaceChecker",
        "TestAggregationChecker",
        "TestIncludePathsChecker",
        "TestPathStructureChecker",
        "ThreadLocalKeywordChecker",
        "UnitTestChecker",
        "UsingNamespaceChecker",
        "UsingNamespaceFlChecker",
        "UsingNamespaceFlInExamplesChecker",
        "WeakAttributeChecker",
    }
)


def remove_rust_supported_checkers(
    checkers_by_scope: dict[str, list[FileContentChecker]],
) -> None:
    """Remove checker instances handled by the Rust fast path."""
    for scope, checkers in checkers_by_scope.items():
        filtered: list[FileContentChecker] = []
        for checker in checkers:
            if checker.__class__.__name__ not in RUST_SUPPORTED_CHECKERS:
                filtered.append(checker)
        checkers_by_scope[scope] = filtered


def run_rust_linter(files: list[str] | None) -> dict[str, CheckerResults]:
    """Run the Rust linter and return results in the Python linter shape."""
    cmd = [
        "soldr",
        "cargo",
        "run",
        "--release",
        "--bin",
        "fastled-lint",
        "--manifest-path",
        "ci/lint_cpp_rs/Cargo.toml",
        "--",
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
            return value

    raise RuntimeError("Rust C++ linter did not produce valid JSON on stdout")


def run_rust_ab_check(
    python_results: dict[str, CheckerResults],
    files: list[str] | None,
) -> bool:
    """Compare Rust-supported checker output against the Python oracle."""
    rust_results = run_rust_linter(files)
    python_canonical = _canonical_results(
        {
            name: results
            for name, results in python_results.items()
            if name in RUST_SUPPORTED_CHECKERS
        }
    )
    rust_canonical = _canonical_results(rust_results)

    if python_canonical == rust_canonical:
        print("Rust/Python C++ linter A/B parity passed for supported checkers.")
        return True

    print("Rust/Python C++ linter A/B parity failed.", file=sys.stderr)
    _print_diff_sample("Python-only", python_canonical - rust_canonical)
    _print_diff_sample("Rust-only", rust_canonical - python_canonical)
    return False


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


def _canonical_results(
    results: dict[str, CheckerResults],
) -> set[tuple[str, str, int, str]]:
    canonical: set[tuple[str, str, int, str]] = set()
    for checker_name, checker_results in results.items():
        for file_path, file_violations in checker_results.violations.items():
            normalized_path = str(Path(file_path)).replace("\\", "/")
            for violation in file_violations.violations:
                canonical.add(
                    (
                        checker_name,
                        normalized_path,
                        violation.line_number,
                        violation.content,
                    )
                )
    return canonical


def _print_diff_sample(label: str, values: set[tuple[str, str, int, str]]) -> None:
    if not values:
        return
    print(f"{label} differences ({len(values)} total, first 20):", file=sys.stderr)
    for checker, path, line, message in sorted(values)[:20]:
        print(f"  {checker} {path}:{line}: {message}", file=sys.stderr)
