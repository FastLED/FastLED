"""Meson compilation utilities."""

import os
import re
import sys
from collections import deque
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from running_process import RunningProcess

from ci.meson.cache_utils import (
    check_ninja_skip,
    save_ninja_skip_state,
)
from ci.meson.compiler import get_meson_executable
from ci.meson.output import print_banner
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.output_formatter import TimestampFormatter
from ci.util.tee import StreamTee
from ci.util.timestamp_print import ts_print as _ts_print


@dataclass
class CompileResult:
    """Result of a Meson compilation."""

    success: bool
    error_output: str
    suppressed_errors: list[
        str
    ]  # Validation errors suppressed during quiet mode (max 5)

    # NEW: Path to full error log file (if compilation attempted)
    error_log_file: Optional[Path] = None

    # NEW: Extracted error snippet (key error lines with context)
    error_snippet: Optional[str] = None


# Error patterns that indicate stale build state recoverable by reconfiguration
STALE_BUILD_PATTERNS = [
    "file not found",
    "no such file or directory",
    "missing and no known rule to make it",
    "does not exist",
]


def is_stale_build_error(output: str) -> bool:
    """Check if compilation output indicates a stale build state error.

    These errors typically occur when source/header files are renamed or deleted
    but the build system still references the old paths. They are recoverable
    by cleaning stale deps and reconfiguring.
    """
    output_lower = output.lower()
    return any(pattern in output_lower for pattern in STALE_BUILD_PATTERNS)


def _is_compilation_error(line: str) -> bool:
    """Detect if a line contains a compilation error pattern.

    Returns:
        True if the line contains an error indicator
    """
    line_lower = line.lower()
    return any(
        pattern in line_lower
        for pattern in [
            "error:",  # Compiler errors
            "note:",  # Compiler diagnostic notes (CRITICAL for context!)
            "warning:",  # Compiler warnings
            "failed:",  # Build system failures
            "fatal error:",  # Catastrophic failures
            "undefined reference",  # Linker errors
            "no such file",  # Missing headers
            "ld returned 1 exit status",  # Linker failure
        ]
    )


