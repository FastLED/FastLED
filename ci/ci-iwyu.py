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

from ci.iwyu_cache import IWYUCache, compute_source_tree_hash
from ci.util.fbuild_adapter import get_compile_commands
from ci.util.fbuild_compiledb import was_compiled_with_fbuild


# ---------------------------------------------------------------------------
# Compiler args key (used for cache keying — change triggers full re-scan)
# ---------------------------------------------------------------------------

_COMPILER_ARGS_KEY = "|".join(
    [
        "-std=gnu++11",
        "-DSTUB_PLATFORM",
        "-DARDUINO=10808",
        "-DFASTLED_USE_STUB_ARDUINO",
        "-DFASTLED_STUB_IMPL",
        "-DFASTLED_TESTING",
        "-DFASTLED_UNIT_TEST=1",
    ]
)


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
        "clang-tool-chain-cpp",
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
        "clang-tool-chain-cpp",
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


def scan_fl_headers(
    quiet: bool = False, cache: IWYUCache | None = None
) -> dict[str, list[str]]:
    """Scan all src/fl/ headers in parallel and return {file: [violations]}.

    Args:
        quiet: Suppress progress output.
        cache: Optional IWYUCache for per-file result caching.
    """
    fl_dir = _PROJECT_ROOT / "src" / "fl"
    files: list[Path] = []
    for ext in ("*.h", "*.hpp"):
        files.extend(fl_dir.rglob(ext))
    files = [f for f in files if not str(f).endswith(".cpp.hpp")]
    files = sorted(files)

    results: dict[str, list[str]] = {}

    # --- Phase 0: compute source tree hash for dependency-aware caching ---
    if cache is not None:
        tree_hash = compute_source_tree_hash(files)
        cache.set_source_tree_hash(tree_hash)

    # --- Phase 1: check cache for each file ---
    uncached_files: list[Path] = []
    if cache is not None:
        for f in files:
            cached = cache.get(f, _COMPILER_ARGS_KEY)
            if cached is not None:
                # Cache hit — use stored result
                if cached:  # non-empty removals
                    rel = str(f.relative_to(_PROJECT_ROOT)).replace("\\", "/")
                    results[rel] = cached
            else:
                uncached_files.append(f)
    else:
        uncached_files = list(files)

    max_workers = os.cpu_count() or 4
    if not quiet:
        cache_info = ""
        if cache is not None:
            cached_count = len(files) - len(uncached_files)
            cache_info = f" ({cached_count} cached, {len(uncached_files)} to scan)"
        print(
            f"Scanning {len(files)} headers with {max_workers} workers...{cache_info}"
        )
    start = time.time()

    # --- Phase 2: run IWYU on cache misses only ---
    done = 0
    if uncached_files:
        with ProcessPoolExecutor(max_workers=max_workers) as pool:
            futures = {pool.submit(_scan_one_header, str(f)): f for f in uncached_files}
            for future in as_completed(futures):
                done += 1
                if not quiet and done % 50 == 0:
                    elapsed = time.time() - start
                    print(f"  [{done}/{len(uncached_files)}] ({elapsed:.1f}s)...")
                source_file = futures[future]
                rel, removals = future.result()

                # Store result in cache (even clean files, to avoid re-scanning)
                if cache is not None:
                    cache.put(source_file, _COMPILER_ARGS_KEY, removals)

                if rel and removals:
                    results[rel] = removals

    # Flush cache to disk
    if cache is not None:
        cache.save()

    elapsed = time.time() - start
    if not quiet:
        stats = ""
        if cache is not None:
            stats = f" | {cache.stats_summary()}"
        print(
            f"Done: {len(files)} files in {elapsed:.1f}s"
            f" ({len(results)} with violations){stats}"
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
    parser.add_argument(
        "--no-cache",
        action="store_true",
        help="Bypass per-file result cache (force full re-scan)",
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


def _load_src_files_from_compile_db(compile_db: Path, project_root: Path) -> list[str]:
    """Return the list of TUs in ``compile_db`` that live under ``<project_root>/src/``.

    Filters out third-party / framework translation units so we only analyze
    FastLED's own code — matches the scope of the legacy
    ``pio check --src-filters=+<src/>`` invocation (and the identically-named
    helper in ``ci/ci-cppcheck.py``). Without this scoping, ESP32-class
    compile DBs carry thousands of framework TUs whose aggregated argv
    overflows Windows' ``CreateProcess`` 32 KiB command-line limit.

    Per the JSON Compilation Database spec, ``file`` may be absolute OR
    relative to ``directory`` (which is itself absolute); resolve relatives
    against ``directory`` (fallback: ``compile_db.parent``) rather than the
    Python CWD.
    """
    src_root = (project_root / "src").resolve()
    with compile_db.open("r", encoding="utf-8") as fh:
        entries = json.load(fh)
    files: list[str] = []
    for entry in entries:
        raw = entry.get("file")
        if not raw:
            continue
        raw_path = Path(raw)
        if not raw_path.is_absolute():
            directory = entry.get("directory") or compile_db.parent
            raw_path = Path(directory) / raw_path
        try:
            resolved = raw_path.resolve()
        except OSError:
            continue
        try:
            resolved.relative_to(src_root)
        except ValueError:
            continue
        files.append(str(resolved))
    return files


def run_iwyu_against_compile_db(
    compile_db: Path, project_root: Path, args: argparse.Namespace
) -> int:
    """Run IWYU against a ``compile_commands.json`` via ``clang-tool-chain-iwyu-tool``.

    This is the fbuild-backend entry point — it drives IWYU off the compile
    database fbuild emits (``fbuild build -e <env> --target compiledb``),
    bypassing the PlatformIO project-tree assumption entirely. The same
    pattern generalizes to other ``clang-tool-chain-*`` wrappers: adding
    ``clang-tidy``, ``clang-format-check``, or ``clang-query`` is a ~10-LOC
    addition — wire the wrapper name, point it at ``compile_db.parent`` via
    ``-p``, and pass the source file list through
    ``_load_src_files_from_compile_db``.

    Args:
        compile_db: Path to ``compile_commands.json`` (its *parent* directory
            is what ``iwyu-tool -p`` expects).
        project_root: Repo root — used to resolve IWYU mapping files
            (``ci/iwyu/fastled.imp``, ``ci/iwyu/stdlib.imp``) and to filter
            the compile DB down to ``src/`` TUs.
        args: Parsed CLI args (consumes ``--verbose``, ``--max-line-length``,
            ``--mapping-file``).

    Returns:
        Subprocess return code from ``clang-tool-chain-iwyu-tool`` (``0`` if
        there were no ``src/`` TUs in the compile DB — nothing to analyze is
        not a failure).
    """
    files = _load_src_files_from_compile_db(compile_db, project_root)
    if not files:
        print(
            f"No src/ translation units found in {compile_db} — nothing to "
            f"analyze. (Compile DB may cover only framework code for this "
            f"board.)"
        )
        return 0

    mapping_args: list[str] = []
    for name in ("fastled.imp", "stdlib.imp"):
        p = project_root / "ci" / "iwyu" / name
        if p.exists():
            mapping_args.extend(["-Xiwyu", f"--mapping_file={p}"])

    if args.mapping_file:
        for mapping in args.mapping_file:
            mapping_args.extend(["-Xiwyu", f"--mapping_file={mapping}"])

    iwyu_tail: list[str] = [
        "-Xiwyu",
        f"--max_line_length={args.max_line_length}",
        "-Xiwyu",
        "--quoted_includes_first",
        "-Xiwyu",
        "--no_comments",
        "-Xiwyu",
        "--verbose=3" if args.verbose else "--verbose=1",
    ] + mapping_args

    jobs = str(os.cpu_count() or 4)
    cmd: list[str] = [
        "uv",
        "run",
        "clang-tool-chain-iwyu-tool",
        "-p",
        str(compile_db.parent),
        "-j",
        jobs,
        *files,
        "--",
    ] + iwyu_tail

    print(
        f"Running clang-tool-chain-iwyu-tool against {len(files)} src/ files "
        f"using compile DB: {compile_db}"
    )
    try:
        result = subprocess.run(cmd, check=True)
        return result.returncode
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        handle_keyboard_interrupt(ki)
        raise
    except subprocess.CalledProcessError as e:
        print(f"clang-tool-chain-iwyu-tool failed with return code {e.returncode}")
        return e.returncode


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


def fix_iwyu_violations(violations: dict[str, list[str]]) -> int:
    """Auto-fix IWYU violations by removing offending lines.

    Args:
        violations: {relative_file_path: [removal_strings]} where each
            removal string contains '// lines N-N' with the line number.

    Returns:
        0 on success, 1 on any error.
    """
    if not violations:
        print("✅ No violations to fix")
        return 0

    total_fixes = 0
    total_files = 0

    for rel_path, removals in sorted(violations.items()):
        file_path = _PROJECT_ROOT / rel_path
        if not file_path.exists():
            print(f"  ⚠️  {rel_path}: file not found, skipping")
            continue

        # Collect line numbers to remove
        lines_to_remove: set[int] = set()
        for removal in removals:
            line_num = _extract_line_number(removal)
            if line_num is not None:
                lines_to_remove.add(line_num)

        if not lines_to_remove:
            continue

        try:
            original = file_path.read_text(encoding="utf-8", errors="replace")
            original_lines = original.splitlines(keepends=True)
            new_lines: list[str] = []
            removed_count = 0

            for i, line in enumerate(original_lines, start=1):
                if i in lines_to_remove:
                    removed_count += 1
                    # Also skip blank line immediately after removal if it
                    # would leave two consecutive blank lines.
                else:
                    new_lines.append(line)

            # Clean up: collapse runs of 3+ blank lines into 2
            cleaned: list[str] = []
            blank_run = 0
            for line in new_lines:
                if line.strip() == "":
                    blank_run += 1
                    if blank_run <= 2:
                        cleaned.append(line)
                else:
                    blank_run = 0
                    cleaned.append(line)

            file_path.write_text("".join(cleaned), encoding="utf-8")
            total_fixes += removed_count
            total_files += 1
            print(f"  ✅ {rel_path}: removed {removed_count} line(s)")

        except KeyboardInterrupt:
            import _thread

            _thread.interrupt_main()
            raise
        except Exception as e:
            print(f"  ❌ {rel_path}: error: {e}")
            return 1

    print(f"\n🔧 Fixed {total_fixes} violation(s) across {total_files} file(s)")
    return 0


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
        no_cache: bool = getattr(args, "no_cache", False)
        cache = IWYUCache(enabled=not no_cache)

        violations = scan_fl_headers(quiet=args.quiet, cache=cache)
        if args.json:
            print(json.dumps(violations, indent=2))
        if violations:
            if args.fix:
                # Clear cache before fix passes — files are about to change
                cache.clear()
                max_passes = 5
                for pass_num in range(1, max_passes + 1):
                    print(
                        f"\n🔧 Fix pass {pass_num}: fixing {len(violations)} file(s)..."
                    )
                    fix_result = fix_iwyu_violations(violations)
                    if fix_result != 0:
                        return fix_result
                    # Re-scan to check for remaining violations (no cache during fix)
                    print(f"\n🔍 Re-scanning after pass {pass_num}...")
                    violations = scan_fl_headers(quiet=args.quiet)
                    if not violations:
                        print("✅ All violations fixed successfully")
                        return 0
                # Exhausted passes
                print(
                    f"\n⚠️  {len(violations)} files still have violations after"
                    f" {max_passes} passes:"
                )
                for file, removals in sorted(violations.items()):
                    print(f"\n  {file}:")
                    for r in removals:
                        print(f"    - {r}")
                return 1
            if not args.json:
                print(f"\n❌ {len(violations)} files with IWYU violations:")
                for file, removals in sorted(violations.items()):
                    print(f"\n  {file}:")
                    for r in removals:
                        print(f"    - {r}")
            return 1
        if not args.quiet:
            if args.fix:
                print("✅ No violations found — nothing to fix")
            else:
                print("✅ All src/fl/ headers pass IWYU")
        return 0

    # --- Board mode: fbuild (preferred) or PlatformIO (legacy) ---
    build = _PROJECT_ROOT / ".build"
    if not build.exists():
        print(f"Build directory {build} not found")
        print("Run a compilation first: ./compile [board] --examples [example]")
        return 1

    # Resolve board aliases to the canonical fbuild env / PIO build dir name.
    # Must match ci-cppcheck.py so both tools see the same backend signal.
    try:
        from ci.boards import create_board

        canonical_board_name = create_board(args.board).board_name
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        raise RuntimeError(
            f"Failed to resolve board '{args.board}' via ci.boards.create_board: {e}"
        ) from e
    if canonical_board_name != args.board:
        print(
            f"Resolved board '{args.board}' to canonical name '{canonical_board_name}'"
        )

    # fbuild-backed boards (esp32c2, esp32c6, …): drive IWYU off fbuild's
    # compile_commands.json via clang-tool-chain-iwyu-tool. Bypasses the PIO
    # project-tree assumption entirely — no platformio.ini required. See
    # FastLED#2303.
    if was_compiled_with_fbuild(build, canonical_board_name):
        compile_db = get_compile_commands(canonical_board_name, build_root=build)
        if compile_db is None:
            print(
                f"ERROR: could not obtain fbuild compile_commands.json for "
                f"'{canonical_board_name}'. fbuild >= 2.1 should emit this "
                f"via `fbuild build -e {canonical_board_name} --target "
                f"compiledb`; verify the fbuild version and the env name. "
                f"Tracking: FastLED#2303.",
                file=sys.stderr,
            )
            return 1
        result_code = run_iwyu_against_compile_db(compile_db, _PROJECT_ROOT, args)
        if args.fix:
            # IWYU-tool mode emits fixits to stdout only — `fix_includes` is a
            # separate pass. For fbuild boards `--fix` is not wired up yet
            # (tracked upstream); fail loudly rather than silently no-op.
            print(
                "NOTE: --fix is not yet supported for fbuild-backed boards; "
                "violations were reported but not auto-fixed.",
                file=sys.stderr,
            )
        return result_code

    # PIO-backed boards: search both the modern layout
    # (``.build/pio/<board>/``) and the legacy one (``.build/<board>/``).
    # This matches ci-cppcheck.py's multi-layout resolver.
    candidate_dirs = [
        build / "pio" / canonical_board_name,
        build / canonical_board_name,
    ]
    board_dir: Path | None = next((d for d in candidate_dirs if d.exists()), None)
    if board_dir is None:
        print(f"Board {args.board} not found in {build}")
        print("Available boards:")
        for d in build.iterdir():
            if d.is_dir() and d.name != "pio":
                print(f"  {d.name}")
        pio_root = build / "pio"
        if pio_root.exists():
            for d in pio_root.iterdir():
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
