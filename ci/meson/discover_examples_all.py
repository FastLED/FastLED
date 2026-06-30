#!/usr/bin/env python3
"""
Consolidated example discovery for Meson build system.

This script combines discover_example_includes.py, discover_example_sources.py,
and filter_examples_for_stub.py into a single pass over the examples directory.

Output format (one per line, using | as delimiter to avoid Windows path issues):
  EXAMPLE|<name>|<include_dir1>|<include_dir2>|...
  SOURCES|<name>|<source1>|<source2>|...
  SKIP|<name>|<reason>

Usage:
    python discover_examples_all.py <examples_dir>
"""

import fnmatch
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(slots=True)
class FilterDiscovery:
    metadata: str | None
    should_skip: bool
    reason: str


def should_skip_for_stub(filter_str: str) -> tuple[bool, str]:
    """
    Evaluate filter expression for STUB platform (host compilation).

    Filter expressions can be:
    - Simple platform list: "STUB" or "ESP32" or "TEENSY"
    - Expression: "(platform is native)" or "(memory is high)"

    For STUB builds (Mac/Linux/Windows host compilation):
    - "STUB" (case-insensitive) -> include
    - "(platform is native)" -> include (native = STUB/host)
    - Other platforms -> skip

    Args:
        filter_str: Filter string from @filter: annotation

    Returns:
        Tuple of (should_skip, reason)
    """
    # Check if filters are disabled via environment variable
    if os.environ.get("FASTLED_IGNORE_EXAMPLE_FILTERS") == "1":
        return False, ""

    # Normalize for comparison
    filter_upper = filter_str.upper()

    # Simple check: if "STUB" appears anywhere, include it
    if "STUB" in filter_upper:
        return False, ""

    # Expression-based filter: "(platform is native)" or similar
    # Pattern: (platform is <value>) or (memory is <value>)
    # Use findall to handle OR expressions like "(platform is esp32) or (platform is native)"
    platform_matches = re.findall(
        r"\(\s*platform\s+is\s+(\w+)\s*\)", filter_str, re.IGNORECASE
    )
    if platform_matches:
        platform_values = [m.lower() for m in platform_matches]
        # "native" means host/STUB build (Mac/Linux/Windows)
        # If any platform in an OR expression is "native", include in STUB builds
        if "native" in platform_values:
            return False, ""
        # Other specific platforms (esp32, teensy, etc.) should be skipped for STUB
        return True, f"Platform-specific (@filter:{filter_str})"

    # Memory filters don't exclude STUB builds
    if re.search(r"\(\s*memory\s+is\s+\w+\s*\)", filter_str, re.IGNORECASE):
        return False, ""

    # Board filters don't exclude STUB builds
    if re.search(r"\(\s*board\s+is\s+(not\s+)?\w+\s*\)", filter_str, re.IGNORECASE):
        return False, ""

    # Default: if we don't recognize the filter and it doesn't mention STUB, skip it
    return True, f"Platform-specific (@filter:{filter_str})"


def _split_filter_values(values: str) -> list[str]:
    return [v.strip() for v in re.split(r"[|,]", values) if v.strip()]


def _filter_value_matches(board_value: str, filter_values: list[str]) -> bool:
    board_value_lower = board_value.lower()
    for filter_value in filter_values:
        filter_value_lower = filter_value.lower()
        if filter_value_lower.startswith("-d"):
            filter_value_lower = filter_value_lower[2:]
        if "*" in filter_value_lower:
            if fnmatch.fnmatch(board_value_lower, filter_value_lower):
                return True
        elif board_value_lower == filter_value_lower:
            return True
    return False


def _should_skip_yaml_filter(
    require: dict[str, list[str]], exclude: dict[str, list[str]]
) -> tuple[bool, str]:
    # Host/stub examples run with native platform semantics. Memory filters do
    # not exclude host builds; host has enough memory for Meson compilation.
    host_values = {
        "platform": "native",
        "board": "stub",
        "target": "native",
    }

    for key, values in exclude.items():
        host_value = host_values.get(key)
        if host_value and _filter_value_matches(host_value, values):
            return True, f"excluded by {key}={host_value}"

    for key, values in require.items():
        if key == "memory":
            continue
        host_value = host_values.get(key)
        if host_value and not _filter_value_matches(host_value, values):
            return True, f"doesn't match required {key}={','.join(values)}"

    return False, ""


def _filter_metadata(
    require: dict[str, list[str]], exclude: dict[str, list[str]]
) -> str | None:
    parts: list[str] = []
    for section_name, constraints in (
        ("require", require),
        ("exclude", exclude),
    ):
        for key, values in sorted(constraints.items()):
            parts.append(f"{section_name}:{key}={','.join(values)}")
    return ";".join(parts) if parts else None


def parse_yaml_filter(content: str) -> FilterDiscovery:
    """Parse YAML-style @filter blocks for host/stub discovery."""
    match = re.search(
        r"//\s*@filter\s*:?\s*\n(.*?)//\s*@end-filter", content, re.DOTALL
    )
    if not match:
        return FilterDiscovery(None, False, "")

    require: dict[str, list[str]] = {}
    exclude: dict[str, list[str]] = {}
    current_section: str | None = None

    for line in match.group(1).split("\n"):
        stripped = line.lstrip("/").strip()
        if not stripped:
            continue
        if stripped.startswith("require:"):
            current_section = "require"
            continue
        if stripped.startswith("exclude:"):
            current_section = "exclude"
            continue
        if not stripped.startswith("- ") or not current_section:
            continue

        rest = stripped[2:].strip()
        if ":" not in rest:
            continue
        key, values = rest.split(":", 1)
        key = key.strip().lower()
        if key not in ("platform", "target", "memory", "board"):
            continue
        target = require if current_section == "require" else exclude
        target.setdefault(key, []).extend(_split_filter_values(values))

    metadata = _filter_metadata(require, exclude)
    should_skip, reason = _should_skip_yaml_filter(require, exclude)
    return FilterDiscovery(metadata, should_skip, reason)