def compile_meson(
    build_dir: Path,
    target: Optional[str] = None,
    check: bool = False,
    quiet: bool = False,
    verbose: bool = False,
    build_mode: Optional[str] = None,
) -> CompileResult:
    """
    Compile using Meson.

    Args:
        build_dir: Meson build directory
        target: Specific target to build (None = all)
        check: Enable IWYU static analysis during compilation (default: False)
        quiet: Suppress banner and progress output (used during target fallback retries)
        verbose: Enable verbose output with section banners
        build_mode: Build mode string for display (e.g., "quick", "debug", "release").
                    If None, derived from build directory name.

    Returns:
        CompileResult with success flag and error_output (empty on success).
    """
    cmd = [get_meson_executable(), "compile", "-C", str(build_dir)]

    # Derive build mode from build directory name if not provided
    if build_mode is None:
        if "meson-quick" in str(build_dir):
            build_mode = "quick"
        elif "meson-debug" in str(build_dir):
            build_mode = "debug"
        elif "meson-release" in str(build_dir):
            build_mode = "release"
        else:
            build_mode = "unknown"

    # NINJA SKIP OPTIMIZATION: If target is up-to-date, skip the full ninja
    # startup (~2-3s) and return immediately.
    #
    # Conditions checked (~20-50ms total):
    #   1. build.ninja mtime unchanged      → no meson reconfiguration
    #   2. libfastled.a mtime unchanged     → no src/ code changes (proxy)
    #   3. tests/ max source file mtime ≤ saved → no test source modifications
    #   4. Output DLL/exe mtime unchanged   → not rebuilt externally or deleted
    #
    # Only applied to specific targets (not all-build) and non-IWYU mode.
    # Overhead: ~20-50ms (os.scandir over tests/ source files). Savings: ~2-3s.
    if target and not check and check_ninja_skip(build_dir, target):
        if not quiet:
            _ts_print(f"[BUILD] ✓ Target up-to-date (ninja skipped)")
        return CompileResult(
            success=True,
            error_output="",
            suppressed_errors=[],
            error_log_file=None,
        )

    if target:
        cmd.append(target)
        if not quiet:
            print_banner("Compile", "📦", verbose=verbose)
            print(f"Compiling: {target}")
    else:
        if not quiet:
            print_banner("Compile", "📦", verbose=verbose)
            print("Compiling all targets...")

    # Show build stage banner (unless in quiet mode for fallback retries)
    # WARNING: Do NOT re-enable quiet mode here! quiet=False is required for:
    #   1. Build progress inspection (users need to see what's being built)
    #   2. Timing information (helps diagnose slow builds)
    #   3. Cache status reporting (shows when artifacts are up-to-date)
    if not quiet:
        _ts_print(f"[BUILD] Building FastLED engine ({build_mode} mode)...")

    # Create compile-errors directory for error logging
    compile_errors_dir = build_dir.parent / "compile-errors"
    compile_errors_dir.mkdir(exist_ok=True)

    # Generate log filename
    test_name_slug = target.replace("/", "_") if target else "all"
    error_log_path = compile_errors_dir / f"{test_name_slug}.log"

    # Inject IWYU wrapper by modifying build.ninja when check mode is enabled
    # This avoids meson setup probe issues while still running IWYU during compilation
    env = os.environ.copy()
    if check:
        # Use custom IWYU wrapper (fixes clang-tool-chain-iwyu argument forwarding issues)
        iwyu_wrapper_path = Path(__file__).parent.parent / "iwyu_wrapper.py"
        if iwyu_wrapper_path.exists():
            # Use Python to invoke our custom wrapper
            python_exe = sys.executable
            iwyu_wrapper = f'"{python_exe}" "{iwyu_wrapper_path}"'
            _ts_print(f"[MESON] Using custom IWYU wrapper: {iwyu_wrapper_path.name}")
        else:
            iwyu_wrapper = None
            _ts_print(f"[MESON] ⚠️ Custom IWYU wrapper not found: {iwyu_wrapper_path}")

        if iwyu_wrapper:
            # Modify build.ninja to wrap C++ compiler with IWYU
            # This is done after meson setup (which uses normal compiler) but before ninja runs
            build_ninja_path = build_dir / "build.ninja"
            if build_ninja_path.exists():
                try:
                    # Read build.ninja
                    build_ninja_content = build_ninja_path.read_text(encoding="utf-8")

                    # Replace C++ compiler command in cpp_COMPILER rule with IWYU wrapper
                    # Pattern matches EITHER:
                    #   1. Single executable: command = "path\to\clang++.EXE" $ARGS
                    #   2. Python wrapper: command = "python.exe" "wrapper.py" $ARGS

                    # Flexible pattern that captures the entire compiler command before $ARGS
                    # Group 1: "rule cpp_COMPILER\n command = "
                    # Group 2: The entire compiler command (could be single or multiple quoted strings)
                    # Group 3: " $ARGS" and rest
                    pattern = r'(rule cpp_COMPILER\s+command = )((?:"[^"]*"\s*)+)(\$ARGS.*?)(?=\n\s*(?:deps|$))'

                    def replace_compiler(match: re.Match[str]) -> str:
                        prefix = match.group(1)
                        compiler_cmd = match.group(2).strip()
                        args_and_rest = match.group(3)

                        # Escape backslashes in paths for re.sub
                        iwyu_escaped = iwyu_wrapper.replace("\\", "\\\\")

                        # Wrap the entire compiler command with IWYU
                        # iwyu_wrapper already contains proper quoting (e.g., "python.exe" "script.py")
                        # Format: python.exe script.py -- original_compiler_command $ARGS...
                        return (
                            f"{prefix}{iwyu_escaped} -- {compiler_cmd} {args_and_rest}"
                        )

                    modified_content = re.sub(
                        pattern, replace_compiler, build_ninja_content, flags=re.DOTALL
                    )

                    # Also strip out PCH flags from all ARGS variables since IWYU doesn't support PCH
                    # Pattern matches ARGS lines containing PCH flags
                    # Example: ARGS = ... -include-pch "path.pch" -Werror=invalid-pch -fpch-validate-input-files-content ...
                    # We need to remove these three flags together
                    pch_pattern = r'(-include-pch\s+"[^"]+"\s+-Werror=invalid-pch\s+-fpch-validate-input-files-content\s+)'
                    modified_content = re.sub(
                        pch_pattern, "", modified_content, flags=re.MULTILINE
                    )

                    if modified_content != build_ninja_content:
                        build_ninja_path.write_text(modified_content, encoding="utf-8")
                        _ts_print(
                            f"[MESON] ✅ IWYU wrapper injected into build.ninja: {iwyu_wrapper}"
                        )
                        _ts_print(
                            f"[MESON] ✅ PCH flags removed from all compile commands (IWYU does not support PCH)"
                        )
                    else:
                        _ts_print(
                            f"[MESON] ⚠️ Failed to modify build.ninja (content unchanged)"
                        )
                        _ts_print(
                            f"[MESON] DEBUG: Looking for cpp_COMPILER rule in build.ninja"
                        )
                except (OSError, IOError) as e:
                    _ts_print(f"[MESON] ⚠️ Failed to modify build.ninja: {e}")
            else:
                _ts_print(f"[MESON] ⚠️ build.ninja not found, skipping IWYU injection")
        else:
            _ts_print(f"[MESON] ⚠️ IWYU wrapper not found, skipping static analysis")

    # Create tee for error log capture (stdout + stderr merged)
    stderr_tee = StreamTee(error_log_path, echo=False)
    last_error_lines: list[str] = []

    try:
        # Use RunningProcess for streaming output
        # Pass modified environment with IWYU wrapper if check=True
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for compilation
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=env,  # Pass environment with IWYU wrapper
            output_formatter=TimestampFormatter(),
        )

        # Pattern to detect linking operations (same as streaming path)
        # Example: "[142/143] Linking target tests/test_foo.exe"
        # Note: TimestampFormatter already adds timestamp, so line comes with that prefix
        # We need to match after the TimestampFormatter's output (e.g., "7.11 3.14 [1/1] Linking...")
        link_pattern = re.compile(
            r"\[\d+/\d+\]\s+Linking (?:CXX executable|target)\s+(.+)$"
        )

        # Pattern to detect static library archiving
        archive_pattern = re.compile(r"\[\d+/\d+\]\s+Linking static target\s+(.+)$")

        # Pattern to detect PCH compilation
        # Example: "[1/1] Generating tests/test_pch with a custom command"
        pch_pattern = re.compile(r"\[\d+/\d+\]\s+Generating\s+\S*test_pch\b")

        # Validation error list for suppressed errors during quiet mode (max 5 entries)
        suppressed_errors: list[str] = []

        # Track build stages for progress reporting
        shown_tests_stage = False  # Track if we've shown "Building tests..." message

        # Track what we've seen during build for cache status reporting
        seen_libfastled = False
        seen_libplatforms_shared = False
        seen_libcrash_handler = False
        seen_pch = False
        seen_any_test = False

        # Stream output line by line to rewrite Ninja paths
        # In quiet mode or non-verbose mode, suppress all output (errors shown via error filter)
        # In verbose mode, show full compilation output for detailed debugging
        with proc.line_iter(timeout=None) as it:
            for line in it:
                # Write to error log (captures both stdout and stderr)
                stderr_tee.write_line(line)

                # Track errors for smart detection
                if _is_compilation_error(line):
                    last_error_lines.append(line)
                    if len(last_error_lines) > 50:
                        last_error_lines.pop(0)

                # Filter out noisy Meson/Ninja INFO lines that clutter output
                # Note: TimestampFormatter may add timestamp prefix, so check contains not startswith
                stripped = line.strip()
                if " INFO:" in stripped or stripped.startswith("INFO:"):
                    continue  # Skip Meson INFO lines
                if "Entering directory" in stripped:
                    continue  # Skip Ninja directory change messages
                if "calculating backend command" in stripped.lower():
                    continue  # Skip Meson backend calculation message
                # Note: "Can't invoke target" errors are NOT filtered here.
                # They are preserved in output for caller diagnostics (e.g., ambiguous target).
                # The caller controls whether to display them via the quiet parameter.
                if "ninja: no work to do" in stripped.lower():
                    continue  # Skip no-work ninja message (already up to date)

                # Check for key build milestones (show even in quiet/non-verbose mode)
                is_key_milestone = False
                custom_message_shown = False

                # Check for library archiving
                archive_match = archive_pattern.search(stripped)
                if archive_match:
                    if any(
                        lib in stripped
                        for lib in [
                            "libfastled",
                            "libplatforms_shared",
                            "libcrash_handler",
                        ]
                    ):
                        is_key_milestone = True
                        custom_message_shown = True
                        # Show library path when it finishes building
                        lib_match = re.search(
                            r"(ci[\\/]meson[\\/]native[\\/]lib\S+\.a)", stripped
                        )
                        if lib_match:
                            rel_path = lib_match.group(1)
                            full_path = build_dir / rel_path
                            try:
                                display_path = full_path.relative_to(Path.cwd())
                            except ValueError:
                                display_path = full_path
                            _ts_print(f"[BUILD] ✓ Core library: {display_path}")

                            # Track which libraries we've seen for cache status reporting
                            if "libfastled" in stripped:
                                seen_libfastled = True
                            if "libplatforms_shared" in stripped:
                                seen_libplatforms_shared = True
                            if "libcrash_handler" in stripped:
                                seen_libcrash_handler = True

                # Check for PCH compilation
                pch_match = pch_pattern.search(stripped)
                if pch_match:
                    is_key_milestone = True
                    custom_message_shown = True
                    seen_pch = True  # Track that we've seen PCH compilation
                    # Extract PCH path from the line
                    # Format: "[1/1] Generating tests/test_pch with a custom command"
                    pch_path_match = re.search(
                        r"Generating\s+(\S+test_pch)\b", stripped
                    )
                    if pch_path_match:
                        rel_path = (
                            pch_path_match.group(1) + ".h.pch"
                        )  # Add extension for display
                        full_path = build_dir / rel_path
                        try:
                            display_path = full_path.relative_to(Path.cwd())
                        except ValueError:
                            display_path = full_path
                        _ts_print(f"[BUILD] ✓ Precompiled header: {display_path}")

                # Check for test/example linking
                test_link_match = link_pattern.search(stripped)
                if test_link_match:
                    if (
                        "tests/" in stripped
                        or "tests\\" in stripped
                        or "examples/" in stripped
                        or "examples\\" in stripped
                    ):
                        # Exclude test infrastructure (runner.exe is not a test)
                        # Extract test name from the stripped line
                        test_name_match = re.search(
                            r"[\\/](runner|test_runner|example_runner)\.", stripped
                        )
                        if not test_name_match:
                            seen_any_test = (
                                True  # Track that we've seen at least one test
                            )
                            # Show "Building tests..." stage message on first test
                            if not shown_tests_stage:
                                _ts_print("[BUILD] Building tests...")
                                shown_tests_stage = True
                            is_key_milestone = True

                # Suppress output unless verbose mode enabled
                # In quiet or non-verbose mode: still print error/failure lines to stderr
                # In verbose mode: show full compilation output for detailed debugging
                # WARNING: The milestone detection below (is_key_milestone) requires quiet=False!
                #   Do NOT suppress milestone messages - they provide essential build progress feedback.
                if quiet or not verbose:
                    # Print error and failure lines so compiler errors are never silently swallowed
                    # Exception: Store "Can't invoke target" errors in validation list (shown on self-healing)
                    stripped_lower = stripped.lower()
                    if "error:" in stripped_lower or "FAILED:" in stripped:
                        # In quiet mode, store "Can't invoke target" errors for later diagnostic use
                        # These are shown only when self-healing occurs (all targets fail)
                        if quiet and "can't invoke target" in stripped_lower:
                            # Cap at 5 errors to prevent list bloat
                            if len(suppressed_errors) < 5:
                                suppressed_errors.append(stripped)
                            continue
                        _ts_print(line, file=sys.stderr)
                        continue

                    # Show key milestones even in quiet/non-verbose mode
                    # But skip if we already printed a custom message (like library path)
                    if is_key_milestone and not custom_message_shown:
                        _ts_print(f"[BUILD] {line}")

                    continue

                # Rewrite Ninja paths to show full build-relative paths for clarity
                # Ninja outputs paths relative to build directory (e.g., "tests/fx_frame.exe")
                # Users expect to see full paths (e.g., ".build/meson-quick/tests/fx_frame.exe")
                display_line = line
                link_match = link_pattern.search(line)
                if link_match:
                    rel_path = link_match.group(1)
                    # Convert build-relative path to project-relative path
                    full_path = build_dir / rel_path
                    try:
                        # Make path relative to project root for cleaner display
                        display_path = full_path.relative_to(Path.cwd())
                        # Rewrite the line with full path
                        display_line = line.replace(rel_path, str(display_path))
                    except ValueError:
                        # If path is outside project, show absolute path
                        display_line = line.replace(rel_path, str(full_path))

                # Echo the (possibly rewritten) line (only in verbose mode)
                _ts_print(display_line)

        returncode = proc.wait()

        # Write standardized footer to error log
        stderr_tee.write_footer(returncode)
        stderr_tee.close()

        # Save last error snippet if compilation failed
        if returncode != 0 and last_error_lines:
            last_error_path = (
                build_dir.parent / "meson-state" / "last-compilation-error.txt"
            )
            last_error_path.parent.mkdir(exist_ok=True)

            with open(last_error_path, "w", encoding="utf-8") as f:
                f.write(f"# Target: {target or 'all'}\n")
                f.write(f"# Full log: {error_log_path}\n\n")
                f.write("--- Error Context ---\n\n")
                f.write("\n".join(last_error_lines))

        # Check for Meson version incompatibility
        # This appears as "Build directory has been generated with Meson version X.Y.Z, which is incompatible with the current version A.B.C"
        # When detected, suggest reconfiguration (setup_meson_build handles auto-healing)
        output = proc.stdout  # RunningProcess combines stdout and stderr
        if (
            "build directory has been generated with meson version" in output.lower()
            and "incompatible" in output.lower()
        ):
            _ts_print(
                "[MESON] ⚠️  Detected Meson version incompatibility",
                file=sys.stderr,
            )
            _ts_print(
                "[MESON] 💡 Solution: Run 'uv run test.py --clean' to force reconfiguration",
                file=sys.stderr,
            )
            _ts_print(
                "[MESON] 💡 Or: setup_meson_build() will auto-heal on next run",
                file=sys.stderr,
            )
            # Return failure - caller should trigger reconfiguration
            return CompileResult(
                success=False,
                error_output=output,
                suppressed_errors=suppressed_errors,
                error_log_file=error_log_path,
            )

        # Check for Ninja dependency database corruption
        # This appears as "ninja: warning: premature end of file; recovering"
        # When detected, automatically repair the .ninja_deps file
        if "premature end of file" in output.lower():
            _ts_print(
                "[MESON] ⚠️  Detected corrupted Ninja dependency database (.ninja_deps)",
                file=sys.stderr,
            )
            _ts_print(
                "[MESON] 🔧 Auto-repairing: Running ninja -t recompact...",
                file=sys.stderr,
            )

            # Run ninja -t recompact to repair the dependency database
            try:
                repair_proc = RunningProcess(
                    ["ninja", "-C", str(build_dir), "-t", "recompact"],
                    timeout=60,
                    auto_run=True,
                    check=False,
                    env=os.environ.copy(),
                    output_formatter=TimestampFormatter(),
                )
                repair_returncode = repair_proc.wait(echo=False)

                if repair_returncode == 0:
                    _ts_print(
                        "[MESON] ✓ Dependency database repaired successfully",
                        file=sys.stderr,
                    )
                    _ts_print(
                        "[MESON] 💡 Next build should be fast (incremental)",
                        file=sys.stderr,
                    )
                else:
                    _ts_print(
                        "[MESON] ⚠️  Repair failed, but continuing anyway",
                        file=sys.stderr,
                    )
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as repair_error:
                _ts_print(
                    f"[MESON] ⚠️  Repair failed with exception: {repair_error}",
                    file=sys.stderr,
                )

        if returncode != 0:
            # In quiet mode, don't print failure messages (fallback retries handle this)
            if not quiet:
                _ts_print(
                    f"[MESON] Compilation failed with return code {returncode}",
                    file=sys.stderr,
                )

                # Show error context from compilation output
                error_filter: Callable[[str], None] = _create_error_context_filter(
                    context_lines=20
                )
                output_lines = output.splitlines()
                for line in output_lines:
                    error_filter(line)

                # Check for stale build cache error (missing files)
                if "missing and no known rule to make it" in output.lower():
                    _ts_print(
                        "[MESON] ⚠️  ERROR: Build cache references missing source files",
                        file=sys.stderr,
                    )
                    _ts_print(
                        "[MESON] 💡 TIP: Source files may have been deleted. Run with --clean to rebuild.",
                        file=sys.stderr,
                    )
                    _ts_print(
                        "[MESON] 💡 NOTE: Future builds should auto-detect this and reconfigure.",
                        file=sys.stderr,
                    )

            return CompileResult(
                success=False,
                error_output=output,
                suppressed_errors=suppressed_errors,
                error_log_file=error_log_path,
            )

        # Report cached artifacts (things we didn't see being built)
        # Only report if we saw at least one artifact being built (otherwise it's a no-op build)
        # Note: Show cached messages even in quiet mode - they provide useful build context
        if (
            seen_libfastled
            or seen_libplatforms_shared
            or seen_libcrash_handler
            or seen_pch
            or seen_any_test
        ):
            # Report cached core libraries
            cached_libs: list[str] = []
            if not seen_libfastled:
                cached_libs.append("libfastled.a")
            if not seen_libplatforms_shared:
                cached_libs.append("libplatforms_shared.a")
            if not seen_libcrash_handler:
                cached_libs.append("libcrash_handler.a")

            if cached_libs:
                _ts_print(
                    f"[BUILD] ✓ Core libraries: up-to-date (cached: {', '.join(cached_libs)})"
                )

            # Report cached PCH
            if not seen_pch:
                _ts_print(f"[BUILD] ✓ Precompiled header: up-to-date (cached)")

        # Don't print "Compilation successful" - the transition to Running phase implies success
        # This was previously conditional on quiet mode, but it's always redundant

        # Save ninja skip state so the next run can bypass ninja for this target.
        # Only for specific targets (not all-build) and non-IWYU mode.
        if target and not check:
            save_ninja_skip_state(build_dir, target)

        return CompileResult(
            success=True,
            error_output="",
            suppressed_errors=suppressed_errors,
            error_log_file=None,  # Success - no error log needed
        )

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(f"[MESON] Compilation failed with exception: {e}", file=sys.stderr)
        return CompileResult(
            success=False,
            error_output=str(e),
            suppressed_errors=[],
            error_log_file=None,
        )


