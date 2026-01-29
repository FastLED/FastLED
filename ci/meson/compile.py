"""Meson compilation utilities."""

import os
import re
import sys
import time
from collections import deque
from collections.abc import Callable
from pathlib import Path
from typing import Optional

from running_process import RunningProcess

from ci.meson.compiler import get_meson_executable
from ci.meson.output import _print_banner
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


def compile_meson(
    build_dir: Path,
    target: Optional[str] = None,
    check: bool = False,
    quiet: bool = False,
    verbose: bool = False,
) -> bool:
    """
    Compile using Meson.

    Args:
        build_dir: Meson build directory
        target: Specific target to build (None = all)
        check: Enable IWYU static analysis during compilation (default: False)
        quiet: Suppress banner and progress output (used during target fallback retries)
        verbose: Enable verbose output with section banners

    Returns:
        True if compilation successful, False otherwise
    """
    cmd = [get_meson_executable(), "compile", "-C", str(build_dir)]

    if target:
        cmd.append(target)
        if not quiet:
            _print_banner("Compile", "📦", verbose=verbose)
            print(f"Compiling: {target}")
    else:
        _print_banner("Compile", "📦", verbose=verbose)
        print("Compiling all targets...")

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

        # Stream output line by line to rewrite Ninja paths
        # In quiet mode or non-verbose mode, suppress all output (errors shown via error filter)
        # In verbose mode, show full compilation output for detailed debugging
        with proc.line_iter(timeout=None) as it:
            for line in it:
                # Filter out noisy Meson/Ninja INFO lines that clutter output
                # Note: TimestampFormatter may add timestamp prefix, so check contains not startswith
                stripped = line.strip()
                if " INFO:" in stripped or stripped.startswith("INFO:"):
                    continue  # Skip Meson INFO lines
                if "Entering directory" in stripped:
                    continue  # Skip Ninja directory change messages
                if "calculating backend command" in stripped.lower():
                    continue  # Skip Meson backend calculation message
                if "ERROR: Can't invoke target" in stripped:
                    continue  # Skip target-not-found errors (handled by fallback)
                if "ninja: no work to do" in stripped.lower():
                    continue  # Skip no-work ninja message (already up to date)

                # Suppress output unless verbose mode enabled
                # In quiet or non-verbose mode: only collect output for error checking, don't print
                # In verbose mode: show full compilation output for detailed debugging
                if quiet or not verbose:
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

        # Check for Ninja dependency database corruption
        # This appears as "ninja: warning: premature end of file; recovering"
        # When detected, automatically repair the .ninja_deps file
        output = proc.stdout  # RunningProcess combines stdout and stderr
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

            return False

        # Don't print "Compilation successful" - the transition to Running phase implies success
        # This was previously conditional on quiet mode, but it's always redundant
        return True

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(f"[MESON] Compilation failed with exception: {e}", file=sys.stderr)
        return False


def _create_error_context_filter(context_lines: int = 20) -> Callable[[str], None]:
    """
    Create a filter function that only shows output when errors are detected.

    The filter accumulates output in a circular buffer. When an error pattern
    is detected, it outputs the buffered context (lines before the error) plus
    the error line, and then continues outputting for context_lines after.

    This version is smart about duplicate errors - it shows the first few instances
    of each unique error, then summarizes the rest to prevent output truncation.

    Args:
        context_lines: Number of lines to show before and after error detection

    Returns:
        Filter function that takes a line and returns None (consumes line)
    """
    # Smart context sizing:
    # - Reduced to 5 lines for Bash tool 30KB output limit
    # - When many errors occur (e.g., 80+ examples failing), keeping output minimal
    #   prevents the Bash tool from truncating with "... [N chars truncated] ..."
    # - 5 lines is enough to show file location + error + immediate context
    if context_lines > 5:
        context_lines = 5  # Minimal context to fit under 30KB Bash tool limit

    # Circular buffer for context before errors
    pre_error_buffer: deque[str] = deque(maxlen=context_lines)

    # Counter for lines after error (to show context after error)
    post_error_lines = 0

    # Track if we've seen any errors
    error_detected = False

    # Track seen error signatures to avoid showing identical errors repeatedly
    seen_errors: set[str] = set()
    error_count = 0
    max_unique_errors_to_show = (
        3  # Show first 3 unique errors to stay under 30KB Bash limit
    )

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
            should_show = len(seen_errors) <= max_unique_errors_to_show

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

            elif is_new_error and len(seen_errors) == max_unique_errors_to_show + 1:
                # First suppressed error - show summary
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