def has_define_before_fastled_include(content: str) -> tuple[bool, str]:
    """Check if any #define appears before the first #include of FastLED.h.

    Any #define before the FastLED include makes the example PCH-incompatible,
    since the define could alter how FastLED.h is parsed.

    Args:
        content: File content to scan

    Returns:
        Tuple of (has_define, first_define_line) — the first #define found
    """
    for line in content.split("\n"):
        stripped = line.strip()
        # Skip comments and empty lines
        if stripped.startswith("//") or stripped == "":
            continue
        # If we hit the FastLED include first, no problematic defines exist
        if re.match(r"#\s*include\s+[<\"]FastLED", stripped):
            return False, ""
        # Any #define before FastLED include triggers NOPCH
        if re.match(r"#\s*define\s+", stripped):
            return True, stripped
    return False, ""


def discover_examples_all(examples_dir: Path) -> None:
    """
    Discover all examples with includes, sources, and filter annotations.

    Args:
        examples_dir: Root examples directory
    """
    # Convert examples_dir to absolute path for consistency
    examples_dir = examples_dir.resolve()

    # Find all .ino files
    ino_files = list(examples_dir.rglob("*.ino"))
    filters_disabled = os.environ.get("FASTLED_IGNORE_EXAMPLE_FILTERS") == "1"

    for ino_file in sorted(ino_files):
        # Example name is the filename without extension
        example_name = ino_file.stem

        # Example root directory (parent of .ino file)
        example_root = ino_file.parent

        # Collect include directories as relative paths (relative to examples_dir)
        # Start with the example root directory
        include_dirs = [
            str(example_root.relative_to(examples_dir.parent)).replace("\\", "/")
        ]

        # Auto-discover all subdirectories for include paths
        for subdir in sorted(example_root.iterdir()):
            if subdir.is_dir() and not subdir.name.startswith("."):
                include_dirs.append(
                    str(subdir.relative_to(examples_dir.parent)).replace("\\", "/")
                )

        # Output EXAMPLE line with include directories (use | delimiter)
        print(f"EXAMPLE|{example_name}|{'|'.join(include_dirs)}")

        # Collect additional .cpp source files as relative paths
        # Look in example root directory and src/ and lib/ subdirectories
        cpp_sources: list[str] = []

        # First, check for .cpp files in the example root directory
        # Exclude .ino.cpp files (PlatformIO preprocessed intermediates)
        for cpp_file in example_root.glob("*.cpp"):
            if cpp_file.name.endswith(".ino.cpp"):
                continue
            cpp_sources.append(
                str(cpp_file.relative_to(examples_dir.parent)).replace("\\", "/")
            )

        # Auto-discover .cpp files in all subdirectories
        for subdir in sorted(example_root.iterdir()):
            if subdir.is_dir() and not subdir.name.startswith("."):
                for cpp_file in subdir.rglob("*.cpp"):
                    cpp_sources.append(
                        str(cpp_file.relative_to(examples_dir.parent)).replace(
                            "\\", "/"
                        )
                    )

        # Output SOURCES line (even if empty - consistent format, use | delimiter)
        sources_str: str = "|".join(cpp_sources) if cpp_sources else ""
        print(f"SOURCES|{example_name}|{sources_str}")

        # Check for @filter annotation and NOPCH detection in .ino file
        try:
            content = ino_file.read_text(encoding="utf-8")

            # Check for @filter annotation. Support both one-line and
            # YAML-style blocks so host builds skip platform-specific sketches.
            if "@filter:" in content or "@filter-out:" in content:
                for line in content.split("\n"):
                    line = line.strip()
                    if line.startswith("// @filter:"):
                        filter_str = line.split("// @filter:")[1].strip()
                        should_skip, reason = should_skip_for_stub(filter_str)
                        if should_skip:
                            print(f"SKIP|{example_name}|{reason}")
                        print(f"FILTER|{example_name}|{filter_str}")
                        break
                    elif line.startswith("// @filter-out:"):
                        platforms = line.split("// @filter-out:")[1].strip()
                        if "STUB" in platforms.upper() and not filters_disabled:
                            print(
                                f"SKIP|{example_name}|Filtered out for STUB (@filter-out:{platforms})"
                            )
                        print(f"FILTER|{example_name}|filter-out:{platforms}")
                        break
            elif "@filter" in content and "@end-filter" in content:
                filter_result = parse_yaml_filter(content)
                if filter_result.metadata:
                    if filter_result.should_skip and not filters_disabled:
                        print(f"SKIP|{example_name}|{filter_result.reason}")
                    print(f"FILTER|{example_name}|{filter_result.metadata}")

            # Check for #define before #include FastLED (PCH-incompatible)
            has_define, define_line = has_define_before_fastled_include(content)
            if has_define:
                print(f"NOPCH|{example_name}|{define_line}")
        except (UnicodeDecodeError, OSError):
            # If we can't read the file, skip silently
            pass


def main() -> None:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <examples_dir>", file=sys.stderr)
        sys.exit(1)

    examples_dir = Path(sys.argv[1])
    if not examples_dir.exists():
        print(f"Error: Directory '{examples_dir}' does not exist", file=sys.stderr)
        sys.exit(1)

    discover_examples_all(examples_dir)


if __name__ == "__main__":
    main()
