#!/usr/bin/env python3
"""
Build Stats Analyzer for --no-fingerprint Mode

Investigates what takes time during `bash test --no-fingerprint` in quick mode.
The degenerate case is when nothing has changed but --no-fingerprint forces
all fingerprint checks to run anyway, computing hashes that are then discarded.

Usage:
    uv run ci/util/no_fingerprint_build_stats.py

Reports:
    - Time spent computing each fingerprint (wasted when --no-fingerprint)
    - File counts per directory
    - Estimated time savings from skipping fingerprint computation
"""

import hashlib
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


@dataclass
class DirectoryStat:
    """Stats for a single directory fingerprint computation."""

    name: str
    path: str
    patterns: list[str]
    file_count: int = 0
    total_bytes: int = 0
    elapsed_seconds: float = 0.0
    hash_value: str = ""


@dataclass
class FingerprintStats:
    """All fingerprint computation stats."""

    check_all_src: Optional[DirectoryStat] = None
    check_cpp_src: Optional[DirectoryStat] = None
    check_cpp_tests: Optional[DirectoryStat] = None
    check_examples_src: Optional[DirectoryStat] = None
    check_examples_examples: Optional[DirectoryStat] = None
    check_python: Optional[DirectoryStat] = None
    check_wasm: Optional[DirectoryStat] = None
    directories: list[DirectoryStat] = field(default_factory=list)

    def total_wasted_time(self) -> float:
        """Total time wasted computing fingerprints (all reads when --no-fingerprint)."""
        return sum(d.elapsed_seconds for d in self.directories)

    def total_files(self) -> int:
        return sum(d.file_count for d in self.directories)

    def total_bytes(self) -> int:
        return sum(d.total_bytes for d in self.directories)


def fingerprint_directory(
    directory: Path, patterns: list[str], label: str
) -> DirectoryStat:
    """Measure time to fingerprint a directory with given patterns."""
    start = time.time()
    hasher = hashlib.sha256()
    file_count = 0
    total_bytes = 0

    all_files: list[Path] = []
    for pattern in patterns:
        all_files.extend(sorted(directory.glob(pattern)))
    all_files = sorted(set(all_files))

    for f in all_files:
        if f.is_file():
            file_count += 1
            rel_path = f.relative_to(directory)
            hasher.update(str(rel_path).encode())
            try:
                with open(f, "rb") as fp:
                    for chunk in iter(lambda: fp.read(4096), b""):
                        total_bytes += len(chunk)
                        hasher.update(chunk)
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                hasher.update(f"ERROR:{e}".encode())

    elapsed = time.time() - start

    return DirectoryStat(
        name=label,
        path=str(directory),
        patterns=patterns,
        file_count=file_count,
        total_bytes=total_bytes,
        elapsed_seconds=elapsed,
        hash_value=hasher.hexdigest()[:16] + "...",
    )


def analyze_fingerprint_costs(root: Path) -> FingerprintStats:
    """
    Analyze how much time is wasted computing fingerprints when --no-fingerprint is used.

    This mirrors what FingerprintManager.check_all(), check_cpp(), check_examples(),
    check_python(), and check_wasm() do internally.
    """
    stats = FingerprintStats()
    src_dir = root / "src"
    tests_dir = root / "tests"
    examples_dir = root / "examples"
    ci_dir = root / "ci"

    cpp_patterns = ["**/*.h", "**/*.cpp", "**/*.hpp"]
    ino_patterns = ["**/*.ino", "**/*.h", "**/*.cpp", "**/*.hpp"]

    print("Measuring fingerprint computation costs...")
    print(
        "(This mirrors what --no-fingerprint wastes when computing hashes it ignores)\n"
    )

    # check_all() reads src/
    if src_dir.exists():
        print(f"  [check_all]      Computing src/ hash...", end="", flush=True)
        stat = fingerprint_directory(src_dir, cpp_patterns, "check_all: src/")
        stats.directories.append(stat)
        stats.check_all_src = stat
        print(
            f" {stat.elapsed_seconds:.3f}s ({stat.file_count} files, {stat.total_bytes / 1024 / 1024:.1f} MB)"
        )

    # check_cpp() reads src/ AND tests/
    if src_dir.exists():
        print(f"  [check_cpp]      Computing src/ hash...", end="", flush=True)
        stat = fingerprint_directory(src_dir, cpp_patterns, "check_cpp: src/")
        stats.directories.append(stat)
        stats.check_cpp_src = stat
        print(f" {stat.elapsed_seconds:.3f}s ({stat.file_count} files)")

    if tests_dir.exists():
        print(f"  [check_cpp]      Computing tests/ hash...", end="", flush=True)
        stat = fingerprint_directory(tests_dir, cpp_patterns, "check_cpp: tests/")
        stats.directories.append(stat)
        stats.check_cpp_tests = stat
        print(
            f" {stat.elapsed_seconds:.3f}s ({stat.file_count} files, {stat.total_bytes / 1024 / 1024:.1f} MB)"
        )

    # check_examples() reads src/ AND examples/
    if src_dir.exists():
        print(f"  [check_examples] Computing src/ hash...", end="", flush=True)
        stat = fingerprint_directory(src_dir, cpp_patterns, "check_examples: src/")
        stats.directories.append(stat)
        stats.check_examples_src = stat
        print(f" {stat.elapsed_seconds:.3f}s ({stat.file_count} files)")

    if examples_dir.exists():
        print(f"  [check_examples] Computing examples/ hash...", end="", flush=True)
        stat = fingerprint_directory(
            examples_dir, ino_patterns, "check_examples: examples/"
        )
        stats.directories.append(stat)
        stats.check_examples_examples = stat
        print(
            f" {stat.elapsed_seconds:.3f}s ({stat.file_count} files, {stat.total_bytes / 1024 / 1024:.1f} MB)"
        )

    # check_python() reads ci/ Python files
    if ci_dir.exists():
        print(f"  [check_python]   Computing ci/ hash...", end="", flush=True)
        stat = fingerprint_directory(ci_dir, ["**/*.py"], "check_python: ci/")
        stats.directories.append(stat)
        stats.check_python = stat
        print(
            f" {stat.elapsed_seconds:.3f}s ({stat.file_count} files, {stat.total_bytes / 1024 / 1024:.1f} MB)"
        )

    # check_wasm() reads wasm-related files (typically small)
    wasm_dir = root / "src"  # wasm fingerprint also covers src/
    if wasm_dir.exists():
        print(f"  [check_wasm]     Computing src/ hash...", end="", flush=True)
        stat = fingerprint_directory(wasm_dir, cpp_patterns, "check_wasm: src/")
        stats.directories.append(stat)
        stats.check_wasm = stat
        print(f" {stat.elapsed_seconds:.3f}s ({stat.file_count} files)")

    return stats


