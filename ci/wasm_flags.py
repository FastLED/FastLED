#!/usr/bin/env python3
"""
Shared WASM build flag loader — single source of truth adapter.

Reads src/platforms/wasm/compiler/build_flags.toml and exposes flags as both
a Python API (imported by build scripts) and a CLI (called by meson via
run_command()).

Python API:
    from ci.wasm_flags import get_lib_compile_flags, get_sketch_compile_flags, get_link_flags
    from ci.wasm_flags import get_lib_compile_flags_dict, get_sketch_compile_flags_dict

CLI (for meson run_command()):
    python ci/wasm_flags.py --target lib_compile --mode quick
    python ci/wasm_flags.py --target sketch_compile --mode debug
    python ci/wasm_flags.py --target link --mode release
"""

import argparse
import sys
import tomllib
from pathlib import Path


PROJECT_ROOT = Path(__file__).parent.parent
BUILD_FLAGS_TOML = (
    PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "build_flags.toml"
)


def _load_toml() -> dict:
    """Load and return the parsed build_flags.toml."""
    if not BUILD_FLAGS_TOML.exists():
        raise FileNotFoundError(f"Build flags TOML not found: {BUILD_FLAGS_TOML}")
    with open(BUILD_FLAGS_TOML, "rb") as f:
        return tomllib.load(f)


def get_lib_compile_flags_dict(mode: str = "quick") -> dict[str, list[str]]:
    """
    Get library compilation flags as a dict.

    Combines [all] + [library] + [build_modes.<mode>].
    If library_flags exists in build mode, it replaces both [library].compiler_flags
    and [build_modes].flags (allows per-mode library flag overrides, e.g. no LTO).

    Returns:
        {"defines": [...], "compiler_flags": [...]}
    """
    config = _load_toml()

    defines = list(config.get("all", {}).get("defines", []))
    compiler_flags = list(config.get("all", {}).get("compiler_flags", []))

    defines.extend(config.get("library", {}).get("defines", []))

    build_mode_config = config.get("build_modes", {}).get(mode, {})
    # Use library-specific flags if available, otherwise fall back to
    # [library].compiler_flags + build mode flags
    library_mode_flags = build_mode_config.get("library_flags")
    if library_mode_flags is not None:
        compiler_flags.extend(library_mode_flags)
    else:
        compiler_flags.extend(config.get("library", {}).get("compiler_flags", []))
        compiler_flags.extend(build_mode_config.get("flags", []))

    return {"defines": defines, "compiler_flags": compiler_flags}


def get_sketch_compile_flags_dict(mode: str = "quick") -> dict[str, list[str]]:
    """
    Get sketch compilation flags as a dict.

    Combines [all] + [sketch] + [build_modes.<mode>].
    Uses sketch_flags from build mode if available, otherwise falls back to flags.
    Also includes link_flags from [linking.base] + [linking.sketch] + mode link_flags.

    Returns:
        {"defines": [...], "compiler_flags": [...], "link_flags": [...]}
    """
    config = _load_toml()

    defines = list(config.get("all", {}).get("defines", []))
    compiler_flags = list(config.get("all", {}).get("compiler_flags", []))

    defines.extend(config.get("sketch", {}).get("defines", []))
    compiler_flags.extend(config.get("sketch", {}).get("compiler_flags", []))

    build_mode_config = config.get("build_modes", {}).get(mode, {})
    # Use sketch-specific flags if available, otherwise fall back to shared flags
    sketch_mode_flags = build_mode_config.get("sketch_flags")
    if sketch_mode_flags is not None:
        compiler_flags.extend(sketch_mode_flags)
    else:
        compiler_flags.extend(build_mode_config.get("flags", []))

    link_flags = list(config.get("linking", {}).get("base", {}).get("flags", []))
    link_flags.extend(config.get("linking", {}).get("sketch", {}).get("flags", []))
    link_flags.extend(build_mode_config.get("link_flags", []))

    return {
        "defines": defines,
        "compiler_flags": compiler_flags,
        "link_flags": link_flags,
    }


def get_lib_compile_flags(mode: str = "quick") -> list[str]:
    """Get library compilation flags as a flat list (defines + compiler_flags)."""
    d = get_lib_compile_flags_dict(mode)
    return d["defines"] + d["compiler_flags"]


def get_sketch_compile_flags(mode: str = "quick") -> list[str]:
    """Get sketch compilation flags as a flat list (defines + compiler_flags)."""
    d = get_sketch_compile_flags_dict(mode)
    return d["defines"] + d["compiler_flags"]


def get_link_flags(mode: str = "quick") -> list[str]:
    """Get all link flags as a flat list (base + sketch + mode link_flags)."""
    d = get_sketch_compile_flags_dict(mode)
    return d["link_flags"]


def main() -> int:
    parser = argparse.ArgumentParser(description="WASM build flag loader")
    parser.add_argument(
        "--target",
        required=True,
        choices=["lib_compile", "sketch_compile", "link"],
        help="Which flag set to emit",
    )
    parser.add_argument(
        "--mode",
        default="quick",
        help="Build mode (debug, fast_debug, quick, release)",
    )
    args = parser.parse_args()

    if args.target == "lib_compile":
        flags = get_lib_compile_flags(args.mode)
    elif args.target == "sketch_compile":
        flags = get_sketch_compile_flags(args.mode)
    elif args.target == "link":
        flags = get_link_flags(args.mode)
    else:
        print(f"Unknown target: {args.target}", file=sys.stderr)
        return 1

    # One flag per line for easy consumption by meson split('\n')
    for flag in flags:
        print(flag)
    return 0


if __name__ == "__main__":
    sys.exit(main())
