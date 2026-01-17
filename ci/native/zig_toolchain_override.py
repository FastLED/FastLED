"""PlatformIO build script to replace toolchain with clang-tool-chain for Windows GNU compatibility.

On Windows, this script replaces the default MinGW GCC toolchain with clang-tool-chain's Clang
targeting x86_64-windows-gnu, matching the toolchain used by the meson build system.

This avoids the WinMain vs main() issue entirely by using a toolchain that properly
supports standard C/C++ main() entry points with the GNU ABI.

Fallback: If clang-tool-chain is not available, uses the WinMain wrapper approach.
"""

Import("env")

import platform
import shutil
import subprocess
import sys
from pathlib import Path


print(
    f"[zig_toolchain_override.py] Running on platform: {platform.system()}",
    file=sys.stderr,
)

# Only apply fixes on Windows
if platform.system() == "Windows":
    print(
        "[zig_toolchain_override.py] Attempting to use clang-tool-chain toolchain for x86_64-windows-gnu",
        file=sys.stderr,
    )

    # Try to use clang-tool-chain (matching meson.build)
    try:
        # Test if clang-tool-chain module is available
        from clang_tool_chain.wrapper import find_tool_binary

        clang_path = find_tool_binary("clang")
        clangxx_path = find_tool_binary("clang++")
        llvm_ar_path = find_tool_binary("llvm-ar")

        # Verify they exist
        if clang_path.exists() and clangxx_path.exists():
            print(
                f"[zig_toolchain_override.py] ✓ Found clang-tool-chain compilers",
                file=sys.stderr,
            )

            # Create wrapper scripts that SCons can call as single executables
            build_dir = env.get(
                "PROJECT_BUILD_DIR", env.get("BUILD_DIR", ".pio/build/native")
            )
            build_path = Path(build_dir)
            build_path.mkdir(parents=True, exist_ok=True)

            # Create Windows batch file wrappers
            clang_cc_wrapper = build_path / "clang-cc.bat"
            clang_cxx_wrapper = build_path / "clang-cxx.bat"

            # Batch file content - use absolute paths to clang binaries
            cc_batch_content = f'@echo off\n"{str(clang_path)}" %*\n'
            cxx_batch_content = f'@echo off\n"{str(clangxx_path)}" %*\n'

            # Write wrapper scripts
            clang_cc_wrapper.write_text(cc_batch_content, encoding="utf-8")
            clang_cxx_wrapper.write_text(cxx_batch_content, encoding="utf-8")

            print(
                f"[zig_toolchain_override.py] ✓ Created clang-tool-chain wrapper scripts:",
                file=sys.stderr,
            )
            print(f"[zig_toolchain_override.py]   {clang_cc_wrapper}", file=sys.stderr)
            print(f"[zig_toolchain_override.py]   {clang_cxx_wrapper}", file=sys.stderr)

            # Now replace the compilers with the wrapper scripts (single executables)
            # Use forward slashes for paths to avoid escaping issues
            cc_path = str(clang_cc_wrapper.absolute()).replace("\\", "/")
            cxx_path = str(clang_cxx_wrapper.absolute()).replace("\\", "/")

            # CRITICAL: PlatformIO resets toolchain after pre: scripts!
            # Solution: Override both the env vars AND the actual tool names
            # By naming our wrappers the same as the default tools (g++.exe, gcc.exe),
            # they'll be found first via PATH and PlatformIO won't know the difference.

            # Add wrapper directory to PATH FIRST so our wrappers shadow system compilers
            wrapper_dir = str(build_path.absolute()).replace("\\", "/")
            env.PrependENVPath("PATH", wrapper_dir)

            # Replace the compilers in SCons environment
            env.Replace(CC=cc_path, CXX=cxx_path, LINK=cxx_path)

            # Also create wrappers with STANDARD NAMES (g++.bat, gcc.bat, ar.bat)
            # This ensures that even if PlatformIO resets to "g++" and "gcc",
            # our wrappers will be found first via PATH (Windows will find .bat files)
            gcc_wrapper = build_path / "gcc.bat"
            gxx_wrapper = build_path / "g++.bat"
            ar_wrapper = build_path / "ar.bat"

            # Create standard-named wrappers (same content as clang wrappers)
            gcc_wrapper.write_text(cc_batch_content, encoding="utf-8")
            gxx_wrapper.write_text(cxx_batch_content, encoding="utf-8")
            ar_batch_content = f'@echo off\n"{str(llvm_ar_path)}" %*\n'
            ar_wrapper.write_text(ar_batch_content, encoding="utf-8")

            # Also set AR to our wrapper
            ar_path = str(ar_wrapper.absolute()).replace("\\", "/")
            env.Replace(AR=ar_path)

            # Debug: Print what SCons actually has
            print(
                f"[zig_toolchain_override.py] ✓ Created standard-named wrappers (to shadow system tools):",
                file=sys.stderr,
            )
            print(f"[zig_toolchain_override.py]   {gcc_wrapper}", file=sys.stderr)
            print(f"[zig_toolchain_override.py]   {gxx_wrapper}", file=sys.stderr)
            print(f"[zig_toolchain_override.py]   {ar_wrapper}", file=sys.stderr)
            print(
                f"[zig_toolchain_override.py]   Debug - After Replace:",
                file=sys.stderr,
            )
            print(
                f"[zig_toolchain_override.py]     env['CC'] = {env['CC']}",
                file=sys.stderr,
            )
            print(
                f"[zig_toolchain_override.py]     env['CXX'] = {env['CXX']}",
                file=sys.stderr,
            )
            print(
                f"[zig_toolchain_override.py]     env['AR'] = {env['AR']}",
                file=sys.stderr,
            )
            print(
                f"[zig_toolchain_override.py]     env['LINK'] = {env['LINK']}",
                file=sys.stderr,
            )
            print(
                f"[zig_toolchain_override.py]     Prepended to PATH: {wrapper_dir}",
                file=sys.stderr,
            )

            # Add compile flags matching meson.build (lines 96-101)
            clang_compile_flags = [
                "-DNOMINMAX",  # Prevent Windows min/max macros
                "-DWIN32_LEAN_AND_MEAN",  # Reduce Windows header conflicts
                "--target=x86_64-windows-gnu",  # Explicit GNU target for MSYS2/MinGW compatibility
                "-fuse-ld=lld-link",  # Use lld-link (MSVC-compatible linker) like meson
            ]

            env.Append(CCFLAGS=clang_compile_flags)
            env.Append(CXXFLAGS=clang_compile_flags)

            # Add linker flags matching meson.build (lines 108-120)
            # These are REQUIRED for clang's lld to find C++ stdlib and Windows libraries
            # CRITICAL: Must include --target and -fuse-ld for linker to find correct libraries
            clang_link_flags = [
                "--target=x86_64-windows-gnu",  # MUST match compile target
                "-fuse-ld=lld-link",  # MUST match compile linker choice
                "-mconsole",  # Console application
                "-nodefaultlibs",  # Don't automatically include default libraries
                "-unwindlib=libunwind",  # Use Clang's libunwind (avoid MinGW unwind)
                "-nostdlib++",  # Don't auto-link standard C++ library
                "-lc++",  # Manually link libc++ (Clang's C++ standard library)
                "-lkernel32",  # Windows kernel functions
                "-luser32",  # Windows user interface
                "-lgdi32",  # Graphics device interface
                "-ladvapi32",  # Advanced Windows API
                "-ldbghelp",  # Debug helper (for stack traces)
                "-lpsapi",  # Process status API
                # Additional Windows libraries from meson link command
                "-lwinspool",  # Windows spooler
                "-lshell32",  # Windows shell
                "-lole32",  # OLE automation
                "-loleaut32",  # OLE automation utilities
                "-luuid",  # UUID support
                "-lcomdlg32",  # Common dialogs
            ]

            env.Append(LINKFLAGS=clang_link_flags)
            print(
                f"[zig_toolchain_override.py]   Applied linker flags with --target and -fuse-ld=lld-link",
                file=sys.stderr,
            )

            # Remove GCC-specific flags that don't work with Clang
            # -Wl,-Map is not supported by lld in the same way
            # -fopt-info-all is GCC-only (Clang doesn't support it)
            # Need to check multiple flag locations
            gcc_only_flags = ["-Map", "-Wl,-Map", "-fopt-info-all"]
            for flag_key in [
                "LINKFLAGS",
                "CXXFLAGS",
                "CCFLAGS",
                "CFLAGS",
                "BUILD_FLAGS",
            ]:
                flags = env.get(flag_key, [])
                # Handle both list and CLVar (SCons command line variable) types
                if flags:
                    # Convert to list for processing
                    flags_list = (
                        list(flags)
                        if hasattr(flags, "__iter__") and not isinstance(flags, str)
                        else [flags]
                    )
                    original_len = len(flags_list)
                    # Filter out GCC-only flags
                    filtered = [
                        f
                        for f in flags_list
                        if not any(gcc_flag in str(f) for gcc_flag in gcc_only_flags)
                    ]
                    if len(filtered) < original_len:
                        env[flag_key] = filtered
                        print(
                            f"[zig_toolchain_override.py]   Removed {original_len - len(filtered)} GCC-only flags from {flag_key}",
                            file=sys.stderr,
                        )

            print(
                f"[zig_toolchain_override.py] ✓ Replaced toolchain with clang-tool-chain's Clang",
                file=sys.stderr,
            )
            print(
                f"[zig_toolchain_override.py]   CC: {env.get('CC')}",
                file=sys.stderr,
            )
            print(
                f"[zig_toolchain_override.py]   CXX: {env.get('CXX')}",
                file=sys.stderr,
            )
            print(
                f"[zig_toolchain_override.py]   This matches meson.build and uses standard main() (no WinMain needed)",
                file=sys.stderr,
            )
        else:
            raise RuntimeError("clang-tool-chain compilers not available")

    except (
        RuntimeError,
        FileNotFoundError,
        ImportError,
    ) as e:
        print(
            f"[zig_toolchain_override.py] ⚠ Could not access clang-tool-chain: {e}",
            file=sys.stderr,
        )
        print(
            "[zig_toolchain_override.py]   Using WinMain wrapper fallback",
            file=sys.stderr,
        )
        print(
            f"[zig_toolchain_override.py]   win_main.cpp will provide WinMain->main() forwarding",
            file=sys.stderr,
        )
