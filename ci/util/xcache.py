#!/usr/bin/env python3
"""
xcache.py - Enhanced sccache wrapper with response file support

This trampoline wrapper handles the ESP32S3 sccache problem where long command lines
use response files (@tmpfile.tmp) that sccache doesn't understand. It creates
temporary wrapper scripts that act as compiler aliases.

WARNING: Never use sys.stdout.flush() in this file!
It causes blocking issues on Windows that hang subprocess processes.
Python's default buffering behavior works correctly across platforms.

Usage:
    xcache.py <compiler> [args...]

When xcache detects response file arguments (@file.tmp), it:
1. Creates a temporary wrapper script that acts as the compiler
2. The wrapper script internally calls: sccache <actual_compiler> "$@"
3. Executes the original command with response files intact
4. The system handles response file expansion normally

This solves the ESP32S3 issue where commands are too long for direct execution
but need caching support.
"""

import os
import shutil
import stat
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


# os.environ["XCACHE_DEBUG"] = "1"


@dataclass
class XCacheConfig:
    """Configuration for xcache wrapper."""

    sccache_path: str
    compiler_path: str
    temp_dir: Path
    debug: bool = False


def find_sccache() -> Optional[str]:
    """Find sccache executable in PATH."""
    sccache_path = shutil.which("sccache")
    if sccache_path:
        return sccache_path

    # Check common locations only if not found in PATH
    common_paths = [
        "/usr/local/bin/sccache",
        "/usr/bin/sccache",
        "/opt/local/bin/sccache",
        os.path.expanduser("~/.cargo/bin/sccache"),
    ]

    for path in common_paths:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path

    return None


def detect_response_files(args: List[str]) -> List[str]:
    """Detect response file arguments (@file.tmp) in command line."""
    response_files: List[str] = []
    for arg in args:
        if arg.startswith("@") and len(arg) > 1:
            response_file = arg[1:]
            if os.path.isfile(response_file):
                response_files.append(response_file)
    return response_files


def create_compiler_wrapper_script(config: XCacheConfig) -> Path:
    """Create temporary wrapper script that acts as a compiler alias."""

    # Determine platform-appropriate script type
    is_windows = os.name == "nt"
    script_suffix = ".bat" if is_windows else ".sh"

    # Create temporary script file
    script_fd, script_path = tempfile.mkstemp(
        suffix=script_suffix, prefix="xcache_compiler_", dir=config.temp_dir
    )

    try:
        if is_windows:
            # Create Windows batch file
            wrapper_content = f'''@echo off
REM Temporary xcache compiler wrapper script
REM Acts as alias for: sccache <actual_compiler>

if /i "{config.debug}"=="true" (
    echo XCACHE: Wrapper executing: "{config.sccache_path}" "{config.compiler_path}" %* >&2
)

REM Execute sccache with the actual compiler and all arguments (including response files)
"{config.sccache_path}" "{config.compiler_path}" %*
'''
        else:
            # Create Unix shell script
            wrapper_content = f'''#!/bin/bash
# Temporary xcache compiler wrapper script
# Acts as alias for: sccache <actual_compiler>

if [ "{config.debug}" = "true" ]; then
    echo "XCACHE: Wrapper executing: {config.sccache_path} {config.compiler_path} $@" >&2
fi

# Execute sccache with the actual compiler and all arguments (including response files)
exec "{config.sccache_path}" "{config.compiler_path}" "$@"
'''

        # Write wrapper script
        with os.fdopen(script_fd, "w", encoding="utf-8") as f:
            f.write(wrapper_content)

        # Make script executable (Unix only)
        script_path_obj = Path(script_path)
        if not is_windows:
            script_path_obj.chmod(script_path_obj.stat().st_mode | stat.S_IEXEC)

        return script_path_obj

    except Exception:
        # Clean up on error
        try:
            os.close(script_fd)
            os.unlink(script_path)
        except Exception:
            pass
        raise


def execute_direct(config: XCacheConfig, args: List[str]) -> int:
    """Execute sccache directly without response file handling."""
    command = [config.sccache_path, config.compiler_path] + args

    if config.debug:
        print(f"XCACHE: Direct execution: {' '.join(command)}", file=sys.stderr)

    try:
        # Use Popen to manually pump stdout/stderr and prevent hanging
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout as requested
            text=True,
            bufsize=1,  # Line buffered
            universal_newlines=True,
        )

        # Manually pump output until process finishes
        while True:
            output = process.stdout.readline()
            if output == "" and process.poll() is not None:
                break
            if output:
                print(output.rstrip())  # Print to stdout, remove trailing newlines

        # Wait for process to complete and get return code
        return_code = process.wait()
        return return_code

    except FileNotFoundError as e:
        print(f"XCACHE ERROR: Command not found: {e}")
        return 127
    except Exception as e:
        print(f"XCACHE ERROR: Execution failed: {e}")
        return 1


