#!/usr/bin/env python3
"""Meson build system integration for FastLED unit tests."""

import os
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple

from running_process import RunningProcess

from ci.util.build_lock import libfastled_build_lock


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
        print(f"[MESON] Warning: Error checking file {path}: {e}")
        path.write_text(content, encoding="utf-8")
        if mode is not None and not sys.platform.startswith("win"):
            path.chmod(mode)
        return True


def detect_system_llvm_tools() -> Tuple[bool, bool]:
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
    source_dir: Path, build_dir: Path, reconfigure: bool = False, unity: bool = False
) -> bool:
    """
    Set up Meson build directory.

    Args:
        source_dir: Project root directory containing meson.build
        build_dir: Build output directory
        reconfigure: Force reconfiguration of existing build
        unity: Enable unity builds (default: False)

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
    force_reconfigure = False

    if already_configured:
        # Check if thin archive setting has changed since last configure
        if thin_archive_marker.exists():
            try:
                last_thin_setting = thin_archive_marker.read_text().strip() == "True"
                if last_thin_setting != use_thin_archives:
                    print(
                        f"[MESON] ⚠️  Thin archive setting changed: {last_thin_setting} → {use_thin_archives}"
                    )
                    print(
                        "[MESON] 🔄 Forcing reconfigure to update AR tool configuration"
                    )
                    force_reconfigure = True
            except (OSError, IOError):
                # If we can't read the marker, force reconfigure to be safe
                print(
                    "[MESON] ⚠️  Could not read thin archive marker, forcing reconfigure"
                )
                force_reconfigure = True
        else:
            # No marker file exists from previous configure - create one after setup
            # This can happen on first run after updating to this version
            print("[MESON] ℹ️  No thin archive marker found, will create after setup")

        # Check if unity setting has changed since last configure
        if unity_marker.exists():
            try:
                last_unity_setting = unity_marker.read_text().strip() == "True"
                if last_unity_setting != unity:
                    print(
                        f"[MESON] ⚠️  Unity build setting changed: {last_unity_setting} → {unity}"
                    )
                    print("[MESON] 🔄 Forcing reconfigure to update build targets")
                    force_reconfigure = True
            except (OSError, IOError):
                # If we can't read the marker, force reconfigure to be safe
                print("[MESON] ⚠️  Could not read unity marker, forcing reconfigure")
                force_reconfigure = True
        else:
            # No marker file exists from previous configure
            # Force reconfigure to ensure Meson is configured with correct unity setting
            print("[MESON] ℹ️  No unity marker found, forcing reconfigure")
            force_reconfigure = True

    # Determine if we need to run meson setup/reconfigure
    # We skip meson setup only if already configured, not explicitly reconfiguring,
    # AND thin archive/unity settings haven't changed
    skip_meson_setup = already_configured and not reconfigure and not force_reconfigure

    # Declare native file path early (needed for meson commands)
    # The native file will be generated later after wrapper creation
    native_file_path = build_dir / "meson_native.txt"

    # Create wrapper scripts for meson since it expects single executables
    # OPTIMIZATION: Only recreate wrappers if content changes to avoid triggering Meson regeneration
    wrapper_dir = build_dir / "compiler_wrappers"
    wrapper_dir.mkdir(parents=True, exist_ok=True)

    # Track if any wrapper files were modified
    wrappers_changed = False

    cmd: Optional[List[str]] = None
    if skip_meson_setup:
        # Build already configured, check wrappers below
        print(f"[MESON] Build directory already configured: {build_dir}")
    elif already_configured and (reconfigure or force_reconfigure):
        # Reconfigure existing build (explicitly requested or forced by thin archive change)
        reason = (
            "forced by thin archive change"
            if force_reconfigure
            else "explicitly requested"
        )
        print(f"[MESON] Reconfiguring build directory ({reason}): {build_dir}")
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
    else:
        # Initial setup
        print(f"[MESON] Setting up build directory: {build_dir}")
        cmd = ["meson", "setup", "--native-file", str(native_file_path), str(build_dir)]
        if unity:
            cmd.extend(["-Dunity=on"])

    # Print unity build status
    if unity:
        print("[MESON] ✅ Unity builds ENABLED (--unity flag)")
    else:
        print("[MESON] Unity builds disabled (use --unity to enable)")

    is_windows = sys.platform.startswith("win") or os.name == "nt"

    # Thin archives configuration (faster builds, smaller disk usage when supported)
    thin_flag = " --thin" if use_thin_archives else ""

    if use_thin_archives:
        print("[MESON] ✅ Thin archives enabled (using system LLVM tools)")
    else:
        # Always show prominent warning when thin archives are disabled
        print("=" * 80)
        print("⚠️  WARNING: Thin archives DISABLED for Zig compatibility")
        print("    Reason: Zig's bundled linker does not support thin archive format")
        print("    Impact: +200ms archive creation, +45MB disk usage per build")

        if has_lld and has_llvm_ar:
            # System HAS the tools, but we're disabling for Zig compatibility
            print("    Note: System has LLVM tools (lld, llvm-ar) but cannot use them")
            print("          Zig compiler does not reliably honor -fuse-ld=lld flag")
        else:
            # System doesn't have LLVM tools anyway
            print("    Note: System LLVM tools not detected (additional reason)")
            missing_tools: List[str] = []
            if not has_lld:
                missing_tools.append("lld/ld.lld")
            if not has_llvm_ar:
                missing_tools.append("llvm-ar")
            print(f"          Missing: {', '.join(missing_tools)}")
        print("=" * 80)

    # Use system tools when available for thin archives, otherwise use zig
    use_system_ar = use_thin_archives and has_llvm_ar

    # When using thin archives, we must use system lld for linking
    # Add -fuse-ld=lld flag to force use of system lld instead of zig's bundled lld
    linker_flag = " -fuse-ld=lld" if use_thin_archives and has_lld else ""

    if is_windows:
        # Windows: Create .cmd wrappers
        cc_wrapper = wrapper_dir / "zig-cc.cmd"
        cxx_wrapper = wrapper_dir / "zig-cxx.cmd"
        ar_wrapper = wrapper_dir / "zig-ar.cmd"

        wrappers_changed |= write_if_different(
            cc_wrapper, f"@echo off\npython -m ziglang cc{linker_flag} %*\n"
        )
        wrappers_changed |= write_if_different(
            cxx_wrapper, f"@echo off\npython -m ziglang c++{linker_flag} %*\n"
        )

        if use_system_ar:
            # Use system llvm-ar with thin archives support
            wrappers_changed |= write_if_different(
                ar_wrapper, f"@echo off\nllvm-ar{thin_flag} %*\n"
            )
        else:
            # Use zig's ar
            # Filter out thin archive flag 'T' from arguments when not using thin archives
            # because Zig's linker doesn't support thin archives
            if use_thin_archives:
                wrappers_changed |= write_if_different(
                    ar_wrapper, f"@echo off\npython -m ziglang ar{thin_flag} %*\n"
                )
            else:
                # When not using thin archives, use Python wrapper to filter T flag (same as Unix)
                # Windows batch regex is too limited, Python is more reliable
                ar_wrapper_py = wrapper_dir / "zig-ar-filter.py"
                ar_filter_content = (
                    "#!/usr/bin/env python3\n"
                    '"""Filter thin archive flags from ar arguments for Zig compatibility"""\n'
                    "import sys\n"
                    "import subprocess\n"
                    "\n"
                    'if __name__ == "__main__":\n'
                    "    args = []\n"
                    "    for arg in sys.argv[1:]:\n"
                    "        # Skip --thin flag entirely\n"
                    '        if arg == "--thin" or arg == "-T":\n'
                    "            continue\n"
                    "        # Remove T from operation flags like csrDT, crT, rcT, Tcr, etc.\n"
                    "        # Operation flags are single args containing only ar flag letters\n"
                    "        # Common ar flags: c r s q t p a d i b D T U V u\n"
                    '        ar_flag_chars = set("crsqtpadibDTUVu")\n'
                    '        if len(arg) <= 10 and arg and set(arg).issubset(ar_flag_chars) and "T" in arg:\n'
                    "            # This looks like an operation flag containing T - remove T\n"
                    '            arg = arg.replace("T", "")\n'
                    "            # Skip if now empty\n"
                    "            if not arg:\n"
                    "                continue\n"
                    "        args.append(arg)\n"
                    "    \n"
                    "    # Execute zig ar with filtered arguments\n"
                    '    result = subprocess.run([sys.executable, "-m", "ziglang", "ar"] + args)\n'
                    "    sys.exit(result.returncode)\n"
                )
                wrappers_changed |= write_if_different(ar_wrapper_py, ar_filter_content)

                # Main wrapper calls the Python filter
                wrappers_changed |= write_if_different(
                    ar_wrapper, f'@echo off\npython "{ar_wrapper_py}" %*\n'
                )
    else:
        # Unix/Linux/macOS: Create shell script wrappers
        cc_wrapper = wrapper_dir / "zig-cc"
        cxx_wrapper = wrapper_dir / "zig-cxx"
        ar_wrapper = wrapper_dir / "zig-ar"

        wrappers_changed |= write_if_different(
            cc_wrapper,
            f'#!/bin/sh\nexec python -m ziglang cc{linker_flag} "$@"\n',
            mode=0o755,
        )
        wrappers_changed |= write_if_different(
            cxx_wrapper,
            f'#!/bin/sh\nexec python -m ziglang c++{linker_flag} "$@"\n',
            mode=0o755,
        )

        if use_system_ar:
            # Use system llvm-ar with thin archives support
            wrappers_changed |= write_if_different(
                ar_wrapper, f'#!/bin/sh\nexec llvm-ar{thin_flag} "$@"\n', mode=0o755
            )
        else:
            # Use zig's ar with Python wrapper to robustly filter thin archive flags
            # CRITICAL: Use Python wrapper instead of shell script for reliable filtering
            # Shell scripts have fragile pattern matching that fails with different flag orders
            if use_thin_archives:
                # Thin archives enabled - pass through directly
                wrappers_changed |= write_if_different(
                    ar_wrapper,
                    f'#!/bin/sh\nexec python -m ziglang ar{thin_flag} "$@"\n',
                    mode=0o755,
                )
            else:
                # Thin archives disabled - use Python wrapper to filter ALL thin archive flags
                # This handles:  'T' in operation string (crT, rcT, Tcr, etc.)
                #                --thin as separate argument
                #                Any combination or order of flags
                ar_wrapper_py = wrapper_dir / "zig-ar-filter.py"
                ar_filter_content = (
                    "#!/usr/bin/env python3\n"
                    '"""Filter thin archive flags from ar arguments for Zig compatibility"""\n'
                    "import sys\n"
                    "import subprocess\n"
                    "\n"
                    'if __name__ == "__main__":\n'
                    "    args = []\n"
                    "    for arg in sys.argv[1:]:\n"
                    "        # Skip --thin flag entirely\n"
                    '        if arg == "--thin" or arg == "-T":\n'
                    "            continue\n"
                    "        # Remove T from operation flags like csrDT, crT, rcT, Tcr, etc.\n"
                    "        # Operation flags are single args containing only ar flag letters\n"
                    "        # Common ar flags: c r s q t p a d i b D T U V u\n"
                    '        ar_flag_chars = set("crsqtpadibDTUVu")\n'
                    '        if len(arg) <= 10 and arg and set(arg).issubset(ar_flag_chars) and "T" in arg:\n'
                    "            # This looks like an operation flag containing T - remove T\n"
                    '            arg = arg.replace("T", "")\n'
                    "            # Skip if now empty\n"
                    "            if not arg:\n"
                    "                continue\n"
                    "        args.append(arg)\n"
                    "    \n"
                    "    # Execute zig ar with filtered arguments\n"
                    '    result = subprocess.run([sys.executable, "-m", "ziglang", "ar"] + args)\n'
                    "    sys.exit(result.returncode)\n"
                )
                wrappers_changed |= write_if_different(
                    ar_wrapper_py, ar_filter_content, mode=0o755
                )

                # Main wrapper calls the Python filter
                wrappers_changed |= write_if_different(
                    ar_wrapper,
                    f'#!/bin/sh\nexec {sys.executable} "{ar_wrapper_py}" "$@"\n',
                    mode=0o755,
                )

    # Generate native file for Meson that persists tool configuration across regenerations
    # When Meson regenerates (e.g., when ninja detects meson.build changes),
    # environment variables are lost. Native file ensures tools are configured.
    # OPTIMIZATION: Only write native file if wrappers changed or if it doesn't exist
    try:
        # Fix the native file content - can't use Python conditionals
        if is_windows:
            native_file_content = f"""# ============================================================================
