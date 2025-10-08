"""Factory for creating FastLED compiler instances."""

import os
from pathlib import Path
from typing import List

from ci.compiler.clang_compiler import BuildFlags, Compiler, CompilerOptions
from ci.compiler.utils.toml_parser import (
    extract_compiler_flags_from_toml,
    extract_stub_platform_defines_from_toml,
    extract_stub_platform_include_paths_from_toml,
    load_build_flags_toml,
)


def create_fastled_compiler(
    use_pch: bool, parallel: bool, strict_mode: bool = False
) -> Compiler:
    """Create compiler with standard FastLED settings for simple build system."""
    import os
    import tempfile

    # Get absolute paths to ensure they work from any working directory
    current_dir = os.getcwd()
    src_path = os.path.join(current_dir, "src")
    arduino_stub_path = os.path.join(current_dir, "src", "platforms", "stub")

    # Load build_example.toml configuration directly from ci/ directory
    toml_path = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
        "build_example.toml",
    )
    build_config = load_build_flags_toml(
        toml_path
    )  # Will raise RuntimeError if not found

    # Extract additional compiler flags from TOML (using ci/build_example.toml directly)
    toml_flags = extract_compiler_flags_from_toml(
        build_config
    )  # Will raise RuntimeError if critical flags missing
    print(f"Loaded {len(toml_flags)} total compiler flags from build_example.toml")

    # Extract stub platform defines from TOML configuration
    stub_defines = extract_stub_platform_defines_from_toml(
        build_config
    )  # Will raise RuntimeError if critical defines missing
    print(f"Loaded {len(stub_defines)} stub platform defines from build_example.toml")

    # Extract stub platform include paths from TOML configuration
    stub_include_paths = extract_stub_platform_include_paths_from_toml(
        build_config
    )  # Will raise RuntimeError if critical paths missing
    print(
        f"Loaded {len(stub_include_paths)} stub platform include paths from build_example.toml"
    )

    # Base compiler settings - convert relative paths to absolute
    base_args: list[str] = []
    for include_path in stub_include_paths:
        if os.path.isabs(include_path):
            base_args.append(f"-I{include_path}")
        else:
            # Convert relative path to absolute from project root
            abs_path = os.path.join(current_dir, include_path)
            base_args.append(f"-I{abs_path}")
    print(f"[CONFIG] Added {len(base_args)} include paths from configuration")

    # Combine base args with TOML flags
    all_args: List[str] = base_args + toml_flags

    # PCH output path in build cache directory (persistent)
    pch_output_path = None
    if use_pch:
        cache_dir = Path(".build") / "cache"
        cache_dir.mkdir(parents=True, exist_ok=True)
        pch_output_path = str(cache_dir / "fastled_pch.hpp.pch")

    # Determine compiler command (with or without cache)
    compiler_cmd = "python -m ziglang c++"
    cache_args: List[str] = []
    print("Using direct compilation with ziglang c++")

    # Combine cache args with other args (cache args go first)
    final_args: List[str] = cache_args + all_args

    settings = CompilerOptions(
        include_path=src_path,
        defines=stub_defines,  # Use configuration-based defines instead of hardcoded ones
        std_version="c++14",
        compiler=compiler_cmd,
        compiler_args=final_args,
        use_pch=use_pch,
        pch_output_path=pch_output_path,
        parallel=parallel,
    )
    # Load build flags from TOML
    current_dir_path = Path(current_dir)
    build_flags_path = current_dir_path / "ci" / "build_example.toml"
    build_flags = BuildFlags.parse(
        build_flags_path, quick_build=False, strict_mode=strict_mode
    )

    return Compiler(settings, build_flags)