def execute_with_wrapper(config: XCacheConfig, args: List[str]) -> int:
    """Execute using wrapper script for response file handling."""
    response_files = detect_response_files(args)

    if not response_files:
        # No response files, use direct execution
        return execute_direct(config, args)

    if config.debug:
        print(f"XCACHE: Detected response files: {response_files}", file=sys.stderr)

    # Create compiler wrapper script (acts as alias for sccache + compiler)
    wrapper_script = None
    try:
        wrapper_script = create_compiler_wrapper_script(config)

        if config.debug:
            print(
                f"XCACHE: Created compiler wrapper: {wrapper_script}", file=sys.stderr
            )
            # Show wrapper script content for debugging
            try:
                with open(wrapper_script, "r") as f:
                    content = f.read()
                print(f"XCACHE: Wrapper script content:", file=sys.stderr)
                for i, line in enumerate(content.split("\n"), 1):
                    print(f"XCACHE:   {i}: {line}", file=sys.stderr)
            except Exception as e:
                print(f"XCACHE: Could not read wrapper script: {e}", file=sys.stderr)

        # Execute the original command but replace the compiler with our wrapper
        # The wrapper script will handle: sccache <actual_compiler> "$@"
        # Response files are passed through and handled normally by the system
        command = [str(wrapper_script)] + args

        if config.debug:
            print(
                f"XCACHE: Executing with wrapper: {' '.join(command)}", file=sys.stderr
            )
            print(f"XCACHE: Wrapper script path: {wrapper_script}", file=sys.stderr)
            print(
                f"XCACHE: Wrapper script exists: {wrapper_script.exists()}",
                file=sys.stderr,
            )
            if wrapper_script.exists():
                print(
                    f"XCACHE: Wrapper script executable: {os.access(wrapper_script, os.X_OK)}",
                    file=sys.stderr,
                )

        # Use Popen to manually pump stdout/stderr and prevent hanging
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout as requested
            text=True,
            bufsize=1,  # Line buffered
            universal_newlines=True,
        )

        # Manually pump output until process finishes
        while True:
            output = process.stdout.readline()
            if output == "" and process.poll() is not None:
                break
            if output:
                print(output.rstrip())  # Print to stdout, remove trailing newlines

        # Wait for process to complete and get return code
        return_code = process.wait()
        return return_code

    except Exception as e:
        print(f"XCACHE ERROR: Wrapper execution failed: {e}", file=sys.stderr)
        return 1
    finally:
        # Clean up wrapper script
        if wrapper_script and wrapper_script.exists():
            try:
                wrapper_script.unlink()
            except Exception:
                pass


def main() -> int:
    """Main xcache entry point."""

    # Parse command line
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <compiler> [args...]", file=sys.stderr)
        print("", file=sys.stderr)
        print(
            "xcache is an enhanced sccache wrapper with response file support.",
            file=sys.stderr,
        )
        print(
            "It handles ESP32S3 long command lines that use @response.tmp files.",
            file=sys.stderr,
        )
        return 1

    compiler_path = sys.argv[1]
    compiler_args = sys.argv[2:]

    # Check for debug mode
    debug = os.environ.get("XCACHE_DEBUG", "").lower() in ("1", "true", "yes")

    # Some ESP-IDF build steps (e.g., linker script generation) invoke the
    # compiler purely as a preprocessor (e.g. `-E -P`) on linker scripts.
    # These operations are not real compilations and often confuse sccache,
    # so bypass the cache entirely in this situation.
    if "-E" in compiler_args and "-P" in compiler_args:
        cmd = [compiler_path] + compiler_args
        return subprocess.call(cmd)

    # Find sccache
    sccache_path = find_sccache()
    if not sccache_path:
        print("XCACHE ERROR: sccache not found in PATH", file=sys.stderr)
        print("Please install sccache or ensure it's in your PATH", file=sys.stderr)
        return 1

    # Set up temporary directory
    temp_dir = Path(tempfile.gettempdir()) / "xcache"
    temp_dir.mkdir(exist_ok=True)

    # Configure xcache
    config = XCacheConfig(
        sccache_path=sccache_path,
        compiler_path=compiler_path,
        temp_dir=temp_dir,
        debug=debug,
    )

    if debug:
        print(f"XCACHE: sccache={sccache_path}", file=sys.stderr)
        print(f"XCACHE: compiler={compiler_path}", file=sys.stderr)
        print(f"XCACHE: args={compiler_args}", file=sys.stderr)
        print(f"XCACHE: temp_dir={temp_dir}", file=sys.stderr)

    # Execute with wrapper handling
    return execute_with_wrapper(config, compiler_args)


if __name__ == "__main__":
    sys.exit(main())