# Meson Native Build Configuration for FastLED (Auto-generated)
# ============================================================================
# This file is auto-generated by meson_runner.py to configure tool paths.
# It persists across build regenerations when ninja detects meson.build changes.

[binaries]
c = ['{str(cc_wrapper)}']
cpp = ['{str(cxx_wrapper)}']
ar = ['{str(ar_wrapper)}']

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
c = ['{str(cc_wrapper)}']
cpp = ['{str(cxx_wrapper)}']
ar = ['{str(ar_wrapper)}']

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
            print(f"[MESON] Regenerated native file: {native_file_path}")
    except (OSError, IOError) as e:
        print(f"[MESON] Warning: Could not write native file: {e}", file=sys.stderr)

    env = os.environ.copy()
    env["CC"] = str(cc_wrapper)
    env["CXX"] = str(cxx_wrapper)
    env["AR"] = str(ar_wrapper)

    ar_tool = "llvm-ar" if use_system_ar else "zig ar"
    linker_tool = "system lld" if (use_thin_archives and has_lld) else "zig lld"
    print(f"[MESON] Using compilers: CC=zig cc, CXX=zig c++, AR={ar_tool}")
    print(f"[MESON] Using linker: {linker_tool}")

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
                    print("[MESON] ⚠️  Detected thin archive from previous build")
                    print(
                        "[MESON] 🗑️  Deleting libfastled.a to force rebuild with regular archive"
                    )
                    libfastled_a.unlink()
                elif not is_thin_archive and use_thin_archives:
                    # Regular archive exists but we've enabled thin archives
                    # Delete it to force rebuild with thin archive
                    print("[MESON] ℹ️  Detected regular archive from previous build")
                    print(
                        "[MESON] 🗑️  Deleting libfastled.a to force rebuild with thin archive"
                    )
                    libfastled_a.unlink()
            except (OSError, IOError) as e:
                # If we can't read/delete, that's okay - build will handle it
                print(f"[MESON] Warning: Could not check/delete libfastled.a: {e}")
                pass
        return True

    # Run meson setup using RunningProcess for proper streaming output
    assert cmd is not None, "cmd should be set when not skipping meson setup"
    try:
        proc = RunningProcess(
            cmd,
            cwd=source_dir,
            timeout=600,
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=env,
        )

        returncode = proc.wait(echo=True)

        if returncode != 0:
            print(
                f"[MESON] Setup failed with return code {returncode}", file=sys.stderr
            )
            return False

        print(f"[MESON] Setup successful")

        # Write marker file to track thin archive setting for future runs
        try:
            thin_archive_marker.write_text(str(use_thin_archives), encoding="utf-8")
            print(f"[MESON] ✅ Saved thin archive configuration: {use_thin_archives}")
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            print(f"[MESON] Warning: Could not write thin archive marker: {e}")

        # Write marker file to track unity setting for future runs
        try:
            unity_marker.write_text(str(unity), encoding="utf-8")
            print(f"[MESON] ✅ Saved unity configuration: {unity}")
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            print(f"[MESON] Warning: Could not write unity marker: {e}")

        return True

    except Exception as e:
        print(f"[MESON] Setup failed with exception: {e}", file=sys.stderr)
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
        print(f"[MESON] Compiling target: {target}")
    else:
        print(f"[MESON] Compiling all targets...")

    try:
        # Use RunningProcess for streaming output
        # Inherit environment to ensure compiler wrappers are available
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for compilation
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),  # Pass current environment with wrapper paths
        )

        returncode = proc.wait(echo=True)

        if returncode != 0:
            print(
                f"[MESON] Compilation failed with return code {returncode}",
                file=sys.stderr,
            )
            return False

        print(f"[MESON] Compilation successful")
        return True

    except Exception as e:
        print(f"[MESON] Compilation failed with exception: {e}", file=sys.stderr)
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
                print("\n[MESON] ⚠️  Test failures detected - showing error context:")
                print("-" * 80)
                for buffered_line in pre_error_buffer:
                    print(buffered_line)
                error_detected = True

            # Output this error line with red color highlighting
            print(f"\033[91m{line}\033[0m")

            # Start counting post-error lines
            post_error_lines = context_lines

            # Don't buffer this line (already printed)
            return

        if post_error_lines > 0:
            # We're in the post-error context window
            print(line)
            post_error_lines -= 1
            return

        # No error detected yet - buffer this line for potential future context
        # Don't print anything - just accumulate in buffer
        pre_error_buffer.append(line)

    return filter_line