def _create_error_context_filter(
    context_lines: int = 20, max_unique_errors: int = 3
) -> Callable[[str], None]:
    """
    Create a filter function that only shows output when errors are detected.

    The filter accumulates output in a circular buffer. When an error pattern
    is detected, it outputs the buffered context (lines before the error) plus
    the error line, and then continues outputting for context_lines after.

    This version is smart about duplicate errors - it shows the first few instances
    of each unique error, then summarizes the rest to prevent output truncation.

    Args:
        context_lines: Number of lines to show before and after error detection
        max_unique_errors: Maximum unique errors to show before summarizing.
                          Use 0 for unlimited (show all errors).

    Returns:
        Filter function that takes a line and returns None (consumes line)
    """
    # Context sizing: use caller's requested amount (default 20).
    # The max_unique_errors limit already prevents output explosion when many
    # errors occur; capping context_lines to 5 was hiding short compiler errors.

    # Circular buffer for context before errors
    pre_error_buffer: deque[str] = deque(maxlen=context_lines)

    # Counter for lines after error (to show context after error)
    post_error_lines = 0

    # Track if we've seen any errors
    error_detected = False

    # Track seen error signatures to avoid showing identical errors repeatedly
    seen_errors: set[str] = set()
    error_count = 0
    max_unique_errors_to_show = max_unique_errors  # 0 means show all

    # Track file context for better error reporting
    current_file: str | None = None

    # Error patterns (case-insensitive)
    error_patterns = [
        "error:",
        "failed",
        "failure",
        "FAILED",
        "ERROR",
        ": fatal",
        "assertion",
        "segmentation fault",
        "core dumped",
    ]

    def filter_line(line: str) -> None:
        """Process a line and print it if it's part of error context."""
        nonlocal post_error_lines, error_detected, error_count, current_file

        # Skip extremely long lines (>800 chars) - these are almost always compilation commands with no diagnostic value
        # Typical error lines are 50-200 chars; compilation commands are 1000-2000+ chars
        if len(line) > 800:
            return

        # Also skip lines that look like compilation commands (red ANSI code + quotes + flags)
        if "[91m" in line and '"' in line and ("-I" in line or "-D" in line):
            return

        # Track current file being compiled for context
        if "Compiling" in line or "Generating" in line:
            # Extract filename from compilation lines
            file_match = re.search(r"([^\\/]+\.(cpp|h|ino))", line)
            if file_match:
                current_file = file_match.group(1)

        # Check if this line contains an error pattern
        line_lower = line.lower()
        is_error_line = any(pattern.lower() in line_lower for pattern in error_patterns)

        if is_error_line:
            error_count += 1

            # Extract error signature (first 100 chars of error for deduplication)
            error_sig = line_lower[:100]

            # Check if we've seen this exact error before
            is_new_error = error_sig not in seen_errors
            if is_new_error:
                seen_errors.add(error_sig)

            # Only show first N unique errors to prevent truncation
            # max_unique_errors_to_show == 0 means unlimited (show all)
            should_show = (
                max_unique_errors_to_show == 0
                or len(seen_errors) <= max_unique_errors_to_show
            )

            if should_show:
                # Error detected! Output all buffered pre-context
                if not error_detected:
                    # First error - show header and pre-context
                    _ts_print(
                        "\n[MESON] ⚠️  Compilation errors detected - showing diagnostic output:"
                    )
                    _ts_print("-" * 80)
                    error_detected = True

                # Show which file this error is from if we have context
                if current_file and is_new_error:
                    _ts_print(f"\n[ERROR in {current_file}]")

                # Output buffered pre-context
                for buffered_line in pre_error_buffer:
                    _ts_print(buffered_line)

                # Output this error line with red color highlighting
                _ts_print(f"\033[91m{line}\033[0m")

                # Start counting post-error lines
                post_error_lines = context_lines

            elif (
                is_new_error
                and max_unique_errors_to_show > 0
                and len(seen_errors) == max_unique_errors_to_show + 1
            ):
                # First suppressed error - show summary (only when limit is active)
                _ts_print("\n" + "=" * 80)
                _ts_print(
                    f"[MESON] 📋 Showing first {max_unique_errors_to_show} unique errors - suppressing duplicates to prevent truncation"
                )
                _ts_print(
                    f"[MESON] 💡 Run with --verbose flag to see all compilation errors"
                )
                _ts_print("=" * 80)

            # Don't buffer this line (already handled)
            return

        if post_error_lines > 0:
            # We're in the post-error context window
            # Skip massive compilation command lines (they add 1000+ chars with no diagnostic value)
            if not (
                line.strip().startswith('"')
                and ("-I" in line or "-D" in line)
                and len(line) > 500
            ):
                _ts_print(line)
            post_error_lines -= 1
            return

        # No error detected yet - buffer this line for potential future context
        # Don't print anything - just accumulate in buffer
        # Skip massive compilation command lines from the buffer (they're noise in error context)
        # These lines typically start with a quoted path and contain many -I/-D flags
        if not (
            line.strip().startswith('"')
            and ("-I" in line or "-D" in line)
            and len(line) > 500
        ):
            pre_error_buffer.append(line)

    return filter_line
