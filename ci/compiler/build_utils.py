"""Build utility functions for FastLED PlatformIO builds."""

import os
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


def get_utf8_env() -> dict[str, str]:
    """Get environment with UTF-8 encoding to prevent Windows CP1252 encoding errors.

    PlatformIO outputs Unicode characters (checkmarks, etc.) that fail on Windows
    when using the default CP1252 console encoding. This ensures UTF-8 is used.

    Note: This function inherits Git Bash environment variables. For PlatformIO
    operations that must run via CMD (e.g., ESP32-C6 compilation), use
    get_pio_execution_env() instead.
    """
    env = os.environ.copy()
    env["PYTHONIOENCODING"] = "utf-8"
    return env


def get_pio_execution_env() -> dict[str, str]:
    """Get environment suitable for PlatformIO execution.

    On Windows Git Bash: Returns clean environment without Git Bash indicators
    (strips MSYSTEM, TERM, SHELL, etc.) to prevent ESP-IDF toolchain errors.

    On other platforms: Returns UTF-8 environment (same as get_utf8_env()).

    This function solves the ESP-IDF v5.5.x incompatibility with Git Bash:
        "ERROR: MSys/Mingw is not supported. Please follow the getting started guide"

    Returns:
        Environment dict suitable for running PlatformIO commands
    """
    from ci.util.windows_cmd_runner import get_clean_windows_env, should_use_cmd_runner

    if should_use_cmd_runner():
        # Windows Git Bash: Use clean environment without Git Bash indicators
        return get_clean_windows_env()
    else:
        # Linux/Mac or native Windows shell: Use UTF-8 environment
        return get_utf8_env()


def create_building_banner(example: str) -> str:
    """Create a building banner for the given example."""
    banner_text = f"BUILDING {example}"
    border_char = "="
    padding = 2
    text_width = len(banner_text)
    total_width = text_width + (padding * 2)

    top_border = border_char * (total_width + 4)
    middle_line = (
        f"{border_char} {' ' * padding}{banner_text}{' ' * padding} {border_char}"
    )
    bottom_border = border_char * (total_width + 4)

    banner = f"{top_border}\n{middle_line}\n{bottom_border}"

    # Apply blue color using ANSI escape codes
    blue_color = "\033[34m"
    reset_color = "\033[0m"
    return f"{blue_color}{banner}{reset_color}"


def get_example_error_message(project_root: Path, example: str) -> str:
    """Generate appropriate error message for missing example.

    Args:
        project_root: FastLED project root directory
        example: Example name or path that was not found

    Returns:
        Error message describing where the example was expected
    """
    example_path = Path(example)

    if example_path.is_absolute():
        return f"Example directory not found: {example}"
    else:
        return f"Example not found: {project_root / 'examples' / example}"


# ============================================================================
# WASM Build Utilities (Shared across wasm_compile_pch.py, wasm_build_library.py)
# ============================================================================


def compute_content_hash(file_path: Path) -> str:
    """Compute SHA256 hash of file content.

    Args:
        file_path: Path to file to hash

    Returns:
        SHA256 hex digest string, or empty string if file doesn't exist
    """
    import hashlib

    if not file_path.exists():
        return ""

    sha256 = hashlib.sha256()
    with open(file_path, "rb") as f:
        sha256.update(f.read())
    return sha256.hexdigest()


def compute_flags_hash(flags: dict[str, list[str]]) -> str:
    """Compute hash of compilation flags for change detection.

    Args:
        flags: Dictionary with 'defines' and 'compiler_flags' keys

    Returns:
        SHA256 hex digest of sorted flags string
    """
    import hashlib

    # Combine all flags into a single sorted string for consistent hashing
    all_flags = sorted(flags["defines"] + flags["compiler_flags"])
    flags_str = " ".join(all_flags)
    return hashlib.sha256(flags_str.encode()).hexdigest()


def load_json_metadata(path: Path) -> dict[str, str]:
    """Load metadata from JSON file with error handling.

    Args:
        path: Path to JSON metadata file

    Returns:
        Dictionary of metadata, or empty dict if file doesn't exist or is invalid
    """
    import json

    if not path.exists():
        return {}

    try:
        with open(path) as f:
            return json.load(f)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Warning: Could not load metadata from {path}: {e}")
        return {}


def save_json_metadata(path: Path, metadata: dict[str, str]) -> None:
    """Save metadata to JSON file.

    Args:
        path: Path to JSON metadata file
        metadata: Dictionary of metadata to save
    """
    import json

    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        json.dump(metadata, f, indent=2)


def parse_depfile(depfile_path: Path) -> list[Path]:
    """Parse Makefile-style dependency file to extract header dependencies.

    Format:
        target: dep1 dep2 dep3 \\
                dep4 dep5

    Args:
        depfile_path: Path to .d dependency file

    Returns:
        List of dependency paths (excluding target)
    """
    if not depfile_path.exists():
        return []

    try:
        with open(depfile_path) as f:
            content = f.read()

        # Remove line continuations
        content = content.replace("\\\n", " ").replace("\\\r\n", " ")

        # Split on colon to separate target from dependencies
        if ":" not in content:
            return []

        deps_part = content.split(":", 1)[1]

        # Split on whitespace and filter out empty strings
        deps = [d.strip() for d in deps_part.split() if d.strip()]

        # Convert to Path objects
        return [Path(d) for d in deps]

        handle_keyboard_interrupt_properly()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Warning: Could not parse dependency file {depfile_path}: {e}")
        return []


def get_latest_mtime(paths: list[Path]) -> float:
    """Get the latest modification time from a list of paths.

    Args:
        paths: List of file paths to check

    Returns:
        Latest modification time in seconds since epoch, or 0.0 if no paths exist
    """
    max_mtime = 0.0
    for p in paths:
        if p.exists():
            mtime = p.stat().st_mtime
            if mtime > max_mtime:
                max_mtime = mtime
    return max_mtime


def write_response_file(path: Path, items: list[Path]) -> None:
    """Write linker/archiver response file.

    Response files allow passing large lists of files to tools without
    hitting command-line length limits.

    Args:
        path: Path to response file to write
        items: List of file paths to include
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        for item in items:
            # Use forward slashes for cross-platform compatibility
            f.write(f"{item.as_posix()}\n")