def run_meson_test(
    build_dir: Path, test_name: Optional[str] = None, verbose: bool = False
) -> bool:
    """
    Run tests using Meson.

    Args:
        build_dir: Meson build directory
        test_name: Specific test to run (None = all)
        verbose: Enable verbose test output

    Returns:
        True if all tests passed, False otherwise
    """
    cmd = ["meson", "test", "-C", str(build_dir), "--print-errorlogs"]

    if verbose:
        cmd.append("-v")

    if test_name:
        cmd.append(test_name)
        print(f"[MESON] Running test: {test_name}")
    else:
        print(f"[MESON] Running all tests...")

    try:
        # Use RunningProcess for streaming output
        # Inherit environment to ensure compiler wrappers are available
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for tests
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),  # Pass current environment with wrapper paths
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
                # Create error-detecting filter
                error_filter = _create_error_context_filter(context_lines=20)

                # Process accumulated output to show error context
                output_lines = proc.stdout.splitlines()
                for line in output_lines:
                    error_filter(line)

        if returncode != 0:
            print(
                f"[MESON] Tests failed with return code {returncode}", file=sys.stderr
            )
            return False

        print(f"[MESON] All tests passed")
        return True

    except Exception as e:
        print(f"[MESON] Test execution failed with exception: {e}", file=sys.stderr)
        return False


