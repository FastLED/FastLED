"""TOML configuration parsing utilities."""

import tomllib
from typing import Any, Dict, List


def load_build_flags_toml(toml_path: str) -> Dict[str, Any]:
    """Load and parse build_example.toml file."""
    try:
        with open(toml_path, "rb") as f:
            config = tomllib.load(f)
            if not config:
                raise RuntimeError(
                    f"build_example.toml at {toml_path} is empty or invalid"
                )
            return config
    except FileNotFoundError:
        raise RuntimeError(
            f"CRITICAL: build_example.toml not found at {toml_path}. This file is required for proper compilation flags."
        )
    except Exception as e:
        raise RuntimeError(
            f"CRITICAL: Failed to parse build_example.toml at {toml_path}: {e}"
        )


def extract_compiler_flags_from_toml(config: Dict[str, Any]) -> List[str]:
    """Extract compiler flags from TOML config - includes universal [all] flags and [build_modes.quick] flags."""
    flags: List[str] = []

    # First, extract universal compiler flags from [all] section
    if "all" in config and isinstance(config["all"], dict):
        all_section: Dict[str, Any] = config["all"]  # type: ignore
        if "compiler_flags" in all_section and isinstance(
            all_section["compiler_flags"], list
        ):
            compiler_flags: List[str] = all_section["compiler_flags"]  # type: ignore
            flags.extend(compiler_flags)
            print(
                f"[CONFIG] Loaded {len(compiler_flags)} universal compiler flags from [all] section"
            )

    # Then, extract flags from [build_modes.quick] section for sketch compilation
    if "build_modes" in config and isinstance(config["build_modes"], dict):
        build_modes: Dict[str, Any] = config["build_modes"]  # type: ignore
        if "quick" in build_modes and isinstance(build_modes["quick"], dict):
            quick_section: Dict[str, Any] = build_modes["quick"]  # type: ignore

            if "flags" in quick_section and isinstance(quick_section["flags"], list):
                quick_flags: List[str] = quick_section["flags"]  # type: ignore
                flags.extend(quick_flags)
                print(
                    f"[CONFIG] Loaded {len(quick_flags)} quick mode flags from [build_modes.quick] section"
                )

    return flags


def extract_stub_platform_defines_from_toml(config: Dict[str, Any]) -> List[str]:
    """Extract stub platform defines from TOML config - uses [stub_platform] section."""
    defines: List[str] = []

    # Extract defines from [stub_platform] section
    if "stub_platform" in config and isinstance(config["stub_platform"], dict):
        stub_section: Dict[str, Any] = config["stub_platform"]  # type: ignore
        if "defines" in stub_section and isinstance(stub_section["defines"], list):
            stub_defines: List[str] = stub_section["defines"]  # type: ignore
            defines.extend(stub_defines)
            print(
                f"[CONFIG] Loaded {len(stub_defines)} stub platform defines from [stub_platform] section"
            )
        else:
            raise RuntimeError(
                "CRITICAL: No 'defines' list found in [stub_platform] section. "
                "This is required for stub platform compilation."
            )
    else:
        raise RuntimeError(
            "CRITICAL: No [stub_platform] section found in build_example.toml. "
            "This section is MANDATORY for stub platform compilation."
        )

    # Validate that all required defines are present
    required_defines = [
        "STUB_PLATFORM",
        "ARDUINO=",  # Partial match for version number
        "FASTLED_USE_STUB_ARDUINO",
        "FASTLED_STUB_IMPL",
    ]

    for required in required_defines:
        if not any(define.startswith(required) for define in defines):
            raise RuntimeError(
                f"CRITICAL: Required stub platform define '{required}' not found in configuration. "
                f"Please ensure [stub_platform] section contains all required defines."
            )

    return defines


def extract_stub_platform_include_paths_from_toml(config: Dict[str, Any]) -> List[str]:
    """Extract stub platform include paths from TOML config - uses [stub_platform] section."""
    include_paths: List[str] = []

    # Extract include paths from [stub_platform] section
    if "stub_platform" in config and isinstance(config["stub_platform"], dict):
        stub_section: Dict[str, Any] = config["stub_platform"]  # type: ignore
        if "include_paths" in stub_section and isinstance(
            stub_section["include_paths"], list
        ):
            stub_include_paths: List[str] = stub_section["include_paths"]  # type: ignore
            include_paths.extend(stub_include_paths)
            print(
                f"[CONFIG] Loaded {len(stub_include_paths)} stub platform include paths from [stub_platform] section"
            )
        else:
            raise RuntimeError(
                "CRITICAL: No 'include_paths' list found in [stub_platform] section. "
                "This is required for stub platform compilation."
            )
    else:
        raise RuntimeError(
            "CRITICAL: No [stub_platform] section found in build_example.toml. "
            "This section is MANDATORY for stub platform compilation."
        )

    return include_paths
