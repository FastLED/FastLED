#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false

"""Run include-what-you-use on the project.

Default mode (no board): parallel scan of all src/fl/ headers (~2 min).
Board mode: run IWYU via PlatformIO on a specific board build.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path


# ---------------------------------------------------------------------------
# Phantom-violation filtering helpers
# ---------------------------------------------------------------------------


def _extract_line_number(removal_text: str) -> int | None:
    """Extract line number from IWYU removal like '#include "foo.h"  // lines 3-3'."""
    m = re.search(r"// lines (\d+)", removal_text)
    return int(m.group(1)) if m else None


def _line_has_real_violation(file_path: Path, line_num: int) -> bool:
    """Check if the given line actually contains a removable #include or forward declaration."""
    try:
        lines = file_path.read_text(encoding="utf-8", errors="replace").splitlines()
        if line_num < 1 or line_num > len(lines):
            return False
        line = lines[line_num - 1].strip()
        # Lines explicitly marked to keep are not violations
        if "IWYU pragma: keep" in line:
            return False
        # Actual #include
        if line.startswith("#include"):
            return True
        # Forward declarations (class/struct/enum/template)
        for kw in ("class ", "struct ", "enum ", "template"):
            if kw in line and "IWYU pragma: no_forward_declare" not in line:
                return True
        # Namespace-wrapped forward decls like "namespace fl { class Foo; }"
        if line.startswith("namespace") and ("class " in line or "struct " in line):
            return True
        return False
    except KeyboardInterrupt:
        import _thread

        _thread.interrupt_main()
        return True  # unreachable, satisfies type checker
    except Exception:
        return True  # Can't read file — keep the violation to be safe


# ---------------------------------------------------------------------------
# Single-file IWYU scanner (runs in worker processes)
# ---------------------------------------------------------------------------

# Resolved once at import time so workers inherit it.
_PROJECT_ROOT = Path(__file__).resolve().parent.parent


def _scan_one_header(file_path_str: str) -> tuple[str, list[str]]:
    """Run IWYU on a single header and return (rel_path, removals).

    Returns ("", []) when there are no real violations.
    """
    f = Path(file_path_str)
    iwyu_cmd = [
        sys.executable,
        str(_PROJECT_ROOT / "ci" / "iwyu_wrapper.py"),
        "-Xiwyu",
        "--error",
        "--",
        "clang-tool-chain-sccache-cpp",
        "-std=gnu++11",
        "-DSTUB_PLATFORM",
        "-DARDUINO=10808",
        "-DFASTLED_USE_STUB_ARDUINO",
        "-DFASTLED_STUB_IMPL",
        "-DFASTLED_TESTING",
        "-DFASTLED_UNIT_TEST=1",
        f"-I{_PROJECT_ROOT / 'src'}",
        f"-I{_PROJECT_ROOT / 'src' / 'platforms' / 'stub'}",
        "-x",
        "c++-header",
        "-c",
        str(f),
    ]
    try:
        result = subprocess.run(iwyu_cmd, capture_output=True, text=True, timeout=30)
    except subprocess.TimeoutExpired:
        return ("", [])

    if result.returncode == 0:
        return ("", [])

    removals: list[str] = []
    in_remove = False
    for line in result.stderr.split("\n"):
        if "should remove" in line:
            in_remove = True
            continue
        if "The full include-list" in line or "should add" in line:
            in_remove = False
            continue
        if in_remove and line.startswith("- "):
            removal = line[2:].strip()
            line_num = _extract_line_number(removal)
            if line_num is not None and not _line_has_real_violation(f, line_num):
                continue  # Phantom violation — skip
            removals.append(removal)

    if removals:
        rel = str(f.relative_to(_PROJECT_ROOT)).replace("\\", "/")
        return (rel, removals)
    return ("", [])


# ---------------------------------------------------------------------------
# Parallel header scanner
# ---------------------------------------------------------------------------