def run_meson_build_and_test(
    source_dir: Path,
    build_dir: Path,
    test_name: Optional[str] = None,
    clean: bool = False,
    verbose: bool = False,
    unity: bool = False,
) -> bool:
    """
    Complete Meson build and test workflow.

    Args:
        source_dir: Project root directory
        build_dir: Build output directory
        test_name: Specific test to run (without test_ prefix, e.g., "json")
        clean: Clean build directory before setup
        verbose: Enable verbose output
        unity: Enable unity builds (default: False)

    Returns:
        True if build and tests successful, False otherwise
    """
    # Check if Meson is installed
    if not check_meson_installed():
        print("[MESON] Error: Meson build system is not installed", file=sys.stderr)
        print("[MESON] Install with: pip install meson ninja", file=sys.stderr)
        return False

    # Clean if requested
    if clean and build_dir.exists():
        print(f"[MESON] Cleaning build directory: {build_dir}")
        import shutil

        shutil.rmtree(build_dir)

    # Setup build
    if not setup_meson_build(source_dir, build_dir, reconfigure=False, unity=unity):
        return False

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
                return False
    except TimeoutError as e:
        print(f"[MESON] {e}", file=sys.stderr)
        return False

    # Run tests
    if unity:
        # Unity mode: Run unified test executable directly
        all_tests_exe = build_dir / "tests" / "all_tests.exe"

        # Check if executable exists (Windows uses .exe, Unix doesn't)
        if not all_tests_exe.exists():
            all_tests_exe = build_dir / "tests" / "all_tests"

        if not all_tests_exe.exists():
            print(
                f"[MESON] Error: Unity test executable not found at {all_tests_exe}",
                file=sys.stderr,
            )
            return False

        # Build command
        test_cmd = [str(all_tests_exe)]

        # If specific test name provided, use doctest filtering
        if meson_test_name:
            # Strip the test_ prefix for doctest filtering to match test case names
            filter_name = meson_test_name.replace("test_", "")
            test_cmd.extend(["--test-case", f"*{filter_name}*"])
            print(f"[MESON] Running unified test executable with filter: {filter_name}")
        else:
            print(f"[MESON] Running unified test executable (all tests)")

        # Execute test
        try:
            proc = RunningProcess(
                test_cmd,
                cwd=source_dir,
                timeout=600,  # 10 minute timeout for tests
                auto_run=True,
                check=False,
                env=os.environ.copy(),
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
                    # Create error-detecting filter
                    error_filter = _create_error_context_filter(context_lines=20)

                    # Process accumulated output to show error context
                    output_lines = proc.stdout.splitlines()
                    for line in output_lines:
                        error_filter(line)

            if returncode != 0:
                print(
                    f"[MESON] Unity tests failed with return code {returncode}",
                    file=sys.stderr,
                )
                return False

            print(f"[MESON] Unity tests passed")
            return True

        except Exception as e:
            print(
                f"[MESON] Unity test execution failed with exception: {e}",
                file=sys.stderr,
            )
            return False
    else:
        # Normal mode: Use Meson's test runner
        if not run_meson_test(build_dir, test_name=meson_test_name, verbose=verbose):
            return False

    return True


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