def print_report(stats: FingerprintStats) -> None:
    """Print analysis report."""
    print("\n" + "=" * 70)
    print("FINGERPRINT COST ANALYSIS REPORT")
    print("What --no-fingerprint wastes (hashes computed but discarded)")
    print("=" * 70)

    print(f"\n{'Directory':<35} {'Files':>6} {'Size':>8} {'Time':>8}")
    print("-" * 65)
    for d in stats.directories:
        size_mb = d.total_bytes / 1024 / 1024 if d.total_bytes else 0
        size_str = (
            f"{size_mb:.1f} MB" if size_mb > 0.1 else f"{d.total_bytes / 1024:.0f} KB"
        )
        print(
            f"  {d.name:<33} {d.file_count:>6} {size_str:>8} {d.elapsed_seconds:>7.3f}s"
        )

    print("-" * 65)
    total_time = stats.total_wasted_time()
    total_files = stats.total_files()
    print(
        f"  {'TOTAL (wasted by --no-fingerprint)':<33} {total_files:>6} {'':>8} {total_time:>7.3f}s"
    )

    print(f"\nKEY INSIGHT:")
    print(f"  When --no-fingerprint is used, ALL of the above is wasted work.")
    print(f"  The fingerprint hashes are computed but then IGNORED because")
    print(f"  --no-fingerprint forces all tests to run regardless.")
    print(f"\n  Wasted time: {total_time:.2f}s on every --no-fingerprint invocation")

    # Show duplication (src/ is read multiple times)
    src_reads = [d for d in stats.directories if "src/" in d.name]
    if len(src_reads) > 1:
        src_time = sum(d.elapsed_seconds for d in src_reads)
        print(f"\n  Note: src/ is read {len(src_reads)}x ({src_time:.2f}s total)")
        print(f"  Each check_*() call independently re-reads src/")

    print(f"\nFIX:")
    print(f"  In test.py, skip fingerprint computation when --no-fingerprint:")
    print(f"  Instead of computing fingerprints and ignoring them,")
    print(f"  set all change flags to True directly.")
    print(f"  Estimated savings: {total_time:.2f}s per run")

    print(f"\nFINGERPRINT FILE COUNTS:")
    total_unique_files = set()
    src_dir = Path("src")
    if src_dir.exists():
        for p in ["**/*.h", "**/*.cpp", "**/*.hpp"]:
            total_unique_files.update(src_dir.glob(p))
    tests_dir = Path("tests")
    if tests_dir.exists():
        for p in ["**/*.h", "**/*.cpp", "**/*.hpp"]:
            total_unique_files.update(tests_dir.glob(p))
    examples_dir = Path("examples")
    if examples_dir.exists():
        for p in ["**/*.ino", "**/*.h", "**/*.cpp", "**/*.hpp"]:
            total_unique_files.update(examples_dir.glob(p))
    total_unique_files = {f for f in total_unique_files if f.is_file()}
    print(f"  Unique source files affected: {len(total_unique_files)}")
    print(f"  Source reads per --no-fingerprint call: {len(stats.directories)}")
    print(
        f"  Redundant reads (due to src/ being read multiple times): {len(src_reads) - 1}"
    )


def main() -> None:
    # Find project root
    script_dir = Path(__file__).parent
    root = script_dir.parent.parent
    os.chdir(root)

    print(f"FastLED Build Stats Analyzer")
    print(f"Project root: {root}")
    print(f"Analyzing --no-fingerprint overhead in quick mode...\n")

    stats = analyze_fingerprint_costs(root)
    print_report(stats)

    # Save results
    output_file = Path(".loop") / "no_fingerprint_stats.json"
    if output_file.parent.exists():
        import json

        results = {
            "total_wasted_seconds": stats.total_wasted_time(),
            "total_files_hashed": stats.total_files(),
            "total_bytes_read": stats.total_bytes(),
            "directories": [
                {
                    "name": d.name,
                    "path": d.path,
                    "file_count": d.file_count,
                    "total_bytes": d.total_bytes,
                    "elapsed_seconds": d.elapsed_seconds,
                }
                for d in stats.directories
            ],
        }
        with open(output_file, "w") as f:
            json.dump(results, f, indent=2)
        print(f"\nResults saved to: {output_file}")


if __name__ == "__main__":
    main()