def scan_single_file(file_path: str) -> tuple[str, list[str]]:
    """Scan a single C++ file for IWYU violations.

    Args:
        file_path: Path to the file to scan.

    Returns:
        (rel_path, removals) — empty rel_path if no violations.
    """
    f = Path(file_path).resolve()
    project_root = _PROJECT_ROOT

    is_header = f.suffix in (".h", ".hpp", ".hh", ".hxx")
    is_test = False
    try:
        f.relative_to(project_root / "tests")
        is_test = True
    except ValueError:
        pass

    compiler_args = [
        "clang-tool-chain-sccache-cpp",
        "-std=gnu++11",
        "-DSTUB_PLATFORM",
        "-DARDUINO=10808",
        "-DFASTLED_USE_STUB_ARDUINO",
        "-DFASTLED_STUB_IMPL",
        "-DFASTLED_TESTING",
        "-DFASTLED_UNIT_TEST=1",
        f"-I{project_root / 'src'}",
        f"-I{project_root / 'src' / 'platforms' / 'stub'}",
    ]
    if is_test:
        compiler_args.append(f"-I{project_root / 'tests'}")
    if is_header:
        compiler_args.extend(["-x", "c++-header"])
    compiler_args.extend(["-c", str(f)])

    iwyu_cmd = [
        sys.executable,
        str(project_root / "ci" / "iwyu_wrapper.py"),
        "-Xiwyu",
        "--error",
        "--",
    ] + compiler_args

    try:
        result = subprocess.run(iwyu_cmd, capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return ("", [])

    if result.returncode == 0:
        return ("", [])

    removals: list[str] = []
    in_remove = False
    for line in result.stderr.split("\n"):
        if "should remove" in line:
            in_remove = True
            continue
        if "The full include-list" in line or "should add" in line:
            in_remove = False
            continue
        if in_remove and line.startswith("- "):
            removal = line[2:].strip()
            line_num = _extract_line_number(removal)
            if line_num is not None and not _line_has_real_violation(f, line_num):
                continue
            removals.append(removal)

    if removals:
        try:
            rel = str(f.relative_to(project_root)).replace("\\", "/")
        except ValueError:
            rel = str(f)
        return (rel, removals)
    return ("", [])


def scan_fl_headers(quiet: bool = False) -> dict[str, list[str]]:
    """Scan all src/fl/ headers in parallel and return {file: [violations]}."""
    fl_dir = _PROJECT_ROOT / "src" / "fl"
    files: list[Path] = []
    for ext in ("*.h", "*.hpp"):
        files.extend(fl_dir.rglob(ext))
    files = [f for f in files if not str(f).endswith(".cpp.hpp")]
    files = sorted(files)

    max_workers = os.cpu_count() or 4
    if not quiet:
        print(f"Scanning {len(files)} headers with {max_workers} workers...")
    start = time.time()

    results: dict[str, list[str]] = {}
    done = 0
    with ProcessPoolExecutor(max_workers=max_workers) as pool:
        futures = {pool.submit(_scan_one_header, str(f)): f for f in files}
        for future in as_completed(futures):
            done += 1
            if not quiet and done % 50 == 0:
                elapsed = time.time() - start
                print(f"  [{done}/{len(files)}] ({elapsed:.1f}s)...")
            rel, removals = future.result()
            if rel and removals:
                results[rel] = removals

    elapsed = time.time() - start
    if not quiet:
        print(
            f"Done: {len(files)} files in {elapsed:.1f}s ({len(results)} with violations)"
        )
    return results


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run include-what-you-use on the project"
    )
    parser.add_argument("board", nargs="?", help="Board to check (optional)")
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Automatically apply suggested fixes using fix_includes",
    )
    parser.add_argument(
        "--mapping-file",
        action="append",
        help="Additional mapping file to use (can be specified multiple times)",
    )
    parser.add_argument(
        "--max-line-length",
        type=int,
        default=100,
        help="Maximum line length for suggestions (default: 100)",
    )
    parser.add_argument("--verbose", action="store_true", help="Enable verbose output")
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress verbose output (show only essential status)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output results as JSON (header scan mode only)",
    )
    parser.add_argument(
        "--file",
        help="Check a single file instead of scanning all headers",
    )
    return parser.parse_args()


# ---------------------------------------------------------------------------
# IWYU availability check
# ---------------------------------------------------------------------------


