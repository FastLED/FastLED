from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""Auto-generate @filter/@end-filter blocks from C++ platform guards in sketches.

This script analyzes existing C++ guards in .ino files and generates appropriate
@filter/@end-filter blocks. The user can review and commit the generated changes.

Supported guard patterns:
- #if !SKETCH_HAS_LOTS_OF_MEMORY / #if SKETCH_HAS_LOTS_OF_MEMORY
- #ifdef ESP32, #ifdef ESP8266, etc. / #ifndef variant guards
- #if defined(CONFIG_IDF_TARGET_ESP32S3) etc.
"""

import re
import sys
from pathlib import Path
from typing import Optional

from ci.compiler.sketch_filter import parse_filter_from_sketch


def extract_guard_filters(content: str) -> Optional[str]:
    """Extract filter directives from C++ platform guards.

    Returns:
        Filter block string (without surrounding comments) or None if no guards found
    """
    # Pattern 1: SKETCH_HAS_LOTS_OF_MEMORY guards
    if re.search(r"#if\s+!SKETCH_HAS_LOTS_OF_MEMORY", content, re.MULTILINE):
        # #if !SKETCH_HAS_LOTS_OF_MEMORY means compile only if high memory
        return "require:\n   - memory: high"
    elif re.search(
        r"#if\s+SKETCH_HAS_LOTS_OF_MEMORY\s*\n.*?#else\s*\n.*?#include.*?sketch_fake",
        content,
        re.MULTILINE | re.DOTALL,
    ):
        # Pattern: #if SKETCH_HAS_LOTS_OF_MEMORY ... real code ... #else ... sketch_fake
        return "require:\n   - memory: high"

    # Pattern 2: Platform-specific guards
    filters: dict[str, list[str]] = {
        "require": [],
        "exclude": [],
    }  # type: Dict[str, List[str]]

    # Check for ESP32-S3 specific
    if re.search(
        r"#if\s+defined\(CONFIG_IDF_TARGET_ESP32S3\)|#define.*IS_ESP32_S3.*ESP32S3",
        content,
        re.MULTILINE,
    ):
        filters["require"].append("target: ESP32S3")

    # Check for ESP32-P4 specific
    if re.search(
        r"#if\s+defined\(CONFIG_IDF_TARGET_ESP32P4\)",
        content,
        re.MULTILINE,
    ):
        filters["require"].append("target: ESP32P4")

    # Check for ESP8266 specific
    if re.search(r"#if\s+!defined\(ESP8266\)|#ifndef\s+ESP8266", content, re.MULTILINE):
        filters["require"].append("platform: esp8266")
    elif re.search(r"#ifdef\s+ESP8266", content, re.MULTILINE):
        filters["require"].append("platform: esp8266")

    # Check for AVR/ATmega exclusion
    if re.search(r"#if\s+!defined\(__AVR__\)|#ifndef\s+__AVR__", content, re.MULTILINE):
        filters["exclude"].append("platform: avr")

    # Check for general ESP32 (not specific variant)
    if re.search(
        r"#ifdef\s+ESP32(?!\d|_|\w)|#ifndef\s+ESP32(?!\d|_|\w)",
        content,
        re.MULTILINE,
    ):
        if not any("esp32" in str(f) for f in filters["require"]):
            filters["require"].append("platform: esp32")

    # Build filter string
    filter_parts: list[str] = []
    if filters["require"]:
        filter_parts.append("require:")
        for req in filters["require"]:
            filter_parts.append(f"   - {req}")
    if filters["exclude"]:
        filter_parts.append("exclude:")
        for exc in filters["exclude"]:
            filter_parts.append(f"   - {exc}")

    if filter_parts:
        return "\n".join(filter_parts)

    return None


def generate_filter_block(filters_str: str) -> str:
    """Generate full @filter block from filter directives."""
    replaced = filters_str.replace("\n", "\n// ")
    return f"// @filter\n// {replaced}\n// @end-filter"


def migrate_sketch(ino_path: Path, dry_run: bool = False) -> tuple[bool, str]:
    """Migrate a single sketch file.

    Returns:
        Tuple of (modified: bool, message: str)
    """
    try:
        content = ino_path.read_text(encoding="utf-8")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        return False, f"Failed to read: {e}"

    # Check if already has filter block
    if parse_filter_from_sketch(ino_path) is not None:
        return False, "Already has @filter block"

    # Try to extract filters from guards
    filters_str = extract_guard_filters(content)
    if not filters_str:
        return False, "No platform guards detected"

    # Generate filter block
    filter_block = generate_filter_block(filters_str)

    # Insert filter block at the top of the file (after includes)
    lines = content.split("\n")
    insert_pos = 0

    # Find position after #include statements and comments
    for i, line in enumerate(lines):
        if line.strip().startswith("//") or line.strip().startswith("#include"):
            insert_pos = i + 1
        elif (
            line.strip()
            and not line.strip().startswith("//")
            and not line.strip().startswith("#")
        ):
            break

    # Insert filter block
    new_lines = (
        lines[:insert_pos] + [""] + filter_block.split("\n") + [""] + lines[insert_pos:]
    )
    new_content = "\n".join(new_lines)

    if not dry_run:
        ino_path.write_text(new_content, encoding="utf-8")

    return True, f"Generated filter:\n{filter_block}"


def main() -> int:
    """Run migration on all sketches."""
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    examples_dir = project_root / "examples"

    if not examples_dir.exists():
        print(f"ERROR: Examples directory not found: {examples_dir}")
        return 1

    # Parse arguments
    dry_run = "--dry-run" in sys.argv or "-n" in sys.argv
    sketch_pattern = None
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        sketch_pattern = sys.argv[1]

    print(f"Migrating sketches in: {examples_dir}")
    if dry_run:
        print("[DRY RUN MODE - No files will be modified]")
    print()

    # Find sketches
    sketches = sorted(examples_dir.rglob("*.ino"))
    if sketch_pattern:
        sketches = [s for s in sketches if sketch_pattern.lower() in s.name.lower()]

    print(f"Found {len(sketches)} sketch(es) to process\n")

    migrated = 0
    skipped = 0
    errors = 0

    for ino_path in sketches:
        rel_path = ino_path.relative_to(examples_dir)
        modified, message = migrate_sketch(ino_path, dry_run=dry_run)

        if modified:
            migrated += 1
            print(f"✓ {rel_path}")
            for line in message.split("\n"):
                print(f"  {line}")
        else:
            if "Failed" in message or "No" in message:
                if "Failed" in message:
                    errors += 1
                    print(f"✗ {rel_path}: {message}")
                else:
                    skipped += 1
                    print(f"- {rel_path}: {message}")

    print()
    print("=" * 70)
    print("Migration summary:")
    print(f"  Migrated: {migrated}")
    print(f"  Skipped: {skipped}")
    if errors:
        print(f"  Errors: {errors}")
    if dry_run:
        print()
        print("(Run without --dry-run to apply changes)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
