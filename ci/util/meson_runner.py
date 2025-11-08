#!/usr/bin/env python3
"""Meson build system integration for FastLED unit tests."""

import hashlib
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

from running_process import RunningProcess

from ci.util.build_lock import libfastled_build_lock
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


@dataclass
class MesonTestResult:
    """Result from running Meson build and tests"""

    success: bool
    duration: float  # Total duration in seconds
    num_tests_run: int = 0  # Number of tests executed
    num_tests_passed: int = 0  # Number of tests that passed
    num_tests_failed: int = 0  # Number of tests that failed


def check_meson_installed() -> bool:
    """Check if Meson is installed and accessible."""
    try:
        result = subprocess.run(
            ["meson", "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
        return result.returncode == 0
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def get_source_files_hash(source_dir: Path) -> tuple[str, list[str]]:
    """
    Get hash of all .cpp source files in src/ directory.

    This detects when source files are added or removed, which requires
    Meson reconfiguration to update the build graph.

    Args:
        source_dir: Project root directory

    Returns:
        Tuple of (hash_string, sorted_file_list)
    """
    try:
        # Recursively discover all .cpp files in src/ directory
        src_path = source_dir / "src"
        source_files = sorted(
            str(p.relative_to(source_dir)) for p in src_path.rglob("*.cpp")
        )

        # Hash the list of file paths (not contents - just detect add/remove)
        hasher = hashlib.sha256()
        for file_path in source_files:
            hasher.update(file_path.encode("utf-8"))
            hasher.update(b"\n")  # Separator

        return (hasher.hexdigest(), source_files)

    except Exception as e:
        _ts_print(f"[MESON] Warning: Failed to get source file hash: {e}")
        return ("", [])


def write_if_different(path: Path, content: str, mode: Optional[int] = None) -> bool:
    """
    Write file only if content differs from existing file.

    This prevents unnecessary file modifications that would trigger Meson
    regeneration. If the file doesn't exist or content has changed, writes
    the new content.

    Args:
        path: Path to file to write
        content: Content to write
        mode: Optional file permissions (Unix only)

    Returns:
        True if file was written (created or modified), False if unchanged
    """
    try:
        if path.exists():
            existing_content = path.read_text(encoding="utf-8")
            if existing_content == content:
                return False  # Content unchanged, skip write

        # Content changed or file doesn't exist - write it
        path.write_text(content, encoding="utf-8")

        # Set permissions if specified (Unix only)
        if mode is not None and not sys.platform.startswith("win"):
            path.chmod(mode)

        return True
    except (OSError, IOError) as e:
        # On error, try to write anyway - caller will handle failures
        _ts_print(f"[MESON] Warning: Error checking file {path}: {e}")
        path.write_text(content, encoding="utf-8")
        if mode is not None and not sys.platform.startswith("win"):
            path.chmod(mode)
        return True


def detect_system_llvm_tools() -> tuple[bool, bool]:
    """
    Detect if system has LLD and LLVM-AR that support thin archives.

    Returns:
        Tuple of (has_lld, has_llvm_ar)
    """
    has_lld = False
    has_llvm_ar = False

    # Check for system lld (not zig's bundled lld)
    # Try 'lld' first (created by workflow symlink or available by default)
    # Then try 'ld.lld' (Linux naming) as fallback
    for lld_cmd in ["lld", "ld.lld"]:
        try:
            result = subprocess.run(
                [lld_cmd, "--version"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=5,
            )
            if result.returncode == 0:
                has_lld = True
                break
        except (subprocess.SubprocessError, FileNotFoundError):
            continue

    # Check for llvm-ar with thin archive support
    # Try unversioned first, then common versioned variants (llvm-ar-20, llvm-ar-19, etc.)
    for ar_cmd in ["llvm-ar", "llvm-ar-20", "llvm-ar-19", "llvm-ar-18"]:
        try:
            result = subprocess.run(
                [ar_cmd, "--help"],
                capture_output=True,
                encoding="utf-8",
                errors="replace",
                timeout=5,
            )
            if result.returncode == 0 and "thin" in result.stdout.lower():
                has_llvm_ar = True
                break
        except (subprocess.SubprocessError, FileNotFoundError):
            continue

    return has_lld, has_llvm_ar


def setup_meson_build(
    source_dir: Path,
    build_dir: Path,
    reconfigure: bool = False,
    unity: bool = False,
    debug: bool = False,
) -> bool:
    """
    Set up Meson build directory.

    Args:
        source_dir: Project root directory containing meson.build
        build_dir: Build output directory
        reconfigure: Force reconfiguration of existing build
        unity: Enable unity builds (default: False)
        debug: Enable debug mode with full symbols and sanitizers (default: False)

    Returns:
        True if setup successful, False otherwise
    """
    # Create build directory if it doesn't exist
    build_dir.mkdir(parents=True, exist_ok=True)

    # Check if already configured
    meson_info = build_dir / "meson-info"
    already_configured = meson_info.exists()

    # Detect system LLVM tools
    has_lld, has_llvm_ar = detect_system_llvm_tools()

    # ============================================================================
    # THIN ARCHIVES: DISABLED FOR ZIG COMPILER
    # ============================================================================
    #
    # Background:
    # -----------
    # Thin archives (created with `ar crT`) store only file paths instead of
    # embedding object files directly. This provides significant benefits:
    # - Faster archive creation (no file copying)
    # - Smaller disk usage (object files not duplicated in archive)
    # - Faster incremental builds (archive update is just path table change)
    #
    # What Are Thin Archives?
    # -----------------------
    # Regular archive:  [header][obj1_data][obj2_data][obj3_data]  (~50MB)
    # Thin archive:     [header][path_to_obj1][path_to_obj2]...    (~50KB)
    #
    # Regular archives embed copies of all object files. Thin archives only
    # store file paths and let the linker read objects from their original
    # locations. This is safe because build systems control object lifetime.
    #
    # The Problem:
    # ------------
    # Zig's bundled linker (lld) does NOT support thin archives and fails with:
    #   error: unexpected token in LD script: literal: '!<thin>' (0:0)
    #   note: while parsing libfastled.a
    #
    # The '!<thin>' marker is the thin archive magic header (like '!<arch>' for
    # regular archives). When Zig's linker encounters this, it cannot parse the
    # thin archive format and aborts with the error above.
    #
    # How The Bug Occurs:
    # -------------------
    # 1. System has llvm-ar (supports thin archives)
    # 2. Meson invokes: llvm-ar crT libfastled.a *.o
    # 3. llvm-ar creates thin archive with !<thin> header
    # 4. Meson invokes: zig c++ main.o -o test libfastled.a
    # 5. Zig's bundled lld tries to read libfastled.a
    # 6. ❌ ERROR: Zig lld doesn't understand !<thin> marker
    #
    # Why -fuse-ld=lld Workaround Doesn't Work:
    # ------------------------------------------
    # We attempted to force system lld (which supports thin archives) via the
    # `-fuse-ld=lld` compiler flag. However:
    # - Zig's compiler wrapper doesn't reliably honor -fuse-ld=lld
    # - Zig may not find system lld in PATH or LD_LIBRARY_PATH
    # - Zig prioritizes its bundled toolchain over system tools
    # - Even when system lld is found, Zig may use bundled lld for some links
    #
    # Solution:
    # ---------
    # Force disable thin archives for ALL Zig-based builds by setting
    # use_thin_archives = False. This ensures llvm-ar (or zig ar) creates
    # regular archives that embed object files, which are universally
    # compatible with all linkers including Zig's bundled lld.
    #
    # Trade-offs:
    # -----------
    # ✗ Slower archive creation (~100-500ms overhead for libfastled.a)
    # ✗ Larger disk usage (~50MB vs ~50KB for libfastled.a)
    # ✗ Slower incremental builds (archive must be rebuilt on object change)
    # ✓ Guaranteed compatibility with Zig's bundled linker
    # ✓ Simpler build configuration (no fragile -fuse-ld workarounds)
    # ✓ Works reliably across all platforms (Linux/macOS/Windows)
    #
    # Impact Assessment:
    # ------------------
    # For FastLED's ~170 object file build:
    # - Archive creation: +200ms (acceptable for CI builds)
    # - Disk usage: +45MB (negligible on modern systems)
    # - Overall build time: <5% increase
    #
    # Future Consideration:
    # ---------------------
    # If we switch away from Zig compiler to pure Clang/GCC, restore the
    # original thin archive logic below to re-enable build optimizations.
    # ============================================================================

    disable_thin_archives = os.environ.get("FASTLED_DISABLE_THIN_ARCHIVES", "0") == "1"

    # Force disable thin archives when using Zig - see detailed explanation above
    use_thin_archives = False  # Always disabled for Zig builds

    # Original logic (restore if switching away from Zig compiler):
    # use_thin_archives = (has_lld and has_llvm_ar) and not disable_thin_archives

    # Set compiler environment variables to use zig
    # This matches the compiler used by the regular build system (ci/compiler/clang_compiler.py:89)

    # ============================================================================
    # CRITICAL: Check if thin archive configuration has changed
    # ============================================================================
    # When Meson is configured, it caches the AR tool path and flags in its
    # build files. If we previously configured with thin archives enabled but
    # now have them disabled (or vice versa), we MUST reconfigure Meson.
    # Otherwise, Meson will continue using the old AR tool/settings.
    #
    # We use a marker file to track the last successful thin archive setting.
    # If it doesn't match the current setting, we force a reconfigure.
    # ============================================================================
    thin_archive_marker = build_dir / ".thin_archive_config"
    unity_marker = build_dir / ".unity_config"
    source_files_marker = build_dir / ".source_files_hash"
    force_reconfigure = False

    # Get current source file hash (used for change detection and saving after setup)
    current_source_hash, current_source_files = get_source_files_hash(source_dir)

    if already_configured:
        # Check if thin archive setting has changed since last configure
        if thin_archive_marker.exists():
            try:
                last_thin_setting = thin_archive_marker.read_text().strip() == "True"
                if last_thin_setting != use_thin_archives:
                    _ts_print(
                        f"[MESON] ⚠️  Thin archive setting changed: {last_thin_setting} → {use_thin_archives}"
                    )
                    _ts_print(
                        "[MESON] 🔄 Forcing reconfigure to update AR tool configuration"
                    )
                    force_reconfigure = True
            except (OSError, IOError):
                # If we can't read the marker, force reconfigure to be safe
                _ts_print(
                    "[MESON] ⚠️  Could not read thin archive marker, forcing reconfigure"
                )
                force_reconfigure = True
        else:
            # No marker file exists from previous configure - create one after setup
            # This can happen on first run after updating to this version
            _ts_print(
                "[MESON] ℹ️  No thin archive marker found, will create after setup"
            )

        # Check if unity setting has changed since last configure
        if unity_marker.exists():
            try:
                last_unity_setting = unity_marker.read_text().strip() == "True"
                if last_unity_setting != unity:
                    _ts_print(
                        f"[MESON] ⚠️  Unity build setting changed: {last_unity_setting} → {unity}"
                    )
                    _ts_print("[MESON] 🔄 Forcing reconfigure to update build targets")
                    force_reconfigure = True
            except (OSError, IOError):
                # If we can't read the marker, force reconfigure to be safe
                _ts_print("[MESON] ⚠️  Could not read unity marker, forcing reconfigure")
                force_reconfigure = True
        else:
            # No marker file exists from previous configure
            # Force reconfigure to ensure Meson is configured with correct unity setting
            _ts_print("[MESON] ℹ️  No unity marker found, forcing reconfigure")
            force_reconfigure = True

        # Check if source files have changed since last configure
        # This detects when files are added or removed, which requires reconfigure
        if current_source_hash:  # Only check if we successfully got the hash
            if source_files_marker.exists():
                try:
                    last_hash = source_files_marker.read_text().strip()
                    if last_hash != current_source_hash:
                        _ts_print(
                            "[MESON] ⚠️  Source file list changed (files added/removed)"
                        )
                        _ts_print(
                            "[MESON] 🔄 Forcing reconfigure to update build graph"
                        )
                        force_reconfigure = True
                except (OSError, IOError):
                    # If we can't read the marker, force reconfigure to be safe
                    _ts_print(
                        "[MESON] ⚠️  Could not read source files marker, forcing reconfigure"
                    )
                    force_reconfigure = True
            else:
                # No marker file exists from previous configure
                # Force reconfigure to ensure build graph matches current source files
                _ts_print(
                    "[MESON] ℹ️  No source files marker found, forcing reconfigure"
                )
                force_reconfigure = True

    # Check if any meson.build files are newer than build.ninja (requires reconfigure)
    build_ninja_path = build_dir / "build.ninja"
    meson_build_modified = False
    if already_configured and build_ninja_path.exists():
        try:
            build_ninja_mtime = build_ninja_path.stat().st_mtime
            # Check all meson.build files (root + subdirs)
            meson_build_files = [
                source_dir / "meson.build",
                source_dir / "tests" / "meson.build",
                source_dir / "examples" / "meson.build",
            ]
            for meson_file in meson_build_files:
                if meson_file.exists():
                    meson_file_mtime = meson_file.stat().st_mtime
                    if meson_file_mtime > build_ninja_mtime:
                        _ts_print(
                            f"[MESON] ⚠️  Detected modified meson.build: {meson_file.relative_to(source_dir)}"
                        )
                        _ts_print(
                            f"[MESON]     File mtime: {meson_file_mtime:.6f} > build.ninja mtime: {build_ninja_mtime:.6f}"
                        )
                        meson_build_modified = True
                        break
        except (OSError, IOError) as e:
            _ts_print(f"[MESON] Warning: Could not check meson.build timestamps: {e}")
            # If we can't check, assume modification to be safe
            meson_build_modified = True

    # Force reconfigure if meson.build files were modified
    if meson_build_modified:
        _ts_print("[MESON] 🔄 Forcing reconfigure due to meson.build modifications")
        force_reconfigure = True

    # Determine if we need to run meson setup/reconfigure
    # We skip meson setup only if already configured, not explicitly reconfiguring,
    # AND thin archive/unity/source file settings haven't changed
    skip_meson_setup = already_configured and not reconfigure and not force_reconfigure

    # Declare native file path early (needed for meson commands)
    # The native file will be generated later after wrapper creation
    native_file_path = build_dir / "meson_native.txt"

    # Use existing sccache wrapper trampolines from .meson directory
    # These are C binary trampolines that call Python wrappers with sccache support
    # Build them if they don't exist
    meson_wrapper_dir = source_dir / ".meson"
    wrapper_build_script = meson_wrapper_dir / "build_wrappers.py"

    # Ensure wrapper trampolines are built
    if wrapper_build_script.exists():
        try:
            import sys as py_sys

            subprocess.run(
                [py_sys.executable, str(wrapper_build_script)],
                capture_output=True,
                text=True,
                check=True,
                timeout=60,
            )
        except subprocess.SubprocessError as e:
            _ts_print(f"[MESON] Warning: Failed to build wrapper trampolines: {e}")
            # Continue anyway - wrappers might already exist

    # Track if any wrapper files were modified
    wrappers_changed = False

    cmd: Optional[list[str]] = None
    if skip_meson_setup:
        # Build already configured, check wrappers below
        _ts_print(f"[MESON] Build directory already configured: {build_dir}")
    elif already_configured and (reconfigure or force_reconfigure):
        # Reconfigure existing build (explicitly requested or forced by thin archive change)
        reason = (
            "forced by thin archive change"
            if force_reconfigure
            else "explicitly requested"
        )
        _ts_print(f"[MESON] Reconfiguring build directory ({reason}): {build_dir}")
        cmd = [
            "meson",
            "setup",
            "--reconfigure",
            "--native-file",
            str(native_file_path),
            str(build_dir),
        ]
        if unity:
            cmd.extend(["-Dunity=on"])
        if debug:
            cmd.extend(["-Dbuild_mode=debug"])
    else:
        # Initial setup
        _ts_print(f"[MESON] Setting up build directory: {build_dir}")
        cmd = ["meson", "setup", "--native-file", str(native_file_path), str(build_dir)]
        if unity:
            cmd.extend(["-Dunity=on"])
        if debug:
            cmd.extend(["-Dbuild_mode=debug"])

    # Print unity build status
    if unity:
        _ts_print("[MESON] ✅ Unity builds ENABLED (--unity flag)")
    else:
        _ts_print("[MESON] Unity builds disabled (use --unity to enable)")

    # Print debug mode status
    if debug:
        _ts_print("[MESON] ✅ Debug mode ENABLED (-g3 + sanitizers)")
    else:
        _ts_print("[MESON] Debug mode disabled (using -g1 for stack traces)")

    is_windows = sys.platform.startswith("win") or os.name == "nt"

    # Thin archives configuration (faster builds, smaller disk usage when supported)
    thin_flag = " --thin" if use_thin_archives else ""

    if use_thin_archives:
        _ts_print("[MESON] ✅ Thin archives enabled (using system LLVM tools)")
    else:
        # Always show prominent warning when thin archives are disabled
        _ts_print("=" * 80)
        _ts_print("⚠️  WARNING: Thin archives DISABLED for Zig compatibility")
        _ts_print(
            "    Reason: Zig's bundled linker does not support thin archive format"
        )
        _ts_print("    Impact: +200ms archive creation, +45MB disk usage per build")

        if has_lld and has_llvm_ar:
            # System HAS the tools, but we're disabling for Zig compatibility
            _ts_print(
                "    Note: System has LLVM tools (lld, llvm-ar) but cannot use them"
            )
            _ts_print(
                "          Zig compiler does not reliably honor -fuse-ld=lld flag"
            )
        else:
            # System doesn't have LLVM tools anyway
            _ts_print("    Note: System LLVM tools not detected (additional reason)")
            missing_tools: list[str] = []
            if not has_lld:
                missing_tools.append("lld/ld.lld")
            if not has_llvm_ar:
                missing_tools.append("llvm-ar")
            _ts_print(f"          Missing: {', '.join(missing_tools)}")
        _ts_print("=" * 80)

    # Use system tools when available for thin archives, otherwise use zig
    use_system_ar = use_thin_archives and has_llvm_ar

    # When using thin archives, we must use system lld for linking
    # Add -fuse-ld=lld flag to force use of system lld instead of zig's bundled lld
    linker_flag = " -fuse-ld=lld" if use_thin_archives and has_lld else ""

    # Use wrapper trampolines from .meson directory instead of creating new wrappers
    # These are C binary trampolines that call Python wrappers with sccache support
    if is_windows:
        # Windows: Use .exe trampolines from .meson directory for compilers
        wrapper_ext = ".exe"
        cc_wrapper = meson_wrapper_dir / f"zig-cc-wrapper{wrapper_ext}"
        cxx_wrapper = meson_wrapper_dir / f"zig-cxx-wrapper{wrapper_ext}"
        # For ar, use the Python script directly (no trampoline needed)
        ar_wrapper = meson_wrapper_dir / "zig-ar.py"

        # Verify wrappers exist
        if not cc_wrapper.exists():
            _ts_print(f"[MESON] Warning: CC wrapper not found at {cc_wrapper}")
        if not cxx_wrapper.exists():
            _ts_print(f"[MESON] Warning: CXX wrapper not found at {cxx_wrapper}")
        if not ar_wrapper.exists():
            _ts_print(f"[MESON] Warning: AR wrapper not found at {ar_wrapper}")
    else:
        # Unix/Linux/macOS: Use wrapper trampolines from .meson directory for compilers
        wrapper_ext = ""
        cc_wrapper = meson_wrapper_dir / f"zig-cc-wrapper{wrapper_ext}"
        cxx_wrapper = meson_wrapper_dir / f"zig-cxx-wrapper{wrapper_ext}"
        # For ar, use the Python script directly (no trampoline needed)
        ar_wrapper = meson_wrapper_dir / "zig-ar.py"

        # Verify wrappers exist
        if not cc_wrapper.exists():
            _ts_print(f"[MESON] Warning: CC wrapper not found at {cc_wrapper}")
        if not cxx_wrapper.exists():
            _ts_print(f"[MESON] Warning: CXX wrapper not found at {cxx_wrapper}")
        if not ar_wrapper.exists():
            _ts_print(f"[MESON] Warning: AR wrapper not found at {ar_wrapper}")

    # Generate native file for Meson that persists tool configuration across regenerations
    # When Meson regenerates (e.g., when ninja detects meson.build changes),
    # environment variables are lost. Native file ensures tools are configured.
    # OPTIMIZATION: Only write native file if wrappers changed or if it doesn't exist
    try:
        # Get python executable for ar wrapper (Python script)
        import sys as py_sys

        python_exe = py_sys.executable

        # Check if sccache is available (before we use it below)
        import shutil

        sccache_available = shutil.which("sccache") is not None

        # Fix the native file content - can't use Python conditionals
        # Configure compiler wrappers with optional sccache
        if sccache_available:
            sccache_exe = shutil.which("sccache")
            c_compiler = f"['{sccache_exe}', '{str(cc_wrapper)}']"
            cpp_compiler = f"['{sccache_exe}', '{str(cxx_wrapper)}']"
        else:
            c_compiler = f"['{str(cc_wrapper)}']"
            cpp_compiler = f"['{str(cxx_wrapper)}']"

        if is_windows:
            native_file_content = f"""# ============================================================================
# Meson Native Build Configuration for FastLED (Auto-generated)
# ============================================================================
# This file is auto-generated by meson_runner.py to configure tool paths.
# It persists across build regenerations when ninja detects meson.build changes.

[binaries]
c = {c_compiler}
cpp = {cpp_compiler}
ar = ['{python_exe}', '{str(ar_wrapper)}']

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
# No additional properties needed - compiler flags are in meson.build
"""
        else:
            native_file_content = f"""# ============================================================================
# Meson Native Build Configuration for FastLED (Auto-generated)
# ============================================================================
# This file is auto-generated by meson_runner.py to configure tool paths.
# It persists across build regenerations when ninja detects meson.build changes.

[binaries]
c = {c_compiler}
cpp = {cpp_compiler}
ar = ['{python_exe}', '{str(ar_wrapper)}']

[host_machine]
system = 'linux'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
# No additional properties needed - compiler flags are in meson.build
"""
        native_file_changed = write_if_different(native_file_path, native_file_content)
        wrappers_changed |= native_file_changed

        if wrappers_changed:
            _ts_print(f"[MESON] Regenerated native file: {native_file_path}")
    except (OSError, IOError) as e:
        _ts_print(f"[MESON] Warning: Could not write native file: {e}", file=sys.stderr)

    env = os.environ.copy()
    env["CC"] = str(cc_wrapper)
    env["CXX"] = str(cxx_wrapper)
    env["AR"] = str(ar_wrapper)

    ar_tool = "llvm-ar" if use_system_ar else "zig ar"
    linker_tool = "system lld" if (use_thin_archives and has_lld) else "zig lld"

    _ts_print(f"[MESON] Using compilers: CC=zig cc, CXX=zig c++, AR={ar_tool}")
    _ts_print(f"[MESON] Using linker: {linker_tool}")
    if sccache_available:
        _ts_print(
            f"[MESON] ✅ sccache is available and will be used for compilation caching"
        )
    else:
        _ts_print(
            f"[MESON] Note: Using Zig's built-in compilation caching (sccache not found)"
        )

    # If we're skipping meson setup (already configured), check for thin archive conflicts
    if skip_meson_setup:
        # CRITICAL FIX: Check if libfastled.a exists as a thin archive
        # If we've disabled thin archives but a thin archive exists from a previous build,
        # we must delete it to force a rebuild. Otherwise the linker will fail.
        libfastled_a = build_dir / "libfastled.a"
        if libfastled_a.exists():
            try:
                # Read first 8 bytes to check for thin archive magic header
                with open(libfastled_a, "rb") as f:
                    header = f.read(8)
                    is_thin_archive = header == b"!<thin>\n"

                if is_thin_archive and not use_thin_archives:
                    # Thin archive exists but we've disabled thin archives
                    # Delete it to force rebuild with regular archive
                    _ts_print("[MESON] ⚠️  Detected thin archive from previous build")
                    _ts_print(
                        "[MESON] 🗑️  Deleting libfastled.a to force rebuild with regular archive"
                    )
                    libfastled_a.unlink()
                elif not is_thin_archive and use_thin_archives:
                    # Regular archive exists but we've enabled thin archives
                    # Delete it to force rebuild with thin archive
                    _ts_print("[MESON] ℹ️  Detected regular archive from previous build")
                    _ts_print(
                        "[MESON] 🗑️  Deleting libfastled.a to force rebuild with thin archive"
                    )
                    libfastled_a.unlink()
            except (OSError, IOError) as e:
                # If we can't read/delete, that's okay - build will handle it
                _ts_print(f"[MESON] Warning: Could not check/delete libfastled.a: {e}")
                pass
        return True

    # Run meson setup using RunningProcess for proper streaming output
    assert cmd is not None, "cmd should be set when not skipping meson setup"
    try:
        # Disable sccache during Meson setup phase to avoid probe command conflicts
        # sccache tries to detect compilers with -E flag which confuses Zig's command structure
        # This will be unset for the actual ninja build phase
        setup_env = env.copy()
        setup_env["FASTLED_DISABLE_SCCACHE"] = "1"

        proc = RunningProcess(
            cmd,
            cwd=source_dir,
            timeout=600,
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=setup_env,
            output_formatter=TimestampFormatter(),
        )

        returncode = proc.wait(echo=True)

        if returncode != 0:
            _ts_print(
                f"[MESON] Setup failed with return code {returncode}", file=sys.stderr
            )
            return False

        _ts_print(f"[MESON] Setup successful")

        # Write marker file to track thin archive setting for future runs
        try:
            thin_archive_marker.write_text(str(use_thin_archives), encoding="utf-8")
            _ts_print(
                f"[MESON] ✅ Saved thin archive configuration: {use_thin_archives}"
            )
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write thin archive marker: {e}")

        # Write marker file to track unity setting for future runs
        try:
            unity_marker.write_text(str(unity), encoding="utf-8")
            _ts_print(f"[MESON] ✅ Saved unity configuration: {unity}")
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write unity marker: {e}")

        # Write marker file to track source file list for future runs
        if current_source_hash:
            try:
                source_files_marker.write_text(current_source_hash, encoding="utf-8")
                _ts_print(
                    f"[MESON] ✅ Saved source files hash ({len(current_source_files)} files)"
                )
            except (OSError, IOError) as e:
                # Not critical if marker file write fails
                _ts_print(f"[MESON] Warning: Could not write source files marker: {e}")

        return True

    except Exception as e:
        _ts_print(f"[MESON] Setup failed with exception: {e}", file=sys.stderr)
        return False


def compile_meson(build_dir: Path, target: Optional[str] = None) -> bool:
    """
    Compile using Meson.

    Args:
        build_dir: Meson build directory
        target: Specific target to build (None = all)

    Returns:
        True if compilation successful, False otherwise
    """
    cmd = ["meson", "compile", "-C", str(build_dir)]

    if target:
        cmd.append(target)
        _ts_print(f"[MESON] Compiling target: {target}")
    else:
        _ts_print(f"[MESON] Compiling all targets...")

    try:
        # Use RunningProcess for streaming output
        # Inherit environment to ensure compiler wrappers are available
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for compilation
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),  # Pass current environment with wrapper paths
            output_formatter=TimestampFormatter(),
        )

        returncode = proc.wait(echo=True)

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
            except Exception as repair_error:
                _ts_print(
                    f"[MESON] ⚠️  Repair failed with exception: {repair_error}",
                    file=sys.stderr,
                )

        if returncode != 0:
            _ts_print(
                f"[MESON] Compilation failed with return code {returncode}",
                file=sys.stderr,
            )

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

        _ts_print(f"[MESON] Compilation successful")
        return True

    except Exception as e:
        _ts_print(f"[MESON] Compilation failed with exception: {e}", file=sys.stderr)
        return False


def _create_error_context_filter(context_lines: int = 20) -> callable:
    """
    Create a filter function that only shows output when errors are detected.

    The filter accumulates output in a circular buffer. When an error pattern
    is detected, it outputs the buffered context (lines before the error) plus
    the error line, and then continues outputting for context_lines after.

    Args:
        context_lines: Number of lines to show before and after error detection

    Returns:
        Filter function that takes a line and returns None (consumes line)
    """
    from collections import deque

    # Circular buffer for context before errors
    pre_error_buffer: deque[str] = deque(maxlen=context_lines)

    # Counter for lines after error (to show context after error)
    post_error_lines = 0

    # Track if we've seen any errors
    error_detected = False

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
        nonlocal post_error_lines, error_detected

        # Check if this line contains an error pattern
        line_lower = line.lower()
        is_error_line = any(pattern.lower() in line_lower for pattern in error_patterns)

        if is_error_line:
            # Error detected! Output all buffered pre-context
            if not error_detected:
                # First error - show header and pre-context
                _ts_print(
                    "\n[MESON] ⚠️  Test failures detected - showing error context:"
                )
                _ts_print("-" * 80)
                for buffered_line in pre_error_buffer:
                    _ts_print(buffered_line)
                error_detected = True

            # Output this error line with red color highlighting
            _ts_print(f"\033[91m{line}\033[0m")

            # Start counting post-error lines
            post_error_lines = context_lines

            # Don't buffer this line (already printed)
            return

        if post_error_lines > 0:
            # We're in the post-error context window
            _ts_print(line)
            post_error_lines -= 1
            return

        # No error detected yet - buffer this line for potential future context
        # Don't print anything - just accumulate in buffer
        pre_error_buffer.append(line)

    return filter_line


def run_meson_test(
    build_dir: Path, test_name: Optional[str] = None, verbose: bool = False
) -> MesonTestResult:
    """
    Run tests using Meson.

    Args:
        build_dir: Meson build directory
        test_name: Specific test to run (None = all)
        verbose: Enable verbose test output

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    cmd = ["meson", "test", "-C", str(build_dir), "--print-errorlogs"]

    if verbose:
        cmd.append("-v")

    if test_name:
        cmd.append(test_name)
        _ts_print(f"[MESON] Running test: {test_name}")
    else:
        _ts_print(f"[MESON] Running all tests...")

    start_time = time.time()
    num_passed = 0
    num_failed = 0
    num_run = 0

    try:
        # Use RunningProcess for streaming output
        # Inherit environment to ensure compiler wrappers are available
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for tests
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),  # Pass current environment with wrapper paths
            output_formatter=TimestampFormatter(),
        )

        # Parse output in real-time to show test progress
        # Pattern matches: "1/143 test_name       OK     0.12s"
        test_pattern = re.compile(
            r"^\s*(\d+)/(\d+)\s+(\S+)\s+(OK|FAIL|SKIP|TIMEOUT)\s+([\d.]+)s"
        )

        if verbose:
            returncode = proc.wait(echo=True)
        else:
            # Stream output line by line to show test progress
            with proc.line_iter(timeout=None) as it:
                for line in it:
                    # Try to parse test result line
                    match = test_pattern.match(line)
                    if match:
                        current = int(match.group(1))
                        total = int(match.group(2))
                        test_name_match = match.group(3)
                        status = match.group(4)
                        duration_str = match.group(5)

                        num_run = current
                        if status == "OK":
                            num_passed += 1
                            # Show brief progress for passed tests
                            _ts_print(
                                f"  [{current}/{total}] ✓ {test_name_match} ({duration_str}s)"
                            )
                        elif status == "FAIL":
                            num_failed += 1
                            _ts_print(
                                f"  [{current}/{total}] ✗ {test_name_match} FAILED ({duration_str}s)"
                            )
                        elif status == "TIMEOUT":
                            num_failed += 1
                            _ts_print(
                                f"  [{current}/{total}] ⏱ {test_name_match} TIMEOUT ({duration_str}s)"
                            )
                    elif verbose or "FAILED" in line or "ERROR" in line:
                        # Show error/important lines
                        _ts_print(f"  {line}")

            returncode = proc.wait()

            # If test failed and we didn't see individual test results, show error context
            if returncode != 0 and num_run == 0:
                # Create error-detecting filter
                error_filter = _create_error_context_filter(context_lines=20)

                # Process accumulated output to show error context
                output_lines = proc.stdout.splitlines()
                for line in output_lines:
                    error_filter(line)

        duration = time.time() - start_time

        if returncode != 0:
            _ts_print(
                f"[MESON] Tests failed with return code {returncode}", file=sys.stderr
            )
            return MesonTestResult(
                success=False,
                duration=duration,
                num_tests_run=num_run,
                num_tests_passed=num_passed,
                num_tests_failed=num_failed,
            )

        _ts_print(
            f"[MESON] All tests passed ({num_passed}/{num_run} tests in {duration:.2f}s)"
        )
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )

    except Exception as e:
        duration = time.time() - start_time
        _ts_print(f"[MESON] Test execution failed with exception: {e}", file=sys.stderr)
        return MesonTestResult(
            success=False,
            duration=duration,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )


def perform_ninja_maintenance(build_dir: Path) -> bool:
    """
    Perform periodic maintenance on Ninja dependency database.

    This function runs 'ninja -t recompact' to optimize and repair the
    .ninja_deps file. It only runs once per day (tracked via marker file)
    to avoid unnecessary overhead.

    Args:
        build_dir: Meson build directory containing .ninja_deps

    Returns:
        True if maintenance was performed or skipped successfully, False on error
    """
    # Create marker file to track last maintenance time
    marker_file = build_dir / ".ninja_deps_maintenance"

    # Check if maintenance was recently performed (within last 24 hours)
    if marker_file.exists():
        try:
            last_maintenance = marker_file.stat().st_mtime
            time_since_maintenance = time.time() - last_maintenance
            hours_since_maintenance = time_since_maintenance / 3600

            # Skip if maintenance was done within last 24 hours
            if hours_since_maintenance < 24:
                return True
        except (OSError, IOError):
            # If we can't read the marker, proceed with maintenance
            pass

    _ts_print("[MESON] 🔧 Performing periodic Ninja dependency database maintenance...")

    try:
        # Run ninja -t recompact to optimize dependency database
        repair_proc = RunningProcess(
            ["ninja", "-C", str(build_dir), "-t", "recompact"],
            timeout=60,
            auto_run=True,
            check=False,
            env=os.environ.copy(),
            output_formatter=TimestampFormatter(),
        )
        returncode = repair_proc.wait(echo=False)

        if returncode == 0:
            _ts_print(
                "[MESON] ✓ Dependency database maintenance completed successfully"
            )

            # Update marker file timestamp
            try:
                marker_file.touch()
            except (OSError, IOError):
                # Not critical if marker update fails
                pass

            return True
        else:
            _ts_print(
                "[MESON] ⚠️  Maintenance completed with warnings (non-fatal)",
                file=sys.stderr,
            )
            return True  # Non-fatal, continue anyway

    except Exception as e:
        _ts_print(
            f"[MESON] ⚠️  Maintenance failed with exception: {e} (non-fatal)",
            file=sys.stderr,
        )
        return True  # Non-fatal, continue anyway


def run_meson_build_and_test(
    source_dir: Path,
    build_dir: Path,
    test_name: Optional[str] = None,
    clean: bool = False,
    verbose: bool = False,
    unity: bool = False,
    debug: bool = False,
) -> MesonTestResult:
    """
    Complete Meson build and test workflow.

    Args:
        source_dir: Project root directory
        build_dir: Build output directory
        test_name: Specific test to run (without test_ prefix, e.g., "json")
        clean: Clean build directory before setup
        verbose: Enable verbose output
        unity: Enable unity builds (default: False)
        debug: Enable debug mode with full symbols and sanitizers (default: False)

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    start_time = time.time()

    # Check if Meson is installed
    if not check_meson_installed():
        _ts_print("[MESON] Error: Meson build system is not installed", file=sys.stderr)
        _ts_print("[MESON] Install with: pip install meson ninja", file=sys.stderr)
        return MesonTestResult(success=False, duration=time.time() - start_time)

    # Clean if requested
    if clean and build_dir.exists():
        _ts_print(f"[MESON] Cleaning build directory: {build_dir}")
        import shutil

        shutil.rmtree(build_dir)

    # Setup build
    if not setup_meson_build(
        source_dir, build_dir, reconfigure=False, unity=unity, debug=debug
    ):
        return MesonTestResult(success=False, duration=time.time() - start_time)

    # Perform periodic maintenance on Ninja dependency database (once per day)
    # This helps prevent .ninja_deps corruption and keeps builds fast
    perform_ninja_maintenance(build_dir)

    # Note: PCH dependency tracking is handled automatically by Ninja via depfiles.
    # The compile_pch.py wrapper ensures depfiles reference the .pch output correctly,
    # and Ninja loads these dependencies into .ninja_deps database (897 headers tracked).
    # No manual staleness checking needed - Ninja rebuilds PCH when any dependency changes.

    # Convert test name to executable name (add test_ prefix if needed, convert to lowercase)
    meson_test_name = None
    if test_name:
        # Convert to lowercase to match Meson target naming convention
        test_name_lower = test_name.lower()
        if not test_name_lower.startswith("test_"):
            meson_test_name = f"test_{test_name_lower}"
        else:
            meson_test_name = test_name_lower

    # Compile with build lock to prevent conflicts with example builds
    try:
        with libfastled_build_lock(timeout=600):  # 10 minute timeout
            # In unity mode, always build all_tests target (no individual test targets exist)
            compile_target = None
            if unity:
                compile_target = "all_tests"
            elif meson_test_name:
                compile_target = meson_test_name

            if not compile_meson(build_dir, target=compile_target):
                return MesonTestResult(success=False, duration=time.time() - start_time)
    except TimeoutError as e:
        _ts_print(f"[MESON] {e}", file=sys.stderr)
        return MesonTestResult(success=False, duration=time.time() - start_time)

    # Run tests
    if unity:
        # Unity mode: Run category test executables
        # Categories: core_tests, fl_tests, fx_tests, noise_tests, codec_tests
        test_categories = [
            "core_tests",
            "fl_tests",
            "fx_tests",
            "noise_tests",
            "codec_tests",
        ]

        # Find which category executables exist
        category_executables = []
        for category in test_categories:
            # Try Windows .exe first, then Unix variant
            exe_path = build_dir / "tests" / f"{category}.exe"
            if not exe_path.exists():
                exe_path = build_dir / "tests" / category

            if exe_path.exists():
                category_executables.append((category, exe_path))

        if not category_executables:
            _ts_print(
                f"[MESON] Error: No unity test executables found in {build_dir / 'tests'}",
                file=sys.stderr,
            )
            return MesonTestResult(success=False, duration=time.time() - start_time)

        # If specific test requested, filter categories or prepare doctest filter
        filter_name = None
        if meson_test_name:
            filter_name = meson_test_name.replace("test_", "")
            _ts_print(
                f"[MESON] Running unity test categories with filter: {filter_name}"
            )
        else:
            _ts_print(
                f"[MESON] Running {len(category_executables)} unity test categories"
            )

        # Run each category executable
        overall_success = True
        num_unity_tests_run = 0
        num_unity_tests_passed = 0
        num_unity_tests_failed = 0

        for category_name, exe_path in category_executables:
            num_unity_tests_run += 1
            # Build command
            test_cmd = [str(exe_path)]

            # Add doctest filter if specific test requested
            if filter_name:
                test_cmd.extend(["--test-case", f"*{filter_name}*"])

            # Always show which category is being tested (even in non-verbose mode)
            _ts_print(f"[MESON] Running {category_name}...")

            # Execute test
            try:
                proc = RunningProcess(
                    test_cmd,
                    cwd=source_dir,
                    timeout=600,  # 10 minute timeout for tests
                    auto_run=True,
                    check=False,
                    env=os.environ.copy(),
                    output_formatter=TimestampFormatter(),
                )

                # In verbose mode, show all output
                # In normal mode, only show errors with context
                if verbose:
                    returncode = proc.wait(echo=True)
                else:
                    # Wait silently, capturing all output
                    returncode = proc.wait(echo=False)

                    # If test failed, show error context from accumulated output
                    if returncode != 0:
                        _ts_print(
                            f"[MESON] Category {category_name} failed:", file=sys.stderr
                        )
                        # Create error-detecting filter
                        error_filter = _create_error_context_filter(context_lines=20)

                        # Process accumulated output to show error context
                        output_lines = proc.stdout.splitlines()
                        for line in output_lines:
                            error_filter(line)

                if returncode != 0:
                    _ts_print(
                        f"[MESON] Unity test category '{category_name}' failed with return code {returncode}",
                        file=sys.stderr,
                    )
                    overall_success = False
                    num_unity_tests_failed += 1
                    # Continue running other categories to get full test results
                else:
                    # Always show success status (even in non-verbose mode)
                    _ts_print(f"[MESON] ✓ {category_name} passed")
                    num_unity_tests_passed += 1

            except KeyboardInterrupt:
                raise
            except Exception as e:
                _ts_print(
                    f"[MESON] Unity test category '{category_name}' execution failed with exception: {e}",
                    file=sys.stderr,
                )
                overall_success = False
                num_unity_tests_failed += 1

        duration = time.time() - start_time

        if not overall_success:
            _ts_print("[MESON] Some unity test categories failed", file=sys.stderr)
            return MesonTestResult(
                success=False,
                duration=duration,
                num_tests_run=num_unity_tests_run,
                num_tests_passed=num_unity_tests_passed,
                num_tests_failed=num_unity_tests_failed,
            )

        _ts_print(f"[MESON] All unity test categories passed")
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=num_unity_tests_run,
            num_tests_passed=num_unity_tests_passed,
            num_tests_failed=num_unity_tests_failed,
        )
    else:
        # Normal mode: Use Meson's test runner
        return run_meson_test(build_dir, test_name=meson_test_name, verbose=verbose)


if __name__ == "__main__":
    # Simple CLI for testing
    import argparse

    parser = argparse.ArgumentParser(
        description="Meson build system runner for FastLED"
    )
    parser.add_argument("--test", help="Specific test to build and run")
    parser.add_argument("--clean", action="store_true", help="Clean build directory")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--build-dir", default=".build/meson", help="Build directory")

    args = parser.parse_args()

    source_dir = Path.cwd()
    build_dir = Path(args.build_dir)

    success = run_meson_build_and_test(
        source_dir=source_dir,
        build_dir=build_dir,
        test_name=args.test,
        clean=args.clean,
        verbose=args.verbose,
    )

    sys.exit(0 if success else 1)
