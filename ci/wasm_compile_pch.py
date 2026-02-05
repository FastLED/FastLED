from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
WASM Precompiled Header (PCH) Compilation Script

This script compiles the WASM PCH file (wasm_pch.h) to speed up incremental builds.
The PCH contains commonly used headers (FastLED.h, emscripten headers, stdlib) that
are parsed once and reused across all compilation units.

Key Features:
- Smart invalidation detection (content hash, flags hash, compiler version)
- Dependency file generation for accurate change detection
- PCH validation flags to catch corruption/staleness issues
- Metadata tracking for rebuild decisions
- Integration with build_flags.toml for flag synchronization

Usage:
    uv run python ci/wasm_compile_pch.py [--mode debug|fast_debug|quick|release] [--force]
    uv run python ci/wasm_compile_pch.py --clean  # Remove PCH artifacts

Performance Impact:
    - PCH build time: ~3 seconds (one-time cost)
    - Incremental build speedup: 80%+ for sketch-only changes
    - Avoids reparsing ~986 header dependencies per build

Architecture:
    1. Check if PCH needs rebuild (invalidation detection)
    2. Compile wasm_pch.h to wasm_pch.h.pch using em++
    3. Generate dependency file (.d) for header tracking
    4. Save metadata (flags hash, compiler version, timestamps)
    5. Validate PCH compilation succeeded