def check_iwyu_available() -> tuple[bool, str]:
    """Return (is_available, command_prefix)."""
    # System IWYU
    try:
        result = subprocess.run(
            ["include-what-you-use", "--version"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode == 0:
            return (True, "")
    except (
        subprocess.CalledProcessError,
        FileNotFoundError,
        subprocess.TimeoutExpired,
    ):
        pass

    # clang-tool-chain-iwyu via uv
    try:
        result = subprocess.run(
            [
                "uv",
                "run",
                "python",
                "-c",
                "from clang_tool_chain.wrapper import iwyu_main",
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode == 0:
            return (True, "uv run ")
    except (
        subprocess.CalledProcessError,
        FileNotFoundError,
        subprocess.TimeoutExpired,
    ):
        pass

    return (False, "")


# ---------------------------------------------------------------------------
# PlatformIO board mode (unchanged from original)
# ---------------------------------------------------------------------------


def find_platformio_project_dir(board_dir: Path) -> Path | None:
    """Find a directory containing platformio.ini in the board's build directory."""
    if not board_dir.exists():
        return None
    for subdir in board_dir.iterdir():
        if subdir.is_dir():
            if (subdir / "platformio.ini").exists():
                print(f"Found platformio.ini in {subdir}")
                return subdir
    if (board_dir / "platformio.ini").exists():
        print(f"Found platformio.ini directly in {board_dir}")
        return board_dir
    return None


def run_iwyu_on_platformio_project(project_dir: Path, args: argparse.Namespace) -> int:
    """Run IWYU on a PlatformIO project."""
    print(f"Running include-what-you-use in {project_dir}")
    os.chdir(str(project_dir))

    mapping_args: list[str] = []
    project_root = project_dir
    while project_root.parent != project_root:
        if (project_root / "ci" / "iwyu").exists():
            break
        project_root = project_root.parent

    for name in ("fastled.imp", "stdlib.imp"):
        p = project_root / "ci" / "iwyu" / name
        if p.exists():
            mapping_args.extend(["--mapping_file", str(p)])

    if args.mapping_file:
        for mapping in args.mapping_file:
            mapping_args.extend(["--mapping_file", mapping])

    iwyu_cmd = [
        "include-what-you-use",
        f"--max_line_length={args.max_line_length}",
        "--quoted_includes_first",
        "--no_comments",
        "--verbose=3" if args.verbose else "--verbose=1",
    ] + mapping_args

    pio_cmd = [
        "pio",
        "check",
        "--skip-packages",
        "--src-filters=+<src/>",
        "--tool=include-what-you-use",
        "--flags",
    ] + iwyu_cmd[1:]

    try:
        result = subprocess.run(pio_cmd)
        return result.returncode
    except subprocess.CalledProcessError as e:
        print(f"PlatformIO IWYU check failed with return code {e.returncode}")
        return e.returncode


def apply_iwyu_fixes(source_dir: Path) -> int:
    """Apply IWYU fixes using fix_includes tool."""
    try:
        subprocess.run(["fix_includes", "--help"], capture_output=True, check=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print(
            "Warning: fix_includes tool not found. Install IWYU tools to use --fix option."
        )
        return 1

    cpp_files: list[Path] = []
    for pattern in ("**/*.cpp", "**/*.h", "**/*.hpp"):
        cpp_files.extend(source_dir.glob(pattern))
    if not cpp_files:
        print("No C++ files found to fix")
        return 0

    print(f"Applying IWYU fixes to {len(cpp_files)} files...")
    cmd = ["fix_includes", "--update_comments", str(source_dir)]
    try:
        result = subprocess.run(cmd)
        if result.returncode == 0:
            print("IWYU fixes applied successfully")
        else:
            print(f"fix_includes failed with return code {result.returncode}")
        return result.returncode
    except subprocess.CalledProcessError as e:
        print(f"Failed to apply IWYU fixes: {e}")
        return e.returncode


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------


def main() -> int:
    args = parse_args()

    iwyu_available, iwyu_prefix = check_iwyu_available()
    if not iwyu_available:
        print("Error: include-what-you-use not found")
        print("Install it with:")
        print("  Ubuntu/Debian: sudo apt install iwyu")
        print("  macOS: brew install include-what-you-use")
        print("  Or it should be available via: uv run clang-tool-chain-iwyu")
        return 1

    if not args.quiet:
        if iwyu_prefix:
            print(f"Found include-what-you-use via clang-tool-chain")
        else:
            print("Found system include-what-you-use")

    # --- Single file mode ---
    if args.file:
        rel, removals = scan_single_file(args.file)
        if removals:
            print(f"❌ IWYU violations in {rel}:")
            for r in removals:
                print(f"  - {r}")
            return 1
        if not args.quiet:
            print(f"✅ {args.file} passes IWYU")
        return 0

    # --- No board: parallel header scan (fast, ~2 min) ---
    if not args.board:
        violations = scan_fl_headers(quiet=args.quiet)
        if args.json:
            print(json.dumps(violations, indent=2))
        if violations:
            if not args.json:
                print(f"\n❌ {len(violations)} files with IWYU violations:")
                for file, removals in sorted(violations.items()):
                    print(f"\n  {file}:")
                    for r in removals:
                        print(f"    - {r}")
            return 1
        if not args.quiet:
            print("✅ All src/fl/ headers pass IWYU")
        return 0

    # --- Board mode: PlatformIO ---
    build = _PROJECT_ROOT / ".build"
    if not build.exists():
        print(f"Build directory {build} not found")
        print("Run a compilation first: ./compile [board] --examples [example]")
        return 1

    board_dir = build / args.board
    if not board_dir.exists():
        print(f"Board {args.board} not found in {build}")
        print("Available boards:")
        for d in build.iterdir():
            if d.is_dir():
                print(f"  {d.name}")
        return 1

    project_dir = find_platformio_project_dir(board_dir)
    if not project_dir:
        print(f"No platformio.ini found in {board_dir} or its subdirectories")
        print(f"Try running: ./compile {args.board} --examples Blink")
        return 1

    result_code = run_iwyu_on_platformio_project(project_dir, args)

    if args.fix and result_code == 0:
        src_dir = project_dir / "src"
        if src_dir.exists():
            fix_result = apply_iwyu_fixes(src_dir)
            if fix_result != 0:
                result_code = fix_result

    return result_code


if __name__ == "__main__":
    sys.exit(main())