"""

import argparse
import hashlib
import json
import subprocess
import sys
import tomllib
from pathlib import Path

from ci.wasm_tools import get_emcc


# Project root
PROJECT_ROOT = Path(__file__).parent.parent

# Paths
BUILD_FLAGS_TOML = (
    PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "build_flags.toml"
)
PCH_HEADER = PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "wasm_pch.h"
BUILD_DIR = PROJECT_ROOT / "build" / "wasm"
PCH_OUTPUT = BUILD_DIR / "wasm_pch.h.pch"
PCH_DEPFILE = BUILD_DIR / "wasm_pch.h.d"
PCH_METADATA = BUILD_DIR / "wasm_pch_metadata.json"


def get_compiler_version(emcc: str) -> str:
    """Get emscripten compiler version string."""
    try:
        result = subprocess.run(
            [emcc, "--version"],
            capture_output=True,
            text=True,
            check=True,
            timeout=10,
        )
        # Return first line which contains version info
        return result.stdout.split("\n")[0].strip()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Warning: Could not get compiler version: {e}")
        return "unknown"


def load_build_flags(build_mode: str = "quick") -> dict[str, list[str]]:
    """
    Load and parse build flags from build_flags.toml for PCH compilation.

    PCH must use the SAME flags as regular compilation to ensure compatibility.
    We use [all] + [library] flags since PCH is used for library compilation.

    Args:
        build_mode: Build mode (debug, fast_debug, quick, release)

    Returns:
        Dictionary with 'defines', 'compiler_flags' lists
    """
    if not BUILD_FLAGS_TOML.exists():
        raise FileNotFoundError(f"Build flags TOML not found: {BUILD_FLAGS_TOML}")

    with open(BUILD_FLAGS_TOML, "rb") as f:
        config = tomllib.load(f)

    # Collect flags from [all] section (used by everything)
    defines = config.get("all", {}).get("defines", [])
    compiler_flags = config.get("all", {}).get("compiler_flags", [])

    # Add library-specific flags (PCH is used for library compilation)
    defines.extend(config.get("library", {}).get("defines", []))
    compiler_flags.extend(config.get("library", {}).get("compiler_flags", []))

    # Add build mode-specific flags
    build_mode_config = config.get("build_modes", {}).get(build_mode, {})
    compiler_flags.extend(build_mode_config.get("flags", []))

    return {
        "defines": defines,
        "compiler_flags": compiler_flags,
    }


def compute_content_hash(file_path: Path) -> str:
    """Compute SHA256 hash of file content."""
    if not file_path.exists():
        return ""

    sha256 = hashlib.sha256()
    with open(file_path, "rb") as f:
        sha256.update(f.read())
    return sha256.hexdigest()


def compute_flags_hash(flags: dict[str, list[str]]) -> str:
    """Compute hash of compilation flags for change detection."""
    # Combine all flags into a single sorted string for consistent hashing
    all_flags = sorted(flags["defines"] + flags["compiler_flags"])
    flags_str = " ".join(all_flags)
    return hashlib.sha256(flags_str.encode()).hexdigest()


def load_metadata() -> dict[str, str]:
    """Load PCH metadata from previous build."""
    if not PCH_METADATA.exists():
        return {}

    try:
        with open(PCH_METADATA) as f:
            return json.load(f)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Warning: Could not load PCH metadata: {e}")
        return {}


def save_metadata(metadata: dict[str, str]) -> None:
    """Save PCH metadata for future builds."""
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    with open(PCH_METADATA, "w") as f:
        json.dump(metadata, f, indent=2)


def parse_dependency_file(depfile: Path) -> list[Path]:
    """
    Parse a .d dependency file and extract all dependency paths.

    Args:
        depfile: Path to .d dependency file

    Returns:
        List of dependency file paths (excluding the target itself)
    """
    if not depfile.exists():
        return []

    try:
        with open(depfile, "r", encoding="utf-8") as f:
            lines = f.readlines()

        # Dependency files have format: target: dep1 dep2 dep3 \
        #                                dep4 dep5 ...
        # The first line contains "target: [optional deps] \"
        # Subsequent lines contain more dependencies

        if not lines:
            return []

        # Parse first line to get target
        first_line = lines[0].rstrip("\r\n")

        # Find the separator ": " or ":\\" in the first line
        # (target paths may contain colons on Windows, e.g., C:/path)
        # Look for ": " pattern which separates target from deps
        sep_idx = first_line.find(": ")
        if sep_idx == -1:
            # Try ":\" pattern (no space before backslash)
            sep_idx = first_line.find(":\\")
            if sep_idx == -1:
                return []
            sep_len = 2  # Length of ":\\"
        else:
            sep_len = 2  # Length of ": "

        target_str = first_line[:sep_idx]
        target = Path(target_str.strip()).resolve()

        # Collect all dependency text from continuation lines
        # Line format: each line ends with " \" except the last one
        deps_text = first_line[sep_idx + sep_len :].rstrip()

        # Remove trailing backslash if present (line continuation)
        if deps_text.endswith("\\"):
            deps_text = deps_text[:-1]

        # Process continuation lines
        for line in lines[1:]:
            line_clean = line.rstrip("\r\n").strip()
            # Remove trailing backslash if present
            if line_clean.endswith("\\"):
                line_clean = line_clean[:-1].strip()
            deps_text += " " + line_clean

        # Split by whitespace and filter out empty strings
        deps = [d.strip() for d in deps_text.split() if d.strip()]

        # Convert to Path objects and resolve to absolute paths
        dep_paths: list[Path] = []
        for dep in deps:
            try:
                dep_path = Path(dep).resolve()
                # Filter out the target itself (shouldn't be in deps, but be safe)
                if dep_path != target and dep_path.exists():
                    dep_paths.append(dep_path)
            except (ValueError, OSError):
                # Skip invalid paths
                continue

        return dep_paths

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Warning: Could not parse dependency file: {e}")
        return []


def check_dependencies_changed(depfile: Path, pch_output: Path) -> tuple[bool, str]:
    """
    Check if any PCH dependencies are newer than the PCH output.

    Args:
        depfile: Path to .d dependency file
        pch_output: Path to PCH output file

    Returns:
        (changed, reason) tuple
    """
    if not depfile.exists():
        return False, ""  # No depfile, can't check dependencies

    if not pch_output.exists():
        return True, "PCH output does not exist"

    # Get PCH output modification time
    pch_mtime = pch_output.stat().st_mtime
    depfile_mtime = depfile.stat().st_mtime

    # If depfile is older than PCH, it's stale - ignore it
    # (This can happen after PCH is rebuilt but before depfile is regenerated)
    if depfile_mtime < pch_mtime:
        return False, ""  # Stale depfile, rely on other checks

    # Parse dependencies
    deps = parse_dependency_file(depfile)
    if not deps:
        return False, ""  # Empty or unparseable depfile

    # Check each dependency
    for dep in deps:
        if not dep.exists():
            # Dependency was deleted - rebuild needed
            # Note: This is expected during initial builds or after clean
            return True, f"dependency missing: {dep.name}"

        dep_mtime = dep.stat().st_mtime
        if dep_mtime > pch_mtime:
            # Dependency is newer than PCH - rebuild needed
            return True, f"dependency changed: {dep.name}"

    return False, ""


def needs_rebuild(
    emcc: str,
    flags: dict[str, list[str]],
    force: bool = False,
) -> tuple[bool, str]:
    """
    Determine if PCH needs to be rebuilt.

    Returns:
        (needs_rebuild, reason) tuple
    """
    if force:
        return True, "forced rebuild (--force flag)"

    if not PCH_OUTPUT.exists():
        return True, "PCH output file does not exist"

    # Load metadata from previous build
    metadata = load_metadata()
    if not metadata:
        return True, "no previous build metadata found"

    # Check header content hash
    current_header_hash = compute_content_hash(PCH_HEADER)
    if metadata.get("header_hash") != current_header_hash:
        return True, "PCH header content changed"

    # Check flags hash
    current_flags_hash = compute_flags_hash(flags)
    if metadata.get("flags_hash") != current_flags_hash:
        return True, "compilation flags changed"

    # Check compiler version
    current_compiler_version = get_compiler_version(emcc)
    if metadata.get("compiler_version") != current_compiler_version:
        return True, "compiler version changed"

    # Check if any dependencies have changed (NEW: dependency tracking)
    deps_changed, reason = check_dependencies_changed(PCH_DEPFILE, PCH_OUTPUT)
    if deps_changed:
        return True, reason

    return False, "PCH is up to date"


def compile_pch(
    emcc: str,
    flags: dict[str, list[str]],
    verbose: bool = False,
) -> int:
    """
    Compile the PCH file.

    Args:
        emcc: Compiler command (e.g., 'clang-tool-chain-emcc')
        flags: Compilation flags from TOML
        verbose: Enable verbose output

    Returns:
        Exit code (0 = success)
    """
    print(f"Compiling PCH: {PCH_HEADER.name} -> {PCH_OUTPUT.name}...")

    # Ensure build directory exists
    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    # Build includes (must match regular compilation)
    includes = [
        f"-I{PROJECT_ROOT / 'src'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm' / 'compiler'}",
    ]

    # PCH validation flags (catch invalidation/corruption issues)
    pch_validation_flags = [
        "-Werror=invalid-pch",  # Treat invalid PCH as hard error
        "-verify-pch",  # Load and verify PCH is not stale
        "-fpch-validate-input-files-content",  # Validate based on content, not mtime
        "-fpch-instantiate-templates",  # Instantiate templates during PCH build
    ]

    # Build command using compile_pch.py wrapper (fixes depfile)
    cmd = (
        [
            "uv",
            "run",
            "python",
            str(PROJECT_ROOT / "ci" / "compile_pch.py"),
            emcc,
            "-x",
            "c++-header",  # Treat input as C++ header
            str(PCH_HEADER),  # Input: wasm_pch.h
            "-o",
            str(PCH_OUTPUT),  # Output: wasm_pch.h.pch
            "-MD",
            "-MF",
            str(PCH_DEPFILE),  # Generate dependency file
        ]
        + includes
        + flags["defines"]
        + flags["compiler_flags"]
        + pch_validation_flags
    )

    if verbose:
        print(f"Command: {' '.join(cmd)}")

    # Run compilation
    result = subprocess.run(cmd, cwd=PROJECT_ROOT)

    if result.returncode != 0:
        print(f"✗ PCH compilation failed with return code {result.returncode}")
        return result.returncode

    # Verify PCH was created
    if not PCH_OUTPUT.exists():
        print(
            f"✗ PCH compilation reported success but output file not found: {PCH_OUTPUT}"
        )
        return 1

    print(f"✓ PCH compiled successfully: {PCH_OUTPUT}")

    # Save metadata for future invalidation checks
    metadata = {
        "header_hash": compute_content_hash(PCH_HEADER),
        "flags_hash": compute_flags_hash(flags),
        "compiler_version": get_compiler_version(emcc),
        "pch_path": str(PCH_OUTPUT),
        "depfile_path": str(PCH_DEPFILE),
    }
    save_metadata(metadata)

    if verbose:
        print(f"Saved PCH metadata: {PCH_METADATA}")

    return 0


def clean_pch() -> int:
    """Remove all PCH artifacts."""
    print("Cleaning PCH artifacts...")

    removed_count = 0
    for artifact in [PCH_OUTPUT, PCH_DEPFILE, PCH_METADATA]:
        if artifact.exists():
            artifact.unlink()
            print(f"  Removed: {artifact}")
            removed_count += 1

    if removed_count == 0:
        print("  No PCH artifacts found to clean")
    else:
        print(f"✓ Cleaned {removed_count} PCH artifact(s)")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile WASM precompiled header for faster builds"
    )
    parser.add_argument(
        "--mode",
        default="quick",
        choices=["debug", "fast_debug", "quick", "release"],
        help="Build mode (default: quick)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Force PCH rebuild even if up to date",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove PCH artifacts and exit",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Verbose output",
    )

    args = parser.parse_args()

    try:
        # Handle clean command
        if args.clean:
            return clean_pch()

        # Verify PCH header exists
        if not PCH_HEADER.exists():
            print(f"Error: PCH header not found: {PCH_HEADER}", file=sys.stderr)
            return 1

        # Get tool command
        emcc = get_emcc()
        if args.verbose:
            print(f"Using emscripten: {emcc}")

        # Load build flags from TOML
        flags = load_build_flags(args.mode)
        if args.verbose:
            print(
                f"Loaded {len(flags['defines'])} defines, {len(flags['compiler_flags'])} compiler flags"
            )

        # Check if rebuild is needed
        rebuild_needed, reason = needs_rebuild(emcc, flags, args.force)

        if not rebuild_needed:
            print(f"✓ PCH is up to date ({reason})")
            return 0

        if args.verbose:
            print(f"Rebuild needed: {reason}")

        # Compile PCH
        return compile_pch(emcc, flags, args.verbose)

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"✗ PCH compilation failed with exception: {e}", file=sys.stderr)
        if args.verbose:
            import traceback

            traceback.print_exc(file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
